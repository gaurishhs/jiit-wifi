#pragma once
#include <QString>
namespace Jiit {
class WalletStore {
public:
  bool write(const QString &accountId, const QString &password,
             QString *error = nullptr) const;
  QString read(const QString &accountId, QString *error = nullptr) const;
  bool remove(const QString &accountId, QString *error = nullptr) const;

private:
  int openWallet(QString *error) const;
};
} // namespace Jiit
