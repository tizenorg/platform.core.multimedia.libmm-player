ifeq ($(ARCH),target)
	PKG_CONFIG_PATH=/usr/lib/pkgconfig
	export PKG_CONFIG_PATH
	CC=arm-linux-gcc -Wall
	CXX=arm-linux-g++ -Wall
else
	PKG_CONFIG_PATH=/usr/lib/pkgconfig
	export PKG_CONFIG_PATH
	CC=gcc -Wall
    CXX=gcc -Wall
endif

TS1=utc_mm_player_create_func
TS2=utc_mm_player_destroy_func
TS3=utc_mm_player_realize_func
TS4=utc_mm_player_unrealize_func
TS5=utc_mm_player_start_func
TS6=utc_mm_player_stop_func
TS7=utc_mm_player_pause_func
TS8=utc_mm_player_resume_func
TS9=utc_mm_player_set_volume_func
TS10=utc_mm_player_get_volume_func
TS11=utc_mm_player_set_position_func
TS12=utc_mm_player_get_position_func
TS13=utc_mm_player_set_mute_func
TS14=utc_mm_player_get_mute_func
TS15=utc_mm_player_activate_section_repeat_func
TS16=utc_mm_player_deactivate_section_repeat_func
TS17=utc_mm_player_set_subtitle_silent_func
TS18=utc_mm_player_get_subtitle_silent_func
TS19=utc_mm_player_get_state_func
TS20=utc_mm_player_set_message_callback_func
TS21=utc_mm_player_set_audio_stream_callback_func
TS22=utc_mm_player_set_attribute_func
TS23=utc_mm_player_get_attribute_func
TS24=utc_mm_player_get_attribute_info_func
TS25=utc_mm_player_common

LIBS = `pkg-config --libs mm-player`
LIBS +=-L/usr/lib/:/usr/lib/pkgconfig
LIBS +=$(TET_ROOT)/lib/tet3/tcm_s.o
LIBS +=-L$(TET_ROOT)/lib/tet3 -ltcm_s
LIBS +=-L$(TET_ROOT)/lib/tet3/ -lapi_s

INCS = -I. `pkg-config --cflags  mm-player `
INCS += -I$(TET_ROOT)/inc/tet3
INCS += -I/usr/include/mmf

CFLAGS = $(INCS) 
CC += $(CFLAGS)
LDFLAGS = $(LIBS)

all : $(TS1) $(TS2) $(TS3) $(TS4) $(TS5) $(TS6) $(TS7) $(TS8) $(TS9) $(TS10) $(TS11) $(TS12) $(TS13) $(TS14) $(TS15) $(TS16) $(TS17) $(TS18) $(TS19) $(TS20) $(TS21) $(TS22) $(TS23) $(TS24)

$(TS1): $(TS1).c $(TS25).c
	$(CC) -o $(TS1) $(TS25).c $(TS1).c $(LDFLAGS)

$(TS2): $(TS2).c $(TS25).c
	$(CC) -o $(TS2) $(TS25).c $(TS2).c $(LDFLAGS)
	
$(TS3): $(TS3).c $(TS25).c
	$(CC) -o $(TS3) $(TS25).c $(TS3).c $(LDFLAGS)

$(TS4): $(TS4).c $(TS25).c
	$(CC) -o $(TS4) $(TS25).c $(TS4).c $(LDFLAGS)

$(TS5): $(TS5).c $(TS25).c
	$(CC) -o $(TS5) $(TS25).c $(TS5).c $(LDFLAGS)
	
$(TS6): $(TS6).c $(TS25).c
	$(CC) -o $(TS6) $(TS25).c $(TS6).c $(LDFLAGS)

$(TS7): $(TS7).c $(TS25).c
	$(CC) -o $(TS7) $(TS25).c $(TS7).c $(LDFLAGS)

$(TS8): $(TS8).c $(TS25).c
	$(CC) -o $(TS8) $(TS25).c $(TS8).c $(LDFLAGS)

$(TS9): $(TS9).c $(TS25).c
	$(CC) -o $(TS9) $(TS25).c $(TS9).c $(LDFLAGS)

$(TS10): $(TS10).c $(TS25).c
	$(CC) -o $(TS10) $(TS25).c $(TS10).c $(LDFLAGS)

$(TS11): $(TS11).c $(TS25).c
	$(CC) -o $(TS11) $(TS25).c $(TS11).c $(LDFLAGS)

$(TS12): $(TS12).c $(TS25).c
	$(CC) -o $(TS12) $(TS25).c $(TS12).c $(LDFLAGS)

$(TS13): $(TS13).c $(TS25).c
	$(CC) -o $(TS13) $(TS25).c $(TS13).c $(LDFLAGS)

$(TS14): $(TS14).c $(TS25).c
	$(CC) -o $(TS14) $(TS25).c $(TS14).c $(LDFLAGS)

$(TS15): $(TS15).c $(TS25).c
	$(CC) -o $(TS15) $(TS25).c $(TS15).c $(LDFLAGS)

$(TS16): $(TS16).c $(TS25).c
	$(CC) -o $(TS16) $(TS25).c $(TS16).c $(LDFLAGS)

$(TS17): $(TS17).c $(TS25).c
	$(CC) -o $(TS17) $(TS25).c $(TS17).c $(LDFLAGS)

$(TS18): $(TS18).c $(TS25).c
	$(CC) -o $(TS18) $(TS25).c $(TS18).c $(LDFLAGS)

$(TS19): $(TS19).c $(TS25).c
	$(CC) -o $(TS19) $(TS25).c $(TS19).c $(LDFLAGS)

$(TS20): $(TS20).c $(TS25).c
	$(CC) -o $(TS20) $(TS25).c $(TS20).c $(LDFLAGS)

$(TS21): $(TS21).c $(TS25).c
	$(CC) -o $(TS21) $(TS25).c $(TS21).c $(LDFLAGS)

$(TS22): $(TS22).c $(TS25).c
	$(CC) -o $(TS22) $(TS25).c $(TS22).c $(LDFLAGS)

$(TS23): $(TS23).c $(TS25).c
	$(CC) -o $(TS23) $(TS25).c $(TS23).c $(LDFLAGS)

$(TS24): $(TS24).c $(TS25).c
	$(CC) -o $(TS24) $(TS25).c $(TS24).c $(LDFLAGS)	

clean:
	rm -rf *~ *.o $(TS1) $(TS2) $(TS3) $(TS4) $(TS5) $(TS6) $(TS7) $(TS8) $(TS9) $(TS10) $(TS11) $(TS12) $(TS13) $(TS14) $(TS15) $(TS16) $(TS17) $(TS18) $(TS19) $(TS20) $(TS21) $(TS22) $(TS23) $(TS24)
	


