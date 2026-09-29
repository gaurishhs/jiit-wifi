#include "jiit/xray.h"
#include <QRegularExpression>
namespace Jiit {

XrayManager::XrayManager(QObject *p) : QObject(p) {}

void XrayManager::configure(const QString &mode, const QString &service) {
  static const QRegularExpression valid("^[A-Za-z0-9_.@:-]+$");
  if ((mode != "system" && mode != "user") ||
      !valid.match(service).hasMatch()) {
    m_available = false;
    m_status = "Unavailable";
    emit stateChanged(m_status);
    return;
  }
  m_mode = mode;
  m_service = service;
  m_available = true;
  run("is-active");
}
QString XrayManager::status() const { return m_status; }
void XrayManager::start() { run("start"); }
void XrayManager::stop() { run("stop"); }
void XrayManager::query() { run("is-active"); }
void XrayManager::run(const QString &action) {
  if (!m_available) {
    emit operationFinished(action, false, "Invalid service settings");
    return;
  }
  auto *p = new QProcess(this);
  QStringList args;
  if (m_mode == "user")
    args << "--user";
  args << action;
  if (action == "is-active")
    args << "--quiet";
  args << m_service;
  connect(p, &QProcess::finished, this,
          [this, p, action](int code, QProcess::ExitStatus) {
            QString detail =
                QString::fromUtf8(p->readAllStandardError()).trimmed();
            if (action == "is-active") {
              m_active = code == 0;
              m_status = m_active ? "Running" : "Stopped";
              emit stateChanged(m_status);
              emit operationFinished(action, code == 0, detail);
            } else {
              m_active = code == 0 ? (action == "start") : m_active;
              m_status = m_active ? "Running" : "Stopped";
              emit operationFinished(action, code == 0, detail);
              emit stateChanged(m_status);
            }
            p->deleteLater();
          });
  connect(p, &QProcess::errorOccurred, this,
          [this, p, action](QProcess::ProcessError) {
            m_available = false;
            m_status = "Unavailable";
            emit stateChanged(m_status);
            emit operationFinished(action, false, p->errorString());
            p->deleteLater();
          });
  p->start("systemctl", args);
}
} // namespace Jiit
