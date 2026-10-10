#!/usr/bin/env python3
"""Sign a local APK with an ephemeral test key; never use this for publication."""
import argparse, os, pathlib, subprocess, tempfile
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('apk',type=pathlib.Path)
parser.add_argument('output',type=pathlib.Path)
args=parser.parse_args()
sdk=pathlib.Path(os.environ['ANDROID_SDK_ROOT'])
java=pathlib.Path(os.environ['JAVA_HOME'])
with tempfile.TemporaryDirectory(prefix='kyna-smoke-key-') as directory:
    key=pathlib.Path(directory)/'smoke.p12'
    subprocess.run([str(java/'bin/keytool'),'-genkeypair','-keystore',str(key),'-storepass','android','-keypass','android','-alias','kyna-smoke','-keyalg','RSA','-keysize','2048','-validity','1','-dname','CN=Kyna ephemeral smoke test'],check=True,capture_output=True)
    subprocess.run([str(sdk/'build-tools/35.0.0/apksigner'),'sign','--ks',str(key),'--ks-pass','pass:android','--key-pass','pass:android','--out',str(args.output),str(args.apk)],check=True)
    subprocess.run([str(sdk/'build-tools/35.0.0/apksigner'),'verify',str(args.output)],check=True)
print('Signed APK for smoke testing only:',args.output)
