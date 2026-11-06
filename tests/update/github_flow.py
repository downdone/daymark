"""Opt-in actual public GitHub -> production Qt update flow, isolated synthetic data.

No credentials, fixture trust, API overrides or test payloads are accepted.
The signed managed bootstrap ZIP must be an existing public GitHub asset.
"""
from __future__ import annotations
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import shutil
import subprocess
import sys
import time
import urllib.request
import zipfile

from flow_fixture import stop_owned, process_exited


def stop_remaining_owned(root):
    """Reap only test-root images even if startup failed before registration."""
    class Entry(ctypes.Structure):
        _fields_ = [('size',wintypes.DWORD),('usage',wintypes.DWORD),('pid',wintypes.DWORD),
                    ('heap',ctypes.c_size_t),('module',wintypes.DWORD),('threads',wintypes.DWORD),
                    ('parent',wintypes.DWORD),('priority',wintypes.LONG),('flags',wintypes.DWORD),
                    ('name',wintypes.WCHAR*260)]
    kernel = ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD,wintypes.DWORD]
    kernel.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    kernel.Process32FirstW.argtypes = kernel.Process32NextW.argtypes = [wintypes.HANDLE,ctypes.POINTER(Entry)]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    snapshot = kernel.CreateToolhelp32Snapshot(2,0)
    if snapshot == ctypes.c_void_p(-1).value:
        return
    try:
        entry = Entry();entry.size = ctypes.sizeof(entry)
        more = kernel.Process32FirstW(snapshot,ctypes.byref(entry))
        while more:
            if entry.name.lower().startswith('daymark'):
                stop_owned(root,entry.pid)  # Independently checks the actual image path.
            more = kernel.Process32NextW(snapshot,ctypes.byref(entry))
    finally:
        kernel.CloseHandle(snapshot)


def audit_qt_runtime(pid, payload):
    """Observe real production modules; a parent Qt SDK must not satisfy them."""
    import re
    kernel = ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD,wintypes.BOOL,wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.K32EnumProcessModulesEx.argtypes = [wintypes.HANDLE,ctypes.POINTER(wintypes.HMODULE),wintypes.DWORD,ctypes.POINTER(wintypes.DWORD),wintypes.DWORD]
    kernel.K32GetModuleFileNameExW.argtypes = [wintypes.HANDLE,wintypes.HMODULE,wintypes.LPWSTR,wintypes.DWORD]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    process = kernel.OpenProcess(0x410,False,int(pid))
    if not process:raise AssertionError('Unable to audit production Qt process modules')
    try:
        modules = (wintypes.HMODULE*2048)();needed = wintypes.DWORD()
        if not kernel.K32EnumProcessModulesEx(process,modules,ctypes.sizeof(modules),ctypes.byref(needed),3) or needed.value>ctypes.sizeof(modules):
            raise AssertionError('Production module inventory unavailable or incomplete')
        observed = set()
        for module in modules[:needed.value//ctypes.sizeof(wintypes.HMODULE)]:
            name = ctypes.create_unicode_buffer(32768)
            if not kernel.K32GetModuleFileNameExW(process,module,name,len(name)):
                # Optional modules may unload between the snapshot and lookup.
                # The required Qt/SQL/CRT set below must still be observed.
                continue
            path = Path(name.value).resolve()
            if re.fullmatch(r'(Qt6.*|qwindows|qsqlite|msvcp140.*|vcruntime140.*|concrt140|vccorlib140)\.dll',path.name,re.I):
                if payload.resolve() not in path.parents:
                    raise AssertionError('Production runtime resolved outside signed payload: '+path.name)
                observed.add(path.name.lower())
        if not {'qt6core.dll','qwindows.dll','qsqlite.dll','msvcp140.dll','vcruntime140.dll'}.issubset(observed):
            raise AssertionError('Expected production Qt/SQL/CRT modules were not observed')
        return sorted(observed)
    finally:kernel.CloseHandle(process)


def run(command, timeout=40, environment=None):
    result = subprocess.run(list(map(str, command)), capture_output=True, text=True,
                            encoding='utf-8', timeout=timeout,env=environment)
    if result.returncode:
        raise AssertionError(f'{Path(command[0]).name} {command[1]} failed ({result.returncode}): {result.stdout[-1200:]} {result.stderr[-600:]}')
    return result.stdout


def wait_for(predicate, seconds, phase, root=None):
    deadline = time.monotonic()+seconds
    last = None
    while time.monotonic() < deadline:
        if root:
            path = root/'update-state/status.json'
            if path.exists():
                try:
                    value = json.loads(path.read_text())
                    state = value.get('state')
                    if state != last:
                        print('UPDATE', phase, state, value.get('error', ''), flush=True)
                        last = state
                    if state == 'error':
                        raise AssertionError('Production updater failed: '+json.dumps(value))
                except (OSError, ValueError):
                    pass
        try:
            value = predicate()
            if value:
                return value
        except (OSError, ValueError, KeyError):
            pass
        time.sleep(.1)
    raise AssertionError('Bounded verification deadline exceeded: '+phase)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--old-version', default='2.0.1')
    parser.add_argument('--new-version', default='2.0.2')
    parser.add_argument('--wait-for-target-seconds',type=int,default=0,
                        help='Wait for public target assets before launching Qt; never changes updater state')
    parser.add_argument('--export-artifacts',type=Path,
                        help='New report directory for public ZIPs and verification evidence; never copies the synthetic database')
    args = parser.parse_args()
    import re
    if any(not re.fullmatch(r'(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)', version)
           for version in (args.old_version,args.new_version)):
        raise ValueError('Two stable semantic versions are required')
    if not 0 <= args.wait_for_target_seconds <= 3600:
        raise ValueError('Target publication wait must be bounded to 0..3600 seconds')
    args.work = args.work.resolve()
    if args.work.exists():
        raise ValueError('Evidence directory already exists; no overwrite')
    args.work.mkdir(parents=True)
    runtime_environment = dict(os.environ)
    for key in ('QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QML_IMPORT_PATH','QML2_IMPORT_PATH','QT_CONF_PATH',
                'GH_TOKEN','GITHUB_TOKEN','ACTIONS_RUNTIME_TOKEN','ACTIONS_ID_TOKEN_REQUEST_TOKEN'):
        runtime_environment.pop(key,None)
    windows = Path(os.environ['SystemRoot'])
    runtime_environment['PATH'] = str(windows/'System32')+';'+str(windows)
    runtime_environment['QT_QPA_PLATFORM'] = 'windows'
    if args.export_artifacts:
        args.export_artifacts = args.export_artifacts.resolve()
        if args.export_artifacts.exists() or args.work == args.export_artifacts or args.work in args.export_artifacts.parents:
            raise ValueError('A new artifact directory outside the temporary installation is required')
        args.export_artifacts.mkdir(parents=True)
    started = time.monotonic()
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    def public_json(url):
        request = urllib.request.Request(url, headers={'User-Agent':'Daymark-public-update-verification',
                'Accept':'application/vnd.github+json', 'X-GitHub-Api-Version':'2026-03-10'})
        with opener.open(request, timeout=30) as response:
            return json.load(response)
    release = public_json('https://api.github.com/repos/downdone/daymark/releases/tags/v'+args.old_version)
    if release['draft'] or release['prerelease']:
        raise ValueError('Stable public bootstrap release required')
    name = f'Daymark-{args.old_version}-windows-x64.zip'
    assets = public_json(f"https://api.github.com/repos/downdone/daymark/releases/{release['id']}/assets?per_page=100")
    matching = [a for a in assets if a['name'] == name]
    if len(matching) != 1:
        raise ValueError('Unique immutable public bootstrap asset required')
    asset = matching[0]
    if not asset['digest'].startswith('sha256:') or asset['size'] > 512*1024**2:
        raise ValueError('Invalid public asset identity')
    archive = args.work/name
    request = urllib.request.Request(asset['url']+'?daymark_verify='+str(int(time.time())),
              headers={'User-Agent':'Daymark-public-update-verification','Accept':'application/octet-stream'})
    print('Downloading actual anonymous GitHub bootstrap asset', asset['id'], flush=True)
    download_started = time.monotonic()
    progress_at = download_started
    with opener.open(request, timeout=90) as response, archive.open('xb') as output:
        while chunk := response.read(1024*1024):
            output.write(chunk)
            if output.tell() > asset['size']:
                raise ValueError('Public bootstrap exceeds declared size')
            if time.monotonic()-progress_at >= 30:
                print('BOOTSTRAP',output.tell(),'/',asset['size'],'bytes',flush=True)
                progress_at = time.monotonic()
    with archive.open('rb') as source:
        actual = hashlib.file_digest(source,'sha256').hexdigest()
    if archive.stat().st_size != asset['size'] or 'sha256:'+actual != asset['digest']:
        raise ValueError('Anonymous bootstrap integrity mismatch')
    if args.export_artifacts:
        shutil.copy2(archive,args.export_artifacts/archive.name)
    download_seconds = time.monotonic()-download_started
    root = args.work/f'Daymark-{args.old_version}-windows-x64'
    with zipfile.ZipFile(archive) as source:
        seen = set()
        unpacked = 0
        for entry in source.infolist():
            path = Path(entry.filename)
            if (not path.parts or path.parts[0] != root.name or path.is_absolute()
                    or any(part in {'.','..'} or ':' in part or '\\' in part for part in path.parts)
                    or (entry.external_attr >> 16) & 0o170000 == 0o120000
                    or entry.filename.lower() in seen):
                raise ValueError('Unsafe bootstrap archive entry')
            seen.add(entry.filename.lower())
            unpacked += entry.file_size
            if unpacked > 2*1024**3 or len(seen) > 10000:
                raise ValueError('Bootstrap archive exceeds updater limits')
        source.extractall(args.work)
    if (root/'update-fixture.json').exists():
        raise ValueError('A fixture must never be used for public GitHub verification')
    repository = Path(__file__).resolve().parents[2]
    sys.path.insert(0,str(repository/'scripts'))
    from update_release_gates import verify_executables
    pin = json.loads((repository/'config/authenticode-policy.json').read_text())['certificate_sha256']
    payloads = list((root/'versions').glob('*/daymark.exe'))
    if len(payloads) != 1:
        raise ValueError('A single signed bootstrap payload is required')
    verify_executables([payloads[0], root/'daymark.exe', root/'daymark-update-cli.exe', root/'daymark-updater.exe'],pin)
    if (root/'current.json').exists() or (root/'update-state').exists():
        raise ValueError('Public bootstrap must be freshly extracted with no pre-initialized state')
    if json.loads((root/'bootstrap-release.json').read_text())['version'] != args.old_version:
        raise ValueError('Bootstrap version mismatch')
    target_wait_started = time.monotonic()
    required_assets = {f'Daymark-{args.new_version}-windows-x64.zip','update-windows-x64.json','update-windows-x64.json.sig'}
    while True:
        target = public_json('https://api.github.com/repos/downdone/daymark/releases/tags/v'+args.new_version)
        if (not target['draft'] and not target['prerelease'] and target['tag_name'] == 'v'+args.new_version
                and required_assets.issubset({a['name'] for a in target['assets'] if a['state']=='uploaded'})):
            break
        if time.monotonic()-target_wait_started >= args.wait_for_target_seconds:
            raise AssertionError('Target signed assets are not yet public; Qt was not launched')
        print('Waiting for complete signed public target assets; updater state untouched',flush=True)
        time.sleep(30)
    target_wait_seconds = time.monotonic()-target_wait_started
    qt_started = time.monotonic()
    data = args.work/'synthetic-data'
    processes = []
    try:
        run([root/'daymark.exe','--data-dir',data],environment=runtime_environment)
        def instance(version):
            for path in (root/'update-state/instances').glob('*.json'):
                value = json.loads(path.read_text())
                if value.get('version') == version and not process_exited(value['pid']):
                    return value
        old = wait_for(lambda:instance(args.old_version),40,'old Qt registration',root)
        processes.append(old['pid'])
        current = json.loads((root/'current.json').read_text())
        assert current['version'] == args.old_version and current['generation'] == 1
        def schema_ready():
            if not (data/'daymark.sqlite').exists(): return False
            with sqlite3.connect(data/'daymark.sqlite') as db:
                return db.execute('PRAGMA user_version').fetchone()[0] == 10
        wait_for(schema_ready,30,'synthetic database creation',root)
        old_modules = audit_qt_runtime(old['pid'],root/current['current'])
        with sqlite3.connect(data/'daymark.sqlite') as db:
            db.execute('INSERT INTO tasks(id,title,created_at_utc,updated_at_utc) VALUES(?,?,?,?)',
                ('github-update-synthetic-task','Public GitHub update preservation check','2026-11-06T00:00:00.000Z','2026-11-06T00:00:00.000Z'))
        print('Old production Qt running with synthetic SQLite; awaiting automatic public GitHub prepare',flush=True)
        wait_for(lambda:(root/'update-state/prepared-update.json').exists(),1800,'public download/verify/prepare',root)
        ready = json.loads((root/'update-state/prepared-update.json').read_text())
        old_directory = root/current['current']
        old_exe = old_directory/'daymark.exe'
        assert old_exe.exists()
        assert json.loads((root/'current.json').read_text())['version'] == args.old_version
        assert (root/ready['payload_dir']/'daymark.exe').exists()
        if args.export_artifacts:
            cache_key = Path(ready['payload_dir']).name
            cached_archive = root/'update-state/downloads'/cache_key/'package.zip'
            metadata = json.loads((root/'update-state/releases'/cache_key/'manifest.json').read_text())
            with cached_archive.open('rb') as source:
                assert hashlib.file_digest(source,'sha256').hexdigest() == metadata['package']['sha256']
            shutil.copy2(cached_archive,args.export_artifacts/metadata['package']['name'])
        unknown = old_directory/'personal-note.txt'; unknown.write_text('Synthetic unknown user file',encoding='utf-8')
        modified = old_directory/'README.txt'
        if not modified.exists():
            raise AssertionError('Expected indexed README.txt for modified-file preservation test')
        modified.write_text('Synthetic modified indexed file',encoding='utf-8')
        response = run([root/'daymark-update-cli.exe','apply','--root',root,'--data-dir',data,'--json'],environment=runtime_environment)
        assert 'restart_requested' in response
        wait_for(lambda:process_exited(old['pid']),40,'old Qt saved/exited',root)
        wait_for(lambda:json.loads((root/'update-state/journal.json').read_text()).get('phase') == 'cleaned',90,'new Qt commit/RUNNING/cleanup',root)
        journal = json.loads((root/'update-state/journal.json').read_text())
        processes.append(journal['new_pid'])
        current = json.loads((root/'current.json').read_text())
        assert current['version'] == args.new_version and current['previous'] is None
        assert not old_exe.exists() and (root/current['current']/'daymark.exe').exists()
        assert unknown.read_text(encoding='utf-8') == 'Synthetic unknown user file'
        assert modified.read_text(encoding='utf-8') == 'Synthetic modified indexed file'
        assert not (root/'update-state/prepared-update.json').exists()
        assert instance(args.new_version)
        new_modules = audit_qt_runtime(journal['new_pid'],root/current['current'])
        accepted_manifest = json.loads((root/'update-state/releases'/Path(current['current']).name/'manifest.json').read_text())
        with sqlite3.connect(data/'daymark.sqlite') as db:
            assert db.execute('PRAGMA quick_check').fetchone()[0] == 'ok'
            assert db.execute('PRAGMA user_version').fetchone()[0] == 10
            assert db.execute("SELECT title FROM tasks WHERE id='github-update-synthetic-task'").fetchone()[0] == 'Public GitHub update preservation check'
        result = dict(passed=True,repository='downdone/daymark',oldVersion=args.old_version,newVersion=args.new_version,
                      bootstrapReleaseId=release['id'],bootstrapAssetId=asset['id'],bootstrapSha256=actual,
                      targetReleaseId=accepted_manifest['release_id'],targetAssetId=accepted_manifest['package']['asset_id'],
                      targetSourceRevision=accepted_manifest['source_revision'],
                      oldQtModules=old_modules,newQtModules=new_modules,productionEnvironmentIsolated=True,
                      automaticPrepare=True,productionCompiledTrust=True,anonymousGitHub=True,fixtureOverrides=False,
                      coldRootLaunch=True,
                      qtSaveQuitRestart=True,durableCommitAndRunning=True,oldExecutableDeleted=True,
                      unknownAndModifiedFilesPreserved=True,syntheticSqlitePreserved=True,seconds=round(time.monotonic()-started,3))
        result.update(bootstrapDownloadSeconds=round(download_seconds,3),
                      targetPublicationWaitSeconds=round(target_wait_seconds,3),
                      qtUpdateSeconds=round(time.monotonic()-qt_started,3))
        (args.work/'report.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        if args.export_artifacts:
            (args.export_artifacts/'report.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
            for name in ('current.json','update-state/journal.json','update-state/running-receipt.json'):
                path = root/name
                if path.exists():shutil.copy2(path,args.export_artifacts/Path(name).name)
        print('PASS: public GitHub Qt update flow',flush=True)
        print(json.dumps(result),flush=True)
    finally:
        for name,field in [('update-state/journal.json','worker_pid'),('update-state/status.json','owner_pid')]:
            try:
                pid = json.loads((root/name).read_text()).get(field)
                if pid:stop_owned(root,pid)
            except (OSError,ValueError):pass
        for path in (root/'update-state/instances').glob('*.json'):
            try:processes.append(json.loads(path.read_text())['pid'])
            except (OSError,ValueError):pass
        for pid in set(processes):stop_owned(root,pid)
        stop_remaining_owned(root)


if __name__ == '__main__':main()
