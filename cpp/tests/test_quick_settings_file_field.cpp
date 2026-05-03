#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "adapters/emulator_adapter.h"
#include "adapters/adapter_registry.h"
#include "services/config_service.h"
#include "core/manifest.h"

namespace {
class FileFieldTestAdapter : public EmulatorAdapter {
public:
    QString mainFile;
    QString altFile;

    bool ensureConfig(const EmulatorManifest&, const QString&, const QString&) override { return true; }
    QString resolveExecutable(const EmulatorManifest&, const QString&) override { return {}; }
    QString configFilePath() const override { return mainFile; }

    ResolutionOptions resolutionOptions() const override {
        return {"Settings", "InternalResolution",
                {{"1x", "1"}, {"2x", "2"}}, "1", altFile};
    }

    AspectRatioOptions aspectRatioOptions() const override {
        return {{
            {"Auto", {{"Settings", "AspectRatio", "0", altFile}}},
            {"16:9", {{"Settings", "AspectRatio", "1", altFile}}},
        }, "Auto"};
    }
};
}

class TestQuickSettingsFileField : public QObject {
    Q_OBJECT

private:
    QTemporaryDir tmp_;
    FileFieldTestAdapter* adapter_ = nullptr;
    ConfigService* svc_ = nullptr;

private slots:
    void initTestCase() {
        QVERIFY(tmp_.isValid());

        auto adapter = std::make_unique<FileFieldTestAdapter>();
        adapter->mainFile = tmp_.path() + "/main.ini";
        adapter->altFile  = tmp_.path() + "/alt.ini";

        // Seed both files with section headers so IniFile loads them.
        for (const QString& path : {adapter->mainFile, adapter->altFile}) {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("[Settings]\n");
            f.close();
        }

        adapter_ = adapter.get();
        AdapterRegistry::instance().registerAdapter("filefieldtest", std::move(adapter));
        // Quick-settings paths only consult AdapterRegistry; nullptr loader is safe.
        svc_ = new ConfigService(/*loader=*/nullptr, /*parent=*/this);
    }

    void resolutionWritesToAltFile() {
        svc_->applyQuickResolution({{"filefieldtest", "2"}});

        // Main file should NOT contain the resolution key.
        QFile main(adapter_->mainFile);
        QVERIFY(main.open(QIODevice::ReadOnly));
        QString mainContent = QString::fromUtf8(main.readAll());
        QVERIFY2(!mainContent.contains("InternalResolution"),
                 "Resolution should not have been written to configFilePath()");

        // Alt file SHOULD contain it.
        QFile alt(adapter_->altFile);
        QVERIFY(alt.open(QIODevice::ReadOnly));
        QString altContent = QString::fromUtf8(alt.readAll());
        QVERIFY2(altContent.contains("InternalResolution = 2"),
                 "Resolution should have been written to ResolutionOptions::iniFilePath");
    }

    void resolutionReadsFromAltFile() {
        QString val = svc_->currentResolution("filefieldtest");
        QCOMPARE(val, QString("2"));
    }

    void aspectRatioWritesToAltFile() {
        svc_->applyQuickAspectRatio({{"filefieldtest", "16:9"}});

        QFile alt(adapter_->altFile);
        QVERIFY(alt.open(QIODevice::ReadOnly));
        QString altContent = QString::fromUtf8(alt.readAll());
        QVERIFY2(altContent.contains("AspectRatio = 1"),
                 "Aspect should have been written to IniPatch::iniFilePath");
    }

    void aspectRatioReadsFromAltFile() {
        QString label = svc_->currentAspectRatio("filefieldtest");
        QCOMPARE(label, QString("16:9"));
    }
};

QTEST_MAIN(TestQuickSettingsFileField)
#include "test_quick_settings_file_field.moc"
