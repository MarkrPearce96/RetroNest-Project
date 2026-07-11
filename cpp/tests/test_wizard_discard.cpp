#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QUuid>
#include "ui/wizard_state.h"

// Safety-critical test: WizardState::discardIncompleteSetup() runs when the
// user closes the setup wizard without finishing it. It must NEVER call
// removeRecursively() on a folder that pre-existed before the wizard ran —
// only on folders the wizard itself created fresh this run.
class TestWizardDiscard : public QObject {
    Q_OBJECT
private slots:
    void freshRootIsRemoved() {
        const QString root = QDir::tempPath() + "/rn-wizard-discard-fresh-"
            + QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVERIFY(!QDir(root).exists());   // must not pre-exist

        WizardState wizard;
        wizard.setRootPath(root);
        wizard.applyStorageLocations();   // creates root/config, records root as wizard-created
        QVERIFY(QDir(root).exists());

        wizard.discardIncompleteSetup();
        QVERIFY(!QDir(root).exists());
    }

    void preExistingFolderIsPreserved() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = tmp.path();

        // A pre-existing user file that must survive discard — this stands
        // in for real user data (e.g. a folder the user pointed the wizard
        // at that already had unrelated files in it).
        const QString dummyFile = root + "/user_data.txt";
        {
            QFile f(dummyFile);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("do not delete me");
        }
        QVERIFY(QFile::exists(dummyFile));

        WizardState wizard;
        wizard.setRootPath(root);
        wizard.applyStorageLocations();   // root pre-existed -> NOT recorded as wizard-created

        wizard.discardIncompleteSetup();

        QVERIFY(QDir(root).exists());
        QVERIFY(QFile::exists(dummyFile));
    }
};

QTEST_APPLESS_MAIN(TestWizardDiscard)
#include "test_wizard_discard.moc"
