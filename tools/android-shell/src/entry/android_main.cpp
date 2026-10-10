// Owns the Android Qt application shell and extraction of bundled Kyna modules.
#include <kyna/language/language_session.hpp>
#include <kyna/language/native_modules.hpp>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QDirIterator>
#include <QFile>
#include <QDebug>
#include <dlfcn.h>
namespace {
void libraryLocationAnchor() {}
std::filesystem::path guiLibraryPath() {
 Dl_info location{};
 if (!dladdr(reinterpret_cast<void *>(&libraryLocationAnchor), &location) || !location.dli_fname)
  throw std::runtime_error("cannot locate Android application native libraries");
 return std::filesystem::path(location.dli_fname).parent_path() / KYNA_ANDROID_GUI_LIBRARY;
}
}
int main(int argc,char **argv){
 QGuiApplication app(argc,argv);
 try {
  auto root=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/app";QDir().mkpath(root);
  QDirIterator files(":/kyna",QDir::Files,QDirIterator::Subdirectories);
  while(files.hasNext()){auto source=files.next();auto relative=source.mid(QString(":/kyna/").size());auto target=root+"/"+relative;QDir().mkpath(QFileInfo(target).path());QFile::remove(target);if(!QFile::copy(source,target))throw std::runtime_error("cannot extract bundled Kyna source");}
  QDir::setCurrent(root);kyna::LanguageSessionOptions options;options.modulePaths.push_back(root.toStdString());
  options.nativeFunctions=kyna::loadNativeModule(guiLibraryPath());
  qInfo("KYNA_ANDROID_STARTED");
  bool success=false;
  { kyna::LanguageSession session(std::move(options));auto result=session.run(std::filesystem::path("main.ky"));
  for(auto &d:result.diagnostics)qCritical().noquote()<<QString::fromStdString(d.code+": "+d.message);
  success=result.ok();if(!success)qCritical("KANDROID1002: Kyna application failed"); }
  if(success)qInfo("KYNA_ANDROID_SMOKE_OK");return success?0:1;
 }catch(const std::exception &e){qCritical("KANDROID1001: %s",e.what());return 2;}
}
