"""Verify and flash a published SparkBot head bundle without erasing NVS."""
import argparse,hashlib,json,subprocess,sys
from pathlib import Path

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle',type=Path,default=Path(__file__).resolve().parent)
    parser.add_argument('--port',help='Serial port, e.g. COM4 or /dev/ttyACM0')
    parser.add_argument('--initial',action='store_true',help='Install all images after backing up and checking hardware compatibility')
    parser.add_argument('--verify-only',action='store_true')
    args=parser.parse_args();base=args.bundle.resolve()
    manifest=json.loads((base/'manifest.json').read_text(encoding='utf8'))
    if manifest['chip']!='ESP32-S3' or manifest['flash_mb']!=16:
        parser.error('Only ESP32-S3 16MB head bundles are supported.')
    limits={'bootloader.bin':(0x0,0x8000),'partition-table.bin':(0x8000,0x1000),
            'ota_data_initial.bin':(0xd000,0x2000),'xiaozhi.bin':(0x100000,0x600000),'srmodels.bin':(0xd00000,0x300000)}
    seen=set();parts=[]
    for item in manifest['files']:
        name=item['name']
        if name not in limits or name in seen:raise ValueError('Unexpected or duplicate image')
        offset,limit=limits[name];path=base/name;data=path.read_bytes()
        if int(item['offset'],16)!=offset or not 0<len(data)<=limit or len(data)!=item['bytes']:
            raise ValueError('Invalid offset/size: '+name)
        if hashlib.sha256(data).hexdigest()!=item['sha256']:raise ValueError('SHA256 mismatch: '+name)
        seen.add(name);parts.append((offset,path));print('Verified:',name)
    if seen!=set(limits):raise ValueError('Missing required images')
    if 'android' in manifest:
        apk=manifest['android'];name=apk['name']
        if Path(name).name!=name or not name.endswith('.apk'):raise ValueError('Invalid APK name')
        data=(base/name).read_bytes()
        if len(data)!=apk['bytes'] or hashlib.sha256(data).hexdigest()!=apk['sha256']:raise ValueError('APK verification failed')
        print('Verified:',name)
    if args.verify_only or not args.port:
        print('Verification complete; no device writes.');return
    images=parts if args.initial else [(offset,path) for offset,path in parts if offset==0x100000]
    cmd=[sys.executable,'-m','esptool','--chip','esp32s3','--port',args.port,'--baud','921600','write-flash']
    if args.initial:cmd+=['--flash-mode','dio','--flash-freq','80m','--flash-size','16MB']
    for offset,path in sorted(images):cmd += [hex(offset),str(path)]
    subprocess.run(cmd,check=True)

if __name__=='__main__':main()
