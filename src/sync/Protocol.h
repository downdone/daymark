#pragma once
#include "domain/Task.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QSet>
#include <QUuid>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <limits>

namespace daymark::sync {
inline QString uuid() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
inline QString stableId(const QString &name) {
    return QUuid::createUuidV5(QUuid(QStringLiteral("43e1f2a4-8d21-4a8c-bb70-16b813d3f5ab")),
                               name.toUtf8())
        .toString(QUuid::WithoutBraces);
}
inline QString orderingId(const QString &kind, const QString &subject = {}) {
    return stableId(
        QStringLiteral("ordering:") + kind + QLatin1Char(':') +
        (subject.isEmpty() ? QStringLiteral("00000000-0000-0000-0000-000000000000") : subject));
}
inline QString now() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}
inline QByteArray json(const QJsonObject &v) {
    return QJsonDocument(v).toJson(QJsonDocument::Compact);
}
inline QJsonValue nullable(const QString &v) {
    return v.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(v);
}
inline QJsonValue nullable(const std::optional<QDate> &v) {
    return v ? QJsonValue(v->toString(Qt::ISODate)) : QJsonValue(QJsonValue::Null);
}
inline Error failure(const QString &key = QStringLiteral("syncInvalidPayload")) {
    return {ErrorCode::ValidationError, key};
}

// Validate lexical information before Qt's object parser can discard it. In
// particular, duplicate escaped keys and lone UTF-16 surrogates are rejected.
class StrictJson final {
  public:
    static Result<QJsonValue> parse(const QByteArray &bytes, qsizetype maximum = 64 * 1024 * 1024) {
        if (bytes.size() > maximum)
            return {{}, failure(QStringLiteral("syncPayloadTooLarge"))};
        QStringDecoder decoder(QStringDecoder::Utf8);
        const QString text = decoder(bytes);
        if (decoder.hasError())
            return {{}, failure()};
        StrictJson p(text);
        if (!p.value(0)) {
            return {{}, failure()};
        }
        p.space();
        if (p.pos != text.size())
            return {{}, failure()};
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson("[" + bytes + "]", &error);
        if (error.error != QJsonParseError::NoError || document.array().size() != 1)
            return {{}, failure()};
        return {document.array().first(), {}};
    }

  private:
    explicit StrictJson(const QString &s) : input(s) {}
    const QString &input;
    qsizetype pos = 0;
    int nodes = 0;
    void space() {
        while (pos < input.size() && (input[pos] == ' ' || input[pos] == '\t' ||
                                      input[pos] == '\r' || input[pos] == '\n'))
            ++pos;
    }
    int hex() {
        if (pos == input.size())
            return -1;
        const auto c = input[pos++].unicode();
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    }
    int unit() {
        int v = 0;
        for (int i = 0; i < 4; ++i) {
            const int h = hex();
            if (h < 0)
                return -1;
            v = (v << 4) | h;
        }
        return v;
    }
    bool string(QString *out = nullptr) {
        if (pos == input.size() || input[pos++] != '"')
            return false;
        QString s;
        while (pos < input.size()) {
            QChar c = input[pos++];
            if (c == '"') {
                if (out)
                    *out = s;
                return true;
            }
            if (c.unicode() < 32)
                return false;
            if (c == '\\') {
                if (pos == input.size())
                    return false;
                c = input[pos++];
                if (c == 'u') {
                    int h = unit();
                    if (h < 0 || (h >= 0xdc00 && h <= 0xdfff))
                        return false;
                    if (h >= 0xd800 && h <= 0xdbff) {
                        if (pos + 2 > input.size() || input[pos++] != '\\' || input[pos++] != 'u')
                            return false;
                        const int l = unit();
                        if (l < 0xdc00 || l > 0xdfff)
                            return false;
                        s += QChar(h);
                        s += QChar(l);
                    } else
                        s += QChar(h);
                } else if (c == '"' || c == '\\' || c == '/')
                    s += c;
                else if (c == 'b')
                    s += QChar(8);
                else if (c == 'f')
                    s += QChar(12);
                else if (c == 'n')
                    s += '\n';
                else if (c == 'r')
                    s += '\r';
                else if (c == 't')
                    s += '\t';
                else
                    return false;
            } else if (c.isHighSurrogate()) {
                if (pos == input.size() || !input[pos].isLowSurrogate())
                    return false;
                s += c;
                s += input[pos++];
            } else if (c.isLowSurrogate())
                return false;
            else
                s += c;
        }
        return false;
    }
    bool value(int depth) {
        if (depth > 64 || ++nodes > 2000000)
            return false;
        space();
        if (pos == input.size())
            return false;
        const QChar c = input[pos];
        if (c == '"')
            return string();
        if (c == '{' || c == '[') {
            ++pos;
            space();
            const QChar end = c == '{' ? '}' : ']';
            if (pos < input.size() && input[pos] == end) {
                ++pos;
                return true;
            }
            QSet<QString> keys;
            for (;;) {
                if (c == '{') {
                    QString key;
                    if (!string(&key) || keys.contains(key))
                        return false;
                    keys.insert(key);
                    space();
                    if (pos == input.size() || input[pos++] != ':')
                        return false;
                }
                if (!value(depth + 1))
                    return false;
                space();
                if (pos == input.size())
                    return false;
                if (input[pos] == end) {
                    ++pos;
                    return true;
                }
                if (input[pos++] != ',')
                    return false;
                space();
            }
        }
        for (const auto &literal :
             {QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("null")})
            if (input.mid(pos, literal.size()) == literal) {
                pos += literal.size();
                return true;
            }
        const auto start = pos;
        if (input[pos] == '-')
            ++pos;
        if (pos == input.size())
            return false;
        if (input[pos] == '0')
            ++pos;
        else {
            if (input[pos] < '1' || input[pos] > '9')
                return false;
            while (pos < input.size() && input[pos] >= '0' && input[pos] <= '9')
                ++pos;
        }
        if (pos < input.size() && (input[pos] == '.' || input[pos] == 'e' || input[pos] == 'E'))
            return false;
        bool ok = false;
        input.mid(start, pos - start).toLongLong(&ok);
        return ok;
    }
};

inline QByteArray canonical(const QJsonValue &v) {
    QByteArray out;
    auto count = [&](quint32 n) {
        const auto be = qToBigEndian(n);
        out.append(reinterpret_cast<const char *>(&be), 4);
    };
    if (v.isNull() || v.isUndefined())
        out.append(char(0));
    else if (v.isBool())
        out.append(v.toBool() ? char(2) : char(1));
    else if (v.isString()) {
        out.append(char(3));
        const auto b = v.toString().toUtf8();
        count(quint32(b.size()));
        out += b;
    } else if (v.isDouble()) {
        out.append(char(4));
        const auto n = qToBigEndian(quint64(v.toInteger()));
        out.append(reinterpret_cast<const char *>(&n), 8);
    } else if (v.isObject()) {
        out.append(char(5));
        const auto o = v.toObject();
        auto keys = o.keys();
        std::sort(keys.begin(), keys.end(),
                  [](const QString &a, const QString &b) { return a.toUtf8() < b.toUtf8(); });
        count(quint32(keys.size()));
        for (const auto &k : keys) {
            out += canonical(k);
            out += canonical(o.value(k));
        }
    } else {
        out.append(char(6));
        const auto a = v.toArray();
        count(quint32(a.size()));
        for (const auto &item : a)
            out += canonical(item);
    }
    return out;
}
inline QByteArray digest(const QJsonValue &v) {
    return QCryptographicHash::hash(canonical(v), QCryptographicHash::Sha256);
}
inline Result<QJsonValue> normalize(QJsonValue value, const QString &name = {}) {
    if (value.isObject()) {
        auto o = value.toObject();
        if (o.value("type") == QStringLiteral("task.create") && !o.contains("initial"))
            o.insert("initial", QJsonObject{});
        for (auto it = o.begin(); it != o.end(); ++it) {
            auto r = normalize(it.value(), it.key());
            if (!r.ok())
                return r;
            it.value() = r.value;
        }
        return {o, {}};
    }
    if (!value.isArray())
        return {value, {}};
    QJsonArray a = value.toArray();
    for (int i = 0; i < a.size(); ++i) {
        auto r = normalize(a[i]);
        if (!r.ok())
            return r;
        a[i] = r.value;
    }
    QStringList fields;
    if (QStringList{"tag_ids", "task_ids", "member_ids"}.contains(name))
        fields = {QString()};
    else if (name == "links")
        fields = {"target_id", "role"};
    else if (name == "entities")
        fields = {"type", "id"};
    else if (name == "structures" || name == "structure_revisions")
        fields = {"kind", "id"};
    else if (name == "aliases")
        fields = {"alias_id"};
    else if (name == "successor_edges")
        fields = {"rule_id", "predecessor_id"};
    if (fields.isEmpty())
        return {a, {}};
    auto key = [&](const QJsonValue &v) {
        QByteArray s;
        for (const auto &f : fields) {
            const auto x = f.isEmpty() ? v : v.toObject().value(f);
            if (!x.isString())
                return QByteArray();
            s += x.toString().toUtf8();
            s += char(0);
        }
        return s;
    };
    QList<QJsonValue> sorted;
    for (const auto &v : a) {
        if (key(v).isEmpty())
            return {{}, failure()};
        sorted.append(v);
    }
    std::sort(sorted.begin(), sorted.end(),
              [&](const auto &l, const auto &r) { return key(l) < key(r); });
    a = {};
    QByteArray previous;
    for (const auto &v : sorted) {
        const auto k = key(v);
        if (k == previous)
            return {{}, failure()};
        previous = k;
        a.append(v);
    }
    return {a, {}};
}
inline bool counter(const QJsonValue &v, qint64 *result = nullptr) {
    if (!v.isString())
        return false;
    const auto s = v.toString();
    static const QRegularExpression re(QStringLiteral("^(0|[1-9][0-9]{0,18})$"));
    bool ok = false;
    const auto n = s.toLongLong(&ok);
    if (!ok || !re.match(s).hasMatch())
        return false;
    if (result)
        *result = n;
    return true;
}
inline bool identifier(const QString &s, bool nil = false) {
    static const QRegularExpression re(
        QStringLiteral("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"));
    return re.match(s).hasMatch() &&
           (nil || s != QStringLiteral("00000000-0000-0000-0000-000000000000"));
}
} // namespace daymark::sync
