package org.dudetronics.droidstar;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.PowerManager;
import android.os.SystemClock;
import android.util.Log;

/*
 * DroidStar Physical PTT - Phase 1A
 *
 * Diagnostic receiver only.
 *
 * This class observes candidate hardware PTT broadcasts.
 * It does NOT activate or release the transmitter.
 */
public final class PhysicalPttProbe extends BroadcastReceiver {

    private static final String TAG = "DroidStarPTT";

    // Registered by the Qt/C++ side. Never call this for untrusted broadcasts.
    private static native void nativePhysicalPtt(boolean down);

    private boolean registered = false;

    public void register(Context context) {

        if (registered) {
            return;
        }

        IntentFilter filter = new IntentFilter();

        filter.addAction("android.intent.action.ptt.down");
        filter.addAction("android.intent.action.ptt.up");

        filter.addAction("com.zello.ptt.down");
        filter.addAction("com.zello.ptt.up");
        filter.addAction("com.ptt1.action.PTT.down");
        filter.addAction("com.ptt1.action.PTT.up");
        filter.addAction("com.ptt2.action.PTT.down");
        filter.addAction("com.ptt2.action.PTT.up");

        filter.addAction("android.intent.action.PTT.down");
        filter.addAction("android.intent.action.PTT.up");

        filter.addAction("android.xin.PTT.start");
        filter.addAction("android.xin.PTT.end");

        try {
            if (Build.VERSION.SDK_INT >= 33) {
                context.registerReceiver(
                    this,
                    filter,
                    "android.permission.STATUS_BAR",
                    null,
                    Context.RECEIVER_EXPORTED
                );
            } else {
                context.registerReceiver(this, filter, "android.permission.STATUS_BAR", null);
            }

            registered = true;

            Log.i(TAG,
                "Physical PTT diagnostic receiver registered");

        } catch (RuntimeException exception) {

            Log.e(TAG,
                "Could not register physical PTT receiver",
                exception);
        }
    }

    public void unregister(Context context) {

        if (!registered) {
            return;
        }

        try {
            context.unregisterReceiver(this);

            try {
                nativePhysicalPtt(false);
            } catch (UnsatisfiedLinkError error) {
                Log.w(TAG, "Native bridge unavailable during cleanup", error);
            }
        } catch (RuntimeException exception) {
            Log.w(TAG,
                "Could not unregister PTT receiver",
                exception);
        }

        registered = false;

        Log.i(TAG,
            "Physical PTT diagnostic receiver unregistered");
    }

    @Override
    public void onReceive(Context context, Intent intent) {

        if (intent == null) {
            return;
        }

        String action = intent.getAction();

        if (action == null) {
            return;
        }

        String event;

        if (action.endsWith(".down") || action.endsWith(".start")) {
            event = "DOWN";
        } else if (action.endsWith(".up") || action.endsWith(".end")) {
            event = "UP";
        } else {
            event = "UNKNOWN";
        }

        PowerManager powerManager =
            (PowerManager) context.getSystemService(
                Context.POWER_SERVICE);

        boolean screenInteractive =
            powerManager != null && powerManager.isInteractive();

        long timestamp = SystemClock.elapsedRealtime();

        Log.i(TAG,
            "PTT " + event
            + " | action=" + action
            + " | screenInteractive=" + screenInteractive
            + " | elapsedMs=" + timestamp
        );

        // F400 transmits both PTT1 and PTT2 for one button; act on PTT1 only.
        if ("com.ptt1.action.PTT.down".equals(action)
                || "com.ptt1.action.PTT.up".equals(action)) {
            // The receiver registration requires a privileged sender permission.
            // Android 15 withholds the UID for this system broadcast (uid=-1).
            // This permission also admits Android shell on this device: keep
            // v6.1 experimental; harden source attribution before release.
            try {
                nativePhysicalPtt("com.ptt1.action.PTT.down".equals(action));
            } catch (UnsatisfiedLinkError error) {
                Log.e(TAG, "Native TX bridge unavailable", error);
            }
        }
    }
}
