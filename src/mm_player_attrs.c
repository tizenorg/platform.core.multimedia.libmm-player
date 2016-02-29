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
|																							|
========================================================================================== */
#include <dlog.h>
#include <mm_attrs_private.h>
#include <mm_attrs.h>
#include "mm_player_utils.h"
#include "mm_player_priv.h"
#include "mm_player_attrs.h"

/*===========================================================================================
|																							|
|  LOCAL DEFINITIONS AND DECLARATIONS FOR MODULE											|
|																							|
========================================================================================== */

/*---------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:												|
---------------------------------------------------------------------------*/
int
__mmplayer_apply_attribute(MMHandleType handle, const char *attribute_name);

/*===========================================================================================
|																										|
|  FUNCTION DEFINITIONS																					|
|																										|
========================================================================================== */

int
_mmplayer_get_attribute(MMHandleType handle,  char **err_attr_name, const char *attribute_name, va_list args_list)
{
	int result = MM_ERROR_NONE;
	MMHandleType attrs = 0;

	/* NOTE : Don't need to check err_attr_name because it can be set NULL */
	/* if it's not want to know it. */
	MMPLAYER_RETURN_VAL_IF_FAIL(attribute_name, MM_ERROR_COMMON_INVALID_ARGUMENT);
	MMPLAYER_RETURN_VAL_IF_FAIL(handle, MM_ERROR_COMMON_INVALID_ARGUMENT);

	attrs = MM_PLAYER_GET_ATTRS(handle);

	result = mm_attrs_get_valist(attrs, err_attr_name, attribute_name, args_list);

	if ( result != MM_ERROR_NONE)
		LOGE("failed to get %s attribute\n", attribute_name);

	return result;
}

int
_mmplayer_set_attribute(MMHandleType handle,  char **err_attr_name, const char *attribute_name, va_list args_list)
{
	int result = MM_ERROR_NONE;
	MMHandleType attrs = 0;

	/* NOTE : Don't need to check err_attr_name because it can be set NULL */
	/* if it's not want to know it. */
	MMPLAYER_RETURN_VAL_IF_FAIL(attribute_name, MM_ERROR_COMMON_INVALID_ARGUMENT);
	MMPLAYER_RETURN_VAL_IF_FAIL(handle, MM_ERROR_COMMON_INVALID_ARGUMENT);

	attrs = MM_PLAYER_GET_ATTRS(handle);

	/* set attributes and commit them */
	result = mm_attrs_set_valist(attrs, err_attr_name, attribute_name, args_list);

	if (result != MM_ERROR_NONE)
	{
		LOGE("failed to set %s attribute\n", attribute_name);
		return result;
	}

	result = __mmplayer_apply_attribute(handle, attribute_name);
	if (result != MM_ERROR_NONE)
	{
		LOGE("failed to apply attributes\n");
		return result;
	}

	return result;
}

int
_mmplayer_get_attributes_info(MMHandleType handle,  const char *attribute_name, MMPlayerAttrsInfo *dst_info)
{
	int result = MM_ERROR_NONE;
	MMHandleType attrs = 0;
	MMAttrsInfo src_info = {0, };

	MMPLAYER_RETURN_VAL_IF_FAIL(attribute_name, MM_ERROR_COMMON_INVALID_ARGUMENT);
	MMPLAYER_RETURN_VAL_IF_FAIL(dst_info, MM_ERROR_COMMON_INVALID_ARGUMENT);
	MMPLAYER_RETURN_VAL_IF_FAIL(handle, MM_ERROR_COMMON_INVALID_ARGUMENT);

	attrs = MM_PLAYER_GET_ATTRS(handle);

	result = mm_attrs_get_info_by_name(attrs, attribute_name, &src_info);

	if ( result != MM_ERROR_NONE)
	{
		LOGE("failed to get attribute info\n");
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

	return result;
}

int
__mmplayer_apply_attribute(MMHandleType handle, const char *attribute_name)
{
	mm_player_t* player = 0;

	MMPLAYER_RETURN_VAL_IF_FAIL(handle, MM_ERROR_COMMON_INVALID_ARGUMENT);
	MMPLAYER_RETURN_VAL_IF_FAIL(attribute_name, MM_ERROR_COMMON_INVALID_ARGUMENT);

	player = MM_PLAYER_CAST(handle);

	if ( g_strrstr(attribute_name, "display") )
	{
		int pipeline_type = 0;
		MMPlayerGstPipelineInfo	*pipeline = player->pipeline;

		/* check videosink element is created */
		if(!pipeline)
			return MM_ERROR_NONE;
		mm_attrs_get_int_by_name(player->attrs, "pipeline_type", &pipeline_type);
		if (pipeline_type == MM_PLAYER_PIPELINE_CLIENT) {
			if(!pipeline->mainbin || !pipeline->mainbin[MMPLAYER_M_V_SINK].gst)
				return MM_ERROR_NONE;
		} else {
			if(!pipeline->videobin || !pipeline->videobin[MMPLAYER_V_SINK].gst)
				return MM_ERROR_NONE;
		}

		if ( MM_ERROR_NONE != _mmplayer_update_video_param( player ) )
		{
			LOGE("failed to update video param");
			return MM_ERROR_PLAYER_INTERNAL;
		}
	}

	return MM_ERROR_NONE;
}

MMHandleType
_mmplayer_construct_attribute(MMHandleType handle)
{
	mm_player_t *player = NULL;
	int idx = 0;
	MMHandleType attrs = 0;
	int num_of_attrs = 0;
	mmf_attrs_construct_info_t *base = NULL;

	MMPLAYER_RETURN_VAL_IF_FAIL (handle, 0);

	player = MM_PLAYER_CAST(handle);

	MMPlayerAttrsSpec player_attrs[] =
	{
		{
			"profile_uri",					// name
			MM_ATTRS_TYPE_STRING,		// type
			MM_ATTRS_FLAG_RW,			// flag
			(void *) NULL,				// default value
			MM_ATTRS_VALID_TYPE_NONE,	// validity type
			0,							// validity min value
			0							// validity max value
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
			"profile_prepare_async",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
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
			"streaming_timeout",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) -1,	// DEFAULT_HTTP_TIMEOUT
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			-1,
			MMPLAYER_MAX_INT
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
			"content_text_track_num",
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
			"display_src_crop_x",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"display_src_crop_y",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"display_src_crop_width",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"display_src_crop_height",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
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
			"display_roi_mode",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) MM_DISPLAY_METHOD_CUSTOM_ROI_FULL_SCREEN,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			MM_DISPLAY_METHOD_CUSTOM_ROI_FULL_SCREEN,
			MM_DISPLAY_METHOD_CUSTOM_ROI_LETER_BOX
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
			(void *) FALSE,
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
#ifdef HAVE_WAYLAND
		{
			"wl_display",
			MM_ATTRS_TYPE_DATA,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"wl_window_render_x",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"wl_window_render_y",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"wl_window_render_width",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"wl_window_render_height",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"use_wl_surface",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) FALSE,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			FALSE,
			TRUE
		},

#endif
		{
			"display_overlay_user_data",
			MM_ATTRS_TYPE_DATA,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"display_surface_type",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) MM_DISPLAY_SURFACE_NULL,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			MM_DISPLAY_SURFACE_OVERLAY,
			MM_DISPLAY_SURFACE_NUM - 1
		},
		{
			"display_evas_surface_sink",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_READABLE,
			(void *) player->ini.videosink_element_evas,
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
			0,
			MMPLAYER_MAX_INT
		},
		{
			"sound_stream_type",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"sound_stream_index",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			-1,
			MMPLAYER_MAX_INT
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
			(void *) 0,			// 0: normal, 1: high 2: high with sound transition 3: mix with others regardless of priority
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			3
		},
		{
			"sound_close_resource",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			1
		},
		{
			"sound_latency_mode",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 1,			// 0: low latency, 1: middle latency 2: high latency
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
			(void *) 44100,				// hz
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
		},
		{
			"accurate_seek",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			1
		},
		{
			"content_video_orientation",	// orientation of video content
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"pcm_audioformat",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"display_surface_client_type",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) MM_DISPLAY_SURFACE_NULL,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			MM_DISPLAY_SURFACE_OVERLAY,
			MM_DISPLAY_SURFACE_NUM - 1
		},
		{
			"pipeline_type",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) MM_PLAYER_PIPELINE_LEGACY,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			MM_PLAYER_PIPELINE_LEGACY,
			MM_PLAYER_PIPELINE_MAX - 1
		}
	};

	num_of_attrs = ARRAY_SIZE(player_attrs);

	base = (mmf_attrs_construct_info_t* )malloc(num_of_attrs * sizeof(mmf_attrs_construct_info_t));

	if ( !base )
	{
		LOGE("failed to alloc attrs constructor");
		return 0;
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
		LOGE("failed to create player attrs");
		return 0;
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
						(int)(intptr_t)(player_attrs[idx].default_value));
			}
			break;

			case MM_ATTRS_VALID_TYPE_INT_ARRAY:
			case MM_ATTRS_VALID_TYPE_DOUBLE_ARRAY:
			case MM_ATTRS_VALID_TYPE_DOUBLE_RANGE:
			default:
			break;
		}
	}

	/* commit */
	mmf_attrs_commit(attrs);

	return attrs;
}

bool
_mmplayer_deconstruct_attribute(MMHandleType handle) // @
{
	mm_player_t *player = MM_PLAYER_CAST(handle);

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );

	if (player->attrs)
	{
		mmf_attrs_free (player->attrs);
		player->attrs = 0;
	}

	return TRUE;
}
