// cpp/tests/test_theme_contract.cpp
//
// Forbidden-globals lint for runtime themes (app-shell review P5/R2).
//
// Theme pages load in the main QML engine, so every root context
// property (app, gameModel, inputManager, ...) is technically visible
// to them — which silently freezes the entire context surface into
// public theme API the moment a theme binds one. The contract is:
// **ThemeContext is the only API themes may touch** (plus their own
// ids/properties). This test scans every .qml under themes/ and fails
// on bare references to the root-context globals, with file:line in
// the failure message.
//
// `themeContext.gameModel` is fine (property access on the allowed
// object); bare `gameModel` is not. Comments are stripped before
// matching so prose can mention "the app." freely.

#include <QtTest>
#include <QDirIterator>
#include <QRegularExpression>

class TestThemeContract : public QObject {
    Q_OBJECT

private slots:
    void noForbiddenGlobalsInThemes() {
        const QString themesDir = QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()
            + "/../../themes";
        QVERIFY2(QDir(themesDir).exists(), qPrintable("themes dir not found: " + themesDir));

        // Bare identifiers (not preceded by '.' or a word char) that
        // resolve to root-context / AppWindow surface. `app` and `Theme`
        // only count when used as an object (`app.` / `Theme.`) so prose
        // and unrelated words can't trip it.
        const QList<QRegularExpression> forbidden = {
            QRegularExpression(QStringLiteral(R"((?<![\w.])app\s*\.)")),
            QRegularExpression(QStringLiteral(R"((?<![\w.])Theme\s*\.)")),
            QRegularExpression(QStringLiteral(R"((?<![\w.])SettingsTheme\s*\.)")),
            QRegularExpression(QStringLiteral(R"((?<![\w.])gameModel\b)")),
            QRegularExpression(QStringLiteral(R"((?<![\w.])inputManager\b)")),
            QRegularExpression(QStringLiteral(R"((?<![\w.])themeManager\b)")),
            QRegularExpression(QStringLiteral(R"((?<![\w.])mainStack\b)")),
            QRegularExpression(QStringLiteral(R"((?<![\w.])settingsOverlay\b)")),
        };

        QStringList violations;
        int scanned = 0;

        QDirIterator it(themesDir, {QStringLiteral("*.qml")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            QFile f(path);
            QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
            ++scanned;

            const QStringList lines =
                QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
            bool inBlockComment = false;
            for (int i = 0; i < lines.size(); ++i) {
                QString line = lines[i];
                // Strip comments (line-based; good enough for QML).
                if (inBlockComment) {
                    const int end = line.indexOf(QLatin1String("*/"));
                    if (end < 0) continue;
                    line = line.mid(end + 2);
                    inBlockComment = false;
                }
                int blockStart;
                while ((blockStart = line.indexOf(QLatin1String("/*"))) >= 0) {
                    const int end = line.indexOf(QLatin1String("*/"), blockStart + 2);
                    if (end < 0) {
                        line = line.left(blockStart);
                        inBlockComment = true;
                        break;
                    }
                    line = line.left(blockStart) + line.mid(end + 2);
                }
                const int slashes = line.indexOf(QLatin1String("//"));
                if (slashes >= 0) line = line.left(slashes);

                for (const auto& re : forbidden) {
                    const auto m = re.match(line);
                    if (m.hasMatch()) {
                        violations << QStringLiteral("%1:%2: forbidden global \"%3\" — themes may only use themeContext")
                                          .arg(QDir(themesDir).relativeFilePath(path))
                                          .arg(i + 1)
                                          .arg(m.captured(0).trimmed());
                    }
                }
            }
        }

        QVERIFY2(scanned > 0, "no theme .qml files found — path wiring broken?");
        QVERIFY2(violations.isEmpty(),
                 qPrintable(QStringLiteral("theme boundary violations:\n  ")
                            + violations.join(QStringLiteral("\n  "))));
    }
};

QTEST_APPLESS_MAIN(TestThemeContract)
#include "test_theme_contract.moc"
