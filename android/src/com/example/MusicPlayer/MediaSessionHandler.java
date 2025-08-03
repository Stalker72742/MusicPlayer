package com.example.MusicPlayer;

import android.content.Context;
import android.media.AudioManager;
import android.media.MediaMetadata;
import android.media.session.MediaSession;
import android.media.session.PlaybackState;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

public class MediaSessionHandler {
    private static final String TAG = "MediaSessionHandler";
    private MediaSession mediaSession;
    private Context context;
    private Handler mainHandler;

    // Native метод для отправки в C++
    public static native void onMediaButton(int keyCode, String action);

    public MediaSessionHandler(Context context) {
        this.context = context;

        // Создаем Handler для UI потока
        mainHandler = new Handler(Looper.getMainLooper());

        // Запускаем создание MediaSession в UI потоке
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                setupMediaSession();
            }
        });
    }

    private void setupMediaSession() {
        Log.d(TAG, "🎵 Создаем MediaSession...");

        // Создаем MediaSession
        mediaSession = new MediaSession(context, "MyAppMediaSession");

        // Устанавливаем callback для обработки медиа команд
        mediaSession.setCallback(new MediaSession.Callback() {
            @Override
            public void onPlay() {
                Log.d(TAG, "▶️ onPlay() вызван");
                onMediaButton(85, "PLAY"); // KEYCODE_MEDIA_PLAY
            }

            @Override
            public void onPause() {
                Log.d(TAG, "⏸️ onPause() вызван");
                onMediaButton(127, "PAUSE"); // KEYCODE_MEDIA_PAUSE
            }

            @Override
            public void onSkipToNext() {
                Log.d(TAG, "⏭️ onSkipToNext() вызван");
                onMediaButton(87, "NEXT"); // KEYCODE_MEDIA_NEXT
            }

            @Override
            public void onSkipToPrevious() {
                Log.d(TAG, "⏮️ onSkipToPrevious() вызван");
                onMediaButton(88, "PREVIOUS"); // KEYCODE_MEDIA_PREVIOUS
            }

            @Override
            public void onStop() {
                Log.d(TAG, "⏹️ onStop() вызван");
                onMediaButton(86, "STOP"); // KEYCODE_MEDIA_STOP
            }

            @Override
            public void onFastForward() {
                Log.d(TAG, "⏩ onFastForward() вызван");
                onMediaButton(90, "FAST_FORWARD"); // KEYCODE_MEDIA_FAST_FORWARD
            }

            @Override
            public void onRewind() {
                Log.d(TAG, "⏪ onRewind() вызван");
                onMediaButton(89, "REWIND"); // KEYCODE_MEDIA_REWIND
            }

            // Этот метод вызывается для кнопки play/pause на гарнитуре
            @Override
            public boolean onMediaButtonEvent(android.content.Intent mediaButtonEvent) {
                Log.d(TAG, "🎧 onMediaButtonEvent() вызван");
                Log.d(TAG, "📨 Intent: " + mediaButtonEvent.toString());

                // Пробуем получить KeyEvent
                android.view.KeyEvent keyEvent = mediaButtonEvent.getParcelableExtra(android.content.Intent.EXTRA_KEY_EVENT);
                if (keyEvent != null && keyEvent.getAction() == android.view.KeyEvent.ACTION_DOWN) {
                    int keyCode = keyEvent.getKeyCode();
                    Log.d(TAG, "🎯 KeyCode: " + keyCode);

                    switch (keyCode) {
                        case android.view.KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
                        case android.view.KeyEvent.KEYCODE_HEADSETHOOK:
                            Log.d(TAG, "🎵 Play/Pause или Headset Hook");
                            onMediaButton(keyCode, "PLAY_PAUSE");
                            return true;
                        case android.view.KeyEvent.KEYCODE_MEDIA_PLAY:
                            onMediaButton(keyCode, "PLAY");
                            return true;
                        case android.view.KeyEvent.KEYCODE_MEDIA_PAUSE:
                            onMediaButton(keyCode, "PAUSE");
                            return true;
                        case android.view.KeyEvent.KEYCODE_MEDIA_NEXT:
                            onMediaButton(keyCode, "NEXT");
                            return true;
                        case android.view.KeyEvent.KEYCODE_MEDIA_PREVIOUS:
                            onMediaButton(keyCode, "PREVIOUS");
                            return true;
                    }
                }

                // Если не обработали - вызываем базовую реализацию
                return super.onMediaButtonEvent(mediaButtonEvent);
            }
        });

        // ВАЖНО: Устанавливаем флаги для получения медиа кнопок
        mediaSession.setFlags(
            MediaSession.FLAG_HANDLES_MEDIA_BUTTONS |
            MediaSession.FLAG_HANDLES_TRANSPORT_CONTROLS
        );

        // Устанавливаем начальное состояние воспроизведения
        updatePlaybackState(PlaybackState.STATE_PAUSED);

        // Устанавливаем метаданные (это помогает системе понять, что мы медиа приложение)
        setDummyMetadata();

        // Активируем сессию
        mediaSession.setActive(true);

        Log.d(TAG, "✅ MediaSession настроен и активирован!");
    }

    private void updatePlaybackState(int state) {
        PlaybackState.Builder builder = new PlaybackState.Builder()
            .setActions(
                PlaybackState.ACTION_PLAY |
                PlaybackState.ACTION_PAUSE |
                PlaybackState.ACTION_SKIP_TO_NEXT |
                PlaybackState.ACTION_SKIP_TO_PREVIOUS |
                PlaybackState.ACTION_STOP |
                PlaybackState.ACTION_FAST_FORWARD |
                PlaybackState.ACTION_REWIND
            )
            .setState(state, 0, 1.0f);

        mediaSession.setPlaybackState(builder.build());
        Log.d(TAG, "🔄 PlaybackState обновлен: " + state);
    }

    private void setDummyMetadata() {
        MediaMetadata metadata = new MediaMetadata.Builder()
            .putString(MediaMetadata.METADATA_KEY_TITLE, "My App")
            .putString(MediaMetadata.METADATA_KEY_ARTIST, "Media Control")
            .putLong(MediaMetadata.METADATA_KEY_DURATION, 0)
            .build();

        mediaSession.setMetadata(metadata);
        Log.d(TAG, "📋 Метаданные установлены");
    }

    // Методы для управления состоянием (тоже выполняем в UI потоке)
    public void setPlaying() {
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                if (mediaSession != null) {
                    updatePlaybackState(PlaybackState.STATE_PLAYING);
                }
            }
        });
    }

    public void setPaused() {
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                if (mediaSession != null) {
                    updatePlaybackState(PlaybackState.STATE_PAUSED);
                }
            }
        });
    }

    public void setStopped() {
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                if (mediaSession != null) {
                    updatePlaybackState(PlaybackState.STATE_STOPPED);
                }
            }
        });
    }

    // Запрос аудио фокуса (важно для приоритета!)
    public void requestAudioFocus() {
        AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        if (audioManager != null) {
            int result = audioManager.requestAudioFocus(
                new AudioManager.OnAudioFocusChangeListener() {
                    @Override
                    public void onAudioFocusChange(int focusChange) {
                        Log.d(TAG, "🔊 Audio focus changed: " + focusChange);
                        switch (focusChange) {
                            case AudioManager.AUDIOFOCUS_GAIN:
                                Log.d(TAG, "✅ Получили аудио фокус");
                                break;
                            case AudioManager.AUDIOFOCUS_LOSS:
                                Log.d(TAG, "❌ Потеряли аудио фокус");
                                break;
                            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                                Log.d(TAG, "⏸️ Временно потеряли аудио фокус");
                                break;
                        }
                    }
                },
                AudioManager.STREAM_MUSIC,
                AudioManager.AUDIOFOCUS_GAIN
            );

            Log.d(TAG, "🎧 Запрос аудио фокуса: " + (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED ? "GRANTED" : "FAILED"));
        }
    }

    public void destroy() {
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                if (mediaSession != null) {
                    mediaSession.setActive(false);
                    mediaSession.release();
                    mediaSession = null;
                    Log.d(TAG, "🗑️ MediaSession уничтожен");
                }
            }
        });
    }
}
