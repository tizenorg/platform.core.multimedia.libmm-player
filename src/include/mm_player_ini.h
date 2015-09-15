/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, YeJin Cho <cho.yejin@samsung.com>,
 * Seungbae Shin <seungbae.shin@samsung.com>, YoungHwan An <younghwan_.an@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef __MM_PLAYER_INI_H__
#define __MM_PLAYER_INI_H__

#include <glib.h>
#include <mm_types.h>
#include "mm_player_audioeffect.h"

#ifdef __cplusplus
	extern "C" {
#endif


#define MM_PLAYER_INI_DEFAULT_PATH	"/usr/etc/mmfw_player.ini"
#define MM_PLAYER_INI_DEFAULT_AUDIOEFFECT_PATH	"/usr/etc/mmfw_player_audio_effect.ini"

#define PLAYER_INI_MAX_STRLEN	100
#define PLAYER_INI_MAX_PARAM_STRLEN	256

#define PLAYER_INI_MAX_ELEMENT	10

/* NOTE : MMPlayer has no initalizing API for library itself
 * so we cannot decide when those ini values to be released.
 * this is the reason of all string items are static array.
 * make it do with malloc when MMPlayerInitialize() API created
 * before that time, we should be careful with size limitation
 * of each string item.
 */
enum keyword_type
{
	KEYWORD_EXCLUDE,	// for element exclude keyworld
	KEYWORD_DUMP		// for dump element keyworld
};

typedef struct __mm_player_ini
{
	/* general */
	gchar videosink_element_x[PLAYER_INI_MAX_STRLEN];
	gchar videosink_element_evas[PLAYER_INI_MAX_STRLEN];
	gchar videosink_element_fake[PLAYER_INI_MAX_STRLEN];
	gchar videosink_element_remote[PLAYER_INI_MAX_STRLEN];
	gchar videosrc_element_remote[PLAYER_INI_MAX_STRLEN];
	gchar videoconverter_element[PLAYER_INI_MAX_STRLEN];
	gchar audioresampler_element[PLAYER_INI_MAX_STRLEN];
	gchar audiosink_element[PLAYER_INI_MAX_STRLEN];
	gboolean skip_rescan;
	gboolean generate_dot;
	gboolean use_system_clock;
	gint live_state_change_timeout;
	gint localplayback_state_change_timeout;
	gint delay_before_repeat;
	gint eos_delay;

	gchar gst_param[5][PLAYER_INI_MAX_PARAM_STRLEN];
	gchar exclude_element_keyword[PLAYER_INI_MAX_ELEMENT][PLAYER_INI_MAX_STRLEN];
	gboolean async_start;
	gboolean disable_segtrap;

	/* http streaming */
	gchar httpsrc_element[PLAYER_INI_MAX_STRLEN];
	gchar http_file_buffer_path[PLAYER_INI_MAX_STRLEN];
	gdouble http_buffering_limit;
	guint http_max_size_bytes;
	gdouble http_buffering_time;
	gint http_timeout;

	/* audio effect */
	gchar audioeffect_element[PLAYER_INI_MAX_STRLEN];
	gchar audioeffect_element_custom[PLAYER_INI_MAX_STRLEN];

	/* audio effect preset mode */
	gboolean use_audio_effect_preset;
	gboolean audio_effect_preset_list[MM_AUDIO_EFFECT_PRESET_NUM];
	gboolean audio_effect_preset_earphone_only_list[MM_AUDIO_EFFECT_PRESET_NUM];

	/* audio effect custom mode */
	gboolean use_audio_effect_custom;
	gboolean audio_effect_custom_list[MM_AUDIO_EFFECT_CUSTOM_NUM];
	gboolean audio_effect_custom_earphone_only_list[MM_AUDIO_EFFECT_CUSTOM_NUM];
	gint audio_effect_custom_eq_band_num;
	gint audio_effect_custom_eq_band_width[MM_AUDIO_EFFECT_EQ_BAND_NUM_MAX];
	gint audio_effect_custom_eq_band_freq[MM_AUDIO_EFFECT_EQ_BAND_NUM_MAX];
	gint audio_effect_custom_ext_num;
	gint audio_effect_custom_min_level_list[MM_AUDIO_EFFECT_CUSTOM_NUM];
	gint audio_effect_custom_max_level_list[MM_AUDIO_EFFECT_CUSTOM_NUM];

	/* dump buffer for debug */
	gchar dump_element_keyword[PLAYER_INI_MAX_ELEMENT][PLAYER_INI_MAX_STRLEN];
	gchar dump_element_path[PLAYER_INI_MAX_STRLEN];
	gboolean set_dump_element_flag;
} mm_player_ini_t;

/* default values if each values are not specified in inifile */
/* general */
#define DEFAULT_AUDIO_EFFECT_ELEMENT			""
#define DEFAULT_USE_AUDIO_EFFECT_PRESET			FALSE
#define DEFAULT_AUDIO_EFFECT_PRESET_LIST		""
#define DEFAULT_AUDIO_EFFECT_PRESET_LIST_EARPHONE_ONLY	""
#define DEFAULT_USE_AUDIO_EFFECT_CUSTOM			FALSE
#define DEFAULT_AUDIO_EFFECT_CUSTOM_LIST		""
#define DEFAULT_AUDIO_EFFECT_CUSTOM_LIST_EARPHONE_ONLY	""
#define DEFAULT_AUDIO_EFFECT_CUSTOM_EQ_BAND_NUM		0
#define DEFAULT_AUDIO_EFFECT_CUSTOM_EQ_BAND_WIDTH		""
#define DEFAULT_AUDIO_EFFECT_CUSTOM_EQ_BAND_FREQ		""
#define DEFAULT_AUDIO_EFFECT_CUSTOM_EQ_MIN		0
#define DEFAULT_AUDIO_EFFECT_CUSTOM_EQ_MAX		0
#define DEFAULT_AUDIO_EFFECT_CUSTOM_EXT_NUM		0
#define DEFAULT_USE_SINK_HANDLER			TRUE
#define DEFAULT_SKIP_RESCAN				TRUE
#define DEFAULT_GENERATE_DOT				FALSE
#define DEFAULT_USE_SYSTEM_CLOCK		TRUE
#define DEFAULT_DELAY_BEFORE_REPEAT	 		50 /* msec */
#define DEFAULT_EOS_DELAY 				150 /* msec */
#define DEFAULT_VIDEOSINK_X				"xvimagesink"
#define DEFAULT_VIDEOSINK_EVAS				"evaspixmapsink"
#define DEFAULT_VIDEOSINK_FAKE				"fakesink"
#define DEFAULT_VIDEOSINK_REMOTE			"shmsink"
#define DEFAULT_VIDEOSRC_REMOTE				"shmsrc"
#define DEFAULT_AUDIORESAMPLER			"audioresample"
#define DEFAULT_AUDIOSINK				"pulsesink"
#define DEFAULT_GST_PARAM				""
#define DEFAULT_EXCLUDE_KEYWORD				""
#define DEFAULT_ASYNC_START				TRUE
#define DEFAULT_DISABLE_SEGTRAP				TRUE
#define DEFAULT_VIDEO_CONVERTER				""
#define DEFAULT_LIVE_STATE_CHANGE_TIMEOUT 		30 /* sec */
#define DEFAULT_LOCALPLAYBACK_STATE_CHANGE_TIMEOUT 	10 /* sec */
/* http streaming */
#define DEFAULT_HTTPSRC				"souphttpsrc"
#define DEFAULT_HTTP_FILE_BUFFER_PATH		"/opt/usr/media"
#define DEFAULT_HTTP_BUFFERING_LIMIT	99.0		/* percent */
#define DEFAULT_HTTP_MAX_SIZE_BYTES		1048576		/* bytes : 1 MBytes  */
#define DEFAULT_HTTP_BUFFERING_TIME		1.2			/* sec */
#define DEFAULT_HTTP_TIMEOUT			-1			/* infinite retry */

/* dump buffer for debug */
#define DEFAULT_DUMP_ELEMENT_KEYWORD				""
#define DEFAULT_DUMP_ELEMENT_PATH				"/opt/usr/media/"

/* NOTE : following content should be same with above default values */
/* FIXIT : need smarter way to generate default ini file. */
/* FIXIT : finally, it should be an external file */
#define MM_PLAYER_DEFAULT_INI \
"\
[general] \n\
\n\
disable segtrap = yes ; same effect with --gst-disable-segtrap \n\
\n\
; set default video sink but, it can be replaced with others selected by application\n\
; 0:v4l2sink, 1:ximagesink, 2:xvimagesink, 3:fakesink 4:evasimagesink 5:glimagesink\n\
videosink element = 2 \n\
\n\
video converter element = \n\
\n\
audiosink element = pulsesink \n\
\n\
; if yes. gstreamer will not update registry \n\
skip rescan = yes \n\
\n\
delay before repeat = 50 ; msec\n\
\n\
; comma separated list of tocken which elemnts has it in it's name will not be used \n\
element exclude keyword = \n\
\n\
async start = yes \n\
\n\
; parameters for initializing gstreamer \n\
gstparam1 = --gst-debug=2\n\
gstparam2 = \n\
gstparam3 = \n\
gstparam4 = \n\
gstparam5 = \n\
\n\
; generating dot file representing pipeline state \n\
; export GST_DEBUG_DUMP_DOT_DIR=/tmp/\n\
generate dot = no \n\
\n\
; parameter is for only video to be determined \n\
; which clock will be used \n\
; if yes, system clock will be used \n\
; apart from this, audiosink is clock provider for audio \n\
use system clock = yes \n\
\n\
; allowed timeout for changing pipeline state \n\
live state change timeout = 30 ; sec \n\
localplayback state change timeout = 4 ; sec \n\
\n\
; delay in msec for sending EOS \n\
eos delay = 150 ; msec \n\
\n\
\n\
[http streaming] \n\
\n\
httppsrc element = souphttpsrc \n\
\n\
; if set, use file or not use memory for buffering\n\
http file buffer path = /opt/usr/media\n\
\n\
http buffering limit = 99 ; percent\n\
\n\
http max size bytes = 1048576 ; bytes\n\
\n\
http buffering time = 1.2 \n\
\n\
http timeout = -1 ; infinite retry \n\
\n\
"

int
mm_player_ini_load(mm_player_ini_t* ini);

int
mm_player_audio_effect_ini_load(mm_player_ini_t* ini);

#ifdef __cplusplus
	}
#endif

#endif
