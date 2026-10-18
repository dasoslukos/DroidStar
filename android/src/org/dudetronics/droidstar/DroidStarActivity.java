package org.dudetronics.droidstar;

import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;

import org.qtproject.qt.android.bindings.QtActivity;

/*
 * DroidStar Physical PTT - Phase 1C
 *
 * Diagnostic Android key event listener.
 *
 * Does not transmit.
 * Does not consume key events.
 * Preserves Qt's normal keyboard handling.
 */
public class DroidStarActivity extends QtActivity {

    private static final String TAG = "DroidStarPTT";
    /* Phase B2: learn ONE candidate key delivered to the foreground Activity.
     * No radio transmit and no hardcoded F400 button mapping. */
    private static volatile boolean customPttLearning = false;
    private static native void nativeCustomPttLearned(int keyCode, int scanCode, String name);
    private static native void nativeCustomPttLearningStopped();
    // B4: only candidate foreground keys; native C++ verifies saved assignment and opt-in.
    private static native void nativeCustomPttKeyEvent(int keyCode, int scanCode, boolean down);

    public static boolean setCustomPttLearning(boolean enabled) {
        customPttLearning = enabled;
        Log.i(TAG, "B2 button learning " + (enabled ? "started" : "cancelled"));
        return true;
    }

    @Override
    protected void onPause() {
        if (customPttLearning) {
            customPttLearning = false;
            try {
                nativeCustomPttLearningStopped();
            } catch (UnsatisfiedLinkError error) {
                Log.w(TAG, "B2 native learning callback unavailable", error);
            }
        }
        // Android may not deliver key UP after leaving the foreground.
        // Always signal a forced release; native does nothing unless custom TX is held.
        try {
            nativeCustomPttKeyEvent(-1, -1, false);
        } catch (UnsatisfiedLinkError error) {
            Log.w(TAG, "B4 native foreground-release callback unavailable", error);
        }
        super.onPause();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {

        super.onCreate(savedInstanceState);

        Log.i(TAG, "DroidStar custom Activity initialized");
    }

    /*
     * Phase B1: foreground hardware-button detection ONLY.
     *
     * Use a whitelist of likely PTT candidates rather than logging every key,
     * so connected physical keyboards cannot produce broad keystroke logs.
     * Android may reserve some keys or consume them before the Activity sees them.
     * B1 does not bind buttons, consume events, or activate the transmitter.
     */
    private static boolean isCandidatePttKey(int code) {
        return code == KeyEvent.KEYCODE_VOLUME_UP
                || code == KeyEvent.KEYCODE_VOLUME_DOWN
                || code == KeyEvent.KEYCODE_VOLUME_MUTE
                || code == KeyEvent.KEYCODE_CAMERA
                || code == KeyEvent.KEYCODE_FOCUS
                || code == KeyEvent.KEYCODE_HEADSETHOOK
                || code == KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE
                || code == KeyEvent.KEYCODE_MEDIA_PLAY
                || code == KeyEvent.KEYCODE_MEDIA_PAUSE
                || code == KeyEvent.KEYCODE_MEDIA_NEXT
                || code == KeyEvent.KEYCODE_MEDIA_PREVIOUS
                || (code >= KeyEvent.KEYCODE_F1
                    && code <= KeyEvent.KEYCODE_F12)
                || (code >= KeyEvent.KEYCODE_BUTTON_A
                    && code <= KeyEvent.KEYCODE_BUTTON_MODE)
                || (code >= KeyEvent.KEYCODE_BUTTON_1
                    && code <= KeyEvent.KEYCODE_BUTTON_16)
                || (code >= KeyEvent.KEYCODE_PROG_RED
                    && code <= KeyEvent.KEYCODE_PROG_BLUE);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event != null && isCandidatePttKey(event.getKeyCode())) {
            final int action = event.getAction();
            final int repeat = event.getRepeatCount();
            if ((action == KeyEvent.ACTION_DOWN && repeat == 0)
                    || action == KeyEvent.ACTION_UP) {
                final String actionName = action == KeyEvent.ACTION_DOWN
                        ? "DOWN" : "UP";
                Log.i(TAG,
                        "B1 CANDIDATE " + actionName
                        + " | key=" + KeyEvent.keyCodeToString(event.getKeyCode())
                        + " | keyCode=" + event.getKeyCode()
                        + " | scanCode=" + event.getScanCode()
                        + " | deviceId=" + event.getDeviceId()
                        + " | source=0x" + Integer.toHexString(event.getSource())
                        + " | eventTime=" + event.getEventTime());

                // Snapshot learning state BEFORE this event can complete learning.
                // The teaching key press must never cause a TX event.
                final boolean wasLearning = customPttLearning;
                if (wasLearning && action == KeyEvent.ACTION_DOWN && repeat == 0) {
                    customPttLearning = false;
                    final int code = event.getKeyCode();
                    final int scanCode = event.getScanCode();
                    final String name = KeyEvent.keyCodeToString(code);
                    Log.i(TAG, "B2 LEARNED " + name + " | keyCode=" + code
                            + " | scanCode=" + scanCode);
                    try {
                        nativeCustomPttLearned(code, scanCode, name);
                    } catch (UnsatisfiedLinkError error) {
                        Log.e(TAG, "B2 native learning callback unavailable", error);
                    }
                }
                if (!wasLearning) {
                    // Observe, do not intercept Android/Qt key handling.
                    // TX requires matching user-saved key + explicit B4 opt-in.
                    try {
                        nativeCustomPttKeyEvent(event.getKeyCode(),
                                event.getScanCode(), action == KeyEvent.ACTION_DOWN);
                    } catch (UnsatisfiedLinkError error) {
                        Log.e(TAG, "B4 native PTT key bridge unavailable", error);
                    }
                }
            }
        }

        // B1 MUST preserve normal Android/Qt key processing and cannot TX.
        return super.dispatchKeyEvent(event);
    }
}
