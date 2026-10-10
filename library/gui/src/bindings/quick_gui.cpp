// Owns Qt Quick object creation and synchronous VM-thread event delivery.
#include <kyna/native/adapter.hpp>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QPointer>
#include <QTimer>
namespace {
using namespace kyna::adapter;
struct Item {QPointer<QObject> object;std::shared_ptr<Item> parent;~Item(){if(object)delete object;}};
class State final:public QObject {
 Q_OBJECT
public:
 kyna_native_host_v2 host{};Resources resources;
 std::unique_ptr<QGuiApplication> application;std::unique_ptr<QQmlEngine> engine;
 std::unordered_map<uint64_t,uint64_t> callbacks;uint64_t nextEvent{1};std::string failure,failureCode;bool running{false};std::vector<uint64_t> lifecycle;
 ~State(){for(auto [_,id]:callbacks)host.release_callback(host.context,id);callbacks.clear();resources.entries.clear();engine.reset();}
 void initialize(){if(engine)return;if(!QGuiApplication::instance()){static int argc=1;static char name[]="kyna-gui";static char *argv[]{name,nullptr};application=std::make_unique<QGuiApplication>(argc,argv);}engine=std::make_unique<QQmlEngine>();engine->rootContext()->setContextProperty("kynaEvents",this);
 QObject::connect(qobject_cast<QGuiApplication*>(QGuiApplication::instance()),&QGuiApplication::applicationStateChanged,this,[this](Qt::ApplicationState state){
  const char *name=state==Qt::ApplicationActive?"active":state==Qt::ApplicationSuspended?"suspended":state==Qt::ApplicationHidden?"hidden":"inactive";
  if(!running)return;
  auto events=lifecycle;for(auto event:events){auto it=callbacks.find(event);if(it==callbacks.end())continue;Value argument{};argument.kind=KYNA_V2_STRING;argument.text=name;argument.size=std::char_traits<char>::length(name);auto result=host.invoke_callback(host.context,it->second,&argument,1);if(result.error_code||result.error_message){failureCode=result.error_code?result.error_code:"KNATIVE1005";failure=result.error_message?result.error_message:"lifecycle callback failed";QGuiApplication::quit();}if(result.release)result.release(result.context);}
 });}
 Q_INVOKABLE void fire(qulonglong event){auto it=callbacks.find(event);if(it==callbacks.end())return;auto result=host.invoke_callback(host.context,it->second,nullptr,0);if(result.error_code||result.error_message){failureCode=result.error_code?result.error_code:"KNATIVE1005";failure=result.error_message?result.error_message:"GUI callback failed";QGuiApplication::quit();}if(result.release)result.release(result.context);}
 Value make(const char *qml,const Value *parent=nullptr){initialize();QQmlComponent c(engine.get());c.setData(qml,QUrl());if(c.isError())throw std::runtime_error(c.errorString().toStdString());QObject *o=c.create();if(!o)throw std::runtime_error(c.errorString().toStdString());auto item=std::make_shared<Item>();item->object=o;if(parent){auto p=resources.get<Item>(*parent,"GuiItem");item->parent=p;if(!p->object)throw std::runtime_error("GUI parent was destroyed");auto child=qobject_cast<QQuickItem*>(o);auto root=qobject_cast<QQuickItem*>(p->object.data());if(auto window=qobject_cast<QQuickWindow*>(p->object.data()))root=window->contentItem();if(!child||!root)throw std::runtime_error("GUI parent must be a window or layout");child->setParentItem(root);o->setParent(root);}return resources.put("GuiItem",item);}
 QObject *object(const Value &v){auto i=resources.get<Item>(v,"GuiItem");if(!i->object)throw std::runtime_error("GUI object was destroyed");return i->object;}
 void releaseEvent(uint64_t event){auto it=callbacks.find(event);if(it==callbacks.end())return;host.release_callback(host.context,it->second);callbacks.erase(it);}
 uint64_t subscribe(uint64_t id){if(!host.retain_callback(host.context,id))throw std::runtime_error("callback could not be retained");auto event=nextEvent++;callbacks[event]=id;return event;}
};
constexpr Type item{KYNA_V2_RESOURCE,0,nullptr,"GuiItem"};constexpr Type callback{KYNA_V2_CALLBACK,0,nullptr,nullptr,nullptr,0,&nothing};
constexpr Type lifecycleCallback{KYNA_V2_CALLBACK,0,nullptr,nullptr,&text,1,&nothing};
constexpr Type lifecycleArgs[]{lifecycleCallback};
constexpr Type windowArgs[]{text,integer,integer},labelArgs[]{item,text},layoutArgs[]{item},buttonArgs[]{item,text,callback},timerArgs[]{integer,callback},eventArgs[]{integer},drawArgs[]{item,matrix,text};
kyna_native_result_v2 window(void*p,const Value*a,size_t) noexcept{return guarded([&](Buffers&){auto s=static_cast<State*>(p);auto v=s->make("import QtQuick\nWindow { visible: true; color: '#fafafa' }");auto o=s->object(v);o->setProperty("title",QString::fromStdString(string(a[0])));o->setProperty("width",int(a[1].integer));o->setProperty("height",int(a[2].integer));return v;});}
kyna_native_result_v2 label(void*p,const Value*a,size_t) noexcept{return guarded([&](Buffers&){auto s=static_cast<State*>(p);auto v=s->make("import QtQuick\nText { font.pixelSize: 18; color: '#202020' }",a);s->object(v)->setProperty("text",QString::fromStdString(string(a[1])));return v;});}
kyna_native_result_v2 input(void*p,const Value*a,size_t) noexcept{return guarded([&](Buffers&){auto s=static_cast<State*>(p);auto v=s->make("import QtQuick\nTextInput { width: 220; height: 36; font.pixelSize: 18; selectByMouse: true; focus: true; Accessible.role: Accessible.EditableText; Accessible.name: 'Text input' }",a);s->object(v)->setProperty("text",QString::fromStdString(string(a[1])));return v;});}
kyna_native_result_v2 row(void*p,const Value*a,size_t) noexcept{return guarded([&](Buffers&){return static_cast<State*>(p)->make("import QtQuick\nRow { spacing: 12 }",a);});}
kyna_native_result_v2 column(void*p,const Value*a,size_t) noexcept{return guarded([&](Buffers&){return static_cast<State*>(p)->make("import QtQuick\nColumn { spacing: 12; x: 16; y: 16 }",a);});}
kyna_native_result_v2 button(void*p,const Value*a,size_t) noexcept{return guarded([&](Buffers&){auto s=static_cast<State*>(p);auto v=s->make("import QtQuick\nRectangle { property string text; property double eventId; activeFocusOnTab: true; Accessible.role: Accessible.Button; Accessible.name: text; Keys.onReturnPressed: kynaEvents.fire(eventId); Keys.onSpacePressed: kynaEvents.fire(eventId); border.width: activeFocus ? 3 : 0; border.color: '#111111'; width: 220; height: 44; radius: 6; color: mouse.pressed ? '#184f9c' : '#2368bd'; Text { anchors.centerIn: parent; text: parent.text; color: 'white'; font.pixelSize: 18 } MouseArea { id: mouse; anchors.fill: parent; onClicked: kynaEvents.fire(parent.eventId) } }",a);auto o=s->object(v);o->setProperty("text",QString::fromStdString(string(a[1])));auto event=s->subscribe(a[2].handle);o->setProperty("eventId",double(event));QObject::connect(o,&QObject::destroyed,s,[s,event]{s->releaseEvent(event);});return v;});}
kyna_native_result_v2 read(void*p,const Value*a,size_t) noexcept{return guarded([&](Buffers &b){return b.string(static_cast<State*>(p)->object(a[0])->property("text").toString().toStdString());});}
kyna_native_result_v2 after(void*p,const Value*a,size_t) noexcept{return guarded([&](Buffers&){auto s=static_cast<State*>(p);s->initialize();if(a[0].integer<0||a[0].integer>2147483647)throw std::runtime_error("timer duration is out of range");auto event=s->subscribe(a[1].handle);QTimer::singleShot(int(a[0].integer),s,[s,event]{s->fire(event);auto it=s->callbacks.find(event);if(it!=s->callbacks.end()){s->host.release_callback(s->host.context,it->second);s->callbacks.erase(it);}});return whole(event);});}
kyna_native_result_v2 unsubscribe(void*p,const Value*a,size_t) noexcept{return guarded([&](Buffers&){auto s=static_cast<State*>(p);auto it=s->callbacks.find(a[0].integer);if(it==s->callbacks.end())throw std::runtime_error("unknown event subscription");s->host.release_callback(s->host.context,it->second);s->callbacks.erase(it);return Value{};});}
kyna_native_result_v2 draw(void*p,const Value*a,size_t) noexcept{return guarded([&](Buffers&){auto s=static_cast<State*>(p);auto v=s->make("import QtQuick\nCanvas { width: 640; height: 400; property var points: []; property string stroke; onPaint: { var c=getContext('2d');c.clearRect(0,0,width,height);c.strokeStyle=stroke;c.lineWidth=2;c.beginPath();for(var i=0;i<points.length;i++){if(i===0)c.moveTo(points[i][0],points[i][1]);else c.lineTo(points[i][0],points[i][1]);}c.closePath();c.stroke();} }",a);QVariantList points;for(size_t i=0;i<a[1].size;++i){auto &q=a[1].items[i];if(q.size!=2)throw std::runtime_error("drawing points require two coordinates");points.push_back(QVariantList{finite(q.items[0]),finite(q.items[1])});}auto o=s->object(v);o->setProperty("points",points);o->setProperty("stroke",QString::fromStdString(string(a[2])));return v;});}
kyna_native_result_v2 onLifecycle(void*p,const Value*a,size_t) noexcept{return guarded([&](Buffers&){auto s=static_cast<State*>(p);s->initialize();auto event=s->subscribe(a[0].handle);s->lifecycle.push_back(event);return whole(event);});}
kyna_native_result_v2 run(void*p,const Value*,size_t) noexcept{return guarded([&](Buffers&){auto s=static_cast<State*>(p);s->initialize();if(s->running)throw std::runtime_error("GUI event loop is already running");s->running=true;QGuiApplication::exec();s->running=false;if(!s->failure.empty())throw Failure(s->failureCode,s->failure);return Value{};});}
kyna_native_result_v2 quit(void*,const Value*,size_t) noexcept{return guarded([&](Buffers&){QGuiApplication::quit();return Value{};});}
const kyna_native_function_v2 functions[]{ {"guiWindow",windowArgs,3,item,window},{"guiText",labelArgs,2,item,label},{"guiInput",labelArgs,2,item,input},{"guiRow",layoutArgs,1,item,row},{"guiColumn",layoutArgs,1,item,column},{"guiButton",buttonArgs,3,item,button},{"guiRead",layoutArgs,1,text,read},{"guiAfter",timerArgs,2,integer,after},{"guiUnsubscribe",eventArgs,1,nothing,unsubscribe},{"guiDraw",drawArgs,3,item,draw},{"guiRun",nullptr,0,nothing,run},{"guiQuit",nullptr,0,nothing,quit},{"guiOnLifecycle",lifecycleArgs,1,integer,onLifecycle} };
int validGui(void *p,uint64_t id,const char *type) noexcept {auto s=static_cast<State*>(p);if(!s->resources.valid(id,type))return 0;auto it=s->resources.entries.find(id);return bool(std::static_pointer_cast<Item>(it->second.object)->object);}
const kyna_native_module_v2_descriptor module{2,sizeof(module),"gui","1.0.16",functions,13,create<State>,close<State>,validGui,destroy<State>};
}
extern "C" KYNA_NATIVE_EXPORT const kyna_native_module_v2_descriptor *kyna_native_module_v2(){return &module;}
#include "quick_gui.moc"
