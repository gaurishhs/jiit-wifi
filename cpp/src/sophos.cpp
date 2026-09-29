#include "jiit/sophos.h"
#include <QDateTime>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QXmlStreamReader>

namespace Jiit {
QString epochTimestamp() {
  return QString::number(QDateTime::currentMSecsSinceEpoch() / 10);
}
Result classifyMessage(const QString &message) {
  const QString m = message.toLower();
  if (m.contains("already") || m.contains("logged in from") ||
      m.contains("signed in as"))
    return Result::AlreadyLoggedIn;
  if (m.contains("fail") || m.contains("invalid") || m.contains("incorrect") ||
      m.contains("denied"))
    return Result::Failed;
  if (m.contains("success") || m.contains("logged in"))
    return Result::Success;
  return Result::Unknown;
}
QByteArray SophosClient::loginPayload(const QString &u, const QString &p,
                                      const QString &t) {
  QUrlQuery q;
  q.addQueryItem("mode", "191");
  q.addQueryItem("username", u);
  q.addQueryItem("password", p);
  q.addQueryItem("a", t);
  q.addQueryItem("producttype", "0");
  return q.query(QUrl::FullyEncoded).toUtf8();
}
QByteArray SophosClient::logoutPayload(const QString &u, const QString &t) {
  QUrlQuery q;
  q.addQueryItem("mode", "193");
  q.addQueryItem("username", u);
  q.addQueryItem("a", t);
  q.addQueryItem("producttype", "0");
  return q.query(QUrl::FullyEncoded).toUtf8();
}
SophosClient::SophosClient(QObject *parent) : QObject(parent) {
  qRegisterMetaType<SophosResult>();
}
SophosResult SophosClient::parseResponse(const QByteArray &xml) {
  QXmlStreamReader reader(xml);
  QString message;
  while (!reader.atEnd()) {
    reader.readNext();
    if (reader.isStartElement() && reader.name() == "message")
      message = reader.readElementText().trimmed();
  }
  if (reader.hasError() || message.isEmpty())
    return {Result::Unknown,
            QString("Malformed Sophos XML: ") + reader.errorString(),
            QString::fromUtf8(xml.left(2048))};
  return {classifyMessage(message), message, QString::fromUtf8(xml.left(2048))};
}
void SophosClient::checkPortal() {
  QNetworkRequest req{QUrl(m_gateway)};
  req.setTransferTimeout(m_timeout * 1000);
  auto *reply = m_manager.get(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool ok = status > 0 && status < 500;
    reply->deleteLater();
    emit portalChecked(ok);
  });
}
void SophosClient::login(const QString &u, const QString &p) {
  post("login.xml", loginPayload(u, p, epochTimestamp()));
}
void SophosClient::logout(const QString &u) {
  post("logout.xml", logoutPayload(u, epochTimestamp()));
}
void SophosClient::post(const QString &endpoint, const QByteArray &body,
                        int attempt) {
  QUrl base(m_gateway);
  if (!base.path().endsWith('/'))
    base.setPath(base.path() + "/");
  base.setPath(base.path() + endpoint);
  QNetworkRequest req(base);
  req.setHeader(QNetworkRequest::ContentTypeHeader,
                "application/x-www-form-urlencoded");
  req.setTransferTimeout(m_timeout * 1000);
  auto *reply = m_manager.post(req, body);
  connect(
      reply, &QNetworkReply::finished, this,
      [this, reply, attempt, endpoint, body] {
        const auto status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto err = reply->error();
        const QByteArray bytes = reply->readAll();
        if (((status == 0 && err != QNetworkReply::NoError) || status >= 500) &&
            attempt < m_retries) {
          reply->deleteLater();
          QTimer::singleShot(qMin(12000, 1000 * (1 << qMin(attempt, 4))), this,
                             [this, endpoint, body, attempt] {
                               post(endpoint, body, attempt + 1);
                             });
          return;
        }
        SophosResult result;
        if (status >= 400)
          result = {Result::Failed, QString("HTTP %1").arg(status), {}};
        else if (err != QNetworkReply::NoError)
          result = {Result::NetworkError, reply->errorString(), {}};
        else
          result = parseResponse(bytes);
        reply->deleteLater();
        emit completed(result);
      });
}
} // namespace Jiit
