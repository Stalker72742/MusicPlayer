package com.example.MusicPlayer;

import android.os.Bundle;
import android.content.pm.PackageManager;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import org.qtproject.qt.android.bindings.QtActivity;
import org.qtproject.qt.android.QtNative;
import android.util.Log;

public class MainActivity extends org.qtproject.qt.android.bindings.QtActivity {
    private static final String TAG = "MainActivity";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.d(TAG, "MainActivity created");
    }

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

            // Уведомляем C++ код о результате
            onPermissionResult(requestCode, allGranted);

        } else {
            Log.d(TAG, "Unknown request code: " + requestCode);
        }
    }

    // Этот метод будет вызван из C++
    public static native void onPermissionResult(int requestCode, boolean granted);
}
