package com.ts.tsproxyd;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;

import androidx.appcompat.app.AppCompatActivity;

public class TestActivity extends AppCompatActivity implements View.OnClickListener {
    boolean DEBUG_DEBUG = true;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_test);

        if (DEBUG_DEBUG) {
            Button button = findViewById(R.id.button_id);
            button.setOnClickListener(this);
        }
    }

    @Override
    public void onClick(View v) {
        Intent intent1 = new Intent(this, TsproxydService.class);
        startService(intent1);
        Button button = (Button) v;
        button.setText("Tsproxyd is runing");
        button.setOnClickListener(null);
    }
}