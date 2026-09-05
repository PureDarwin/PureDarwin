# fbDOOM (https://github.com/maximevince/fbDOOM) for PureDarwin.
#
# fbDOOM is GPL-2.0-or-later, unlike the rest of this tree; it is kept fully
# opt-in (PUREDARWIN_ENABLE_FBDOOM, off by default) and built from an
# untouched, externally-provided checkout (PUREDARWIN_FBDOOM_SOURCE). We only
# ever compile FROM that tree (target_sources reads its .c files directly),
# never write into it - treat it as read-only third-party source.
# PureDarwin-specific changes live only in this directory.
#
# fbDOOM's own Linux-specific backends (i_video_fbdev.c: /dev/fb0 + ioctl
# FBIOGET_VSCREENINFO; i_input_tty.c: KDSKBMODE/K_MEDIUMRAW raw console
# scancodes; i_sound.c: an external sndserv helper process) have no
# PureDarwin equivalent, so they are excluded from the build entirely and
# replaced by PureDarwin-native implementations that live in THIS directory (not
# in the copied fbDOOM tree): i_video_pd.c (real framebuffer output via
# PDGOP, the same userland IOGOPFramebuffer client the puredarwingop Xorg
# DDX driver uses) and i_input_pd.c (plain termios raw-mode keyboard input).
if(NOT PUREDARWIN_ENABLE_FBDOOM)
    return()
endif()

if(NOT PUREDARWIN_FBDOOM_SOURCE AND DEFINED ENV{PUREDARWIN_FBDOOM_SOURCE})
    set(PUREDARWIN_FBDOOM_SOURCE "$ENV{PUREDARWIN_FBDOOM_SOURCE}")
endif()

if(NOT PUREDARWIN_FBDOOM_SOURCE)
    message(FATAL_ERROR "PUREDARWIN_FBDOOM_SOURCE must point to an fbDOOM checkout (the 'fbdoom' subdir with i_main.c etc)")
endif()

if(NOT EXISTS "${PUREDARWIN_FBDOOM_SOURCE}/i_main.c")
    message(FATAL_ERROR "PUREDARWIN_FBDOOM_SOURCE='${PUREDARWIN_FBDOOM_SOURCE}' has no i_main.c")
endif()

if(NOT PUREDARWIN_CHOCOLATE_DOOM_SOURCE AND DEFINED ENV{PUREDARWIN_CHOCOLATE_DOOM_SOURCE})
    set(PUREDARWIN_CHOCOLATE_DOOM_SOURCE "$ENV{PUREDARWIN_CHOCOLATE_DOOM_SOURCE}")
endif()

if(NOT PUREDARWIN_CHOCOLATE_DOOM_SOURCE)
    message(FATAL_ERROR "PUREDARWIN_CHOCOLATE_DOOM_SOURCE must point to the patched chocolate-doom source (midifile.c, mus2mid.c, opl/)")
endif()

if(NOT EXISTS "${PUREDARWIN_CHOCOLATE_DOOM_SOURCE}/midifile.c")
    message(FATAL_ERROR "PUREDARWIN_CHOCOLATE_DOOM_SOURCE='${PUREDARWIN_CHOCOLATE_DOOM_SOURCE}' has no midifile.c")
endif()

# Same file list as fbDOOM's own Makefile SRC_DOOM, minus i_joystick.o (no
# joystick support glued up) and i_sound.o/i_cdmus.o (replaced by
# i_sound_pd.c - real fbDOOM's i_sound_dummy.c is, despite the name, a real
# Linux OSS /dev/dsp driver no longer even prototype-compatible with this
# revision's i_sound.h. There's a real audio path available on PureDarwin that
# could replace i_sound_pd.c's silence with real sound as a follow-up).
set(_fbdoom_upstream_srcs
    i_main.c dummy.c am_map.c doomdef.c doomstat.c dstrings.c d_event.c
    d_items.c d_iwad.c d_loop.c d_main.c d_mode.c d_net.c f_finale.c
    f_wipe.c g_game.c hu_lib.c hu_stuff.c info.c i_endoom.c i_scale.c
    i_system.c i_timer.c memio.c m_argv.c m_bbox.c
    m_cheat.c m_config.c m_controls.c m_fixed.c m_menu.c m_misc.c
    m_random.c p_ceilng.c p_doors.c p_enemy.c p_floor.c p_inter.c
    p_lights.c p_map.c p_maputl.c p_mobj.c p_plats.c p_pspr.c p_saveg.c
    p_setup.c p_sight.c p_spec.c p_switch.c p_telept.c p_tick.c p_user.c
    r_bsp.c r_data.c r_draw.c r_main.c r_plane.c r_segs.c r_sky.c
    r_things.c sha1.c sounds.c statdump.c st_lib.c st_stuff.c s_sound.c
    tables.c v_video.c wi_stuff.c w_checksum.c w_file.c
    w_file_stdc_unbuffered.c w_main.c w_wad.c z_zone.c
)

add_darwin_executable(fbdoom MACOSX_VERSION_MIN 26.5)

foreach(_src ${_fbdoom_upstream_srcs})
    target_sources(fbdoom PRIVATE "${PUREDARWIN_FBDOOM_SOURCE}/${_src}")
endforeach()
target_sources(fbdoom PRIVATE
    "${PUREDARWIN_FBDOOM_SOURCE}/i_oplmusic.c"
    i_video_pd.c
    i_input_pd.c
    i_sound_pd.c
    i_music_opl_glue.c
    pd_choco_compat.c
    opl/opl_pd.c
    "${PUREDARWIN_CHOCOLATE_DOOM_SOURCE}/midifile.c"
    "${PUREDARWIN_CHOCOLATE_DOOM_SOURCE}/mus2mid.c"
    "${PUREDARWIN_CHOCOLATE_DOOM_SOURCE}/opl/opl.c"
    "${PUREDARWIN_CHOCOLATE_DOOM_SOURCE}/opl/opl_queue.c"
    "${PUREDARWIN_CHOCOLATE_DOOM_SOURCE}/opl/opl3.c"
)

get_target_property(_fbdoom_opts fbdoom COMPILE_OPTIONS)
if(_fbdoom_opts)
    list(REMOVE_ITEM _fbdoom_opts -nostdinc)
    set_target_properties(fbdoom PROPERTIES COMPILE_OPTIONS "${_fbdoom_opts}")
endif()

target_include_directories(fbdoom PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/opl"
    "${PUREDARWIN_FBDOOM_SOURCE}"
    "${PUREDARWIN_CHOCOLATE_DOOM_SOURCE}"
    "${PUREDARWIN_CHOCOLATE_DOOM_SOURCE}/opl"
    "${CMAKE_SOURCE_DIR}/src/Libraries/PDGOP/include"
)
target_compile_definitions(fbdoom PRIVATE NORMALUNIX)
target_compile_options(fbdoom PRIVATE -isysroot ${PUREDARWIN_MACOSX_SDK} -Wno-everything)
target_link_options(fbdoom PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem)
target_link_libraries(fbdoom PRIVATE $<TARGET_FILE:PDGOP> $<TARGET_FILE:IOKit>)
add_dependencies(fbdoom PDGOP IOKit)
add_dependencies(fbdoom libSystem_B_stub dyld)

install(TARGETS fbdoom DESTINATION usr/bin COMPONENT BaseSystem)
