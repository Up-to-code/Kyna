#!/usr/bin/env python3
"""Require actual application completion rather than treating APK compilation as support."""
import subprocess,time
transitioned=False
for _ in range(60):
    log=subprocess.check_output(['adb','logcat','-d'],text=True)
    if not transitioned and 'KYNA_ANDROID_STARTED' in log:
        time.sleep(2)
        subprocess.run(['adb','shell','input','keyevent','KEYCODE_HOME'],check=True)
        time.sleep(1)
        subprocess.run(['adb','shell','monkey','-p','org.kyna.application','1'],check=True,stdout=subprocess.DEVNULL)
        transitioned=True
    if 'KYNA_ANDROID_SMOKE_OK' in log:
        print('Android application callback/shutdown smoke passed');break
    if 'KANDROID100' in log or 'Fatal signal' in log:
        raise SystemExit('Android application failed:\n'+log[-8000:])
    time.sleep(1)
else:raise SystemExit('Android application did not produce its completion marker')
