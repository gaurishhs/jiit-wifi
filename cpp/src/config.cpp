#include "jiit/config.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

namespace Jiit {
QString stateName(State s) {
  switch (s) {
  case State::Disconnected:
    return "DISCONNECTED";
  case State::WifiConnected:
    return "WIFI_CONNECTED";
  case State::JiitNetworkDetected:
    return "JIIT_NETWORK_DETECTED";
  case State::WaitingForPortal:
    return "WAITING_FOR_PORTAL";
  case State::Authenticating:
    return "AUTHENTICATING";
  case State::Authenticated:
    return "AUTHENTICATED";
  case State::XrayStarting:
    return "XRAY_STARTING";
  case State::XrayActive:
    return "XRAY_ACTIVE";
  case State::AuthenticationFailed:
    return "AUTHENTICATION_FAILED";
  case State::PortalUnavailable:
    return "PORTAL_UNAVAILABLE";
  case State::NetworkError:
    return "NETWORK_ERROR";
  case State::XrayError:
    return "XRAY_ERROR";
  case State::LoggingOut:
    return "LOGGING_OUT";
  case State::Sleeping:
    return "SLEEPING";
  }
  return "UNKNOWN";
}
QString resultName(Result r) {
  switch (r) {
  case Result::Success:
    return "SUCCESS";
  case Result::Failed:
    return "FAILED";
  case Result::AlreadyLoggedIn:
    return "ALREADY_LOGGED_IN";
  case Result::NotConnected:
    return "NOT_CONNECTED";
  case Result::PortalUnavailable:
    return "PORTAL_UNAVAILABLE";
  case Result::NetworkError:
    return "NETWORK_ERROR";
  case Result::Unknown:
    return "UNKNOWN";
  }
  return "UNKNOWN";
}
QJsonObject Settings::toJson() const {
  QJsonArray a, n;
  for (const auto &x : accounts)
    a.append(QJsonObject{{"id", x.id},
                         {"name", x.name},
                         {"username", x.username},
                         {"priority", x.priority},
                         {"enabled", x.enabled}});
  for (const auto &x : networks) {
    QJsonArray b, i;
    for (const auto &v : x.bssids)
      b.append(v);
    for (const auto &v : x.interfaces)
      i.append(v);
    n.append(QJsonObject{{"name", x.name},
                         {"ssid", x.ssid},
                         {"bssids", b},
                         {"interfaces", i},
                         {"enabled", x.enabled}});
  }
  return {{"accounts", a},
          {"networks", n},
          {"gateway", gateway},
          {"timeout", timeoutSeconds},
          {"retry_count", retryCount},
          {"retry_interval", retryIntervalSeconds},
          {"detection_interval", pollIntervalSeconds},
          {"debug", debug},
          {"xray_enabled", xrayEnabled},
          {"xray_mode", xrayMode},
          {"xray_service", xrayService},
          {"start_xray_after_login", startXrayAfterLogin},
          {"stop_xray_on_leave", stopXrayOnLeave},
          {"logout_before_sleep", logoutBeforeSleep},
          {"auto_login_after_wake", autoLoginAfterWake}};
}
Settings Settings::fromJson(const QJsonObject &j) {
  Settings s;
  s.gateway = j.value("gateway").toString(s.gateway);
  s.timeoutSeconds = j.value("timeout").toInt(s.timeoutSeconds);
  s.retryCount = j.value("retry_count").toInt(s.retryCount);
  s.retryIntervalSeconds =
      j.value("retry_interval").toInt(s.retryIntervalSeconds);
  s.pollIntervalSeconds =
      j.value("detection_interval").toInt(s.pollIntervalSeconds);
  s.debug = j.value("debug").toBool(s.debug);
  s.xrayEnabled = j.value("xray_enabled").toBool(s.xrayEnabled);
  s.xrayMode = j.value("xray_mode").toString(s.xrayMode);
  s.xrayService = j.value("xray_service").toString(s.xrayService);
  s.startXrayAfterLogin =
      j.value("start_xray_after_login").toBool(s.startXrayAfterLogin);
  s.stopXrayOnLeave = j.value("stop_xray_on_leave").toBool(s.stopXrayOnLeave);
  s.logoutBeforeSleep =
      j.value("logout_before_sleep").toBool(s.logoutBeforeSleep);
  s.autoLoginAfterWake =
      j.value("auto_login_after_wake").toBool(s.autoLoginAfterWake);
  for (const auto v : j.value("accounts").toArray()) {
    auto o = v.toObject();
    s.accounts.append({o.value("id").toString(), o.value("name").toString(),
                       o.value("username").toString(),
                       o.value("priority").toInt(1),
                       o.value("enabled").toBool(true)});
  }
  for (const auto v : j.value("networks").toArray()) {
    auto o = v.toObject();
    NetworkProfile p;
    p.name = o.value("name").toString();
    p.ssid = o.value("ssid").toString();
    p.enabled = o.value("enabled").toBool(true);
    for (auto b : o.value("bssids").toArray())
      p.bssids.append(b.toString().toLower());
    for (auto i : o.value("interfaces").toArray())
      p.interfaces.append(i.toString());
    s.networks.append(p);
  }
  return s;
}
QString configPath() {
  return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
         "/jiit-firewall/config.json";
}
Settings loadSettings() {
  QFile f(configPath());
  if (!f.open(QIODevice::ReadOnly))
    return {};
  QJsonParseError e;
  auto d = QJsonDocument::fromJson(f.readAll(), &e);
  return e.error == QJsonParseError::NoError ? Settings::fromJson(d.object())
                                             : Settings{};
}
bool saveSettings(const Settings &s, QString *error) {
  QString path = configPath();
  QDir().mkpath(QFileInfo(path).absolutePath());
  QSaveFile f(path);
  if (!f.open(QIODevice::WriteOnly)) {
    if (error)
      *error = f.errorString();
    return false;
  }
  f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  if (f.write(QJsonDocument(s.toJson()).toJson(QJsonDocument::Indented)) < 0 ||
      !f.commit()) {
    if (error)
      *error = f.errorString();
    return false;
  }
  return true;
}
} // namespace Jiit
