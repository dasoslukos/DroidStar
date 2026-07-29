# DroidStar Android 16 foreground-audio patch

Copy these files over the corresponding files in the DroidStar source tree:

- `main.cpp`
- `droidstar.cpp`
- `android/AndroidManifest.xml`
- `android/src/org/dudetronics/droidstar/NotificationClient.java`

Then rebuild and deploy from Qt Creator.

This patch:

- starts a real foreground service before the mode thread begins;
- declares media-playback and microphone foreground-service types;
- keeps a partial wake lock only while DroidStar is connected;
- updates the persistent notification after connection;
- stops the foreground service when DroidStar disconnects;
- explicitly keeps Qt event loops active while the Activity is backgrounded.

Useful test log:

```bash
ADB="$HOME/Android/Sdk/platform-tools/adb"
PID="$($ADB shell pidof -s org.dudetronics.droidstar)"
$ADB logcat --pid="$PID" -v time | tee ~/droidstar-android16-fgs-test.log
```

Look for `DroidStarService: Foreground audio service created` before
backgrounding the app.
