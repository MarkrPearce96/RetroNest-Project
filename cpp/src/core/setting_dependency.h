#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

// Evaluates SettingDef::dependsOn as a small boolean expression and returns
// whether the dependent should be active.
//
// Grammar (single top-level operator, no parentheses):
//   expr := term ( ('&&' | '||') term )*
//   term := key                — true when masterStates[key] is true
//         | '!' key            — true when masterStates[key] is false
//         | key '=' value      — true when masterValues[key] == value
//         | key '!=' value     — true when masterValues[key] != value
//
// Mixing '&&' and '||' in the same expression is rejected (qWarning + return
// true so the dependent stays active rather than being silently hidden).
//
// `masterStates` carries the truthy/falsy reading of toggles + combos
// (combos are "active" when their value is not in {"", "0", "false",
// "Disabled", "None"}). `masterValues` carries the raw current value of
// each master, used by the equality/inequality atoms.
//
// A bare-key expression with no operators evaluates exactly like the legacy
// "single dependsOn key" path — backward compatible with existing schema
// entries that pre-date the expression form.
inline bool evaluateDependencyExpression(
    const QString& expression,
    const QHash<QString, bool>& masterStates,
    const QHash<QString, QString>& masterValues)
{
    const QString expr = expression.trimmed();
    if (expr.isEmpty())
        return true;

    const bool hasAnd = expr.contains(QStringLiteral("&&"));
    const bool hasOr  = expr.contains(QStringLiteral("||"));
    if (hasAnd && hasOr) {
        qWarning("dependsOn mixes '&&' and '||' (no parentheses supported): %s",
                 qUtf8Printable(expr));
        return true;
    }

    const QString separator = hasOr ? QStringLiteral("||")
                                    : QStringLiteral("&&");
    const QStringList terms = expr.split(separator, Qt::SkipEmptyParts);

    auto evalTerm = [&](QString term) -> bool {
        term = term.trimmed();
        if (term.isEmpty())
            return true;

        // '!=' is detected before '=' so "key!=val" doesn't get parsed as
        // "key" / "=val".
        const int neIdx = term.indexOf(QStringLiteral("!="));
        if (neIdx >= 0) {
            const QString key = term.left(neIdx).trimmed();
            const QString val = term.mid(neIdx + 2).trimmed();
            return masterValues.value(key) != val;
        }
        const int eqIdx = term.indexOf(QChar('='));
        if (eqIdx >= 0) {
            const QString key = term.left(eqIdx).trimmed();
            const QString val = term.mid(eqIdx + 1).trimmed();
            return masterValues.value(key) == val;
        }

        bool negate = false;
        if (term.startsWith(QChar('!'))) {
            negate = true;
            term = term.mid(1).trimmed();
        }
        // Unknown master keys default to true so an unrelated typo doesn't
        // permanently grey a row out — matches the legacy single-key
        // behavior (`masterStates.value(key, true)`).
        const bool state = masterStates.value(term, true);
        return negate ? !state : state;
    };

    if (terms.isEmpty())
        return true;

    if (hasOr) {
        for (const QString& t : terms)
            if (evalTerm(t)) return true;
        return false;
    }
    // '&&' (or single term — same code path).
    for (const QString& t : terms)
        if (!evalTerm(t)) return false;
    return true;
}
