package com.ts.tsproxyd;

import android.content.Context;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class Config {
    private static final String TAG = "Config";

    private Config() {
    }

    public static String copyConfigFile(Context context) {
        // copy tproxyd's config from asserts to file system local
        // tsproxyd that run native will load these data from this copy
        int result = 0;
        String filePath;
        try {
            File outFile = new File(context.getExternalFilesDir(null), "tsproxyd.conf");
            Log.d(TAG, "outFile:" + outFile.getAbsolutePath());
            if (!outFile.exists()) {
                boolean notExist = outFile.createNewFile();
                InputStream is = context.getAssets().open("tsproxyd.conf");
                FileOutputStream fos = new FileOutputStream(outFile);
                byte[] buffer = new byte[1024];
                int byteCount = 0;
                while ((byteCount = is.read(buffer)) != -1) {
                    fos.write(buffer, 0, byteCount);
                }
                fos.flush();
                is.close();
                fos.close();
            }
            filePath = outFile.getAbsolutePath();
        } catch (IOException | SecurityException e) {
            e.printStackTrace();
            filePath = "";
        }

        return filePath;
    }
}
