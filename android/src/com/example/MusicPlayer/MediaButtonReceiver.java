package com.example.MusicPlayer;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.view.KeyEvent;
import android.util.Log;

public class MediaButtonReceiver extends BroadcastReceiver {
    private static final String TAG = "MediaButtonReceiver";

    public static native void onHeadsetButton(int keyCode);

    public MediaButtonReceiver() {
        Log.d(TAG, "🔨 MediaButtonReceiver создан!");
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        Log.d(TAG, "🎧 onReceive вызван!");
        Log.d(TAG, "📨 Intent action: " + intent.getAction());
        Log.d(TAG, "📦 Intent: " + intent.toString());

        Log.d(TAG, "🚨 ПОЛУЧЕН ЛЮБОЙ INTENT!");
        Log.d(TAG, "📨 Action: " + intent.getAction());
        Log.d(TAG, "📦 Extras: " + intent.getExtras());
        Log.d(TAG, "🔗 Categories: " + intent.getCategories());
        Log.d(TAG, "📍 Data: " + intent.getData());

        if (Intent.ACTION_MEDIA_BUTTON.equals(intent.getAction())) {
            Log.d(TAG, "✅ Это ACTION_MEDIA_BUTTON!");
        } else {
            Log.d(TAG, "❌ Это НЕ ACTION_MEDIA_BUTTON: " + intent.getAction());
        }

        KeyEvent event = intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
        if (event != null) {
            Log.d(TAG, "🎯 KeyEvent получен: " + event.getKeyCode());
        } else {
            Log.d(TAG, "💥 KeyEvent is NULL!");
        }
    }
}
