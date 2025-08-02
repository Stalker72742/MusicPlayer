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
    private BroadcastReceiver testReceiver;
    private AudioManager audioManager;
    private MediaSession mediaSession;

    @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            Log.d(TAG, "=== MainActivity onCreate START ===");

            audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
            Log.d(TAG, "AudioManager получен: " + (audioManager != null));

            // ТЕСТ 1: Простой receiver для проверки работы системы
            testReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    Log.d(TAG, "🔥 ЛЮБОЕ СОБЫТИЕ ПОЛУЧЕНО!");
                    Log.d(TAG, "📨 Action: " + intent.getAction());
                    Log.d(TAG, "📦 Data: " + intent.getDataString());
                    Log.d(TAG, "🏷️ Categories: " + intent.getCategories());

                    // Если это медиа-кнопка
                    if (Intent.ACTION_MEDIA_BUTTON.equals(intent.getAction())) {
                        Log.d(TAG, "🎵 ЭТО МЕДИА КНОПКА!");
                        KeyEvent keyEvent = intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
                        if (keyEvent != null) {
                            Log.d(TAG, "🔑 KeyEvent найден: " + keyEvent.toString());
                            Log.d(TAG, "🔑 KeyCode: " + keyEvent.getKeyCode());
                            Log.d(TAG, "🔑 Action: " + keyEvent.getAction());
                        } else {
                            Log.d(TAG, "❌ KeyEvent = NULL!");
                        }
                    }

                    // Bluetooth события
                    if (intent.getAction().contains("bluetooth") || intent.getAction().contains("BLUETOOTH")) {
                        Log.d(TAG, "🔵 BLUETOOTH EVENT!");
                        int state = intent.getIntExtra(BluetoothProfile.EXTRA_STATE, -1);
                        Log.d(TAG, "🔵 Bluetooth state: " + state);
                    }
                }
            };

            // Регистрируем НА ВСЕ возможные события
            IntentFilter filter = new IntentFilter();

            // Медиа события
            filter.addAction(Intent.ACTION_MEDIA_BUTTON);
            filter.addAction("android.intent.action.MEDIA_BUTTON");

            // Наушники
            filter.addAction(Intent.ACTION_HEADSET_PLUG);
            filter.addAction("android.intent.action.HEADSET_PLUG");

            // Bluetooth события
            filter.addAction(BluetoothAdapter.ACTION_CONNECTION_STATE_CHANGED);
            filter.addAction(BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED);
            filter.addAction("android.bluetooth.headset.profile.action.CONNECTION_STATE_CHANGED");
            filter.addAction("android.bluetooth.a2dp.profile.action.CONNECTION_STATE_CHANGED");

            // Системные медиа события
            filter.addAction("com.android.music.metachanged");
            filter.addAction("com.android.music.playstatechanged");
            filter.addAction("android.media.AUDIO_BECOMING_NOISY");

            // Другие возможные события
            filter.addAction(Intent.ACTION_SCREEN_ON);
            filter.addAction(Intent.ACTION_SCREEN_OFF);
            filter.addAction(Intent.ACTION_USER_PRESENT);

            // Максимальный приоритет
            filter.setPriority(IntentFilter.SYSTEM_HIGH_PRIORITY);

            try {
                registerReceiver(testReceiver, filter, Context.RECEIVER_EXPORTED);
                Log.d(TAG, "✅ TestReceiver зарегистрирован УСПЕШНО");
            } catch (Exception e) {
                Log.e(TAG, "❌ ОШИБКА регистрации receiver: " + e.getMessage());
                e.printStackTrace();
            }

            // ТЕСТ 2: Регистрируем через AudioManager (старый метод)
            ComponentName mediaButtonReceiver = new ComponentName(this, MediaButtonReceiver.class);
            try {
                audioManager.registerMediaButtonEventReceiver(mediaButtonReceiver);
                Log.d(TAG, "✅ AudioManager.registerMediaButtonEventReceiver УСПЕШНО");
            } catch (Exception e) {
                Log.e(TAG, "❌ ОШИБКА AudioManager.registerMediaButtonEventReceiver: " + e.getMessage());
            }

            // ТЕСТ 3: Проверим состояние аудио
            Log.d(TAG, "🎧 Headset подключен: " + audioManager.isWiredHeadsetOn());
            Log.d(TAG, "🎧 Bluetooth подключен: " + audioManager.isBluetoothA2dpOn());
            Log.d(TAG, "🎧 Bluetooth SCO включен: " + audioManager.isBluetoothScoOn());
            Log.d(TAG, "🔊 Режим аудио: " + audioManager.getMode());

            // Дополнительная проверка Bluetooth
            BluetoothAdapter bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
            if (bluetoothAdapter != null) {
                Log.d(TAG, "🔵 Bluetooth адаптер доступен: " + bluetoothAdapter.isEnabled());
                Log.d(TAG, "🔵 Bluetooth state: " + bluetoothAdapter.getState());
            }

            // ТЕСТ 4: Попробуем получить аудио фокус
            int result = audioManager.requestAudioFocus(
                new AudioManager.OnAudioFocusChangeListener() {
                    @Override
                    public void onAudioFocusChange(int focusChange) {
                        Log.d(TAG, "🎵 Audio focus изменился: " + focusChange);
                    }
                },
                AudioManager.STREAM_MUSIC,
                AudioManager.AUDIOFOCUS_GAIN
            );
            Log.d(TAG, "🎵 Audio focus запрошен, результат: " + result);

            // ТЕСТ 5: Создаем MediaSession для современных версий Android
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                setupMediaSession();
            }

            // ТЕСТ 6: Имитируем что мы медиа-приложение
            simulateMediaApp();

            Log.d(TAG, "=== MainActivity onCreate END ===");
        }

        // Переопределяем методы активности для отслеживания
        @Override
        protected void onResume() {
            super.onResume();
            Log.d(TAG, "📱 onResume вызван");
        }

        @Override
        protected void onPause() {
            super.onPause();
            Log.d(TAG, "📱 onPause вызван");
        }

        @Override
        public boolean onKeyDown(int keyCode, KeyEvent event) {
            Log.d(TAG, "🔑 onKeyDown: " + keyCode + ", event: " + event.toString());

            // Проверяем медиа-кнопки
            switch (keyCode) {
                case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
                case KeyEvent.KEYCODE_MEDIA_NEXT:
                case KeyEvent.KEYCODE_MEDIA_PREVIOUS:
                case KeyEvent.KEYCODE_HEADSETHOOK:
                    Log.d(TAG, "🎵 МЕДИА КНОПКА через onKeyDown!");
                    return true;
            }

            return super.onKeyDown(keyCode, event);
        }

        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        private void setupMediaSession() {
            Log.d(TAG, "🎵 Настройка MediaSession...");

            try {
                mediaSession = new MediaSession(this, "MyMediaSession");

                mediaSession.setCallback(new MediaSession.Callback() {
                    @Override
                    public boolean onMediaButtonEvent(Intent mediaButtonEvent) {
                        Log.d(TAG, "🔥 MediaSession.onMediaButtonEvent ВЫЗВАН!");
                        KeyEvent keyEvent = mediaButtonEvent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
                        if (keyEvent != null && keyEvent.getAction() == KeyEvent.ACTION_DOWN) {
                            Log.d(TAG, "🎵 MediaSession получил кнопку: " + keyEvent.getKeyCode());
                            return true;
                        }
                        return false;
                    }

                    @Override
                    public void onPlay() {
                        Log.d(TAG, "🎵 MediaSession.onPlay()");
                    }

                    @Override
                    public void onPause() {
                        Log.d(TAG, "🎵 MediaSession.onPause()");
                    }
                });

                mediaSession.setFlags(MediaSession.FLAG_HANDLES_MEDIA_BUTTONS |
                                     MediaSession.FLAG_HANDLES_TRANSPORT_CONTROLS);

                // Устанавливаем состояние плеера
                PlaybackState state = new PlaybackState.Builder()
                    .setActions(PlaybackState.ACTION_PLAY | PlaybackState.ACTION_PAUSE |
                               PlaybackState.ACTION_SKIP_TO_NEXT | PlaybackState.ACTION_SKIP_TO_PREVIOUS)
                    .setState(PlaybackState.STATE_PAUSED, 0, 1.0f)
                    .build();
                mediaSession.setPlaybackState(state);

                // Устанавливаем метаданные
                MediaMetadata metadata = new MediaMetadata.Builder()
                    .putString(MediaMetadata.METADATA_KEY_TITLE, "Test Track")
                    .putString(MediaMetadata.METADATA_KEY_ARTIST, "Test Artist")
                    .build();
                mediaSession.setMetadata(metadata);

                mediaSession.setActive(true);
                Log.d(TAG, "✅ MediaSession активирована успешно");

            } catch (Exception e) {
                Log.e(TAG, "❌ Ошибка создания MediaSession: " + e.getMessage());
                e.printStackTrace();
            }
        }

        private void simulateMediaApp() {
            Log.d(TAG, "🎵 Имитация медиа-приложения...");

            // Создаем уведомление как медиа-плеер
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                NotificationManager notificationManager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);

                NotificationChannel channel = new NotificationChannel(
                    "media_channel",
                    "Media Player",
                    NotificationManager.IMPORTANCE_LOW
                );
                notificationManager.createNotificationChannel(channel);

                Notification notification = new Notification.Builder(this, "media_channel")
                    .setContentTitle("Test Media Player")
                    .setContentText("Listening for media buttons...")
                    .setSmallIcon(android.R.drawable.ic_media_play)
                    .setOngoing(true)
                    .build();

                notificationManager.notify(1, notification);
                Log.d(TAG, "✅ Media notification создано");
            }
        }

        @Override
        protected void onDestroy() {
            Log.d(TAG, "📱 onDestroy вызван");

            if (testReceiver != null) {
                try {
                    unregisterReceiver(testReceiver);
                    Log.d(TAG, "✅ TestReceiver отрегистрирован");
                } catch (Exception e) {
                    Log.e(TAG, "❌ ОШИБКА отрегистрации receiver: " + e.getMessage());
                }
            }

            if (audioManager != null) {
                ComponentName mediaButtonReceiver = new ComponentName(this, MediaButtonReceiver.class);
                try {
                    audioManager.unregisterMediaButtonEventReceiver(mediaButtonReceiver);
                    Log.d(TAG, "✅ AudioManager receiver отрегистрирован");
                } catch (Exception e) {
                    Log.e(TAG, "❌ ОШИБКА отрегистрации AudioManager: " + e.getMessage());
                }
            }

            if (mediaSession != null) {
                try {
                    mediaSession.setActive(false);
                    mediaSession.release();
                    Log.d(TAG, "✅ MediaSession освобождена");
                } catch (Exception e) {
                    Log.e(TAG, "❌ ОШИБКА освобождения MediaSession: " + e.getMessage());
                }
            }

            super.onDestroy();
        }

    public static native void onMediaKeyPressed(int keyCode);
    private native void sendToQt(String action);
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
