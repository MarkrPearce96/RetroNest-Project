#include "declared_options.h"
#include "libretro.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>

void populateFromV2(DeclaredOptionsDoc& doc, const retro_core_options_v2* v2)
{
    doc.categories.clear();
    doc.options.clear();
    if (!v2 || !v2->definitions)
        return;

    if (v2->categories) {
        for (const auto* c = v2->categories; c->key != nullptr; ++c) {
            DeclaredCategory cat;
            cat.key = QString::fromUtf8(c->key);
            cat.label = QString::fromUtf8(c->desc ? c->desc : c->key);
            cat.info = QString::fromUtf8(c->info ? c->info : "");
            doc.categories.append(cat);
        }
    }

    for (const auto* d = v2->definitions; d->key != nullptr; ++d) {
        DeclaredOption o;
        o.key = QString::fromUtf8(d->key);
        // Prefer the categorized (short) strings: our UI always shows options
        // inside a category page, so "Color Correction" beats
        // "Video > Color Correction".
        const char* desc = d->desc_categorized ? d->desc_categorized : d->desc;
        o.label = QString::fromUtf8(desc ? desc : d->key);
        const char* info = d->info_categorized ? d->info_categorized : d->info;
        o.info = QString::fromUtf8(info ? info : "");
        o.categoryKey = QString::fromUtf8(d->category_key ? d->category_key : "");
        o.defaultValue = QString::fromUtf8(d->default_value ? d->default_value : "");
        for (int i = 0; i < RETRO_NUM_CORE_OPTION_VALUES_MAX && d->values[i].value; ++i) {
            DeclaredOptionValue v;
            v.value = QString::fromUtf8(d->values[i].value);
            v.label = QString::fromUtf8(d->values[i].label ? d->values[i].label : "");
            o.values.append(v);
        }
        doc.options.append(o);
    }
}

QJsonDocument DeclaredOptionsDoc::toJson() const
{
    QJsonArray cats;
    for (const auto& c : categories)
        cats.append(QJsonObject{ { "key", c.key }, { "label", c.label }, { "info", c.info } });

    QJsonArray opts;
    for (const auto& o : options) {
        QJsonArray vals;
        for (const auto& v : o.values)
            vals.append(QJsonObject{ { "value", v.value }, { "label", v.label } });
        opts.append(QJsonObject{
            { "key", o.key },
            { "label", o.label },
            { "category", o.categoryKey },
            { "info", o.info },
            { "default", o.defaultValue },
            { "values", vals },
        });
    }

    return QJsonDocument(QJsonObject{
        { "format", format },
        { "core_library_version", coreLibraryVersion },
        { "categories", cats },
        { "options", opts },
    });
}

std::optional<DeclaredOptionsDoc> DeclaredOptionsDoc::fromJson(const QByteArray& bytes)
{
    const QJsonDocument jd = QJsonDocument::fromJson(bytes);
    if (!jd.isObject())
        return std::nullopt;
    const QJsonObject root = jd.object();
    if (!root.contains("options") || !root["options"].isArray())
        return std::nullopt;

    DeclaredOptionsDoc doc;
    doc.format = root["format"].toInt(1);
    doc.coreLibraryVersion = root["core_library_version"].toString();

    for (const auto& cv : root["categories"].toArray()) {
        const QJsonObject c = cv.toObject();
        doc.categories.append({ c["key"].toString(), c["label"].toString(), c["info"].toString() });
    }
    for (const auto& ov : root["options"].toArray()) {
        const QJsonObject obj = ov.toObject();
        DeclaredOption o;
        o.key = obj["key"].toString();
        o.label = obj["label"].toString();
        o.categoryKey = obj["category"].toString();
        o.info = obj["info"].toString();
        o.defaultValue = obj["default"].toString();
        for (const auto& vv : obj["values"].toArray()) {
            const QJsonObject vo = vv.toObject();
            o.values.append({ vo["value"].toString(), vo["label"].toString() });
        }
        if (!o.key.isEmpty())
            doc.options.append(o);
    }
    return doc;
}

bool DeclaredOptionsDoc::save(const QString& path) const
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(toJson().toJson(QJsonDocument::Indented));
    return f.commit();
}

std::optional<DeclaredOptionsDoc> DeclaredOptionsDoc::load(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return std::nullopt;
    return fromJson(f.readAll());
}

QVector<CoreOption> DeclaredOptionsDoc::toCoreOptions() const
{
    QVector<CoreOption> thin;
    thin.reserve(options.size());
    for (const auto& o : options) {
        CoreOption c;
        c.key = o.key;
        c.label = o.label;
        c.defaultValue = o.defaultValue;
        for (const auto& v : o.values)
            c.values << v.value;
        thin.append(c);
    }
    return thin;
}
