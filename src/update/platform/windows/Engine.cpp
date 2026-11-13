#include "Engine.h"
#include <cctype>
#include <cwctype>
#include "Ipc.h"
#include "FileLease.h"
#include "UpdateTrust.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <limits>
#include <miniz.h>
#include <sddl.h>
#include <set>
#include <thread>

namespace daymark::update::win {
namespace {
std::int64_t now() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
Json optionalJson(const fs::path &p, std::size_t limit = 131072) {
    if (!fs::exists(p))
        return Json::object();
    return parseJson(read(p, limit), limit);
}
void jsonWrite(const fs::path &p, const Json &j) { writeAtomic(p, j.dump()); }
void removeFile(const fs::path &p) {
    noLinks(p);
    std::error_code ec;
    fs::remove(p, ec);
    if (ec)
        fail("update.file_write", 31);
}
std::wstring quote(std::wstring_view text) {
    std::wstring out = L"\"";
    unsigned slashes = 0;
    for (auto c : text) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        if (c == L'"')
            out.append(slashes * 2 + 1, L'\\');
        else
            out.append(slashes, L'\\');
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, L'\\');
    return out + L"\"";
}
std::string indexedHash(const std::vector<FileEntry> &files, std::string_view name) {
    for (const auto &f : files)
        if (f.path == name)
            return f.sha256;
    fail("update.invalid_file_index");
}
void verifyFiles(const fs::path &root, const std::vector<FileEntry> &files, std::string_view prefix,
                 bool full) {
    bool entry = false;
    for (const auto &item : files) {
        if (!item.path.starts_with(prefix))
            continue;
        auto path = inside(root, item.path);
        if (item.path == std::string(prefix) + "daymark.exe")
            entry = true;
        else if (!full)
            continue;
        std::error_code ec;
        if (fs::file_size(path, ec) != item.size || ec || fileHash(path) != item.sha256)
            fail("update.hash_mismatch");
    }
    if (!entry)
        fail("update.invalid_file_index");
}
struct Archive {
    mz_zip_archive zip{};
    std::ifstream stream;
    std::uint64_t size = 0;
    explicit Archive(const fs::path &p) : stream(p, std::ios::binary) {
        if (!stream)
            fail("update.file_read", 31);
        size = fs::file_size(p);
        zip.m_pIO_opaque = this;
        zip.m_pRead = [](void *opaque, mz_uint64 offset, void *buffer, size_t count) -> size_t {
            auto &a = *static_cast<Archive *>(opaque);
            a.stream.clear();
            a.stream.seekg(static_cast<std::streamoff>(offset));
            a.stream.read(static_cast<char *>(buffer), static_cast<std::streamsize>(count));
            return static_cast<size_t>(a.stream.gcount());
        };
        if (!mz_zip_reader_init(&zip, size, 0))
            fail("update.archive_unsafe");
    }
    ~Archive() { mz_zip_reader_end(&zip); }
};
} // namespace
FileLock::FileLock(const fs::path &p, bool exclusive, DWORD timeoutMs) {
    noLinks(p);
    auto reject = [&](DWORD error, bool opening) {
        const auto key = lockErrorKey(error, opening);
        if (p.parent_path().filename() == "locks" &&
            p.parent_path().parent_path().filename() == "update-state") {
            // Only fixed resource names are logged, never the installation path.
            const auto resource = utf8(p.filename().native());
            if (resource == "launch" || resource == "runtime" || resource == "state" ||
                resource == "prepare" || resource == "job")
                logEvent(p.parent_path().parent_path().parent_path(),
                         {{"event", "lock.failed"}, {"error", key},
                          {"platform_error", error}, {"stage", opening ? "open" : "acquire"},
                          {"resource", resource}});
        }
        Failure failure(key, error == ERROR_LOCK_VIOLATION && !opening ? 30 : 31);
        failure.platformError = error;
        throw failure;
    };
    std::error_code directoryError;
    fs::create_directories(p.parent_path(), directoryError);
    if (directoryError)
        reject(static_cast<DWORD>(directoryError.value()), true);
    noLinks(p.parent_path());
    handle = Handle(CreateFileW(p.c_str(), GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!handle)
        reject(GetLastError(), true);
    const auto error = acquireByteLock(handle.value, exclusive, overlapped, timeoutMs);
    if (error != ERROR_SUCCESS)
        reject(error, false);
}
FileLock::~FileLock() { release(); }
void FileLock::release() {
    if (handle)
        UnlockFileEx(handle.value, 0, 1, 0, &overlapped);
    handle = Handle();
}
std::string processStamp(HANDLE handle) {
    FILETIME created, exit, kernel, user;
    if (!GetProcessTimes(handle, &created, &exit, &kernel, &user))
        fail("update.process_identity", 30);
    return std::to_string((std::uint64_t(created.dwHighDateTime) << 32) | created.dwLowDateTime);
}
Child spawn(const fs::path &exe, const std::vector<std::wstring> &args, bool hidden) {
    noLinks(exe);
    std::wstring command = quote(exe.native());
    for (const auto &arg : args)
        command += L" " + quote(arg);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = hidden ? SW_HIDE : SW_SHOWNORMAL;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE,
                        hidden ? CREATE_NO_WINDOW : 0, nullptr, exe.parent_path().c_str(), &startup,
                        &process))
        fail("update.start_failed", 40);
    CloseHandle(process.hThread);
    return {Handle(process.hProcess), process.dwProcessId};
}
std::vector<TrustKey> compiledKeys() {
    std::vector<TrustKey> out;
    for (const auto &[id, xy] : BuildTrustKeys)
        out.push_back({std::string(id), unhex(xy)});
    return out;
}
fs::path installRoot(const fs::path &exe) {
    auto folder = fs::absolute(exe).parent_path();
    if (folder.parent_path().filename() == "tools" &&
        folder.parent_path().parent_path().filename() == "update-state")
        return folder.parent_path().parent_path().parent_path();
    if (folder.parent_path().filename() == "versions")
        return folder.parent_path().parent_path();
    return folder;
}
fs::path nativeTool(const Settings &settings, std::string_view role) {
    if (role != "daymark-update-cli.exe" && role != "daymark-updater.exe")
        fail("update.invalid_arguments", 2);
    const auto root = settings.root;
    const auto fallback = root / wide(role);
    if (!fs::exists(root / "bootstrap-release.json") || !fs::exists(root / "current.json"))
        return fallback;
    const auto active = parseJson(read(root / "current.json"));
    if (number(active,"format",1)!=1 || field(active,"status")!="committed")
        fail("update.recovery_required",41);
    const auto payload = field(active,"current");
    if (!safeRelative(payload) || !payload.starts_with("versions/") ||
        payload.substr(9).find('/')!=std::string::npos)
        fail("update.recovery_required",41);
    const auto tools = inside(root,"update-state/tools/"+payload.substr(9));
    auto base = tools;
    if (!fs::exists(tools)) base=root;
    const auto seed=verifySignature(read(base/"bootstrap-release.json"),
        read(base/"bootstrap-release.json.sig",4096),settings.keys,"Daymark.BootstrapPayload.v1");
    if (number(seed,"format",1)!=1 || field(seed,"repository")!=Repository ||
        field(seed,"payload_dir")!=payload || field(seed,"version")!=field(active,"version"))
        fail("update.recovery_required",41);
    const auto index=read(base/"FILES.json",8*1024*1024);
    if(hex(sha256(index))!=field(seed,"files_index_sha256"))fail("update.hash_mismatch");
    const auto files=fileIndex(index);
    const auto path=inside(base,role);
    const auto found=std::find_if(files.begin(),files.end(),[&](const auto& f){return f.path==role;});
    if(found==files.end() || !fs::is_regular_file(path) || fs::file_size(path)!=found->size || fileHash(path)!=found->sha256)
        fail("update.hash_mismatch");
    return path;
}
std::optional<DWORD> forwardNative(const Settings &settings, std::string_view role,
                                   const std::vector<std::wstring> &arguments) {
    if (!fs::exists(settings.root/"bootstrap-release.json")) return {};
    const auto target=nativeTool(settings,role);
    if(fs::equivalent(target,executable()))return {};
    if(role=="daymark-updater.exe"){
        spawn(target,arguments,false);
        return 0;
    }
    std::wstring command=quote(target.native());
    for(const auto& arg:arguments)command+=L" "+quote(arg);
    // Preserve CLI JSON output without inheriting any installation/lease handles.
    SECURITY_ATTRIBUTES attributes{sizeof(attributes),nullptr,TRUE};
    Handle output, error;
    auto copyStream=[&](DWORD which,Handle& result){
        HANDLE copy=nullptr;auto source=GetStdHandle(which);
        if(source && source!=INVALID_HANDLE_VALUE &&
           DuplicateHandle(GetCurrentProcess(),source,GetCurrentProcess(),&copy,0,TRUE,DUPLICATE_SAME_ACCESS))result=Handle(copy);
        else result=Handle(CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&attributes,OPEN_EXISTING,0,nullptr));
        if(!result)fail("update.start_failed",40);
    };
    copyStream(STD_OUTPUT_HANDLE,output);copyStream(STD_ERROR_HANDLE,error);
    HANDLE inherited[]{output.value,error.value};
    SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
    Bytes storage(bytes);auto* list=reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if(!InitializeProcThreadAttributeList(list,1,0,&bytes))fail("update.start_failed",40);
    if(!UpdateProcThreadAttribute(list,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr)){
        DeleteProcThreadAttributeList(list);fail("update.start_failed",40);
    }
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);
    startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
    startup.StartupInfo.wShowWindow=role=="daymark-updater.exe"?SW_SHOWNORMAL:SW_HIDE;
    startup.StartupInfo.hStdOutput=output.value;startup.StartupInfo.hStdError=error.value;
    startup.lpAttributeList=list;PROCESS_INFORMATION process{};
    auto ok=CreateProcessW(target.c_str(),command.data(),nullptr,nullptr,TRUE,
        EXTENDED_STARTUPINFO_PRESENT|CREATE_NO_WINDOW,nullptr,settings.root.c_str(),&startup.StartupInfo,&process);
    DeleteProcThreadAttributeList(list);
    if(!ok)fail("update.start_failed",40);
    Handle child(process.hProcess),thread(process.hThread);
    if(WaitForSingleObject(child.value,INFINITE)!=WAIT_OBJECT_0)fail("update.start_failed",40);
    DWORD exit=40;if(!GetExitCodeProcess(child.value,&exit))fail("update.start_failed",40);
    return exit;
}
bool requestRestart(const fs::path &root) {
    try {
        auto path = fs::weakly_canonical(root).generic_wstring();
        for (auto &c : path)
            c = std::towlower(c);
        auto name =
            L"\\\\.\\pipe\\daymark-update-control-" + wide(hex(sha256(utf8(path))).substr(0, 32));
        if (!WaitNamedPipeW(name.c_str(), 1000))
            return false;
        Handle pipe(CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr));
        if (!pipe)
            return false;
        ipc::send(pipe.value, "{\"type\":\"RESTART\"}", 3000);
        return parseJson(ipc::receive(pipe.value, 3000)).value("accepted", false);
    } catch (...) {
        return false;
    }
}
Json verifySignature(std::string_view raw, std::string_view signature,
                     const std::vector<TrustKey> &keys, std::string_view domain) {
    if (raw.size() > 65536)
        fail("update.invalid_manifest");
    auto e = parseJson(signature, 4096);
    if (number(e, "format", 1) != 1 || field(e, "algorithm") != "ecdsa-p256-sha256-p1363")
        fail("update.signature_invalid");
    auto id = field(e, "key_id");
    auto key = std::find_if(keys.begin(), keys.end(), [&](const auto &k) { return k.id == id; });
    if (key == keys.end())
        fail("update.signature_invalid");
    std::string input(domain);
    input += '\0';
    input += raw;
    auto sig = decodeBase64(field(e, "signature"));
    if (!crypto().verify(*key, sha256(input), sig))
        fail("update.signature_invalid");
    return parseJson(raw);
}
Engine::Engine(Settings value, Cancellation &cancellation, Notify callback)
    : settings(std::move(value)), cancel(cancellation), notify(std::move(callback)) {
    settings.root = fs::absolute(settings.root).lexically_normal();
    noLinks(settings.root);
    const auto installGate=settings.root.parent_path()/(L"."+settings.root.filename().native()+L".daymark-setup.lock");
    if(!settings.installationOperation && fs::exists(installGate))installationLease=std::make_unique<FileLock>(installGate,false);
    if (settings.root.native().starts_with(L"\\\\"))
        fail("update.directory_unsupported", 21);
    wchar_t volume[MAX_PATH]{}, filesystem[MAX_PATH]{};
    if (!GetVolumePathNameW(settings.root.c_str(), volume, MAX_PATH) ||
        GetDriveTypeW(volume) != DRIVE_FIXED ||
        !GetVolumeInformationW(volume, nullptr, 0, nullptr, nullptr, nullptr, filesystem,
                               MAX_PATH) ||
        _wcsicmp(filesystem, L"NTFS") != 0)
        fail("update.directory_unsupported", 21);
}
void Engine::publish(std::string_view state, Json extra) {
    if (!extra.is_object())
        extra = Json::object();
    extra["state"] = state;
    extra["format"] = 1;
    extra["owner_pid"] = GetCurrentProcessId();
    extra["owner_stamp"] = processStamp(GetCurrentProcess());
    if(state!=loggedState){logEvent(settings.root,extra);loggedState=std::string(state);}
    {
        FileLock lock(settings.root / "update-state/locks/state", true);
        auto old = optionalJson(settings.root / "update-state/status.json");
        extra["revision"] = old.value("revision", std::uint64_t(0)) + 1;
        jsonWrite(settings.root / "update-state/status.json", extra);
    }
    if (notify)
        notify(extra);
}
Release Engine::trusted(std::string raw, std::string signature) const {
    if (settings.keys.empty())
        fail("update.trust_not_configured", 21);
    return verifyRelease(std::move(raw), std::move(signature), settings.keys, crypto());
}
Json Engine::descriptor(std::string_view relative, bool full, bool verify) const {
    if (!safeRelative(relative) || !relative.starts_with("versions/"))
        fail("update.recovery_required", 41);
    const auto saved =
        inside(settings.root, "update-state/releases/" + std::string(relative.substr(9)));
    Json desc;
    std::string index;
    if (fs::exists(saved / "manifest.json")) {
        auto r = trusted(read(saved / "manifest.json"), read(saved / "manifest.json.sig", 4096));
        desc = r.manifest;
        if (field(desc["package"], "payload_dir") != relative)
            fail("update.recovery_required", 41);
        index = read(saved / "FILES.json", 8 * 1024 * 1024);
        if (hex(sha256(index)) != field(desc["package"], "files_index_sha256"))
            fail("update.hash_mismatch");
    } else {
        desc = verifySignature(read(settings.root / "bootstrap-release.json"),
                               read(settings.root / "bootstrap-release.json.sig", 4096),
                               settings.keys, "Daymark.BootstrapPayload.v1");
        if (number(desc, "format", 1) != 1 || field(desc, "repository") != Repository ||
            field(desc, "payload_dir") != relative || field(desc, "entry") != "daymark.exe")
            fail("update.recovery_required", 41);
        index = read(settings.root / "FILES.json", 8 * 1024 * 1024);
        if (hex(sha256(index)) != field(desc, "files_index_sha256"))
            fail("update.hash_mismatch");
    }
    if (verify)
        verifyFiles(settings.root, fileIndex(index), std::string(relative) + "/", full);
    desc["index_raw"] = index;
    return desc;
}
std::vector<FileEntry> Engine::ownedFiles() const {
    auto seed=verifySignature(read(settings.root/"bootstrap-release.json"),read(settings.root/"bootstrap-release.json.sig",4096),settings.keys,"Daymark.BootstrapPayload.v1");
    const auto initial=descriptor(field(seed,"payload_dir"),false,false);
    auto result=fileIndex(field(initial,"index_raw",8*1024*1024));
    for(const auto& name:{"bootstrap-release.json","bootstrap-release.json.sig","FILES.json"}){
        const auto path=settings.root/name;result.push_back({name,fileHash(path),fs::file_size(path)});
    }
    if(fs::exists(settings.root/"versions"))for(const auto& item:fs::directory_iterator(settings.root/"versions")) {
        if(!item.is_directory())continue;
        const auto relative="versions/"+utf8(item.path().filename().native());
        if(relative==field(seed,"payload_dir"))continue;
        try {const auto description=descriptor(relative,false,false);auto entries=fileIndex(field(description,"index_raw",8*1024*1024));
            for(auto& entry:entries)if(entry.path.starts_with(relative+"/"))result.push_back(std::move(entry));
        }catch(const Failure&){/* Unknown/untrusted directories are never owned by this installation. */}
    }
    const auto tools=settings.root/"update-state/tools";
    if(fs::exists(tools))for(const auto& directory:fs::directory_iterator(tools)){
        try{
            noLinks(directory.path());
            if(!directory.is_directory())continue;
            const auto metadata=verifySignature(read(directory.path()/"bootstrap-release.json"),
                read(directory.path()/"bootstrap-release.json.sig",4096),settings.keys,"Daymark.BootstrapPayload.v1");
            if(field(metadata,"repository")!=Repository || field(metadata,"payload_dir")!="versions/"+utf8(directory.path().filename().native()))continue;
            const auto raw=read(directory.path()/"FILES.json",8*1024*1024);
            if(hex(sha256(raw))!=field(metadata,"files_index_sha256"))continue;
            const auto prefix="update-state/tools/"+utf8(directory.path().filename().native())+"/";
            for(const auto& entry:fileIndex(raw))if(entry.path=="daymark.exe" || entry.path=="daymark-update-cli.exe" || entry.path=="daymark-updater.exe" || entry.path=="daymark-setup-helper.exe" || entry.path.starts_with("update-licenses/"))
                result.push_back({prefix+entry.path,entry.sha256,entry.size});
            for(const auto* name:{"bootstrap-release.json","bootstrap-release.json.sig","FILES.json"}){
                const auto path=directory.path()/name;result.push_back({prefix+name,fileHash(path),fs::file_size(path)});
            }
        }catch(const Failure&){/* Untrusted tool directories are never owned. */}
    }
    return result;
}
Version Engine::installedVersion() const {
    const auto state=parseJson(read(settings.root/"current.json"));
    if(number(state,"format",1)!=1 || field(state,"status")!="committed")fail("update.recovery_required",41);
    const auto trusted=descriptor(field(state,"current"),false,false);
    if(field(state,"version")!=field(trusted,"version"))fail("update.recovery_required",41);
    return Version::parse(field(trusted,"version"));
}
Json Engine::current() {
    const auto file = settings.root / "current.json";
    if (fs::exists(file)) {
        auto j = parseJson(read(file));
        if (number(j, "format", 1) != 1 || field(j, "status") != "committed")
            fail("update.recovery_required", 41);
        descriptor(field(j, "current"), false);
        return j;
    }
    FileLock gate(settings.root / "update-state/locks/launch", true, 10000);
    if (fs::exists(file))
        return parseJson(read(file));
    auto seed = verifySignature(read(settings.root / "bootstrap-release.json"),
                                read(settings.root / "bootstrap-release.json.sig", 4096),
                                settings.keys, "Daymark.BootstrapPayload.v1");
    auto path = field(seed, "payload_dir");
    descriptor(path, true);
    auto j = Json{{"format", 1},
                  {"generation", 1},
                  {"status", "committed"},
                  {"current", path},
                  {"previous", nullptr},
                  {"version", field(seed, "version")},
                  {"runtime_family", field(seed, "runtime_family")},
                  {"transaction_id", "seed"},
                  {"install_id", randomHex()}};
    jsonWrite(file, j);
    jsonWrite(settings.root / "update-state/current.last-good.json", j);
    return j;
}
Json Engine::status() const {
    auto j = optionalJson(settings.root / "update-state/status.json");
    if (j.empty())
        j = {{"format", 1}, {"state", "idle"}};
    j["ready"] = fs::exists(settings.root / "update-state/prepared-update.json");
    j["auto_update"] =
        optionalJson(settings.root / "update-state/preferences.json").value("auto_update", true);
    j["trust_configured"] = !settings.keys.empty();
    return j;
}
bool Engine::shouldCheck(bool manual) const {
    auto p = optionalJson(settings.root / "update-state/preferences.json");
    if (!manual && !p.value("auto_update", true))
        return false;
    auto time = now();
    const auto deadline = p.value(manual ? "rate_limit_until" : "not_before", std::int64_t(0));
    return time >= deadline &&
           (manual || time - p.value("last_attempt", std::int64_t(0)) >= 86400);
}
void Engine::setAuto(bool enabled) {
    FileLock lock(settings.root / "update-state/locks/state", true);
    auto p = optionalJson(settings.root / "update-state/preferences.json");
    p["auto_update"] = enabled;
    jsonWrite(settings.root / "update-state/preferences.json", p);
}
std::optional<Release> Engine::check(bool manual) {
    {
        auto preferences = optionalJson(settings.root / "update-state/preferences.json");
        auto deadline = preferences.value("rate_limit_until", std::int64_t(0));
        if (deadline > now()) {
            Failure error("update.rate_limited", 11);
            error.retryAtUtcSeconds = deadline;
            throw error;
        }
    }
    auto installed = current();
    if (settings.keys.empty())
        fail("update.trust_not_configured", 21);
    publish("checking");
    {
        FileLock lock(settings.root / "update-state/locks/state", true);
        auto p = optionalJson(settings.root / "update-state/preferences.json");
        p["last_attempt"] = now();
        jsonWrite(settings.root / "update-state/preferences.json", p);
    }
    try {
        std::vector<Json> candidates;
        unsigned requests = 0;
        bool complete = false;
        const auto started = std::chrono::steady_clock::now();
        auto request = [&](std::string url, std::uint64_t limit) {
            if (++requests > 12 ||
                std::chrono::steady_clock::now() - started > std::chrono::seconds(90))
                fail("update.discovery_incomplete", 10);
            const auto path =
                inside(settings.root, "update-state/metadata/" + hex(sha256(url)) + ".json");
            auto cached = optionalJson(path, 4 * 1024 * 1024);
            std::map<std::string, std::string> headers{{"Accept", "application/vnd.github+json"},
                                                       {"X-GitHub-Api-Version", "2026-03-10"}};
            const auto etag = cached.value("etag", std::string());
            if (cached.value("url", std::string()) == url && !etag.empty())
                headers["If-None-Match"] = etag;
            auto response = get(url, limit, cancel, settings.network, headers);
            if (response.status == 304) {
                if (!headers.contains("If-None-Match") || !cached.contains("body"))
                    fail("update.discovery_incomplete", 10);
                response.body = field(cached, "body", static_cast<std::size_t>(limit));
            } else if (!response.etag.empty())
                jsonWrite(path, {{"url", url}, {"etag", response.etag}, {"body", response.body}});
            return response;
        };
        for (unsigned page = 1; page <= 3; ++page) {
            auto response =
                request(settings.api + "/releases?per_page=30&page=" + std::to_string(page),
                        2 * 1024 * 1024);
            auto releases = parseJson(response.body, 2 * 1024 * 1024);
            if (!releases.is_array())
                fail("update.invalid_manifest");
            for (const auto &r : releases) {
                if (r.value("draft", true) || r.value("prerelease", true))
                    continue;
                auto tag = r.value("tag_name", std::string());
                try {
                    if (tag.starts_with('v') &&
                        Version::parse(tag.substr(1)) > Version::parse(field(installed, "version")))
                        candidates.push_back(r);
                } catch (const Failure &) {
                }
            }
            if (releases.size() < 30) {
                complete = true;
                break;
            }
        }
        std::sort(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) {
            return Version::parse(a["tag_name"].template get<std::string>().substr(1)) >
                   Version::parse(b["tag_name"].template get<std::string>().substr(1));
        });
        bool pending = false;
        for (const auto &r : candidates) {
            cancel.check();
            const auto releaseId =
                r.at("id").is_string()
                    ? r.at("id").get<std::string>()
                    : std::to_string(number(r, "id", std::numeric_limits<std::uint64_t>::max()));
            auto assets =
                parseJson(request(settings.api + "/releases/" + releaseId + "/assets?per_page=100",
                                  2 * 1024 * 1024)
                              .body,
                          2 * 1024 * 1024);
            if (!assets.is_array() || assets.size() >= 100)
                fail("update.discovery_incomplete", 10);
            auto asset = [&](std::string_view name) -> Json {
                Json result;
                for (const auto &a : assets)
                    if (a.value("name", std::string()) == name) {
                        if (!result.is_null())
                            fail("update.duplicate_asset");
                        result = a;
                    }
                return result;
            };
            const auto m = asset("update-windows-x64.json"),
                       s = asset("update-windows-x64.json.sig");
            if (m.is_null() || s.is_null()) {
                pending = true;
                continue;
            }
            // Accept octet-stream is needed to obtain bytes, not the JSON asset record.
            auto rawAsset = [&](const Json &a, std::uint64_t limit) {
                if (++requests > 12)
                    fail("update.discovery_incomplete", 10);
                auto id = a.at("id").is_string()
                              ? a["id"].get<std::string>()
                              : std::to_string(
                                    number(a, "id", std::numeric_limits<std::uint64_t>::max()));
                return get(settings.api + "/releases/assets/" + id, limit, cancel, settings.network,
                           {{"Accept", "application/octet-stream"},
                            {"X-GitHub-Api-Version", "2026-03-10"}})
                    .body;
            };
            auto release = trusted(rawAsset(m, 65536), rawAsset(s, 4096));
            if (field(release.manifest, "release_id") != releaseId ||
                field(release.manifest, "tag") != field(r, "tag_name"))
                fail("update.asset_identity");
            const auto &p = release.manifest["package"];
            auto zip = asset(field(p, "name"));
            if (zip.is_null()) {
                pending = true;
                continue;
            }
            auto zipId =
                zip["id"].is_string()
                    ? zip["id"].get<std::string>()
                    : std::to_string(number(zip, "id", std::numeric_limits<std::uint64_t>::max()));
            if (zipId != field(p, "asset_id") ||
                number(zip, "size", MaxPackage) != number(p, "size", MaxPackage) ||
                field(zip, "state") != "uploaded")
                fail("update.asset_identity");
            if (zip.contains("digest") && !zip["digest"].is_null() &&
                field(zip, "digest") != "sha256:" + field(p, "sha256"))
                fail("update.hash_mismatch");
            checkCompatibility(release, Version::parse(field(installed, "version")),
                               field(installed, "runtime_family"), osBuild());
            auto preferences = optionalJson(settings.root / "update-state/preferences.json");
            if (!manual && preferences.value("cancelled_version", std::string()) == release.version.text() &&
                preferences.value("cancelled_until", std::int64_t(0)) > now()) {
                publish("deferred");
                return {};
            }
            publish("available", {{"version", release.version.text()},
                                  {"bytes", number(p, "size", MaxPackage)}});
            return release;
        }
        if (pending)
            fail("update.assets_pending", 10);
        if (!complete)
            fail("update.discovery_incomplete", 10);
        publish("up_to_date");
        return {};
    } catch (const Failure &e) {
        FileLock lock(settings.root / "update-state/locks/state", true);
        auto p = optionalJson(settings.root / "update-state/preferences.json");
        p["not_before"] = std::max(now() + 3600, e.retryAtUtcSeconds);
        if (e.exitCode == 11)
            p["rate_limit_until"] = p["not_before"];
        jsonWrite(settings.root / "update-state/preferences.json", p);
        throw;
    }
}
void Engine::stage(const Release &r, const fs::path &archivePath) {
    publish("staging");
    const auto &p = r.manifest["package"];
    Archive archive(archivePath);
    const auto count = mz_zip_reader_get_num_files(&archive.zip);
    if (count > 11000)
        fail("update.archive_unsafe");
    const auto archiveRoot = field(p, "archive_root") + "/";
    std::vector<std::pair<mz_uint, std::string>> entries;
    std::set<std::string> names;
    std::uint64_t total = 0;
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&archive.zip, i, &stat))
            fail("update.archive_unsafe");
        auto length = mz_zip_reader_get_filename(&archive.zip, i, nullptr, 0);
        if (!length || length > 512)
            fail("update.archive_unsafe");
        std::string name(length, '\0');
        mz_zip_reader_get_filename(&archive.zip, i, name.data(), length);
        name.resize(length - 1);
        if (!name.starts_with(archiveRoot))
            fail("update.archive_unsafe");
        name.erase(0, archiveRoot.size());
        if (stat.m_is_directory) {
            if (name.empty())
                continue;
            if (name.back() == '/')
                name.pop_back();
            if (!safeRelative(name))
                fail("update.archive_unsafe");
            continue;
        }
        if (!safeRelative(name) || stat.m_is_encrypted || stat.m_bit_flag & 1 ||
            (stat.m_method != 0 && stat.m_method != 8) ||
            ((stat.m_external_attr >> 16) & 0170000) == 0120000 || stat.m_uncomp_size > MaxPackage)
            fail("update.archive_unsafe");
        auto lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (!names.insert(lower).second)
            fail("update.archive_unsafe");
        total += stat.m_uncomp_size;
        if (total > MaxUnpacked)
            fail("update.archive_unsafe");
        entries.emplace_back(i, name);
    }
    if (entries.size() != number(p, "file_count", 10000) ||
        total != number(p, "unpacked_size", MaxUnpacked))
        fail("update.archive_unsafe");
    auto readEntry = [&](std::string_view name, std::size_t limit) {
        auto it = std::find_if(entries.begin(), entries.end(),
                               [&](const auto &e) { return e.second == name; });
        if (it == entries.end())
            fail("update.invalid_file_index");
        mz_zip_archive_file_stat stat{};
        mz_zip_reader_file_stat(&archive.zip, it->first, &stat);
        if (stat.m_uncomp_size > limit)
            fail("update.archive_unsafe");
        std::string result(static_cast<size_t>(stat.m_uncomp_size), 0);
        if (!mz_zip_reader_extract_to_mem(&archive.zip, it->first, result.data(), result.size(), 0))
            fail("update.archive_unsafe");
        return result;
    };
    const auto index = readEntry("FILES.json", 8 * 1024 * 1024);
    if (hex(sha256(index)) != field(p, "files_index_sha256"))
        fail("update.hash_mismatch");
    const auto files = fileIndex(index);
    if (files.size() + 3 != entries.size())
        fail("update.invalid_file_index");
    auto seedRaw = readEntry("bootstrap-release.json", 65536),
         seedSig = readEntry("bootstrap-release.json.sig", 4096);
    auto seed = verifySignature(seedRaw, seedSig, settings.keys, "Daymark.BootstrapPayload.v1");
    if (field(seed, "files_index_sha256") != field(p, "files_index_sha256") ||
        field(seed, "payload_dir") != field(p, "payload_dir") ||
        field(seed, "version") != r.version.text())
        fail("update.invalid_manifest");
    for (const auto &[i, name] : entries)
        if (name != "FILES.json" && name != "bootstrap-release.json" &&
            name != "bootstrap-release.json.sig")
            indexedHash(files, name);
    const auto txn = randomHex();
    const auto staging = inside(settings.root, ".staging/" + txn);
    if (fs::exists(staging))
        fail("update.directory_conflict", 31);
    fs::create_directories(staging);
    const auto needed = number(p, "unpacked_size", MaxUnpacked) + 128 * 1024 * 1024;
    if (fs::space(settings.root).available < needed)
        fail("update.insufficient_space", 31);
    try {
        for (const auto &[i, name] : entries) {
            cancel.check();
            auto target = inside(staging, name);
            fs::create_directories(target.parent_path());
            std::ofstream out(target, std::ios::binary | std::ios::trunc);
            if (!out)
                fail("update.file_write", 31);
            struct Write {
                std::ofstream &stream;
                Cancellation &cancel;
                std::uint64_t count = 0;
            };
            Write writer{out, cancel};
            auto callback = [](void *opaque, mz_uint64 offset, const void *bytes,
                               size_t count) -> size_t {
                auto &w = *static_cast<Write *>(opaque);
                if (w.cancel.requested.load() || offset != w.count || w.count + count > MaxPackage)
                    return 0;
                w.stream.write(static_cast<const char *>(bytes),
                               static_cast<std::streamsize>(count));
                if (!w.stream)
                    return 0;
                w.count += count;
                return count;
            };
            if (!mz_zip_reader_extract_to_callback(&archive.zip, i, callback, &writer, 0))
                fail(cancel.requested ? "update.cancelled" : "update.archive_unsafe",
                     cancel.requested ? 50 : 20);
            out.close();
        }
        for (const auto &f : files) {
            cancel.check();
            auto target = inside(staging, f.path);
            if (fs::file_size(target) != f.size || fileHash(target) != f.sha256)
                fail("update.hash_mismatch");
        }
        const auto relative = field(p, "payload_dir");
        const auto destination = inside(settings.root, relative);
        // A cancelled Ready or an interrupted receipt write may leave an immutable
        // candidate. Reuse only an exactly verified signed payload; never overwrite it.
        const bool existing = fs::exists(destination);
        if (existing)
            verifyFiles(settings.root, files, relative + "/", true);
        fs::create_directories(destination.parent_path());
        auto preflight = spawn(inside(staging, relative + "/daymark.exe"), {L"--update-preflight"});
        if (WaitForSingleObject(preflight.process.value, 30000) != WAIT_OBJECT_0) {
            TerminateProcess(preflight.process.value, 40);
            fail("update.preflight_failed", 40);
        }
        DWORD exit = 1;
        GetExitCodeProcess(preflight.process.value, &exit);
        if (exit)
            fail("update.preflight_failed", 40);
        auto save = inside(settings.root, "update-state/releases/" + versionKey(r.manifest));
        fs::create_directories(save);
        writeAtomic(save / "manifest.json", r.raw);
        writeAtomic(save / "manifest.json.sig", r.signature);
        writeAtomic(save / "FILES.json", index);
        // Keep complete signed native tools beside this immutable application
        // version. current.json selects both, so prepare never changes the host.
        const auto tools=inside(settings.root,"update-state/tools/"+versionKey(r.manifest));
        if(!fs::exists(tools)){
            const auto sourceTools=inside(staging,"native-tools");
            fs::create_directories(sourceTools);
            for(const auto* name:{"daymark.exe","daymark-update-cli.exe","daymark-updater.exe","daymark-setup-helper.exe"}){
                const auto source=inside(staging,name);
                if(!fs::is_regular_file(source)){
                    if(std::string_view(name)=="daymark-setup-helper.exe")continue;
                    fail("update.invalid_file_index");
                }
                fs::copy_file(source,sourceTools/name);
            }
            for(const auto& entry:files)if(entry.path.starts_with("update-licenses/")){
                const auto target=inside(sourceTools,entry.path);
                fs::create_directories(target.parent_path());
                fs::copy_file(inside(staging,entry.path),target);
            }
            writeAtomic(sourceTools/"bootstrap-release.json",seedRaw);
            writeAtomic(sourceTools/"bootstrap-release.json.sig",seedSig);
            writeAtomic(sourceTools/"FILES.json",index);
            fs::create_directories(tools.parent_path());
            fs::rename(sourceTools,tools);
        }
        // Verify reuse, including a partial/tampered directory from a previous
        // interrupted preparation, before making any Ready receipt visible.
        if(read(tools/"bootstrap-release.json")!=seedRaw || read(tools/"bootstrap-release.json.sig",4096)!=seedSig ||
           read(tools/"FILES.json",8*1024*1024)!=index)fail("update.hash_mismatch");
        for(const auto* name:{"daymark-update-cli.exe","daymark-updater.exe"}){
            const auto path=inside(tools,name);
            if(fileHash(path)!=indexedHash(files,name))fail("update.hash_mismatch");
        }
        for(const auto& entry:files)if(entry.path.starts_with("update-licenses/"))
            if(fileHash(inside(tools,entry.path))!=entry.sha256)fail("update.hash_mismatch");
        if (!existing)
            fs::rename(inside(staging, relative), destination);
        auto old = current();
        jsonWrite(settings.root / "update-state/prepared-update.json",
                  {{"format", 1},
                   {"transaction_id", txn},
                   {"expected_generation", old["generation"]},
                   {"payload_dir", relative},
                   {"version", r.version.text()},
                   {"manifest_sha256", hex(sha256(r.raw))}});
        std::error_code cleanup;
        fs::remove_all(staging, cleanup);
        publish("ready", {{"version", r.version.text()}});
    } catch (...) {
        std::error_code ec;
        fs::remove_all(staging, ec);
        throw;
    }
}
void Engine::prepare(const Release &release) try {
    // Revalidate at the public SDK boundary; a caller cannot promote an edited
    // in-memory DTO to a trusted package by skipping discovery.
    const auto verified = trusted(release.raw, release.signature);
    if (verified.manifest != release.manifest || verified.version != release.version)
        fail("update.invalid_manifest");
    const auto installed = current();
    checkCompatibility(verified, Version::parse(field(installed, "version")),
                       field(installed, "runtime_family"), osBuild());
    FileLock lock(settings.root / "update-state/locks/prepare", true);
    cancel.check();
    if (fs::exists(settings.root / "update-state/prepared-update.json")) {
        publish("ready");
        return;
    }
    auto j = optionalJson(settings.root / "update-state/journal.json");
    if (j.value("cleanup", std::string()) == "pending")
        fail("update.cleanup_pending", 31);
    const auto &p = release.manifest["package"];
    auto cache = inside(settings.root, "update-state/downloads/" + versionKey(release.manifest));
    fs::create_directories(cache);
    auto zip = cache / "package.zip";
    const auto size = number(p, "size", MaxPackage);
    if (!fs::exists(zip) || fs::file_size(zip) != size || fileHash(zip) != field(p, "sha256")) {
        if (fs::space(settings.root).available <
            size + number(p, "unpacked_size", MaxUnpacked) + 128 * 1024 * 1024)
            fail("update.insufficient_space", 31);
        const auto part = cache / "package.zip.part", receiptPath = cache / "download.json";
        auto receipt = optionalJson(receiptPath);
        std::uint64_t offset = 0;
        std::string etag;
        if (fs::exists(part) && receipt.value("asset_id", std::string()) == field(p, "asset_id") &&
            receipt.value("sha256", std::string()) == field(p, "sha256") &&
            receipt.value("size", std::uint64_t(0)) == size) {
            etag = receipt.value("etag", std::string());
            offset = receipt.value("received", std::uint64_t(0));
            if (etag.size() < 2 || etag.front() != '"' || etag.starts_with("W/") || offset > size ||
                fs::file_size(part) < offset) {
                offset = 0;
                etag.clear();
            } else if (fs::file_size(part) != offset)
                fs::resize_file(part, offset);
        }
        auto received = offset;
        std::ofstream out;
        auto last = std::chrono::steady_clock::now();
        auto checkpoint = last;
        auto saveReceipt = [&] {
            if (!out.is_open())
                return;
            out.flush();
            if (!out)
                fail("update.file_write", 31);
            jsonWrite(receiptPath, {{"asset_id", field(p, "asset_id")},
                                    {"sha256", field(p, "sha256")},
                                    {"size", size},
                                    {"etag", etag},
                                    {"received", received}});
        };
        std::map<std::string, std::string> headers{{"Accept", "application/octet-stream"}};
        if (offset) {
            headers["Range"] = "bytes=" + std::to_string(offset) + "-";
            headers["If-Range"] = etag;
        }
        publish("downloading",
                {{"version", release.version.text()}, {"total", size}, {"received", received}});
        try {
            auto response = get(
                settings.api + "/releases/assets/" + field(p, "asset_id"), size, cancel,
                settings.network, headers,
                [&](const char *bytes, std::size_t count) {
                    cancel.check();
                    if (received + count > size)
                        fail("update.download_size");
                    out.write(bytes, static_cast<std::streamsize>(count));
                    if (!out)
                        fail("update.file_write", 31);
                    received += count;
                    auto time = std::chrono::steady_clock::now();
                    if (time - last > std::chrono::milliseconds(100)) {
                        last = time;
                        publish("downloading", {{"received", received},
                                                {"total", size},
                                                {"version", release.version.text()}});
                    }
                    if (time - checkpoint > std::chrono::seconds(5)) {
                        checkpoint = time;
                        saveReceipt();
                    }
                },
                [&](const HttpResponse &head) {
                    if (head.status == 206) {
                        auto expected = "bytes " + std::to_string(offset) + "-";
                        if (!offset || head.etag != etag || !head.range.starts_with(expected) ||
                            !head.range.ends_with("/" + std::to_string(size)))
                            fail("update.invalid_range");
                        auto end =
                            head.range.substr(expected.size(), head.range.size() - expected.size() -
                                                                   std::to_string(size).size() - 1);
                        if (end != std::to_string(size - 1))
                            fail("update.invalid_range");
                        out.open(part, std::ios::binary | std::ios::app);
                    } else {
                        offset = received = 0;
                        etag = head.etag;
                        out.open(part, std::ios::binary | std::ios::trunc);
                    }
                    if (!out)
                        fail("update.file_write", 31);
                    saveReceipt();
                });
            if (response.status == 416) {
                if (!fs::exists(part) || fs::file_size(part) != size ||
                    fileHash(part) != field(p, "sha256")) {
                    removeFile(part);
                    removeFile(receiptPath);
                    fail("update.invalid_range");
                }
            } else {
                saveReceipt();
                out.close();
            }
            if (!fs::exists(part) || fs::file_size(part) != size ||
                fileHash(part) != field(p, "sha256")) {
                removeFile(part);
                removeFile(receiptPath);
                fail("update.hash_mismatch");
            }
            if (fs::exists(zip))
                removeFile(zip);
            fs::rename(part, zip);
            if (fs::exists(receiptPath))
                removeFile(receiptPath);
        } catch (const Failure &e) {
            try {
                saveReceipt();
            } catch (...) {
            }
            out.close();
            throw;
        }
    }
    if(settings.checkpoint)settings.checkpoint("before_verify");
    cancel.check();
    publish("verifying");
    if (fs::file_size(zip) != size || fileHash(zip) != field(p, "sha256")) {
        removeFile(zip);
        fail("update.hash_mismatch");
    }
    stage(release, zip);
} catch(const Failure& error) {
    // The same cancellation/backoff policy applies to cached verification and
    // extraction, not only an in-progress HTTP download.
    try {
        if(error.exitCode==50 || error.exitCode==10 || error.exitCode==11){
            FileLock state(settings.root/"update-state/locks/state",true);
            auto preferences=optionalJson(settings.root/"update-state/preferences.json");
            if(error.exitCode==50){preferences["cancelled_version"]=release.version.text();preferences["cancelled_until"]=now()+86400;}
            else {preferences["not_before"]=std::max(now()+3600,error.retryAtUtcSeconds);if(error.exitCode==11)preferences["rate_limit_until"]=preferences["not_before"];}
            jsonWrite(settings.root/"update-state/preferences.json",preferences);
        }
    }catch(...){} // Preserve the original failure if recording preferences also fails.
    throw;
}
void Engine::cancelReady() {
    FileLock lock(settings.root / "update-state/locks/prepare", true);
    FileLock gate(settings.root / "update-state/locks/launch", true);
    auto ready = optionalJson(settings.root / "update-state/prepared-update.json");
    if (ready.empty())
        return;
    auto active = current();
    auto relative = field(ready, "payload_dir");
    if (relative == field(active, "current"))
        fail("update.instance_busy", 30);
    {
        FileLock state(settings.root / "update-state/locks/state", true);
        auto p = optionalJson(settings.root / "update-state/preferences.json");
        p["cancelled_version"] = field(ready, "version");
        p["cancelled_until"] = now() + 86400;
        jsonWrite(settings.root / "update-state/preferences.json", p);
    }
    removeFile(settings.root / "update-state/prepared-update.json");
    publish("idle");
}
namespace {
std::wstring userSecurity() {
    Handle token;
    HANDLE raw = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw))
        fail("update.ipc_failed", 31);
    token = Handle(raw);
    DWORD size = 0;
    GetTokenInformation(token.value, TokenUser, nullptr, 0, &size);
    Bytes buffer(size);
    if (!GetTokenInformation(token.value, TokenUser, buffer.data(), size, &size))
        fail("update.ipc_failed", 31);
    LPWSTR sid = nullptr;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(buffer.data())->User.Sid, &sid))
        fail("update.ipc_failed", 31);
    std::wstring s = L"D:P(A;;GA;;;";
    s += sid;
    s += L")";
    LocalFree(sid);
    return s;
}
struct PipeServer {
    Handle pipe;
    std::wstring name;
    PipeServer() {
        name = L"\\\\.\\pipe\\daymark-update-" + wide(randomHex(24));
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        auto sddl = userSecurity();
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1,
                                                                  &descriptor, nullptr))
            fail("update.ipc_failed", 31);
        SECURITY_ATTRIBUTES attributes{sizeof(attributes), descriptor, FALSE};
        pipe = Handle(CreateNamedPipeW(
            name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 65540,
            65540, 0, &attributes));
        LocalFree(descriptor);
        if (!pipe)
            fail("update.ipc_failed", 31);
    }
    void connect(HANDLE child) {
        OVERLAPPED over{};
        Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        over.hEvent = event.value;
        BOOL connected = ConnectNamedPipe(pipe.value, &over);
        if (!connected) {
            auto error = GetLastError();
            if (error == ERROR_PIPE_CONNECTED)
                return;
            if (error != ERROR_IO_PENDING)
                fail("update.ipc_failed", 40);
            HANDLE wait[]{event.value, child};
            auto result = WaitForMultipleObjects(2, wait, FALSE, 30000);
            if (result != WAIT_OBJECT_0) {
                CancelIoEx(pipe.value, &over);
                DWORD bytes = 0;
                GetOverlappedResult(pipe.value, &over, &bytes, TRUE);
                fail("update.health_timeout", 40);
            }
            DWORD bytes = 0;
            if (!GetOverlappedResult(pipe.value, &over, &bytes, FALSE))
                fail("update.ipc_failed", 40);
        }
    }
};
Child spawnHandoff(const fs::path &exe, std::vector<std::wstring> args, const Json &context,
                   Handle &mapping) {
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, TRUE};
    mapping = Handle(
        CreateFileMappingW(INVALID_HANDLE_VALUE, &attributes, PAGE_READWRITE, 0, 16384, nullptr));
    if (!mapping)
        fail("update.ipc_failed", 31);
    auto bytes = context.dump();
    if (bytes.size() > 16000)
        fail("update.ipc_failed", 31);
    auto *memory = MapViewOfFile(mapping.value, FILE_MAP_WRITE, 0, 0, 16384);
    if (!memory)
        fail("update.ipc_failed", 31);
    std::memset(memory, 0, 16384);
    std::memcpy(memory, bytes.data(), bytes.size());
    UnmapViewOfFile(memory);
    args.push_back(L"--update-handoff");
    args.push_back(std::to_wstring(reinterpret_cast<std::uintptr_t>(mapping.value)));
    std::wstring command = quote(exe.native());
    for (const auto &arg : args)
        command += L" " + quote(arg);
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    Bytes storage(size);
    auto *list = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if (!InitializeProcThreadAttributeList(list, 1, 0, &size))
        fail("update.ipc_failed", 31);
    HANDLE inherit = mapping.value;
    if (!UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &inherit,
                                   sizeof(inherit), nullptr, nullptr)) {
        DeleteProcThreadAttributeList(list);
        fail("update.ipc_failed", 31);
    }
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.lpAttributeList = list;
    PROCESS_INFORMATION process{};
    auto ok = CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
                             EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW, nullptr,
                             exe.parent_path().c_str(), &startup.StartupInfo, &process);
    DeleteProcThreadAttributeList(list);
    if (!ok)
        fail("update.start_failed", 40);
    CloseHandle(process.hThread);
    return {Handle(process.hProcess), process.dwProcessId};
}
} // namespace
void Engine::cleanOld(const Json &input) {
    auto journal = input;
    auto active = parseJson(read(settings.root / "current.json"));
    if (journal.value("phase", std::string()) != "running" &&
        journal.value("phase", std::string()) != "cleaning" &&
        journal.value("phase", std::string()) != "cleaned")
        return;
    if (journal.value("phase", std::string()) == "cleaned")
        return;
    if (field(active, "current") != field(journal, "new"))
        fail("update.recovery_required", 41);
    const auto previous = field(journal, "old");
    if (previous == field(active, "current"))
        fail("update.recovery_required", 41);
    // Keep the current and immediately preceding toolchain. Older signed
    // program files are removable only if unchanged; open GUIs are deferred.
    const auto toolRoot=settings.root/"update-state/tools";
    if(fs::exists(toolRoot))for(const auto& directory:fs::directory_iterator(toolRoot)){
        try{
            noLinks(directory.path());if(!directory.is_directory())continue;
            const auto payload="versions/"+utf8(directory.path().filename().native());
            if(payload==previous || payload==field(active,"current"))continue;
            const auto seed=verifySignature(read(directory.path()/"bootstrap-release.json"),
                read(directory.path()/"bootstrap-release.json.sig",4096),settings.keys,"Daymark.BootstrapPayload.v1");
            const auto raw=read(directory.path()/"FILES.json",8*1024*1024);
            if(field(seed,"repository")!=Repository || field(seed,"payload_dir")!=payload || hex(sha256(raw))!=field(seed,"files_index_sha256"))continue;
            bool preserved=false;
            std::set<fs::path,std::greater<fs::path>> emptyDirectories;
            for(const auto& entry:fileIndex(raw))if(entry.path=="daymark.exe" || entry.path=="daymark-update-cli.exe" || entry.path=="daymark-updater.exe" || entry.path=="daymark-setup-helper.exe" || entry.path.starts_with("update-licenses/")){
                const auto path=inside(directory.path(),entry.path);
                if(!fs::exists(path))continue;
                if(fileHash(path)!=entry.sha256){preserved=true;continue;}
                std::error_code ec;fs::remove(path,ec);if(ec)preserved=true;
                for(auto parent=path.parent_path();parent!=directory.path();parent=parent.parent_path())emptyDirectories.insert(parent);
            }
            if(!preserved){
                for(const auto& path:emptyDirectories){std::error_code ignored;fs::remove(path,ignored);}
                for(const auto* name:{"FILES.json","bootstrap-release.json.sig","bootstrap-release.json"})removeFile(directory.path()/name);
                std::error_code ignored;fs::remove(directory.path(),ignored);
            }
        }catch(const std::exception&){logEvent(settings.root,{{"event","toolchain.cleanup_deferred"}});}
    }
    auto oldDescriptor = descriptor(previous, false, false);
    auto files = fileIndex(oldDescriptor.at("index_raw").get<std::string>());
    // Never roll back once deletion begins. Keep a new-version recovery pointer first.
    jsonWrite(settings.root / "update-state/current.last-good.json", active);
    journal["phase"] = "cleaning";
    journal["cleanup"] = "in_progress";
    jsonWrite(settings.root / "update-state/journal.json", journal);
    bool pending = false;
    std::set<fs::path, std::greater<fs::path>> directories;
    const auto prefix = previous + "/";
    for (const auto &item : files) {
        if (!item.path.starts_with(prefix))
            continue;
        try {
            auto path = inside(settings.root, item.path);
            if (!fs::exists(path))
                continue;
            if (fileHash(path) != item.sha256)
                continue;
            std::error_code error;
            fs::remove(path, error);
            if (error)
                pending = true;
            auto directory = path.parent_path();
            while (directory != settings.root && directory != settings.root / "versions") {
                directories.insert(directory);
                directory = directory.parent_path();
            }
        } catch (...) {
            pending = true;
        }
    }
    for (const auto &directory : directories) {
        std::error_code ec;
        fs::remove(directory, ec);
    }
    journal["cleanup"] = pending ? "pending" : "complete";
    journal["phase"] = pending ? "cleaning" : "cleaned";
    jsonWrite(settings.root / "update-state/journal.json", journal);
    if (!pending) {
        active["previous"] = nullptr;
        jsonWrite(settings.root / "current.json", active);
        jsonWrite(settings.root / "update-state/current.last-good.json", active);
    }
    publish("running",
            {{"cleanup", pending ? "pending" : "complete"}, {"version", field(active, "version")}});
}
void Engine::recover() {
    auto journal = optionalJson(settings.root / "update-state/journal.json");
    if (journal.empty())
        return;
    auto active = current();
    const auto phase = journal.value("phase", std::string());
    if (phase == "aborted" || phase == "cleaned")
        return;
    if (phase == "running" || phase == "cleaning") {
        FileLock prepareLock(settings.root / "update-state/locks/prepare", true);
        FileLock gate(settings.root / "update-state/locks/launch", true);
        cleanOld(journal);
        return;
    }
    if (active.value("transaction_id", std::string()) ==
        journal.value("transaction_id", std::string())) {
        auto receipt = optionalJson(settings.root / "update-state/running-receipt.json");
        if (receipt.value("transaction_id", std::string()) ==
                journal.value("transaction_id", std::string()) &&
            receipt.value("generation", std::uint64_t(0)) ==
                active.value("generation", std::uint64_t(0))) {
            Handle process(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                       static_cast<DWORD>(number(receipt, "pid", 0xffffffff))));
            if (process && WaitForSingleObject(process.value, 0) == WAIT_TIMEOUT &&
                processStamp(process.value) == field(receipt, "stamp")) {
                std::wstring image(32768, 0);
                DWORD length = static_cast<DWORD>(image.size());
                const auto expected =
                    inside(settings.root, field(active, "current") + "/daymark.exe")
                        .lexically_normal()
                        .native();
                if (QueryFullProcessImageNameW(process.value, 0, image.data(), &length) &&
                    CompareStringOrdinal(
                        fs::path(image.substr(0, length)).lexically_normal().c_str(), -1,
                        expected.c_str(), -1, TRUE) == CSTR_EQUAL) {
                    FileLock prepareLock(settings.root / "update-state/locks/prepare", true);
                    FileLock gate(settings.root / "update-state/locks/launch", true);
                    journal["phase"] = "running";
                    journal["cleanup"] = "eligible";
                    journal["new_pid"] = receipt["pid"];
                    journal["new_stamp"] = receipt["stamp"];
                    jsonWrite(settings.root / "update-state/journal.json", journal);
                    if (fs::exists(settings.root / "update-state/prepared-update.json"))
                        removeFile(settings.root / "update-state/prepared-update.json");
                    cleanOld(journal);
                    return;
                }
            }
        }
        publish("recovery_required", {{"error", "update.recovery_required"}});
        return;
    }
    // A precommit candidate never has write permission. Do not activate old until it has exited.
    if (journal.contains("new_pid")) {
        Handle candidate(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                     static_cast<DWORD>(number(journal, "new_pid", 0xffffffff))));
        if (candidate &&
            processStamp(candidate.value) == journal.value("new_stamp", std::string()) &&
            WaitForSingleObject(candidate.value, 5000) != WAIT_OBJECT_0)
            fail("update.instance_busy", 30);
    }
    descriptor(field(active, "current"), true);
    journal["phase"] = "aborted";
    jsonWrite(settings.root / "update-state/journal.json", journal);
    publish("rolled_back");
}
void Engine::apply(const std::vector<std::wstring> &arguments, DWORD oldPid, std::string oldStamp) {
    FileLock prepareLock(settings.root / "update-state/locks/prepare", true);
    auto old = current();
    FileLock gate(settings.root / "update-state/locks/launch", true);
    auto ready = optionalJson(settings.root / "update-state/prepared-update.json");
    if (ready.empty())
        fail("update.not_ready", 21);
    if (ready.at("expected_generation") != old.at("generation"))
        fail("update.recovery_required", 41);
    const auto relative = field(ready, "payload_dir");
    auto desc = descriptor(relative, true);
    auto saved = inside(settings.root, "update-state/releases/" + std::string(relative.substr(9)));
    auto release = trusted(read(saved / "manifest.json"), read(saved / "manifest.json.sig", 4096));
    checkCompatibility(release, Version::parse(field(old, "version")), field(old, "runtime_family"),
                       osBuild());
    if (hex(sha256(release.raw)) != field(ready, "manifest_sha256"))
        fail("update.hash_mismatch");
    publish("waiting_for_exit");
    if (oldPid) {
        Handle process(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, oldPid));
        if (process) {
            if (processStamp(process.value) != oldStamp)
                fail("update.process_identity", 30);
            if (WaitForSingleObject(process.value, 0) == WAIT_TIMEOUT) {
                std::wstring image(32768, 0);
                DWORD n = static_cast<DWORD>(image.size());
                const bool queried = QueryFullProcessImageNameW(process.value, 0, image.data(), &n);
                if (!queried) {
                    if (WaitForSingleObject(process.value, 0) != WAIT_OBJECT_0)
                        fail("update.process_identity", 30);
                } else {
                    auto actual = fs::path(image.substr(0, n)).lexically_normal().native();
                    auto expected = inside(settings.root, field(old, "current") + "/daymark.exe")
                                        .lexically_normal()
                                        .native();
                    if (CompareStringOrdinal(actual.c_str(), -1, expected.c_str(), -1, TRUE) !=
                        CSTR_EQUAL)
                        fail("update.process_identity", 30);
                }
                if (WaitForSingleObject(process.value, 30000) != WAIT_OBJECT_0)
                    fail("update.exit_timeout", 30);
            }
        }
    }
    // All old-version processes must have released their shared runtime lease.
    {
        FileLock exclusive(settings.root / "update-state/locks/runtime", true);
    }
    Json journal{{"format", 1},
                 {"phase", "starting"},
                 {"worker_pid", GetCurrentProcessId()},
                 {"worker_stamp", processStamp(GetCurrentProcess())},
                 {"transaction_id", field(ready, "transaction_id")},
                 {"old", field(old, "current")},
                 {"new", relative},
                 {"cleanup", "not_eligible"}};
    Child child;
    bool committed = false;
    try {
        jsonWrite(settings.root / "update-state/journal.json", journal);
        publish("starting");
        if (settings.checkpoint)
            settings.checkpoint("before_spawn");
        PipeServer server;
        Handle mapping;
        const auto nonce = randomHex(32);
        const auto generation = number(old, "generation", 1000000000) + 1;
        auto context = Json{{"protocol", 1},
                            {"root", utf8(settings.root.native())},
                            {"pipe", utf8(server.name)},
                            {"nonce", nonce},
                            {"transaction_id", field(ready, "transaction_id")},
                            {"generation", generation},
                            {"worker_pid", GetCurrentProcessId()},
                            {"worker_stamp", processStamp(GetCurrentProcess())},
                            {"version", release.version.text()},
                            {"payload_dir", relative}};
        child = spawnHandoff(inside(settings.root, relative + "/daymark.exe"), arguments, context,
                             mapping);
        journal["new_pid"] = child.pid;
        journal["new_stamp"] = processStamp(child.process.value);
        jsonWrite(settings.root / "update-state/journal.json", journal);
        server.connect(child.process.value);
        ULONG peer = 0;
        if (!GetNamedPipeClientProcessId(server.pipe.value, &peer) || peer != child.pid)
            fail("update.process_identity", 40);
        auto hello = parseJson(ipc::receive(server.pipe.value));
        if (field(hello, "type") != "HELLO" || field(hello, "nonce") != nonce ||
            field(hello, "version") != release.version.text())
            fail("update.process_identity", 40);
        ipc::send(server.pipe.value, Json{{"type", "ACCEPT"}, {"nonce", nonce}}.dump());
        auto health = parseJson(ipc::receive(server.pipe.value, 90000));
        if (field(health, "type") != "READY" || field(health, "nonce") != nonce)
            fail("update.health_timeout", 40);
        if (settings.checkpoint)
            settings.checkpoint("after_ready");
        Json next = old;
        next["generation"] = generation;
        next["current"] = relative;
        next["previous"] = old["current"];
        next["version"] = release.version.text();
        next["transaction_id"] = ready["transaction_id"];
        jsonWrite(settings.root / "update-state/current.last-good.json", old);
        jsonWrite(settings.root / "current.json", next);
        committed = true;
        journal["phase"] = "committed";
        jsonWrite(settings.root / "update-state/journal.json", journal);
        if (settings.checkpoint)
            settings.checkpoint("after_commit");
        ipc::send(server.pipe.value,
                  Json{{"type", "COMMIT"}, {"nonce", nonce}, {"generation", generation}}.dump());
        auto running = parseJson(ipc::receive(server.pipe.value, 90000));
        if (field(running, "type") != "RUNNING" || field(running, "nonce") != nonce ||
            WaitForSingleObject(child.process.value, 0) != WAIT_TIMEOUT)
            fail("update.start_failed", 40);
        journal["phase"] = "running";
        journal["cleanup"] = "eligible";
        jsonWrite(settings.root / "update-state/journal.json", journal);
        removeFile(settings.root / "update-state/prepared-update.json");
        cleanOld(journal);
    } catch (...) {
        if (!committed) {
            if (child.process && WaitForSingleObject(child.process.value, 0) == WAIT_TIMEOUT) {
                TerminateProcess(child.process.value, 40);
                WaitForSingleObject(child.process.value, 5000);
            }
            try {
                journal["phase"] = "aborted";
                jsonWrite(settings.root / "update-state/journal.json", journal);
                removeFile(settings.root / "update-state/prepared-update.json");
            } catch (...) {
            } // A storage failure must not suppress the old-program recovery.
            try {
                FileLock state(settings.root / "update-state/locks/state", true);
                auto preferences = optionalJson(settings.root / "update-state/preferences.json");
                preferences["cancelled_version"] = release.version.text();
                preferences["cancelled_until"] = now() + 86400;
                jsonWrite(settings.root / "update-state/preferences.json", preferences);
            } catch (...) {
            }
            try {
                publish("rolled_back");
            } catch (...) {
            }
            gate.release();
            try {
                spawn(inside(settings.root, field(old, "current") + "/daymark.exe"), arguments,
                      false);
            } catch (...) {
            }
        } else {
            try {
                publish("recovery_required", {{"error", "update.recovery_required"}});
            } catch (...) {
            }
        }
        throw;
    }
}
void Engine::launch(const std::vector<std::wstring> &arguments) {
    current();
    recover();
    if (fs::exists(settings.root / "update-state/prepared-update.json")) {
        auto j = optionalJson(settings.root / "update-state/journal.json");
        if (j.value("phase", std::string()) != "committed") {
            apply(arguments);
            return;
        }
    }
    FileLock gate(settings.root / "update-state/locks/launch", true, 10000);
    auto active = current();
    spawn(inside(settings.root, field(active, "current") + "/daymark.exe"), arguments, false);
}
} // namespace daymark::update::win
