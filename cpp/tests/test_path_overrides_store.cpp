#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "core/path_overrides_store.h"

class TestPathOverridesStore : public QObject {
    Q_OBJECT
private slots:
    void testReadMissingReturnsEmpty() {
        QTemporaryDir dir;
        PathOverridesStore store(dir.filePath("overrides.json"));
        QCOMPARE(store.read("pcsx2", "MemoryCards"), QString());
    }
    void testWriteThenReadRoundTrips() {
        QTemporaryDir dir;
        PathOverridesStore store(dir.filePath("overrides.json"));
        store.write("pcsx2", "MemoryCards", "/Volumes/Ext/memcards");
        QCOMPARE(store.read("pcsx2", "MemoryCards"), QString("/Volumes/Ext/memcards"));
    }
    void testWritePersistsAcrossInstances() {
        QTemporaryDir dir;
        const QString path = dir.filePath("overrides.json");
        { PathOverridesStore a(path); a.write("mgba", "Saves", "/x/y"); }
        PathOverridesStore b(path);
        QCOMPARE(b.read("mgba", "Saves"), QString("/x/y"));
    }
    void testClearRemovesKey() {
        QTemporaryDir dir;
        PathOverridesStore store(dir.filePath("overrides.json"));
        store.write("pcsx2", "Textures", "/t");
        store.clear("pcsx2", "Textures");
        QCOMPARE(store.read("pcsx2", "Textures"), QString());
    }
    void testEmptyStringTreatedAsUnset() {
        QTemporaryDir dir;
        const QString p = dir.filePath("overrides.json");
        // First write a real value, then overwrite with empty string —
        // empty must clear the key from disk, not store a "" placeholder.
        { PathOverridesStore a(p); a.write("pcsx2", "MemoryCards", "/val"); }
        { PathOverridesStore b(p); b.write("pcsx2", "MemoryCards", ""); }
        PathOverridesStore c(p);
        QCOMPARE(c.read("pcsx2", "MemoryCards"), QString());
    }
    void testMultipleEmulatorsCoexist() {
        QTemporaryDir dir;
        PathOverridesStore store(dir.filePath("overrides.json"));
        store.write("pcsx2", "MemoryCards", "/p");
        store.write("mgba",  "Saves",       "/m");
        QCOMPARE(store.read("pcsx2", "MemoryCards"), QString("/p"));
        QCOMPARE(store.read("mgba",  "Saves"),       QString("/m"));
    }
    void testCorruptFileTreatedAsEmpty() {
        QTemporaryDir dir;
        const QString p = dir.filePath("overrides.json");
        QFile f(p); f.open(QIODevice::WriteOnly); f.write("not json"); f.close();
        PathOverridesStore store(p);
        QCOMPARE(store.read("pcsx2", "MemoryCards"), QString());
    }
};
QTEST_MAIN(TestPathOverridesStore)
#include "test_path_overrides_store.moc"
