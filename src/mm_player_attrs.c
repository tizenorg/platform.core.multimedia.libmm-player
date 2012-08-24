/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, YeJin Cho <cho.yejin@samsung.com>,
 * YoungHwan An <younghwan_.an@samsung.com>
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

/*===========================================================================================
|																							|
|  INCLUDE FILES																			|
|  																							|
========================================================================================== */
#include <vconf.h>
#include <mm_attrs_private.h>
#include <mm_attrs.h>
#include <gst/interfaces/xoverlay.h>

#include "mm_player_priv.h"
#include "mm_player_attrs.h"

/*===========================================================================================
|																							|
|  LOCAL DEFINITIONS AND DECLARATIONS FOR MODULE											|
|  																							|
========================================================================================== */

typedef struct{
	char *name;
	int value_type;
	int flags;				// r, w
	void *default_value;
	int valid_type;			// validity type
	int value_min;			//<- set validity value range
	int value_max;		//->
}MMPlayerAttrsSpec;

/*---------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:												|
---------------------------------------------------------------------------*/
int
__mmplayer_apply_attribute(MMHandleType handle, const char *attribute_name);

/*===========================================================================================
|																										|
|  FUNCTION DEFINITIONS																					|
|  																										|
========================================================================================== */

int
_mmplayer_get_attribute(MMHandleType handle,  char **err_attr_name, const char *attribute_name, va_list args_list)
{
	int result = MM_ERROR_NONE;
	MMHandleType attrs = 0;

	debug_fenter();

	/* NOTE : Don't need to check err_attr_name because it can be set NULL */
	/* if it's not want to know it. */
	return_val_if_fail(attribute_name, MM_ERROR_COMMON_INVALID_ARGUMENT);
	return_val_if_fail(handle, MM_ERROR_COMMON_INVALID_ARGUMENT);

	attrs = MM_PLAYER_GET_ATTRS(handle);

	result = mm_attrs_get_valist(attrs, err_attr_name, attribute_name, args_list);

	if ( result != MM_ERROR_NONE)
		debug_error("failed to get %s attribute\n", attribute_name);

	debug_fleave();

	return result;
}

int
_mmplayer_set_attribute(MMHandleType handle,  char **err_attr_name, const char *attribute_name, va_list args_list)
{
	int result = MM_ERROR_NONE;
	MMHandleType attrs = 0;

	debug_fenter();

	/* NOTE : Don't need to check err_attr_name because it can be set NULL */
	/* if it's not want to know it. */
	return_val_if_fail(attribute_name, MM_ERROR_COMMON_INVALID_ARGUMENT);
	return_val_if_fail(handle, MM_ERROR_COMMON_INVALID_ARGUMENT);

	attrs = MM_PLAYER_GET_ATTRS(handle);

	/* set attributes and commit them */
	result = mm_attrs_set_valist(attrs, err_attr_name, attribute_name, args_list);

	if (result != MM_ERROR_NONE)
	{
		debug_error("failed to set %s attribute\n", attribute_name);
		return result;
	}

	result = __mmplayer_apply_attribute(handle, attribute_name);
	if (result != MM_ERROR_NONE)
	{
		debug_error("failed to apply attributes\n");
		return result;
	}

	debug_fleave();

	return result;
}

int
_mmplayer_get_attributes_info(MMHandleType handle,  const char *attribute_name, MMPlayerAttrsInfo *dst_info)
{
	int result = MM_ERROR_NONE;
	MMHandleType attrs = 0;
	MMAttrsInfo src_info = {0, };

	debug_fenter();

	return_val_if_fail(attribute_name, MM_ERROR_COMMON_INVALID_ARGUMENT);
	return_val_if_fail(dst_info, MM_ERROR_COMMON_INVALID_ARGUMENT);
	return_val_if_fail(handle, MM_ERROR_COMMON_INVALID_ARGUMENT);

	attrs = MM_PLAYER_GET_ATTRS(handle);

	result = mm_attrs_get_info_by_name(attrs, attribute_name, &src_info);

	if ( result != MM_ERROR_NONE)
	{
		debug_error("failed to get attribute info\n");
		return result;
	}

	memset(dst_info, 0x00, sizeof(MMPlayerAttrsInfo));

	dst_info->type = src_info.type;
	dst_info->flag = src_info.flag;
	dst_info->validity_type= src_info.validity_type;

	switch(src_info.validity_type)
	{
		case MM_ATTRS_VALID_TYPE_INT_ARRAY:
			dst_info->int_array.array = src_info.int_array.array;
			dst_info->int_array.count = src_info.int_array.count;
			dst_info->int_array.d_val = src_info.int_array.dval;
		break;

		case MM_ATTRS_VALID_TYPE_INT_RANGE:
			dst_info->int_range.min = src_info.int_range.min;
			dst_info->int_range.max = src_info.int_range.max;
			dst_info->int_range.d_val = src_info.int_range.dval;
		break;

		case MM_ATTRS_VALID_TYPE_DOUBLE_ARRAY:
			dst_info->double_array.array = src_info.double_array.array;
			dst_info->double_array.count = src_info.double_array.count;
			dst_info->double_array.d_val = src_info.double_array.dval;
		break;

		case MM_ATTRS_VALID_TYPE_DOUBLE_RANGE:
			dst_info->double_range.min = src_info.double_range.min;
			dst_info->double_range.max = src_info.double_range.max;
			dst_info->double_range.d_val = src_info.double_range.dval;
		break;

		default:
		break;
	}

	debug_fleave();

	return result;
}

int
__mmplayer_apply_attribute(MMHandleType handle, const char *attribute_name)
{
	MMHandleType attrs = 0;
	mm_player_t* player = 0;

	debug_fenter();

	return_val_if_fail(handle, MM_ERROR_COMMON_INVALID_ARGUMENT);
	return_val_if_fail(attribute_name, MM_ERROR_COMMON_INVALID_ARGUMENT);

	attrs = MM_PLAYER_GET_ATTRS(handle);;
	player = MM_PLAYER_CAST(handle);

	if ( g_strrstr(attribute_name, "display") )
	{
		/* check videosink element is created */
		if ( !player->pipeline ||
			 !player->pipeline->videobin ||
			 !player->pipeline->videobin[MMPLAYER_V_SINK].gst )
		{
			debug_warning("videosink element is not yet ready\n");
			/*
			 * The attribute should be committed even though videobin is not created yet.
			 * So, true should be returned here.
			 * Otherwise, video can be diaplayed abnormal.
			 */
			return MM_ERROR_NONE;
		}

		if ( MM_ERROR_NONE != _mmplayer_update_video_param( player ) )
		{
			debug_error("failed to update video param\n");
			return MM_ERROR_PLAYER_INTERNAL;
		}
	}

	debug_fleave();

	return MM_ERROR_NONE;
}

MMHandleType
_mmplayer_construct_attribute(MMHandleType handle)
{
	int idx = 0;
	MMHandleType attrs = 0;
	int num_of_attrs = 0;
	mmf_attrs_construct_info_t *base = NULL;
	gchar *system_ua = NULL;
	gchar *system_proxy = NULL;

	debug_fenter();

	return_if_fail(handle);

	MMPlayerAttrsSpec player_attrs[] =
	{
		{
			"profile_uri",			// name
			MM_ATTRS_TYPE_STRING,		// type
			MM_ATTRS_FLAG_RW, 		// flag
			(void *) NULL,			// default value
			MM_ATTRS_VALID_TYPE_NONE,	// validity type
			0,				// validity min value
			0				// validity max value
		},
		{
			"profile_user_param",
			MM_ATTRS_TYPE_DATA,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"profile_play_count",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 1,			// -1 : repeat continually
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			-1,
			MMPLAYER_MAX_INT
		},
		{
			"profile_async_start",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 1,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			1
		},
		{	/* update registry for downloadable codec */
			"profile_update_registry",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			1
		},
		{
			"streaming_type",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) STREAMING_SERVICE_NONE,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			STREAMING_SERVICE_VOD,
			STREAMING_SERVICE_NUM
		},
		{
			"streaming_udp_timeout",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 10000,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"streaming_user_agent",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"streaming_wap_profile",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"streaming_network_bandwidth",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 128000,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"streaming_cookie",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"streaming_proxy",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"subtitle_uri",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"content_duration",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"content_bitrate",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"content_max_bitrate",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"content_video_found",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			1
		},
		{
			"content_video_codec",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"content_video_bitrate",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"content_video_fps",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"content_video_width",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"content_video_height",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"content_video_track_num",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"content_audio_found",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			1
		},
		{
			"content_audio_codec",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"content_audio_bitrate",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"content_audio_channels",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"content_audio_samplerate",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"content_audio_track_num",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"content_audio_format",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"tag_artist",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"tag_title",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"tag_album",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL
		},
		{
			"tag_genre",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"tag_author",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"tag_copyright",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"tag_date",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"tag_description",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"tag_track_num",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"tag_album_cover",
			MM_ATTRS_TYPE_DATA,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"display_roi_x",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"display_roi_y",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"display_roi_width",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 480,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"display_roi_height",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 800,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"display_rotation",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) MM_DISPLAY_ROTATION_NONE,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			MM_DISPLAY_ROTATION_NONE,
			MM_DISPLAY_ROTATION_270
		},
		{
			"display_visible",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) TRUE,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			1
		},
		{
			"display_method",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) MM_DISPLAY_METHOD_LETTER_BOX,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			MM_DISPLAY_METHOD_LETTER_BOX,
			MM_DISPLAY_METHOD_CUSTOM_ROI
		},
		{
			"display_overlay",
			MM_ATTRS_TYPE_DATA,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"display_overlay_ext",
			MM_ATTRS_TYPE_DATA,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"display_zoom",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 1,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			1,
			MMPLAYER_MAX_INT
		},
		{
			"display_surface_type",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			MM_DISPLAY_SURFACE_X,
			MM_DISPLAY_SURFACE_NULL
		},
		{
			"display_surface_use_multi",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) FALSE,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			FALSE,
			TRUE
		},
		{
			"display_evas_surface_sink",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_READABLE,
			(void *) PLAYER_INI()->videosink_element_evas,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"display_force_aspect_ration",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 1,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"display_width",		// dest width of fimcconvert ouput
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"display_height",		// dest height of fimcconvert ouput
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"display_evas_do_scaling",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) TRUE,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			FALSE,
			TRUE
		},
		{
			"sound_fadeup",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) FALSE,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			FALSE,
			TRUE
		},
		{
			"sound_fadedown",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) FALSE,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			FALSE,
			TRUE
		},
		{
			"sound_volume_type",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) MM_SOUND_VOLUME_TYPE_MEDIA,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			MM_SOUND_VOLUME_TYPE_SYSTEM,
			MM_SOUND_VOLUME_TYPE_CALL
		},
		{
			"sound_route",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) MM_AUDIOROUTE_USE_EXTERNAL_SETTING,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			MM_AUDIOROUTE_USE_EXTERNAL_SETTING,
			MM_AUDIOROUTE_CAPTURE_STEREOMIC_ONLY
		},
		{
			"sound_stop_when_unplugged",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) TRUE,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			FALSE,
			TRUE
		},
		{
			"sound_application_pid",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"sound_spk_out_only",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) FALSE,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			FALSE,
			TRUE
		},
		{
			"sound_priority",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,			// 0: normal, 1: high 2: high with sound transition
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			2
		},
		{
			"pcm_extraction",		// enable pcm extraction
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) FALSE,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			FALSE,
			TRUE
		},
		{
			"pcm_extraction_samplerate",	// set samplerate for pcm extraction
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 8000,				// hz
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"pcm_extraction_depth",	// set depth for pcm extraction
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 16,			// bits
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"pcm_extraction_channels",	// set channels for pcm extraction
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 1,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"pcm_extraction_start_msec",	// set start position to extract pcm
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"pcm_extraction_end_msec",	// set end position to extract pcm
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"profile_smooth_repeat",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) FALSE,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"profile_progress_interval",	// will be deprecated
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 500,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"display_x",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"display_y",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"pd_mode",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) MM_PLAYER_PD_MODE_NONE,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			MM_PLAYER_PD_MODE_NONE,
			MM_PLAYER_PD_MODE_URI		// not tested yet, because of no fixed scenario
		},
		{
			"pd_location",			// location of the file to write
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		}
	};

	num_of_attrs = ARRAY_SIZE(player_attrs);

	base = (mmf_attrs_construct_info_t* )malloc(num_of_attrs * sizeof(mmf_attrs_construct_info_t));

	if ( !base )
	{
		debug_error("Cannot create mmplayer attribute\n");
		goto ERROR;
	}

	/* initialize values of attributes */
	for ( idx = 0; idx < num_of_attrs; idx++ )
	{
		base[idx].name = player_attrs[idx].name;
		base[idx].value_type = player_attrs[idx].value_type;
		base[idx].flags = player_attrs[idx].flags;
		base[idx].default_value = player_attrs[idx].default_value;
	}

	attrs = mmf_attrs_new_from_data(
					"mmplayer_attrs",
					base,
					num_of_attrs,
					NULL,
					NULL);

	/* clean */
	MMPLAYER_FREEIF(base);

	if ( !attrs )
	{
		debug_error("Cannot create mmplayer attribute\n");
		goto ERROR;
	}

	/* set validity type and range */
	for ( idx = 0; idx < num_of_attrs; idx++ )
	{
		switch ( player_attrs[idx].valid_type)
		{
			case MM_ATTRS_VALID_TYPE_INT_RANGE:
			{
				mmf_attrs_set_valid_type (attrs, idx, MM_ATTRS_VALID_TYPE_INT_RANGE);
				mmf_attrs_set_valid_range (attrs, idx,
						player_attrs[idx].value_min,
						player_attrs[idx].value_max,
						player_attrs[idx].default_value);
			}
			break;

			case MM_ATTRS_VALID_TYPE_INT_ARRAY:
			case MM_ATTRS_VALID_TYPE_DOUBLE_ARRAY:
			case MM_ATTRS_VALID_TYPE_DOUBLE_RANGE:
			default:
			break;
		}
	}

	/* set proxy and user agent */
	system_ua = vconf_get_str(VCONFKEY_ADMIN_UAGENT);
	system_proxy = vconf_get_str(VCONFKEY_NETWORK_PROXY);

	if (system_ua)
	{
			mm_attrs_set_string_by_name(attrs, "streaming_user_agent", system_ua);
			g_free(system_ua);
	}

	if (system_proxy)
	{
			mm_attrs_set_string_by_name(attrs, "streaming_proxy", system_proxy);
			g_free(system_proxy);
	}

	/* commit */
	mmf_attrs_commit(attrs);

	debug_fleave();

	return attrs;

ERROR:
	_mmplayer_deconstruct_attribute(handle);

	return FALSE;
}

bool
_mmplayer_deconstruct_attribute(MMHandleType handle) // @
{
	debug_fenter();

	mm_player_t *player = MM_PLAYER_CAST(handle);

	return_if_fail( player );

	if (player->attrs)
	{
		mmf_attrs_free (player->attrs);
		player->attrs = 0;
	}

	debug_fleave();
}
