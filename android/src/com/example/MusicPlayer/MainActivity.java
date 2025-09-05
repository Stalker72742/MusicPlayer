package com.example.MusicPlayer;

import android.os.Bundle;
import android.content.pm.PackageManager;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import android.util.Log;
import android.view.KeyEvent;
import android.content.IntentFilter;
import android.content.Context;
import android.content.Intent;
import android.content.BroadcastReceiver;
import android.media.session.MediaSession;
import android.media.session.PlaybackState;
import android.media.AudioManager;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.media.AudioManager;
import android.media.session.MediaSession;
import android.media.session.PlaybackState;
import android.media.MediaMetadata;
import android.app.NotificationManager;
import android.app.NotificationChannel;
import android.app.Notification;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothHeadset;
import android.bluetooth.BluetoothProfile;
import android.view.KeyEvent;
import android.os.Build;

import android.content.ComponentName;
import android.media.AudioManager;
import android.media.session.MediaSession;
import android.os.Build;
import android.annotation.TargetApi;

import org.qtproject.qt.android.QtNative;
import org.qtproject.qt.android.bindings.QtActivity;

public class MainActivity extends org.qtproject.qt.android.bindings.QtActivity {
    private static final String TAG = "MainActivity";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.d(TAG, "MainActivity created");
    }

    public static native void onPermissionResult(int requestCode, boolean granted);

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);

        Log.d(TAG, "onRequestPermissionsResult called with requestCode: " + requestCode);

        if (requestCode == 1001) {
            Log.d(TAG, "Processing media permissions result");

            boolean allGranted = true;
            for (int i = 0; i < permissions.length; i++) {
                String permission = permissions[i];
                int result = grantResults[i];

                Log.d(TAG, "Permission: " + permission + " = " +
                      (result == PackageManager.PERMISSION_GRANTED ? "GRANTED" : "DENIED"));

                if (result != PackageManager.PERMISSION_GRANTED) {
                    allGranted = false;
                }
            }

            onPermissionResult(requestCode, allGranted);

        } else {
            Log.d(TAG, "Unknown request code: " + requestCode);
        }
    }
}
