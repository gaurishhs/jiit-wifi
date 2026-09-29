#pragma once
#include "types.h"
#include <QNetworkAccessManager>
#include <QObject>
namespace Jiit {
class SophosClient final : public QObject {
  Q_OBJECT
public:
  explicit SophosClient(QObject *parent = nullptr);
  void setGateway(const QString &gateway) { m_gateway = gateway; }
  void setTimeout(int seconds) { m_timeout = seconds; }
  void setRetries(int retries) { m_retries = retries; }
  void checkPortal();
  void login(const QString &username, const QString &password);
  void logout(const QString &username);
  static SophosResult parseResponse(const QByteArray &xml);
  static QByteArray loginPayload(const QString &username,
                                 const QString &password,
                                 const QString &timestamp);
  static QByteArray logoutPayload(const QString &username,
                                  const QString &timestamp);
signals:
  void portalChecked(bool reachable);
  void completed(const Jiit::SophosResult &result);

private:
  void post(const QString &endpoint, const QByteArray &body, int attempt = 0);
  QNetworkAccessManager m_manager;
  QString m_gateway{"http://172.16.68.6:8090/"};
  int m_timeout{8}, m_retries{3};
};
} // namespace Jiit
Q_DECLARE_METATYPE(Jiit::SophosResult)
