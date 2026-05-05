#include <QtTest>
#include <QHash>
#include <QString>
#include "core/setting_dependency.h"

class TestSettingDependency : public QObject {
    Q_OBJECT

private:
    QHash<QString, bool> states_;
    QHash<QString, QString> values_;

    void setMaster(const QString& key, bool active, const QString& value) {
        states_.insert(key, active);
        values_.insert(key, value);
    }

private slots:
    void init() {
        states_.clear();
        values_.clear();
    }

    // ─── Backward-compatible single-key form ───────────────────

    void testEmptyExpressionIsActive() {
        QVERIFY(evaluateDependencyExpression("", states_, values_));
    }

    void testBareKeyTruthy() {
        setMaster("Foo", true, "true");
        QVERIFY(evaluateDependencyExpression("Foo", states_, values_));
    }

    void testBareKeyFalsy() {
        setMaster("Foo", false, "false");
        QVERIFY(!evaluateDependencyExpression("Foo", states_, values_));
    }

    void testUnknownKeyDefaultsActive() {
        // Matches legacy behavior: an unknown master key returns true so a
        // typo doesn't permanently grey out a row.
        QVERIFY(evaluateDependencyExpression("Mystery", states_, values_));
    }

    // ─── Negation ─────────────────────────────────────────────

    void testBangPrefixInverts() {
        setMaster("Foo", false, "false");
        QVERIFY(evaluateDependencyExpression("!Foo", states_, values_));
    }

    void testBangPrefixOnTruthyIsFalse() {
        setMaster("Foo", true, "true");
        QVERIFY(!evaluateDependencyExpression("!Foo", states_, values_));
    }

    // ─── Equality / inequality on combo values ────────────────

    void testEqualsTrue() {
        setMaster("Backend", true, "OpenAL");
        QVERIFY(evaluateDependencyExpression("Backend=OpenAL", states_, values_));
    }

    void testEqualsFalse() {
        setMaster("Backend", true, "Cubeb");
        QVERIFY(!evaluateDependencyExpression("Backend=OpenAL", states_, values_));
    }

    void testNotEqualsTrue() {
        setMaster("DSPHLE", true, "LLE Recompiler");
        QVERIFY(evaluateDependencyExpression("DSPHLE!=HLE", states_, values_));
    }

    void testNotEqualsFalse() {
        setMaster("DSPHLE", true, "HLE");
        QVERIFY(!evaluateDependencyExpression("DSPHLE!=HLE", states_, values_));
    }

    // ─── '&&' chains ──────────────────────────────────────────

    void testAndAllTrue() {
        setMaster("A", true, "true");
        setMaster("B", true, "true");
        QVERIFY(evaluateDependencyExpression("A && B", states_, values_));
    }

    void testAndShortCircuitsOnFalse() {
        setMaster("A", true, "true");
        setMaster("B", false, "false");
        QVERIFY(!evaluateDependencyExpression("A && B", states_, values_));
    }

    void testDpl2DecoderGate() {
        // Mirrors AudioPane::OnDspChanged — DPL2 decoder enabled when
        // backend is OpenAL AND DSP is in LLE mode.
        setMaster("Backend", true, "OpenAL");
        setMaster("DSPHLE",  true, "LLE Recompiler");
        QVERIFY(evaluateDependencyExpression(
            "Backend=OpenAL && DSPHLE!=HLE", states_, values_));

        // Switch DSP back to HLE → gate closes.
        setMaster("DSPHLE", true, "HLE");
        QVERIFY(!evaluateDependencyExpression(
            "Backend=OpenAL && DSPHLE!=HLE", states_, values_));

        // Or switch backend off OpenAL → gate also closes.
        setMaster("DSPHLE",  true, "LLE Recompiler");
        setMaster("Backend", true, "Cubeb");
        QVERIFY(!evaluateDependencyExpression(
            "Backend=OpenAL && DSPHLE!=HLE", states_, values_));
    }

    // ─── '||' chains ──────────────────────────────────────────

    void testOrAnyTrue() {
        setMaster("A", false, "false");
        setMaster("B", true,  "true");
        QVERIFY(evaluateDependencyExpression("A || B", states_, values_));
    }

    void testOrAllFalse() {
        setMaster("A", false, "false");
        setMaster("B", false, "false");
        QVERIFY(!evaluateDependencyExpression("A || B", states_, values_));
    }

    void testDeferEfbCopiesGate() {
        // Mirrors HacksWidget::UpdateDeferEFBCopiesEnabled — Defer EFB
        // Copies is disabled when BOTH StoreEFB and StoreXFB are checked
        // (no RAM copy left to defer). Equivalent enabled-when:
        //   !EFBToTextureEnable || !XFBToTextureEnable
        setMaster("EFBToTextureEnable", true,  "true");
        setMaster("XFBToTextureEnable", true,  "true");
        QVERIFY(!evaluateDependencyExpression(
            "!EFBToTextureEnable || !XFBToTextureEnable", states_, values_));

        setMaster("EFBToTextureEnable", false, "false");
        QVERIFY(evaluateDependencyExpression(
            "!EFBToTextureEnable || !XFBToTextureEnable", states_, values_));
    }

    void testSkipDuplicateXfbsGate() {
        // HacksWidget::UpdateSkipPresentingDuplicateFramesEnabled — skip
        // duplicate XFBs is disabled when ImmediateXFB or VISkip is on.
        setMaster("ImmediateXFBEnable", false, "false");
        setMaster("VISkip",             false, "false");
        QVERIFY(evaluateDependencyExpression(
            "!ImmediateXFBEnable && !VISkip", states_, values_));

        setMaster("ImmediateXFBEnable", true, "true");
        QVERIFY(!evaluateDependencyExpression(
            "!ImmediateXFBEnable && !VISkip", states_, values_));
    }

    // ─── Pathological inputs ──────────────────────────────────

    void testMixedAndOrIsRejected() {
        // No parentheses means a mixed expression is ambiguous. The
        // evaluator returns false (inactive) so a malformed schema entry
        // shows up as a permanently-greyed row — that's much more
        // visible during development than a silently-always-active row.
        setMaster("A", true, "true");
        setMaster("B", true, "true");
        QVERIFY(!evaluateDependencyExpression("A && B || A", states_, values_));
    }

    void testWhitespaceTolerated() {
        setMaster("Foo", true, "Bar");
        QVERIFY(evaluateDependencyExpression("  Foo = Bar  ", states_, values_));
        QVERIFY(evaluateDependencyExpression(" Foo  ",        states_, values_));
    }
};

QTEST_MAIN(TestSettingDependency)
#include "test_setting_dependency.moc"
