#include "jiit/sophos.h"
#include "jiit/types.h"
#include <QtTest>

class CoreTests final : public QObject {
  Q_OBJECT
private slots:
  void epochUsesHundredths() {
    QCOMPARE(
        Jiit::epochTimestamp().size(),
        QByteArray::number(QDateTime::currentMSecsSinceEpoch() / 10).size());
  }
  void loginPayloadKeepsProtocolFields() {
    const auto p = Jiit::SophosClient::loginPayload("user", "secret", "123");
    QVERIFY(p.contains("mode=191"));
    QVERIFY(p.contains("username=user"));
    QVERIFY(p.contains("password=secret"));
    QVERIFY(p.contains("a=123"));
    QVERIFY(p.contains("producttype=0"));
  }
  void logoutPayloadKeepsProtocolFields() {
    const auto p = Jiit::SophosClient::logoutPayload("user", "42");
    QVERIFY(p.contains("mode=193"));
    QVERIFY(p.contains("username=user"));
    QVERIFY(p.contains("a=42"));
    QVERIFY(p.contains("producttype=0"));
  }
  void parsesXmlSafely() {
    auto r = Jiit::SophosClient::parseResponse(
        "<response><message>Login Successful</message></response>");
    QCOMPARE(r.code, Jiit::Result::Success);
    QCOMPARE(r.message, QString("Login Successful"));
    QCOMPARE(Jiit::SophosClient::parseResponse("<broken>").code,
             Jiit::Result::Unknown);
  }
  void recognizesAlreadyLoggedIn() {
    QCOMPARE(Jiit::classifyMessage("You are already logged in"),
             Jiit::Result::AlreadyLoggedIn);
  }
  void ssidMatchesAcrossBssids() {
    Jiit::NetworkProfile p;
    p.ssid = "JIIT_WIFI";
    p.enabled = true;
    for (const QString b : {"AP1711", "AP1712", "aa:bb:cc"}) {
      Jiit::NetworkSnapshot n{true, "JIIT_WIFI", b, "wlan0"};
      QVERIFY(Jiit::matches(n, {p}));
    }
  }
  void bssidRestrictionIsOptionalAndSecondary() {
    Jiit::NetworkProfile p;
    p.ssid = "JIIT";
    Jiit::NetworkSnapshot n{true, "JIIT", "ap-different", "wlan0"};
    QVERIFY(Jiit::matches(n, {p}));
    p.bssids = {"ap-one"};
    QVERIFY(!Jiit::matches(n, {p}));
  }
  void multipleSsidsWork() {
    Jiit::NetworkProfile a;
    a.ssid = "Hostel";
    Jiit::NetworkProfile b;
    b.ssid = "Campus";
    QVERIFY(Jiit::matches({true, "Campus", "ap", "wlan0"}, {a, b}));
  }
  void parsesNmcliActiveWifiAndEscapedBssid() {
    const auto n = Jiit::parseNmcliWifiOutput(
        "no:Guest:11\\:22\\:33\\:44\\:55\\:66:wlan0\n"
        "yes:JIIT_WIFI:aa\\:bb\\:cc\\:dd\\:ee\\:ff:wlp2s0\n");
    QVERIFY(n.connected);
    QCOMPARE(n.ssid, QString("JIIT_WIFI"));
    QCOMPARE(n.bssid, QString("aa:bb:cc:dd:ee:ff"));
    QCOMPARE(n.interfaceName, QString("wlp2s0"));
  }
  void enabledAccountsAreSortedByPriority() {
    QVector<Jiit::Account> a{{"b", "Backup", "b", 2, true},
                             {"off", "Disabled", "x", 0, false},
                             {"a", "Primary", "a", 1, true}};
    const auto sorted = Jiit::orderedAccounts(a);
    QCOMPARE(sorted.size(), 2);
    QCOMPARE(sorted[0].id, QString("a"));
    QCOMPARE(sorted[1].id, QString("b"));
  }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "test_core.moc"
