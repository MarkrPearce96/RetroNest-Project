// Packet 7 Stage 2: CoreProber — offline declared-options seeding.
// Probes the built fake core (which emits SET_CORE_OPTIONS_V2 during
// retro_set_environment, like every real core in the suite — spike-verified).
#include <QtTest>
#include <QCoreApplication>
#include "core/libretro/core_prober.h"

class TestCoreProber : public QObject {
    Q_OBJECT
private:
    QString fakeCorePath() const {
        return QCoreApplication::applicationDirPath() + "/fake_libretro_core.dylib";
    }
private slots:
    void probeFakeCore() {
        const auto doc = CoreProber::probe(fakeCorePath());
        QVERIFY(doc.has_value());
        QCOMPARE(doc->options.size(), 2);
        QCOMPARE(doc->options[0].key, QString("fake_speed"));
        QCOMPARE(doc->options[0].label, QString("Emulation Speed"));
        QCOMPARE(doc->options[0].values.size(), 2);
        QCOMPARE(doc->options[0].values[0].label, QString("Normal"));
        QCOMPARE(doc->options[1].key, QString("fake_bool"));
        QCOMPARE(doc->coreLibraryVersion, QString("1.0"));
    }
    void probeAgainReusesHandle() {
        // Second probe of the same dylib must work (retained-handle path).
        const auto doc = CoreProber::probe(fakeCorePath());
        QVERIFY(doc.has_value());
        QCOMPARE(doc->options.size(), 2);
    }
    void probeMissingFile() {
        const auto doc = CoreProber::probe("/nonexistent/core.dylib");
        QVERIFY(!doc.has_value());
    }
};

QTEST_MAIN(TestCoreProber)
#include "test_core_prober.moc"
