"""Signed installer transactions. Called with a QTemporaryDir-owned root by Qt Test."""
import argparse, json, shutil, subprocess
from pathlib import Path

def run(args, code=0):
    p=subprocess.run([str(x) for x in args],capture_output=True,text=True,timeout=30)
    assert p.returncode==code,(p.returncode,p.stdout,p.stderr)
    return p.stdout

def main():
    a=argparse.ArgumentParser();a.add_argument('--bin',type=Path,required=True);a.add_argument('--work',type=Path,required=True);a=a.parse_args()
    cli=a.bin/'daymark-update-cli.exe';helper=a.bin/'test_setup_helper.exe';w=a.work
    run([cli,'keygen',w/'keys']);trust=w/'keys/public.json'
    def bundle(version,kind):
        root=w/('bundle-'+version);payload=root/'versions'/version;payload.mkdir(parents=True)
        shutil.copy2(a.bin/('test_update_payload_'+kind+'.exe'),payload/'daymark.exe')
        # Native fixtures deliberately lack Qt/deployment; this is never a distributable installer.
        import hashlib
        files=[{'path':p.relative_to(root).as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'size':p.stat().st_size} for p in root.rglob('*') if p.is_file()]
        (root/'FILES.json').write_text(json.dumps({'format':1,'files':files}))
        seed={'format':1,'repository':'downdone/daymark','version':version,'source_revision':'a'*40,'runtime_family':'mingw131','payload_dir':'versions/'+version,'entry':'daymark.exe','files_index_sha256':hashlib.sha256((root/'FILES.json').read_bytes()).hexdigest()}
        (root/'bootstrap-release.json').write_text(json.dumps(seed));run([cli,'sign',w/'keys/private.json',root/'bootstrap-release.json','Daymark.BootstrapPayload.v1'])
        return root
    old=bundle('2.0.0','old');new=bundle('2.0.1','new');root=w/'installed'
    def setup(command,src=None,code=0,extra=()):
        args=[helper,command,'--root',root,'--fixture-trust',trust]
        if src:args+=['--source',src]
        return run(args+list(extra),code)
    setup('install',old);assert json.loads((root/'current.json').read_text())['version']=='2.0.0'
    setup('install',old) # same-version repair
    (root/'my-tasks.db').write_bytes(b'private synthetic database')
    run([helper,'install','--root',root,'--source',new,'--fixture-trust',trust,'--fixture-failure','throw'],40)
    assert (root/'my-tasks.db').read_bytes()==b'private synthetic database'
    # A hard crash in the only missing-root gap is recovered before the next attempt.
    setup('install',new,91,('--fixture-failure','crash'));assert not root.exists()
    setup('install',new);assert json.loads((root/'current.json').read_text())['version']=='2.0.1'
    setup('install',old,21)
    installed=root/'versions/2.0.1/daymark.exe';installed.write_bytes(b'corrupted installed executable')
    setup('install',new);assert installed.read_bytes()==(new/'versions/2.0.1/daymark.exe').read_bytes()
    # Unknown/nonmanaged targets, mutated source and added files are rejected before commit.
    other=w/'unmanaged';other.mkdir();(other/'keep.db').write_text('keep')
    run([helper,'install','--root',other,'--source',new,'--fixture-trust',trust],31);assert (other/'keep.db').read_text()=='keep'
    import _winapi
    junction=w/'bundle-junction';_winapi.CreateJunction(str(new),str(junction))
    setup('install',junction,20);junction.rmdir()
    (new/'extra.db').write_text('private');setup('install',new,20);(new/'extra.db').unlink()
    executable=new/'versions/2.0.1/daymark.exe';data=executable.read_bytes();executable.write_bytes(b'tampered');setup('install',new,20);executable.write_bytes(data)
    # A live runtime byte-range lease prevents repair/removal, including in another process.
    import ctypes
    from ctypes import wintypes
    k=ctypes.WinDLL('kernel32',use_last_error=True);k.CreateFileW.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p,wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE];k.CreateFileW.restype=wintypes.HANDLE
    class Overlapped(ctypes.Structure):_fields_=[('Internal',ctypes.c_size_t),('InternalHigh',ctypes.c_size_t),('Offset',wintypes.DWORD),('OffsetHigh',wintypes.DWORD),('hEvent',wintypes.HANDLE)]
    k.LockFileEx.argtypes=[wintypes.HANDLE,wintypes.DWORD,wintypes.DWORD,wintypes.DWORD,wintypes.DWORD,ctypes.POINTER(Overlapped)];k.CloseHandle.argtypes=[wintypes.HANDLE]
    h=k.CreateFileW(str(root/'update-state/locks/runtime'),0xC0000000,7,None,4,0x80,None);assert h!=wintypes.HANDLE(-1).value
    ov=Overlapped();assert k.LockFileEx(h,0,0,1,0,ctypes.byref(ov))
    try:setup('check-idle',code=30);setup('install',new,30);setup('uninstall',code=30)
    finally:k.CloseHandle(h)
    setup('check-idle')
    foreign=w/'foreign';foreign.mkdir();(foreign/'keep.txt').write_text('untouched')
    _winapi.CreateJunction(str(foreign),str(root/'foreign-link'))
    (root/'keep.db').write_text('keep');(root/'versions/2.0.1/daymark.exe').write_bytes(b'user modified')
    setup('uninstall');assert (root/'keep.db').read_text()=='keep';assert (root/'versions/2.0.1/daymark.exe').read_bytes()==b'user modified'
    assert (foreign/'keep.txt').read_text()=='untouched';(root/'foreign-link').rmdir()
    print('PASS: signed first install, repair, upgrade, rollback/crash recovery, downgrade, tamper/extra rejection, live lease, nonmanaged refusal and preserved files')
if __name__=='__main__':main()
