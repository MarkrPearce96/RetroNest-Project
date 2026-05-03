#include <QtTest>
#include "adapters/emulator_adapter.h"

// Expose the protected static helpers for testing.
class TestableAdapter : public EmulatorAdapter {
public:
    using EmulatorAdapter::IniKeyPatch;
    using EmulatorAdapter::patchIniKeys;
};
using KeyPatch = TestableAdapter::IniKeyPatch;

class TestPatchIniKeys : public QObject {
    Q_OBJECT
private slots:
    void updatesExistingKey() {
        QString content = "[Audio]\nVolume = 50\n";
        QVector<KeyPatch> patches = {{"Audio", "Volume", "75"}};
        bool changed = TestableAdapter::patchIniKeys(content, patches);
        QVERIFY(changed);
        QVERIFY(content.contains("Volume = 75"));
        QVERIFY(!content.contains("Volume = 50"));
    }

    void noChangeWhenValueAlreadyMatches() {
        QString content = "[Audio]\nVolume = 50\n";
        QVector<KeyPatch> patches = {{"Audio", "Volume", "50"}};
        bool changed = TestableAdapter::patchIniKeys(content, patches);
        QVERIFY2(!changed, "patchIniKeys should report no change when value already matches");
        QVERIFY(content.contains("Volume = 50"));
    }

    void appendsKeyToExistingSection() {
        QString content = "[Audio]\nVolume = 50\n";
        QVector<KeyPatch> patches = {{"Audio", "Driver", "wasapi"}};
        bool changed = TestableAdapter::patchIniKeys(content, patches);
        QVERIFY(changed);
        QVERIFY(content.contains("Driver = wasapi"));
        QVERIFY(content.contains("Volume = 50"));  // existing key preserved
    }

    void appendsSectionAndKeyWhenSectionMissing() {
        QString content = "[Audio]\nVolume = 50\n";
        QVector<KeyPatch> patches = {{"Graphics", "Renderer", "Vulkan"}};
        bool changed = TestableAdapter::patchIniKeys(content, patches);
        QVERIFY(changed);
        QVERIFY(content.contains("[Graphics]"));
        QVERIFY(content.contains("Renderer = Vulkan"));
        QVERIFY(content.contains("[Audio]"));         // existing section preserved
        QVERIFY(content.contains("Volume = 50"));
    }

    void multiplePatchesInOneCall() {
        QString content = "[Audio]\nVolume = 50\n";
        QVector<KeyPatch> patches = {
            {"Audio", "Volume", "75"},
            {"Audio", "Driver", "wasapi"},
            {"Graphics", "Renderer", "Vulkan"},
        };
        bool changed = TestableAdapter::patchIniKeys(content, patches);
        QVERIFY(changed);
        QVERIFY(content.contains("Volume = 75"));
        QVERIFY(content.contains("Driver = wasapi"));
        QVERIFY(content.contains("Renderer = Vulkan"));
    }

    void idempotentOnSecondCall() {
        QString content = "[Audio]\nVolume = 50\n";
        QVector<KeyPatch> patches = {
            {"Audio", "Driver", "wasapi"},
            {"Graphics", "Renderer", "Vulkan"},
        };
        // First call writes everything
        bool firstChanged = TestableAdapter::patchIniKeys(content, patches);
        QVERIFY(firstChanged);
        // Second call should detect no change is needed
        bool secondChanged = TestableAdapter::patchIniKeys(content, patches);
        QVERIFY2(!secondChanged, "patchIniKeys must be idempotent — second identical call should report no change");
    }

    void preservesUnrelatedKeys() {
        QString content =
            "[Audio]\n"
            "Volume = 50\n"
            "Backend = SDL\n"
            "[Graphics]\n"
            "Renderer = OpenGL\n";
        QVector<KeyPatch> patches = {{"Audio", "Volume", "75"}};
        TestableAdapter::patchIniKeys(content, patches);
        QVERIFY(content.contains("Backend = SDL"));     // sibling key in same section
        QVERIFY(content.contains("Renderer = OpenGL")); // key in other section
    }
};

QTEST_GUILESS_MAIN(TestPatchIniKeys)
#include "test_patch_ini_keys.moc"
