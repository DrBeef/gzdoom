LOCAL_PATH := $(call my-dir)/../src


include $(CLEAR_VARS)

LOCAL_MODULE    := qzdoom

LOCAL_CFLAGS   :=  -D__MOBILE__ -DNO_PIX_BUFF -DOPNMIDI_DISABLE_GX_EMULATOR -DGZDOOM  -DLZDOOM -DNO_VBO -D__STDINT_LIMITS -DENGINE_NAME=\"lzdoom\"

LOCAL_CPPFLAGS := -DHAVE_FLUIDSYNTH -DHAVE_MPG123 -DHAVE_SNDFILE -std=c++14 -DHAVE_JWZGLES -Wno-switch -Wno-inconsistent-missing-override -Werror=format-security \
    -fexceptions -fpermissive -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -D__forceinline=inline -DNO_GTK -DNO_SSE -fsigned-char

LOCAL_CFLAGS  += -DNO_SEND_STATS

LOCAL_CFLAGS  += -DOPNMIDI_USE_LEGACY_EMULATOR
LOCAL_CFLAGS  += -DADLMIDI_DISABLE_MUS_SUPPORT -DADLMIDI_DISABLE_XMI_SUPPORT -DADLMIDI_DISABLE_MIDI_SEQUENCER

ifeq ($(BUILD_SERIAL),1)
LOCAL_CPPFLAGS += -DANTI_HACK 
endif

	
LOCAL_C_INCLUDES := \
 $(TOP_DIR)/ \
 ${TOP_DIR}/OpenXR-SDK/include \
 ${TOP_DIR}/OpenXR-SDK/src/common \
 $(GZDOOM_TOP_PATH)/src/  \
 $(GZDOOM_TOP_PATH)/mobile/src/extrafiles  \
 $(GZDOOM_TOP_PATH)/game-music-emu/ \
 $(GZDOOM_TOP_PATH)/gdtoa \
 $(GZDOOM_TOP_PATH)/lzma/C \
 $(GZDOOM_TOP_PATH)/bzip2 \
 $(GZDOOM_TOP_PATH)/asmjit \
 $(GZDOOM_TOP_PATH)/src/sound \
	$(GZDOOM_TOP_PATH)/src/sound/music \
	$(GZDOOM_TOP_PATH)/src/sound/backend \
 $(GZDOOM_TOP_PATH)/src/textures \
 $(GZDOOM_TOP_PATH)/src/thingdef \
 $(GZDOOM_TOP_PATH)/src/sdl \
 $(GZDOOM_TOP_PATH)/src/g_inventory \
 $(GZDOOM_TOP_PATH)/src/g_strife \
 $(GZDOOM_TOP_PATH)/src/g_shared \
 $(GZDOOM_TOP_PATH)/src/g_statusbar \
 $(GZDOOM_TOP_PATH)/src/scripting \
 $(GZDOOM_TOP_PATH)/src/scripting/vm \
 $(GZDOOM_TOP_PATH)/src/posix \
 $(GZDOOM_TOP_PATH)/src/posix/nosdl \
 $(GZDOOM_TOP_PATH)/src/../libraries/gdtoa \
 $(GZDOOM_TOP_PATH)/src/../libraries/bzip2 \
 $(GZDOOM_TOP_PATH)/src/../libraries/game-music-emu/ \
 $(GZDOOM_TOP_PATH)/src/../libraries/dumb/include \
 $(GZDOOM_TOP_PATH)/src/../libraries/glslang/glslang/Public \
 $(GZDOOM_TOP_PATH)/src/../libraries/glslang/spirv \
 $(GZDOOM_TOP_PATH)/src/../libraries/lzma/C \
 $(GZDOOM_TOP_PATH)/src/../libraries/zmusic \
 $(SUPPORT_LIBS)/fluidsynth-lite/include \
 $(SUPPORT_LIBS)/openal/include/AL \
 $(SUPPORT_LIBS)/libsndfile-android/jni/ \
 $(SUPPORT_LIBS)/libmpg123 \
 $(SUPPORT_LIBS)/jpeg8d \
 $(GZDOOM_TOP_PATH)/mobile/src \
 $(GL4ES_PATH)


#############################################################################
# CLIENT/SERVER
#############################################################################


ANDROID_SRC_FILES = \
    ../mobile/src/i_specialpaths_android.cpp

PLAT_POSIX_SOURCES = \
	posix/i_steam.cpp \
	posix/i_system_posix.cpp

PLAT_NOSDL_SOURCES = \
	posix/nosdl/crashcatcher.c \
	posix/nosdl/hardware.cpp \
	posix/nosdl/i_gui.cpp \
	posix/nosdl/i_joystick.cpp \
	posix/nosdl/i_system.cpp \
	posix/nosdl/glvideo.cpp \
	posix/nosdl/st_start.cpp


FASTMATH_SOURCES = \
	swrenderer/r_all.cpp \
	swrenderer/r_swscene.cpp \
	polyrenderer/poly_all.cpp \
	sound/music/music_midi_base.cpp \
	sound/backend/oalsound.cpp \
	textures/hires/hqnx/init.cpp \
	textures/hires/hqnx/hq2x.cpp \
	textures/hires/hqnx/hq3x.cpp \
	textures/hires/hqnx/hq4x.cpp \
	textures/hires/xbr/xbrz.cpp \
	textures/hires/xbr/xbrz_old.cpp \
	gl/scene/gl_drawinfo.cpp \
	gl/scene/gl_flats.cpp \
	gl/scene/gl_sprite.cpp \
	gl/scene/gl_skydome.cpp \
	gl/scene/gl_weapon.cpp \
	gl/scene/gl_scene.cpp \
	gl/scene/gl_portal.cpp \
	gl/scene/gl_walls_draw.cpp \
	gl_load/gl_load.c \
	hwrenderer/dynlights/hw_dynlightdata.cpp \
	hwrenderer/scene/hw_bsp.cpp \
	hwrenderer/scene/hw_fakeflat.cpp \
	hwrenderer/scene/hw_decal.cpp \
	hwrenderer/scene/hw_drawinfo.cpp \
	hwrenderer/scene/hw_drawlist.cpp \
	hwrenderer/scene/hw_clipper.cpp \
	hwrenderer/scene/hw_flats.cpp \
	hwrenderer/scene/hw_portal.cpp \
	hwrenderer/scene/hw_renderhacks.cpp \
	hwrenderer/scene/hw_sky.cpp \
	hwrenderer/scene/hw_sprites.cpp \
	hwrenderer/scene/hw_spritelight.cpp \
	hwrenderer/scene/hw_walls.cpp \
	hwrenderer/scene/hw_walls_vertex.cpp \
	hwrenderer/scene/hw_weapon.cpp \
	r_data/models/models.cpp \
	r_data/matrix.cpp \



PCH_SOURCES = \
	actorptrselect.cpp \
	am_map.cpp \
	b_bot.cpp \
	b_func.cpp \
	b_game.cpp \
	b_move.cpp \
	b_think.cpp \
	bbannouncer.cpp \
	c_bind.cpp \
	c_cmds.cpp \
	c_console.cpp \
	c_consolebuffer.cpp \
	c_cvars.cpp \
	c_dispatch.cpp \
	c_expr.cpp \
	c_functions.cpp \
	cmdlib.cpp \
	colormatcher.cpp \
	compatibility.cpp \
	configfile.cpp \
	ct_chat.cpp \
	cycler.cpp \
	d_dehacked.cpp \
	d_iwad.cpp \
	d_main.cpp \
	d_anonstats.cpp \
	d_net.cpp \
	d_netinfo.cpp \
	d_protocol.cpp \
	decallib.cpp \
	dobject.cpp \
	dobjgc.cpp \
	dobjtype.cpp \
	doomstat.cpp \
	dsectoreffect.cpp \
	dthinker.cpp \
	edata.cpp \
	f_wipe.cpp \
	files.cpp \
	files_decompress.cpp \
	g_doomedmap.cpp \
	g_game.cpp \
	g_hub.cpp \
	g_level.cpp \
	g_mapinfo.cpp \
	g_skill.cpp \
	gameconfigfile.cpp \
	gi.cpp \
	gitinfo.cpp \
	hu_scores.cpp \
	i_net.cpp \
	i_time.cpp \
	info.cpp \
	keysections.cpp \
	m_alloc.cpp \
	m_argv.cpp \
	m_bbox.cpp \
	m_cheat.cpp \
	m_joy.cpp \
	m_misc.cpp \
	m_png.cpp \
	m_random.cpp \
	memarena.cpp \
	md5.cpp \
	name.cpp \
	nodebuild.cpp \
	nodebuild_classify_nosse2.cpp \
	nodebuild_events.cpp \
	nodebuild_extract.cpp \
	nodebuild_gl.cpp \
	nodebuild_utility.cpp \
	p_3dfloors.cpp \
	p_3dmidtex.cpp \
	p_acs.cpp \
	p_actionfunctions.cpp \
	p_ceiling.cpp \
	p_conversation.cpp \
	p_destructible.cpp \
	p_doors.cpp \
	p_effect.cpp \
	p_enemy.cpp \
	p_floor.cpp \
	p_glnodes.cpp \
	p_interaction.cpp \
	p_lights.cpp \
	p_linkedsectors.cpp \
	p_lnspec.cpp \
	p_map.cpp \
	p_maputl.cpp \
	p_mobj.cpp \
	p_openmap.cpp \
	p_pillar.cpp \
	p_plats.cpp \
	p_pspr.cpp \
	p_pusher.cpp \
	p_saveg.cpp \
	p_scroll.cpp \
	p_secnodes.cpp \
	p_sectors.cpp \
	p_setup.cpp \
	p_sight.cpp \
	p_slopes.cpp \
	p_spec.cpp \
	p_states.cpp \
	p_switch.cpp \
	p_tags.cpp \
	p_teleport.cpp \
	p_terrain.cpp \
	p_things.cpp \
	p_tick.cpp \
	p_trace.cpp \
	p_udmf.cpp \
	p_usdf.cpp \
	p_user.cpp \
	p_xlat.cpp \
	parsecontext.cpp \
	po_man.cpp \
	portal.cpp \
	r_utility.cpp \
	r_sky.cpp \
	r_videoscale.cpp \
	sound/s_advsound.cpp \
	sound/s_environment.cpp \
	sound/s_reverbedit.cpp \
	sound/s_sndseq.cpp \
	sound/s_doomsound.cpp \
	sound/s_sound.cpp \
	sound/s_music.cpp \
	s_playlist.cpp \
	serializer.cpp \
	sc_man.cpp \
	scriptutil.cpp \
	st_stuff.cpp \
	statistics.cpp \
	stats.cpp \
	stringtable.cpp \
	teaminfo.cpp \
	umapinfo.cpp \
	v_2ddrawer.cpp \
	v_blend.cpp \
	v_collection.cpp \
	v_draw.cpp \
	v_font.cpp \
	v_framebuffer.cpp \
	v_palette.cpp \
	v_pfx.cpp \
	v_text.cpp \
	v_video.cpp \
	w_wad.cpp \
	wi_stuff.cpp \
	utf8.cpp \
	zstrformat.cpp \
	g_inventory/a_keys.cpp \
	g_inventory/a_pickups.cpp \
	g_inventory/a_weapons.cpp \
	g_shared/a_action.cpp \
	g_shared/a_decals.cpp \
	g_shared/a_dynlight.cpp \
	g_shared/a_flashfader.cpp \
	g_shared/a_lightning.cpp \
	g_shared/a_morph.cpp \
	g_shared/a_quake.cpp \
	g_shared/a_specialspot.cpp \
	g_shared/hudmessages.cpp \
	g_shared/shared_hud.cpp \
	g_statusbar/sbarinfo.cpp \
	g_statusbar/sbar_mugshot.cpp \
	g_statusbar/shared_sbar.cpp \
	gl/data/gl_vertexbuffer.cpp \
	gl/data/gl_uniformbuffer.cpp \
	gl/dynlights/gl_lightbuffer.cpp \
	gl/dynlights/gl_shadowmap.cpp \
	gl/models/gl_models.cpp \
	gl/renderer/gl_quaddrawer.cpp \
	gl/renderer/gl_renderer.cpp \
	gl/renderer/gl_renderstate.cpp \
	gl/renderer/gl_renderbuffers.cpp \
	gl/renderer/gl_lightdata.cpp \
	gl/renderer/gl_postprocess.cpp \
	gl/renderer/gl_postprocessstate.cpp \
	gl/renderer/gl_stereo3d.cpp \
	gl/shaders/gl_shader.cpp \
	gl/shaders/gl_shaderprogram.cpp \
	gl/shaders/gl_postprocessshader.cpp \
	gl/stereo3d/gl_openxrdevice.cpp \
	gl_load/gl_interface.cpp \
	gl/system/gl_framebuffer.cpp \
	gl/system/gl_debug.cpp \
	gl/system/gl_wipe.cpp \
	gl/textures/gl_hwtexture.cpp \
	gl/textures/gl_samplers.cpp \
	hwrenderer/data/flatvertices.cpp \
	hwrenderer/dynlights/hw_aabbtree.cpp \
	hwrenderer/dynlights/hw_shadowmap.cpp \
	hwrenderer/scene/hw_skydome.cpp \
	hwrenderer/postprocessing/hw_postprocess.cpp \
	hwrenderer/postprocessing/hw_postprocess_cvars.cpp \
	hwrenderer/postprocessing/hw_postprocessshader.cpp \
	hwrenderer/postprocessing/hw_shadowmapshader.cpp \
	hwrenderer/postprocessing/hw_presentshader.cpp \
	hwrenderer/postprocessing/hw_present3dRowshader.cpp \
	hwrenderer/textures/hw_material.cpp \
	hwrenderer/textures/hw_precache.cpp \
	hwrenderer/utility/hw_clock.cpp \
	hwrenderer/utility/hw_cvars.cpp \
	hwrenderer/utility/hw_lighting.cpp \
	hwrenderer/utility/hw_shaderpatcher.cpp \
	hwrenderer/utility/hw_vrmodes.cpp \
	menu/joystickmenu.cpp \
	menu/loadsavemenu.cpp \
	menu/menu.cpp \
	menu/menudef.cpp \
	menu/messagebox.cpp \
	menu/optionmenu.cpp \
	menu/playermenu.cpp \
	menu/resolutionmenu.cpp \
	menu/profiledef.cpp \
	resourcefiles/ancientzip.cpp \
	resourcefiles/file_7z.cpp \
	resourcefiles/file_grp.cpp \
	resourcefiles/file_lump.cpp \
	resourcefiles/file_rff.cpp \
	resourcefiles/file_wad.cpp \
	resourcefiles/file_zip.cpp \
	resourcefiles/file_pak.cpp \
	resourcefiles/file_directory.cpp \
	resourcefiles/resourcefile.cpp \
	textures/animations.cpp \
	textures/anim_switches.cpp \
	textures/bitmap.cpp \
	textures/texture.cpp \
	textures/texturemanager.cpp \
	textures/skyboxtexture.cpp \
	textures/formats/automaptexture.cpp \
	textures/formats/brightmaptexture.cpp \
	textures/formats/buildtexture.cpp \
	textures/formats/canvastexture.cpp \
	textures/formats/ddstexture.cpp \
	textures/formats/flattexture.cpp \
	textures/formats/imgztexture.cpp \
	textures/formats/jpegtexture.cpp \
	textures/formats/md5check.cpp \
	textures/formats/multipatchtexture.cpp \
	textures/formats/patchtexture.cpp \
	textures/formats/pcxtexture.cpp \
	textures/formats/pngtexture.cpp \
	textures/formats/rawpagetexture.cpp \
	textures/formats/emptytexture.cpp \
	textures/formats/shadertexture.cpp \
	textures/formats/tgatexture.cpp \
	textures/formats/worldtexture.cpp \
	textures/formats/warptexture.cpp \
	textures/hires/hqresize.cpp \
	textures/hires/hirestex.cpp \
	xlat/parse_xlat.cpp \
	fragglescript/t_func.cpp \
	fragglescript/t_load.cpp \
	fragglescript/t_oper.cpp \
	fragglescript/t_parse.cpp \
	fragglescript/t_prepro.cpp \
	fragglescript/t_script.cpp \
	fragglescript/t_spec.cpp \
	fragglescript/t_variable.cpp \
	fragglescript/t_cmd.cpp \
	intermission/intermission.cpp \
	intermission/intermission_parse.cpp \
	r_data/colormaps.cpp \
	r_data/gldefs.cpp \
	r_data/a_dynlightdata.cpp \
	r_data/r_translate.cpp \
	r_data/sprites.cpp \
	r_data/portalgroups.cpp \
	r_data/voxels.cpp \
	r_data/renderinfo.cpp \
	r_data/renderstyle.cpp \
	r_data/r_interpolate.cpp \
	r_data/r_vanillatrans.cpp \
	r_data/models/models_md3.cpp \
	r_data/models/models_md2.cpp \
	r_data/models/models_voxel.cpp \
	r_data/models/models_ue1.cpp \
	r_data/models/models_obj.cpp \
	scripting/symbols.cpp \
	scripting/vmiterators.cpp \
	scripting/vmthunks.cpp \
	scripting/vmthunks_actors.cpp \
	scripting/types.cpp \
	scripting/thingdef.cpp \
	scripting/thingdef_data.cpp \
	scripting/thingdef_properties.cpp \
	scripting/backend/codegen.cpp \
	scripting/backend/scopebarrier.cpp \
	scripting/backend/dynarrays.cpp \
	scripting/backend/vmbuilder.cpp \
	scripting/backend/vmdisasm.cpp \
	scripting/decorate/olddecorations.cpp \
	scripting/decorate/thingdef_exp.cpp \
	scripting/decorate/thingdef_parse.cpp \
	scripting/decorate/thingdef_states.cpp \
	scripting/vm/vmexec.cpp \
	scripting/vm/vmframe.cpp \
	scripting/zscript/ast.cpp \
	scripting/zscript/zcc_compile.cpp \
	scripting/zscript/zcc_parser.cpp \
	sfmt/SFMT.cpp \
	sound/music/i_music.cpp \
	sound/music/i_soundfont.cpp \
	sound/backend/i_sound.cpp \
	sound/music/music_config.cpp \
	events.cpp \
	

QZDOOM_SRC = \
   ../../QzDoom/TBXR_Common.cpp \
   ../../QzDoom/QzDoom_OpenXR.cpp \
   ../../QzDoom/OpenXrInput.cpp \
   ../../QzDoom/VrInputCommon.cpp \
   ../../QzDoom/VrInputDefault.cpp \
   ../../QzDoom/mathlib.c \
   ../../QzDoom/matrixlib.c \
   ../../QzDoom/argtable3.c


LOCAL_SRC_FILES = \
    __autostart.cpp \
    $(QZDOOM_SRC) \
    $(ANDROID_SRC_FILES) \
    $(PLAT_POSIX_SOURCES) \
    $(PLAT_NOSDL_SOURCES) \
    $(FASTMATH_SOURCES) \
    $(PCH_SOURCES) \
	x86.cpp \
	strnatcmp.c \
	zstring.cpp \
	dictionary.cpp \
	math/asin.c \
	math/atan.c \
	math/const.c \
	math/cosh.c \
	math/exp.c \
	math/isnan.c \
	math/log.c \
	math/log10.c \
	math/mtherr.c \
	math/polevl.c \
	math/pow.c \
	math/powi.c \
	math/sin.c \
	math/sinh.c \
	math/sqrt.c \
	math/tan.c \
	math/tanh.c \
	math/fastsin.cpp \
	zzautozend.cpp


# Turn down optimisation of this file so clang doesnt produce ldrd instructions which are missaligned
p_acs.cpp_CFLAGS := -O1

LOCAL_LDLIBS := -ldl -llog -lOpenSLES -landroid
LOCAL_LDLIBS += -lGLESv3

LOCAL_LDLIBS +=  -lEGL

# This is stop a linker warning for mp123 lib failing build
#LOCAL_LDLIBS += -Wl,--no-warn-shared-textrel

LOCAL_STATIC_LIBRARIES :=  sndfile mpg123 fluidsynth-static libjpeg zlib_lz lzma_lz gdtoa_lz dumb_lz gme_lz bzip2_lz zmusic_lz
LOCAL_SHARED_LIBRARIES :=  openal openxr_loader

LOCAL_STATIC_LIBRARIES +=

include $(BUILD_SHARED_LIBRARY)

$(call import-module,AndroidPrebuilt/jni)


