package com.techno.srb2mem;

import android.os.Environment;

import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.io.StringWriter;

/** Writes crash information into /storage/emulated/0/SRB2ME so it can be inspected without a PC. */
final class DebugLog {
    private DebugLog() {}

    static File dir() {
        File d = new File(Environment.getExternalStorageDirectory(), "SRB2ME");
        return d.isDirectory() && d.canWrite() ? d : null;
    }

    /** Java exceptions that kill the app are written to java_crash.txt (rotated to java_crash_prev.txt each run). */
    static void installHandler() {
        File d0 = dir();
        if (d0 != null) {
            File cur = new File(d0, "java_crash.txt");
            if (cur.exists()) cur.renameTo(new File(d0, "java_crash_prev.txt"));
        }
        final Thread.UncaughtExceptionHandler previous = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread t, Throwable e) {
                try {
                    File d = dir();
                    if (d != null) {
                        StringWriter sw = new StringWriter();
                        e.printStackTrace(new PrintWriter(sw));
                        FileWriter w = new FileWriter(new File(d, "java_crash.txt"), true);
                        w.write("=== uncaught exception in thread " + t.getName() + " ===\n" + sw + "\n");
                        w.close();
                    }
                } catch (Throwable ignored) {
                }
                if (previous != null) previous.uncaughtException(t, e);
            }
        });
    }

    /** Saves this app's recent Android log (which still holds the previous run) to logcat_last.txt. */
    static void dumpLogcat() {
        try {
            File d = dir();
            if (d == null) return;
            Process p = new ProcessBuilder("logcat", "-d", "-t", "600")
                    .redirectErrorStream(true)
                    .redirectOutput(new File(d, "logcat_last.txt"))
                    .start();
            long end = System.currentTimeMillis() + 4000;
            while (System.currentTimeMillis() < end) {
                try {
                    p.exitValue();
                    return;
                } catch (IllegalThreadStateException running) {
                    Thread.sleep(50);
                }
            }
            p.destroy();
        } catch (Throwable ignored) {
        }
    }
}
