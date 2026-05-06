#include <QtTest>
#include <QCoreApplication>
#include <QFileInfo>
#include "core/libretro/core_loader.h"

class TestCoreLoader : public QObject {
    Q_OBJECT
private:
    QString fakeCorePath() const {
        // Test runs from build dir; fake core is alongside
        return QCoreApplication::applicationDirPath() + "/fake_libretro_core.dylib";
    }
private slots:
    void testInitialState() {
        CoreLoader l;
        QVERIFY(!l.isOpen());
    }
    void testOpenSucceedsForValidCore() {
        QVERIFY2(QFileInfo::exists(fakeCorePath()),
                 qPrintable("fake core not at " + fakeCorePath()));
        CoreLoader l;
        QString err;
        QVERIFY2(l.open(fakeCorePath(), &err), qPrintable(err));
        QVERIFY(l.isOpen());
        QVERIFY(l.symbols().retro_api_version != nullptr);
        QCOMPARE(l.symbols().retro_api_version(), 1u);
    }
    void testOpenFailsForMissingFile() {
        CoreLoader l;
        QString err;
        QVERIFY(!l.open("/nonexistent/path.dylib", &err));
        QVERIFY(!err.isEmpty());
        QVERIFY(!l.isOpen());
    }
    void testCloseReleasesHandle() {
        CoreLoader l;
        l.open(fakeCorePath(), nullptr);
        l.close();
        QVERIFY(!l.isOpen());
    }
};
QTEST_MAIN(TestCoreLoader)
#include "test_core_loader.moc"
