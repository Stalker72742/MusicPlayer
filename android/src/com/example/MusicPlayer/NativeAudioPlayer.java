package com.example.MusicPlayer;

import android.content.Context;
import android.media.MediaPlayer;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import java.io.IOException;

public class NativeAudioPlayer implements
    MediaPlayer.OnPreparedListener,
    MediaPlayer.OnErrorListener,
    MediaPlayer.OnCompletionListener,
    MediaPlayer.OnBufferingUpdateListener {

    private static final String TAG = "NativeAudioPlayer";

    // Константы для callback'ов
    public static final int MESSAGE_STATE_CHANGED = 0;
    public static final int MESSAGE_ERROR = 1;
    public static final int MESSAGE_END_OF_MEDIA = 2;
    public static final int MESSAGE_BUFFERING = 3;

    // Состояния плеера
    public static final int STATE_IDLE = 0;
    public static final int STATE_PREPARING = 1;
    public static final int STATE_PREPARED = 2;
    public static final int STATE_PLAYING = 3;
    public static final int STATE_PAUSED = 4;
    public static final int STATE_STOPPED = 5;
    public static final int STATE_ERROR = 6;

    private Context context;
    private MediaPlayer mediaPlayer;
    private AudioManager audioManager;
    private long nativePtr; // Указатель на C++ объект

    private int currentState = STATE_IDLE;
    private String currentSource = "";
    private boolean isPrepared = false;

    // Handler для UI thread
    private Handler mainHandler;

    public NativeAudioPlayer(Context context, long nativePtr) {
        this.context = context;
        this.nativePtr = nativePtr;
        this.mainHandler = new Handler(Looper.getMainLooper());

        audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);

        initializeMediaPlayer();

        Log.d(TAG, "NativeAudioPlayer created with native pointer: " + nativePtr);
    }

    private void initializeMediaPlayer() {
        try {
            mediaPlayer = new MediaPlayer();

            // Настройка listeners
            mediaPlayer.setOnPreparedListener(this);
            mediaPlayer.setOnErrorListener(this);
            mediaPlayer.setOnCompletionListener(this);
            mediaPlayer.setOnBufferingUpdateListener(this);

            // Настройка аудио
            mediaPlayer.setAudioStreamType(AudioManager.STREAM_MUSIC);

            setState(STATE_IDLE);

        } catch (Exception e) {
            Log.e(TAG, "Failed to initialize MediaPlayer", e);
            setState(STATE_ERROR);
        }
    }

    public boolean setSource(String source) {
        Log.d(TAG, "Setting source: " + source);

        try {
            if (mediaPlayer == null) {
                initializeMediaPlayer();
            }

            // Сброс предыдущего источника
            if (currentState != STATE_IDLE) {
                mediaPlayer.reset();
                setState(STATE_IDLE);
            }

            currentSource = source;
            isPrepared = false;

            // Установка источника
            if (source.startsWith("http://") || source.startsWith("https://")) {
                mediaPlayer.setDataSource(source);
            } else {
                mediaPlayer.setDataSource(context, Uri.parse(source));
            }

            setState(STATE_PREPARING);

            // Асинхронная подготовка
            mediaPlayer.prepareAsync();

            return true;

        } catch (IOException e) {
            Log.e(TAG, "Failed to set source: " + source, e);
            setState(STATE_ERROR);
            sendNativeCallback(MESSAGE_ERROR, "Failed to set source: " + e.getMessage());
            return false;
        } catch (Exception e) {
            Log.e(TAG, "Unexpected error setting source", e);
            setState(STATE_ERROR);
            sendNativeCallback(MESSAGE_ERROR, "Unexpected error: " + e.getMessage());
            return false;
        }
    }

    public boolean play() {
        try {
            if (mediaPlayer == null) {
                Log.w(TAG, "MediaPlayer is null");
                return false;
            }

            if (!isPrepared) {
                Log.w(TAG, "MediaPlayer not prepared");
                return false;
            }

            if (currentState == STATE_PREPARED || currentState == STATE_PAUSED) {
                mediaPlayer.start();
                setState(STATE_PLAYING);
                return true;
            }

            Log.w(TAG, "Cannot play in current state: " + currentState);
            return false;

        } catch (Exception e) {
            Log.e(TAG, "Failed to play", e);
            setState(STATE_ERROR);
            sendNativeCallback(MESSAGE_ERROR, "Failed to play: " + e.getMessage());
            return false;
        }
    }

    public boolean pause() {
        try {
            if (mediaPlayer == null || currentState != STATE_PLAYING) {
                return false;
            }

            mediaPlayer.pause();
            setState(STATE_PAUSED);
            return true;

        } catch (Exception e) {
            Log.e(TAG, "Failed to pause", e);
            setState(STATE_ERROR);
            return false;
        }
    }

    public boolean stop() {
        try {
            if (mediaPlayer == null) {
                return false;
            }

            if (currentState == STATE_PLAYING || currentState == STATE_PAUSED) {
                mediaPlayer.stop();
                setState(STATE_STOPPED);
            }

            return true;

        } catch (Exception e) {
            Log.e(TAG, "Failed to stop", e);
            setState(STATE_ERROR);
            return false;
        }
    }

    public boolean setVolume(float volume) {
        try {
            if (mediaPlayer == null) {
                return false;
            }

            // Ограничиваем значение от 0 до 1
            volume = Math.max(0.0f, Math.min(1.0f, volume));
            mediaPlayer.setVolume(volume, volume);

            Log.d(TAG, "Volume set to: " + volume);
            return true;

        } catch (Exception e) {
            Log.e(TAG, "Failed to set volume", e);
            return false;
        }
    }

    public float getVolume() {
        // MediaPlayer не предоставляет метод получения громкости
        // Возвращаем системную громкость как приближение
        if (audioManager != null) {
            int currentVolume = audioManager.getStreamVolume(AudioManager.STREAM_MUSIC);
            int maxVolume = audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
            return (float) currentVolume / maxVolume;
        }
        return 1.0f;
    }

    public boolean seekTo(long positionMs) {
        try {
            if (mediaPlayer == null || !isPrepared) {
                return false;
            }

            mediaPlayer.seekTo((int) positionMs);
            Log.d(TAG, "Seeked to: " + positionMs + "ms");
            return true;

        } catch (Exception e) {
            Log.e(TAG, "Failed to seek", e);
            return false;
        }
    }

    public long getCurrentPosition() {
        try {
            if (mediaPlayer != null && isPrepared) {
                return mediaPlayer.getCurrentPosition();
            }
        } catch (Exception e) {
            Log.w(TAG, "Failed to get current position", e);
        }
        return 0;
    }

    public long getDuration() {
        try {
            if (mediaPlayer != null && isPrepared) {
                return mediaPlayer.getDuration();
            }
        } catch (Exception e) {
            Log.w(TAG, "Failed to get duration", e);
        }
        return 0;
    }

    public int getState() {
        return currentState;
    }

    public void cleanup() {
        Log.d(TAG, "Cleaning up NativeAudioPlayer");

        try {
            if (mediaPlayer != null) {
                if (currentState == STATE_PLAYING || currentState == STATE_PAUSED) {
                    mediaPlayer.stop();
                }
                mediaPlayer.release();
                mediaPlayer = null;
            }

            setState(STATE_IDLE);

        } catch (Exception e) {
            Log.e(TAG, "Error during cleanup", e);
        }
    }

    // MediaPlayer.OnPreparedListener
    @Override
    public void onPrepared(MediaPlayer mp) {
        Log.d(TAG, "MediaPlayer prepared");
        isPrepared = true;
        setState(STATE_PREPARED);

        // Автоматически начинаем воспроизведение после подготовки
        // (можно изменить это поведение при необходимости)
    }

    // MediaPlayer.OnErrorListener
    @Override
    public boolean onError(MediaPlayer mp, int what, int extra) {
        String errorMsg = "MediaPlayer error: what=" + what + ", extra=" + extra;
        Log.e(TAG, errorMsg);

        setState(STATE_ERROR);
        sendNativeCallback(MESSAGE_ERROR, errorMsg);

        return true; // Мы обработали ошибку
    }

    // MediaPlayer.OnCompletionListener
    @Override
    public void onCompletion(MediaPlayer mp) {
        Log.d(TAG, "MediaPlayer completed");
        setState(STATE_STOPPED);
        sendNativeCallback(MESSAGE_END_OF_MEDIA, "");
    }

    // MediaPlayer.OnBufferingUpdateListener
    @Override
    public void onBufferingUpdate(MediaPlayer mp, int percent) {
        // Можно использовать для показа прогресса буферизации
        sendNativeCallback(MESSAGE_BUFFERING, String.valueOf(percent));
    }

    private void setState(int newState) {
        if (currentState != newState) {
            Log.d(TAG, "State changed: " + currentState + " -> " + newState);
            currentState = newState;
            sendNativeCallback(MESSAGE_STATE_CHANGED, String.valueOf(newState));
        }
    }

    private void sendNativeCallback(final int messageType, final String data) {
        // Вызываем callback в main thread для безопасности
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                if (nativePtr != 0) {
                    nativeCallback(nativePtr, messageType, data);
                }
            }
        });
    }

    // Native method - объявляется в C++
    private native void nativeCallback(long nativePtr, int messageType, String data);

    // Вспомогательные методы для диагностики
    public String getStateString() {
        switch (currentState) {
            case STATE_IDLE: return "IDLE";
            case STATE_PREPARING: return "PREPARING";
            case STATE_PREPARED: return "PREPARED";
            case STATE_PLAYING: return "PLAYING";
            case STATE_PAUSED: return "PAUSED";
            case STATE_STOPPED: return "STOPPED";
            case STATE_ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }

    public boolean isPlaying() {
        return currentState == STATE_PLAYING;
    }

    public boolean isPrepared() {
        return isPrepared;
    }

    public String getCurrentSource() {
        return currentSource;
    }
}
