// Shared contract between native package acquisition and installation.
#pragma once
#include <filesystem>
#include <map>
#include <string>
namespace kyna::cli {
std::map<std::filesystem::path,std::string> unpackNativeArchive(const std::string &);
}
