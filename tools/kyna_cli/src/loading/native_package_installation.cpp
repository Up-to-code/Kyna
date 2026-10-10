// Owns verified, target-specific native dependency installation without install scripts.
#include "../commands/project_internals.hpp"
#include "native_artifacts.hpp"
#include <kyna/stdlib/sha256.hpp>
#include <kyna/language/native_modules.hpp>
#include <fstream>
#include <sstream>
namespace kyna::cli {
namespace {
[[noreturn]] void fail(const char *code,const std::string &message){Diagnostic d{message,{},false,code};d.help="Check dependency native version, target, ABI, artifact path/URL, and SHA-256 in kyna.toml.";throw KynaError(d);}
std::string bytes(const fs::path &p){std::ifstream f(p,std::ios::binary);if(!f)fail("KPACKAGE1001","cannot read native artifact: "+p.string());std::ostringstream s;s<<f.rdbuf();return s.str();}
}
std::string nativeTarget(){
#if defined(__ANDROID__)
 const char *os="android";
#elif defined(_WIN32)
 const char *os="windows";
#elif defined(__APPLE__)
 const char *os="darwin";
#else
 const char *os="linux";
#endif
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
 return std::string(os)+"-arm64";
#else
 return std::string(os)+"-x86_64";
#endif
}
void installNativePackages(const fs::path &root,const toml::table &manifest,bool locked){
 const auto *deps=manifest["dependencies"].as_table();if(!deps)return;
 toml::array records;
 struct Artifact {fs::path file; std::string data, version; std::map<fs::path,std::string> extra; fs::path directory;};
 std::vector<Artifact> artifacts;
 for(const auto &[name,node]:*deps){const auto *entry=node.as_table();if(!entry)continue;const auto *native=(*entry)["native"].as_table();if(!native)continue;
  auto package=std::string(name.str());if(package.empty()||package.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos)fail("KPACKAGE1001","invalid native package name");
  auto version=(*native)["version"].value_or(std::string{}),target=(*native)["target"].value_or(std::string{}),sha=(*native)["sha256"].value_or(std::string{});auto abi=(*native)["abi"].value_or(int64_t{});
  if(version.empty()||target!=nativeTarget()||abi!=2||sha.size()!=64||sha.find_first_not_of("0123456789abcdef")!=std::string::npos)fail("KPACKAGE1002","native contract mismatch for "+package);
  auto path=(*native)["path"].value<std::string>(),url=(*native)["url"].value<std::string>();std::string data;
  if(path)data=bytes(root / *path);else if(url){if(!url->starts_with("https://"))fail("KPACKAGE1001","native downloads require HTTPS");NetworkRequest request;request.url=*url;NetworkFailure failure;auto response=productionRuntimeCapabilities().network->send(request,failure);if(!response||!response->ok()||!response->effectiveUrl.starts_with("https://"))fail("KPACKAGE1001","native download failed: "+failure.message);data=std::move(response->body);}else fail("KPACKAGE1001","native artifact needs path or URL");
  if(detail::sha256Hex(data)!=sha)fail("KPACKAGE1003","SHA-256 mismatch for "+package);
  fs::path file=fs::path(".kyna/native")/(package+"-"+sha+".native"),directory;
  std::map<fs::path,std::string> extra;
  if((*native)["archive"].value_or(false)) {
   extra=unpackNativeArchive(data);auto library=(*native)["library"].value_or(std::string{});
   auto found=extra.find(fs::path(library));if(library.empty()||found==extra.end())fail("KPACKAGE1005","native archive is missing the declared library");
   directory=fs::path(".kyna/native")/(package+"-"+sha);file=directory/library;data=found->second;
  }
  toml::table record{{"name",package},{"version",version},{"target",target},{"abi",abi},{"sha256",detail::sha256Hex(data)},{"artifact_sha256",sha},{"file",file.generic_string()}};
  if(!extra.empty()){record.insert("module_root",directory.generic_string());toml::table files;for(auto &[path,bytes]:extra)files.insert((directory/path).generic_string(),detail::sha256Hex(bytes));record.insert("files",std::move(files));}
  records.push_back(std::move(record));artifacts.push_back({root/file,std::move(data),version,std::move(extra),directory.empty()?fs::path{}:root/directory});
 }
 if(records.empty())return;
 toml::table contract{{"schema","kyna.native-lock/v1"},{"package",std::move(records)}};std::ostringstream rendered;rendered<<contract<<'\n';auto lock=root/".kyna/native.lock";
 if(locked&&(!fs::exists(lock)||bytes(lock)!=rendered.str()))fail("KPACKAGE1004","native lock disagrees with manifest");
 // Compare the contract before writing artifacts, particularly for --locked.
 for (const auto &[file,data,version,extra,directory] : artifacts) {
  fs::create_directories(file.parent_path()); auto staging=file;staging += ".pending";
  fs::path stagingDirectory;
  if(!extra.empty()){stagingDirectory=directory;stagingDirectory += ".pending";fs::remove_all(stagingDirectory);fs::create_directories(stagingDirectory);for(auto &[path,bytes]:extra){auto output=stagingDirectory/path;fs::create_directories(output.parent_path());std::string error;if(!projectWrite(output,bytes,error))fail("KPACKAGE1001",error);}staging=stagingDirectory/fs::relative(file,directory);}
  std::string error;if(!projectWrite(staging,data,error))fail("KPACKAGE1001",error);
  try { auto functions=loadNativeModule(staging); if (functions.empty() || functions.front().abiVersion != 2 || functions.front().moduleVersion != version) fail("KPACKAGE1002", "native descriptor version or ABI differs from manifest"); for(auto &function:functions)if(function.shutdown)function.shutdown(); }
  catch (const KynaError &) {fs::remove(staging);throw;}
  catch (const std::exception &error) {fs::remove(staging);fail("KPACKAGE1002",error.what());}
  if(!extra.empty()){fs::remove_all(directory);fs::rename(stagingDirectory,directory);continue;}
  if(fs::exists(file)) {fs::copy_file(staging,file,fs::copy_options::overwrite_existing);fs::remove(staging);} else fs::rename(staging,file);
 }
 std::string error;if(!locked&&!projectWrite(lock,rendered.str(),error))fail("KPACKAGE1001",error);
}
}
