package org.dudetronics.droidstar;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.MediaStore;
import android.util.Log;

public final class RecordingStorage {
    private static final String TAG = "DroidStarRecording";
    private static final String RELATIVE_DIRECTORY =
            Environment.DIRECTORY_MUSIC + "/DroidStar/Recordings";

    private RecordingStorage() {
    }

    /*
     * Creates a pending WAV entry in Android MediaStore.
     *
     * Returns:
     *     <native file descriptor>\n<content URI>
     *
     * The descriptor is detached from ParcelFileDescriptor and becomes
     * the responsibility of the native Qt/C++ recorder.
     */
    public static String createSharedMusicRecording(
            Context context,
            String displayName
    ) {
        if (context == null ||
                displayName == null ||
                displayName.trim().isEmpty()) {
            return "";
        }

        // RELATIVE_PATH and IS_PENDING require Android 10/API 29.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            Log.w(TAG, "Shared Music recordings require Android 10+");
            return "";
        }

        ContentResolver resolver = context.getContentResolver();
        Uri recordingUri = null;

        try {
            ContentValues values = new ContentValues();
            values.put(
                    MediaStore.Audio.Media.DISPLAY_NAME,
                    displayName
            );
            values.put(
                    MediaStore.Audio.Media.MIME_TYPE,
                    "audio/wav"
            );
            values.put(
                    MediaStore.Audio.Media.RELATIVE_PATH,
                    RELATIVE_DIRECTORY
            );
            values.put(
                    MediaStore.Audio.Media.IS_PENDING,
                    1
            );

            Uri collection = MediaStore.Audio.Media.getContentUri(
                    MediaStore.VOLUME_EXTERNAL_PRIMARY
            );

            recordingUri = resolver.insert(collection, values);

            if (recordingUri == null) {
                Log.e(TAG, "MediaStore insert returned null");
                return "";
            }

            ParcelFileDescriptor descriptor =
                    resolver.openFileDescriptor(
                            recordingUri,
                            "rw",
                            null
                    );

            if (descriptor == null) {
                resolver.delete(recordingUri, null, null);
                Log.e(TAG, "Could not open MediaStore file descriptor");
                return "";
            }

            int fd = descriptor.detachFd();

            return Integer.toString(fd) +
                    "\n" +
                    recordingUri.toString();
        }
        catch (Exception exception) {
            Log.e(
                    TAG,
                    "Could not create shared Music recording",
                    exception
            );

            if (recordingUri != null) {
                try {
                    resolver.delete(recordingUri, null, null);
                }
                catch (Exception ignored) {
                }
            }

            return "";
        }
    }

    /*
     * Makes a completed recording visible to file browsers and media apps.
     */
    public static boolean publishSharedMusicRecording(
            Context context,
            String uriString
    ) {
        if (context == null ||
                uriString == null ||
                uriString.trim().isEmpty()) {
            return false;
        }

        try {
            Uri uri = Uri.parse(uriString);
            ContentValues values = new ContentValues();

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                values.put(MediaStore.Audio.Media.IS_PENDING, 0);
            }

            context.getContentResolver().update(
                    uri,
                    values,
                    null,
                    null
            );

            return true;
        }
        catch (Exception exception) {
            Log.e(TAG, "Could not publish recording", exception);
            return false;
        }
    }

    /*
     * Removes a MediaStore item if native recording setup fails.
     */
    public static boolean discardSharedMusicRecording(
            Context context,
            String uriString
    ) {
        if (context == null ||
                uriString == null ||
                uriString.trim().isEmpty()) {
            return false;
        }

        try {
            Uri uri = Uri.parse(uriString);

            context.getContentResolver().delete(
                    uri,
                    null,
                    null
            );

            return true;
        }
        catch (Exception exception) {
            Log.e(TAG, "Could not discard recording", exception);
            return false;
        }
    }
}
