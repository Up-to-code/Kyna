// Owns Android HTTPS through Qt Network, avoiding host libcurl during cross-compilation.
#include "../host_private.hpp"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>
namespace kyna::detail {
namespace {
class QtNetwork final: public NetworkPort {
 std::optional<NetworkResponse> send(const NetworkRequest &input,NetworkFailure &failure) override {
  QNetworkAccessManager manager;QNetworkRequest request{QUrl(QString::fromStdString(input.url))};
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);
  for(auto &[key,value]:input.headers)request.setRawHeader(QByteArray::fromStdString(key),QByteArray::fromStdString(value));
  QByteArray data;if(input.body)data=QByteArray(input.body->data(),qsizetype(input.body->size()));
  auto reply=manager.sendCustomRequest(request,QByteArray::fromStdString(input.method),data);
  QEventLoop loop;QTimer timer;timer.setSingleShot(true);QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);QObject::connect(&timer,&QTimer::timeout,reply,&QNetworkReply::abort);timer.start(int(input.timeout.count()));loop.exec();
  if(reply->error()!=QNetworkReply::NoError){failure.message=reply->errorString().toStdString();failure.nativeCode=reply->error();return std::nullopt;}
  NetworkResponse result;result.status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();auto body=reply->readAll();result.body.assign(body.constData(),body.size());result.effectiveUrl=reply->url().toString().toStdString();for(auto &pair:reply->rawHeaderPairs())result.headers[pair.first.toStdString()]=pair.second.toStdString();return result;
 }
};
}
std::shared_ptr<NetworkPort> makeCurlNetwork(){return std::make_shared<QtNetwork>();}
}
