// Owns Unicode Windows argv execution, environment overrides, and concurrent stream capture.
#include "../host_private.hpp"
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <thread>
#include <stdexcept>
#include <cwchar>
namespace kyna::detail {
namespace {
struct Handle {HANDLE value{};~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}Handle()=default;Handle(const Handle&)=delete;Handle&operator=(const Handle&)=delete;};
std::wstring wide(const std::string &s){if(s.find('\0')!=std::string::npos)throw std::runtime_error("process arguments cannot contain null bytes");if(s.empty())return {};auto size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0);if(!size)throw std::runtime_error("process text must be UTF-8");std::wstring out(size,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),out.data(),size);return out;}
std::wstring quote(const std::wstring &s){std::wstring out=L"\"";size_t slashes=0;for(auto c:s){if(c==L'\\'){++slashes;continue;}out.append(c==L'"'?slashes*2+1:slashes,L'\\');slashes=0;out+=c;}out.append(slashes*2,L'\\');return out+L'"';}
struct IgnoreCase {bool operator()(const std::wstring &a,const std::wstring &b)const{return CompareStringOrdinal(a.data(),int(a.size()),b.data(),int(b.size()),TRUE)==CSTR_LESS_THAN;}};
std::vector<wchar_t> environment(const ProcessConfig &config){if(config.env.empty())return {};std::map<std::wstring,std::wstring,IgnoreCase> values;auto inherited=GetEnvironmentStringsW();if(inherited){for(auto entry=inherited;*entry;entry+=std::wcslen(entry)+1){std::wstring text(entry);auto split=text.find(L'=',text.front()==L'='?1:0);if(split!=std::wstring::npos)values[text.substr(0,split)]=text.substr(split+1);}FreeEnvironmentStringsW(inherited);}for(auto &[key,value]:config.env){if(key.empty()||key.find('=')!=std::string::npos)throw std::runtime_error("invalid process environment key");values[wide(key)]=wide(value);}std::vector<wchar_t> result;for(auto &[key,value]:values){auto text=key+L'='+value;result.insert(result.end(),text.begin(),text.end());result.push_back(0);}result.push_back(0);return result;}
void drain(HANDLE pipe,std::string &text){char buffer[4096];DWORD size;while(ReadFile(pipe,buffer,sizeof(buffer),&size,nullptr)&&size)text.append(buffer,size);}
}
ProcessResult spawnWindowsProcess(const ProcessConfig &config){
 ProcessResult result;
 try {
  auto command=quote(wide(config.program));for(auto &arg:config.args)command+=L' '+quote(wide(arg));auto env=environment(config);auto cwd=config.workingDir.wstring();
  Handle outRead,outWrite,errRead,errWrite,input,process,thread;SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(STARTUPINFOW);DWORD flags=CREATE_UNICODE_ENVIRONMENT;
  std::vector<unsigned char> attributes;
  if(config.captureOutput){
   if(!CreatePipe(&outRead.value,&outWrite.value,&security,0)||!CreatePipe(&errRead.value,&errWrite.value,&security,0))throw std::runtime_error("cannot create process capture pipes");
   SetHandleInformation(outRead.value,HANDLE_FLAG_INHERIT,0);SetHandleInformation(errRead.value,HANDLE_FLAG_INHERIT,0);
   input.value=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);if(input.value==INVALID_HANDLE_VALUE)throw std::runtime_error("cannot open process input");
   startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;startup.StartupInfo.hStdInput=input.value;startup.StartupInfo.hStdOutput=outWrite.value;startup.StartupInfo.hStdError=errWrite.value;
   SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);attributes.resize(bytes);startup.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
   if(!InitializeProcThreadAttributeList(startup.lpAttributeList,1,0,&bytes))throw std::runtime_error("cannot initialize process handle list");
   HANDLE handles[]{input.value,outWrite.value,errWrite.value};if(!UpdateProcThreadAttribute(startup.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof(handles),nullptr,nullptr)){DeleteProcThreadAttributeList(startup.lpAttributeList);throw std::runtime_error("cannot restrict child process handles");}
   startup.StartupInfo.cb=sizeof(startup);flags|=EXTENDED_STARTUPINFO_PRESENT;
  }
  PROCESS_INFORMATION info{};auto started=CreateProcessW(nullptr,command.data(),nullptr,nullptr,config.captureOutput,flags,env.empty()?nullptr:env.data(),cwd.empty()?nullptr:cwd.c_str(),&startup.StartupInfo,&info);
  auto error=GetLastError();if(startup.lpAttributeList)DeleteProcThreadAttributeList(startup.lpAttributeList);
  if(!started)throw std::runtime_error("CreateProcessW failed with error "+std::to_string(error));process.value=info.hProcess;thread.value=info.hThread;
  if(outWrite.value){CloseHandle(outWrite.value);outWrite.value=nullptr;}if(errWrite.value){CloseHandle(errWrite.value);errWrite.value=nullptr;}
  std::jthread output,errors;if(config.captureOutput){output=std::jthread([&]{drain(outRead.value,result.stdoutText);});errors=std::jthread([&]{drain(errRead.value,result.stderrText);});}
  WaitForSingleObject(process.value,INFINITE);if(output.joinable())output.join();if(errors.joinable())errors.join();DWORD code=0;GetExitCodeProcess(process.value,&code);result.exitCode=int(code);
 }catch(const std::exception &e){result.failedToStart=true;result.startError=e.what();}
 return result;
}
}
#endif
