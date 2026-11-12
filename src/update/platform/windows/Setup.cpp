#include "Setup.h"
#include "SetupDiagnosticCodes.h"
#include <algorithm>
#include <cwctype>
#include <map>
#include <memory>
#include <set>
#include <shlobj.h>

namespace daymark::update::win {
std::string setupDiagnosticCode(std::string_view key) {
  std::string_view fallback = "C999";
  for (const auto &[name, code] : BuildSetupCodes) {
    if (name == key)
      return "S000-" + std::string(code);
    if (name == "setup.update_dependency")
      fallback = code;
  }
  return "S000-" + std::string(fallback);
}

namespace {
std::wstring normalized(const fs::path &path) {
  auto value = fs::absolute(path).lexically_normal().native();
  for (auto &ch : value)
    ch = std::towlower(ch);
  return value;
}
bool beneath(const fs::path &value, const fs::path &root) {
  auto a = normalized(value), b = normalized(root);
  return a == b || a.starts_with(b + L"\\");
}
void location(const fs::path &root) {
  noLinks(root);
  if (root == root.root_path() || root.filename().empty())
    fail("setup.target_unsafe", 31);
  if (root.native().starts_with(L"\\\\"))
    fail("setup.target_unsafe", 31);
  wchar_t volume[MAX_PATH]{}, filesystem[MAX_PATH]{};
  if (!GetVolumePathNameW(root.c_str(), volume, MAX_PATH) ||
      !GetVolumeInformationW(volume, nullptr, 0, nullptr, nullptr, nullptr,
                             filesystem, MAX_PATH) ||
      std::wstring_view(filesystem) != L"NTFS")
    fail("setup.target_filesystem", 31);
  wchar_t windows[32768]{};
  if (!GetWindowsDirectoryW(windows, 32768) || beneath(root, windows))
    fail("setup.target_unsafe", 31);
  for (int folder :
       {CSIDL_PROFILE, CSIDL_DESKTOPDIRECTORY, CSIDL_APPDATA,
        CSIDL_LOCAL_APPDATA, CSIDL_PROGRAM_FILES, CSIDL_PROGRAM_FILESX86,
        CSIDL_COMMON_APPDATA, CSIDL_PROGRAMS, CSIDL_STARTMENU}) {
    wchar_t value[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, folder, nullptr, SHGFP_TYPE_CURRENT,
                                   value)) &&
        normalized(root) == normalized(value))
      fail("setup.target_unsafe", 31);
  }
}
Settings at(Settings value, const fs::path &root) {
  value.root = root;
  return value;
}
void erase(const fs::path &path) {
  noLinks(path);
  std::error_code error;
  fs::remove(path, error);
  if (error)
    fail("setup.file_remove", 31);
}
void renameDirectory(const fs::path &from, const fs::path &to) {
  noLinks(from);
  noLinks(to);
  if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_WRITE_THROUGH)) {
    Failure error("setup.directory_commit", 31);
    error.platformError = GetLastError();
    throw error;
  }
}
std::vector<FileEntry> verifiedBundle(const Settings &settings,
                                      const fs::path &root) {
  Cancellation cancellation;
  Engine engine(at(settings, root), cancellation);
  auto entries = engine.ownedFiles();
  std::set<std::string> allowed;
  for (const auto &entry : entries) {
    auto path = inside(root, entry.path);
    if (!fs::is_regular_file(path) || fs::file_size(path) != entry.size ||
        fileHash(path) != entry.sha256)
      fail("update.hash_mismatch", 20);
    allowed.insert(entry.path);
  }
  for (const auto &item : fs::recursive_directory_iterator(root)) {
    noLinks(item.path());
    if (item.is_regular_file() &&
        !allowed.contains(
            utf8(fs::relative(item.path(), root).generic_wstring())))
      fail("setup.extra_bundle_file", 20);
  }
  return entries;
}
void removeEmpty(const fs::path &root) {
  std::vector<fs::path> paths;
  if (!fs::exists(root))
    return;
  for (auto it = fs::recursive_directory_iterator(
           root, fs::directory_options::skip_permission_denied);
       it != fs::recursive_directory_iterator(); ++it) {
    if (GetFileAttributesW(it->path().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) {
      it.disable_recursion_pending();
      continue;
    }
    if (it->is_directory())
      paths.push_back(it->path());
  }
  std::sort(paths.rbegin(), paths.rend());
  for (const auto &path : paths) {
    std::error_code ignored;
    fs::remove(path, ignored);
  }
  std::error_code ignored;
  fs::remove(root, ignored);
}
Json removeOwned(const Settings &settings, const fs::path &root) {
  Cancellation cancellation;
  Engine engine(at(settings, root), cancellation);
  const auto entries = engine.ownedFiles();
  std::uint64_t removed = 0, preserved = 0;
  std::set<std::string> processed;
  const auto receipt = root / "update-state/install-receipt.json";
  std::string uninstallerHash;
  if (fs::exists(receipt)) {
    uninstallerHash =
        parseJson(read(receipt)).value("uninstaller_sha256", std::string());
  }
  // Program files first; bootstrap/index metadata remain available until the
  // end.
  for (const auto &entry : entries) {
    if (entry.path == "bootstrap-release.json" ||
        entry.path == "bootstrap-release.json.sig" ||
        entry.path == "FILES.json")
      continue;
    if (!processed.insert(entry.path).second)
      continue;
    const auto path = inside(root, entry.path);
    if (!fs::exists(path))
      continue;
    if (fs::is_regular_file(path) && fs::file_size(path) == entry.size &&
        fileHash(path) == entry.sha256) {
      erase(path);
      ++removed;
    } else
      ++preserved;
  }
  // The updater owns these exact metadata names. Never recursively delete the
  // root.
  const auto uninstaller = root / "Uninstall.exe";
  if (fs::is_regular_file(uninstaller)) {
    if (uninstallerHash == fileHash(uninstaller)) {
      erase(uninstaller);
      ++removed;
    } else
      ++preserved;
  }
  // Once programs are removed, metadata cleanup is best effort. In-use metadata
  // cannot turn a successful uninstall into an unrecoverable partial retry.
  for (const auto *name :
       {"bootstrap-release.json", "bootstrap-release.json.sig", "FILES.json",
        "current.json", "update-state/current.last-good.json",
        "update-state/status.json", "update-state/preferences.json",
        "update-state/journal.json", "update-state/running-receipt.json",
        "update-state/prepared-update.json",
        "update-state/install-receipt.json"}) {
    try {
      const auto path = inside(root, name);
      if (fs::is_regular_file(path))
        erase(path);
    } catch (const Failure &) {
      ++preserved;
    }
  }
  return Json{{"removed", removed}, {"preserved", preserved}};
}
fs::path previous(const fs::path &root, const Json &receipt) {
  const auto name = field(receipt, "previous", 512);
  const auto prefix = utf8(root.filename().native()) + ".previous-";
  if (name.size() != prefix.size() + 32 || !name.starts_with(prefix) ||
      !std::all_of(name.begin() + prefix.size(), name.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
      }))
    fail("setup.receipt_invalid", 41);
  return root.parent_path() / wide(name);
}
fs::path transaction(const fs::path &root) {
  return root.parent_path() /
         wide(".daymark-install-" +
              hex(sha256(utf8(normalized(root)))).substr(0, 24) + ".json");
}
fs::path setupLockPath(const fs::path &root) {
  return root.parent_path() /
         (L"." + root.filename().native() + L".daymark-setup.lock");
}
void recover(const Settings &settings, const fs::path &root) {
  const auto journal = transaction(root);
  if (!fs::exists(journal))
    return;
  const auto value = parseJson(read(journal));
  const auto backup = previous(root, value);
  if (!fs::exists(root) && fs::exists(backup)) {
    Cancellation cancel;
    Engine old(at(settings, backup), cancel);
    old.ownedFiles();
    renameDirectory(backup, root);
  }
  if (!fs::exists(root))
    fail("setup.recovery_required", 41);
  Cancellation cancel;
  Engine installed(at(settings, root), cancel);
  installed.ownedFiles();
  erase(journal);
}
} // namespace
Json SetupManager::verifyBundle(const fs::path &source) {
  const auto entries =
      verifiedBundle(settings, fs::absolute(source).lexically_normal());
  return {{"format", 1}, {"result", "verified"}, {"files", entries.size()}};
}
Json SetupManager::checkIdle(const fs::path &target) {
  const auto root = fs::absolute(target).lexically_normal();
  location(root);
  std::vector<std::unique_ptr<FileLock>> leases;
  if (fs::exists(root / "bootstrap-release.json")) {
    for (const auto *name : {"job", "prepare", "launch", "runtime"})
      leases.push_back(
          std::make_unique<FileLock>(root / "update-state/locks" / name, true));
  }
  return {{"format", 1}, {"result", "idle"}};
}
Json SetupManager::install(const fs::path &input, const fs::path &output) {
  const auto source = fs::absolute(input).lexically_normal(),
             root = fs::absolute(output).lexically_normal();
  location(root);
  if (beneath(source, root) || beneath(root, source))
    fail("setup.source_target_overlap", 31);
  const auto entries = verifiedBundle(settings, source);
  fs::create_directories(root.parent_path());
  noLinks(root.parent_path());
  FileLock setupLock(setupLockPath(root), true);
  recover(settings, root);
  const bool existing = fs::exists(root) && !fs::is_empty(root);
  std::vector<std::unique_ptr<FileLock>> leases;
  Json oldPreferences = Json::object();
  Json ancestors = Json::array();
  if (existing) {
    if (!fs::exists(root / "bootstrap-release.json"))
      fail("setup.target_not_managed", 31);
    Cancellation c;
    Engine old(at(settings, root), c);
    old.ownedFiles();
    const auto oldSeed =
        verifySignature(read(root / "bootstrap-release.json"),
                        read(root / "bootstrap-release.json.sig", 4096),
                        settings.keys, "Daymark.BootstrapPayload.v1");
    const auto newSeed =
        verifySignature(read(source / "bootstrap-release.json"),
                        read(source / "bootstrap-release.json.sig", 4096),
                        settings.keys, "Daymark.BootstrapPayload.v1");
    auto oldVersion = Version::parse(field(oldSeed, "version"));
    if (fs::exists(root / "current.json"))
      oldVersion = old.installedVersion();
    if (Version::parse(field(newSeed, "version")) < oldVersion)
      fail("setup.downgrade", 21);
    for (const auto *name : {"job", "prepare", "launch", "runtime"})
      leases.push_back(
          std::make_unique<FileLock>(root / "update-state/locks" / name, true));
    if (fs::exists(root / "update-state/preferences.json"))
      oldPreferences = parseJson(read(root / "update-state/preferences.json"));
    if (fs::exists(root / "update-state/install-receipt.json")) {
      auto prior = parseJson(read(root / "update-state/install-receipt.json"));
      if (!prior.value("previous", std::string()).empty()) {
        previous(root, prior);
        ancestors.push_back(prior["previous"]);
      }
      if (prior.contains("ancestors"))
        for (const auto &name : prior.at("ancestors")) {
          previous(root, Json{{"previous", name}});
          ancestors.push_back(name);
        }
      if (ancestors.size() > 32)
        fail("setup.cleanup_required", 31);
    }
  }
  std::uint64_t bytes = 0;
  for (const auto &entry : entries)
    bytes += entry.size;
  if (fs::space(root.parent_path()).available < bytes * 2 + 16 * 1024 * 1024)
    fail("setup.disk_space", 31);
  const auto stage = root.parent_path() / wide(utf8(root.filename().native()) +
                                               ".install-" + randomHex());
  const auto backup = root.parent_path() / wide(utf8(root.filename().native()) +
                                                ".previous-" + randomHex());
  fs::create_directory(stage);
  bool movedOld = false, committed = false;
  try {
    for (const auto &entry : entries) {
      const auto target = inside(stage, entry.path);
      fs::create_directories(target.parent_path());
      fs::copy_file(inside(source, entry.path), target);
    }
    verifiedBundle(settings, stage);
    Cancellation c;
    Engine ready(at(settings, stage), c);
    const auto current = ready.current();
    if (oldPreferences.contains("auto_update") &&
        oldPreferences["auto_update"].is_boolean())
      writeAtomic(stage / "update-state/preferences.json",
                  Json{{"auto_update", oldPreferences["auto_update"]}}.dump());
    writeAtomic(
        stage / "update-state/install-receipt.json",
        Json{{"format", 1},
             {"version", current["version"]},
             {"previous", existing ? utf8(backup.filename().native()) : ""},
             {"ancestors", ancestors},
             {"phase", "installed"}}
            .dump());
    if (existing) {
      writeAtomic(
          transaction(root),
          Json{{"format", 1}, {"previous", utf8(backup.filename().native())}}
              .dump());
      // Windows denies a parent directory rename while descendant lease handles
      // are open. The sibling installation gate remains exclusive across
      // commit.
      leases.clear();
      renameDirectory(root, backup);
      movedOld = true;
      if (settings.checkpoint)
        settings.checkpoint("setup_old_moved");
    } else if (fs::exists(root))
      erase(root);
    renameDirectory(stage, root);
    committed = true;
    if (fs::exists(transaction(root)))
      erase(transaction(root));
    return Json{{"format", 1},
                {"result", "installed"},
                {"version", current["version"]},
                {"previous_retained", existing}};
  } catch (...) {
    if (movedOld && !committed && !fs::exists(root))
      renameDirectory(backup, root);
    // Keep a fully staged signed directory as a recoverable failure receipt.
    throw;
  }
}
Json SetupManager::remove(const fs::path &input) {
  const auto root = fs::absolute(input).lexically_normal();
  location(root);
  FileLock setupLock(setupLockPath(root), true);
  recover(settings, root);
  Cancellation cancellation;
  Engine managed(at(settings, root), cancellation);
  managed.ownedFiles();
  Json result;
  {
    FileLock job(root / "update-state/locks/job", true),
        prepare(root / "update-state/locks/prepare", true),
        gate(root / "update-state/locks/launch", true),
        runtime(root / "update-state/locks/runtime", true);
    result = removeOwned(settings, root);
  }
  for (const auto *name : {"job", "prepare", "launch", "runtime", "state"}) {
    const auto path = root / "update-state/locks" / name;
    if (fs::is_regular_file(path) && fs::file_size(path) == 0) {
      std::error_code ignored;
      fs::remove(path, ignored);
    }
  }
  removeEmpty(root);
  result["format"] = 1;
  result["result"] = "uninstalled";
  return result;
}
Json SetupManager::finish(const fs::path &input) {
  const auto root = fs::absolute(input).lexically_normal();
  location(root);
  FileLock setupLock(setupLockPath(root), false);
  const auto file = root / "update-state/install-receipt.json";
  if (!fs::exists(file))
    return {{"result", "no_install_cleanup"}};
  auto receipt = parseJson(read(file));
  if (receipt.value("previous", std::string()).empty())
    return {{"result", "no_install_cleanup"}};
  Cancellation cancel;
  Engine installed(at(settings, root), cancel);
  const auto current = installed.current(),
             running =
                 parseJson(read(root / "update-state/running-receipt.json"));
  if (running.value("transaction_id", std::string()) !=
          current.value("transaction_id", std::string()) ||
      running.at("generation") != current.at("generation"))
    fail("setup.not_running", 30);
  Handle process(
      OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                  static_cast<DWORD>(number(running, "pid", 0xffffffff))));
  if (!process || WaitForSingleObject(process.value, 0) != WAIT_TIMEOUT ||
      processStamp(process.value) != field(running, "stamp"))
    fail("setup.not_running", 30);
  wchar_t image[32768]{};
  DWORD length = 32768;
  if (!QueryFullProcessImageNameW(process.value, 0, image, &length) ||
      normalized(image) !=
          normalized(inside(root, field(current, "current") + "/daymark.exe")))
    fail("setup.not_running", 30);
  Json pending = receipt.value("ancestors", Json::array());
  pending.push_back(field(receipt, "previous"));
  if (pending.size() > 33)
    fail("setup.receipt_invalid", 41);
  for (const auto &name : pending)
    previous(root, Json{{"previous", name}});
  Json result;
  for (const auto &name : pending) {
    const auto backup = previous(root, Json{{"previous", name}});
    // A previous completed removal can leave unknown user files. Never delete
    // them.
    if (fs::exists(backup / "bootstrap-release.json"))
      result = remove(backup);
  }
  receipt["previous"] = "";
  receipt["ancestors"] = Json::array();
  receipt["phase"] = "running";
  writeAtomic(file, receipt.dump());
  result["result"] = "install_cleanup_complete";
  return result;
}
Json SetupManager::sealUninstaller(const fs::path &input) {
  const auto root = fs::absolute(input).lexically_normal();
  location(root);
  FileLock lock(setupLockPath(root), true);
  Cancellation cancel;
  Engine installed(at(settings, root), cancel);
  installed.ownedFiles();
  const auto file = root / "update-state/install-receipt.json";
  auto receipt = parseJson(read(file));
  receipt["uninstaller_sha256"] = fileHash(inside(root, "Uninstall.exe"));
  writeAtomic(file, receipt.dump());
  return {{"format", 1}, {"result", "uninstaller_registered"}};
}
} // namespace daymark::update::win
