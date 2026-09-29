#pragma once
#include "types.h"
#include <QObject>
#include <QProcess>
#include <QTimer>
namespace Jiit {
class NetworkMonitor final : public QObject {
  Q_OBJECT
public:
  explicit NetworkMonitor(QObject *parent = nullptr);
  NetworkSnapshot snapshot() const { return m_snapshot; }
  void setInterval(int seconds);
signals:
  void changed(const Jiit::NetworkSnapshot &snapshot);
private slots:
  void refresh();

private:
  NetworkSnapshot m_snapshot;
  QTimer m_timer;
  QProcess *m_process{nullptr};
};
} // namespace Jiit
Q_DECLARE_METATYPE(Jiit::NetworkSnapshot)
