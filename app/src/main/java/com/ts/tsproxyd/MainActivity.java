package com.ts.tsproxyd;

import android.os.Bundle;
import android.widget.TextView;

import com.ts.tsproxyd.databinding.ActivityMainBinding;

public class MainActivity extends android.app.NativeActivity {

    // Used to load the 'tsproxy' library on application startup.
    static {
       System.loadLibrary("hv");
        System.loadLibrary("tsproxyd");
    }

//    private ActivityMainBinding binding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

//        binding = ActivityMainBinding.inflate(getLayoutInflater());
//        setContentView(binding.getRoot());
//
//        // Example of a call to a native method
//        TextView tv = binding.sampleText;
//        tv.setText(stringFromJNI());
    }

    /**
     * A native method that is implemented by the 'tsproxy' native library,
     * which is packaged with this application.
     */
//    public native String stringFromJNI();
}