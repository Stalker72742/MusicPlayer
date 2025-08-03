package com.example.MusicPlayer;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.view.KeyEvent;
import android.util.Log;
import android.os.Bundle;

public class MediaButtonReceiver extends BroadcastReceiver {
    private static final String TAG = "MediaButtonReceiver";

    public static native void onHeadsetButton(int keyCode);

    public MediaButtonReceiver() {
        Log.d(TAG, "🔨 MediaButtonReceiver создан!");
    }

private void tryAlternativeKeyDetection(Intent intent) {
        Log.d(TAG, "🔍 Пробуем альтернативные способы определения клавиши...");

        // Проверяем все extras в Intent
        Bundle extras = intent.getExtras();
        if (extras != null) {
            Log.d(TAG, "📋 Все extras в Intent:");
            for (String key : extras.keySet()) {
                Object value = extras.get(key);
                Log.d(TAG, "  🔑 " + key + " = " + value + " (" + (value != null ? value.getClass().getSimpleName() : "null") + ")");

                // Если находим что-то связанное с клавишами
                if (key.toLowerCase().contains("key") || key.toLowerCase().contains("button")) {
                    Log.d(TAG, "  ⭐ Потенциально важный extra: " + key + " = " + value);
                }
            }
        }

        // Пробуем получить KeyEvent под другими именами
        Object keyEvent1 = intent.getParcelableExtra("android.intent.extra.KEY_EVENT");
        Object keyEvent2 = intent.getParcelableExtra("key_event");
        Object keyEvent3 = intent.getParcelableExtra("keyevent");

        Log.d(TAG, "🔍 Альтернативные KeyEvent:");
        Log.d(TAG, "  android.intent.extra.KEY_EVENT: " + keyEvent1);
        Log.d(TAG, "  key_event: " + keyEvent2);
        Log.d(TAG, "  keyevent: " + keyEvent3);

        // Предполагаем, что это кнопка play/pause (самая частая)
        Log.d(TAG, "🎯 Предполагаем PLAY_PAUSE по умолчанию");
        //onHeadsetButton(KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE);
    }

    private void checkOtherMediaActions(Intent intent) {
        String action = intent.getAction();
        Log.d(TAG, "🔍 Проверяем другие медиа actions...");

        // Возможные альтернативные action'ы
        if (action != null) {
            if (action.contains("PLAY")) {
                Log.d(TAG, "🎵 Найден PLAY action");
                //onHeadsetButton(KeyEvent.KEYCODE_MEDIA_PLAY);
            } else if (action.contains("PAUSE")) {
                Log.d(TAG, "⏸️ Найден PAUSE action");
                //onHeadsetButton(KeyEvent.KEYCODE_MEDIA_PAUSE);
            } else if (action.contains("NEXT")) {
                Log.d(TAG, "⏭️ Найден NEXT action");
                //onHeadsetButton(KeyEvent.KEYCODE_MEDIA_NEXT);
            } else if (action.contains("PREVIOUS")) {
                Log.d(TAG, "⏮️ Найден PREVIOUS action");
                //onHeadsetButton(KeyEvent.KEYCODE_MEDIA_PREVIOUS);
            } else if (action.contains("HEADSET") || action.contains("BLUETOOTH")) {
                Log.d(TAG, "🎧 Найден HEADSET/BLUETOOTH action");
                //onHeadsetButton(KeyEvent.KEYCODE_HEADSETHOOK);
            }
        }
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

            // Пробуем получить KeyEvent разными способами
            KeyEvent event = intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
            if (event != null) {
                int keyCode = event.getKeyCode();
                int action = event.getAction();

                Log.d(TAG, "🎯 KeyEvent получен: " + keyCode);
                Log.d(TAG, "🎬 Action: " + (action == KeyEvent.ACTION_DOWN ? "DOWN" : "UP"));

                // Определяем тип медиа клавиши
                String keyName = getMediaKeyName(keyCode);
                Log.d(TAG, "🎵 Тип клавиши: " + keyName);

                // Обрабатываем только нажатие (ACTION_DOWN), игнорируем отпускание (ACTION_UP)
                if (action == KeyEvent.ACTION_DOWN) {
                    //onHeadsetButton(keyCode);
                }
            } else {
                Log.d(TAG, "💥 KeyEvent is NULL!");

                // Альтернативные способы получения информации
                tryAlternativeKeyDetection(intent);
            }
        } else {
            Log.d(TAG, "❌ Это НЕ ACTION_MEDIA_BUTTON: " + intent.getAction());

            // Проверяем другие возможные action'ы для медиа кнопок
            checkOtherMediaActions(intent);
        }
    }

    private String getMediaKeyName(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_MEDIA_PLAY:
                return "PLAY";
            case KeyEvent.KEYCODE_MEDIA_PAUSE:
                return "PAUSE";
            case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
                return "PLAY_PAUSE";
            case KeyEvent.KEYCODE_MEDIA_STOP:
                return "STOP";
            case KeyEvent.KEYCODE_MEDIA_NEXT:
                return "NEXT";
            case KeyEvent.KEYCODE_MEDIA_PREVIOUS:
                return "PREVIOUS";
            case KeyEvent.KEYCODE_MEDIA_REWIND:
                return "REWIND";
            case KeyEvent.KEYCODE_MEDIA_FAST_FORWARD:
                return "FAST_FORWARD";
            case KeyEvent.KEYCODE_VOLUME_UP:
                return "VOLUME_UP";
            case KeyEvent.KEYCODE_VOLUME_DOWN:
                return "VOLUME_DOWN";
            case KeyEvent.KEYCODE_VOLUME_MUTE:
                return "VOLUME_MUTE";
            case KeyEvent.KEYCODE_HEADSETHOOK:
                return "HEADSET_HOOK";
            default:
                return "UNKNOWN (" + keyCode + ")";
        }
    }
}
