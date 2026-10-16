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

    @Override
    public void onCreate(Bundle savedInstanceState) {

        super.onCreate(savedInstanceState);

        Log.i(TAG, "DroidStar custom Activity initialized");
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {

        if (event != null &&
                event.getKeyCode() == KeyEvent.KEYCODE_F2) {

            String action;

            switch (event.getAction()) {

                case KeyEvent.ACTION_DOWN:
                    action = "DOWN";
                    break;

                case KeyEvent.ACTION_UP:
                    action = "UP";
                    break;

                default:
                    action = "OTHER";
                    break;
            }

            Log.i(TAG,
                "HARDWARE KEY_F2 " + action
                + " | keyCode=" + event.getKeyCode()
                + " | repeat=" + event.getRepeatCount()
                + " | deviceId=" + event.getDeviceId()
                + " | eventTime=" + event.getEventTime()
            );
        }

        return super.dispatchKeyEvent(event);
    }
}
