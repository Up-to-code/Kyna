// Owns argv-based working-directory spawning on Android before API 34.
#include "../host_private.hpp"
#if defined(__ANDROID__)
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
namespace kyna::detail {
int spawnAndroidWithDirectory(const ProcessConfig &config,int &child,
    char *const *argv,char *const *environment,const int *output,const int *errors) {
  // Allocate PATH candidates before fork. The child only calls async-signal-safe syscalls.
  std::vector<std::string> candidates;
  if(config.program.find('/')!=std::string::npos)candidates.push_back(config.program);
  else {
    auto override=config.env.find("PATH");const char *inherited=std::getenv("PATH");
    std::string path=override!=config.env.end()?override->second:inherited?inherited:"/system/bin:/system/xbin";
    for(std::size_t start=0;;){auto end=path.find(':',start);auto directory=path.substr(start,end-start);candidates.push_back(directory.empty()?config.program:directory+"/"+config.program);if(end==std::string::npos)break;start=end+1;}
  }
  int failurePipe[2];if(pipe2(failurePipe,O_CLOEXEC))return errno;
  child=fork();
  if(child<0){auto error=errno;close(failurePipe[0]);close(failurePipe[1]);return error;}
  if(child==0){
    close(failurePipe[0]);int failure=0;
    if(chdir(config.workingDir.c_str()))failure=errno;
    if(!failure&&config.captureOutput){
      if(dup2(output[1],STDOUT_FILENO)<0||dup2(errors[1],STDERR_FILENO)<0)failure=errno;
      close(output[0]);close(errors[0]);close(output[1]);close(errors[1]);
    }
    if(!failure){
      bool denied=false;
      for(const auto &program:candidates){execve(program.c_str(),argv,environment);failure=errno;if(failure==EACCES){denied=true;continue;}if(failure!=ENOENT&&failure!=ENOTDIR)break;}
      if(denied&&(failure==ENOENT||failure==ENOTDIR))failure=EACCES;
    }
    while(write(failurePipe[1],&failure,sizeof(failure))<0&&errno==EINTR){}
    _exit(127);
  }
  close(failurePipe[1]);int failure=0;ssize_t count;
  do{count=read(failurePipe[0],&failure,sizeof(failure));}while(count<0&&errno==EINTR);
  close(failurePipe[0]);
  if(count>0){while(waitpid(child,nullptr,0)<0&&errno==EINTR){}return failure?failure:EIO;}
  return count<0?errno:0;
}
}
#endif
