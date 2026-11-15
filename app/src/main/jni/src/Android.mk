LOCAL_PATH := $(call my-dir)

# Whole SRB2 2.0 engine, built as libmain.so (SDLActivity loads "main")
# relative to LOCAL_PATH (ndk-build prepends LOCAL_PATH to LOCAL_SRC_FILES)
SRB2_REL := ../../../../../srb2src
SRB2 := $(LOCAL_PATH)/$(SRB2_REL)

include $(CLEAR_VARS)
LOCAL_MODULE := main

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../SDL/include \
    $(LOCAL_PATH)/../SDL2_mixer/include \
    $(SRB2) \
    $(SRB2)/sdl

LOCAL_SRC_FILES := \
    $(SRB2_REL)/am_map.c \
    $(SRB2_REL)/command.c \
    $(SRB2_REL)/comptime.c \
    $(SRB2_REL)/console.c \
    $(SRB2_REL)/d_clisrv.c \
    $(SRB2_REL)/d_main.c \
    $(SRB2_REL)/d_net.c \
    $(SRB2_REL)/d_netcmd.c \
    $(SRB2_REL)/d_netfil.c \
    $(SRB2_REL)/dehacked.c \
    $(SRB2_REL)/dstrings.c \
    $(SRB2_REL)/f_finale.c \
    $(SRB2_REL)/f_wipe.c \
    $(SRB2_REL)/filesrch.c \
    $(SRB2_REL)/g_game.c \
    $(SRB2_REL)/g_input.c \
    $(SRB2_REL)/hu_stuff.c \
    $(SRB2_REL)/i_tcp.c \
    $(SRB2_REL)/info.c \
    $(SRB2_REL)/lzf.c \
    $(SRB2_REL)/m_argv.c \
    $(SRB2_REL)/m_bbox.c \
    $(SRB2_REL)/m_cheat.c \
    $(SRB2_REL)/m_fixed.c \
    $(SRB2_REL)/m_menu.c \
    $(SRB2_REL)/m_misc.c \
    $(SRB2_REL)/m_queue.c \
    $(SRB2_REL)/m_random.c \
    $(SRB2_REL)/md5.c \
    $(SRB2_REL)/mserv.c \
    $(SRB2_REL)/p_ceilng.c \
    $(SRB2_REL)/p_enemy.c \
    $(SRB2_REL)/p_fab.c \
    $(SRB2_REL)/p_floor.c \
    $(SRB2_REL)/p_inter.c \
    $(SRB2_REL)/p_lights.c \
    $(SRB2_REL)/p_map.c \
    $(SRB2_REL)/p_maputl.c \
    $(SRB2_REL)/p_mobj.c \
    $(SRB2_REL)/p_polyobj.c \
    $(SRB2_REL)/p_saveg.c \
    $(SRB2_REL)/p_setup.c \
    $(SRB2_REL)/p_sight.c \
    $(SRB2_REL)/p_spec.c \
    $(SRB2_REL)/p_telept.c \
    $(SRB2_REL)/p_tick.c \
    $(SRB2_REL)/p_user.c \
    $(SRB2_REL)/r_bsp.c \
    $(SRB2_REL)/r_data.c \
    $(SRB2_REL)/r_draw.c \
    $(SRB2_REL)/r_main.c \
    $(SRB2_REL)/r_plane.c \
    $(SRB2_REL)/r_segs.c \
    $(SRB2_REL)/r_sky.c \
    $(SRB2_REL)/r_splats.c \
    $(SRB2_REL)/r_things.c \
    $(SRB2_REL)/s_sound.c \
    $(SRB2_REL)/screen.c \
    $(SRB2_REL)/sounds.c \
    $(SRB2_REL)/st_stuff.c \
    $(SRB2_REL)/string.c \
    $(SRB2_REL)/tables.c \
    $(SRB2_REL)/v_video.c \
    $(SRB2_REL)/w_wad.c \
    $(SRB2_REL)/y_inter.c \
    $(SRB2_REL)/z_zone.c \
    $(SRB2_REL)/sdl/i_main.c \
    $(SRB2_REL)/sdl/i_net.c \
    $(SRB2_REL)/sdl/i_system.c \
    $(SRB2_REL)/sdl/i_sound.c \
    $(SRB2_REL)/sdl/dosstr.c \
    $(SRB2_REL)/sdl/endtxt.c \
    $(SRB2_REL)/android/i_cdmus.c \
    $(SRB2_REL)/sdl2/i_video.c \
    $(SRB2_REL)/sdl2/android_crash.c

LOCAL_CFLAGS := -include sdl2/sdl12_shim.h \
    -DLINUX -DSDL -DUNIXCOMMON -DNOASM -DNONX86 -DNOPIX -DHAVE_SDL -DHAVE_MIXER \
    -DNOTERMIOS -DNOIPX -DLOGMESSAGES -DDEFAULTDIR=\"srb2\" \
    -fcommon -fsigned-char -w \
    -Wno-implicit-function-declaration -Wno-int-conversion -Wno-implicit-int \
    -Wno-incompatible-function-pointer-types -Wno-incompatible-pointer-types \
    -Wno-deprecated-non-prototype

LOCAL_SHARED_LIBRARIES := SDL2 SDL2_mixer
LOCAL_LDLIBS := -llog -lm -lz

include $(BUILD_SHARED_LIBRARY)
