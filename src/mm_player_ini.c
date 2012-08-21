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

/* global variables here */
static mm_player_ini_t g_player_ini;

/* internal functions, macros here */
static gboolean	__generate_default_ini(void);
static void	__get_string_list(gchar** out_list, gchar* str);

static void __mm_player_ini_force_setting(void);
static void __mm_player_ini_check_ini_status(void);

/* macro */
#define MMPLAYER_INI_GET_STRING( x_item, x_ini, x_default ) \
do \
{ \
	gchar* str = iniparser_getstring(dict, x_ini, x_default); \
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

/* x_ini is the list of index to set TRUE at x_list[index] */
#define MMPLAYER_INI_GET_BOOLEAN_FROM_LIST( x_list, x_list_max, x_ini, x_default ) \
do \
{ \
		int index = 0; \
		const char *delimiters = " ,"; \
		char *usr_ptr = NULL; \
		char *token = NULL; \
		gchar temp_arr[PLAYER_INI_MAX_STRLEN] = {0}; \
		MMPLAYER_INI_GET_STRING(temp_arr, x_ini, x_default); \
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
#define MMPLAYER_INI_GET_INT_FROM_LIST( x_list, x_list_max, x_ini, x_default ) \
do \
{ \
		int index = 0; \
		int value = 0; \
		const char *delimiters = " ,"; \
		char *usr_ptr = NULL; \
		char *token = NULL; \
		gchar temp_arr[PLAYER_INI_MAX_STRLEN] = {0}; \
		MMPLAYER_INI_GET_STRING(temp_arr, x_ini, x_default); \
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
mm_player_ini_load(void)
{
	static gboolean loaded = FALSE;
	dictionary * dict = NULL;
	gint idx = 0;

	if ( loaded )
		return MM_ERROR_NONE;

	dict = NULL;

	/* disabling ini parsing for launching */
#if 1 //debianize
	/* get player ini status because system will be crashed 
	 * if ini file is corrupted. 
	 */
	/* FIXIT : the api actually deleting illregular ini. but the function name said it's just checking. */
	__mm_player_ini_check_ini_status();

	/* first, try to load existing ini file */
	dict = iniparser_load(MM_PLAYER_INI_DEFAULT_PATH);

	/* if no file exists. create one with set of default values */
	if ( !dict )
	{
		#if 0
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
		debug_log("No inifile found. \n");

		return MM_ERROR_FILE_NOT_FOUND;
		#endif
	}
#endif

	/* get ini values */
	memset( &g_player_ini, 0, sizeof(mm_player_ini_t) );

	if ( dict ) /* if dict is available */
	{
		/* general */
		g_player_ini.use_decodebin = iniparser_getboolean(dict, "general:use decodebin", DEFAULT_USE_DECODEBIN);
		g_player_ini.use_sink_handler = iniparser_getboolean(dict, "general:use sink handler", DEFAULT_USE_SINK_HANDLER);
		g_player_ini.disable_segtrap = iniparser_getboolean(dict, "general:disable segtrap", DEFAULT_DISABLE_SEGTRAP);
		g_player_ini.skip_rescan = iniparser_getboolean(dict, "general:skip rescan", DEFAULT_SKIP_RESCAN);
		g_player_ini.video_surface = DEFAULT_VIDEO_SURFACE;
		g_player_ini.generate_dot = iniparser_getboolean(dict, "general:generate dot", DEFAULT_GENERATE_DOT);
		g_player_ini.provide_clock= iniparser_getboolean(dict, "general:provide clock", DEFAULT_PROVIDE_CLOCK);
		g_player_ini.live_state_change_timeout = iniparser_getint(dict, "general:live state change timeout", DEFAULT_LIVE_STATE_CHANGE_TIMEOUT);
		g_player_ini.localplayback_state_change_timeout = iniparser_getint(dict, "general:localplayback state change timeout", DEFAULT_LOCALPLAYBACK_STATE_CHANGE_TIMEOUT);
		g_player_ini.eos_delay = iniparser_getint(dict, "general:eos delay", DEFAULT_EOS_DELAY);
		g_player_ini.async_start = iniparser_getboolean(dict, "general:async start", DEFAULT_ASYNC_START);
		g_player_ini.multiple_codec_supported = iniparser_getboolean(dict, "general:multiple codec supported", DEFAULT_MULTIPLE_CODEC_SUPPORTED);

		g_player_ini.delay_before_repeat = iniparser_getint(dict, "general:delay before repeat", DEFAULT_DELAY_BEFORE_REPEAT);

		MMPLAYER_INI_GET_STRING( g_player_ini.videosink_element_x, "general:videosink element x", DEFAULT_VIDEOSINK_X);
		MMPLAYER_INI_GET_STRING( g_player_ini.videosink_element_evas, "general:videosink element evas", DEFAULT_VIDEOSINK_EVAS);
		MMPLAYER_INI_GET_STRING( g_player_ini.videosink_element_fake, "general:videosink element fake", DEFAULT_VIDEOSINK_FAKE);
		MMPLAYER_INI_GET_STRING( g_player_ini.name_of_drmsrc, "general:drmsrc element", DEFAULT_DRMSRC );
		MMPLAYER_INI_GET_STRING( g_player_ini.name_of_audiosink, "general:audiosink element", DEFAULT_AUDIOSINK );
		MMPLAYER_INI_GET_STRING( g_player_ini.name_of_video_converter, "general:video converter element", DEFAULT_VIDEO_CONVERTER );

		__get_string_list( (gchar**) g_player_ini.exclude_element_keyword, 
			iniparser_getstring(dict, "general:element exclude keyword", DEFAULT_EXCLUDE_KEYWORD));

		MMPLAYER_INI_GET_STRING( g_player_ini.gst_param[0], "general:gstparam1", DEFAULT_GST_PARAM );
		MMPLAYER_INI_GET_STRING( g_player_ini.gst_param[1], "general:gstparam2", DEFAULT_GST_PARAM );
		MMPLAYER_INI_GET_STRING( g_player_ini.gst_param[2], "general:gstparam3", DEFAULT_GST_PARAM );
		MMPLAYER_INI_GET_STRING( g_player_ini.gst_param[3], "general:gstparam4", DEFAULT_GST_PARAM );
		MMPLAYER_INI_GET_STRING( g_player_ini.gst_param[4], "general:gstparam5", DEFAULT_GST_PARAM );

		/* audio filter (Preset)*/
		g_player_ini.use_audio_filter_preset = iniparser_getboolean(dict, "sound effect:audio filter preset", DEFAULT_USE_AUDIO_FILTER_PRESET);
		if (g_player_ini.use_audio_filter_preset)
		{
			MMPLAYER_INI_GET_BOOLEAN_FROM_LIST( g_player_ini.audio_filter_preset_list, MM_AUDIO_FILTER_PRESET_NUM,
					"sound effect:audio filter preset list", DEFAULT_AUDIO_FILTER_PRESET_LIST );
			MMPLAYER_INI_GET_BOOLEAN_FROM_LIST( g_player_ini.audio_filter_preset_earphone_only_list, MM_AUDIO_FILTER_PRESET_NUM,
					"sound effect:audio filter preset earphone only", DEFAULT_AUDIO_FILTER_PRESET_LIST_EARPHONE_ONLY );
		}
		/* for audio filter custom (EQ / Extension filters) */
		g_player_ini.use_audio_filter_custom = iniparser_getboolean(dict, "sound effect:audio filter custom", DEFAULT_USE_AUDIO_FILTER_CUSTOM);
		if (g_player_ini.use_audio_filter_custom)
		{
			MMPLAYER_INI_GET_BOOLEAN_FROM_LIST( g_player_ini.audio_filter_custom_list, MM_AUDIO_FILTER_CUSTOM_NUM,
					"sound effect:audio filter custom list", DEFAULT_AUDIO_FILTER_CUSTOM_LIST );
			MMPLAYER_INI_GET_BOOLEAN_FROM_LIST( g_player_ini.audio_filter_custom_earphone_only_list, MM_AUDIO_FILTER_CUSTOM_NUM,
					"sound effect:audio filter custom earphone only", DEFAULT_AUDIO_FILTER_CUSTOM_LIST_EARPHONE_ONLY );
			/* for audio filter custom : EQ */
			if (g_player_ini.audio_filter_custom_list[MM_AUDIO_FILTER_CUSTOM_EQ])
			{
				g_player_ini.audio_filter_custom_eq_num = iniparser_getint(dict, "sound effect:audio filter eq num",
						DEFAULT_AUDIO_FILTER_CUSTOM_EQ_NUM);
				if (g_player_ini.audio_filter_custom_eq_num < DEFAULT_AUDIO_FILTER_CUSTOM_EQ_NUM || g_player_ini.audio_filter_custom_eq_num > MM_AUDIO_FILTER_EQ_BAND_MAX)
				{
					debug_error("audio_filter_custom_eq_num(%d) is not valid range(%d - %d), set the value %d",
						g_player_ini.audio_filter_custom_eq_num, DEFAULT_AUDIO_FILTER_CUSTOM_EQ_NUM, MM_AUDIO_FILTER_EQ_BAND_MAX, DEFAULT_AUDIO_FILTER_CUSTOM_EQ_NUM);
					g_player_ini.audio_filter_custom_eq_num = DEFAULT_AUDIO_FILTER_CUSTOM_EQ_NUM;
				}
			}
			/* for audio filter custom : extension filters */
			g_player_ini.audio_filter_custom_ext_num = iniparser_getint(dict, "sound effect:audio filter ext num",
					DEFAULT_AUDIO_FILTER_CUSTOM_EXT_NUM);
			if (g_player_ini.audio_filter_custom_ext_num > 0)
			{
				MMPLAYER_INI_GET_INT_FROM_LIST( g_player_ini.audio_filter_custom_min_level_list, MM_AUDIO_FILTER_CUSTOM_NUM,
						"sound effect:audio filter custom min list", DEFAULT_AUDIO_FILTER_CUSTOM_LIST );
				MMPLAYER_INI_GET_INT_FROM_LIST( g_player_ini.audio_filter_custom_max_level_list, MM_AUDIO_FILTER_CUSTOM_NUM,
						"sound effect:audio filter custom max list", DEFAULT_AUDIO_FILTER_CUSTOM_LIST );
			}
		}
#if 0
		int i;
		for (i=0; i<MM_AUDIO_FILTER_PRESET_NUM; i++)
		{
			debug_log("audio_filter_preset_list: %d (is it for earphone only?(%d))\n", g_player_ini.audio_filter_preset_list[i], g_player_ini.audio_filter_preset_earphone_only_list[i]);
		}
		for (i=0; i<MM_AUDIO_FILTER_CUSTOM_NUM; i++)
		{
			debug_log("audio_filter_custom_list : %d (is it for earphone only?(%d))\n", g_player_ini.audio_filter_custom_list[i], g_player_ini.audio_filter_custom_earphone_only_list[i]);
		}
		debug_log("audio_filter_custom : eq_num(%d), ext_num(%d)\n", g_player_ini.audio_filter_custom_eq_num, g_player_ini.audio_filter_custom_ext_num )
		for (i=0; i<MM_AUDIO_FILTER_CUSTOM_NUM; i++)
		{
			debug_log("aaudio_filter_custom_level_min_max_list : min(%d), max(%d)\n", g_player_ini.audio_filter_custom_min_level_list[i], g_player_ini.audio_filter_custom_max_level_list[i]);
		}
#endif

		/* http streaming */
		MMPLAYER_INI_GET_STRING( g_player_ini.name_of_httpsrc, "http streaming:httpsrc element", DEFAULT_HTTPSRC );
		MMPLAYER_INI_GET_STRING( g_player_ini.http_file_buffer_path, "http streaming:http file buffer path", DEFAULT_HTTP_FILE_BUFFER_PATH );
		g_player_ini.http_buffering_limit = iniparser_getdouble(dict, "http streaming:http buffering high limit", DEFAULT_HTTP_BUFFERING_LIMIT);
		g_player_ini.http_max_size_bytes = iniparser_getint(dict, "http streaming:http max size bytes", DEFAULT_HTTP_MAX_SIZE_BYTES);
		g_player_ini.http_buffering_time = iniparser_getdouble(dict, "http streaming:http buffering time", DEFAULT_HTTP_BUFFERING_TIME);		
		g_player_ini.http_timeout = iniparser_getint(dict, "http streaming:http timeout", DEFAULT_HTTP_TIMEOUT);

		/* rtsp streaming */
		MMPLAYER_INI_GET_STRING( g_player_ini.name_of_rtspsrc, "rtsp streaming:rtspsrc element", DEFAULT_RTSPSRC );
		g_player_ini.rtsp_buffering_time = iniparser_getint(dict, "rtsp streaming:rtsp buffering time", DEFAULT_RTSP_BUFFERING);
		g_player_ini.rtsp_rebuffering_time = iniparser_getint(dict, "rtsp streaming:rtsp rebuffering time", DEFAULT_RTSP_REBUFFERING);
		g_player_ini.rtsp_do_typefinding = iniparser_getboolean(dict, "rtsp streaming:rtsp do typefinding", DEFAULT_RTSP_DO_TYPEFINDING);
		g_player_ini.rtsp_error_concealment = iniparser_getboolean(dict, "rtsp streaming:rtsp error concealment", DEFAULT_RTSP_ERROR_CONCEALMENT);

		/* hw accelation */
		g_player_ini.use_video_hw_accel = iniparser_getboolean(dict, "hw accelation:use video hw accel", DEFAULT_USE_VIDEO_HW_ACCEL);
		
		/* priority */
		g_player_ini.use_priority_setting = iniparser_getboolean(dict, "priority:use priority setting", DEFAULT_USE_PRIORITY_SETTING);
		g_player_ini.demux_priority = iniparser_getint(dict, "priority:demux", DEFAULT_PRIORITY_DEMUX);
		g_player_ini.videosink_priority = iniparser_getint(dict, "priority:videosink", DEFAULT_PRIORITY_VIDEO_SINK);
		g_player_ini.audiosink_priority = iniparser_getint(dict, "priority:audiosink", DEFAULT_PRIORITY_AUDIO_SINK);
		g_player_ini.ringbuffer_priority = iniparser_getint(dict, "priority:ringbuffer", DEFAULT_PRIORITY_RINGBUFFER);
	}	
	else /* if dict is not available just fill the structure with default value */
	{
		debug_warning("failed to load ini. using hardcoded default\n");

		/* general */
		g_player_ini.use_decodebin = DEFAULT_USE_DECODEBIN;
		g_player_ini.use_sink_handler = DEFAULT_USE_SINK_HANDLER;
		g_player_ini.disable_segtrap = DEFAULT_DISABLE_SEGTRAP;
		g_player_ini.use_audio_filter_preset = DEFAULT_USE_AUDIO_FILTER_PRESET;
		g_player_ini.use_audio_filter_custom = DEFAULT_USE_AUDIO_FILTER_CUSTOM;
		g_player_ini.skip_rescan = DEFAULT_SKIP_RESCAN;
		g_player_ini.video_surface = DEFAULT_VIDEO_SURFACE;
		strncpy( g_player_ini.videosink_element_x, DEFAULT_VIDEOSINK_X, PLAYER_INI_MAX_STRLEN - 1 );
		strncpy( g_player_ini.videosink_element_evas, DEFAULT_VIDEOSINK_EVAS, PLAYER_INI_MAX_STRLEN - 1 );
		strncpy( g_player_ini.videosink_element_fake, DEFAULT_VIDEOSINK_FAKE, PLAYER_INI_MAX_STRLEN - 1 );
		g_player_ini.generate_dot = DEFAULT_GENERATE_DOT;
		g_player_ini.provide_clock= DEFAULT_PROVIDE_CLOCK;
		g_player_ini.live_state_change_timeout = DEFAULT_LIVE_STATE_CHANGE_TIMEOUT;
		g_player_ini.localplayback_state_change_timeout = DEFAULT_LOCALPLAYBACK_STATE_CHANGE_TIMEOUT;
		g_player_ini.eos_delay = DEFAULT_EOS_DELAY;
		g_player_ini.multiple_codec_supported = DEFAULT_MULTIPLE_CODEC_SUPPORTED;
		g_player_ini.async_start = DEFAULT_ASYNC_START;
		g_player_ini.delay_before_repeat = DEFAULT_DELAY_BEFORE_REPEAT;


		strncpy( g_player_ini.name_of_drmsrc, DEFAULT_DRMSRC, PLAYER_INI_MAX_STRLEN - 1 );
		strncpy( g_player_ini.name_of_audiosink, DEFAULT_AUDIOSINK, PLAYER_INI_MAX_STRLEN -1 );
		strncpy( g_player_ini.name_of_video_converter, DEFAULT_VIDEO_CONVERTER, PLAYER_INI_MAX_STRLEN -1 );

		{
			__get_string_list( (gchar**) g_player_ini.exclude_element_keyword, DEFAULT_EXCLUDE_KEYWORD);
		}


		strncpy( g_player_ini.gst_param[0], DEFAULT_GST_PARAM, PLAYER_INI_MAX_PARAM_STRLEN - 1 );
		strncpy( g_player_ini.gst_param[1], DEFAULT_GST_PARAM, PLAYER_INI_MAX_PARAM_STRLEN - 1 );
		strncpy( g_player_ini.gst_param[2], DEFAULT_GST_PARAM, PLAYER_INI_MAX_PARAM_STRLEN - 1 );
		strncpy( g_player_ini.gst_param[3], DEFAULT_GST_PARAM, PLAYER_INI_MAX_PARAM_STRLEN - 1 );
		strncpy( g_player_ini.gst_param[4], DEFAULT_GST_PARAM, PLAYER_INI_MAX_PARAM_STRLEN - 1 );

		/* http streaming */
		strncpy( g_player_ini.name_of_httpsrc, DEFAULT_HTTPSRC, PLAYER_INI_MAX_STRLEN - 1 );
		strncpy( g_player_ini.http_file_buffer_path, DEFAULT_HTTP_FILE_BUFFER_PATH, PLAYER_INI_MAX_STRLEN - 1 );
		g_player_ini.http_buffering_limit = DEFAULT_HTTP_BUFFERING_LIMIT;
		g_player_ini.http_max_size_bytes = DEFAULT_HTTP_MAX_SIZE_BYTES;
		g_player_ini.http_buffering_time = DEFAULT_HTTP_BUFFERING_TIME;		
		g_player_ini.http_timeout = DEFAULT_HTTP_TIMEOUT;
		
		/* rtsp streaming */
		strncpy( g_player_ini.name_of_rtspsrc, DEFAULT_RTSPSRC, PLAYER_INI_MAX_STRLEN - 1 );
		g_player_ini.rtsp_buffering_time = DEFAULT_RTSP_BUFFERING;
		g_player_ini.rtsp_rebuffering_time = DEFAULT_RTSP_REBUFFERING;
		g_player_ini.rtsp_do_typefinding = DEFAULT_RTSP_DO_TYPEFINDING;
		g_player_ini.rtsp_error_concealment = DEFAULT_RTSP_ERROR_CONCEALMENT;

		/* hw accelation */
		g_player_ini.use_video_hw_accel = DEFAULT_USE_VIDEO_HW_ACCEL;

		/* priority  */
		g_player_ini.use_priority_setting = DEFAULT_USE_PRIORITY_SETTING;
		g_player_ini.demux_priority = DEFAULT_PRIORITY_DEMUX;
		g_player_ini.videosink_priority = DEFAULT_PRIORITY_VIDEO_SINK;
		g_player_ini.audiosink_priority = DEFAULT_PRIORITY_AUDIO_SINK;
		g_player_ini.ringbuffer_priority = DEFAULT_PRIORITY_RINGBUFFER;
	}

	/* free dict as we got our own structure */
	iniparser_freedict (dict);

	loaded = TRUE;

	/* The simulator uses a separate ini file. */
	//__mm_player_ini_force_setting();


	/* dump structure */
	debug_log("player settings -----------------------------------\n");

	/* general */
	debug_log("use_decodebin : %d\n", g_player_ini.use_decodebin);
	debug_log("use_audio_filter_preset : %d\n", g_player_ini.use_audio_filter_preset);
	debug_log("use_audio_filter_custom : %d\n", g_player_ini.use_audio_filter_custom);
	debug_log("use_sink_handler : %d\n", g_player_ini.use_sink_handler);
	debug_log("disable_segtrap : %d\n", g_player_ini.disable_segtrap);
	debug_log("skip rescan : %d\n", g_player_ini.skip_rescan);
	debug_log("video surface(0:X, 1:EVAS, 2:GL, 3:NULL) : %d\n", g_player_ini.video_surface);
	debug_log("videosink element x: %s\n", g_player_ini.videosink_element_x);
	debug_log("videosink element evas: %s\n", g_player_ini.videosink_element_evas);
	debug_log("videosink element fake: %s\n", g_player_ini.videosink_element_fake);
	debug_log("generate_dot : %d\n", g_player_ini.generate_dot);
	debug_log("provide_clock : %d\n", g_player_ini.provide_clock);
	debug_log("live_state_change_timeout(sec) : %d\n", g_player_ini.live_state_change_timeout);
	debug_log("localplayback_state_change_timeout(sec) : %d\n", g_player_ini.localplayback_state_change_timeout);	
	debug_log("eos_delay(msec) : %d\n", g_player_ini.eos_delay);
	debug_log("delay_before_repeat(msec) : %d\n", g_player_ini.delay_before_repeat);
	debug_log("name_of_drmsrc : %s\n", g_player_ini.name_of_drmsrc);
	debug_log("name_of_audiosink : %s\n", g_player_ini.name_of_audiosink);
	debug_log("name_of_video_converter : %s\n", g_player_ini.name_of_video_converter);
	debug_log("async_start : %d\n", g_player_ini.async_start);
	debug_log("multiple_codec_supported : %d\n", g_player_ini.multiple_codec_supported);	

	debug_log("gst_param1 : %s\n", g_player_ini.gst_param[0]);
	debug_log("gst_param2 : %s\n", g_player_ini.gst_param[1]);
	debug_log("gst_param3 : %s\n", g_player_ini.gst_param[2]);
	debug_log("gst_param4 : %s\n", g_player_ini.gst_param[3]);
	debug_log("gst_param5 : %s\n", g_player_ini.gst_param[4]);

	for ( idx = 0; g_player_ini.exclude_element_keyword[idx][0] != '\0'; idx++ )
	{
		debug_log("exclude_element_keyword [%d] : %s\n", idx, g_player_ini.exclude_element_keyword[idx]);
	}
	
	/* http streaming */
	debug_log("name_of_httpsrc : %s\n", g_player_ini.name_of_httpsrc);
	debug_log("http_file_buffer_path : %s \n", g_player_ini.http_file_buffer_path);
	debug_log("http_buffering_limit : %f \n", g_player_ini.http_buffering_limit);
	debug_log("http_max_size_bytes : %d \n", g_player_ini.http_max_size_bytes);
	debug_log("http_buffering_time : %f \n", g_player_ini.http_buffering_time);
	debug_log("http_timeout : %d \n", g_player_ini.http_timeout);
	
	/* rtsp streaming */
	debug_log("name_of_rtspsrc : %s\n", g_player_ini.name_of_rtspsrc);
	debug_log("rtsp_buffering_time(msec) : %d\n", g_player_ini.rtsp_buffering_time);
	debug_log("rtsp_rebuffering_time(msec) : %d\n", g_player_ini.rtsp_rebuffering_time);
	debug_log("rtsp_do_typefinding : %d \n", g_player_ini.rtsp_do_typefinding);
	debug_log("rtsp_error_concealment : %d \n", g_player_ini.rtsp_error_concealment);

	/* hw accel */
	debug_log("use_video_hw_accel : %d\n", g_player_ini.use_video_hw_accel);

	/* priority */
	debug_log("use_priority_setting : %d\n", g_player_ini.use_priority_setting);
	debug_log("demux_priority : %d\n", g_player_ini.demux_priority);
	debug_log("audiosink_priority : %d\n", g_player_ini.audiosink_priority);
	debug_log("videosink_priority : %d\n", g_player_ini.videosink_priority);
	debug_log("ringbuffer_priority : %d\n", g_player_ini.ringbuffer_priority);

	debug_log("---------------------------------------------------\n");	

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
			
			g_remove( MM_PLAYER_INI_DEFAULT_PATH );
		}
	}
}

static 
void __mm_player_ini_force_setting(void)
{
	/* FIXIT : remove it when all other elements are available on simulator, SDK */
	
	#if ! defined(__arm__)
		debug_warning("player is running on simulator. force to use ximagesink\n");
		//g_player_ini.videosink_element = PLAYER_INI_VSINK_XIMAGESINK;
		g_player_ini.use_audio_filter_preset = FALSE;
		g_player_ini.use_audio_filter_custom = FALSE;

		strcpy( g_player_ini.name_of_drmsrc, "filesrc" );

		// Force setting for simulator :+:091218 
		strcpy( g_player_ini.name_of_audiosink, "alsasink" );

		
//		__get_string_list( (gchar**) g_player_ini.exclude_element_keyword, "");
		
	#endif

	#if defined(VDF_SDK) || defined (SEC_SDK)
		debug_warning("player is running on SDK.\n");
		debug_warning("So, it seems like that some plugin values are not same with those\n");
		debug_warning("which are written in default ini file.\n");

		//g_player_ini.videosink_element = PLAYER_INI_VSINK_XIMAGESINK;
		g_player_ini.use_audio_filter_preset = FALSE;
		g_player_ini.use_audio_filter_custom = FALSE;

		strcpy( g_player_ini.name_of_drmsrc, "filesrc" );
	#endif

	#if defined(NEW_SOUND) 
		strcpy (g_player_ini.name_of_audiosink, "soundsink"); // :+:090707
	#endif

	/* FIXIT : The HW quality of volans is not better than protector.
	 * So, it can't use same timeout value because state change(resume) is sometimes failed in volans.
	 * Thus, it should be set more than 10sec. 
	 */
	#if defined(_MM_PROJECT_VOLANS)
		g_player_ini.localplayback_state_change_timeout = 10;
		debug_log("localplayback_state_change_timeout is set as 30sec by force\n");
	#endif

	#if 0
	#if defined(_MM_PROJECT_VOLANS)
		debug_warning("player is running on VOLANS\n");
		g_player_ini.use_audio_filter = FALSE;		// (+)090702, disabled temporally
	#endif
	#endif
	
}

mm_player_ini_t* 
mm_player_ini_get_structure(void)
{
	return &g_player_ini;
}

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

static 
void	__get_string_list(gchar** out_list, gchar* str)
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
	for( walk = list; *walk; walk++ )
	{
		strncpy( g_player_ini.exclude_element_keyword[i], *walk, (PLAYER_INI_MAX_STRLEN - 1) );

		g_strstrip( g_player_ini.exclude_element_keyword[i] );

		g_player_ini.exclude_element_keyword[i][PLAYER_INI_MAX_STRLEN - 1] = '\0';

		i++;
	}

	/* mark last item to NULL */
	g_player_ini.exclude_element_keyword[i][0] = '\0';

	g_strfreev( list );
	if (strtmp)
		g_free (strtmp);
}

#endif



