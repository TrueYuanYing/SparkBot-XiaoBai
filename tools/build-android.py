"""Offline Android SDK build; no Gradle downloads or global environment changes."""
from pathlib import Path
import os, secrets, subprocess, zipfile
root=Path(__file__).resolve().parent.parent
app=root/'android'; build=app/'build'; build.mkdir(parents=True,exist_ok=True)
jdk_env=os.environ.get('JAVA_HOME'); sdk_env=os.environ.get('ANDROID_SDK_ROOT') or os.environ.get('ANDROID_HOME')
if not jdk_env or not sdk_env:raise SystemExit('Set JAVA_HOME (JDK 17) and ANDROID_SDK_ROOT (SDK 35) first.')
jdk=Path(jdk_env);sdk=Path(sdk_env)
bt=sdk/'build-tools/35.0.0'; platform=sdk/'platforms/android-35/android.jar'
env=dict(os.environ,JAVA_HOME=str(jdk)); env['PATH']=str(jdk/'bin')+';'+env.get('PATH','')
def run(args): subprocess.run([str(x) for x in args],check=True,env=env,cwd=app)
run([bt/'aapt2.exe','compile','--dir',app/'res','-o',build/'resources.zip'])
run([bt/'aapt2.exe','link','-o',build/'unsigned.apk','--manifest',app/'AndroidManifest.xml','-I',platform,build/'resources.zip','--auto-add-overlay'])
classes=build/'classes';classes.mkdir(exist_ok=True)
run([jdk/'bin/javac.exe','-encoding','UTF-8','-source','8','-target','8','-classpath',platform,'-d',classes,*sorted((app/'src').rglob('*.java'))])
jar=build/'classes.jar'
with zipfile.ZipFile(jar,'w') as z:
 for path in classes.rglob('*.class'):z.write(path,path.relative_to(classes).as_posix())
run([jdk/'bin/java.exe','-cp',bt/'lib/d8.jar','com.android.tools.r8.D8','--release','--min-api','26','--lib',platform,'--output',build,jar])
with zipfile.ZipFile(build/'unsigned.apk','a',zipfile.ZIP_DEFLATED) as z:z.write(build/'classes.dex','classes.dex')
run([bt/'zipalign.exe','-f','4',build/'unsigned.apk',build/'aligned.apk'])
sign=app/'.signing';sign.mkdir(exist_ok=True);key=sign/'xiaobai.jks';password=sign/'password.txt'
if not key.exists():
 if not password.exists():password.write_text(secrets.token_urlsafe(32),encoding='ascii')
 env['XIAOBAI_SIGN_PASSWORD']=password.read_text().strip()
 run([jdk/'bin/keytool.exe','-genkeypair','-keystore',key,'-storepass:env','XIAOBAI_SIGN_PASSWORD','-keypass:env','XIAOBAI_SIGN_PASSWORD','-alias','xiaobai','-keyalg','RSA','-keysize','3072','-validity','10000','-dname','CN=XiaoBai Local Controller'])
env['XIAOBAI_SIGN_PASSWORD']=password.read_text().strip()
target=build/'XiaoBai-0.9.apk'
run([jdk/'bin/java.exe','-jar',bt/'lib/apksigner.jar','sign','--ks',key,'--ks-key-alias','xiaobai','--ks-pass','env:XIAOBAI_SIGN_PASSWORD','--key-pass','env:XIAOBAI_SIGN_PASSWORD','--out',target,build/'aligned.apk'])
run([jdk/'bin/java.exe','-jar',bt/'lib/apksigner.jar','verify','--verbose',target])
print('APK:',target,'bytes:',target.stat().st_size)
