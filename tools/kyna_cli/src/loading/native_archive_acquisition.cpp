// Owns bounded extraction of verified native archives without links or escaping paths.
#include "native_artifacts.hpp"
#include "../commands/project_internals.hpp"
#include <chrono>
#include <fstream>
#include <sstream>
namespace kyna::cli {
namespace {
[[noreturn]] void reject(std::string message){throw KynaError({std::move(message),{},false,"KPACKAGE1005"});}
struct Temporary {fs::path path;~Temporary(){std::error_code error;fs::remove_all(path,error);}};
bool safe(const fs::path &path){auto name=path.generic_string();if(name.empty()||path.is_absolute()||name.find_first_of("\\:\r\n")!=std::string::npos)return false;for(const auto &part:path)if(part=="..")return false;return true;}
}
std::map<fs::path,std::string> unpackNativeArchive(const std::string &data){
 Temporary folder{fs::temp_directory_path()/("kyna-archive-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
 if(!fs::create_directory(folder.path))reject("cannot allocate archive staging directory");fs::permissions(folder.path,fs::perms::owner_all);
 auto archive=folder.path/"artifact.tar";std::string error;if(!projectWrite(archive,data,error))reject(error);
 auto host=productionRuntimeCapabilities();ProcessConfig config;config.program="tar";config.captureOutput=true;config.args={"-tf",archive.string()};auto listing=host.processes->spawn(config);
 if(listing.failedToStart||listing.exitCode||listing.stdoutText.size()>8*1024*1024)reject("cannot inspect native tar archive; install tar or obtain a binary artifact");
 std::istringstream names(listing.stdoutText);std::string line;size_t count=0;while(std::getline(names,line)){if(++count>10000||!safe(fs::path(line)))reject("unsafe native archive path");}
 config.args={"-tvf",archive.string()};listing=host.processes->spawn(config);if(listing.failedToStart||listing.exitCode)reject("cannot inspect native archive types");
 std::istringstream types(listing.stdoutText);size_t expanded=0;
 while(std::getline(types,line)){
  if(line.empty()||(line.front()!='-'&&line.front()!='d'))reject("native archives cannot contain links or special files");
  if(line.front()=='d')continue;
  // GNU tar uses owner/group then size; BSD tar uses link count, owner, group, size.
  std::istringstream fields(line);std::vector<std::string> tokens;std::string token;
  while(fields>>token)tokens.push_back(token);
  if(tokens.size()<5)reject("cannot inspect native archive size");
  auto numeric=[](const std::string &value){return !value.empty()&&value.find_first_not_of("0123456789")==std::string::npos;};
  auto index=numeric(tokens[1])?4:2;
  if(!numeric(tokens[index]))reject("cannot inspect native archive size");
  std::uint64_t bytes=0;try{bytes=std::stoull(tokens[index]);}catch(...){reject("invalid native archive size");}
  if(bytes>512*1024*1024||expanded>512*1024*1024-bytes)reject("expanded native archive exceeds 512 MiB");
  expanded+=bytes;
 }
 auto extracted=folder.path/"files";fs::create_directory(extracted);config.args={"-xf",archive.string(),"-C",extracted.string(),"--no-same-owner","--no-same-permissions"};auto result=host.processes->spawn(config);if(result.failedToStart||result.exitCode)reject("native archive extraction failed");
 std::map<fs::path,std::string> files;size_t total=0;
 for(auto &entry:fs::recursive_directory_iterator(extracted)){
  if(entry.is_symlink()||(!entry.is_directory()&&!entry.is_regular_file()))reject("native archive contains unsupported entry");if(!entry.is_regular_file())continue;
  auto relative=fs::relative(entry.path(),extracted);if(!safe(relative))reject("native archive escaped staging");auto size=entry.file_size();if(size>512*1024*1024||total>512*1024*1024-size)reject("expanded native archive exceeds 512 MiB");total+=size;
  std::ifstream stream(entry.path(),std::ios::binary);std::ostringstream bytes;bytes<<stream.rdbuf();if(!stream)reject("cannot read extracted native file");files.emplace(relative,bytes.str());
 }
 return files;
}
}
