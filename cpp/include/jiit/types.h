#pragma once
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace Jiit {
enum class Result {
  Success,
  Failed,
  AlreadyLoggedIn,
  NotConnected,
  PortalUnavailable,
  NetworkError,
  Unknown
};
enum class State {
  Disconnected,
  WifiConnected,
  JiitNetworkDetected,
  WaitingForPortal,
  Authenticating,
  Authenticated,
  XrayStarting,
  XrayActive,
  AuthenticationFailed,
  PortalUnavailable,
  NetworkError,
  XrayError,
  LoggingOut,
  Sleeping
};
QString stateName(State state);
QString resultName(Result result);

struct SophosResult {
  Result code{Result::Unknown};
  QString message;
  QString raw;
};
struct Account {
  QString id, name, username;
  int priority{1};
  bool enabled{true};
};
struct NetworkProfile {
  QString name, ssid;
  QStringList bssids, interfaces;
  bool enabled{true};
};
struct Settings {
  QVector<Account> accounts;
  QVector<NetworkProfile> networks;
  QString gateway{"http://172.16.68.6:8090/"};
  int timeoutSeconds{8}, retryCount{4}, retryIntervalSeconds{15},
      pollIntervalSeconds{5};
  bool debug{false}, xrayEnabled{true}, startXrayAfterLogin{true},
      stopXrayOnLeave{true};
  bool logoutBeforeSleep{true}, autoLoginAfterWake{true};
  QString xrayMode{"system"}, xrayService{"xray.service"};
  QJsonObject toJson() const;
  static Settings fromJson(const QJsonObject &json);
};
struct NetworkSnapshot {
  bool connected{false};
  QString ssid, bssid, interfaceName;
  QJsonObject toJson() const;
};
bool matches(const NetworkSnapshot &network,
             const QVector<NetworkProfile> &profiles);
NetworkSnapshot parseNmcliWifiOutput(const QString &output);
QVector<Account> orderedAccounts(const QVector<Account> &accounts);
Result classifyMessage(const QString &message);
QString epochTimestamp();
} // namespace Jiit
