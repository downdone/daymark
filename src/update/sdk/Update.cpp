#include "Update.h"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>
#include <set>

namespace daymark::update {
Failure::Failure(std::string value, int code)
    : std::runtime_error(value), key(std::move(value)), exitCode(code) {}
void fail(std::string_view key, int code) { throw Failure(std::string(key), code); }
std::string Version::text() const {
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}
Version Version::parse(std::string_view value) {
    Version out;
    std::uint32_t *parts[]{&out.major, &out.minor, &out.patch};
    for (unsigned i = 0; i < 3; ++i) {
        const auto dot = value.find('.');
        const auto part = value.substr(0, dot);
        if (part.empty() || (part.size() > 1 && part.front() == '0') ||
            (i < 2) == (dot == value.npos))
            fail("update.invalid_version");
        const auto result = std::from_chars(part.data(), part.data() + part.size(), *parts[i]);
        if (result.ec != std::errc() || result.ptr != part.data() + part.size())
            fail("update.invalid_version");
        if (i < 2)
            value.remove_prefix(dot + 1);
    }
    return out;
}
Json parseJson(std::string_view text, std::size_t limit) {
    if (text.empty() || text.size() > limit || static_cast<unsigned char>(text.front()) == 0xef)
        fail("update.invalid_json");
    std::vector<std::set<std::string>> keys;
    try {
        return Json::parse(
            text.begin(), text.end(), [&](int depth, Json::parse_event_t event, Json &value) {
                if (depth > 16)
                    fail("update.invalid_json");
                if (event == Json::parse_event_t::object_start)
                    keys.emplace_back();
                if (event == Json::parse_event_t::key &&
                    (keys.empty() || !keys.back().insert(value.get<std::string>()).second))
                    fail("update.duplicate_json_key");
                if (event == Json::parse_event_t::object_end)
                    keys.pop_back();
                if (event == Json::parse_event_t::value && value.is_number_float())
                    fail("update.invalid_json");
                return true;
            });
    } catch (const Failure &) {
        throw;
    } catch (const Json::exception &) {
        fail("update.invalid_json");
    }
}
std::string field(const Json &object, std::string_view key, std::size_t maximum) {
    const auto it = object.find(std::string(key));
    if (!object.is_object() || it == object.end() || !it->is_string())
        fail("update.invalid_manifest");
    auto result = it->get<std::string>();
    if (result.empty() || result.size() > maximum || result.find('\0') != result.npos)
        fail("update.invalid_manifest");
    return result;
}
std::uint64_t number(const Json &object, std::string_view key, std::uint64_t maximum) {
    const auto it = object.find(std::string(key));
    if (!object.is_object() || it == object.end() || !it->is_number_unsigned())
        fail("update.invalid_manifest");
    const auto result = it->get<std::uint64_t>();
    if (result > maximum)
        fail("update.invalid_manifest");
    return result;
}
bool safeRelative(std::string_view path) {
    if (path.empty() || path.size() > 220 || path.front() == '/' || path.back() == '/' ||
        path.find_first_of("\\:*?\"<>|\0", 0, 9) != path.npos)
        return false;
    while (!path.empty()) {
        auto slash = path.find('/');
        auto part = path.substr(0, slash);
        if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ')
            return false;
        for (const unsigned char c : part)
            if (c < 32 || c >= 127)
                return false; // shipped payload names are ASCII; reject aliases
        std::string upper(part.substr(0, part.find('.')));
        std::transform(upper.begin(), upper.end(), upper.begin(),
                       [](unsigned char c) { return std::toupper(c); });
        if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL" ||
            (upper.size() == 4 && (upper.starts_with("COM") || upper.starts_with("LPT")) &&
             upper[3] >= '0' && upper[3] <= '9'))
            return false;
        if (slash == path.npos)
            break;
        path.remove_prefix(slash + 1);
    }
    return true;
}
std::string hex(const Bytes &bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string value;
    value.reserve(bytes.size() * 2);
    for (auto c : bytes) {
        value += digits[c >> 4];
        value += digits[c & 15];
    }
    return value;
}
Bytes unhex(std::string_view value) {
    if (value.size() % 2)
        fail("update.invalid_encoding");
    Bytes result;
    auto digit = [](char c) -> unsigned {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        fail("update.invalid_encoding");
    };
    for (std::size_t i = 0; i < value.size(); i += 2)
        result.push_back(static_cast<unsigned char>(digit(value[i]) * 16 + digit(value[i + 1])));
    return result;
}
std::string encodeBase64(const Bytes &data) {
    constexpr char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (std::size_t i = 0; i < data.size(); i += 3) {
        unsigned n = unsigned(data[i]) << 16;
        if (i + 1 < data.size())
            n |= unsigned(data[i + 1]) << 8;
        if (i + 2 < data.size())
            n |= data[i + 2];
        out += table[(n >> 18) & 63];
        out += table[(n >> 12) & 63];
        out += i + 1 < data.size() ? table[(n >> 6) & 63] : '=';
        out += i + 2 < data.size() ? table[n & 63] : '=';
    }
    return out;
}
Bytes decodeBase64(std::string_view value) {
    if (value.empty())
        return {};
    constexpr std::string_view chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (value.empty() || value.size() % 4 || value.size() > 8192)
        fail("update.invalid_encoding");
    Bytes out;
    for (std::size_t i = 0; i < value.size(); i += 4) {
        unsigned n = 0;
        int padding = 0;
        for (unsigned j = 0; j < 4; ++j) {
            char c = value[i + j];
            if (c == '=') {
                ++padding;
                if (i + 4 != value.size() || j < 2)
                    fail("update.invalid_encoding");
                n <<= 6;
            } else {
                if (padding)
                    fail("update.invalid_encoding");
                auto d = chars.find(c);
                if (d == chars.npos)
                    fail("update.invalid_encoding");
                n = (n << 6) | unsigned(d);
            }
        }
        out.push_back(static_cast<unsigned char>(n >> 16));
        if (padding < 2)
            out.push_back(static_cast<unsigned char>(n >> 8));
        if (padding < 1)
            out.push_back(static_cast<unsigned char>(n));
    }
    if (encodeBase64(out) != value)
        fail("update.invalid_encoding");
    return out;
}
std::string versionKey(const Json &m) {
    auto v = Version::parse(field(m, "version"));
    auto source = field(m, "source_revision");
    if (source.size() != 40 || unhex(source).size() != 20)
        fail("update.invalid_manifest");
    return v.text() + "-" + source.substr(0, 12);
}
std::string runtimeFamily() {
#ifdef _MSC_VER
    return "msvc143";
#elif defined(_WIN32)
    return "mingw131";
#else
    return "unsupported";
#endif
}
Release verifyRelease(std::string raw, std::string signature, const std::vector<TrustKey> &keys,
                      const Platform &platform) {
    if (raw.size() > 65536)
        fail("update.invalid_manifest");
    const auto envelope = parseJson(signature, 4096);
    if (number(envelope, "format", 1) != 1 ||
        field(envelope, "algorithm") != "ecdsa-p256-sha256-p1363")
        fail("update.signature_invalid");
    const auto id = field(envelope, "key_id");
    const auto key =
        std::find_if(keys.begin(), keys.end(), [&](const auto &k) { return k.id == id; });
    if (key == keys.end())
        fail("update.signature_invalid");
    auto sig = decodeBase64(field(envelope, "signature"));
    std::string payload = "Daymark.UpdateManifest.v1";
    payload += '\0';
    payload += raw;
    if (sig.size() != 64 || !platform.verify(*key, platform.hash(payload), sig))
        fail("update.signature_invalid");
    auto m = parseJson(raw);
    if (number(m, "format", 1) != 1 || field(m, "repository") != Repository ||
        field(m, "channel") != "stable")
        fail("update.invalid_manifest");
    auto version = Version::parse(field(m, "version"));
    if (field(m, "tag") != "v" + version.text())
        fail("update.invalid_manifest");
    for (const auto *k : {"release_id"}) {
        auto s = field(m, k, 20);
        if (s.front() == '0' || s.find_first_not_of("0123456789") != s.npos)
            fail("update.invalid_manifest");
    }
    const auto &p = m.at("package");
    auto idValue = field(p, "asset_id", 20);
    if (idValue.front() == '0' || idValue.find_first_not_of("0123456789") != idValue.npos)
        fail("update.invalid_manifest");
    if (field(p, "layout") != "portable-managed-v1" || field(p, "entry") != "daymark.exe" ||
        field(p, "payload_dir") != "versions/" + versionKey(m) ||
        field(p, "archive_root") != "Daymark-" + version.text() + "-windows-x64" ||
        field(p, "name") != field(p, "archive_root") + ".zip" ||
        field(p, "files_index") != "FILES.json")
        fail("update.unsupported_layout", 21);
    if (number(p, "size", MaxPackage) == 0 || number(p, "unpacked_size", MaxUnpacked) == 0 ||
        number(p, "file_count", 10000) < 4)
        fail("update.invalid_manifest");
    for (const auto *name : {"sha256", "files_index_sha256"})
        if (field(p, name).size() != 64 || unhex(field(p, name)).size() != 32)
            fail("update.invalid_manifest");
    if (m.contains("required_capabilities") &&
        !m["required_capabilities"].is_array())
        fail("update.unsupported_capability",21);
    if(m.contains("required_capabilities"))for(const auto& capability:m["required_capabilities"])
        if(!capability.is_string() || capability.get<std::string>()!="native-toolchain-v1")
            fail("update.unsupported_capability",21);
    return {std::move(m), std::move(raw), std::move(signature), version};
}
void checkCompatibility(const Release &r, const Version &installed, std::string_view runtime,
                        std::uint32_t os, bool newer) {
    const auto &m = r.manifest;
    const auto &t = m.at("target");
    const auto &b = m.at("bootstrap_protocol");
    const auto &d = m.at("data");
    if (newer && r.version <= installed)
        fail("update.no_newer_version", 21);
    if (field(t, "os") != "windows" || field(t, "arch") != "x86_64" ||
        field(t, "runtime_family") != runtime || number(t, "minimum_windows_build", 100000) > os)
        fail("update.incompatible_runtime", 21);
    if (field(t, "runtime_min") != "bundled")
        fail("update.incompatible_runtime", 21);
    if (number(b, "min", 100) > 1 || number(b, "max", 100) < 1)
        fail("update.bootstrap_too_old", 21);
    if (number(d, "schema", 10000) != Schema || field(d, "read_write_compatibility") != DataAbi)
        fail("update.data_compatibility", 21);
}
std::vector<FileEntry> fileIndex(std::string_view text) {
    auto j = parseJson(text, 8 * 1024 * 1024);
    if (number(j, "format", 1) != 1 || !j.at("files").is_array() || j["files"].size() > 10000)
        fail("update.invalid_file_index");
    std::vector<FileEntry> out;
    std::set<std::string> names;
    for (const auto &item : j["files"]) {
        auto path = field(item, "path");
        auto lower = path;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (!safeRelative(path) || !names.insert(lower).second || lower == "files.json" ||
            lower == "bootstrap-release.json" || lower == "bootstrap-release.json.sig")
            fail("update.archive_unsafe");
        auto hash = field(item, "sha256");
        if (hash.size() != 64 || unhex(hash).size() != 32)
            fail("update.invalid_file_index");
        out.push_back({path, hash, number(item, "size", MaxPackage)});
    }
    return out;
}
} // namespace daymark::update
