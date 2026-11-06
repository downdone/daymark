"""Real signed NSIS -> Qt app -> repair -> cleanup -> uninstall, QTemporaryDir-owned."""
import argparse, hashlib, json, subprocess, time, winreg
from pathlib import Path
from flow_fixture import wait_for, stop_owned
KEY=r'Software\Microsoft\Windows\CurrentVersion\Uninstall\Daymark'
def read_registry():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER,KEY) as k:return winreg.QueryValueEx(k,'InstallLocation')[0]
    except FileNotFoundError:return None

def main():
    p=argparse.ArgumentParser();p.add_argument('--artifact',type=Path,required=True);p.add_argument('--work',type=Path,required=True);p.add_argument('--repo',type=Path,required=True);a=p.parse_args()
    root=a.work/'安装目录 with spaces';profile=a.work/'isolated-profile';integration=read_registry() is None
    def install(expected=0):
        # NSIS requires /D as an unquoted, final command-line tail.
        command='"'+str(a.artifact.resolve())+'" /S '+('' if integration else '/NOINTEGRATION ')+'/D='+str(root)
        result=subprocess.run(command,cwd=a.work,timeout=90)
        assert (result.returncode==0)==(expected==0),('installer exit',result.returncode)
    def snapshot():return {f.relative_to(profile).as_posix():hashlib.sha256(f.read_bytes()).hexdigest() for f in profile.rglob('*') if f.is_file()}
    def launch():
        subprocess.run([str(root/'daymark-update-cli.exe'),'auto','--root',str(root),'--enabled','false'],check=True,capture_output=True,timeout=10)
        receipt=root/'update-state/running-receipt.json'
        if receipt.exists():receipt.unlink()
        child=subprocess.Popen([str(root/'daymark.exe'),'--data-dir',str(profile)],cwd=a.work)
        try:
            child.wait(timeout=30);assert child.returncode==0,'bootstrap failed'
            record=wait_for(lambda:json.loads(receipt.read_text()) if receipt.exists() else None,30)
            wait_for(lambda:list(profile.rglob('*.sqlite*')) or list(profile.rglob('*.db')),20)
            return record['pid']
        except BaseException:
            if receipt.exists():stop_owned(root,json.loads(receipt.read_text()).get('pid',0))
            raise
    install();assert (root/'daymark.exe').is_file();assert (root/'Uninstall.exe').is_file()
    if integration:assert Path(read_registry())==root
    # Check installed uninstaller Authenticode against the project's pin, without trusting a root.
    pin=json.loads((a.repo/'config/authenticode-policy.json').read_text())['certificate_sha256']
    subprocess.run(['pwsh','-NoProfile','-Command','Import-Module $args[0]; Assert-DaymarkSelfSignedExecutable $args[1] $args[2] | Out-Null',str(a.repo/'scripts/Authenticode.psm1'),str(root/'Uninstall.exe'),pin],check=True,timeout=30)
    pid=launch()
    try:install(1)
    finally:stop_owned(root,pid)
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
    finally:stop_owned(root,pid)
    before=snapshot();(root/'personal.db').write_bytes(b'synthetic uninstall preservation')
    subprocess.run([str(root/'Uninstall.exe'),'/S'],cwd=a.work,check=True,timeout=30)
    wait_for(lambda:not (root/'daymark.exe').exists(),30)
    if integration:wait_for(lambda:read_registry() is None,30)
    assert (root/'personal.db').read_bytes()==b'synthetic uninstall preservation';assert snapshot()==before
    print('PASS: real NSIS Unicode/space path, pinned setup/uninstaller, registration='+str(integration)+', real Qt RUNNING, busy refusal, two repairs, old-program cleanup, uninstall with data preserved')
if __name__=='__main__':main()
