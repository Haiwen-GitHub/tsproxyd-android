package com.ts.tsproxyd;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;

import java.io.File;

import dalvik.system.BaseDexClassLoader;

public class TsproxydService extends Service {

    public final static int TSPROXYD_NOTIFICATION_ID = 112;
    public static final String META_DATA_PROXY_LIB = "com.ts.tsproxyd.lib_proxy";
    public static final String META_DATA_HV_LIB = "com.ts.tsproxyd.lib_hv";
    public static final String META_DATA_FUNC_NAME = "com.ts.tsproxyd.func_name";
    private static final String TAG = "TsproxydService";

    // Used to load the 'tsproxydservice' library on application startup.
    static {
//        System.loadLibrary("hv");
        System.loadLibrary("tsproxydservice");
//        System.loadLibrary("tsproxyd");
    }

    private long mNativeHandle = 0;


    private static String getAbsolutePath(File file) {
        return (file != null) ? file.getAbsolutePath() : null;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "onCreate");

        // copy config file to file system here
        String configFile = Config.copyConfigFile(this);
        Log.i(TAG, "configFile:" + configFile);

        // load Native code
        loadNative(configFile);

        // start native service on main thread,then start work thread by main and detach it
        startNative();

        // run service back ground
        startForegroundService();
    }

    private void loadNative(String configFilePath) {
        String libname = "";//= "tsproxyd";
        String libname_hv = "";// = "hv";
        String funcname = ""; // "android_main_func";

        try {
            ServiceInfo ai = getPackageManager().getServiceInfo(
                    new ComponentName(this, TsproxydService.class), PackageManager.GET_META_DATA);
            if (ai.metaData != null) {
                String ln = ai.metaData.getString(META_DATA_PROXY_LIB);
                if (ln != null) {
                    libname = ln;
                }
                ln = ai.metaData.getString(META_DATA_HV_LIB);
                if (ln != null) {
                    libname_hv = ln;
                }
                ln = ai.metaData.getString(META_DATA_FUNC_NAME);
                if (ln != null) {
                    funcname = ln;
                }
            }
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException("Error getting activity info", e);
        }

        Log.d(TAG, "libname:" + libname);
        Log.d(TAG, "libname_hv:" + libname_hv);
        Log.d(TAG, "funcname:" + funcname);

        BaseDexClassLoader classLoader = (BaseDexClassLoader) getClassLoader();
        String path = classLoader.findLibrary(libname);
        if (path == null) {
            throw new IllegalArgumentException("Unable to find native library " + libname +
                    " using classloader: " + classLoader.toString());
        }

        String path_hv = classLoader.findLibrary(libname_hv);
        if (path_hv == null) {
            throw new IllegalArgumentException("Unable to find native library " + libname_hv +
                    " using classloader: " + classLoader.toString());
        }

        mNativeHandle = loadNativeService(path, path_hv, funcname, configFilePath, getAbsolutePath(getFilesDir()));
        if (mNativeHandle == 0) {
            throw new UnsatisfiedLinkError(
                    "Unable to load native library \"" + path + "\" ");
        }
    }

    private void startNative() {
        int result = onStartNativeService(mNativeHandle);
        if (result != 0) {
            throw new RuntimeException("onStartNativeService failed!");
        }
    }

    @Override
    public void onDestroy() {
        stopForegroundService();
        onStopNativeService(mNativeHandle);
        unloadNativeService(mNativeHandle);
        mNativeHandle = 0;
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        // TODO: Return the communication channel to the service.
        throw new UnsupportedOperationException("Not yet implemented");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.i(TAG, "onStartCommand startId:" + startId);
        if (intent == null) {
            return START_STICKY;
        }

        return super.onStartCommand(intent, flags, startId);
    }

    private void startForegroundService() {
        Notification.Builder builder = new Notification.Builder(this.getApplicationContext())
                .setContentIntent(PendingIntent.getActivity(this, 0, new Intent(this, TestActivity.class), 0))
                .setLargeIcon(BitmapFactory.decodeResource(this.getResources(), R.mipmap.tsproxyed_fg))
                .setSmallIcon(R.mipmap.tsproxyed_fg)
                .setContentTitle(this.getResources().getString(R.string.ts_proxy_name))
                .setContentText(this.getResources().getString(R.string.ts_proxy_name))
                .setWhen(System.currentTimeMillis());

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            String channelName = "com.ts.tsproxyd";
            String channelId = "com.ts.tsproxyd";
            NotificationChannel channel = null;

            channel = new NotificationChannel(channelId, channelName, NotificationManager.IMPORTANCE_HIGH);
            channel.enableLights(true);
            channel.setLightColor(Color.RED);
            channel.setShowBadge(true);
            channel.setLockscreenVisibility(Notification.VISIBILITY_PUBLIC);

            NotificationManager manager = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
            manager.createNotificationChannel(channel);
            builder.setChannelId(channelId);
        }

        Notification notification = builder.build();
        //notification.defaults = Notification.DEFAULT_SOUND;
        startForeground(TSPROXYD_NOTIFICATION_ID, notification);
    }

    private void stopForegroundService() {
        stopForeground(true);
    }

    // NATIVE METHODS
    private native long loadNativeService(String path, String path_hv, String funcName, String configFilePath, String interDataPath);

    private native void unloadNativeService(long handle);

    private native int onStartNativeService(long handle);

    private native int onStopNativeService(long handle);
}