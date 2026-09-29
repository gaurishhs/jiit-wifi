#pragma once
#include "service.h"
#include <QDBusAbstractAdaptor>
namespace Jiit {
class FirewallAdaptor final : public QDBusAbstractAdaptor {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.jiit.FirewallManager")
public:
  explicit FirewallAdaptor(FirewallService *service);
public slots:
  QString GetStatus() const;
  QString GetAccounts() const;
  QString GetLogs() const;
  QString GetSettings() const;
  QString SetSettings(const QString &settings);
  QString SaveAccount(const QString &account);
  QString RemoveAccount(const QString &id);
  QString Login();
  QString Logout();
  QString Retry();
  QString StartXray();
  QString StopXray();
  QString DetectCurrentNetwork() const;
signals:
  void StatusChanged(const QString &status);
  void LoginStateChanged(const QString &status);
  void XrayStateChanged(const QString &status);
  void NetworkStateChanged(const QString &status);
  void LogMessage(const QString &message);

private:
  FirewallService *m_service;
};
} // namespace Jiit
