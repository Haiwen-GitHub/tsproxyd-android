package com.ts.tsproxyd;

import android.os.Bundle;
import android.util.Log;

public class MainActivity extends android.app.NativeActivity {
    // Used to load the 'tsproxyd' library on application startup.
    static {
        System.loadLibrary("hv");
        //System.loadLibrary("tsproxyd");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        String configFile = Config.copyConfigFile(this);
        Log.i("MainActivity", "configFile:" + configFile);
        super.onCreate(savedInstanceState);
    }
}