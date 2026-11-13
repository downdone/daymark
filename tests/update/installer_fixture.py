"""Real signed NSIS -> Qt app -> repair -> cleanup -> uninstall, QTemporaryDir-owned."""
import argparse, hashlib, json, subprocess, time, winreg, os
from pathlib import Path
from flow_fixture import wait_for, stop_owned
KEY=r'Software\Microsoft\Windows\CurrentVersion\Uninstall\Daymark'
def read_registry():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER,KEY) as k:return winreg.QueryValueEx(k,'InstallLocation')[0]
    except FileNotFoundError:return None

def main():
    p=argparse.ArgumentParser();p.add_argument('--artifact',type=Path,required=True);p.add_argument('--work',type=Path,required=True);p.add_argument('--repo',type=Path,required=True);p.add_argument('--interactive-close',action='store_true');a=p.parse_args()
    # Interactive repair must never be redirected to a real registered install.
    if a.interactive_close:assert read_registry() is None,'interactive test requires no existing Daymark registration'
    root=a.work/'安装目录 with spaces';profile=a.work/'isolated-profile';integration=read_registry() is None and not a.interactive_close
    def install(expected=0):
        # NSIS requires /D as an unquoted, final command-line tail.
        command='"'+str(a.artifact.resolve())+'" /S '+('' if integration else '/NOINTEGRATION ')+'/D='+str(root)
        result=subprocess.run(command,cwd=a.work,timeout=90)
        assert (result.returncode==0)==(expected==0),('installer exit',result.returncode)
    def snapshot():return {f.relative_to(profile).as_posix():hashlib.sha256(f.read_bytes()).hexdigest() for f in profile.rglob('*') if f.is_file()}
    clean_env={k:v for k,v in os.environ.items() if not k.upper().startswith(('QT','QML','DAYMARK','SSL_CERT'))}
    clean_env['PATH']=os.environ.get('SystemRoot',r'C:\Windows')+r'\System32;'+os.environ.get('SystemRoot',r'C:\Windows')
    def audit_modules(pid):
        import ctypes
        from ctypes import wintypes
        kernel=ctypes.WinDLL('kernel32',use_last_error=True);api=ctypes.WinDLL('psapi',use_last_error=True)
        kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD];kernel.OpenProcess.restype=wintypes.HANDLE;kernel.CloseHandle.argtypes=[wintypes.HANDLE]
        handle=kernel.OpenProcess(0x410,False,pid);assert handle
        try:
            modules=(wintypes.HMODULE*2048)();needed=wintypes.DWORD()
            api.EnumProcessModulesEx.argtypes=[wintypes.HANDLE,ctypes.c_void_p,wintypes.DWORD,ctypes.POINTER(wintypes.DWORD),wintypes.DWORD]
            api.GetModuleFileNameExW.argtypes=[wintypes.HANDLE,wintypes.HMODULE,wintypes.LPWSTR,wintypes.DWORD]
            assert api.EnumProcessModulesEx(handle,modules,ctypes.sizeof(modules),ctypes.byref(needed),3)
            found=[]
            for module in modules[:needed.value//ctypes.sizeof(wintypes.HMODULE)]:
                name=ctypes.create_unicode_buffer(32768);assert api.GetModuleFileNameExW(handle,module,name,len(name))
                path=Path(name.value)
                if path.name.lower().startswith(('qt6','libgcc','libstdc++','libwinpthread','msvcp140','vcruntime140','concrt140')) or path.name.lower()=='qwindows.dll':
                    assert root.resolve() in path.resolve().parents,('external runtime dependency',path.name);found.append(path.name.lower())
            assert {'qt6core.dll','qt6quick.dll','qwindows.dll'}<=set(found)
            return len(found)
        finally:kernel.CloseHandle(handle)
    def launch():
        subprocess.run([str(root/'daymark-update-cli.exe'),'auto','--root',str(root),'--enabled','false'],check=True,capture_output=True,timeout=10)
        receipt=root/'update-state/running-receipt.json'
        if receipt.exists():receipt.unlink()
        child=subprocess.Popen([str(root/'daymark.exe'),'--data-dir',str(profile)],cwd=a.work,env=clean_env)
        try:
            child.wait(timeout=30);assert child.returncode==0,'bootstrap failed'
            record=wait_for(lambda:json.loads(receipt.read_text()) if receipt.exists() else None,30)
            wait_for(lambda:list(profile.rglob('*.sqlite*')) or list(profile.rglob('*.db')),20)
            assert audit_modules(record['pid'])>=3
            return record['pid']
        except BaseException:
            if receipt.exists():stop_owned(root,json.loads(receipt.read_text()).get('pid',0))
            raise
    install();assert (root/'daymark.exe').is_file();assert (root/'Uninstall.exe').is_file()
    if integration:assert Path(read_registry())==root
    # Check installed uninstaller Authenticode against the project's pin, without trusting a root.
    pin=json.loads((a.repo/'config/authenticode-policy.json').read_text())['certificate_sha256']
    subprocess.run(['pwsh','-NoProfile','-File',str(a.repo/'scripts/Verify-WindowsExecutable.ps1'),'-Executable',str(root/'Uninstall.exe'),'-CertificateSha256',pin],check=True,timeout=30)
    pid=launch()
    try:
        install(1)
        if a.interactive_close:
            import sqlite3
            from installer_ui_fixture import repair_with_running_app
            database=profile/'daymark.sqlite'
            with sqlite3.connect(database) as db:
                db.execute("INSERT INTO tasks(id,title,created_at_utc,updated_at_utc) VALUES(?,?,?,?)",('force-close-preservation','Synthetic force-close preservation','2026-11-13T00:00:00.000Z','2026-11-13T00:00:00.000Z'))
            current=(root/'current.json').read_bytes()
            for confirm in (False,True):
                repair_with_running_app(a.artifact,root,a.work,pid,confirm)
                assert (root/'current.json').read_bytes()==current
                with sqlite3.connect(database) as db:
                    assert db.execute('PRAGMA quick_check').fetchone()[0]=='ok'
                    assert db.execute("SELECT title FROM tasks WHERE id='force-close-preservation'").fetchone()[0]=='Synthetic force-close preservation'
    finally:stop_owned(root,pid)
    # Emulate an existing schema10 profile before manual reinstall. The real new
    # Qt app must migrate it after the signed installer has replaced the hosts.
    import sqlite3
    database=profile/'daymark.sqlite'
    with sqlite3.connect(database) as db:
        db.execute("PRAGMA wal_checkpoint(TRUNCATE)")
        db.execute("ALTER TABLE tasks DROP COLUMN purged_at_utc")
        db.execute("ALTER TABLE tasks DROP COLUMN local_removed_at_utc")
        db.execute("PRAGMA user_version=10")
        db.execute("INSERT INTO tasks(id,title,created_at_utc,updated_at_utc) VALUES(?,?,?,?)",('installer-migration','Synthetic migration preservation','2026-11-10T00:00:00.000Z','2026-11-10T00:00:00.000Z'))
    before=snapshot();assert before
    (root/'personal.db').write_bytes(b'synthetic file must survive')
    install();install();assert snapshot()==before
    pid=launch()
    try:
        receipt=root/'update-state/install-receipt.json'
        wait_for(lambda:json.loads(receipt.read_text()).get('phase')=='running',30)
        for backup in root.parent.glob(root.name+'.previous-*'):
            assert not list(backup.glob('versions/**/daymark.exe')),'old executable retained despite RUNNING'
        assert any((backup/'personal.db').read_bytes()==b'synthetic file must survive' for backup in root.parent.glob(root.name+'.previous-*') if (backup/'personal.db').exists())
        with sqlite3.connect(database) as db:
            assert db.execute('PRAGMA user_version').fetchone()[0]==11
            assert db.execute("SELECT title FROM tasks WHERE id='installer-migration'").fetchone()[0]=='Synthetic migration preservation'
        assert list((profile/'backups/migrations').glob('*.sqlite')),'migration backup missing'

    finally:stop_owned(root,pid)
    before=snapshot();(root/'personal.db').write_bytes(b'synthetic uninstall preservation')
    subprocess.run([str(root/'Uninstall.exe'),'/S'],cwd=a.work,check=True,timeout=30)
    wait_for(lambda:not (root/'daymark.exe').exists(),30)
    if integration:wait_for(lambda:read_registry() is None,30)
    assert (root/'personal.db').read_bytes()==b'synthetic uninstall preservation';assert snapshot()==before
    print('PASS: real NSIS Unicode/space path, pinned setup/uninstaller, registration='+str(integration)+', real Qt RUNNING, busy refusal, two repairs, schema10-to-11 migration/backup, old-program cleanup, uninstall with data preserved')
if __name__=='__main__':main()
