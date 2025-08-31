package com.example.MusicPlayer;

import android.content.Context;
import android.media.MediaPlayer;
import android.net.Uri;
import android.util.Log;
import java.io.IOException;

public class JavaMusicPlayer {
    private static final String TAG = "JavaMusicPlayer";

    private MediaPlayer mediaPlayer;
    private Context context;
    private boolean isPrepared = false;

    // Нативные методы для колбэков в C++
    public static native void onPlaybackStateChanged(int state);
    public static native void onSongFinished();
    public static native void onError(String error);

    // Состояния плеера
    public static final int STATE_IDLE = 0;
    public static final int STATE_PLAYING = 1;
    public static final int STATE_PAUSED = 2;
    public static final int STATE_STOPPED = 3;

    public JavaMusicPlayer(Context context) {
        this.context = context;
        initMediaPlayer();
    }

    private void initMediaPlayer() {
        mediaPlayer = new MediaPlayer();

        mediaPlayer.setOnPreparedListener(mp -> {
            isPrepared = true;
            Log.d(TAG, "MediaPlayer prepared");
            mp.start();
            onPlaybackStateChanged(STATE_PLAYING);
        });

        mediaPlayer.setOnCompletionListener(mp -> {
            Log.d(TAG, "Song completed");
            onSongFinished();
        });

        mediaPlayer.setOnErrorListener((mp, what, extra) -> {
            String errorMsg = "MediaPlayer error: what=" + what + ", extra=" + extra;
            Log.e(TAG, errorMsg);
            onError(errorMsg);
            return true;
        });
    }

    public void playSong(String songPath) {
        try {
            if (mediaPlayer.isPlaying()) {
                mediaPlayer.stop();
            }

            mediaPlayer.reset();
            isPrepared = false;

            // Определяем тип источника
            if (songPath.startsWith("http")) {
                // URL источник
                mediaPlayer.setDataSource(songPath);
            } else {
                // Локальный файл
                Uri uri = Uri.parse(songPath);
                mediaPlayer.setDataSource(context, uri);
            }

            mediaPlayer.prepareAsync();
            Log.d(TAG, "Started preparing: " + songPath);

        } catch (IOException e) {
            Log.e(TAG, "Error setting data source: " + e.getMessage());
            onError("Failed to load song: " + e.getMessage());
        }
    }

    public void pause() {
        if (mediaPlayer.isPlaying()) {
            mediaPlayer.pause();
            onPlaybackStateChanged(STATE_PAUSED);
            Log.d(TAG, "Playback paused");
        }
    }

    public void resume() {
        if (isPrepared && !mediaPlayer.isPlaying()) {
            mediaPlayer.start();
            onPlaybackStateChanged(STATE_PLAYING);
            Log.d(TAG, "Playback resumed");
        }
    }

    public void stop() {
        if (mediaPlayer.isPlaying()) {
            mediaPlayer.stop();
        }
        onPlaybackStateChanged(STATE_STOPPED);
        Log.d(TAG, "Playback stopped");
    }

    public void setVolume(float volume) {
        // volume должен быть от 0.0 до 1.0
        mediaPlayer.setVolume(volume, volume);
        Log.d(TAG, "Volume set to: " + volume);
    }

    public long getCurrentPosition() {
        return mediaPlayer.getCurrentPosition();
    }

    public long getDuration() {
        return mediaPlayer.getDuration();
    }

    public boolean isPlaying() {
        return mediaPlayer.isPlaying();
    }

    public void seekTo(int position) {
        if (isPrepared) {
            mediaPlayer.seekTo(position);
        }
    }

    public void release() {
        if (mediaPlayer != null) {
            mediaPlayer.release();
            mediaPlayer = null;
        }
    }
}
