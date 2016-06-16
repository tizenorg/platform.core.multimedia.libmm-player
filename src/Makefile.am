lib_LTLIBRARIES = libmmfplayer.la

includelibmmfplayerdir = $(includedir)/mmf

includelibmmfplayer_HEADERS = include/mm_player.h \
			      include/mm_player_internal.h \
			      include/mm_player_audioeffect.h

libmmfplayer_la_SOURCES = mm_player.c \
			  mm_player_priv.c \
			  mm_player_utils.c \
			  mm_player_resource.c \
			  mm_player_sound_focus.c \
			  mm_player_capture.c \
			  mm_player_pd.c \
			  mm_player_streaming.c \
			  mm_player_tracks.c \
			  mm_player_audioeffect.c \
			  mm_player_es.c \
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
			$(MRP_RESOURCE_CFLAGS) \
			$(MRP_GLIB_CFLAGS) \
			$(VCONF_CFLAGS) \
			$(ICU_CFLAGS) \
			$(UTILX_CFLAGS) \
			$(MEDIAPACKET_CFLAGS) \
			$(DLOG_CFLAGS) \
			$(EVAS_CFLAGS) \
			$(ECORE_CFLAGS) \
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
		$(MRP_RESOURCE_LIBS) \
		$(MRP_GLIB_LIBS) \
		$(VCONF_LIBS) \
		$(ICU_LIBS) \
		$(MEDIAPACKET_LIBS) \
		$(DLOG_LIBS) \
		$(UTILX_LIBS) \
		$(EVAS_LIBS) \
		$(ECORE_LIBS)


if IS_SDK
libmmfplayer_la_CFLAGS += -DIS_SDK
endif

if WAYLAND_SUPPORT
libmmfplayer_la_CFLAGS += $(GST_WAYLAND_CFLAGS)
libmmfplayer_la_LIBADD += $(GST_WAYLAND_LIBS)
endif

libmmfplayer_la_CFLAGS += $(TZPLATFORM_CONFIG_CFLAGS)
libmmfplayer_la_LIBADD += $(TZPLATFORM_CONFIG_LIBS)