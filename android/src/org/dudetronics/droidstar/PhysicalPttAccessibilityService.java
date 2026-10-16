package org.dudetronics.droidstar;

import android.accessibilityservice.AccessibilityService;
import android.accessibilityservice.AccessibilityServiceInfo;
import android.content.Context;
import android.os.PowerManager;
import android.util.Log;
import android.view.KeyEvent;
import android.view.accessibility.AccessibilityEvent;

/*
 * DroidStar Physical PTT - Phase 1D
 *
 * Diagnostic-only accessibility key listener.
 *
 * Observes KEYCODE_F2.
 * Does not transmit.
 * Does not consume events.
 * Does not inspect screen contents.
 */
public class PhysicalPttAccessibilityService
        extends AccessibilityService {

    private static final String TAG = "DroidStarPTT";

    @Override
    protected void onServiceConnected() {
        super.onServiceConnected();

        AccessibilityServiceInfo info = getServiceInfo();

        if (info != null) {
            info.flags |=
                AccessibilityServiceInfo.FLAG_REQUEST_FILTER_KEY_EVENTS;

            setServiceInfo(info);

            Log.i(TAG,
                "ACCESSIBILITY SERVICE CONNECTED"
                + " | key filtering requested");
        } else {
            Log.w(TAG,
                "ACCESSIBILITY SERVICE CONNECTED"
                + " | service info unavailable");
        }
    }

    @Override
    protected boolean onKeyEvent(KeyEvent event) {

        if (event == null) {
            return false;
        }

        if (event.getKeyCode() != KeyEvent.KEYCODE_F2) {
            return false;
        }

        String action;

        if (event.getAction() == KeyEvent.ACTION_DOWN) {

            // Ignore Android's repeated DOWN events.
            if (event.getRepeatCount() != 0) {
                return false;
            }

            action = "DOWN";

        } else if (event.getAction() == KeyEvent.ACTION_UP) {

            action = "UP";

        } else {
            return false;
        }

        PowerManager pm = (PowerManager)
            getSystemService(Context.POWER_SERVICE);

        boolean interactive =
            pm != null && pm.isInteractive();

        Log.i(TAG,
            "ACCESSIBILITY KEY_F2 " + action
            + " | keyCode=" + event.getKeyCode()
            + " | deviceId=" + event.getDeviceId()
            + " | screenInteractive=" + interactive
            + " | eventTime=" + event.getEventTime()
        );

        // Do not consume the key.
        return false;
    }

    @Override
    public void onAccessibilityEvent(AccessibilityEvent event) {
        // Diagnostic intentionally ignores accessibility events.
    }

    @Override
    public void onInterrupt() {
        Log.i(TAG, "ACCESSIBILITY SERVICE INTERRUPTED");
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "ACCESSIBILITY SERVICE DESTROYED");
        super.onDestroy();
    }
}
