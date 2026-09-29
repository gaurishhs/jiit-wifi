#include "jiit/wallet.h"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

namespace Jiit {
static constexpr auto service = "org.kde.kwalletd6";
static constexpr auto path = "/modules/kwalletd6";
static constexpr auto iface = "org.kde.KWallet";
int WalletStore::openWallet(QString *error) const {
  QDBusInterface wallet(service, path, iface, QDBusConnection::sessionBus());
  if (!wallet.isValid()) {
    if (error)
      *error = wallet.lastError().message();
    return -1;
  }
  auto reply = wallet.call("open", "kdewallet", qlonglong(0),
                           QCoreApplication::applicationName());
  if (reply.type() == QDBusMessage::ErrorMessage) {
    if (error)
      *error = reply.errorMessage();
    return -1;
  }
  const int handle = reply.arguments().value(0).toInt();
  if (handle < 0 && error)
    *error = "KDE Wallet is locked or unavailable";
  return handle;
}
bool WalletStore::write(const QString &id, const QString &password,
                        QString *error) const {
  const int h = openWallet(error);
  if (h < 0)
    return false;
  QDBusInterface w(service, path, iface, QDBusConnection::sessionBus());
  if (!w.call("hasFolder", h, "JIIT Firewall",
              QCoreApplication::applicationName())
           .arguments()
           .value(0)
           .toBool()) {
    auto r = w.call("createFolder", h, "JIIT Firewall",
                    QCoreApplication::applicationName());
    if (r.type() == QDBusMessage::ErrorMessage) {
      if (error)
        *error = r.errorMessage();
      return false;
    }
  }
  auto r = w.call("writePassword", h, "JIIT Firewall", id, password,
                  QCoreApplication::applicationName());
  if (r.type() == QDBusMessage::ErrorMessage) {
    if (error)
      *error = r.errorMessage();
    return false;
  }
  return r.arguments().value(0).toInt() == 0;
}
QString WalletStore::read(const QString &id, QString *error) const {
  const int h = openWallet(error);
  if (h < 0)
    return {};
  QDBusInterface w(service, path, iface, QDBusConnection::sessionBus());
  auto r = w.call("readPassword", h, "JIIT Firewall", id,
                  QCoreApplication::applicationName());
  if (r.type() == QDBusMessage::ErrorMessage) {
    if (error)
      *error = r.errorMessage();
    return {};
  }
  return r.arguments().value(0).toString();
}
bool WalletStore::remove(const QString &id, QString *error) const {
  const int h = openWallet(error);
  if (h < 0)
    return false;
  QDBusInterface w(service, path, iface, QDBusConnection::sessionBus());
  auto r = w.call("removeEntry", h, "JIIT Firewall", id,
                  QCoreApplication::applicationName());
  if (r.type() == QDBusMessage::ErrorMessage) {
    if (error)
      *error = r.errorMessage();
    return false;
  }
  return r.arguments().value(0).toInt() == 0;
}
} // namespace Jiit
