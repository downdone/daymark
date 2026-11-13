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
    # Termination is scoped to verified images in this installation, not names.
    from flow_fixture import wait_for
    parallel=w/'parallel installation'
    run([helper,'install','--root',parallel,'--source',new,'--fixture-trust',trust])
    current_image=root/'versions/2.0.1/daymark.exe'
    peer=subprocess.Popen([str(parallel/'versions/2.0.1/daymark.exe')],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    application=subprocess.Popen([str(current_image)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    try:
        wait_for(lambda:(root/f'fixture-running-{application.pid}.json').exists())
        wait_for(lambda:(parallel/f'fixture-running-{peer.pid}.json').exists())
        setup('confirm-close-running',code=50,extra=('--fixture-confirm','no'))
        assert application.poll() is None and peer.poll() is None
        # Exercise the actual native confirmation and its default Cancel route.
        dialog=subprocess.Popen([str(helper),'confirm-close-running','--root',str(root),'--fixture-trust',str(trust)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=subprocess.CREATE_NO_WINDOW)
        u=ctypes.WinDLL('user32',use_last_error=True)
        u.GetWindowThreadProcessId.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.DWORD)]
        u.PostMessageW.argtypes=[wintypes.HWND,wintypes.UINT,wintypes.WPARAM,wintypes.LPARAM]
        u.GetWindowRect.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.RECT)]
        u.IsWindowVisible.argtypes=[wintypes.HWND]
        u.GetClassNameW.argtypes=[wintypes.HWND,wintypes.LPWSTR,ctypes.c_int]
        u.GetDlgItem.argtypes=[wintypes.HWND,ctypes.c_int];u.GetDlgItem.restype=wintypes.HWND
        callback=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
        def own_dialog():
            found=[]
            @callback
            def visit(hwnd,_):
                owner=wintypes.DWORD();u.GetWindowThreadProcessId(hwnd,ctypes.byref(owner))
                name=ctypes.create_unicode_buffer(128);u.GetClassNameW(hwnd,name,128)
                rect=wintypes.RECT()
                if owner.value==dialog.pid and name.value=='#32770' and u.IsWindowVisible(hwnd) and u.GetWindowRect(hwnd,ctypes.byref(rect)) and rect.right>rect.left and rect.bottom>rect.top:found.append(hwnd)
                return True
            u.EnumWindows(visit,0)
            return found[0] if found else None
        try:
            hwnd=wait_for(own_dialog,5)
            import os
            proof=os.environ.get('DAYMARK_SETUP_UI_PROOF')
            if proof:
                from PIL import Image
                import struct
                u.SetThreadDpiAwarenessContext.argtypes=[wintypes.HANDLE];u.SetThreadDpiAwarenessContext.restype=wintypes.HANDLE
                previous_dpi=u.SetThreadDpiAwarenessContext(ctypes.c_void_p(-4))
                rect=wintypes.RECT();assert u.GetWindowRect(hwnd,ctypes.byref(rect))
                width,height=rect.right-rect.left,rect.bottom-rect.top
                g=ctypes.WinDLL('gdi32',use_last_error=True)
                g.CreateCompatibleDC.argtypes=[wintypes.HDC];g.CreateCompatibleDC.restype=wintypes.HDC
                g.CreateDIBSection.argtypes=[wintypes.HDC,ctypes.c_void_p,wintypes.UINT,ctypes.POINTER(ctypes.c_void_p),wintypes.HANDLE,wintypes.DWORD];g.CreateDIBSection.restype=wintypes.HANDLE
                g.SelectObject.argtypes=[wintypes.HDC,wintypes.HANDLE];g.SelectObject.restype=wintypes.HANDLE
                g.DeleteObject.argtypes=[wintypes.HANDLE];g.DeleteDC.argtypes=[wintypes.HDC]
                u.PrintWindow.argtypes=[wintypes.HWND,wintypes.HDC,wintypes.UINT]
                dc=g.CreateCompatibleDC(None);assert dc
                bitmap=None;previous=None
                try:
                    info=ctypes.create_string_buffer(struct.pack('<IiiHHIIiiII',40,width,-height,1,32,0,width*height*4,0,0,0,0))
                    pixels=ctypes.c_void_p()
                    bitmap=g.CreateDIBSection(dc,info,0,ctypes.byref(pixels),None,0);assert bitmap and pixels.value
                    previous=g.SelectObject(dc,bitmap)
                    assert u.PrintWindow(hwnd,dc,2)
                    Image.frombytes('RGB',(width,height),ctypes.string_at(pixels,width*height*4),'raw','BGRX').save(proof)
                finally:
                    if previous:g.SelectObject(dc,previous)
                    if bitmap:g.DeleteObject(bitmap)
                    g.DeleteDC(dc)
                    if previous_dpi:u.SetThreadDpiAwarenessContext(previous_dpi)
            assert u.PostMessageW(hwnd,0x111,2,0)
            output,_=dialog.communicate(timeout=5)
            assert dialog.returncode==50,output
        finally:
            if dialog.poll() is None:dialog.kill();dialog.wait()
        assert application.poll() is None and peer.poll() is None
        job=k.CreateFileW(str(root/'update-state/locks/job'),0xC0000000,7,None,4,0x80,None)
        assert job!=wintypes.HANDLE(-1).value
        held=Overlapped();assert k.LockFileEx(job,0,0,1,0,ctypes.byref(held))
        try:setup('confirm-close-running',code=30,extra=('--fixture-confirm','yes'));assert application.poll() is None
        finally:k.CloseHandle(job)
        result=json.loads(setup('confirm-close-running',extra=('--fixture-confirm','yes')))
        assert result['closed']==1
        application.wait(timeout=5);assert peer.poll() is None
        setup('install',new)
        assert json.loads((root/'current.json').read_text())['version']=='2.0.1'
        print('PASS: real native Cancel, confirmed exact-install force close, other installation preserved, active transaction refusal and repair',flush=True)
    finally:
        for child in (application,peer):
            if child.poll() is None:child.kill();child.wait()
    foreign=w/'foreign';foreign.mkdir();(foreign/'keep.txt').write_text('untouched')
    _winapi.CreateJunction(str(foreign),str(root/'foreign-link'))
    (root/'keep.db').write_text('keep');(root/'versions/2.0.1/daymark.exe').write_bytes(b'user modified')
    setup('uninstall');assert (root/'keep.db').read_text()=='keep';assert (root/'versions/2.0.1/daymark.exe').read_bytes()==b'user modified'
    assert (foreign/'keep.txt').read_text()=='untouched';(root/'foreign-link').rmdir()
    print('PASS: signed first install, repair, upgrade, rollback/crash recovery, downgrade, tamper/extra rejection, live lease, nonmanaged refusal and preserved files')
if __name__=='__main__':main()
