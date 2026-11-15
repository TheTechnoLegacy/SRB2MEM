# SRB2MEM - SRB2 2.0 for Android!

SRB2MEM (MEM stands for Marriage Edition Mobile) is an Android port of SRB2 2.0 (version 2.0.7 to be exact) with SDL2. It currently has Software renderer only, on-screen touch controls and SDL2_mixer audio.

## Build status

The native code (the whole engine + SDL2 + SDL2_mixer) was compiled with the real Android NDK r26d
for both `arm64-v8a` and `armeabi-v7a` and links cleanly.

## Get the APK! (no PC tools needed)

1. Create a free GitHub account and a new **empty repository**.
2. Upload everything in this folder (drag and drop the files/folders into the repo page,
   or `git push`). Make sure the `.github` folder is included.
3. Open the **Actions** tab, choose **Build SRB2MEM APK**, click **Run workflow**
   (it also runs automatically on every push).
4. After ~10-20 minutes open the finished run and download **SRB2MEM-apk**
   from *Artifacts*. Unzip it and install `app-release.apk` (allow "install unknown apps").
5. Alternatively, install it from GitHub Releases (you need to copy the game files to `.srb2` or `gamedata/` folder in `/storage/emulated/0/SRB2MEM/`

The workflow downloads SDL2, SDL2_mixer and the official SRB2 2.0.6 data files for you.
If the data download fails, put `srb2.wad`, `zones.dta`, `soar.dta`, `drill.dta`, `music.dta`,
`rings.wpn`, `sonic.plr`, `tails.plr`, `knux.plr` in the `gamedata/` folder and run again.
You can also copy them by hand to `/storage/emulated/0/SRB2ME/` on the phone.

## Game folder

All game files (data, config, saves, log) live in **`/storage/emulated/0/SRB2ME`**. On first
launch a small screen asks for file access ("All files access" on Android 11+, the storage
permission on older versions); after you allow it the app creates the folder and starts the game.

## App icon

`app/src/main/res/mipmap-*/ic_launcher.png` (the app icon was scaled up to `xxxhdpi` using Nearest Neighbor)

## Controls

Touch overlay (appears after the first touch): d-pad on the left; **A** jump, **B** spin,
**X** fire, **OK** = Enter, **ESC** = Escape, **CON** opens/closes the console.
The on-screen keyboard appears automatically while the console is open. A hardware keyboard works too
(arrows, `/` jump, `.` spin, Right Ctrl fire). The Android Back button = Escape.

## What was changed from the SRB2 2.0 source:

- `src/sdl2/i_video.c` - new SDL2 video backend (replaces SDL 1.2 `sdl/i_video.c`), touch controls, on-screen keyboard for the console
- `src/sdl2/sdl12_shim.h` - force-included; maps the few remaining SDL 1.2 calls to SDL2
- `src/sdl/i_main.c` - on Android: HOME + working dir = `/storage/emulated/0/SRB2ME`
- `src/d_main.c` - removed hard-coded `/data/data/org.srb2/` home path
- `src/sdl/i_system.c` - Mumble shared-memory code disabled on Android (no `shm_open`)
- `src/doomtype.h`, `m_swap.h`, `byteptr.h`, `md5.c` - bionic defines `_BIG_ENDIAN` on every device; use the real byte order instead (this made every WAD look corrupt)
- `src/sdl/i_sound.c` - music-finished hook no longer blocks on a mutex (audio/game thread deadlock), busy-waits are bounded; freeze watchdog dumps thread backtraces to `crash.txt`
- `src/sdl/i_sound.c` - Ogg loop-point scan no longer reads past the end of the music lump (segfault on some levels)
- `src/sdl/i_sound.c` - sound-effect callback no longer wipes the stream (it erased SDL_mixer's music)
- `src/r_things.c` - range-checked sprite column lookup (bad column = wild pointer crash); built with `-fsigned-char` (ARM `char` is unsigned, x86 signed)
- `src/filesrch.c` - no more `readdir(NULL)` abort when a search folder does not exist
- `src/doomtype.h` - fixed a stray `ATTRPACK` global that breaks modern compilers
- audio uses the existing SDL_mixer code (`HAVE_MIXER`), CD audio is a stub

## Known limitations

- Software renderer only (no OpenGL). Netplay and the master server are untested.
- The engine is old 32-bit-era code. `armeabi-v7a` is the safer build; `arm64-v8a` may
  crash in places with 64-bit assumptions. Android picks the matching one automatically.
- MIDI needs a TiMidity config/patch set to make sound; MOD/IT/XM, OGG, MP3 work.
- Each launch starts a fresh `crash.txt` (the previous run's is kept as `crash_prev.txt`, Java's as `java_crash_prev.txt`).
- If something fails, the app writes these into `/storage/emulated/0/SRB2ME/`: `crash.txt` (native crash
  backtrace), `srb2_stdout.txt` / `srb2_stderr.txt` (everything the engine printed), `java_crash.txt`
  and `logcat_last.txt` (Android log of the previous run), plus `srb2/srb2log.txt`. Send those.

## Additional info
1. **Game assets are owned by Sonic Team Jr. (STJr) meanwhile the "Sonic" brand and characters are owned by SEGA.**
2. It's licensed under the **GNU General Public License 2**, any modification and distribution of the mod requires open-sourcing the app.
