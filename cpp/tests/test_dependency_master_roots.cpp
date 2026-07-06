// Regression net for the first-render dependsOn bug (found at Packet 7
// Stage 2 GATE 8, present since b2e0d5a 2026-05-18): GenericSettingsPage's
// constructor runs refreshDependencies() BEFORE pushPage() parents the page
// into the dialog, so harvesting dependency masters from the dialog's child
// tree alone misses the page's own rows on first render — same-page gates
// (e.g. dolphin_overclock -> dolphin_overclock_enable) evaluated against a
// missing master, and unknown masters default to active (not greyed).
//
// settingsMasterRoots() is the extracted root-selection rule:
//   - no dialog            -> just the page
//   - page inside dialog   -> just the dialog (its tree includes the page)
//   - page NOT yet parented -> dialog AND page

#include <QtTest>
#include <QWidget>
#include "ui/settings/generic_settings_page.h"

class TestDependencyMasterRoots : public QObject {
    Q_OBJECT
private slots:
    void noDialog_returnsPageOnly() {
        QWidget page;
        const auto roots = settingsMasterRoots(nullptr, &page);
        QCOMPARE(roots.size(), 1);
        QCOMPARE(roots.first(), static_cast<QObject*>(&page));
    }

    void pageParentedIntoDialog_returnsDialogOnly() {
        QWidget dialog;
        auto* page = new QWidget(&dialog);
        const auto roots = settingsMasterRoots(&dialog, page);
        QCOMPARE(roots.size(), 1);
        QCOMPARE(roots.first(), static_cast<QObject*>(&dialog));
    }

    void pageNotYetParented_returnsDialogAndPage() {
        // The constructor-time case: the page exists, the dialog exists,
        // but pushPage hasn't reparented the page yet. Masters must be
        // harvested from BOTH or same-page gates see no master.
        QWidget dialog;
        QWidget page;  // no parent
        const auto roots = settingsMasterRoots(&dialog, &page);
        QCOMPARE(roots.size(), 2);
        QVERIFY(roots.contains(static_cast<QObject*>(&dialog)));
        QVERIFY(roots.contains(static_cast<QObject*>(&page)));
    }
};

QTEST_MAIN(TestDependencyMasterRoots)
#include "test_dependency_master_roots.moc"
