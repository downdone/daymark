#pragma once
#include "Protocol.h"
#include "SchemaDefinitions.h"

namespace daymark::sync {
// This deliberately small validator implements the keywords used by our pinned
// OpenAPI schemas. The shared positive AND negative fixture suite guards drift.
class Schema final {
  public:
    static const QJsonObject &definitions() {
        static const auto d = QJsonDocument::fromJson(schemaDefinitions()).object();
        return d;
    }
    static bool accepts(const QString &name, const QJsonValue &value) {
        const auto d = definitions();
        return d.contains(name) && check(d.value(name), value, 0);
    }
    static Result<QJsonObject> decode(const QString &name, const QByteArray &bytes,
                                      qsizetype maximum = 64 * 1024 * 1024) {
        auto v = StrictJson::parse(bytes, maximum);
        if (!v.ok())
            return {{}, v.error};
        if (!accepts(name, v.value))
            return {{}, failure(QStringLiteral("syncProtocolMismatch"))};
        return {v.value.toObject(), {}};
    }

  private:
    static bool type(const QString &t, const QJsonValue &v) {
        if (t == "null")
            return v.isNull();
        if (t == "object")
            return v.isObject();
        if (t == "array")
            return v.isArray();
        if (t == "string")
            return v.isString();
        if (t == "boolean")
            return v.isBool();
        if (t == "integer")
            return v.isDouble() && std::isfinite(v.toDouble()) &&
                   double(v.toInteger(std::numeric_limits<qint64>::min())) == v.toDouble();
        return t == "number" && v.isDouble();
    }
    static bool check(const QJsonValue &schema, const QJsonValue &v, int depth) {
        if (depth > 96)
            return false;
        if (schema.isBool())
            return schema.toBool();
        if (!schema.isObject())
            return false;
        const auto s = schema.toObject();
        if (s.contains("$ref")) {
            const auto ref = s.value("$ref").toString();
            const QString prefix = QStringLiteral("#/components/schemas/");
            if (!ref.startsWith(prefix) ||
                !check(definitions().value(ref.mid(prefix.size())), v, depth + 1))
                return false;
        }
        if (s.contains("type")) {
            const auto t = s.value("type");
            bool ok = false;
            if (t.isArray()) {
                for (const auto &candidate : t.toArray())
                    ok |= type(candidate.toString(), v);
            } else
                ok = type(t.toString(), v);
            if (!ok)
                return false;
        }
        if (s.contains("const") && s.value("const") != v)
            return false;
        if (s.contains("enum") && !s.value("enum").toArray().contains(v))
            return false;
        for (const auto &name :
             {QStringLiteral("allOf"), QStringLiteral("anyOf"), QStringLiteral("oneOf")})
            if (s.contains(name)) {
                int matches = 0;
                const auto a = s.value(name).toArray();
                for (const auto &sub : a)
                    if (check(sub, v, depth + 1))
                        ++matches;
                if ((name == "allOf" && matches != a.size()) || (name == "anyOf" && matches == 0) ||
                    (name == "oneOf" && matches != 1))
                    return false;
            }
        if (s.contains("not") && check(s.value("not"), v, depth + 1))
            return false;
        if (s.contains("if")) {
            const auto branch = check(s.value("if"), v, depth + 1) ? QStringLiteral("then")
                                                                   : QStringLiteral("else");
            if (s.contains(branch) && !check(s.value(branch), v, depth + 1))
                return false;
        }
        if (v.isObject()) {
            const auto o = v.toObject(), properties = s.value("properties").toObject();
            if (o.size() < s.value("minProperties").toInt(0))
                return false;
            for (const auto &key : s.value("required").toArray())
                if (!o.contains(key.toString()))
                    return false;
            for (auto it = o.begin(); it != o.end(); ++it) {
                if (properties.contains(it.key())) {
                    if (!check(properties.value(it.key()), it.value(), depth + 1))
                        return false;
                } else if (s.contains("additionalProperties") &&
                           !check(s.value("additionalProperties"), it.value(), depth + 1))
                    return false;
            }
        }
        if (v.isArray()) {
            const auto a = v.toArray();
            if (a.size() < s.value("minItems").toInt(0) ||
                a.size() > s.value("maxItems").toInt(std::numeric_limits<int>::max()))
                return false;
            QSet<QByteArray> seen;
            for (const auto &item : a) {
                if (s.contains("items") && !check(s.value("items"), item, depth + 1))
                    return false;
                if (s.value("uniqueItems").toBool()) {
                    const auto bytes = canonical(item);
                    if (seen.contains(bytes))
                        return false;
                    seen.insert(bytes);
                }
            }
        }
        if (v.isString()) {
            const auto text = v.toString();
            const auto length = text.toUcs4().size();
            if (length < s.value("minLength").toInt(0) ||
                length > s.value("maxLength").toInt(std::numeric_limits<int>::max()))
                return false;
            if (s.contains("pattern")) {
                const QRegularExpression re(s.value("pattern").toString());
                if (!re.isValid() || !re.match(text).hasMatch())
                    return false;
            }
            const auto format = s.value("format").toString();
            if (format == "uuid" && !identifier(text, true))
                return false;
            if (format == "date") {
                const auto d = QDate::fromString(text, Qt::ISODate);
                if (!d.isValid() || d.year() < 1 || d.year() > 9999 ||
                    d.toString(Qt::ISODate) != text)
                    return false;
            }
            if (format == "date-time") {
                const auto d = QDateTime::fromString(text, Qt::ISODateWithMs);
                if (!d.isValid() || d.date().year() < 1 ||
                    d.toUTC().toString(Qt::ISODateWithMs) != text)
                    return false;
            }
        }
        if (v.isDouble()) {
            if (s.contains("minimum") && v.toDouble() < s.value("minimum").toDouble())
                return false;
            if (s.contains("maximum") && v.toDouble() > s.value("maximum").toDouble())
                return false;
        }
        return true;
    }
};
} // namespace daymark::sync
