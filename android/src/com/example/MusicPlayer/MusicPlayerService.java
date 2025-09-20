package com.example.MusicPlayer;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.AudioManager;
import android.media.MediaMetadata;
import android.media.MediaPlayer;
import android.media.session.MediaSession;
import android.media.session.PlaybackState;
import android.net.Uri;
import android.os.Binder;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.support.v4.media.session.MediaSessionCompat;
import android.util.Log;
import android.widget.RemoteViews;
import androidx.core.app.NotificationCompat;
import java.io.IOException;

public class MusicPlayerService extends Service {
    private static final String TAG = "MusicPlayerService";
    private static final String CHANNEL_ID = "music_player_channel";
    private static final int NOTIFICATION_ID = 1001;

    // Actions for notification buttons
    private static final String ACTION_PLAY_PAUSE = "com.example.ACTION_PLAY_PAUSE";
    private static final String ACTION_NEXT = "com.example.ACTION_NEXT";
    private static final String ACTION_PREVIOUS = "com.example.ACTION_PREVIOUS";
    private static final String ACTION_STOP = "com.example.ACTION_STOP";

    // MediaPlayer
    private MediaPlayer mediaPlayer;
    private boolean isPrepared = false;

    // MediaSession
    private MediaSession mediaSession;
    private Handler mainHandler;

    // Notification
    private NotificationManager notificationManager;

    // Current song info
    private String currentSongTitle = "Unknown Title";
    private String currentArtist = "Unknown Artist";
    private Bitmap albumArt;

    // Playback state
    private int currentState = STATE_IDLE;

    // Native callback methods
    public static native void onPlaybackStateChanged(int state);
    public static native void onSongFinished();
    public static native void onError(String error);
    public static native void onMediaButton(int keyCode, String action);

    // Player states
    public static final int STATE_IDLE = 0;
    public static final int STATE_PLAYING = 1;
    public static final int STATE_PAUSED = 2;
    public static final int STATE_STOPPED = 3;

    // Binder for service connection
    public class MusicPlayerBinder extends Binder {
        public MusicPlayerService getService() {
            return MusicPlayerService.this;
        }
    }

    private final IBinder binder = new MusicPlayerBinder();

    @Override
    public void onCreate() {
        super.onCreate();
        Log.d(TAG, "🎵 MusicPlayerService создается...");

        mainHandler = new Handler(Looper.getMainLooper());
        notificationManager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);

        createNotificationChannel();
        initMediaPlayer();
        setupMediaSession();
        requestAudioFocus();

        Log.d(TAG, "✅ MusicPlayerService создан успешно!");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && intent.getAction() != null) {
            handleNotificationAction(intent.getAction());
        }

        Log.d(TAG, "onStartCommand called");


        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return binder;
    }

    private void createNotificationChannel() {
        NotificationChannel channel = new NotificationChannel(
            CHANNEL_ID,
            "Music Player",
            NotificationManager.IMPORTANCE_LOW
        );
        channel.setDescription("Music playback controls");
        channel.setShowBadge(false);
        channel.setSound(null, null);
        notificationManager.createNotificationChannel(channel);
    }

    private void initMediaPlayer() {
        mediaPlayer = new MediaPlayer();

        mediaPlayer.setOnPreparedListener(mp -> {
            isPrepared = true;
            Log.d(TAG, "MediaPlayer prepared");
            mp.start();
            updateState(STATE_PLAYING);
        });

        mediaPlayer.setOnCompletionListener(mp -> {
            Log.d(TAG, "Song completed");
            updateState(STATE_STOPPED);
            onSongFinished();
        });

        mediaPlayer.setOnErrorListener((mp, what, extra) -> {
            String errorMsg = "MediaPlayer error: what=" + what + ", extra=" + extra;
            Log.e(TAG, errorMsg);
            updateState(STATE_STOPPED);
            onError(errorMsg);
            return true;
        });
    }

    private void setupMediaSession() {
        mediaSession = new MediaSession(this, "MusicPlayerService");

        mediaSession.setCallback(new MediaSession.Callback() {
            @Override
            public void onPlay() {
                Log.d(TAG, "▶️ MediaSession onPlay()");
                resume();
                onMediaButton(85, "PLAY");
            }

            @Override
            public void onPause() {
                Log.d(TAG, "⏸️ MediaSession onPause()");
                pause();
                onMediaButton(127, "PAUSE");
            }

            @Override
            public void onSkipToNext() {
                Log.d(TAG, "⏭️ MediaSession onSkipToNext()");
                onMediaButton(87, "NEXT");
            }

            @Override
            public void onSkipToPrevious() {
                Log.d(TAG, "⏮️ MediaSession onSkipToPrevious()");
                onMediaButton(88, "PREVIOUS");
            }

            @Override
            public void onStop() {
                Log.d(TAG, "⏹️ MediaSession onStop()");
                stop();
                onMediaButton(86, "STOP");
            }

            @Override
            public boolean onMediaButtonEvent(Intent mediaButtonEvent) {
                android.view.KeyEvent keyEvent = mediaButtonEvent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
                if (keyEvent != null && keyEvent.getAction() == android.view.KeyEvent.ACTION_DOWN) {
                    int keyCode = keyEvent.getKeyCode();
                    switch (keyCode) {
                        case android.view.KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
                        case android.view.KeyEvent.KEYCODE_HEADSETHOOK:
                            if (isPlaying()) {
                                pause();
                            } else {
                                resume();
                            }
                            onMediaButton(keyCode, "PLAY_PAUSE");
                            return true;
                    }
                }
                return super.onMediaButtonEvent(mediaButtonEvent);
            }
        });

        mediaSession.setFlags(
            MediaSession.FLAG_HANDLES_MEDIA_BUTTONS |
            MediaSession.FLAG_HANDLES_TRANSPORT_CONTROLS
        );

        updateMediaSessionPlaybackState(PlaybackState.STATE_PAUSED);
        mediaSession.setActive(true);
    }

    private void handleNotificationAction(String action) {
        Log.d(TAG, "🎯 Handling notification action: " + action);

        switch (action) {
            case ACTION_PLAY_PAUSE:
                if (isPlaying()) {
                    pause();
                } else {
                    resume();
                }
                break;
            case ACTION_NEXT:
                onMediaButton(87, "NEXT");
                break;
            case ACTION_PREVIOUS:
                onMediaButton(88, "PREVIOUS");
                break;
            case ACTION_STOP:
                stop();
                break;
        }
    }

    private void showNotification() {

            RemoteViews notificationView = new RemoteViews(getPackageName(), R.layout.notification_music_player);

                            // Set song info
                           /*  notificationView.setTextViewText(R.id.notification_title, currentSongTitle);
                            notificationView.setTextViewText(R.id.notification_artist, currentArtist);

                            // Set album art
                            if (albumArt != null) {
                                notificationView.setImageViewBitmap(R.id.notification_album_art, albumArt);
                            } */

                            // Set play/pause button icon
                            int playPauseIcon = isPlaying() ? R.drawable.ic_pause : R.drawable.ic_play;
                            //notificationView.setImageViewResource(R.id.notification_play_pause, playPauseIcon);

                            // Create PendingIntents for buttons
                            //PendingIntent playPauseIntent = createActionIntent(ACTION_PLAY_PAUSE);
                            //PendingIntent nextIntent = createActionIntent(ACTION_NEXT);
                            //PendingIntent previousIntent = createActionIntent(ACTION_PREVIOUS);
                            //PendingIntent stopIntent = createActionIntent(ACTION_STOP);

                            // Set click listeners
                           // notificationView.setOnClickPendingIntent(R.id.notification_play_pause, playPauseIntent);
                            //notificationView.setOnClickPendingIntent(R.id.notification_next, nextIntent);
                           // notificationView.setOnClickPendingIntent(R.id.notification_previous, previousIntent);

                            // Build notification
                            NotificationCompat.Builder builder = new NotificationCompat.Builder(this, CHANNEL_ID)
                                .setSmallIcon(R.drawable.ic_music_note)
                                .setCustomContentView(notificationView)
                                .setStyle(new NotificationCompat.DecoratedCustomViewStyle())
                                .setOngoing(true) // Делает уведомление постоянным
                                .setAutoCancel(false)
                                .setPriority(NotificationCompat.PRIORITY_LOW)
                                .setVisibility(NotificationCompat.VISIBILITY_PUBLIC);

                            // Show notification as foreground service
                            startForeground(NOTIFICATION_ID, builder.build());
    }

    private PendingIntent createActionIntent(String action) {
        Intent intent = new Intent(this, MusicPlayerService.class);
        intent.setAction(action);

        int flags = PendingIntent.FLAG_UPDATE_CURRENT;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            flags |= PendingIntent.FLAG_IMMUTABLE;
        }

        return PendingIntent.getService(this, action.hashCode(), intent, flags);
    }

    private void requestAudioFocus() {
        AudioManager audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        if (audioManager != null) {
            int result = audioManager.requestAudioFocus(
                focusChange -> {
                    Log.d(TAG, "🔊 Audio focus changed: " + focusChange);
                    switch (focusChange) {
                        case AudioManager.AUDIOFOCUS_LOSS:
                            pause();
                            break;
                        case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                            pause();
                            break;
                        case AudioManager.AUDIOFOCUS_GAIN:
                            // Можно возобновить воспроизведение
                            break;
                    }
                },
                AudioManager.STREAM_MUSIC,
                AudioManager.AUDIOFOCUS_GAIN
            );
            Log.d(TAG, "🎧 Audio focus request: " + (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED ? "GRANTED" : "FAILED"));
        }
    }

    private void updateState(int newState) {
        currentState = newState;

        // Update MediaSession
        int sessionState;
        switch (newState) {
            case STATE_PLAYING:
                sessionState = PlaybackState.STATE_PLAYING;
                break;
            case STATE_PAUSED:
                sessionState = PlaybackState.STATE_PAUSED;
                break;
            case STATE_STOPPED:
                sessionState = PlaybackState.STATE_STOPPED;
                break;
            default:
                sessionState = PlaybackState.STATE_NONE;
        }
        updateMediaSessionPlaybackState(sessionState);

        // Update notification
        showNotification();

        // Notify native code
        onPlaybackStateChanged(newState);
    }

    private void updateMediaSessionPlaybackState(int state) {
        PlaybackState.Builder builder = new PlaybackState.Builder()
            .setActions(
                PlaybackState.ACTION_PLAY |
                PlaybackState.ACTION_PAUSE |
                PlaybackState.ACTION_SKIP_TO_NEXT |
                PlaybackState.ACTION_SKIP_TO_PREVIOUS |
                PlaybackState.ACTION_STOP
            )
            .setState(state, getCurrentPosition(), 1.0f);

        mediaSession.setPlaybackState(builder.build());
    }

    // Public methods for controlling playback
    public void playSong(String songPath) {
        try {
            if (mediaPlayer.isPlaying()) {
                mediaPlayer.stop();
            }

            mediaPlayer.reset();
            isPrepared = false;

            if (songPath.startsWith("http")) {
                mediaPlayer.setDataSource(songPath);
            } else {
                Uri uri = Uri.parse(songPath);
                mediaPlayer.setDataSource(this, uri);
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
            updateState(STATE_PAUSED);
            Log.d(TAG, "Playback paused");
        }
    }

    public void resume() {
        if (isPrepared && !mediaPlayer.isPlaying()) {
            mediaPlayer.start();
            updateState(STATE_PLAYING);
            Log.d(TAG, "Playback resumed");
        }
    }

    public void stop() {
        if (mediaPlayer.isPlaying()) {
            mediaPlayer.stop();
        }
        updateState(STATE_STOPPED);
        Log.d(TAG, "Playback stopped");
    }

    public void setVolume(float volume) {
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

    public void setSongInfo(String title, String artist, byte[] albumArtBytes) {
        currentSongTitle = title;
        currentArtist = artist;

        if (albumArtBytes != null && albumArtBytes.length > 0) {
            albumArt = BitmapFactory.decodeByteArray(albumArtBytes, 0, albumArtBytes.length);
        }

        // Update MediaSession metadata
        MediaMetadata metadata = new MediaMetadata.Builder()
            .putString(MediaMetadata.METADATA_KEY_TITLE, title)
            .putString(MediaMetadata.METADATA_KEY_ARTIST, artist)
            .putBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART, albumArt)
            .putLong(MediaMetadata.METADATA_KEY_DURATION, getDuration())
            .build();

        mediaSession.setMetadata(metadata);

        // Update notification
        showNotification();
    }

    @Override
    public void onDestroy() {
        Log.d(TAG, "🗑️ MusicPlayerService уничтожается...");

        if (mediaPlayer != null) {
            mediaPlayer.release();
            mediaPlayer = null;
        }

        if (mediaSession != null) {
            mediaSession.setActive(false);
            mediaSession.release();
            mediaSession = null;
        }

        stopForeground(true);
        super.onDestroy();

        Log.d(TAG, "✅ MusicPlayerService уничтожен");
    }
}
