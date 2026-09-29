#include "jiit/network.h"
#include <QProcess>
#include <QRegularExpression>
#include <algorithm>

namespace Jiit {
QJsonObject NetworkSnapshot::toJson() const {
  return {{"connected", connected},
          {"ssid", ssid},
          {"bssid", bssid},
          {"interface", interfaceName},
          {"type", "wifi"}};
}
bool matches(const NetworkSnapshot &n,
             const QVector<NetworkProfile> &profiles) {
  for (const auto &p : profiles) {
    if (!p.enabled || !n.connected || n.ssid != p.ssid)
      continue;
    if (!p.interfaces.isEmpty() && !p.interfaces.contains(n.interfaceName))
      continue;
    if (!p.bssids.isEmpty() && !p.bssids.contains(n.bssid.toLower()))
      continue;
    return true;
  }
  return false;
}
NetworkSnapshot parseNmcliWifiOutput(const QString &output) {
  for (QString line : output.split('\n', Qt::SkipEmptyParts)) {
    QStringList fields;
    QString field;
    bool escape = false;
    for (QChar c : line) {
      if (escape) {
        field += c;
        escape = false;
      } else if (c == '\\') {
        escape = true;
      } else if (c == ':') {
        fields << field;
        field.clear();
      } else {
        field += c;
      }
    }
    fields << field;
    if (fields.size() >= 4 && fields[0] == "yes")
      return {true, fields[1], fields[2], fields[3]};
  }
  return {};
}
QVector<Account> orderedAccounts(const QVector<Account> &accounts) {
  QVector<Account> sorted;
  for (const auto &a : accounts)
    if (a.enabled)
      sorted.append(a);
  std::stable_sort(sorted.begin(), sorted.end(),
                   [](const Account &a, const Account &b) {
                     return a.priority < b.priority;
                   });
  return sorted;
}
NetworkMonitor::NetworkMonitor(QObject *parent) : QObject(parent) {
  m_timer.setInterval(5000);
  connect(&m_timer, &QTimer::timeout, this, &NetworkMonitor::refresh);
  m_timer.start();
  QTimer::singleShot(0, this, &NetworkMonitor::refresh);
}
void NetworkMonitor::setInterval(int s) {
  m_timer.setInterval(qMax(2000, s * 1000));
}
void NetworkMonitor::refresh() {
  if (m_process)
    return;
  m_process = new QProcess(this);
  connect(m_process, &QProcess::finished, this,
          [this](int exitCode, QProcess::ExitStatus status) {
            const QByteArray output = m_process->readAllStandardOutput();
            const QByteArray error = m_process->readAllStandardError();
            NetworkSnapshot n;
            if (exitCode == 0 && status == QProcess::NormalExit) {
              n = parseNmcliWifiOutput(QString::fromUtf8(output));
            } else {
              qWarning() << "NetworkManager query failed:" << error.trimmed();
            }
            m_process->deleteLater();
            m_process = nullptr;
            if (n.toJson() != m_snapshot.toJson()) {
              m_snapshot = n;
              emit changed(m_snapshot);
            }
          });
  connect(m_process, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError) {
            if (!m_process)
              return;
            auto *p = m_process;
            m_process = nullptr;
            p->deleteLater();
            NetworkSnapshot empty;
            if (empty.toJson() != m_snapshot.toJson()) {
              m_snapshot = empty;
              emit changed(m_snapshot);
            }
          });
  m_process->start("nmcli",
                   {"-t", "--escape", "yes", "-f", "ACTIVE,SSID,BSSID,DEVICE",
                    "device", "wifi", "list"});
}
} // namespace Jiit
