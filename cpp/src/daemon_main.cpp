#include "jiit/dbus_adaptor.h"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMutex>
#include <QStandardPaths>
#include <QTextStream>

static void logToState(QtMsgType type, const QMessageLogContext &,
                       const QString &message) {
  static QMutex mutex;
  QMutexLocker lock(&mutex);
  const QString path =
      QStandardPaths::writableLocation(QStandardPaths::StateLocation) +
      "/daemon.log";
  if (QFileInfo(path).size() > 2 * 1024 * 1024) {
    QFile::remove(path + ".1");
    QFile::rename(path, path + ".1");
  }
  QFile f(path);
  if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QTextStream s(&f);
    s << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << ' ' << type
      << ' ' << message << '\n';
  }
  QTextStream(stderr) << message << Qt::endl;
}

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("jiit-firewall");
  QDir().mkpath(
      QStandardPaths::writableLocation(QStandardPaths::StateLocation));
  qInstallMessageHandler(logToState);
  QDBusConnection bus = QDBusConnection::sessionBus();
  if (!bus.registerService("org.jiit.FirewallManager")) {
    qCritical() << "Could not own org.jiit.FirewallManager:"
                << bus.lastError().message();
    return 2;
  }
  Jiit::FirewallService service;
  Jiit::FirewallAdaptor adaptor(&service);
  Q_UNUSED(adaptor);
  if (!bus.registerObject("/org/jiit/FirewallManager", &service,
                          QDBusConnection::ExportAdaptors)) {
    qCritical() << "Could not export firewall D-Bus interface:"
                << bus.lastError().message();
    return 3;
  }
  QDBusConnection system = QDBusConnection::systemBus();
  system.connect("org.freedesktop.login1", "/org/freedesktop/login1",
                 "org.freedesktop.login1.Manager", "PrepareForSleep", &service,
                 SLOT(prepareForSleep(bool)));
  return app.exec();
}
