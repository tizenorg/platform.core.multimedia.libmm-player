lib_LTLIBRARIES = libmmfplayer_m3u8.la


libmmfplayer_m3u8_la_SOURCES = mm_player_m3u8.c

libmmfplayer_m3u8_la_CFLAGS =  -I$(srcdir)/include \
                          $(MMCOMMON_CFLAGS) \
                          $(GLIB_CFLAGS)

noinst_HEADERS = include/mm_player_m3u8.h

libmmfplayer_m3u8_la_LIBADD = $(GLIB_LIBS) \
                         $(MMCOMMON_LIBS)


lib_LTLIBRARIES += libmmfplayer.la

includelibmmfplayerdir = $(includedir)/mmf

includelibmmfplayer_HEADERS = include/mm_player.h \
			      include/mm_player_internal.h \
			      include/mm_player_sndeffect.h

libmmfplayer_la_SOURCES = mm_player.c \
			  mm_player_priv.c \
			  mm_player_ini.c \
			  mm_player_utils.c \
			  mm_player_asm.c \
			  mm_player_attrs.c \
			  mm_player_ahs_hls.c \
			  mm_player_ahs.c \
			  mm_player_capture.c \
			  mm_player_pd.c \
			  mm_player_streaming.c

libmmfplayer_la_CFLAGS =  -I$(srcdir)/include \
			  $(MMCOMMON_CFLAGS) \
			  $(MMTA_CFLAGS) \
			  $(MMUTIL_CFLAGS) \
			  $(GST_CFLAGS) \
			  $(GST_INTERFACE_CFLAGS) \
			  $(GST_APP_CFLAGS) \
			  $(MMSESSION_CFLAGS) \
			  $(AUDIOSESSIONMGR_CFLAGS) \
			  $(VCONF_CFLAGS) \
			  $(CRYPTO_CFLAGS)

noinst_HEADERS += include/mm_player_utils.h \
		 include/mm_player_ini.h \
		 include/mm_player_priv.h \
		 include/mm_player_asm.h \
		 include/mm_player_attrs.h \
		 include/mm_player_ahs.h \
		 include/mm_player_ahs_hls.h \
		 include/mm_player_capture.h \
		 include/mm_player_pd.h \
		 include/mm_player_streaming.h

libmmfplayer_la_DEPENDENCIES = $(top_builddir)/src/libmmfplayer_m3u8.la

libmmfplayer_la_LIBADD = $(GST_LIBS) \
	    	 	 $(MMCOMMON_LIBS) \
	    	 	 $(MMTA_LIBS) \
	    	 	 $(MMUTIL_LIBS) \
			 $(GST_INTERFACE_LIBS) \
			 $(GST_APP_LIBS) \
			 $(INIPARSER_LIBS) \
			 $(CRYPTO_LIBS) \
			 $(MMSESSION_LIBS) \
			 $(AUDIOSESSIONMGR_LIBS) \
			 $(VCONF_LIBS) \
			 $(top_builddir)/src/libmmfplayer_m3u8.la

libmmfplayer_la_CFLAGS += $(MMLOG_CFLAGS) -DMMF_LOG_OWNER=0x008 -DMMF_DEBUG_PREFIX=\"MMF-PLAYER\" -D_INTERNAL_SESSION_MANAGER_
libmmfplayer_la_LIBADD += $(MMLOG_LIBS)
