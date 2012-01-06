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

/*===========================================================================================
|																							|
|  INCLUDE FILES																			|
|  																							|
========================================================================================== */
#include "mm_player_priv.h"
#include "mm_player_attrs.h"
#include <mm_attrs_private.h>
#include <mm_attrs.h>

/*===========================================================================================
|																							|
|  LOCAL DEFINITIONS AND DECLARATIONS FOR MODULE											|
|  																							|
========================================================================================== */

/*---------------------------------------------------------------------------
|    GLOBAL CONSTANT DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    IMPORTED VARIABLE DECLARATIONS:										|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    IMPORTED FUNCTION DECLARATIONS:										|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL #defines:														|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL CONSTANT DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL DATA TYPE DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:												|
---------------------------------------------------------------------------*/
static gboolean __mmplayer_apply_attribute(mm_player_t* player, const char *attribute_name);

/*===========================================================================================
|																							|
|  FUNCTION DEFINITIONS																		|
|  																							|
========================================================================================== */


int
_mmplayer_get_attribute(MMHandleType hplayer,  char **err_atr_name, const char *attribute_name, va_list args_list)
{
	int result = MM_ERROR_NONE;
	MMHandleType attrs = 0;
	mm_player_t* player = (mm_player_t*)hplayer;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(attribute_name, MM_ERROR_COMMON_INVALID_ARGUMENT);

#if 0
	/* update duration for VBR */
	if (strcmp(attribute_name, "content_duration") == 0 && player->can_support_codec == FOUND_PLUGIN_AUDIO)
	{
		player->need_update_content_attrs = TRUE;
		_mmplayer_update_content_attrs(player);
	}
#endif	

	attrs = MMPLAYER_GET_ATTRS(hplayer);

	return_val_if_fail(attrs, MM_ERROR_COMMON_INVALID_ARGUMENT);

	result = mm_attrs_get_valist(attrs, err_atr_name, attribute_name, args_list);

	return result;
}


int
_mmplayer_set_attribute(MMHandleType hplayer,  char **err_atr_name, const char *attribute_name, va_list args_list)
{
	int result = MM_ERROR_NONE;
	mm_player_t* player = (mm_player_t*)hplayer;
	MMHandleType attrs = 0;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(attribute_name, MM_ERROR_COMMON_INVALID_ARGUMENT);

	attrs = MMPLAYER_GET_ATTRS(hplayer);

	return_val_if_fail(attrs, MM_ERROR_COMMON_INVALID_ARGUMENT);

	/* set attributes and commit them */
	result = mm_attrs_set_valist(attrs, err_atr_name, attribute_name, args_list);

	if (result == MM_ERROR_NONE)
		__mmplayer_apply_attribute(player, attribute_name);

	return result;
}


int
_mmplayer_get_attributes_info(MMHandleType player,  const char *attribute_name, MMPlayerAttrsInfo *dst_info)
{
	int result = MM_ERROR_NONE;
	MMHandleType attrs = 0;
	MMAttrsInfo src_info = {0, };
	
	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(attribute_name, MM_ERROR_COMMON_INVALID_ARGUMENT);
	return_val_if_fail(dst_info, MM_ERROR_COMMON_INVALID_ARGUMENT);

	attrs = MMPLAYER_GET_ATTRS(player);

	return_val_if_fail(attrs, MM_ERROR_COMMON_INVALID_ARGUMENT);

	result = mm_attrs_get_info_by_name(attrs, attribute_name, &src_info);

	if (result == MM_ERROR_NONE)
	{
		memset(dst_info, 0x00, sizeof(MMPlayerAttrsInfo));
		dst_info->type = src_info.type;
		dst_info->flag = src_info.flag;
		dst_info->validity_type= src_info.validity_type;

		switch(src_info.validity_type)
		{
			case MM_ATTRS_VALID_TYPE_INT_ARRAY:
				dst_info->int_array.array = src_info.int_array.array;
				dst_info->int_array.count = src_info.int_array.count;
			break;
			
			case MM_ATTRS_VALID_TYPE_INT_RANGE:
				dst_info->int_range.min = src_info.int_range.min;
				dst_info->int_range.max = src_info.int_range.max;
			break;
			
			case MM_ATTRS_VALID_TYPE_DOUBLE_ARRAY:
				dst_info->double_array.array = src_info.double_array.array;
				dst_info->double_array.count = src_info.double_array.count;
			break;
			
			case MM_ATTRS_VALID_TYPE_DOUBLE_RANGE:
				dst_info->double_range.min = src_info.double_range.min;
				dst_info->double_range.max = src_info.double_range.max;
			break;
			
			default:
			break;
		}
	}

	return result;
}


static gboolean
__mmplayer_apply_attribute(mm_player_t* player, const char *attribute_name)
{
	MMHandleType attrs = 0;

	debug_log("name: %s \n", attribute_name);

	attrs = player->attrs;

	if ( !attrs )
	{
		return FALSE;
	}

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
			return TRUE;
		}

		/* check video stream callback is used */
		if( player->use_video_stream )
		{
			if ( MM_ERROR_NONE != _mmplayer_update_video_param( player ) )
			{
				debug_error("failed to update video param for memsink\n");
				return MM_ERROR_PLAYER_INTERNAL;
			}

			return FALSE;
		}

		if ( PLAYER_INI()->videosink_element == PLAYER_INI_VSINK_V4l2SINK )
		{
			if ( MM_ERROR_NONE != _mmplayer_update_video_param( player ) )
			{
				debug_error("failed to update video param\n");
				return MM_ERROR_PLAYER_INTERNAL;
			}
		}
		/* FIXIT : think about ximagesink, xvimagesink could be handled in same manner */
		else
		{
			int display_method = 0;
			int display_rotation = 0;
			gboolean bvisible = 0;
			int roi_x = 0;
			int roi_y = 0;
			int roi_w = 0;
			int roi_h = 0;

			#if 0
			MMAttrsGetData(attrs, MM_PLAYER_DISPLAY_OVERLAY, &val,  &size);

			/* set overlay id */
			if ( val )
			{
				gst_x_overlay_set_xwindow_id(
					GST_X_OVERLAY(player->pipeline->videobin[MMPLAYER_V_SINK].gst), *(int*)val);
			}
			else
			{
				debug_warning("still we don't have xid on player attribute. create it's own surface.\n");
				return MM_ERROR_NONE;
			}
			#endif

			mm_attrs_get_int_by_name(attrs, "display_rotation", &display_rotation);
			mm_attrs_get_int_by_name(attrs, "display_method", &display_method);
			mm_attrs_get_int_by_name(attrs, "display_roi_x", &roi_x);
			mm_attrs_get_int_by_name(attrs, "display_roi_y", &roi_y);
			mm_attrs_get_int_by_name(attrs, "display_roi_width", &roi_w);
			mm_attrs_get_int_by_name(attrs, "display_roi_height", &roi_h);
			mm_attrs_get_int_by_name(attrs, "display_visible", &bvisible);			

			g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
				"rotate", display_rotation,
				"visible", bvisible,				
				"display-geometry-method", display_method,
				"dst-roi-x", roi_x,
				"dst-roi-y", roi_y,
				"dst-roi-w", roi_w,
				"dst-roi-h", roi_h,
				NULL );
			
			debug_log("setting video param \n"); 
			debug_log("rotate:%d, geometry:%d, visible:%d \n", display_rotation, display_method, bvisible);
			debug_log("dst-roi-x:%d, dst-roi-y:%d, dst-roi-w:%d, dst-roi-h:%d\n",
				display_rotation, display_method, roi_x, roi_y, roi_w, roi_h );

		}
	}

	return TRUE;
}


bool
_mmplayer_construct_attribute(mm_player_t* player)
{
	int idx = 0;
	MMHandleType attrs = 0;

	debug_log("\n");

	return_val_if_fail(player != NULL, FALSE);

	mmf_attrs_construct_info_t player_attrs[] = {
		/* profile  */
		{"profile_uri",						MM_ATTRS_TYPE_STRING,	MM_ATTRS_FLAG_RW, (void *)0},
		{"profile_user_param",				MM_ATTRS_TYPE_DATA, 	MM_ATTRS_FLAG_RW, (void *)0},
		{"profile_play_count",				MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)1},
		{"profile_update_registry",			MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		/*streaming */
		{"streaming_type",					MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *) STREAMING_SERVICE_NONE },
		{"streaming_udp_timeout",			MM_ATTRS_TYPE_INT,		MM_ATTRS_FLAG_RW, (void *)10000},
		{"streaming_user_agent",			MM_ATTRS_TYPE_STRING, 	MM_ATTRS_FLAG_RW, (void *)NULL},
		{"streaming_wap_profile",			MM_ATTRS_TYPE_STRING, 	MM_ATTRS_FLAG_RW, (void *)NULL},
		{"streaming_network_bandwidth",	MM_ATTRS_TYPE_INT,		MM_ATTRS_FLAG_RW, (void *)128000},
		{"streaming_cookie",				MM_ATTRS_TYPE_STRING, 	MM_ATTRS_FLAG_RW, (void *)NULL},
		{"streaming_proxy",				MM_ATTRS_TYPE_STRING, 	MM_ATTRS_FLAG_RW, (void *)NULL},
		/* subtitle */
		{"subtitle_uri",					MM_ATTRS_TYPE_STRING, 	MM_ATTRS_FLAG_RW, (void *)NULL},
		{"subtitle_silent",					MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		/* content */
		{"content_duration",				MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"content_bitrate",					MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"content_max_bitrate",				MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"content_video_codec",			MM_ATTRS_TYPE_STRING,	MM_ATTRS_FLAG_RW, (void *)NULL},
		{"content_video_bitrate",			MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"content_video_fps",				MM_ATTRS_TYPE_INT,		MM_ATTRS_FLAG_RW, (void *)0},
		{"content_video_width",			MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"content_video_height",			MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"content_video_track_num",		MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"content_audio_codec",			MM_ATTRS_TYPE_STRING, 	MM_ATTRS_FLAG_RW, (void *)NULL},
		{"content_audio_bitrate",			MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"content_audio_channels",			MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"content_audio_samplerate",		MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"content_audio_track_num",		MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"content_audio_format",			MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		/* tag */
		{"tag_artist",						MM_ATTRS_TYPE_STRING, 	MM_ATTRS_FLAG_RW, (void *)NULL},
		{"tag_title",						MM_ATTRS_TYPE_STRING, 	MM_ATTRS_FLAG_RW, (void *)NULL},
		{"tag_album",						MM_ATTRS_TYPE_STRING, 	MM_ATTRS_FLAG_RW, (void *)NULL},
		{"tag_genre",						MM_ATTRS_TYPE_STRING, 	MM_ATTRS_FLAG_RW, (void *)NULL},
		{"tag_author",					MM_ATTRS_TYPE_STRING, 	MM_ATTRS_FLAG_RW, (void *)NULL},
		{"tag_copyright",					MM_ATTRS_TYPE_STRING, 	MM_ATTRS_FLAG_RW, (void *)NULL},
		{"tag_date",						MM_ATTRS_TYPE_STRING, 	MM_ATTRS_FLAG_RW, (void *)NULL},
		{"tag_description",					MM_ATTRS_TYPE_STRING, 	MM_ATTRS_FLAG_RW, (void *)NULL},
		{"tag_track_num",					MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, 	(void *)0},
		/* display */
		{"display_roi_x",					MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"display_roi_y",					MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"display_roi_width",				MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)480},
		{"display_roi_height",				MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)800},
		{"display_rotation",				MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)MM_DISPLAY_ROTATION_NONE},
		{"display_visible",					MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)TRUE},
		{"display_method",					MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"display_overlay",   		          	MM_ATTRS_TYPE_DATA, 	MM_ATTRS_FLAG_RW, (void *)NULL},
		{"display_zoom",					MM_ATTRS_TYPE_INT,		MM_ATTRS_FLAG_RW, (void *)1},
		{"display_surface_type",			MM_ATTRS_TYPE_INT,		MM_ATTRS_FLAG_RW, (void *)-1},
		{"display_force_aspect_ration",         MM_ATTRS_TYPE_INT,        MM_ATTRS_FLAG_RW, (void *)1},		
		/* sound */
		{"sound_fadeup", 					MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)FALSE},
		{"sound_fadedown", 				MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)FALSE},		
		{"sound_bgm_mode", 				MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"sound_volume_type", 				MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)MM_SOUND_VOLUME_TYPE_MEDIA},
		{"sound_route", 					MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)MM_AUDIOROUTE_USE_EXTERNAL_SETTING},
		{"sound_stop_when_unplugged", 		MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)TRUE},
		{"sound_application_pid",			MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"sound_spk_out_only",				MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)FALSE},
		{"sound_priority",					MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0}, // 0: normal, 1: high 2: high with sound transition
		/* pcm extraction */
		{"pcm_extraction",					MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)FALSE},
		{"pcm_extraction_samplerate",		MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"pcm_extraction_start_msec",		MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"pcm_extraction_end_msec",		MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		/* etc */
		{"profile_smooth_repeat",			MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)FALSE},
		{"profile_progress_interval",			MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)500},
		{"display_x",						MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"display_y",						MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"display_width",					MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"display_height",					MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)0},
		{"pd_mode",	                                        MM_ATTRS_TYPE_INT, 		MM_ATTRS_FLAG_RW, (void *)MM_PLAYER_PD_MODE_NONE},
	};

	player->attrs = mmf_attrs_new_from_data( "mmplayer_attrs",
												player_attrs,
												ARRAY_SIZE(player_attrs),
												NULL,
												NULL);

	if ( ! player->attrs )
	{
		debug_error("Cannot create mmplayer attribute\n");
		goto ERROR;
	}

	attrs = player->attrs;

	/* profile */
	mm_attrs_get_index (attrs, "profile_uri", &idx);
	mmf_attrs_set_valid_type (attrs, idx, MM_ATTRS_VALID_TYPE_INT_RANGE);
	mmf_attrs_set_valid_range (attrs, idx, 0, MMPLAYER_MAX_INT);

	mm_attrs_get_index (attrs, "profile_play_count", &idx);
	mmf_attrs_set_valid_type (attrs, idx, MM_ATTRS_VALID_TYPE_INT_RANGE);
	mmf_attrs_set_valid_range (attrs, idx, -1, MMPLAYER_MAX_INT);

	mm_attrs_get_index (attrs, "streaming_type", &idx);
	mmf_attrs_set_valid_type (attrs, idx, MM_ATTRS_VALID_TYPE_INT_RANGE);
	mmf_attrs_set_valid_range (attrs, idx, STREAMING_SERVICE_VOD, STREAMING_SERVICE_NUM);

	mm_attrs_get_index (attrs, "streaming_network_bandwidth", &idx);
	mmf_attrs_set_valid_type (attrs, idx, MM_ATTRS_VALID_TYPE_INT_RANGE);
	mmf_attrs_set_valid_range (attrs, idx, 0, MMPLAYER_MAX_INT);

	mm_attrs_get_index (attrs, "streaming_udp_timeout", &idx);
	mmf_attrs_set_valid_type (attrs, idx, MM_ATTRS_VALID_TYPE_INT_RANGE);
	mmf_attrs_set_valid_range (attrs, idx, 0, MMPLAYER_MAX_INT);

	/* display */
	mm_attrs_get_index (attrs, "display_zoom", &idx);
	mmf_attrs_set_valid_type (attrs, idx, MM_ATTRS_VALID_TYPE_INT_RANGE);
	mmf_attrs_set_valid_range (attrs, idx, 1, MMPLAYER_MAX_INT);

	mm_attrs_get_index (attrs, "display_method", &idx);
	mmf_attrs_set_valid_type (attrs, idx, MM_ATTRS_VALID_TYPE_INT_RANGE);
	mmf_attrs_set_valid_range (attrs, idx, MM_DISPLAY_METHOD_LETTER_BOX, MM_DISPLAY_METHOD_CUSTOM_ROI);

	mm_attrs_get_index (attrs, "display_surface_type", &idx);
	mmf_attrs_set_valid_type (attrs, idx, MM_ATTRS_VALID_TYPE_INT_RANGE);
	mmf_attrs_set_valid_range (attrs, idx, MM_DISPLAY_SURFACE_X, MM_DISPLAY_SURFACE_NULL);

	/* sound */
	mm_attrs_get_index (attrs, "sound_volume_table", &idx);
	mmf_attrs_set_valid_type (attrs, idx, MM_ATTRS_VALID_TYPE_INT_RANGE);
	mmf_attrs_set_valid_range (attrs, idx, MM_SOUND_VOLUME_TYPE_SYSTEM, MM_SOUND_VOLUME_TYPE_CALL);

	mm_attrs_get_index (attrs, "sound_volume_type", &idx);
	mmf_attrs_set_valid_type (attrs, idx, MM_ATTRS_VALID_TYPE_INT_RANGE);
	mmf_attrs_set_valid_range (attrs, idx, MM_SOUND_VOLUME_TYPE_SYSTEM, MM_SOUND_VOLUME_TYPE_CALL);

	mm_attrs_get_index (attrs, "sound_route", &idx);
	mmf_attrs_set_valid_type (attrs, idx, MM_ATTRS_VALID_TYPE_INT_RANGE);
	mmf_attrs_set_valid_range (attrs, idx, MM_AUDIOROUTE_USE_EXTERNAL_SETTING, MM_AUDIOROUTE_CAPTURE_STEREOMIC_ONLY);

	mm_attrs_get_index (attrs, "sound_fadedown", &idx);
	mmf_attrs_set_valid_type (attrs, idx, MM_ATTRS_VALID_TYPE_INT_RANGE);
	mmf_attrs_set_valid_range (attrs, idx, 0, 1);

	mm_attrs_get_index (attrs, "sound_priority", &idx);
	mmf_attrs_set_valid_type (attrs, idx, MM_ATTRS_VALID_TYPE_INT_RANGE);
	mmf_attrs_set_valid_range (attrs, idx, 0, 2);

	mm_attrs_get_index (attrs, "pd_mode", &idx);
	mmf_attrs_set_valid_type (attrs, idx, MM_ATTRS_VALID_TYPE_INT_RANGE);
	mmf_attrs_set_valid_range (attrs, idx, MM_PLAYER_PD_MODE_NONE, MM_PLAYER_PD_MODE_FILE);
	
	return TRUE;

ERROR:

	_mmplayer_release_attrs(player);

	return FALSE;
}


void
_mmplayer_release_attrs(mm_player_t* player) // @
{
	return_if_fail( player );

	if (player->attrs)
	{
		mmf_attrs_free (player->attrs);
	}

	player->attrs = 0;
}
