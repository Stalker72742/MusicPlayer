import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

public class TestReceiver extends BroadcastReceiver {
    private static final String TAG = "TestReceiver";

    static {
        Log.d(TAG, "📦 TestReceiver ЗАГРУЖЕН!");
    }

    public TestReceiver() {
        Log.d(TAG, "🔨 TestReceiver СОЗДАН!");
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        Log.d(TAG, "🎧 ПОЛУЧЕНО СОБЫТИЕ!");
        Log.d(TAG, "📨 Action: " + intent.getAction());
    }
}
