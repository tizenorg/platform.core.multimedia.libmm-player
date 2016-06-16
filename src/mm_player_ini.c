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
#include <dlog.h>
#include <glib.h>
#include <stdlib.h>
#include "iniparser.h"
#include <mm_player_ini.h>
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
		strncpy ( x_item, str, PLAYER_INI_MAX_STRLEN-1 ); \
	} \
	else \
	{ \
		strncpy ( x_item, x_default, PLAYER_INI_MAX_STRLEN-1 ); \
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
				LOGW("%d is not valid index\n", index); \
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
				LOGE("%d is not valid index\n", index); \
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
		LOGD("No inifile found. player will create default inifile.\n");
		if ( FALSE == __generate_default_ini() )
		{
			LOGW("Creating default inifile failed. Player will use default values.\n");
		}
		else
		{
			/* load default ini */
			dict = iniparser_load(MM_PLAYER_INI_DEFAULT_PATH);
		}
#else
		LOGD("No ini file found. \n");
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
		ini->use_system_clock = iniparser_getboolean(dict, "general:use system clock", DEFAULT_USE_SYSTEM_CLOCK);
		ini->live_state_change_timeout = iniparser_getint(dict, "general:live state change timeout", DEFAULT_LIVE_STATE_CHANGE_TIMEOUT);
		ini->localplayback_state_change_timeout = iniparser_getint(dict, "general:localplayback state change timeout", DEFAULT_LOCALPLAYBACK_STATE_CHANGE_TIMEOUT);
		ini->eos_delay = iniparser_getint(dict, "general:eos delay", DEFAULT_EOS_DELAY);
		ini->async_start = iniparser_getboolean(dict, "general:async start", DEFAULT_ASYNC_START);
		ini->video_playback_supported = iniparser_getboolean(dict, "general:video playback supported", DEFAULT_VIDEO_PLAYBACK_SUPPORTED);
		ini->delay_before_repeat = iniparser_getint(dict, "general:delay before repeat", DEFAULT_DELAY_BEFORE_REPEAT);
		ini->pcm_buffer_size = iniparser_getint(dict, "general:pcm buffer size", DEFAULT_PCM_BUFFER_SIZE);
		ini->num_of_video_bo = iniparser_getint(dict, "general:video bo max", DEFAULT_NUM_OF_VIDEO_BO);
		ini->video_bo_timeout = iniparser_getint(dict, "general:video bo timeout", DEFAULT_TIMEOUT_OF_VIDEO_BO);

		MMPLAYER_INI_GET_STRING(dict, ini->videosink_element_overlay, "general:videosink element overlay", DEFAULT_VIDEOSINK_OVERLAY);
		MMPLAYER_INI_GET_STRING(dict, ini->videosink_element_evas, "general:videosink element evas", DEFAULT_VIDEOSINK_EVAS);
		MMPLAYER_INI_GET_STRING(dict, ini->videosink_element_fake, "general:videosink element fake", DEFAULT_VIDEOSINK_FAKE);
		MMPLAYER_INI_GET_STRING(dict, ini->videosink_element_remote, "general:videosink element remote", DEFAULT_VIDEOSINK_REMOTE);
		MMPLAYER_INI_GET_STRING(dict, ini->videosrc_element_remote, "general:videosrc element remote", DEFAULT_VIDEOSRC_REMOTE);
		MMPLAYER_INI_GET_STRING(dict, ini->audioresampler_element, "general:audio resampler element", DEFAULT_AUDIORESAMPLER );
		MMPLAYER_INI_GET_STRING(dict, ini->audiosink_element, "general:audiosink element", DEFAULT_AUDIOSINK );
		MMPLAYER_INI_GET_STRING(dict, ini->videoconverter_element, "general:video converter element", DEFAULT_VIDEO_CONVERTER );

		__get_element_list(ini,
			iniparser_getstring(dict, "general:element exclude keyword", DEFAULT_EXCLUDE_KEYWORD), KEYWORD_EXCLUDE);

		MMPLAYER_INI_GET_STRING(dict, ini->gst_param[0], "general:gstparam1", DEFAULT_GST_PARAM );
		MMPLAYER_INI_GET_STRING(dict, ini->gst_param[1], "general:gstparam2", DEFAULT_GST_PARAM );
		MMPLAYER_INI_GET_STRING(dict, ini->gst_param[2], "general:gstparam3", DEFAULT_GST_PARAM );
		MMPLAYER_INI_GET_STRING(dict, ini->gst_param[3], "general:gstparam4", DEFAULT_GST_PARAM );
		MMPLAYER_INI_GET_STRING(dict, ini->gst_param[4], "general:gstparam5", DEFAULT_GST_PARAM );

		/* http streaming */
		MMPLAYER_INI_GET_STRING( dict, ini->httpsrc_element, "http streaming:httpsrc element", DEFAULT_HTTPSRC );
		MMPLAYER_INI_GET_STRING( dict, ini->http_file_buffer_path, "http streaming:http file buffer path", DEFAULT_HTTP_FILE_BUFFER_PATH );
		ini->http_buffering_limit = iniparser_getdouble(dict, "http streaming:http buffering high limit", DEFAULT_HTTP_BUFFERING_LIMIT);
		ini->http_max_size_bytes = iniparser_getint(dict, "http streaming:http max size bytes", DEFAULT_HTTP_MAX_SIZE_BYTES);
		ini->http_buffering_time = iniparser_getdouble(dict, "http streaming:http buffering time", DEFAULT_HTTP_BUFFERING_TIME);
		ini->http_timeout = iniparser_getint(dict, "http streaming:http timeout", DEFAULT_HTTP_TIMEOUT);

		/* dump buffer for debug */
		__get_element_list(ini,
			iniparser_getstring(dict, "general:dump element keyword", DEFAULT_EXCLUDE_KEYWORD), KEYWORD_DUMP);

		MMPLAYER_INI_GET_STRING(dict, ini->dump_element_path, "general:dump element path", DEFAULT_DUMP_ELEMENT_PATH);
	}
	else /* if dict is not available just fill the structure with default value */
	{
		LOGW("failed to load ini. using hardcoded default\n");

		/* general */
		ini->disable_segtrap = DEFAULT_DISABLE_SEGTRAP;
		ini->skip_rescan = DEFAULT_SKIP_RESCAN;
		strncpy( ini->videosink_element_overlay, DEFAULT_VIDEOSINK_OVERLAY, PLAYER_INI_MAX_STRLEN - 1 );
		strncpy( ini->videosink_element_evas, DEFAULT_VIDEOSINK_EVAS, PLAYER_INI_MAX_STRLEN - 1 );
		strncpy( ini->videosink_element_fake, DEFAULT_VIDEOSINK_FAKE, PLAYER_INI_MAX_STRLEN - 1 );
		strncpy( ini->videosink_element_remote, DEFAULT_VIDEOSINK_REMOTE, PLAYER_INI_MAX_STRLEN - 1 );
		strncpy( ini->videosrc_element_remote, DEFAULT_VIDEOSRC_REMOTE, PLAYER_INI_MAX_STRLEN - 1 );
		ini->generate_dot = DEFAULT_GENERATE_DOT;
		ini->use_system_clock = DEFAULT_USE_SYSTEM_CLOCK;
		ini->live_state_change_timeout = DEFAULT_LIVE_STATE_CHANGE_TIMEOUT;
		ini->localplayback_state_change_timeout = DEFAULT_LOCALPLAYBACK_STATE_CHANGE_TIMEOUT;
		ini->eos_delay = DEFAULT_EOS_DELAY;
		ini->async_start = DEFAULT_ASYNC_START;
		ini->delay_before_repeat = DEFAULT_DELAY_BEFORE_REPEAT;
		ini->video_playback_supported = DEFAULT_VIDEO_PLAYBACK_SUPPORTED;
		ini->pcm_buffer_size = DEFAULT_PCM_BUFFER_SIZE;
		ini->num_of_video_bo = DEFAULT_NUM_OF_VIDEO_BO;
		ini->video_bo_timeout = DEFAULT_TIMEOUT_OF_VIDEO_BO;

		strncpy( ini->audioresampler_element, DEFAULT_AUDIORESAMPLER, PLAYER_INI_MAX_STRLEN -1 );
		strncpy( ini->audiosink_element, DEFAULT_AUDIOSINK, PLAYER_INI_MAX_STRLEN -1 );
		strncpy( ini->videoconverter_element, DEFAULT_VIDEO_CONVERTER, PLAYER_INI_MAX_STRLEN -1 );

		__get_element_list(ini, DEFAULT_EXCLUDE_KEYWORD, KEYWORD_EXCLUDE);

		strncpy( ini->gst_param[0], DEFAULT_GST_PARAM, PLAYER_INI_MAX_PARAM_STRLEN - 1 );
		strncpy( ini->gst_param[1], DEFAULT_GST_PARAM, PLAYER_INI_MAX_PARAM_STRLEN - 1 );
		strncpy( ini->gst_param[2], DEFAULT_GST_PARAM, PLAYER_INI_MAX_PARAM_STRLEN - 1 );
		strncpy( ini->gst_param[3], DEFAULT_GST_PARAM, PLAYER_INI_MAX_PARAM_STRLEN - 1 );
		strncpy( ini->gst_param[4], DEFAULT_GST_PARAM, PLAYER_INI_MAX_PARAM_STRLEN - 1 );

		/* http streaming */
		strncpy( ini->httpsrc_element, DEFAULT_HTTPSRC, PLAYER_INI_MAX_STRLEN - 1 );
		strncpy( ini->http_file_buffer_path, DEFAULT_HTTP_FILE_BUFFER_PATH, PLAYER_INI_MAX_STRLEN - 1 );
		ini->http_buffering_limit = DEFAULT_HTTP_BUFFERING_LIMIT;
		ini->http_max_size_bytes = DEFAULT_HTTP_MAX_SIZE_BYTES;
		ini->http_buffering_time = DEFAULT_HTTP_BUFFERING_TIME;
		ini->http_timeout = DEFAULT_HTTP_TIMEOUT;

		/* dump buffer for debug */
		__get_element_list(ini, DEFAULT_DUMP_ELEMENT_KEYWORD, KEYWORD_DUMP);
		strncpy(ini->dump_element_path, DEFAULT_DUMP_ELEMENT_PATH, PLAYER_INI_MAX_STRLEN - 1);
	}

	/* free dict as we got our own structure */
	iniparser_freedict (dict);

	/* dump structure */
	LOGD("player settings -----------------------------------\n");

	/* general */
	LOGD("disable segtrap : %d\n", ini->disable_segtrap);
	LOGD("skip rescan : %d\n", ini->skip_rescan);
	LOGD("videosink element overlay: %s\n", ini->videosink_element_overlay);
	LOGD("videosink element evas: %s\n", ini->videosink_element_evas);
	LOGD("videosink element fake: %s\n", ini->videosink_element_fake);
	LOGD("videosink element remote: %s\n", ini->videosink_element_remote);
	LOGD("videosrc element remote: %s\n", ini->videosrc_element_remote);
	LOGD("video converter element : %s\n", ini->videoconverter_element);
	LOGD("audio resampler element : %s\n", ini->audioresampler_element);
	LOGD("audiosink element : %s\n", ini->audiosink_element);
	LOGD("generate dot : %d\n", ini->generate_dot);
	LOGD("use system clock(video only) : %d\n", ini->use_system_clock);
	LOGD("live state change timeout(sec) : %d\n", ini->live_state_change_timeout);
	LOGD("localplayback state change timeout(sec) : %d\n", ini->localplayback_state_change_timeout);
	LOGD("eos_delay(msec) : %d\n", ini->eos_delay);
	LOGD("delay before repeat(msec) : %d\n", ini->delay_before_repeat);
	LOGD("async_start : %d\n", ini->async_start);
	LOGD("video_playback_supported : %d\n", ini->video_playback_supported);
	LOGD("pcm buffer size(bytes) : %d\n", ini->pcm_buffer_size);
	LOGD("num of video bo : %d\n", ini->num_of_video_bo);
	LOGD("video bo timeout : %d\n", ini->video_bo_timeout);
	LOGD("gst param1 : %s\n", ini->gst_param[0]);
	LOGD("gst param2 : %s\n", ini->gst_param[1]);
	LOGD("gst param3 : %s\n", ini->gst_param[2]);
	LOGD("gst param4 : %s\n", ini->gst_param[3]);
	LOGD("gst param5 : %s\n", ini->gst_param[4]);

	for ( idx = 0; ini->exclude_element_keyword[idx][0] != '\0'; idx++ )
	{
		LOGD("exclude_element_keyword [%d] : %s\n", idx, ini->exclude_element_keyword[idx]);
	}

	for ( idx = 0; ini->dump_element_keyword[idx][0] != '\0'; idx++ )
	{
		LOGD("dump_element_keyword [%d] : %s\n", idx, ini->dump_element_keyword[idx]);
	}

	/* http streaming */
	LOGD("httpsrc element : %s\n", ini->httpsrc_element);
	LOGD("http file buffer path : %s \n", ini->http_file_buffer_path);
	LOGD("http buffering limit : %f \n", ini->http_buffering_limit);
	LOGD("http max_size bytes : %d \n", ini->http_max_size_bytes);
	LOGD("http buffering time : %f \n", ini->http_buffering_time);
	LOGD("http timeout : %d \n", ini->http_timeout);

	return MM_ERROR_NONE;
}

int
mm_player_audio_effect_ini_load(mm_player_ini_t* ini)
{
	dictionary * dict_audioeffect = NULL;

	dict_audioeffect = iniparser_load(MM_PLAYER_INI_DEFAULT_AUDIOEFFECT_PATH);
	if ( !dict_audioeffect )
	{
		LOGE("No audio effect ini file found. \n");
		return MM_ERROR_FILE_NOT_FOUND;
	}

	/* audio effect element name */
	MMPLAYER_INI_GET_STRING( dict_audioeffect, ini->audioeffect_element, "audio effect:audio effect element", DEFAULT_AUDIO_EFFECT_ELEMENT );
	if (!ini->audioeffect_element[0])
	{
		LOGW("could not parse name of audio effect. \n");
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

	/* audio effect user (EQ / Extension effects) */
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
				LOGE("audio_effect_custom_eq_band_num(%d) is not valid range(%d - %d), set the value %d",
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

	/* audio effect element name */
	MMPLAYER_INI_GET_STRING(dict_audioeffect, ini->audioeffect_element_custom, "audio effect:audio effect element custom", DEFAULT_AUDIO_EFFECT_ELEMENT );
	if (!ini->audioeffect_element_custom[0])
	{
		LOGW("no secondary audio effect \n");
	}
	else
	{
		LOGD("audioeffect element custom : %s\n", ini->audioeffect_element_custom);
	}

	/* dump structure */
	LOGD("audioeffect element : %s\n", ini->audioeffect_element);
	LOGD("audio effect preset mode : %d\n", ini->use_audio_effect_preset);
	LOGD("audio effect custom mode : %d\n", ini->use_audio_effect_custom);
#if 0 // debug
	int i;
	for (i=0; i<MM_AUDIO_EFFECT_PRESET_NUM; i++)
	{
		LOGD("audio_effect_preset_list: %d (is it for earphone only?(%d))\n", ini->audio_effect_preset_list[i], ini->audio_effect_preset_earphone_only_list[i]);
	}
	for (i=0; i<MM_AUDIO_EFFECT_CUSTOM_NUM; i++)
	{
		LOGD("audio_effect_custom_list : %d (is it for earphone only?(%d))\n", ini->audio_effect_custom_list[i], ini->audio_effect_custom_earphone_only_list[i]);
	}
	LOGD("audio_effect_custom : eq_band_num(%d), ext_num(%d)\n", ini->audio_effect_custom_eq_band_num, ini->audio_effect_custom_ext_num );
	LOGD("audio_effect_custom_EQ : width(Hz) / central frequency(Hz)");
	for (i=0; i<ini->audio_effect_custom_eq_band_num; i++)
	{
		LOGD("     EQ band index(%d) :  %8d / %8d", i, ini->audio_effect_custom_eq_band_width[i], ini->audio_effect_custom_eq_band_freq[i]);
	}
	for (i=0; i<MM_AUDIO_EFFECT_CUSTOM_NUM; i++)
	{
		LOGD("audio_effect_custom_level_min_max(idx:%d) : Min(%d), Max(%d)\n", i, ini->audio_effect_custom_min_level_list[i], ini->audio_effect_custom_max_level_list[i]);
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
		LOGW("failed to get player ini status\n");
	}
	else
	{
		if ( ini_buff.st_size < 5 )
		{
			LOGW("player.ini file size=%d, Corrupted! So, Removed\n", (int)ini_buff.st_size);

			if ( g_remove( MM_PLAYER_INI_DEFAULT_PATH ) == -1)
			{
				LOGE("failed to delete corrupted ini");
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
