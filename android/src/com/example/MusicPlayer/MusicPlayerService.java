package com.example.MusicPlayer;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.ComponentName;
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
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.support.v4.media.MediaBrowserCompat;
import android.support.v4.media.MediaDescriptionCompat;
import android.support.v4.media.MediaMetadataCompat;
import android.support.v4.media.session.MediaSessionCompat;
import android.support.v4.media.session.PlaybackStateCompat;
import android.util.Log;
import android.view.KeyEvent;
import androidx.core.app.NotificationCompat;
import androidx.media.MediaBrowserServiceCompat;
import androidx.media.session.MediaButtonReceiver;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class MusicPlayerService extends MediaBrowserServiceCompat {
    private static final String TAG = "MusicPlayerService";
    private static final String CHANNEL_ID = "music_player_channel";
    private static final int NOTIFICATION_ID = 1001;

    // MediaBrowser root ID
    private static final String MEDIA_ROOT_ID = "root_id";
    private static final String EMPTY_MEDIA_ROOT_ID = "empty_root_id";

    // MediaPlayer
    private MediaPlayer mediaPlayer;
    private boolean isPrepared = false;

    // MediaSession (using support library for better compatibility)
    private MediaSessionCompat mediaSession;
    private Handler mainHandler;

    // Notification
    private NotificationManager notificationManager;

    // Current song info
    private String currentSongTitle = "Unknown Title";
    private String currentArtist = "Unknown Artist";
    private String currentAlbum = "Unknown Album";
    private Bitmap albumArt;
    private long currentDuration = 0;

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

        showNotification();

        Log.d(TAG, "✅ MusicPlayerService создан успешно!");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // Handle media button events
        MediaButtonReceiver.handleIntent(mediaSession, intent);
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return binder;
    }

    // MediaBrowserService methods for widget integration
    @Override
    public BrowserRoot onGetRoot(String clientPackageName, int clientUid, Bundle rootHints) {
        // Allow all clients to browse (you can add security checks here)
        return new BrowserRoot(MEDIA_ROOT_ID, null);
    }

    @Override
    public void onLoadChildren(String parentId, Result<List<MediaBrowserCompat.MediaItem>> result) {
        List<MediaBrowserCompat.MediaItem> mediaItems = new ArrayList<>();

        if (MEDIA_ROOT_ID.equals(parentId)) {
            // Create a sample media item (you can populate this with your actual playlist)
            MediaDescriptionCompat description = new MediaDescriptionCompat.Builder()
                    .setMediaId("current_song")
                    .setTitle(currentSongTitle)
                    .setSubtitle(currentArtist)
                    .setDescription(currentAlbum)
                    .setIconBitmap(albumArt)
                    .build();

            MediaBrowserCompat.MediaItem mediaItem = new MediaBrowserCompat.MediaItem(
                    description, MediaBrowserCompat.MediaItem.FLAG_PLAYABLE);

            mediaItems.add(mediaItem);
        }

        result.sendResult(mediaItems);
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
            currentDuration = mp.getDuration();
            Log.d(TAG, "MediaPlayer prepared");
            mp.start();
            updateState(STATE_PLAYING);
            updateMediaSessionMetadata(); // Update metadata after preparation
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
        // Create MediaSession with proper component name
        ComponentName mediaButtonReceiver = new ComponentName(this, MediaButtonReceiver.class);
        mediaSession = new MediaSessionCompat(this, TAG, mediaButtonReceiver, null);

        // Set the session's token so that client activities can communicate with it
        setSessionToken(mediaSession.getSessionToken());

        // Set callback for media session events
        mediaSession.setCallback(new MediaSessionCompat.Callback() {
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
            public void onSeekTo(long pos) {
                seekTo((int) pos);
            }

            @Override
            public void onPlayFromMediaId(String mediaId, Bundle extras) {
                // Handle play from widget/notification
                Log.d(TAG, "🎯 Play from media ID: " + mediaId);
                if ("current_song".equals(mediaId)) {
                    resume();
                }
            }
        });

        // Set session flags for media buttons and transport controls
        mediaSession.setFlags(
            MediaSessionCompat.FLAG_HANDLES_MEDIA_BUTTONS |
            MediaSessionCompat.FLAG_HANDLES_TRANSPORT_CONTROLS
        );

        // Initialize playback state
        updateMediaSessionPlaybackState(PlaybackStateCompat.STATE_PAUSED);

        // Set initial metadata
        updateMediaSessionMetadata();

        // Activate the session
        mediaSession.setActive(true);
    }

    private void updateMediaSessionPlaybackState(int state) {
        long position = isPrepared ? getCurrentPosition() : 0;

        PlaybackStateCompat.Builder stateBuilder = new PlaybackStateCompat.Builder()
            .setActions(
                PlaybackStateCompat.ACTION_PLAY |
                PlaybackStateCompat.ACTION_PAUSE |
                PlaybackStateCompat.ACTION_SKIP_TO_NEXT |
                PlaybackStateCompat.ACTION_SKIP_TO_PREVIOUS |
                PlaybackStateCompat.ACTION_STOP |
                PlaybackStateCompat.ACTION_SEEK_TO
            )
            .setState(state, position, 1.0f);

        mediaSession.setPlaybackState(stateBuilder.build());
    }

    private void updateMediaSessionMetadata() {
        MediaMetadataCompat.Builder metadataBuilder = new MediaMetadataCompat.Builder()
            .putString(MediaMetadataCompat.METADATA_KEY_TITLE, currentSongTitle)
            .putString(MediaMetadataCompat.METADATA_KEY_ARTIST, currentArtist)
            .putString(MediaMetadataCompat.METADATA_KEY_ALBUM, currentAlbum)
            .putLong(MediaMetadataCompat.METADATA_KEY_DURATION, currentDuration);

        if (albumArt != null) {
            metadataBuilder.putBitmap(MediaMetadataCompat.METADATA_KEY_ALBUM_ART, albumArt);
            metadataBuilder.putBitmap(MediaMetadataCompat.METADATA_KEY_DISPLAY_ICON, albumArt);
        }

        mediaSession.setMetadata(metadataBuilder.build());
    }

    private void showNotification() {
        // Create pending intent for main activity
        Intent intent = new Intent(this, MainActivity.class);
        PendingIntent contentIntent = PendingIntent.getActivity(
            this, 0, intent,
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.M ?
            PendingIntent.FLAG_IMMUTABLE : PendingIntent.FLAG_UPDATE_CURRENT
        );

        // Create MediaStyle notification
        androidx.media.app.NotificationCompat.MediaStyle mediaStyle =
            new androidx.media.app.NotificationCompat.MediaStyle()
                .setMediaSession(mediaSession.getSessionToken())
                .setShowActionsInCompactView(0, 1, 2); // Show play/pause, previous, next

        NotificationCompat.Builder builder = new NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_music_note)
            .setContentTitle(currentSongTitle)
            .setContentText(currentArtist)
            .setSubText(currentAlbum)
            .setLargeIcon(albumArt)
            .setContentIntent(contentIntent)
            .setStyle(mediaStyle)
            .setOngoing(true)
            .setAutoCancel(false)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC);

        // Add action buttons
        builder.addAction(createNotificationAction(
            R.drawable.ic_skip_previous, "Previous",
            KeyEvent.KEYCODE_MEDIA_PREVIOUS));

        if (isPlaying()) {
            builder.addAction(createNotificationAction(
                R.drawable.ic_pause, "Pause",
                KeyEvent.KEYCODE_MEDIA_PAUSE));
        } else {
            builder.addAction(createNotificationAction(
                R.drawable.ic_play, "Play",
                KeyEvent.KEYCODE_MEDIA_PLAY));
        }

        builder.addAction(createNotificationAction(
            R.drawable.ic_skip_next, "Next",
            KeyEvent.KEYCODE_MEDIA_NEXT));

        // Start foreground service
        startForeground(NOTIFICATION_ID, builder.build());
    }

    private NotificationCompat.Action createNotificationAction(int iconResId, String title, int keyCode) {
        Intent intent = new Intent(this, MusicPlayerService.class);
        intent.setAction(Intent.ACTION_MEDIA_BUTTON);
        intent.putExtra(Intent.EXTRA_KEY_EVENT, new KeyEvent(KeyEvent.ACTION_DOWN, keyCode));

        PendingIntent pendingIntent = PendingIntent.getService(
            this, keyCode, intent,
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.M ?
            PendingIntent.FLAG_IMMUTABLE : PendingIntent.FLAG_UPDATE_CURRENT
        );

        return new NotificationCompat.Action(iconResId, title, pendingIntent);
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

        // Update MediaSession playback state
        int sessionState;
        switch (newState) {
            case STATE_PLAYING:
                sessionState = PlaybackStateCompat.STATE_PLAYING;
                break;
            case STATE_PAUSED:
                sessionState = PlaybackStateCompat.STATE_PAUSED;
                break;
            case STATE_STOPPED:
                sessionState = PlaybackStateCompat.STATE_STOPPED;
                break;
            default:
                sessionState = PlaybackStateCompat.STATE_NONE;
        }

        updateMediaSessionPlaybackState(sessionState);
        updateMediaSessionMetadata();

        // Update notification
        showNotification();

        // Notify native code
        onPlaybackStateChanged(newState);
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
        return isPrepared ? mediaPlayer.getCurrentPosition() : 0;
    }

    public long getDuration() {
        return isPrepared ? mediaPlayer.getDuration() : 0;
    }

    public boolean isPlaying() {
        return mediaPlayer != null && mediaPlayer.isPlaying();
    }

    public void seekTo(int position) {
        if (isPrepared) {
            mediaPlayer.seekTo(position);
            updateMediaSessionPlaybackState(currentState == STATE_PLAYING ?
                PlaybackStateCompat.STATE_PLAYING : PlaybackStateCompat.STATE_PAUSED);
        }
    }

    public void setSongInfo(String title, String artist, String album, byte[] albumArtBytes) {
        currentSongTitle = title != null ? title : "Unknown Title";
        currentArtist = artist != null ? artist : "Unknown Artist";
        currentAlbum = album != null ? album : "Unknown Album";

        if (albumArtBytes != null && albumArtBytes.length > 0) {
            albumArt = BitmapFactory.decodeByteArray(albumArtBytes, 0, albumArtBytes.length);
        }

        // Update MediaSession metadata
        updateMediaSessionMetadata();

        // Update notification
        showNotification();

        Log.d(TAG, "Song info updated: " + title + " by " + artist);
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