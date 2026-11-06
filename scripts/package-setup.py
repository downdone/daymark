"""Build a native offline wizard from an already signed managed package; no publication."""
from __future__ import annotations
import argparse, hashlib, json, subprocess
from pathlib import Path

COMPILER_SHA256='4ec390d0f96f9728507fbc2f4e91311dd90bfc6c8ddfe2159357b3651ae151c0'
def sha(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def build(a):
    compiler=a.makensis.resolve(strict=True);bundle=a.bundle.resolve(strict=True);helper=bundle/'daymark-setup-helper.exe'
    if sha(compiler)!=COMPILER_SHA256:raise ValueError('NSIS compiler does not match the reviewed 3.13 pin')
    if a.output.exists():raise ValueError('refusing to replace an existing installer')
    if a.certificate_sha256!=json.loads((Path(__file__).resolve().parents[1]/'config/authenticode-policy.json').read_text())['certificate_sha256']:raise ValueError('installer certificate must match repository pin')
    subprocess.run([str(helper),'verify-bundle','--source',str(bundle),'--root',str(bundle)],check=True,timeout=120)
    seed=json.loads((bundle/'bootstrap-release.json').read_text());version=seed['version']
    files=json.loads((bundle/'FILES.json').read_text())['files'];owned={e['path']:e['sha256'] for e in files}
    if owned.get('daymark-setup-helper.exe')!=sha(helper):raise ValueError('setup helper is not part of signed bundle')
    if not (bundle/seed['payload_dir']/'licenses/NSIS-COPYING.txt').is_file():raise ValueError('NSIS license notice missing from signed payload')
    repo=Path(__file__).resolve().parents[1];a.output.parent.mkdir(parents=True,exist_ok=True)
    command=[str(compiler),'/V2','/INPUTCHARSET','UTF8',f'/DAPP_VERSION={version}',f'/DOUTPUT_FILE={a.output.resolve()}',f'/DBUNDLE_DIR={bundle}',f'/DHELPER_FILE={helper}',f'/DLICENSE_FILE={repo / "LICENSE"}',f'/DAPP_ICON={repo / "assets/icons/daymark.ico"}',f'/DSIGN_TOOL={a.signtool.resolve()}',f'/DCERT_THUMBPRINT={a.certificate_thumbprint}',str(repo/'packaging/windows/Daymark.nsi')]
    subprocess.run(command,check=True,timeout=300)
    subprocess.run([str(a.signtool),'sign','/fd','SHA256','/s','My','/sha1',a.certificate_thumbprint,'/d','Daymark',str(a.output)],check=True,timeout=60)
    subprocess.run(['pwsh','-NoProfile','-File',str(repo/'scripts/Verify-WindowsExecutable.ps1'),'-Executable',str(a.output.resolve()),'-CertificateSha256',a.certificate_sha256],check=True,timeout=60)
    digest=sha(a.output);a.output.with_suffix('.exe.sha256').write_text(digest+'  '+a.output.name+'\n',encoding='ascii')
    a.output.with_suffix('.exe.json').write_text(json.dumps({'format':1,'version':version,'source_revision':seed['source_revision'],'sha256':digest,'compiler':'NSIS 3.13','compiler_sha256':COMPILER_SHA256,'signature_policy':'self-signed-pinned','public_trust':False,'layout':'portable-managed-v1','local_candidate':'-local' in a.output.name},indent=2),encoding='utf-8')
    print('PASS: signed native installer and signed embedded uninstaller; '+str(a.output))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('makensis','bundle','output','signtool'):p.add_argument('--'+name,type=Path,required=True)
    for name in ('certificate-thumbprint','certificate-sha256'):p.add_argument('--'+name,required=True)
    build(p.parse_args())
