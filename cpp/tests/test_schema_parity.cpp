// Packet 7 Stage 2 TRANSITIONAL parity net — deleted when the last adapter
// converts (Task 11). Guards each adapter's conversion from hand-mirrored
// schema rows to the declared-options × overlay merge.
//
// Two modes:
//   SCHEMA_SNAPSHOT_WRITE=1  → writes, per core being converted:
//       fixtures/schema_snapshots/<core>.json   (current settingsSchema())
//       fixtures/declared/<core>_declared_options.json (CoreProber against
//       the locally installed core — dev-machine path; write mode is a
//       dev-machine-only step, compare mode is hermetic)
//   default                  → loads both fixtures, injects the declared doc,
//       and compares the CURRENT schema against the snapshot:
//         - every snapshot row must exist matched by (key, category, storage)
//         - defaults must match
//         - value sets may GAIN values (reported) but must not LOSE any
//       Labels/tooltips are NOT compared (wording adopted from the core —
//       Packet 7 decision 4).
#include <QtTest>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "adapters/libretro/duckstation_libretro_adapter.h"
#include "adapters/libretro/mgba_libretro_adapter.h"
#include "core/libretro/core_prober.h"

namespace {

QString testsDir() { return QFileInfo(QString::fromUtf8(__FILE__)).absolutePath(); }
QString snapshotPath(const QString& core) {
    return testsDir() + "/fixtures/schema_snapshots/" + core + ".json";
}
QString declaredFixturePath(const QString& core) {
    return testsDir() + "/fixtures/declared/" + core + "_declared_options.json";
}
QString installedCorePath(const QString& core) {
    return QDir::homePath() + "/Documents/RetroNest/emulators/libretro/cores/" + core + "_libretro.dylib";
}

QJsonArray schemaToJson(const QVector<SettingDef>& schema)
{
    QJsonArray rows;
    for (const auto& def : schema) {
        QJsonArray vals;
        for (const auto& pair : def.options)
            vals.append(pair.second);   // stored values only; display labels not compared
        rows.append(QJsonObject{
            { "key", def.key },
            { "category", def.category },
            { "storage", int(def.storage) },
            { "type", int(def.type) },
            { "default", def.defaultValue },
            { "values", vals },
        });
    }
    return rows;
}

bool writeJson(const QString& path, const QJsonArray& rows)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(rows).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace

class TestSchemaParity : public QObject {
    Q_OBJECT
private:
    void runParity(LibretroAdapter& adapter, const QString& core) {
        if (qEnvironmentVariableIsSet("SCHEMA_SNAPSHOT_WRITE")) {
            if (QFile::exists(snapshotPath(core))) {
                // Never overwrite a committed snapshot — it records the
                // PRE-conversion hand schema, which no longer exists once
                // the adapter converts. Delete the file deliberately if a
                // regeneration is truly intended.
                qInfo() << core << "snapshot already exists — skipped";
                return;
            }
            QVERIFY2(writeJson(snapshotPath(core), schemaToJson(adapter.settingsSchema())),
                     "failed to write schema snapshot");
            const auto probed = CoreProber::probe(installedCorePath(core));
            QVERIFY2(probed.has_value(), "probe of installed core failed");
            QDir().mkpath(QFileInfo(declaredFixturePath(core)).path());
            QVERIFY(probed->save(declaredFixturePath(core)));
            qInfo() << "snapshot written for" << core << "— re-run without SCHEMA_SNAPSHOT_WRITE to compare";
            return;
        }

        // Hermetic compare: inject the recorded declared table.
        const auto doc = DeclaredOptionsDoc::load(declaredFixturePath(core));
        QVERIFY2(doc.has_value(), "declared fixture missing — run once with SCHEMA_SNAPSHOT_WRITE=1");
        adapter.setDeclaredDocForTest(*doc);

        QFile f(snapshotPath(core));
        QVERIFY2(f.open(QIODevice::ReadOnly), "schema snapshot missing — run once with SCHEMA_SNAPSHOT_WRITE=1");
        const QJsonArray expected = QJsonDocument::fromJson(f.readAll()).array();
        QVERIFY(!expected.isEmpty());

        const QVector<SettingDef> current = adapter.settingsSchema();
        int gains = 0;
        for (const auto& rowVal : expected) {
            const QJsonObject row = rowVal.toObject();
            const QString key = row["key"].toString();
            const QString category = row["category"].toString();
            const int storage = row["storage"].toInt();

            const SettingDef* match = nullptr;
            for (const auto& def : current) {
                if (def.key == key && def.category == category && int(def.storage) == storage) {
                    match = &def;
                    break;
                }
            }
            QVERIFY2(match, qPrintable(QString("row lost in conversion: %1 [%2]").arg(key, category)));
            QVERIFY2(match->defaultValue == row["default"].toString(),
                     qPrintable(QString("default changed for %1 [%2]: '%3' -> '%4'")
                                    .arg(key, category, row["default"].toString(), match->defaultValue)));

            QStringList currentValues;
            for (const auto& pair : match->options)
                currentValues << pair.second;
            for (const auto& v : row["values"].toArray()) {
                QVERIFY2(currentValues.contains(v.toString()),
                         qPrintable(QString("value LOST for %1 [%2]: '%3'")
                                        .arg(key, category, v.toString())));
            }
            const int gained = currentValues.size() - row["values"].toArray().size();
            if (gained > 0) {
                gains += gained;
                qInfo().noquote() << QString("value-set GAIN for %1 [%2]: +%3 values (core declares more than the hand mirror did)")
                                        .arg(key, category).arg(gained);
            }
        }
        qInfo() << core << "parity:" << expected.size() << "rows preserved," << gains << "values gained";
    }

private slots:
    void mgbaParity() {
        MgbaLibretroAdapter adapter;
        runParity(adapter, "mgba");
    }
    void duckstationParity() {
        DuckStationLibretroAdapter adapter;
        runParity(adapter, "duckstation");
    }
};

QTEST_APPLESS_MAIN(TestSchemaParity)
#include "test_schema_parity.moc"
