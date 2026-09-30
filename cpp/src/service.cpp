#include "jiit/service.h"
#include "jiit/config.h"
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <algorithm>

namespace Jiit {
FirewallService::FirewallService(QObject *parent)
    : QObject(parent), m_settings(loadSettings()), m_network(this),
      m_sophos(this), m_xray(this) {
  m_sophos.setGateway(m_settings.gateway);
  m_sophos.setTimeout(m_settings.timeoutSeconds);
  m_sophos.setRetries(m_settings.retryCount);
  m_network.setInterval(m_settings.pollIntervalSeconds);
  m_xray.configure(m_settings.xrayMode, m_settings.xrayService);
  m_sleepLogoutDeadline.setSingleShot(true);
  connect(&m_sleepLogoutDeadline, &QTimer::timeout, this, [this] {
    qWarning() << "Sleep logout exceeded the logind delay window";
    releaseSleepInhibitor();
  });
  acquireSleepInhibitor();
  connect(&m_network, &NetworkMonitor::changed, this,
          &FirewallService::networkChanged);
  connect(&m_sophos, &SophosClient::portalChecked, this,
          &FirewallService::portalChecked);
  connect(&m_sophos, &SophosClient::completed, this,
          &FirewallService::sophosCompleted);
  connect(&m_xray, &XrayManager::operationFinished, this,
          &FirewallService::xrayOperation);
  connect(&m_xray, &XrayManager::stateChanged, this, [this](const QString &s) {
    m_xrayStatus = s;
    emit xrayStateChanged(QString::fromUtf8(
        QJsonDocument(status()).toJson(QJsonDocument::Compact)));
    publish();
  });
  m_retryTimer.setSingleShot(true);
  connect(&m_retryTimer, &QTimer::timeout, this, [this] {
    if (m_busy && m_state == State::PortalUnavailable)
      m_sophos.checkPortal();
    else if (!m_busy && !m_authenticated && !m_manualLogout &&
             matches(m_snapshot, m_settings.networks))
      beginLogin();
  });
  m_debounce.setSingleShot(true);
  m_debounce.setInterval(8000);
  connect(&m_debounce, &QTimer::timeout, this, &FirewallService::handleLeaving);
}
QJsonObject FirewallService::status() const {
  QString profile;
  for (const auto &p : m_settings.networks)
    if (p.enabled && m_snapshot.connected && p.ssid == m_snapshot.ssid &&
        (p.interfaces.isEmpty() ||
         p.interfaces.contains(m_snapshot.interfaceName)) &&
        (p.bssids.isEmpty() || p.bssids.contains(m_snapshot.bssid.toLower()))) {
      profile = p.name;
      break;
    }
  return {{"state", stateName(m_state)},
          {"network", m_snapshot.toJson()},
          {"profile", profile},
          {"portal_reachable", m_portalReachable},
          {"authenticated", m_authenticated},
          {"account", m_accountName},
          {"xray", m_xrayStatus},
          {"automation", m_busy}};
}
QString FirewallService::getSettings() const {
  return QString::fromUtf8(
      QJsonDocument(m_settings.toJson()).toJson(QJsonDocument::Compact));
}
QString FirewallService::getAccounts() const {
  QJsonArray a;
  for (const auto &x : m_settings.accounts)
    a.append(QJsonObject{{"id", x.id},
                         {"name", x.name},
                         {"username", x.username},
                         {"priority", x.priority},
                         {"enabled", x.enabled}});
  return QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact));
}
QString FirewallService::getLogs() const {
  const QString path =
      QStandardPaths::writableLocation(QStandardPaths::StateLocation) +
      "/daemon.log";
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return "No log file is available yet.";
  return QString::fromUtf8(file.readAll().right(100000));
}
QString FirewallService::setSettings(const QString &json) {
  QJsonParseError e;
  auto doc = QJsonDocument::fromJson(json.toUtf8(), &e);
  if (e.error != QJsonParseError::NoError || !doc.isObject())
    return R"({"ok":false,"message":"Invalid settings JSON"})";
  Settings next = Settings::fromJson(doc.object());
  const QUrl gateway(next.gateway);
  static const QRegularExpression unitName("^[A-Za-z0-9_.@:-]+$");
  if ((gateway.scheme() != "http" && gateway.scheme() != "https") ||
      gateway.host().isEmpty() ||
      (next.xrayMode != "system" && next.xrayMode != "user") ||
      !unitName.match(next.xrayService).hasMatch() || next.timeoutSeconds < 1 ||
      next.timeoutSeconds > 120 || next.retryCount < 0 ||
      next.retryCount > 12 || next.retryIntervalSeconds < 2 ||
      next.retryIntervalSeconds > 600 || next.pollIntervalSeconds < 2 ||
      next.pollIntervalSeconds > 120) {
    return R"({"ok":false,"message":"Settings contain an invalid URL, unit name or out-of-range value"})";
  }
  QString error;
  if (!saveSettings(next, &error))
    return QString::fromUtf8(
        QJsonDocument(QJsonObject{{"ok", false}, {"message", error}})
            .toJson(QJsonDocument::Compact));
  m_settings = next;
  m_sophos.setGateway(next.gateway);
  m_sophos.setTimeout(next.timeoutSeconds);
  m_sophos.setRetries(next.retryCount);
  m_network.setInterval(next.pollIntervalSeconds);
  m_xray.configure(next.xrayMode, next.xrayService);
  if (next.logoutBeforeSleep)
    acquireSleepInhibitor();
  else
    releaseSleepInhibitor();
  if (matches(m_snapshot, m_settings.networks) && !m_authenticated && !m_busy &&
      !m_manualLogout && !m_resumeReauthPending)
    beginLogin();
  publish();
  return R"({"ok":true})";
}
QString FirewallService::saveAccount(const QString &json) {
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject())
    return R"({"ok":false,"message":"Invalid account data"})";

  const auto object = document.object();
  const QString name = object.value("name").toString().trimmed();
  const QString username = object.value("username").toString().trimmed();
  const QString password = object.value("password").toString();
  QString id = object.value("id").toString().trimmed();
  const bool isNew = id.isEmpty();
  if (name.isEmpty() || username.isEmpty() || (isNew && password.isEmpty()))
    return R"({"ok":false,"message":"Name, Sophos ID, and a password for new accounts are required"})";

  Settings next = m_settings;
  int index = -1;
  for (int i = 0; i < next.accounts.size(); ++i)
    if (next.accounts[i].id == id) {
      index = i;
      break;
    }
  if (isNew) {
    id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    next.accounts.append(
        {id, name, username, static_cast<int>(next.accounts.size() + 1), true});
    index = next.accounts.size() - 1;
  } else if (index < 0) {
    return R"({"ok":false,"message":"Account no longer exists; reload the configuration"})";
  } else {
    next.accounts[index].name = name;
    next.accounts[index].username = username;
  }
  next.accounts[index].priority =
      qMax(1, object.value("priority").toInt(next.accounts[index].priority));
  next.accounts[index].enabled =
      object.value("enabled").toBool(next.accounts[index].enabled);

  if (!password.isEmpty()) {
    QString walletError;
    if (!m_wallet.write(id, password, &walletError))
      return QString::fromUtf8(
          QJsonDocument(
              QJsonObject{
                  {"ok", false},
                  {"message", walletError.isEmpty()
                                  ? "Could not save password to KDE Wallet"
                                  : walletError}})
              .toJson(QJsonDocument::Compact));
  }

  QString saveError;
  if (!saveSettings(next, &saveError)) {
    if (isNew)
      m_wallet.remove(id);
    return QString::fromUtf8(
        QJsonDocument(QJsonObject{{"ok", false}, {"message", saveError}})
            .toJson(QJsonDocument::Compact));
  }
  m_settings = next;
  publish();
  if (matches(m_snapshot, m_settings.networks) && !m_authenticated && !m_busy &&
      !m_manualLogout && !m_resumeReauthPending)
    beginLogin();
  return QString::fromUtf8(
      QJsonDocument(
          QJsonObject{{"ok", true}, {"id", id}, {"message", "Account saved"}})
          .toJson(QJsonDocument::Compact));
}
QString FirewallService::removeAccount(const QString &id) {
  Settings next = m_settings;
  const auto it =
      std::find_if(next.accounts.begin(), next.accounts.end(),
                   [&id](const Account &account) { return account.id == id; });
  if (it == next.accounts.end())
    return R"({"ok":false,"message":"Account not found"})";
  next.accounts.erase(it);
  QString saveError;
  if (!saveSettings(next, &saveError))
    return QString::fromUtf8(
        QJsonDocument(QJsonObject{{"ok", false}, {"message", saveError}})
            .toJson(QJsonDocument::Compact));
  m_settings = next;
  QString walletError;
  m_wallet.remove(id, &walletError);
  publish();
  return QString::fromUtf8(
      QJsonDocument(
          QJsonObject{{"ok", true},
                      {"message", walletError.isEmpty()
                                      ? "Account removed"
                                      : "Account removed; KDE Wallet could not "
                                        "delete its saved password"}})
          .toJson(QJsonDocument::Compact));
}
QString FirewallService::login() {
  m_manualLogout = false;
  if (m_busy)
    return R"({"ok":false,"message":"Automation is already running"})";
  if (!matches(m_snapshot, m_settings.networks))
    return R"({"ok":false,"message":"Not on a configured JIIT network"})";
  m_retryTimer.stop();
  beginLogin();
  return R"({"ok":true,"message":"Login workflow started"})";
}
QString FirewallService::logout() {
  m_manualLogout = true;
  m_retryTimer.stop();
  if (m_busy)
    return R"({"ok":false,"message":"Automation is busy"})";
  m_busy = true;
  m_loggingOut = true;
  setState(State::LoggingOut);
  if (m_xray.isActive())
    m_xray.stop();
  else if (m_authenticated)
    m_sophos.logout(m_accountUser);
  else {
    m_busy = false;
    m_loggingOut = false;
    setState(State::Disconnected);
  }
  return R"({"ok":true,"message":"Logout workflow started"})";
}
QString FirewallService::logoutForNetworkDisconnect() {
  if (!m_busy && m_authenticated) {
    m_sophos.setTimeout(qMin(3, m_settings.timeoutSeconds));
    m_sophos.setRetries(0);
  }
  return logout();
}
QString FirewallService::retry() { return login(); }
QString FirewallService::startXray() {
  m_xray.start();
  return R"({"ok":true,"message":"Xray start requested"})";
}
QString FirewallService::stopXray() {
  m_ownsXray = false;
  m_xray.stop();
  return R"({"ok":true,"message":"Xray stop requested"})";
}
QString FirewallService::detectCurrentNetwork() const {
  return QString::fromUtf8(
      QJsonDocument(m_snapshot.toJson()).toJson(QJsonDocument::Compact));
}
void FirewallService::publish() {
  const QString payload =
      QString::fromUtf8(QJsonDocument(status()).toJson(QJsonDocument::Compact));
  emit statusChanged(payload);
  emit loginStateChanged(payload);
}
void FirewallService::setState(State s) {
  m_state = s;
  qInfo().noquote() << "state" << stateName(s) << "ssid" << m_snapshot.ssid
                    << "bssid" << m_snapshot.bssid;
  emit logMessage(QString("state=%1 ssid=%2 bssid=%3")
                      .arg(stateName(s), m_snapshot.ssid, m_snapshot.bssid));
  publish();
}
void FirewallService::notifyUser(const QString &title, const QString &message) {
  QDBusInterface n(
      "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
      "org.freedesktop.Notifications", QDBusConnection::sessionBus());
  if (n.isValid())
    n.asyncCall("Notify", "JIIT Firewall Manager", uint(0), "network-wireless",
                title, message, QStringList{}, QVariantMap{}, 5000);
}
void FirewallService::networkChanged(const NetworkSnapshot &n) {
  const bool oldValid = matches(m_snapshot, m_settings.networks);
  m_snapshot = n;
  const bool valid = matches(n, m_settings.networks);
  emit networkStateChanged(QString::fromUtf8(
      QJsonDocument(status()).toJson(QJsonDocument::Compact)));
  publish();
  if (!valid && m_resumeReauthPending && !m_resumeReloginAfterLogout) {
    m_busy = false;
    m_retryTimer.stop();
  }
  if (valid) {
    m_debounce.stop();
    if (m_resumeReauthPending) {
      beginResumeRefresh();
      return;
    }
    if (!oldValid && !m_authenticated && !m_busy && !m_manualLogout &&
        !m_sleeping)
      beginLogin();
    return;
  }
  if (oldValid)
    m_debounce.start();
}
void FirewallService::handleLeaving() {
  if (matches(m_snapshot, m_settings.networks))
    return;
  m_retryTimer.stop();
  if (m_ownsXray && m_settings.stopXrayOnLeave) {
    m_busy = true;
    m_loggingOut = m_authenticated;
    setState(m_authenticated ? State::LoggingOut : State::Disconnected);
    m_xray.stop();
  } else if (m_authenticated) {
    m_busy = true;
    m_loggingOut = true;
    setState(State::LoggingOut);
    m_sophos.logout(m_accountUser);
  } else
    setState(State::Disconnected);
  m_manualLogout = false;
}
void FirewallService::beginLogin() {
  if (m_sleeping || m_busy || !matches(m_snapshot, m_settings.networks))
    return;
  m_busy = true;
  m_portalAttempts = 0;
  m_accountIndex = 0;
  setState(State::JiitNetworkDetected);
  setState(State::WaitingForPortal);
  if (m_settings.xrayEnabled)
    m_xray.query();
  else
    m_sophos.checkPortal();
}
void FirewallService::portalChecked(bool reachable) {
  if (m_sleeping || !matches(m_snapshot, m_settings.networks)) {
    m_busy = false;
    if (m_sleeping)
      finishSleepPreparation();
    return;
  }
  m_portalReachable = reachable;
  if (!reachable) {
    setState(State::PortalUnavailable);
    notifyUser("JIIT portal unavailable",
               "Waiting for the Sophos gateway to become reachable.");
    if (m_portalAttempts++ < m_settings.retryCount) {
      m_retryTimer.start(
          qMin(120000, m_settings.retryIntervalSeconds * 1000 *
                           (1 << qMin(m_portalAttempts - 1, 5))));
    } else {
      m_busy = false;
      m_retryTimer.start(qMax(30000, m_settings.retryIntervalSeconds * 1000));
    }
    return;
  }
  if (m_resumeReauthPending) {
    if (m_resumeLogoutUser.isEmpty()) {
      const auto accounts = orderedAccounts(m_settings.accounts);
      if (!accounts.isEmpty())
        m_resumeLogoutUser = accounts.front().username;
    }
    if (m_resumeLogoutUser.isEmpty()) {
      m_resumeReauthPending = false;
      m_busy = false;
      beginLogin();
      return;
    }
    m_busy = true;
    m_loggingOut = true;
    m_resumeReloginAfterLogout = true;
    setState(State::LoggingOut);
    m_sophos.logout(m_resumeLogoutUser);
    return;
  }
  m_accountIndex = 0;
  setState(State::Authenticating);
  notifyUser("JIIT sign in", "Authenticating with the Sophos portal.");
  tryNextAccount();
}
void FirewallService::tryNextAccount() {
  const QVector<Account> accounts = orderedAccounts(m_settings.accounts);
  if (m_accountIndex >= accounts.size()) {
    m_busy = false;
    setState(State::AuthenticationFailed);
    notifyUser("Sophos login failed",
               "All enabled accounts were rejected or unavailable.");
    if (!m_manualLogout)
      m_retryTimer.start(qMax(30000, m_settings.retryIntervalSeconds * 1000));
    return;
  }
  const Account a = accounts[m_accountIndex++];
  QString error;
  const QString password = m_wallet.read(a.id, &error);
  if (password.isEmpty()) {
    qWarning() << "Could not read KDE Wallet item for account" << a.name
               << error;
    tryNextAccount();
    return;
  }
  m_accountId = a.id;
  m_accountName = a.name;
  m_accountUser = a.username;
  m_sophos.login(a.username, password);
}
void FirewallService::sophosCompleted(const SophosResult &r) {
  qInfo().noquote() << "Sophos response" << resultName(r.code) << r.message;
  if (m_loggingOut) {
    const bool reloginAfterLogout = m_resumeReloginAfterLogout;
    m_authenticated = false;
    m_accountId.clear();
    m_accountName.clear();
    m_accountUser.clear();
    m_busy = false;
    m_loggingOut = false;
    m_resumeReloginAfterLogout = false;
    m_sophos.setTimeout(m_settings.timeoutSeconds);
    m_sophos.setRetries(m_settings.retryCount);
    m_sleepLogoutDeadline.stop();
    setState(m_sleeping ? State::Sleeping : State::Disconnected);
    if (m_sleeping)
      releaseSleepInhibitor();
    if (reloginAfterLogout) {
      m_resumeReauthPending = false;
      m_resumeLogoutUser.clear();
      if (matches(m_snapshot, m_settings.networks))
        beginLogin();
    }
    return;
  }
  if (m_sleeping || !matches(m_snapshot, m_settings.networks)) {
    if (r.code == Result::Success || r.code == Result::AlreadyLoggedIn) {
      m_authenticated = true;
      m_loggingOut = true;
      m_sophos.logout(m_accountUser);
    } else {
      m_busy = false;
      m_sophos.setTimeout(m_settings.timeoutSeconds);
      m_sophos.setRetries(m_settings.retryCount);
      if (m_sleeping)
        finishSleepPreparation();
    }
    return;
  }
  if (r.code == Result::Success || r.code == Result::AlreadyLoggedIn) {
    m_authenticated = true;
    m_busy = false;
    setState(State::Authenticated);
    notifyUser("Connected to JIIT Wi-Fi",
               QString("Logged in as %1").arg(m_accountName));
    if (m_settings.xrayEnabled && m_settings.startXrayAfterLogin) {
      setState(State::XrayStarting);
      m_xray.start();
    }
    return;
  }
  tryNextAccount();
}
void FirewallService::xrayOperation(const QString &action, bool ok,
                                    const QString &detail) {
  m_xrayStatus = m_xray.status();
  if (!detail.isEmpty())
    qInfo() << "Xray" << action << detail;
  if (action == "is-active" && m_busy && m_state == State::WaitingForPortal) {
    if (ok && m_xray.isActive()) {
      m_ownsXray = false;
      m_xray.stop();
    } else
      m_sophos.checkPortal();
  } else if (action == "stop" && ok) {
    m_ownsXray = false;
    if (m_busy && m_state == State::WaitingForPortal)
      m_sophos.checkPortal();
  } else if (action == "stop" && !ok && m_state == State::WaitingForPortal) {
    m_busy = false;
    setState(State::XrayError);
  }
  if (action == "stop" && m_loggingOut && m_authenticated)
    m_sophos.logout(m_accountUser);
  if (action == "start" && ok) {
    m_ownsXray = true;
    if (m_authenticated) {
      m_busy = false;
      setState(State::XrayActive);
    }
    notifyUser("Xray started", "The Xray service is running.");
  } else if (!ok && action == "start") {
    if (m_authenticated) {
      m_busy = false;
      setState(State::XrayError);
    }
    notifyUser("Xray failed to start",
               detail.isEmpty()
                   ? "Check the Xray systemd service and permissions."
                   : detail);
  }
  publish();
}
void FirewallService::prepareForPowerDevilSuspend() {
  qInfo() << "PowerDevil signaled suspend preparation";
  prepareForSleep(true);
}

void FirewallService::beginResumeRefresh() {
  if (!m_resumeReauthPending || m_sleeping || m_busy ||
      !matches(m_snapshot, m_settings.networks))
    return;
  m_busy = true;
  setState(State::WaitingForPortal);
  if (m_settings.xrayEnabled)
    m_xray.query();
  else
    m_sophos.checkPortal();
}

void FirewallService::prepareForSleep(bool sleeping) {
  if (sleeping == m_sleeping)
    return;
  if (sleeping) {
    m_sleeping = true;
    m_wasOnConfiguredNetworkBeforeSleep =
        matches(m_snapshot, m_settings.networks);
    if (!m_accountUser.isEmpty())
      m_resumeLogoutUser = m_accountUser;
    m_retryTimer.stop();
    if (m_settings.logoutBeforeSleep && (m_authenticated || m_busy)) {
      acquireSleepInhibitor();
      m_sleepLogoutDeadline.start(4500);
      m_sophos.setTimeout(qMin(4, m_settings.timeoutSeconds));
      m_sophos.setRetries(0);
    }
    if (m_settings.logoutBeforeSleep && m_authenticated) {
      if (m_loggingOut)
        return;
      m_busy = true;
      m_loggingOut = true;
      setState(State::LoggingOut);
      if (m_xray.isActive())
        m_xray.stop();
      else
        m_sophos.logout(m_accountUser);
      return;
    }
    if (m_ownsXray)
      m_xray.stop();
    setState(State::Sleeping);
    if (!m_busy)
      finishSleepPreparation();
  } else {
    m_sleeping = false;
    m_manualLogout = false;
    m_sleepLogoutDeadline.stop();
    releaseSleepInhibitor();
    m_sophos.setTimeout(m_settings.timeoutSeconds);
    m_sophos.setRetries(m_settings.retryCount);
    acquireSleepInhibitor();
    m_resumeReauthPending =
        m_settings.autoLoginAfterWake && m_wasOnConfiguredNetworkBeforeSleep;
    m_wasOnConfiguredNetworkBeforeSleep = false;
    if (m_resumeReauthPending)
      beginResumeRefresh();
  }
}
void FirewallService::acquireSleepInhibitor() {
  if (!m_settings.logoutBeforeSleep || m_sleepDelayInhibitor.isValid())
    return;
  QDBusInterface login1("org.freedesktop.login1", "/org/freedesktop/login1",
                        "org.freedesktop.login1.Manager",
                        QDBusConnection::systemBus());
  if (!login1.isValid()) {
    qWarning() << "Could not access logind to delay sleep for Sophos logout"
               << login1.lastError().message();
    return;
  }
  const QDBusReply<QDBusUnixFileDescriptor> reply =
      login1.call("Inhibit", "sleep", "JIIT Firewall Manager",
                  "Log out from the JIIT Sophos portal before sleep", "delay");
  if (!reply.isValid() || !reply.value().isValid()) {
    qWarning() << "Could not acquire logind sleep delay inhibitor"
               << reply.error().message();
    return;
  }
  m_sleepDelayInhibitor = reply.value();
}
void FirewallService::releaseSleepInhibitor() {
  m_sleepDelayInhibitor = QDBusUnixFileDescriptor{};
}
void FirewallService::finishSleepPreparation() {
  m_sleepLogoutDeadline.stop();
  releaseSleepInhibitor();
}
} // namespace Jiit
