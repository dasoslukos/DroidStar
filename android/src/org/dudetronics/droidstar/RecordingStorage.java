package org.dudetronics.droidstar;

import android.content.ContentResolver;
import android.content.ContentUris;
import android.database.Cursor;
import java.io.InputStream;
import java.io.File;
import java.io.FileOutputStream;
import org.json.JSONArray;
import org.json.JSONObject;
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

    /*
     * Returns completed DroidStar shared recordings as a JSON array.
     *
     * Each item contains:
     *     fileName, uri, sizeBytes, modifiedSeconds, durationMs
     */
    public static String listSharedMusicRecordings(Context context) {
        JSONArray recordings = new JSONArray();

        if (context == null ||
                Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return recordings.toString();
        }

        ContentResolver resolver = context.getContentResolver();

        Uri collection = MediaStore.Audio.Media.getContentUri(
                MediaStore.VOLUME_EXTERNAL_PRIMARY
        );

        String[] projection = new String[] {
                MediaStore.Audio.Media._ID,
                MediaStore.Audio.Media.DISPLAY_NAME,
                MediaStore.Audio.Media.SIZE,
                MediaStore.Audio.Media.DATE_MODIFIED
        };

        String selection =
                MediaStore.Audio.Media.RELATIVE_PATH +
                " LIKE ? AND " +
                MediaStore.Audio.Media.DISPLAY_NAME +
                " LIKE ?";

        String[] selectionArguments = new String[] {
                RELATIVE_DIRECTORY + "%",
                "%.wav"
        };

        try (Cursor cursor = resolver.query(
                collection,
                projection,
                selection,
                selectionArguments,
                MediaStore.Audio.Media.DATE_MODIFIED + " DESC"
        )) {
            if (cursor == null) {
                return recordings.toString();
            }

            int idColumn = cursor.getColumnIndexOrThrow(
                    MediaStore.Audio.Media._ID
            );

            int nameColumn = cursor.getColumnIndexOrThrow(
                    MediaStore.Audio.Media.DISPLAY_NAME
            );

            int sizeColumn = cursor.getColumnIndexOrThrow(
                    MediaStore.Audio.Media.SIZE
            );

            int modifiedColumn = cursor.getColumnIndexOrThrow(
                    MediaStore.Audio.Media.DATE_MODIFIED
            );

            while (cursor.moveToNext()) {
                long id = cursor.getLong(idColumn);
                String fileName = cursor.getString(nameColumn);
                long sizeBytes = cursor.getLong(sizeColumn);
                long modifiedSeconds =
                        cursor.getLong(modifiedColumn);

                Uri recordingUri = ContentUris.withAppendedId(
                        collection,
                        id
                );

                JSONObject recording = new JSONObject();
                recording.put("fileName", fileName);
                recording.put(
                        "uri",
                        recordingUri.toString()
                );
                recording.put("sizeBytes", sizeBytes);
                recording.put(
                        "modifiedSeconds",
                        modifiedSeconds
                );
                recording.put(
                        "durationMs",
                        readWavDurationMs(
                                resolver,
                                recordingUri,
                                sizeBytes
                        )
                );

                recordings.put(recording);
            }
        }
        catch (Exception exception) {
            Log.e(
                    TAG,
                    "Could not list shared Music recordings",
                    exception
            );
        }

        return recordings.toString();
    }

    private static long readWavDurationMs(
            ContentResolver resolver,
            Uri uri,
            long sizeBytes
    ) {
        byte[] header = new byte[44];
        int offset = 0;

        try (InputStream input = resolver.openInputStream(uri)) {
            if (input != null) {
                while (offset < header.length) {
                    int count = input.read(
                            header,
                            offset,
                            header.length - offset
                    );

                    if (count < 0) {
                        break;
                    }

                    offset += count;
                }
            }
        }
        catch (Exception exception) {
            Log.w(
                    TAG,
                    "Could not read WAV header for " + uri,
                    exception
            );
        }

        if (offset >= 44 &&
                header[0] == 'R' &&
                header[1] == 'I' &&
                header[2] == 'F' &&
                header[3] == 'F' &&
                header[8] == 'W' &&
                header[9] == 'A' &&
                header[10] == 'V' &&
                header[11] == 'E') {
            long byteRate = readLe32(header, 28);
            long dataBytes = readLe32(header, 40);

            if (byteRate > 0 && dataBytes > 0) {
                return (dataBytes * 1000L) / byteRate;
            }
        }

        if (sizeBytes > 44) {
            // DroidStar recordings are 8 kHz mono signed 16-bit PCM.
            return ((sizeBytes - 44L) * 1000L) / 16000L;
        }

        return 0;
    }

    private static long readLe32(byte[] data, int offset) {
        return
                ((long)data[offset] & 0xffL) |
                (((long)data[offset + 1] & 0xffL) << 8) |
                (((long)data[offset + 2] & 0xffL) << 16) |
                (((long)data[offset + 3] & 0xffL) << 24);
    }

    /*
     * Copies a shared MediaStore recording into the app cache so Qt's
     * FFmpeg media backend can play it through a normal file URL.
     */
    public static String prepareSharedRecordingForPlayback(
            Context context,
            String uriString,
            String displayName
    ) {
        if (context == null ||
                uriString == null ||
                uriString.trim().isEmpty()) {
            return "";
        }

        String safeName =
                displayName == null
                ? "droidstar-recording.wav"
                : new File(displayName).getName();

        safeName = safeName.replaceAll(
                "[^A-Za-z0-9._-]",
                "_"
        );

        if (safeName.isEmpty()) {
            safeName = "droidstar-recording.wav";
        }

        File playbackDirectory = new File(
                context.getCacheDir(),
                "recording-playback"
        );

        if (!playbackDirectory.exists() &&
                !playbackDirectory.mkdirs()) {
            Log.e(
                    TAG,
                    "Could not create recording playback cache"
            );
            return "";
        }

        File targetFile = new File(
                playbackDirectory,
                safeName
        );

        Uri sourceUri = Uri.parse(uriString);

        try (
                InputStream input =
                        context.getContentResolver()
                                .openInputStream(sourceUri);
                FileOutputStream output =
                        new FileOutputStream(targetFile, false)
        ) {
            if (input == null) {
                Log.e(
                        TAG,
                        "Could not open shared recording for playback"
                );
                return "";
            }

            byte[] buffer = new byte[64 * 1024];
            int count;

            while ((count = input.read(buffer)) >= 0) {
                if (count > 0) {
                    output.write(buffer, 0, count);
                }
            }

            output.flush();

            Log.d(
                    TAG,
                    "Prepared recording for playback: " +
                            targetFile.getAbsolutePath()
            );

            return targetFile.getAbsolutePath();
        }
        catch (Exception exception) {
            Log.e(
                    TAG,
                    "Could not prepare recording for playback",
                    exception
            );

            try {
                targetFile.delete();
            }
            catch (Exception ignored) {
            }

            return "";
        }
    }

}
