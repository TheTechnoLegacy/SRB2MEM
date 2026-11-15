package com.techno.srb2mem;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * Launcher screen. The game keeps its files in /storage/emulated/0/SRB2ME, which needs
 * file access: "All files access" on Android 11+, the storage permission before that.
 * Once access is granted this starts the game.
 */
public class PermissionActivity extends Activity {

    private static final int REQ_STORAGE = 1;
    private boolean launched = false;

    static boolean hasStorageAccess(Activity a) {
        if (Build.VERSION.SDK_INT >= 30) {
            return Environment.isExternalStorageManager();
        }
        if (Build.VERSION.SDK_INT >= 23) {
            return a.checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                    == PackageManager.PERMISSION_GRANTED;
        }
        return true; // granted at install time
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setBackgroundColor(Color.BLACK);
        int pad = (int) (24 * getResources().getDisplayMetrics().density);
        root.setPadding(pad, pad, pad, pad);

        TextView text = new TextView(this);
        text.setTextColor(Color.WHITE);
        text.setTextSize(18);
        text.setGravity(Gravity.CENTER);
        text.setText("SRB2 stores its game files in\n/storage/emulated/0/SRB2ME\n\n"
                + "Please allow file access so it can create and use that folder.");
        root.addView(text);

        Button button = new Button(this);
        button.setText("Allow file access");
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                requestAccess();
            }
        });
        root.addView(button);

        setContentView(root);
    }

    @Override
    protected void onResume() {
        super.onResume();
        startGameIfAllowed();
    }

    private void requestAccess() {
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                Intent i = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                        Uri.parse("package:" + getPackageName()));
                startActivity(i);
            } catch (Exception e) {
                startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
            }
        } else if (Build.VERSION.SDK_INT >= 23) {
            requestPermissions(new String[] {
                    Manifest.permission.WRITE_EXTERNAL_STORAGE,
                    Manifest.permission.READ_EXTERNAL_STORAGE }, REQ_STORAGE);
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        startGameIfAllowed();
    }

    private void startGameIfAllowed() {
        if (launched || !hasStorageAccess(this)) return;
        launched = true;
        new java.io.File(Environment.getExternalStorageDirectory(), "SRB2ME").mkdirs();
        DebugLog.dumpLogcat();      // keeps the previous run's log in SRB2ME/logcat_last.txt
        DebugLog.installHandler();  // Java crashes -> SRB2ME/java_crash.txt
        startActivity(new Intent(this, SRB2Activity.class));
        finish();
    }
}
