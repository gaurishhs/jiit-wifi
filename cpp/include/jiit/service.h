#pragma once
#include "network.h"
#include "sophos.h"
#include "wallet.h"
#include "xray.h"
#include <QDBusUnixFileDescriptor>
#include <QObject>
#include <QTimer>
namespace Jiit {
class FirewallService final : public QObject {
  Q_OBJECT
public:
  explicit FirewallService(QObject *parent = nullptr);
  QJsonObject status() const;
  QString getSettings() const;
  QString getAccounts() const;
  QString getLogs() const;
  QString setSettings(const QString &json);
  QString saveAccount(const QString &json);
  QString removeAccount(const QString &id);
  QString login();
  QString logout();
  QString logoutForNetworkDisconnect();
  QString retry();
  QString startXray();
  QString stopXray();
  QString detectCurrentNetwork() const;
public slots:
  void prepareForPowerDevilSuspend();
  void prepareForSleep(bool sleeping);
signals:
  void statusChanged(const QString &json);
  void loginStateChanged(const QString &json);
  void xrayStateChanged(const QString &json);
  void networkStateChanged(const QString &json);
  void logMessage(const QString &message);
private slots:
  void networkChanged(const NetworkSnapshot &network);
  void portalChecked(bool reachable);
  void sophosCompleted(const SophosResult &result);
  void xrayOperation(const QString &action, bool ok, const QString &detail);

private:
  void publish();
  void setState(State state);
  void beginLogin();
  void tryNextAccount();
  void handleLeaving();
  void notifyUser(const QString &title, const QString &message);
  void acquireSleepInhibitor();
  void releaseSleepInhibitor();
  void finishSleepPreparation();
  void beginResumeRefresh();
  Settings m_settings;
  NetworkMonitor m_network;
  SophosClient m_sophos;
  WalletStore m_wallet;
  XrayManager m_xray;
  NetworkSnapshot m_snapshot;
  State m_state{State::Disconnected};
  int m_accountIndex{0}, m_portalAttempts{0};
  bool m_portalReachable{false}, m_authenticated{false}, m_ownsXray{false},
      m_busy{false}, m_sleeping{false}, m_manualLogout{false},
      m_loggingOut{false}, m_resumeReauthPending{false},
      m_resumeReloginAfterLogout{false},
      m_wasOnConfiguredNetworkBeforeSleep{false};
  QString m_accountId, m_accountName, m_accountUser, m_resumeLogoutUser,
      m_xrayStatus{"Unknown"};
  QDBusUnixFileDescriptor m_sleepDelayInhibitor;
  QTimer m_retryTimer, m_debounce, m_sleepLogoutDeadline;
};
} // namespace Jiit
