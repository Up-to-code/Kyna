# Android ARM64 application shell

`tools/android-shell` embeds Kyna in a Qt Android application. Qt supplies the
Activity and Gradle packaging; the C++ shell extracts the explicit application
file list, loads the packaged GUI adapter, and runs `main.ky`. Application logic
and event handlers stay in Kyna. Qt Network replaces host libcurl for this target.
Lifecycle callbacks use `guiOnLifecycle` on the UI/VM thread. Resource and callback
cleanup runs when the language session closes.

Install matching desktop host Qt and Android ARM64 Qt, Android SDK/NDK, and JDK.
Use Qt's `qt-cmake`, not a host-only CMake configuration:

```bash
/path/to/Qt/android_arm64_v8a/bin/qt-cmake -S . -B build-android \
  -DCMAKE_BUILD_TYPE=Release -DKYNA_BUILD_TESTS=OFF -DKYNA_BUILD_CLI=OFF \
  -DKYNA_WITH_GUI=ON -DKYNA_ANDROID_SHELL=ON \
  -DQT_HOST_PATH=/path/to/matching/host/Qt \
  -DANDROID_SDK_ROOT=/path/to/sdk -DANDROID_NDK_ROOT=/path/to/ndk \
  -DKYNA_ANDROID_APP_SOURCE=/path/to/kyna-app \
  '-DKYNA_ANDROID_APP_FILES=main.ky;gui.ky'
cmake --build build-android --target apk --parallel
```

`main.ky` is required. Supply every imported module explicitly in
`KYNA_ANDROID_APP_FILES`. The default application is the GUI callback smoke test.
The default smoke application runs for 12 seconds and requires a background/foreground transition. After launch, run `python3 tests/native/verify_android_smoke.py`; it sends Home, relaunches the Activity, and requires the timer, lifecycle, and post-session-cleanup completion marker `KYNA_ANDROID_SMOKE_OK`. A successful cross-compilation
alone is not a supported-target claim: installation, launch, lifecycle, callback,
and shutdown smoke must pass on the target.

On 2026-10-11, the Release shell passed installation, launch, timer callback,
background/foreground lifecycle delivery, garbage collection, and session
shutdown on the Android 14 Google APIs ARM64 emulator. The rendered text, input,
and button were also inspected. This verifies that configuration; other Android
versions, physical devices, geometry/ML on Android, and production signing remain
unverified. The shell resolves the GUI adapter alongside its installed native
library rather than relative to extracted Kyna source.

The checked-in packaging uses SDK/build tools 35, NDK 26.1.10909125, matching
Qt host/Android 6.7.3, JDK 17, Android Gradle plugin 8.6.1, and Gradle 8.7.
[AGP 8.6 compatibility](https://developer.android.com/build/releases/agp-8-6-0-release-notes).
The APK produced by `apk` is unsigned. For local smoke testing only, set
`ANDROID_SDK_ROOT` and `JAVA_HOME`, then run:

```bash
python3 tests/native/sign_android_smoke.py path/to/unsigned.apk kyna-smoke.apk
adb install kyna-smoke.apk
adb logcat -c
adb shell monkey -p org.kyna.application 1
python3 tests/native/verify_android_smoke.py
```

The signing helper uses an ephemeral test certificate. Release publication needs
the application's own signing key.

Qt setup and deployment references:
[Getting started](https://doc.qt.io/qt-6/android-getting-started.html),
[androiddeployqt](https://doc.qt.io/qt-6/android-deploy-qt-tool.html).


The local cross-compilation toolchains were archived after verification to save
space. Restore both generated directories before rebuilding this checkout:

```bash
tar -xf build-release/verification/android-ndk-26.1.tar.gz
tar -xf build-release/verification/android-qt-6.7.3.tar.gz
```

These archives are local verification backups, not release distributions.
