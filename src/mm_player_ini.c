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

#ifndef __MM_PLAYER_INI_C__
#define __MM_PLAYER_INI_C__

/* includes here */
#include <glib.h>
#include <stdlib.h>
#include "iniparser.h"
#include <mm_player_ini.h>
#include "mm_debug.h"
#include <mm_error.h>
#include <glib/gstdio.h>

/* internal functions, macros here */
#ifdef MM_PLAYER_DEFAULT_INI
static gboolean	__generate_default_ini(void);
#endif
static void	__get_element_list(mm_player_ini_t* ini, gchar* str, int keyword_type);

static void __mm_player_ini_check_ini_status(void);

/* macro */
#define MMPLAYER_INI_GET_STRING( x_dict, x_item, x_ini, x_default ) \
do \
{ \
	gchar* str = iniparser_getstring(x_dict, x_ini, x_default); \
 \
	if ( str &&  \
		( strlen( str ) > 0 ) && \
		( strlen( str ) < PLAYER_INI_MAX_STRLEN ) ) \
	{ \
		strcpy ( x_item, str ); \
	} \
	else \
	{ \
		strcpy ( x_item, x_default ); \
	} \
}while(0)

#define MMPLAYER_INI_GET_COLOR( x_dict, x_item, x_ini, x_default ) \
do \
{ \
  gchar* str = iniparser_getstring(x_dict, x_ini, x_default); \
 \
  if ( str &&  \
    ( strlen( str ) > 0 ) && \
    ( strlen( str ) < PLAYER_INI_MAX_STRLEN ) ) \
  { \
    x_item = (guint) strtoul(str, NULL, 16); \
  } \
  else \
  { \
    x_item = (guint) strtoul(x_default, NULL, 16); \
  } \
}while(0)

/* x_ini is the list of index to set TRUE at x_list[index] */
#define MMPLAYER_INI_GET_BOOLEAN_FROM_LIST( x_dict, x_list, x_list_max, x_ini, x_default ) \
do \
{ \
		int index = 0; \
		const char *delimiters = " ,"; \
		char *usr_ptr = NULL; \
		char *token = NULL; \
		gchar temp_arr[PLAYER_INI_MAX_STRLEN] = {0}; \
		MMPLAYER_INI_GET_STRING( x_dict, temp_arr, x_ini, x_default); \
		token = strtok_r( temp_arr, delimiters, &usr_ptr ); \
		while (token) \
		{ \
			index = atoi(token); \
			if (index < 0 || index > x_list_max -1) \
			{ \
				debug_warning("%d is not valid index\n", index); \
			} \
			else \
			{ \
				x_list[index] = TRUE; \
			} \
			token = strtok_r( NULL, delimiters, &usr_ptr ); \
		} \
}while(0)

/* x_ini is the list of value to be set at x_list[index] */
#define MMPLAYER_INI_GET_INT_FROM_LIST( x_dict, x_list, x_list_max, x_ini, x_default ) \
do \
{ \
		int index = 0; \
		int value = 0; \
		const char *delimiters = " ,"; \
		char *usr_ptr = NULL; \
		char *token = NULL; \
		gchar temp_arr[PLAYER_INI_MAX_STRLEN] = {0}; \
		MMPLAYER_INI_GET_STRING(x_dict, temp_arr, x_ini, x_default); \
		token = strtok_r( temp_arr, delimiters, &usr_ptr ); \
		while (token) \
		{ \
			if ( index > x_list_max -1) \
			{ \
				debug_error("%d is not valid index\n", index); \
				break; \
			} \
			else \
			{ \
				value = atoi(token); \
				x_list[index] = value; \
				index++; \
			} \
			token = strtok_r( NULL, delimiters, &usr_ptr ); \
		} \
}while(0)

int
mm_player_ini_load(mm_player_ini_t* ini)
{
	dictionary * dict = NULL;
	gint idx = 0;

	__mm_player_ini_check_ini_status();

	/* first, try to load existing ini file */
	dict = iniparser_load(MM_PLAYER_INI_DEFAULT_PATH);

	/* if no file exists. create one with set of default values */
	if ( !dict )
	{
#ifdef MM_PLAYER_DEFAULT_INI
		debug_log("No inifile found. player will create default inifile.\n");
		if ( FALSE == __generate_default_ini() )
		{
			debug_warning("Creating default inifile failed. Player will use default values.\n");
		}
		else
		{
			/* load default ini */
			dict = iniparser_load(MM_PLAYER_INI_DEFAULT_PATH);
		}
#else
		debug_log("No ini file found. \n");
		return MM_ERROR_FILE_NOT_FOUND;
#endif
	}

	/* get ini values */
	memset( ini, 0, sizeof(mm_player_ini_t) );

	if ( dict ) /* if dict is available */
	{
		/* general */
		ini->disable_segtrap = iniparser_getboolean(dict, "general:disable segtrap", DEFAULT_DISABLE_SEGTRAP);
		ini->skip_rescan = iniparser_getboolean(dict, "general:skip rescan", DEFAULT_SKIP_RESCAN);
		ini->generate_dot = iniparser_getboolean(dict, "general:generate dot", DEFAULT_GENERATE_DOT);
		ini->provide_clock_for_music = iniparser_getboolean(dict, "general:provide clock for music", DEFAULT_PROVIDE_CLOCK_FOR_MUSIC);
		ini->provide_clock_for_movie = iniparser_getboolean(dict, "general:provide clock for movie", DEFAULT_PROVIDE_CLOCK_FOR_MOVIE);
		ini->live_state_change_timeout = iniparser_getint(dict, "general:live state change timeout", DEFAULT_LIVE_STATE_CHANGE_TIMEOUT);
		ini->localplayback_state_change_timeout = iniparser_getint(dict, "general:localplayback state change timeout", DEFAULT_LOCALPLAYBACK_STATE_CHANGE_TIMEOUT);
		ini->eos_delay = iniparser_getint(dict, "general:eos delay", DEFAULT_EOS_DELAY);
		ini->async_start = iniparser_getboolean(dict, "general:async start", DEFAULT_ASYNC_START);
		ini->multiple_codec_supported = iniparser_getboolean(dict, "general:multiple codec supported", DEFAULT_MULTIPLE_CODEC_SUPPORTED);

		ini->delay_before_repeat = iniparser_getint(dict, "general:delay before repeat", DEFAULT_DELAY_BEFORE_REPEAT);

		MMPLAYER_INI_GET_STRING(dict, ini->videosink_element_x, "general:videosink element x", DEFAULT_VIDEOSINK_X);
		MMPLAYER_INI_GET_STRING(dict, ini->videosink_element_evas, "general:videosink element evas", DEFAULT_VIDEOSINK_EVAS);
		MMPLAYER_INI_GET_STRING(dict, ini->videosink_element_fake, "general:videosink element fake", DEFAULT_VIDEOSINK_FAKE);
		MMPLAYER_INI_GET_STRING(dict, ini->name_of_drmsrc, "general:drmsrc element", DEFAULT_DRMSRC );
		MMPLAYER_INI_GET_STRING(dict, ini->name_of_audio_resampler, "general:audio resampler element", DEFAULT_AUDIORESAMPLER );
		MMPLAYER_INI_GET_STRING(dict, ini->name_of_audiosink, "general:audiosink element", DEFAULT_AUDIOSINK );
		MMPLAYER_INI_GET_STRING(dict, ini->name_of_video_converter, "general:video converter element", DEFAULT_VIDEO_CONVERTER );

		__get_element_list(ini,
			iniparser_getstring(dict, "general:element exclude keyword", DEFAULT_EXCLUDE_KEYWORD), KEYWORD_EXCLUDE);

		MMPLAYER_INI_GET_STRING(dict, ini->gst_param[0], "general:gstparam1", DEFAULT_GST_PARAM );
		MMPLAYER_INI_GET_STRING(dict, ini->gst_param[1], "general:gstparam2", DEFAULT_GST_PARAM );
		MMPLAYER_INI_GET_STRING(dict, ini->gst_param[2], "general:gstparam3", DEFAULT_GST_PARAM );
		MMPLAYER_INI_GET_STRING(dict, ini->gst_param[3], "general:gstparam4", DEFAULT_GST_PARAM );
		MMPLAYER_INI_GET_STRING(dict, ini->gst_param[4], "general:gstparam5", DEFAULT_GST_PARAM );

		/* http streaming */
		MMPLAYER_INI_GET_STRING( dict, ini->name_of_httpsrc, "http streaming:httpsrc element", DEFAULT_HTTPSRC );
		MMPLAYER_INI_GET_STRING( dict, ini->http_file_buffer_path, "http streaming:http file buffer path", DEFAULT_HTTP_FILE_BUFFER_PATH );
		ini->http_buffering_limit = iniparser_getdouble(dict, "http streaming:http buffering high limit", DEFAULT_HTTP_BUFFERING_LIMIT);
		ini->http_max_size_bytes = iniparser_getint(dict, "http streaming:http max size bytes", DEFAULT_HTTP_MAX_SIZE_BYTES);
		ini->http_buffering_time = iniparser_getdouble(dict, "http streaming:http buffering time", DEFAULT_HTTP_BUFFERING_TIME);
		ini->http_timeout = iniparser_getint(dict, "http streaming:http timeout", DEFAULT_HTTP_TIMEOUT);

		/* rtsp streaming */
		MMPLAYER_INI_GET_STRING( dict, ini->name_of_rtspsrc, "rtsp streaming:rtspsrc element", DEFAULT_RTSPSRC );
		ini->rtsp_buffering_time = iniparser_getint(dict, "rtsp streaming:rtsp buffering time", DEFAULT_RTSP_BUFFERING);
		ini->rtsp_rebuffering_time = iniparser_getint(dict, "rtsp streaming:rtsp rebuffering time", DEFAULT_RTSP_REBUFFERING);
		ini->rtsp_do_typefinding = iniparser_getboolean(dict, "rtsp streaming:rtsp do typefinding", DEFAULT_RTSP_DO_TYPEFINDING);
		ini->rtsp_error_concealment = iniparser_getboolean(dict, "rtsp streaming:rtsp error concealment", DEFAULT_RTSP_ERROR_CONCEALMENT);

		/* hw accelation */
		ini->use_video_hw_accel = iniparser_getboolean(dict, "hw accelation:use video hw accel", DEFAULT_USE_VIDEO_HW_ACCEL);

		/* priority */
		ini->use_priority_setting = iniparser_getboolean(dict, "priority:use priority setting", DEFAULT_USE_PRIORITY_SETTING);
		ini->demux_priority = iniparser_getint(dict, "priority:demux", DEFAULT_PRIORITY_DEMUX);
		ini->videosink_priority = iniparser_getint(dict, "priority:videosink", DEFAULT_PRIORITY_VIDEO_SINK);
		ini->audiosink_priority = iniparser_getint(dict, "priority:audiosink", DEFAULT_PRIORITY_AUDIO_SINK);
		ini->ringbuffer_priority = iniparser_getint(dict, "priority:ringbuffer", DEFAULT_PRIORITY_RINGBUFFER);

		/* dump buffer for debug */
		__get_element_list(ini,
			iniparser_getstring(dict, "general:dump element keyword", DEFAULT_EXCLUDE_KEYWORD), KEYWORD_DUMP);

		MMPLAYER_INI_GET_STRING(dict, ini->dump_element_path, "general:dump element path", DEFAULT_DUMP_ELEMENT_PATH);
	}
	else /* if dict is not available just fill the structure with default value */
	{
		debug_warning("failed to load ini. using hardcoded default\n");

		/* general */
		ini->disable_segtrap = DEFAULT_DISABLE_SEGTRAP;
		ini->skip_rescan = DEFAULT_SKIP_RESCAN;
		strncpy( ini->videosink_element_x, DEFAULT_VIDEOSINK_X, PLAYER_INI_MAX_STRLEN - 1 );
		strncpy( ini->videosink_element_evas, DEFAULT_VIDEOSINK_EVAS, PLAYER_INI_MAX_STRLEN - 1 );
		strncpy( ini->videosink_element_fake, DEFAULT_VIDEOSINK_FAKE, PLAYER_INI_MAX_STRLEN - 1 );
		ini->generate_dot = DEFAULT_GENERATE_DOT;
		ini->provide_clock_for_music = DEFAULT_PROVIDE_CLOCK_FOR_MUSIC;
		ini->provide_clock_for_movie = DEFAULT_PROVIDE_CLOCK_FOR_MOVIE;
		ini->live_state_change_timeout = DEFAULT_LIVE_STATE_CHANGE_TIMEOUT;
		ini->localplayback_state_change_timeout = DEFAULT_LOCALPLAYBACK_STATE_CHANGE_TIMEOUT;
		ini->eos_delay = DEFAULT_EOS_DELAY;
		ini->multiple_codec_supported = DEFAULT_MULTIPLE_CODEC_SUPPORTED;
		ini->async_start = DEFAULT_ASYNC_START;
		ini->delay_before_repeat = DEFAULT_DELAY_BEFORE_REPEAT;

		strncpy( ini->name_of_drmsrc, DEFAULT_DRMSRC, PLAYER_INI_MAX_STRLEN - 1 );
		strncpy( ini->name_of_audio_resampler, DEFAULT_AUDIORESAMPLER, PLAYER_INI_MAX_STRLEN -1 );
		strncpy( ini->name_of_audiosink, DEFAULT_AUDIOSINK, PLAYER_INI_MAX_STRLEN -1 );
		strncpy( ini->name_of_video_converter, DEFAULT_VIDEO_CONVERTER, PLAYER_INI_MAX_STRLEN -1 );

		{
			__get_element_list(ini, DEFAULT_EXCLUDE_KEYWORD, KEYWORD_EXCLUDE);
		}


		strncpy( ini->gst_param[0], DEFAULT_GST_PARAM, PLAYER_INI_MAX_PARAM_STRLEN - 1 );
		strncpy( ini->gst_param[1], DEFAULT_GST_PARAM, PLAYER_INI_MAX_PARAM_STRLEN - 1 );
		strncpy( ini->gst_param[2], DEFAULT_GST_PARAM, PLAYER_INI_MAX_PARAM_STRLEN - 1 );
		strncpy( ini->gst_param[3], DEFAULT_GST_PARAM, PLAYER_INI_MAX_PARAM_STRLEN - 1 );
		strncpy( ini->gst_param[4], DEFAULT_GST_PARAM, PLAYER_INI_MAX_PARAM_STRLEN - 1 );

		/* http streaming */
		strncpy( ini->name_of_httpsrc, DEFAULT_HTTPSRC, PLAYER_INI_MAX_STRLEN - 1 );
		strncpy( ini->http_file_buffer_path, DEFAULT_HTTP_FILE_BUFFER_PATH, PLAYER_INI_MAX_STRLEN - 1 );
		ini->http_buffering_limit = DEFAULT_HTTP_BUFFERING_LIMIT;
		ini->http_max_size_bytes = DEFAULT_HTTP_MAX_SIZE_BYTES;
		ini->http_buffering_time = DEFAULT_HTTP_BUFFERING_TIME;
		ini->http_timeout = DEFAULT_HTTP_TIMEOUT;

		/* rtsp streaming */
		strncpy( ini->name_of_rtspsrc, DEFAULT_RTSPSRC, PLAYER_INI_MAX_STRLEN - 1 );
		ini->rtsp_buffering_time = DEFAULT_RTSP_BUFFERING;
		ini->rtsp_rebuffering_time = DEFAULT_RTSP_REBUFFERING;
		ini->rtsp_do_typefinding = DEFAULT_RTSP_DO_TYPEFINDING;
		ini->rtsp_error_concealment = DEFAULT_RTSP_ERROR_CONCEALMENT;

		/* hw accelation */
		ini->use_video_hw_accel = DEFAULT_USE_VIDEO_HW_ACCEL;

		/* priority  */
		ini->use_priority_setting = DEFAULT_USE_PRIORITY_SETTING;
		ini->demux_priority = DEFAULT_PRIORITY_DEMUX;
		ini->videosink_priority = DEFAULT_PRIORITY_VIDEO_SINK;
		ini->audiosink_priority = DEFAULT_PRIORITY_AUDIO_SINK;
		ini->ringbuffer_priority = DEFAULT_PRIORITY_RINGBUFFER;

		/* dump buffer for debug */
		__get_element_list(ini, DEFAULT_DUMP_ELEMENT_KEYWORD, KEYWORD_DUMP);
		strncpy(ini->dump_element_path, DEFAULT_DUMP_ELEMENT_PATH, PLAYER_INI_MAX_STRLEN - 1);
	}

	/* free dict as we got our own structure */
	iniparser_freedict (dict);

	/* dump structure */
	debug_log("player settings -----------------------------------\n");

	/* general */
	debug_log("disable_segtrap : %d\n", ini->disable_segtrap);
	debug_log("skip rescan : %d\n", ini->skip_rescan);
	debug_log("videosink element x: %s\n", ini->videosink_element_x);
	debug_log("videosink element evas: %s\n", ini->videosink_element_evas);
	debug_log("videosink element fake: %s\n", ini->videosink_element_fake);
	debug_log("generate_dot : %d\n", ini->generate_dot);
	debug_log("provide_clock for music : %d\n", ini->provide_clock_for_music);
	debug_log("provide_clock for movie : %d\n", ini->provide_clock_for_movie);
	debug_log("live_state_change_timeout(sec) : %d\n", ini->live_state_change_timeout);
	debug_log("localplayback_state_change_timeout(sec) : %d\n", ini->localplayback_state_change_timeout);
	debug_log("eos_delay(msec) : %d\n", ini->eos_delay);
	debug_log("delay_before_repeat(msec) : %d\n", ini->delay_before_repeat);
	debug_log("name_of_drmsrc : %s\n", ini->name_of_drmsrc);
	debug_log("name_of_audioresampler : %s\n", ini->name_of_audio_resampler);
	debug_log("name_of_audiosink : %s\n", ini->name_of_audiosink);
	debug_log("name_of_video_converter : %s\n", ini->name_of_video_converter);
	debug_log("async_start : %d\n", ini->async_start);
	debug_log("multiple_codec_supported : %d\n", ini->multiple_codec_supported);

	debug_log("gst_param1 : %s\n", ini->gst_param[0]);
	debug_log("gst_param2 : %s\n", ini->gst_param[1]);
	debug_log("gst_param3 : %s\n", ini->gst_param[2]);
	debug_log("gst_param4 : %s\n", ini->gst_param[3]);
	debug_log("gst_param5 : %s\n", ini->gst_param[4]);

	for ( idx = 0; ini->exclude_element_keyword[idx][0] != '\0'; idx++ )
	{
		debug_log("exclude_element_keyword [%d] : %s\n", idx, ini->exclude_element_keyword[idx]);
	}

	for ( idx = 0; ini->dump_element_keyword[idx][0] != '\0'; idx++ )
	{
		debug_log("dump_element_keyword [%d] : %s\n", idx, ini->dump_element_keyword[idx]);
	}

	/* http streaming */
	debug_log("name_of_httpsrc : %s\n", ini->name_of_httpsrc);
	debug_log("http_file_buffer_path : %s \n", ini->http_file_buffer_path);
	debug_log("http_buffering_limit : %f \n", ini->http_buffering_limit);
	debug_log("http_max_size_bytes : %d \n", ini->http_max_size_bytes);
	debug_log("http_buffering_time : %f \n", ini->http_buffering_time);
	debug_log("http_timeout : %d \n", ini->http_timeout);

	/* rtsp streaming */
	debug_log("name_of_rtspsrc : %s\n", ini->name_of_rtspsrc);
	debug_log("rtsp_buffering_time(msec) : %d\n", ini->rtsp_buffering_time);
	debug_log("rtsp_rebuffering_time(msec) : %d\n", ini->rtsp_rebuffering_time);
	debug_log("rtsp_do_typefinding : %d \n", ini->rtsp_do_typefinding);
	debug_log("rtsp_error_concealment : %d \n", ini->rtsp_error_concealment);

	/* hw accel */
	debug_log("use_video_hw_accel : %d\n", ini->use_video_hw_accel);

	/* priority */
	debug_log("use_priority_setting : %d\n", ini->use_priority_setting);
	debug_log("demux_priority : %d\n", ini->demux_priority);
	debug_log("audiosink_priority : %d\n", ini->audiosink_priority);
	debug_log("videosink_priority : %d\n", ini->videosink_priority);
	debug_log("ringbuffer_priority : %d\n", ini->ringbuffer_priority);

	return MM_ERROR_NONE;
}


int
mm_player_audio_effect_ini_load(mm_player_ini_t* ini)
{
	dictionary * dict_audioeffect = NULL;

	dict_audioeffect = iniparser_load(MM_PLAYER_INI_DEFAULT_AUDIOEFFECT_PATH);
	if ( !dict_audioeffect )
	{
		debug_error("No audio effect ini file found. \n");
		return MM_ERROR_FILE_NOT_FOUND;
	}

	/* audio effect element name */
	MMPLAYER_INI_GET_STRING( dict_audioeffect, ini->name_of_audio_effect, "audio effect:audio effect element", DEFAULT_AUDIO_EFFECT_ELEMENT );
	if (!ini->name_of_audio_effect[0])
	{
		debug_warning("could not parse name of audio effect. \n");
		iniparser_freedict (dict_audioeffect);
		/* NOTE : in this case, we are not going to create audio filter element */
		return MM_ERROR_NONE;
	}

	/* audio effect (Preset)*/
	ini->use_audio_effect_preset = iniparser_getboolean(dict_audioeffect, "audio effect:audio effect preset", DEFAULT_USE_AUDIO_EFFECT_PRESET);
	if (ini->use_audio_effect_preset)
	{
		MMPLAYER_INI_GET_BOOLEAN_FROM_LIST( dict_audioeffect, ini->audio_effect_preset_list, MM_AUDIO_EFFECT_PRESET_NUM,
				"audio effect:audio effect preset list", DEFAULT_AUDIO_EFFECT_PRESET_LIST );
		MMPLAYER_INI_GET_BOOLEAN_FROM_LIST( dict_audioeffect, ini->audio_effect_preset_earphone_only_list, MM_AUDIO_EFFECT_PRESET_NUM,
				"audio effect:audio effect preset earphone only", DEFAULT_AUDIO_EFFECT_PRESET_LIST_EARPHONE_ONLY );
	}

	/* audio effect custom (EQ / Extension effects) */
	ini->use_audio_effect_custom = iniparser_getboolean(dict_audioeffect, "audio effect:audio effect custom", DEFAULT_USE_AUDIO_EFFECT_CUSTOM);
	if (ini->use_audio_effect_custom)
	{
		MMPLAYER_INI_GET_BOOLEAN_FROM_LIST( dict_audioeffect, ini->audio_effect_custom_list, MM_AUDIO_EFFECT_CUSTOM_NUM,
				"audio effect:audio effect custom list", DEFAULT_AUDIO_EFFECT_CUSTOM_LIST );
		MMPLAYER_INI_GET_BOOLEAN_FROM_LIST( dict_audioeffect, ini->audio_effect_custom_earphone_only_list, MM_AUDIO_EFFECT_CUSTOM_NUM,
				"audio effect:audio effect custom earphone only", DEFAULT_AUDIO_EFFECT_CUSTOM_LIST_EARPHONE_ONLY );

		/* audio effect custom : EQ */
		if (ini->audio_effect_custom_list[MM_AUDIO_EFFECT_CUSTOM_EQ])
		{
			ini->audio_effect_custom_eq_band_num = iniparser_getint(dict_audioeffect, "audio effect:audio effect custom eq band num",
					DEFAULT_AUDIO_EFFECT_CUSTOM_EQ_BAND_NUM);
			if (ini->audio_effect_custom_eq_band_num < DEFAULT_AUDIO_EFFECT_CUSTOM_EQ_BAND_NUM ||
					ini->audio_effect_custom_eq_band_num > MM_AUDIO_EFFECT_EQ_BAND_NUM_MAX)
			{
				debug_error("audio_effect_custom_eq_band_num(%d) is not valid range(%d - %d), set the value %d",
					ini->audio_effect_custom_eq_band_num, DEFAULT_AUDIO_EFFECT_CUSTOM_EQ_BAND_NUM, MM_AUDIO_EFFECT_EQ_BAND_NUM_MAX, DEFAULT_AUDIO_EFFECT_CUSTOM_EQ_BAND_NUM);
				ini->audio_effect_custom_eq_band_num = DEFAULT_AUDIO_EFFECT_CUSTOM_EQ_BAND_NUM;

				iniparser_freedict (dict_audioeffect);
				return MM_ERROR_PLAYER_INTERNAL;
			}
			else
			{
				if (ini->audio_effect_custom_eq_band_num)
				{
					MMPLAYER_INI_GET_INT_FROM_LIST( dict_audioeffect, ini->audio_effect_custom_eq_band_width, MM_AUDIO_EFFECT_EQ_BAND_NUM_MAX,
							"audio effect:audio effect custom eq band width", DEFAULT_AUDIO_EFFECT_CUSTOM_EQ_BAND_WIDTH );
					MMPLAYER_INI_GET_INT_FROM_LIST( dict_audioeffect, ini->audio_effect_custom_eq_band_freq, MM_AUDIO_EFFECT_EQ_BAND_NUM_MAX,
							"audio effect:audio effect custom eq band freq", DEFAULT_AUDIO_EFFECT_CUSTOM_EQ_BAND_FREQ );
				}
			}
		}

		/* audio effect custom : Extension effects */
		ini->audio_effect_custom_ext_num = iniparser_getint(dict_audioeffect, "audio effect:audio effect custom ext num",
				DEFAULT_AUDIO_EFFECT_CUSTOM_EXT_NUM);

		/* Min/Max value list of EQ / Extension effects */
		if (ini->audio_effect_custom_eq_band_num || ini->audio_effect_custom_ext_num)
		{

			MMPLAYER_INI_GET_INT_FROM_LIST( dict_audioeffect, ini->audio_effect_custom_min_level_list, MM_AUDIO_EFFECT_CUSTOM_NUM,
					"audio effect:audio effect custom min list", DEFAULT_AUDIO_EFFECT_CUSTOM_LIST );
			MMPLAYER_INI_GET_INT_FROM_LIST( dict_audioeffect, ini->audio_effect_custom_max_level_list, MM_AUDIO_EFFECT_CUSTOM_NUM,
					"audio effect:audio effect custom max list", DEFAULT_AUDIO_EFFECT_CUSTOM_LIST );
		}
	}

	ini->use_audio_effect_square= iniparser_getboolean(dict_audioeffect, "audio effect:audio effect square", DEFAULT_USE_AUDIO_EFFECT_SQUARE);
	if (ini->use_audio_effect_custom)
	{
		ini->audio_effect_square_max_row= iniparser_getint(dict_audioeffect, "audio effect:audio effect square max row",
				DEFAULT_AUDIO_EFFECT_SQUARE_ROW_MAX);
		ini->audio_effect_square_max_col= iniparser_getint(dict_audioeffect, "audio effect:audio effect square max col",
				DEFAULT_AUDIO_EFFECT_SQUARE_COL_MAX);
	}

	/* audio effect element name */
	MMPLAYER_INI_GET_STRING( dict_audioeffect, ini->name_of_audio_effect_sec, "audio effect:audio effect element sec", DEFAULT_AUDIO_EFFECT_ELEMENT );
	if (!ini->name_of_audio_effect_sec[0])
	{
		debug_warning("could not parse name of secondary audio effect. \n");
	}

	/* dump structure */
	debug_log("name_of_audio_effect : %s\n", ini->name_of_audio_effect);
	debug_log("use_audio_effect_preset : %d\n", ini->use_audio_effect_preset);
	debug_log("use_audio_effect_custom : %d\n", ini->use_audio_effect_custom);
	debug_log("name_of_audio_effect_sec : %s\n", ini->name_of_audio_effect_sec);
	debug_log("Use_audio_effect_square : %d\n", ini->use_audio_effect_square);
#if 0
	int i;
	for (i=0; i<MM_AUDIO_EFFECT_PRESET_NUM; i++)
	{
		debug_log("audio_effect_preset_list: %d (is it for earphone only?(%d))\n", ini->audio_effect_preset_list[i], ini->audio_effect_preset_earphone_only_list[i]);
	}
	for (i=0; i<MM_AUDIO_EFFECT_CUSTOM_NUM; i++)
	{
		debug_log("audio_effect_custom_list : %d (is it for earphone only?(%d))\n", ini->audio_effect_custom_list[i], ini->audio_effect_custom_earphone_only_list[i]);
	}
	debug_log("audio_effect_custom : eq_band_num(%d), ext_num(%d)\n", ini->audio_effect_custom_eq_band_num, ini->audio_effect_custom_ext_num );
	debug_log("audio_effect_custom_EQ : width(Hz) / central frequency(Hz)");
	for (i=0; i<ini->audio_effect_custom_eq_band_num; i++)
	{
		debug_log("     EQ band index(%d) :  %8d / %8d", i, ini->audio_effect_custom_eq_band_width[i], ini->audio_effect_custom_eq_band_freq[i]);
	}
	for (i=0; i<MM_AUDIO_EFFECT_CUSTOM_NUM; i++)
	{
		debug_log("audio_effect_custom_level_min_max(idx:%d) : Min(%d), Max(%d)\n", i, ini->audio_effect_custom_min_level_list[i], ini->audio_effect_custom_max_level_list[i]);
	}
#endif
	iniparser_freedict (dict_audioeffect);

	return MM_ERROR_NONE;

}


static
void __mm_player_ini_check_ini_status(void)
{
	struct stat ini_buff;

	if ( g_stat(MM_PLAYER_INI_DEFAULT_PATH, &ini_buff) < 0 )
	{
		debug_warning("failed to get player ini status\n");
	}
	else
	{
		if ( ini_buff.st_size < 5 )
		{
			debug_warning("player.ini file size=%d, Corrupted! So, Removed\n", (int)ini_buff.st_size);

			if ( g_remove( MM_PLAYER_INI_DEFAULT_PATH ) == -1)
			{
				debug_error("failed to delete corrupted ini");
			}
		}
	}
}

#ifdef MM_PLAYER_DEFAULT_INI
static
gboolean __generate_default_ini(void)
{
	FILE* fp = NULL;
	gchar* default_ini = MM_PLAYER_DEFAULT_INI;


	/* create new file */
	fp = fopen(MM_PLAYER_INI_DEFAULT_PATH, "wt");

	if ( !fp )
	{
		return FALSE;
	}

	/* writing default ini file */
	if ( strlen(default_ini) != fwrite(default_ini, 1, strlen(default_ini), fp) )
	{
		fclose(fp);
		return FALSE;
	}

	fclose(fp);
	return TRUE;
}
#endif

static
void	__get_element_list(mm_player_ini_t* ini, gchar* str, int keyword_type)
{
	gchar** list = NULL;
	gchar** walk = NULL;
	gint i = 0;
	gchar* strtmp = NULL;

	if ( ! str )
		return;

	if ( strlen( str ) < 1 )
		return;

	strtmp = g_strdup (str);

	/* trimming. it works inplace */
	g_strstrip( strtmp );


	/* split */
	list = g_strsplit( strtmp, ",", 10 );

	if ( !list )
	{
		if (strtmp)
			g_free(strtmp);

		return;
	}

	/* copy list */
	switch (keyword_type)
	{
		case KEYWORD_EXCLUDE:
		{
			for( walk = list; *walk; walk++ )
			{
				strncpy( ini->exclude_element_keyword[i], *walk, (PLAYER_INI_MAX_STRLEN - 1) );

				g_strstrip( ini->exclude_element_keyword[i] );

				ini->exclude_element_keyword[i][PLAYER_INI_MAX_STRLEN -1]= '\0';

				i++;
			}
			/* mark last item to NULL */
			ini->exclude_element_keyword[i][0] = '\0';

			break;
		}
		case KEYWORD_DUMP:
		{
			for( walk = list; *walk; walk++ )
			{
				strncpy( ini->dump_element_keyword[i], *walk, (PLAYER_INI_MAX_STRLEN - 1) );

				g_strstrip( ini->dump_element_keyword[i] );

				ini->dump_element_keyword[i][PLAYER_INI_MAX_STRLEN -1]= '\0';

				i++;
			}
			/* mark last item to NULL */
			ini->dump_element_keyword[i][0] = '\0';

			break;
		}
		default:
			break;
	}

	g_strfreev( list );
	if (strtmp)
		g_free (strtmp);
}

#endif



