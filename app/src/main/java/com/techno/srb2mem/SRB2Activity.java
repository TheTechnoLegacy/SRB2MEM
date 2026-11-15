package com.techno.srb2mem;

import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class SRB2Activity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "SDL2_mixer", "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        copyGameData();
        super.onCreate(savedInstanceState);
    }

    /** The game's data folder: /storage/emulated/0/SRB2ME (falls back to the app's own files dir). */
    private File gameDir() {
        File dir = new File(Environment.getExternalStorageDirectory(), "SRB2ME");
        if (!dir.exists()) dir.mkdirs();
        if (dir.isDirectory() && dir.canWrite()) return dir;
        File fallback = getExternalFilesDir(null);
        return fallback != null ? fallback : getFilesDir();
    }

    /** Copies the bundled game data (assets, if any) into the game folder, where the engine looks for it. */
    private void copyGameData() {
        try {
            File dir = gameDir();

            AssetManager am = getAssets();
            String[] names = am.list("");
            if (names == null) return;
            for (String name : names) {
                File out = new File(dir, name);
                if (out.exists() && out.length() > 0) continue; // never overwrite files already in SRB2ME
                try (InputStream in = am.open(name); OutputStream os = new FileOutputStream(out)) {
                    byte[] buf = new byte[1 << 16];
                    int n;
                    while ((n = in.read(buf)) > 0) os.write(buf, 0, n);
                } catch (IOException e) {
                    // directories and unreadable entries are skipped
                }
            }
        } catch (Exception e) {
            Log.e("SRB2", "Could not copy game data", e);
        }
    }
}
