#include "Windows.h"
#include <bcrypt.h>
#include <fstream>
#include <utility>
#include <algorithm>

namespace daymark::update::win {
namespace {
void checked(NTSTATUS status) {
    if (status < 0)
        fail("update.crypto_failed", 31);
}
struct Algorithm {
    BCRYPT_ALG_HANDLE handle = nullptr;
    ~Algorithm() {
        if (handle)
            BCryptCloseAlgorithmProvider(handle, 0);
    }
};
struct Key {
    BCRYPT_KEY_HANDLE handle = nullptr;
    ~Key() {
        if (handle)
            BCryptDestroyKey(handle);
    }
};
struct Hash {
    Algorithm algorithm;
    BCRYPT_HASH_HANDLE handle = nullptr;
    Hash() {
        checked(
            BCryptOpenAlgorithmProvider(&algorithm.handle, BCRYPT_SHA256_ALGORITHM, nullptr, 0));
        checked(BCryptCreateHash(algorithm.handle, &handle, nullptr, 0, nullptr, 0, 0));
    }
    ~Hash() {
        if (handle)
            BCryptDestroyHash(handle);
    }
    void append(const void *data, std::size_t size) {
        checked(BCryptHashData(handle, (PUCHAR)data, static_cast<ULONG>(size), 0));
    }
    Bytes finish() {
        Bytes b(32);
        checked(BCryptFinishHash(handle, b.data(), 32, 0));
        return b;
    }
};
void importPrivate(const Bytes &bytes, Algorithm &alg, Key &key) {
    checked(BCryptOpenAlgorithmProvider(&alg.handle, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0));
    checked(BCryptImportKeyPair(alg.handle, nullptr, BCRYPT_ECCPRIVATE_BLOB, &key.handle,
                                (PUCHAR)bytes.data(), static_cast<ULONG>(bytes.size()), 0));
}
} // namespace
std::wstring wide(std::string_view s) {
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()),
                                nullptr, 0);
    if (!n)
        fail("update.invalid_encoding");
    std::wstring out(n, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()),
                        out.data(), n);
    return out;
}
std::string utf8(std::wstring_view s) {
    if (s.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()),
                                nullptr, 0, nullptr, nullptr);
    if (!n)
        fail("update.invalid_encoding");
    std::string out(n, 0);
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()),
                        out.data(), n, nullptr, nullptr);
    return out;
}
fs::path executable() {
    std::wstring value(32768, 0);
    auto n = GetModuleFileNameW(nullptr, value.data(), static_cast<DWORD>(value.size()));
    if (!n || n >= value.size())
        fail("update.process_identity", 31);
    value.resize(n);
    return fs::path(value);
}
void noLinks(const fs::path &path) {
    auto absolute = fs::absolute(path).lexically_normal();
    fs::path part;
    for (const auto &segment : absolute) {
        part /= segment;
        auto flags = GetFileAttributesW(part.c_str());
        if (flags != INVALID_FILE_ATTRIBUTES && (flags & FILE_ATTRIBUTE_REPARSE_POINT))
            fail("update.archive_unsafe");
    }
}
fs::path inside(const fs::path &root, std::string_view relative) {
    if (!safeRelative(relative))
        fail("update.archive_unsafe");
    auto path = fs::absolute(root) / wide(relative);
    noLinks(path);
    return path;
}
std::string read(const fs::path &path, std::size_t limit) {
    noLinks(path);
    Handle input(CreateFileW(path.c_str(), GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    LARGE_INTEGER size{};
    if (!input || !GetFileSizeEx(input.value, &size) || size.QuadPart < 0 ||
        static_cast<std::uint64_t>(size.QuadPart) > limit)
        fail("update.file_read", 31);
    std::string out(static_cast<std::size_t>(size.QuadPart), '\0');
    std::size_t offset = 0;
    while (offset < out.size()) {
        DWORD received = 0;
        const auto count =
            static_cast<DWORD>(std::min<std::size_t>(out.size() - offset, 1024 * 1024));
        if (!ReadFile(input.value, out.data() + offset, count, &received, nullptr) || received == 0)
            fail("update.file_read", 31);
        offset += received;
    }
    return out;
}
void writeAtomic(const fs::path &path, std::string_view data) {
    noLinks(path);
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec)
        fail("update.directory_read_only", 31);
    noLinks(path.parent_path());
    auto temp = path;
    temp += L"." + wide(randomHex()) + L".tmp";
    Handle out(CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!out) {
        Failure error("update.file_write", 31);
        error.platformError = GetLastError();
        throw error;
    }
    bool ok = true;
    DWORD error = 0;
    std::size_t offset = 0;
    while (offset < data.size()) {
        DWORD written = 0;
        DWORD n = static_cast<DWORD>(std::min<std::size_t>(data.size() - offset, 1024 * 1024));
        if (!WriteFile(out.value, data.data() + offset, n, &written, nullptr) || written != n) {
            ok = false;
            error = GetLastError();
            if (!error)
                error = ERROR_WRITE_FAULT;
            break;
        }
        offset += written;
    }
    if (!FlushFileBuffers(out.value)) {
        ok = false;
        error = GetLastError();
    }
    out = Handle();
    bool moved = false;
    const auto deadline = GetTickCount64() + 250;
    while (ok) {
        if (MoveFileExW(temp.c_str(), path.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            moved = true;
            break;
        }
        error = GetLastError();
        const auto attributes = GetFileAttributesW(path.c_str());
        const bool transientAccess = error == ERROR_ACCESS_DENIED &&
                                     attributes != INVALID_FILE_ATTRIBUTES &&
                                     !(attributes & FILE_ATTRIBUTE_READONLY);
        if ((error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION &&
             !transientAccess) ||
            GetTickCount64() >= deadline)
            break;
        Sleep(5);
    }
    if (!moved) {
        DeleteFileW(temp.c_str());
        Failure failure("update.file_write", 31);
        failure.platformError = error;
        throw failure;
    }
}
Bytes sha256(std::string_view s) {
    Hash h;
    h.append(s.data(), s.size());
    return h.finish();
}
void logEvent(const fs::path& root,Json event) noexcept
{
    try {
        const auto directory=root/"update-state/logs";noLinks(directory);fs::create_directories(directory);
        FILETIME created,exit,kernel,user;if(!GetProcessTimes(GetCurrentProcess(),&created,&exit,&kernel,&user))return;
        const auto stamp=std::to_string((std::uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime);
        const auto path=directory/("update-"+std::to_string(GetCurrentProcessId())+"-"+stamp+".jsonl");noLinks(path);
        event["format"]=1;event["pid"]=GetCurrentProcessId();event["component"]="updater";
        event["tick_ms"]=GetTickCount64();
        SYSTEMTIME utc;GetSystemTime(&utc);char timestamp[32];
        std::snprintf(timestamp,sizeof(timestamp),"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",utc.wYear,utc.wMonth,utc.wDay,utc.wHour,utc.wMinute,utc.wSecond,utc.wMilliseconds);
        event["at_utc"]=timestamp;
        const auto bytes=event.dump()+"\n";if(bytes.size()>16384)return;
        Handle file(CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));if(!file)return;
        LARGE_INTEGER size{};if(!GetFileSizeEx(file.value,&size))return;
        if(size.QuadPart>2*1024*1024){LARGE_INTEGER start{};if(!SetFilePointerEx(file.value,start,nullptr,FILE_BEGIN)||!SetEndOfFile(file.value))return;}
        LARGE_INTEGER end{};if(!SetFilePointerEx(file.value,end,nullptr,FILE_END))return;
        DWORD written=0;WriteFile(file.value,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr);FlushFileBuffers(file.value);file=Handle();
        std::vector<fs::directory_entry> entries;
        for(const auto& entry:fs::directory_iterator(directory)){
            if(!entry.is_regular_file()||entry.is_symlink())continue;const auto name=entry.path().filename().string();
            if(!name.starts_with("update-")||!name.ends_with(".jsonl"))continue;
            const auto identity=name.substr(7,name.size()-13);const auto split=identity.find('-');
            if(split==std::string::npos||identity.substr(0,split).find_first_not_of("0123456789")!=std::string::npos||identity.substr(split+1).find_first_not_of("0123456789")!=std::string::npos)continue;
            entries.push_back(entry);
        }
        if(entries.size()<=20)return;
        std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.last_write_time()>b.last_write_time();});
        for(std::size_t i=20;i<entries.size();++i){
            if(entries[i].path()==path)continue;const auto name=entries[i].path().filename().string();const auto split=name.find('-',7);
            const auto pid=static_cast<DWORD>(std::stoul(name.substr(7,split-7)));Handle process(OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid));
            if(process && WaitForSingleObject(process.value,0)==WAIT_TIMEOUT)continue;
            std::error_code ignored;fs::remove(entries[i].path(),ignored);
        }
    }catch(...){} // Diagnostics never change an installation transaction's result.
}
std::string fileHash(const fs::path &path) {
    noLinks(path);
    std::ifstream in(path, std::ios::binary);
    if (!in)
        fail("update.file_read", 31);
    Hash hash;
    std::array<char, 65536> buffer;
    while (in) {
        in.read(buffer.data(), buffer.size());
        if (auto count = in.gcount(); count > 0)
            hash.append(buffer.data(), static_cast<std::size_t>(count));
    }
    if (!in.eof())
        fail("update.file_read", 31);
    return hex(hash.finish());
}
std::string randomHex(std::size_t size) {
    if (size > 128)
        fail("update.invalid_encoding");
    Bytes b(size);
    checked(BCryptGenRandom(nullptr, b.data(), static_cast<ULONG>(size),
                            BCRYPT_USE_SYSTEM_PREFERRED_RNG));
    return hex(b);
}
Platform crypto() {
    return {sha256, [](const TrustKey &key, const Bytes &hash, const Bytes &sig) {
                if (key.xy.size() != 64 || hash.size() != 32 || sig.size() != 64)
                    return false;
                Algorithm alg;
                Key imported;
                checked(BCryptOpenAlgorithmProvider(&alg.handle, BCRYPT_ECDSA_P256_ALGORITHM,
                                                    nullptr, 0));
                BCRYPT_ECCKEY_BLOB header{BCRYPT_ECDSA_PUBLIC_P256_MAGIC, 32};
                Bytes blob(sizeof(header) + 64);
                std::memcpy(blob.data(), &header, sizeof(header));
                std::copy(key.xy.begin(), key.xy.end(), blob.begin() + sizeof(header));
                if (BCryptImportKeyPair(alg.handle, nullptr, BCRYPT_ECCPUBLIC_BLOB,
                                        &imported.handle, blob.data(),
                                        static_cast<ULONG>(blob.size()), 0) < 0)
                    return false;
                return BCryptVerifySignature(imported.handle, nullptr, (PUCHAR)hash.data(), 32,
                                             (PUCHAR)sig.data(), 64, 0) >= 0;
            }};
}
Bytes generatePrivate() {
    Algorithm alg;
    Key key;
    checked(BCryptOpenAlgorithmProvider(&alg.handle, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0));
    checked(BCryptGenerateKeyPair(alg.handle, &key.handle, 256, 0));
    checked(BCryptFinalizeKeyPair(key.handle, 0));
    ULONG size = 0;
    checked(BCryptExportKey(key.handle, nullptr, BCRYPT_ECCPRIVATE_BLOB, nullptr, 0, &size, 0));
    Bytes out(size);
    checked(
        BCryptExportKey(key.handle, nullptr, BCRYPT_ECCPRIVATE_BLOB, out.data(), size, &size, 0));
    return out;
}
TrustKey publicKey(const Bytes &bytes, std::string id) {
    Algorithm alg;
    Key key;
    importPrivate(bytes, alg, key);
    ULONG size = 0;
    checked(BCryptExportKey(key.handle, nullptr, BCRYPT_ECCPUBLIC_BLOB, nullptr, 0, &size, 0));
    Bytes blob(size);
    checked(
        BCryptExportKey(key.handle, nullptr, BCRYPT_ECCPUBLIC_BLOB, blob.data(), size, &size, 0));
    return {std::move(id), Bytes(blob.begin() + sizeof(BCRYPT_ECCKEY_BLOB), blob.end())};
}
std::string sign(std::string_view raw, const Bytes &bytes, std::string_view id,
                 std::string_view domain) {
    Algorithm alg;
    Key key;
    importPrivate(bytes, alg, key);
    std::string value(domain);
    value += '\0';
    value += raw;
    auto hash = sha256(value);
    Bytes signature(64);
    ULONG size = 0;
    checked(BCryptSignHash(key.handle, nullptr, hash.data(), 32, signature.data(), 64, &size, 0));
    return Json{{"format", 1},
                {"key_id", id},
                {"algorithm", "ecdsa-p256-sha256-p1363"},
                {"signature", encodeBase64(signature)}}
        .dump();
}
std::uint32_t osBuild() {
    using RtlVersion = LONG(WINAPI *)(OSVERSIONINFOW *);
    auto function = reinterpret_cast<RtlVersion>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    OSVERSIONINFOW value{};
    value.dwOSVersionInfoSize = sizeof(value);
    if (!function || function(&value) != 0)
        fail("update.platform_unsupported", 21);
    return value.dwBuildNumber;
}
} // namespace daymark::update::win
