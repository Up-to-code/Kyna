// Owns conversion from CLI settings into a checked embedding session.
#include "../commands/project_internals.hpp"
#include <kyna/language/native_modules.hpp>
#include <kyna/stdlib/sha256.hpp>
#include <toml++/toml.hpp>
#include <fstream>
#include <sstream>
namespace kyna::cli {
LanguageSessionOptions makeSessionOptions(const Options &options) {
  LanguageSessionOptions result;
  result.modulePaths = options.modulePaths;
  result.collectMetrics = !options.metricsFile.empty();
  result.capabilities.consoleColors = options.consoleColor;
  try {
    if (!options.sourceOverlay.empty()) {
      if (std::filesystem::file_size(options.sourceOverlay) > 16 * 1024 * 1024) throw std::runtime_error("source overlay exceeds size limit");
      auto overlay = toml::parse_file(options.sourceOverlay.string());
      auto records = overlay["source"].as_array(); if (!records) throw std::runtime_error("source overlay requires source records");
      for (auto &node : *records) {
        auto table = node.as_table(); if (!table) throw std::runtime_error("invalid source overlay record");
        auto path = (*table)["path"].value<std::string>(), content = (*table)["content"].value<std::string>();
        if (!path || !content || !std::filesystem::path(*path).is_absolute()) throw std::runtime_error("overlay needs absolute path and source content");
        result.sourceOverlays.insert_or_assign(std::filesystem::weakly_canonical(*path), *content);
      }
    }
    auto libraries = options.nativeLibraries;
    auto root = discoverProject(options.input.empty() || options.input == "-" ? std::filesystem::current_path() : std::filesystem::absolute(options.input));
    if (!root.empty() && std::filesystem::exists(root / ".kyna/native.lock")) {
      auto lock = toml::parse_file((root / ".kyna/native.lock").string());
      if (auto packages = lock["package"].as_array()) for (auto &node : *packages) {
        auto table = node.as_table(); if (!table) throw std::runtime_error("invalid native lock record");
        auto file = (*table)["file"].value_or(std::string{}); auto path = std::filesystem::weakly_canonical(root / file);
        auto relative = std::filesystem::relative(path, std::filesystem::canonical(root));
        if (file.empty() || relative.empty() || relative.is_absolute() || relative.generic_string().starts_with("..")) throw std::runtime_error("invalid native lock path");
        std::ifstream stream(path, std::ios::binary); std::ostringstream data; data << stream.rdbuf();
        if (!stream || detail::sha256Hex(data.str()) != (*table)["sha256"].value_or(std::string{})) throw std::runtime_error("installed native artifact checksum mismatch");
        if (auto files = (*table)["files"].as_table()) for (auto &[file,node] : *files) {
          auto artifact = std::filesystem::weakly_canonical(root / std::string(file.str()));auto relative=std::filesystem::relative(artifact,root);
          if(relative.is_absolute()||relative.generic_string().starts_with("..")) throw std::runtime_error("invalid native archive lock path");
          std::ifstream stream(artifact,std::ios::binary);std::ostringstream data;data<<stream.rdbuf();
          if(!stream||detail::sha256Hex(data.str())!=node.value_or(std::string{}))throw std::runtime_error("installed native archive file checksum mismatch");
        }
        if(auto moduleRoot=(*table)["module_root"].value<std::string>())result.modulePaths.push_back(root / *moduleRoot);
        libraries.push_back(path);
      }
      result.modulePaths.push_back(root);
    }
    if (!root.empty()) {
      auto manifest = toml::parse_file((root / "kyna.toml").string());
      if (auto dependencies = manifest["dependencies"].as_table()) for (auto &[name,node] : *dependencies) {
        if (auto table = node.as_table()) {
          if (auto path = (*table)["path"].value<std::string>()) result.modulePaths.push_back(root / *path);
          else if ((*table)["git"].value<std::string>()) result.modulePaths.push_back(cacheRoot() / "git");
        }
      }
    }
    for (const auto &path : libraries) {
      auto functions = loadNativeModule(path);
      result.nativeFunctions.insert(result.nativeFunctions.end(), functions.begin(), functions.end());
    }
    nativeFunctionBindings(result.nativeFunctions);
  } catch (const std::exception &error) {
    Diagnostic diagnostic{error.what(), {}, false, "KNATIVE1001"};
    diagnostic.help = "Check the library path, target architecture, and native ABI version.";
    throw KynaError(diagnostic);
  }
  return result;
}
}
