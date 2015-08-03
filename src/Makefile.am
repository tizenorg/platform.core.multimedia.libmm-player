lib_LTLIBRARIES = libmmfplayer.la

includelibmmfplayerdir = $(includedir)/mmf

includelibmmfplayer_HEADERS = include/mm_player.h \
			      include/mm_player_internal.h \
			      include/mm_player_audioeffect.h \
			      include/mused/mm_player_mused.h

libmmfplayer_la_SOURCES = mm_player.c \
			  mm_player_priv.c \
			  mm_player_ini.c \
			  mm_player_utils.c \
			  mm_player_sound_focus.c \
			  mm_player_attrs.c \
			  mm_player_capture.c \
			  mm_player_pd.c \
			  mm_player_streaming.c \
			  mm_player_tracks.c \
			  mm_player_audioeffect.c \
			  mm_player_es.c \
			  mused/mm_player_mused.c

libmmfplayer_la_CFLAGS =  -I$(srcdir)/include \
			$(MMCOMMON_CFLAGS) \
			$(MMUTIL_CFLAGS) \
			$(GST_CFLAGS) \
			$(GST_VIDEO_CFLAGS) \
			$(GST_APP_CFLAGS) \
			$(MMSESSION_CFLAGS) \
			$(MMSOUND_CFLAGS) \
			$(VCONF_CFLAGS) \
			$(ICU_CFLAGS) \
			$(UTILX_CFLAGS) \
			$(MEDIAPACKET_CFLAGS) \
			-Werror -Wno-deprecated -Wno-deprecated-declarations -Wno-cpp

noinst_HEADERS = include/mm_player_utils.h \
		 include/mm_player_ini.h \
		 include/mm_player_priv.h \
		 include/mm_player_sound_focus.h \
		 include/mm_player_attrs.h \
		 include/mm_player_capture.h \
		 include/mm_player_pd.h \
		 include/mm_player_tracks.h \
		 include/mm_player_streaming.h \
		 include/mm_player_es.h

libmmfplayer_la_LIBADD = $(GST_LIBS) \
	    	$(MMCOMMON_LIBS) \
	    	$(MMUTIL_LIBS) \
		$(GST_INTERFACE_LIBS) \
		$(GST_VIDEO_LIBS) \
		$(GST_APP_LIBS) \
		$(INIPARSER_LIBS) \
		$(MMSESSION_LIBS) \
		$(MMSOUND_LIBS) \
		$(VCONF_LIBS) \
		$(ICU_LIBS) \
		$(MEDIAPACKET_LIBS) \
		$(UTILX_LIBS)

libmmfplayer_la_CFLAGS += $(MMLOG_CFLAGS) -DMMF_LOG_OWNER=0x008 -DMMF_DEBUG_PREFIX=\"MMF-PLAYER\" -D_INTERNAL_SESSION_MANAGER_
libmmfplayer_la_LIBADD += $(MMLOG_LIBS)

if IS_SDK
libmmfplayer_la_CFLAGS += -DIS_SDK
endif

if WAYLAND_SUPPORT
libmmfplayer_la_CFLAGS += $(GST_WAYLAND_CFLAGS)
libmmfplayer_la_LIBADD += $(GST_WAYLAND_LIBS)
endif
