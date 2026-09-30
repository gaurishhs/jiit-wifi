#include "jiit/dbus_adaptor.h"

namespace Jiit {
FirewallAdaptor::FirewallAdaptor(FirewallService *s)
    : QDBusAbstractAdaptor(s), m_service(s) {
  connect(s, &FirewallService::statusChanged, this,
          &FirewallAdaptor::StatusChanged);
  connect(s, &FirewallService::loginStateChanged, this,
          &FirewallAdaptor::LoginStateChanged);
  connect(s, &FirewallService::xrayStateChanged, this,
          &FirewallAdaptor::XrayStateChanged);
  connect(s, &FirewallService::networkStateChanged, this,
          &FirewallAdaptor::NetworkStateChanged);
  connect(s, &FirewallService::logMessage, this, &FirewallAdaptor::LogMessage);
}
QString FirewallAdaptor::GetStatus() const {
  return QString::fromUtf8(
      QJsonDocument(m_service->status()).toJson(QJsonDocument::Compact));
}
QString FirewallAdaptor::GetAccounts() const {
  return m_service->getAccounts();
}
QString FirewallAdaptor::GetLogs() const { return m_service->getLogs(); }
QString FirewallAdaptor::GetSettings() const {
  return m_service->getSettings();
}
QString FirewallAdaptor::SetSettings(const QString &s) {
  return m_service->setSettings(s);
}
QString FirewallAdaptor::SaveAccount(const QString &a) {
  return m_service->saveAccount(a);
}
QString FirewallAdaptor::RemoveAccount(const QString &id) {
  return m_service->removeAccount(id);
}
QString FirewallAdaptor::Login() { return m_service->login(); }
QString FirewallAdaptor::Logout() { return m_service->logout(); }
QString FirewallAdaptor::LogoutForNetworkDisconnect() {
  return m_service->logoutForNetworkDisconnect();
}
QString FirewallAdaptor::Retry() { return m_service->retry(); }
QString FirewallAdaptor::StartXray() { return m_service->startXray(); }
QString FirewallAdaptor::StopXray() { return m_service->stopXray(); }
QString FirewallAdaptor::DetectCurrentNetwork() const {
  return m_service->detectCurrentNetwork();
}
} // namespace Jiit
