lib_LTLIBRARIES = libmmfplayer.la libmmfplayerclient.la

includelibmmfplayerdir = $(includedir)/mmf

includelibmmfplayer_HEADERS = include/mm_player.h \
			      include/mm_player_internal.h \
			      include/mm_player_audioeffect.h \
			      include/mm_player_mused.h

libmmfplayer_la_SOURCES = server/mm_player.c \
			  server/mm_player_priv.c \
			  server/mm_player_utils.c \
			  server/mm_player_sound_focus.c \
			  server/mm_player_capture.c \
			  server/mm_player_pd.c \
			  server/mm_player_streaming.c \
			  server/mm_player_tracks.c \
			  server/mm_player_audioeffect.c \
			  server/mm_player_es.c \
			  mm_player_common_priv.c \
			  mm_player_attrs.c \
			  mm_player_ini.c

libmmfplayerclient_la_SOURCES = client/mm_player_client.c \
			  mm_player_common_priv.c \
			  mm_player_attrs.c \
			  mm_player_ini.c

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
			$(DLOG_CFLAGS) \
			-Werror -Wno-deprecated -Wno-deprecated-declarations -Wno-cpp

libmmfplayerclient_la_CFLAGS =  -I$(srcdir)/include \
			$(MMCOMMON_CFLAGS) \
			$(MMUTIL_CFLAGS) \
			$(GST_CFLAGS) \
			$(GST_VIDEO_CFLAGS) \
			$(GST_APP_CFLAGS) \
			$(VCONF_CFLAGS) \
			$(DLOG_CFLAGS) \
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
		$(DLOG_LIBS) \
		$(UTILX_LIBS)

libmmfplayerclient_la_LIBADD = $(GST_LIBS) \
	    	$(MMCOMMON_LIBS) \
	    	$(MMUTIL_LIBS) \
		$(GST_INTERFACE_LIBS) \
		$(GST_VIDEO_LIBS) \
		$(GST_APP_LIBS) \
		$(INIPARSER_LIBS) \
		$(DLOG_LIBS) \
		$(VCONF_LIBS)

if IS_SDK
libmmfplayer_la_CFLAGS += -DIS_SDK
endif

if WAYLAND_SUPPORT
libmmfplayer_la_CFLAGS += $(GST_WAYLAND_CFLAGS)
libmmfplayer_la_LIBADD += $(GST_WAYLAND_LIBS)
libmmfplayerclient_la_CFLAGS += $(GST_WAYLAND_CFLAGS)
libmmfplayerclient_la_LIBADD += $(GST_WAYLAND_LIBS)
endif
