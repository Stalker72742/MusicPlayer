package com.example.MusicPlayer;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Binder;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;
import android.view.KeyEvent;

import androidx.core.app.NotificationCompat;
import androidx.media.MediaBrowserServiceCompat;
import android.support.v4.media.MediaBrowserCompat;
import android.support.v4.media.MediaDescriptionCompat;
import android.support.v4.media.MediaMetadataCompat;
import androidx.media.session.MediaButtonReceiver;
import android.support.v4.media.session.MediaSessionCompat;
import android.support.v4.media.session.PlaybackStateCompat;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class MusicPlayerService extends MediaBrowserServiceCompat {
    private static final String TAG = "MusicPlayerService";
    private static final String CHANNEL_ID = "music_player_channel";
    private static final int NOTIFICATION_ID = 1001;

    // MediaBrowser root ID
    private static final String MEDIA_ROOT_ID = "root_id";

    // MediaPlayer
    private MediaPlayer mediaPlayer;
    private boolean isPrepared = false;

    // MediaSession (using support library / AndroidX)
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

    // --- Static instance & static wrappers for easy JNI calls from Qt ---
    private static MusicPlayerService instance = null;

    public static boolean playSongStatic(String path) {
        if (instance != null) {
            instance.playSong(path);
            return true;
        }
        Log.d(TAG, "MusicPlayerService instance null");
        return false;
    }

    public static boolean setVolumeStatic(float vol) {
        if (instance != null) {
            instance.setVolume(vol);
            return true;
        }
        return false;
    }

    public static boolean playStatic() {
        if (instance != null) {
            instance.resume();
            return true;
        }
        return false;
    }

    public static boolean pauseStatic() {
        if (instance != null) {
            instance.pause();
            return true;
        }
        return false;
    }

    public static boolean stopStatic() {
        if (instance != null) {
            instance.stop();
            return true;
        }
        return false;
    }

    public static boolean seekToStatic(int pos) {
        if (instance != null) {
            instance.seekTo(pos);
            return true;
        }
        return false;
    }

    public static long getCurrentPositionStatic() {
        if (instance != null) return instance.getCurrentPosition();
        return 0;
    }

    public static long getDurationStatic() {
        if (instance != null) return instance.getDuration();
        return 0;
    }

    public static boolean isPlayingStatic() {
        if (instance != null) return instance.isPlaying();
        return false;
    }

    public static void setSongInfoStatic(String title, String artist, String album, byte[] albumArtBytes) {
        if (instance != null) instance.setSongInfo(title, artist, album, albumArtBytes);
    }
    // --- end static wrappers ---

    @Override
    public void onCreate() {
        super.onCreate();
        Log.d(TAG, "🎵 MusicPlayerService создается...");

        // set static instance
        instance = this;

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
        // Handle media button events (safe to call even if intent is null)
        try {
            MediaButtonReceiver.handleIntent(mediaSession, intent);
        } catch (Exception e) {
            Log.w(TAG, "handleIntent failed: " + e.getMessage());
        }
        return START_STICKY;
    }

    // Important: return super.onBind(...) for MediaBrowser clients, otherwise return our binder.
    @Override
    public IBinder onBind(Intent intent) {
        if (intent != null && "android.media.browse.MediaBrowserService".equals(intent.getAction())) {
            return super.onBind(intent);
        }
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
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
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
    }

    private void initMediaPlayer() {
        mediaPlayer = new MediaPlayer();

        mediaPlayer.setOnPreparedListener(mp -> {
            isPrepared = true;
            currentDuration = mp.getDuration();
            Log.d(TAG, "MediaPlayer prepared");
            mp.start();
            updateState(STATE_PLAYING);
            updateMediaSessionMetadata();
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
        ComponentName mediaButtonReceiver = new ComponentName(this, MediaButtonReceiver.class);
        mediaSession = new MediaSessionCompat(this, TAG, mediaButtonReceiver, null);

        setSessionToken(mediaSession.getSessionToken());

        mediaSession.setCallback(new MediaSessionCompat.Callback() {
            @Override
            public void onPlay() {
                Log.d(TAG, "▶️ MediaSession onPlay()");
                resume();
                onMediaButton(KeyEvent.KEYCODE_MEDIA_PLAY, "PLAY");
            }

            @Override
            public void onPause() {
                Log.d(TAG, "⏸️ MediaSession onPause()");
                pause();
                onMediaButton(KeyEvent.KEYCODE_MEDIA_PAUSE, "PAUSE");
            }

            @Override
            public void onSkipToNext() {
                Log.d(TAG, "⏭️ MediaSession onSkipToNext()");
                onMediaButton(KeyEvent.KEYCODE_MEDIA_NEXT, "NEXT");
            }

            @Override
            public void onSkipToPrevious() {
                Log.d(TAG, "⏮️ MediaSession onSkipToPrevious()");
                onMediaButton(KeyEvent.KEYCODE_MEDIA_PREVIOUS, "PREVIOUS");
            }

            @Override
            public void onStop() {
                Log.d(TAG, "⏹️ MediaSession onStop()");
                stop();
                onMediaButton(KeyEvent.KEYCODE_MEDIA_STOP, "STOP");
            }

            @Override
            public void onSeekTo(long pos) {
                seekTo((int) pos);
            }

            @Override
            public void onPlayFromMediaId(String mediaId, Bundle extras) {
                Log.d(TAG, "🎯 Play from media ID: " + mediaId);
                if ("current_song".equals(mediaId)) {
                    resume();
                }
            }
        });

        mediaSession.setFlags(
            MediaSessionCompat.FLAG_HANDLES_MEDIA_BUTTONS |
            MediaSessionCompat.FLAG_HANDLES_TRANSPORT_CONTROLS
        );

        updateMediaSessionPlaybackState(PlaybackStateCompat.STATE_PAUSED);
        updateMediaSessionMetadata();
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
        Intent intent = new Intent(this, MainActivity.class);
        int flags = Build.VERSION.SDK_INT >= Build.VERSION_CODES.M ? PendingIntent.FLAG_IMMUTABLE : PendingIntent.FLAG_UPDATE_CURRENT;
        PendingIntent contentIntent = PendingIntent.getActivity(this, 0, intent, flags);

        androidx.media.app.NotificationCompat.MediaStyle mediaStyle =
            new androidx.media.app.NotificationCompat.MediaStyle()
                .setMediaSession(mediaSession.getSessionToken())
                .setShowActionsInCompactView(0, 1, 2);

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

        startForeground(NOTIFICATION_ID, builder.build());
    }

    private NotificationCompat.Action createNotificationAction(int iconResId, String title, int keyCode) {
        Intent intent = new Intent(this, MusicPlayerService.class);
        intent.setAction(Intent.ACTION_MEDIA_BUTTON);
        intent.putExtra(Intent.EXTRA_KEY_EVENT, new KeyEvent(KeyEvent.ACTION_DOWN, keyCode));

        int flags = Build.VERSION.SDK_INT >= Build.VERSION_CODES.M ? PendingIntent.FLAG_IMMUTABLE : PendingIntent.FLAG_UPDATE_CURRENT;
        PendingIntent pendingIntent = PendingIntent.getService(this, keyCode, intent, flags);

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
                        case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                            pause();
                            break;
                        case AudioManager.AUDIOFOCUS_GAIN:
                            // resume if needed
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
        showNotification();
        onPlaybackStateChanged(newState);
    }

    // Public instance methods
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
        if (mediaPlayer != null && mediaPlayer.isPlaying()) {
            mediaPlayer.pause();
            updateState(STATE_PAUSED);
            Log.d(TAG, "Playback paused");
        }
    }

    public void resume() {
        if (isPrepared && mediaPlayer != null && !mediaPlayer.isPlaying()) {
            mediaPlayer.start();
            updateState(STATE_PLAYING);
            Log.d(TAG, "Playback resumed");
        }
    }

    public void stop() {
        if (mediaPlayer != null && mediaPlayer.isPlaying()) {
            mediaPlayer.stop();
        }
        updateState(STATE_STOPPED);
        Log.d(TAG, "Playback stopped");
    }

    // volume: user might pass 0..100 -> convert to 0..1 for MediaPlayer
    public void setVolume(float volume) {
        if (mediaPlayer == null) return;
        float v = Math.max(0f, Math.min(100f, volume)) / 100f;
        mediaPlayer.setVolume(v, v);
        Log.d(TAG, "Volume set to: " + v + " (from " + volume + ")");
    }

    public long getCurrentPosition() {
        return isPrepared && mediaPlayer != null ? mediaPlayer.getCurrentPosition() : 0;
    }

    public long getDuration() {
        return isPrepared && mediaPlayer != null ? mediaPlayer.getDuration() : 0;
    }

    public boolean isPlaying() {
        return mediaPlayer != null && mediaPlayer.isPlaying();
    }

    public void seekTo(int position) {
        if (isPrepared && mediaPlayer != null) {
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

        updateMediaSessionMetadata();
        showNotification();
        Log.d(TAG, "Song info updated: " + title + " by " + artist);
    }

    @Override
    public void onDestroy() {
        Log.d(TAG, "🗑️ MusicPlayerService уничтожается...");

        instance = null;

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
