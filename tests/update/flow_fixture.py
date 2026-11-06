"""Owned, signed loopback release fixture. Work directory comes from QTemporaryDir."""
from __future__ import annotations
import argparse
import ctypes
from ctypes import wintypes
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import shutil
import subprocess
import threading
import time
import sys
import os
import zipfile


def wait_for(predicate, seconds=20):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        try:
            value=predicate()
            if value:
                return value
        except (OSError, ValueError, KeyError):
            pass
        time.sleep(.05)
    raise AssertionError('fixture state deadline exceeded')


def run(args, expected=0, timeout=45):
    result=subprocess.run([str(a) for a in args],capture_output=True,text=True,encoding='utf-8',timeout=timeout)
    if result.returncode!=expected:
        raise AssertionError(f'command {Path(args[0]).name} {args[1]} exit={result.returncode}; {result.stdout[-2000:]} {result.stderr[-500:]}')
    return result.stdout


def stop_owned(root, pid):
    """Only terminate a test child after verifying its image belongs to this fixture."""
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD]
    kernel.OpenProcess.restype=wintypes.HANDLE
    kernel.CloseHandle.argtypes=[wintypes.HANDLE]
    kernel.QueryFullProcessImageNameW.argtypes=[wintypes.HANDLE,wintypes.DWORD,wintypes.LPWSTR,ctypes.POINTER(wintypes.DWORD)]
    kernel.TerminateProcess.argtypes=[wintypes.HANDLE,wintypes.UINT]
    handle=kernel.OpenProcess(0x1000|1|0x100000,False,int(pid))
    if not handle:
        return
    try:
        buffer=ctypes.create_unicode_buffer(32768);length=wintypes.DWORD(32768)
        if kernel.QueryFullProcessImageNameW(handle,0,buffer,ctypes.byref(length)):
            image=Path(buffer.value).resolve()
            if root.resolve() in image.parents:
                kernel.TerminateProcess(handle,0)
                kernel.WaitForSingleObject.argtypes=[wintypes.HANDLE,wintypes.DWORD]
                kernel.WaitForSingleObject(handle,5000)
    finally:
        kernel.CloseHandle(handle)


def process_exited(pid):
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD];kernel.OpenProcess.restype=wintypes.HANDLE
    kernel.WaitForSingleObject.argtypes=[wintypes.HANDLE,wintypes.DWORD];kernel.CloseHandle.argtypes=[wintypes.HANDLE]
    handle=kernel.OpenProcess(0x100000,False,int(pid))
    if not handle:return True
    try:return kernel.WaitForSingleObject(handle,0)==0
    finally:kernel.CloseHandle(handle)


def main():
    parser=argparse.ArgumentParser()
    for name in ('work','bin','repo'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--old-payload',type=Path)
    parser.add_argument('--new-payload',type=Path)
    args=parser.parse_args();args.work.mkdir(parents=True,exist_ok=True)
    real=bool(args.old_payload)
    cli=args.bin/'daymark-update-cli.exe'
    runtime=json.loads(run([cli,'capabilities']))['runtime']
    fixture_cli=args.bin/'test_update_cli.exe'
    key=args.work/'signing'
    run([cli,'keygen',key])
    public=json.loads((key/'public.json').read_text())
    assets={}
    inventory=[]
    ranges=[]
    range_mode='resume'
    cached_requests=[]
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*unused):
            pass
        def do_GET(self):
            path=self.path.split('?')[0]
            if path=='/releases':
                body=json.dumps([{'id':123,'tag_name':'v2.0.1','draft':False,'prerelease':False}]).encode()
            elif path=='/releases/123/assets':
                body=json.dumps(inventory).encode()
            elif path.startswith('/releases/assets/'):
                file=assets.get(path.rsplit('/',1)[-1])
                if not file:
                    self.send_error(404);return
                body=file.read_bytes()
            else:
                self.send_error(404);return
            etag='"'+hashlib.sha256(body).hexdigest()+'"'
            if self.headers.get('If-None-Match')==etag:
                cached_requests.append(path);self.send_response(304);self.end_headers();return
            if self.headers.get('Range') and path.endswith('/1003'):
                offset=int(self.headers['Range'].removeprefix('bytes=').removesuffix('-'))
                assert self.headers.get('If-Range')==etag
                ranges.append((range_mode,offset))
                if range_mode=='resume':
                    total=len(body);body=body[offset:]
                    self.send_response(206);self.send_header('Content-Range',f'bytes {offset}-{total-1}/{total}')
                else:
                    self.send_response(200)
            else:
                self.send_response(200)
            self.send_header('Content-Length',str(len(body)));self.send_header('ETag',etag);self.end_headers();self.wfile.write(body)
    server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    api=f'http://127.0.0.1:{server.server_port}'
    processes=[]
    root=args.work/'installation'
    try:
        for version,payload_input,source in [('2.0.0',args.old_payload,'1'*40),('2.0.1',args.new_payload,'2'*40)]:
            payload=args.work/('payload-'+version)
            if payload_input:
                shutil.copytree(payload_input,payload)
            else:
                payload.mkdir();shutil.copy2(args.bin/('test_update_payload_old.exe' if version=='2.0.0' else 'test_update_payload_new.exe'),payload/'daymark.exe')
            (payload/'preserve-indexed.txt').write_text('original signed file',encoding='utf-8')
            command=[sys.executable,args.repo/'scripts/package-update.py','--payload',payload,'--launcher',args.bin/'test_update_bootstrap.exe','--updater',args.bin/'daymark-updater.exe','--cli',fixture_cli,'--key',key/'private.json','--output',args.work/('release-'+version),'--version',version,'--source',source,'--runtime',runtime,'--runtime-min','bundled','--min-windows-build','19045','--release-id','123','--asset-id','1003','--local-fixture']
            if version=='2.0.0':
                command+=['--extract',root]
            run(command,timeout=90)
        new=args.work/'release-2.0.1'
        for id_,name in [('1001','update-windows-x64.json'),('1002','update-windows-x64.json.sig'),('1003','Daymark-2.0.1-windows-x64.zip')]:
            path=new/name;assets[id_]=path
            inventory.append({'id':int(id_),'name':name,'size':path.stat().st_size,'state':'uploaded','digest':'sha256:'+hashlib.sha256(path.read_bytes()).hexdigest()})
        fixture_config=dict(public,api=api)
        if os.environ.get('DAYMARK_UPDATE_CRASH_POINT'):
            fixture_config['crash_at']=os.environ['DAYMARK_UPDATE_CRASH_POINT']
        (root/'update-fixture.json').write_text(json.dumps(fixture_config),encoding='utf-8')
        data=args.work/'synthetic-data'
        def seed_partial():
            manifest=json.loads((new/'update-windows-x64.json').read_text());package=manifest['package']
            cache=root/'update-state/downloads'/('2.0.1-'+'2'*12);cache.mkdir(parents=True,exist_ok=True)
            raw=assets['1003'].read_bytes()
            (cache/'package.zip').unlink(missing_ok=True)
            (cache/'package.zip.part').write_bytes(raw[:4096])
            (cache/'download.json').write_text(json.dumps({'asset_id':'1003','sha256':package['sha256'],'size':len(raw),'etag':'"'+hashlib.sha256(raw).hexdigest()+'"','received':4096}))
        if real:
            data.mkdir()
            run([root/'daymark.exe','--data-dir',data])
            def old_instance():
                for path in (root/'update-state/instances').glob('*.json'):
                    value=json.loads(path.read_text())
                    if value.get('version')=='2.0.0':
                        return value
            old_identity=wait_for(old_instance,30)
            old_pid=old_identity['pid'];processes.append(old_pid)
            current=json.loads((root/'current.json').read_text());old=root/current['current']/'daymark.exe'
            wait_for(lambda:(data/'daymark.sqlite').exists(),30)
            import sqlite3
            def schema_ready():
                with sqlite3.connect(data/'daymark.sqlite') as db:
                    return db.execute('PRAGMA user_version').fetchone()[0]==10
            wait_for(schema_ready,30)
            with sqlite3.connect(data/'daymark.sqlite') as db:
                db.execute("INSERT INTO tasks(id,title,created_at_utc,updated_at_utc) VALUES(?,?,?,?)",('update-flow-synthetic-task','Synthetic update preservation check','2026-11-05T00:00:00.000Z','2026-11-05T00:00:00.000Z'))
            # The real Qt application performs the automatic prepare after RuntimeReady.
            wait_for(lambda:(root/'update-state/prepared-update.json').exists(),90)
        else:
            run([root/'daymark-update-cli.exe','check','--root',root,'--json'])
            current=json.loads((root/'current.json').read_text());old=root/current['current']/'daymark.exe'
            old_process=subprocess.Popen([str(old)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);processes.append(old_process.pid)
            old_identity=wait_for(lambda:json.loads((root/f'fixture-running-{old_process.pid}.json').read_text()))
            manifest_path=assets['1001'];signature_path=assets['1002'];zip_path=assets['1003']
            baseline_manifest=manifest_path.read_bytes();baseline_signature=signature_path.read_bytes();baseline_zip=zip_path.read_bytes()
            def refresh_inventory():
                for item in inventory:
                    raw=assets[str(item['id'])].read_bytes();item['size']=len(raw);item['digest']='sha256:'+hashlib.sha256(raw).hexdigest()
            def rejected(expected,key_fragment):
                output=run([root/'daymark-update-cli.exe','prepare','--root',root,'--json'],expected=expected)
                assert key_fragment in output,output
                assert json.loads((root/'current.json').read_text())['current']==current['current']
                assert not (root/'update-state/prepared-update.json').exists()
                assert old.exists() and old_process.poll() is None
            manifest_path.write_bytes(baseline_manifest+b' ')
            refresh_inventory();rejected(20,'update.signature_invalid')
            manifest_path.write_bytes(baseline_manifest);signature_path.write_bytes(baseline_signature)
            for unsafe in ('../escape.exe','payload.dll:stream','CON.txt'):
                zip_path.write_bytes(baseline_zip)
                with zipfile.ZipFile(zip_path,'a') as archive:
                    archive.writestr('Daymark-2.0.1-windows-x64/'+unsafe,b'unsafe')
                altered=json.loads(baseline_manifest);package=altered['package'];package['sha256']=hashlib.sha256(zip_path.read_bytes()).hexdigest();package['size']=zip_path.stat().st_size;package['file_count']+=1;package['unpacked_size']+=6
                manifest_path.write_text(json.dumps(altered,separators=(',',':')),encoding='utf-8');run([cli,'sign',key/'private.json',manifest_path]);refresh_inventory()
                rejected(20,'update.archive_unsafe')
            zip_path.write_bytes(baseline_zip);manifest_path.write_bytes(baseline_manifest);signature_path.write_bytes(baseline_signature)
            future=json.loads(baseline_manifest);future['data']['schema']=11
            manifest_path.write_text(json.dumps(future),encoding='utf-8');run([cli,'sign',key/'private.json',manifest_path]);refresh_inventory();rejected(21,'update.data_compatibility')
            manifest_path.write_bytes(baseline_manifest);signature_path.write_bytes(baseline_signature);refresh_inventory()
            # Inventory changed during negative cases; prime the restored asset
            # ETag once before asserting its next conditional request returns304.
            run([root/'daymark-update-cli.exe','check','--root',root,'--json'])
            seed_partial()
            run([root/'daymark-update-cli.exe','prepare','--root',root,'--json'])
            assert ranges==[('resume',4096)],ranges
            assert '/releases' in cached_requests and '/releases/123/assets' in cached_requests,cached_requests
            run([root/'daymark-update-cli.exe','cancel-ready','--root',root,'--json'])
            assert not (root/'update-state/prepared-update.json').exists()
            assert old.exists()
            preferences=root/'update-state/preferences.json'
            state=json.loads(preferences.read_text());assert state['cancelled_until']>time.time()
            # Make scheduling eligible while retaining the actual 24h candidate
            # cancellation. Automatic discovery must defer; explicit retry may proceed.
            state['not_before']=0;state['last_attempt']=0;preferences.write_text(json.dumps(state))
            run([root/'daymark-update-cli.exe','prepare','--root',root,'--json','--auto'])
            assert not (root/'update-state/prepared-update.json').exists()
            assert json.loads((root/'update-state/status.json').read_text())['state']=='deferred'
            range_mode='full';seed_partial()
            run([root/'daymark-update-cli.exe','prepare','--root',root,'--json'])
            assert ranges[-1]==('full',4096),ranges
            # A user cancellation after download (also when using an existing
            # complete ZIP) gets the same policy as cancellation during transport.
            run([root/'daymark-update-cli.exe','cancel-ready','--root',root,'--json'])
            checkpoint=dict(fixture_config,crash_at='cancel_before_verify')
            (root/'update-fixture.json').write_text(json.dumps(checkpoint))
            output=run([root/'daymark-update-cli.exe','prepare','--root',root,'--json'],expected=50)
            assert 'update.cancelled' in output
            assert not (root/'update-state/prepared-update.json').exists()
            assert json.loads(preferences.read_text())['cancelled_until']>time.time()
            (root/'update-fixture.json').write_text(json.dumps(fixture_config))
            run([root/'daymark-update-cli.exe','prepare','--root',root,'--json'])
        assert old.exists(),'old binary must remain before restart'
        ready=json.loads((root/'update-state/prepared-update.json').read_text())
        assert json.loads((root/'current.json').read_text())['current']==current['current']
        assert (root/ready['payload_dir']/'daymark.exe').exists()
        unknown=old.parent/'personal-note.txt';unknown.write_text('user-owned unknown file',encoding='utf-8')
        modified=old.parent/'preserve-indexed.txt'
        if os.environ.get('DAYMARK_UPDATE_CRASH_POINT') not in ('after_ready','throw_before_spawn'):
            modified.write_text('user-modified signed file',encoding='utf-8')
        if real:
            # Public CLI requests the real Qt save/quit path; that app starts apply with its own process identity.
            output=run([root/'daymark-update-cli.exe','apply','--root',root,'--data-dir',data,'--json'])
            assert 'restart_requested' in output,output
            wait_for(lambda:not (root/f'update-state/instances/{old_pid}.json').exists(),30)
        else:
            apply=subprocess.Popen([str(root/'daymark-update-cli.exe'),'apply','--root',str(root),'--old-pid',str(old_process.pid),'--old-stamp',old_identity['stamp'],'--json'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            (root/f'fixture-exit-{old_process.pid}').touch()
            stdout,stderr=apply.communicate(timeout=45)
            assert apply.returncode==0,(stdout,stderr)
        if real and os.environ.get('DAYMARK_UPDATE_CRASH_POINT')=='throw_before_spawn':
            wait_for(lambda:json.loads((root/'update-state/journal.json').read_text()).get('phase')=='aborted',15)
            def restarted_old():
                value=old_instance()
                return value if value and value['pid']!=old_pid else None
            identity=wait_for(restarted_old,20);processes.append(identity['pid'])
            assert json.loads((root/'current.json').read_text())['current']==current['current']
            assert old.exists() and not (root/'update-state/prepared-update.json').exists()
            import sqlite3
            with sqlite3.connect(data/'daymark.sqlite') as db:
                assert db.execute('PRAGMA quick_check').fetchone()[0]=='ok'
                assert db.execute("SELECT title FROM tasks WHERE id='update-flow-synthetic-task'").fetchone()[0]=='Synthetic update preservation check'
            print('PASS: signed Windows flow; actual Qt pre-spawn exception -> protected rollback releases launch gate -> old application automatically resumes -> SQLite preserved',flush=True)
            return
        if real and os.environ.get('DAYMARK_UPDATE_CRASH_POINT')=='after_ready':
            checkpoint=wait_for(lambda:json.loads((root/'update-state/fixture-checkpoint.json').read_text()),20)
            assert checkpoint['point']=='after_ready'
            wait_for(lambda:process_exited(checkpoint['worker_pid']),5)
            def spawned_candidate():
                value=json.loads((root/'update-state/journal.json').read_text())
                return value if value.get('new_pid') else None
            journal=wait_for(spawned_candidate)
            candidate_pid=journal['new_pid'];processes.append(candidate_pid)
            wait_for(lambda:not (root/f'update-state/instances/{candidate_pid}.json').exists(),20)
            assert json.loads((root/'current.json').read_text())['current']==current['current']
            assert old.exists(),'precommit failure must retain the old binary'
            run([root/'daymark-update-cli.exe','recover','--root',root,'--json'])
            assert json.loads((root/'update-state/journal.json').read_text())['phase']=='aborted'
            run([root/'daymark-update-cli.exe','cancel-ready','--root',root,'--json'])
            run([root/'daymark.exe','--data-dir',data])
            identity=wait_for(old_instance,30);processes.append(identity['pid'])
            import sqlite3
            with sqlite3.connect(data/'daymark.sqlite') as db:
                assert db.execute('PRAGMA quick_check').fetchone()[0]=='ok'
                assert db.execute("SELECT title FROM tasks WHERE id='update-flow-synthetic-task'").fetchone()[0]=='Synthetic update preservation check'
            print('PASS: signed Windows flow; actual Qt worker killed after READY/precommit -> no activation or deletion -> verified old-version recovery -> SQLite preserved',flush=True)
            return
        journal=wait_for(lambda:json.loads((root/'update-state/journal.json').read_text()))
        wait_for(lambda:json.loads((root/'update-state/journal.json').read_text()).get('phase')=='cleaned',45)
        journal=json.loads((root/'update-state/journal.json').read_text());processes.append(journal['new_pid'])
        new_current=json.loads((root/'current.json').read_text())
        assert new_current['version']=='2.0.1'
        assert new_current['previous'] is None
        assert not old.exists(),'old binary must be removed after RUNNING'
        assert unknown.read_text(encoding='utf-8')=='user-owned unknown file'
        assert modified.read_text(encoding='utf-8')=='user-modified signed file'
        assert (root/new_current['current']/'daymark.exe').exists()
        assert not (root/'update-state/prepared-update.json').exists()
        if real:
            import sqlite3
            with sqlite3.connect(data/'daymark.sqlite') as db:
                assert db.execute('PRAGMA quick_check').fetchone()[0]=='ok'
                assert db.execute('PRAGMA user_version').fetchone()[0]==10
                assert db.execute("SELECT title FROM tasks WHERE id='update-flow-synthetic-task'").fetchone()[0]=='Synthetic update preservation check'
        print('PASS: signed Windows flow; live old process -> loopback discovery/download -> side-by-side prepare -> authenticated restart -> durable commit/RUNNING -> old binary cleanup'+(' -> actual Qt application/SQLite' if real else ' (synthetic native payload)'),flush=True)
    except BaseException:
        for name in ('current.json','update-state/status.json','update-state/journal.json'):
            path=root/name
            if path.exists():
                print('FIXTURE '+name+': '+path.read_text(encoding='utf-8')[:10000],flush=True)
        raise
    finally:
        server.shutdown();server.server_close()
        if root.exists():
            # Stop fixture workers before children: a live apply worker can
            # otherwise restore another Qt child while QTemporaryDir is removing
            # its DLLs. Never enumerate or stop an image outside this exact root.
            for name,field in [('update-state/journal.json','worker_pid'),('update-state/status.json','owner_pid')]:
                try:
                    value=json.loads((root/name).read_text());pid=value.get(field)
                    if pid:stop_owned(root,pid)
                except (OSError,ValueError):
                    pass
            for path in (root/'update-state/instances').glob('*.json'):
                try:
                    processes.append(json.loads(path.read_text())['pid'])
                except (OSError,ValueError,KeyError):
                    pass
            try:
                journal=json.loads((root/'update-state/journal.json').read_text())
                if journal.get('new_pid'):
                    processes.append(journal['new_pid'])
            except (OSError,ValueError):
                pass
            for pid in set(processes):
                stop_owned(root,pid)


if __name__=='__main__':
    main()
