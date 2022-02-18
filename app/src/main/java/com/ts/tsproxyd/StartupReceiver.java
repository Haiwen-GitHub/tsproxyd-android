package com.ts.tsproxyd;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

public class StartupReceiver extends BroadcastReceiver {

    @Override
    public void onReceive(Context context, Intent intent) {
        // This method is called when the BroadcastReceiver is receiving
        // an Intent broadcast.
        //throw new UnsupportedOperationException("Not yet implemented");
        Log.e("StartupReceiver", "StartupReceiver is done");
        Intent intent1 = new Intent(context.getApplicationContext(), TsproxydService.class);
        context.getApplicationContext().startService(intent1);
    }
}