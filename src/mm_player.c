/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, YeJin Cho <cho.yejin@samsung.com>, YoungHwan An <younghwan_.an@samsung.com>
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

#include <gst/gst.h>
#include <string.h>

#include <mm_types.h>
#include <mm_message.h>

#include "mm_player.h"
#include "mm_player_priv.h"
#include "mm_player_attrs.h"
#include "mm_player_utils.h"
#include "mm_player_ini.h"
#include "mm_debug.h"
#include "mm_player_capture.h"

int mm_player_create(MMHandleType *player)
{
	int result = MM_ERROR_NONE;
	mm_player_t* new_player = NULL;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if (!g_thread_supported ())
    	g_thread_init (NULL);

	MMTA_INIT();

	__ta__("mm_player_ini_load",
	result = mm_player_ini_load();
	)
	if(result != MM_ERROR_NONE)
		return result;

	__ta__("mm_player_audio_effect_ini_load",
	result = mm_player_audio_effect_ini_load();
	)
	if(result != MM_ERROR_NONE)
		return result;

	/* alloc player structure */
	new_player = g_malloc(sizeof(mm_player_t));
	if ( ! new_player )
	{
		debug_critical("Cannot allocate memory for player\n");
		goto ERROR;
	}
	memset(new_player, 0, sizeof(mm_player_t));

	/* create player lock */
	new_player->cmd_lock = g_mutex_new();

	if ( ! new_player->cmd_lock )
	{
		debug_critical("failed to create player lock\n");
		goto ERROR;
	}

	/* create msg callback lock */
	new_player->msg_cb_lock = g_mutex_new();

	if ( ! new_player->msg_cb_lock )
	{
		debug_critical("failed to create msg cb lock\n");
		goto ERROR;
	}
	__ta__("[KPI] create media player service",
	result = _mmplayer_create_player((MMHandleType)new_player);
	)

	if(result != MM_ERROR_NONE)
		goto ERROR;

	*player = (MMHandleType)new_player;

	return result;

ERROR:

	if ( new_player )
	{
		if (new_player->cmd_lock)
		{
			g_mutex_free(new_player->cmd_lock);
			new_player->cmd_lock = NULL;
		}

		_mmplayer_destroy( (MMHandleType)new_player );
		MMPLAYER_FREEIF( new_player );
	}

	*player = (MMHandleType)0;
	return MM_ERROR_PLAYER_NO_FREE_SPACE; // are you sure?
}

int  mm_player_destroy(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	__ta__("[KPI] destroy media player service",
	result = _mmplayer_destroy(player);
	)

	MMPLAYER_CMD_UNLOCK( player );

	if (((mm_player_t*)player)->cmd_lock)
	{
		g_mutex_free(((mm_player_t*)player)->cmd_lock);
		((mm_player_t*)player)->cmd_lock = NULL;
	}

	memset( (mm_player_t*)player, 0x00, sizeof(mm_player_t) );

	/* free player */
	g_free( (void*)player );

	MMTA_ACUM_ITEM_SHOW_RESULT_TO(MMTA_SHOW_FILE);

	MMTA_RELEASE();

	return result;
}


int mm_player_realize(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	__ta__("[KPI] initialize media player service",
	result = _mmplayer_realize(player);
	)

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_unrealize(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	__ta__("[KPI] cleanup media player service",
	result = _mmplayer_unrealize(player);
	)

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_set_message_callback(MMHandleType player, MMMessageCallback callback, void *user_param)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_message_callback(player, callback, user_param);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_pd_message_callback(MMHandleType player, MMMessageCallback callback, void *user_param)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	result = _mm_player_set_pd_downloader_message_cb(player, callback, user_param);

	return result;
}

int mm_player_set_audio_stream_callback(MMHandleType player, mm_player_audio_stream_callback callback, void *user_param)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_audiostream_cb(player, callback, user_param);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_set_audio_buffer_callback(MMHandleType player, mm_player_audio_stream_callback callback, void *user_param)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_audiobuffer_cb(player, callback, user_param);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_video_stream_callback(MMHandleType player, mm_player_video_stream_callback callback, void *user_param)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

    result = _mmplayer_set_videostream_cb(player, callback, user_param);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_do_video_capture(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_do_video_capture(player);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_buffer_need_data_callback(MMHandleType player, mm_player_buffer_need_data_callback callback, void * user_param)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

    result = _mmplayer_set_buffer_need_data_cb(player, callback, user_param);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_set_buffer_enough_data_callback(MMHandleType player, mm_player_buffer_enough_data_callback callback, void * user_param)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

    result = _mmplayer_set_buffer_enough_data_cb(player, callback, user_param);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_set_buffer_seek_data_callback(MMHandleType player, mm_player_buffer_seek_data_callback callback, void * user_param)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

    result = _mmplayer_set_buffer_seek_data_cb(player, callback, user_param);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_set_volume(MMHandleType player, MMPlayerVolumeType *volume)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(volume, MM_ERROR_INVALID_ARGUMENT);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_volume(player, *volume);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_get_volume(MMHandleType player, MMPlayerVolumeType *volume)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(volume, MM_ERROR_INVALID_ARGUMENT);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_volume(player, volume);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_set_mute(MMHandleType player, int mute)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_mute(player, mute);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_get_mute(MMHandleType player, int *mute)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(mute, MM_ERROR_INVALID_ARGUMENT);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_mute(player, mute);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_get_state(MMHandleType player, MMPlayerStateType *state)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(state, MM_ERROR_COMMON_INVALID_ARGUMENT);

	*state = MM_PLAYER_STATE_NULL;

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_state(player, (int*)state); /* FIXIT : why int* ? */

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

/* NOTE : Not supported */
int mm_player_change_videosink(MMHandleType player, MMDisplaySurfaceType display_surface_type, void *display_overlay)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	return MM_ERROR_NOT_SUPPORT_API;
}

int mm_player_push_buffer(MMHandleType player, unsigned char *buf, int size)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	//MMPLAYER_CMD_LOCK( player );

	//MMTA_ACUM_ITEM_BEGIN("[KPI] start media player service", false);
	result = _mmplayer_push_buffer(player, buf, size);

	//MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_start(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	MMTA_ACUM_ITEM_BEGIN("[KPI] start media player service", false);
	result = _mmplayer_start(player);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int  mm_player_stop(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	__ta__("[KPI] stop media player service",
	result = _mmplayer_stop(player);
	)

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_pause(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	__ta__("[KPI] pause media player service",
	result = _mmplayer_pause(player);
	)

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_resume(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	__ta__("[KPI] resume media player service",
	result = _mmplayer_resume(player);
	)

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_activate_section_repeat(MMHandleType player, int start_pos, int end_pos)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_activate_section_repeat(player, start_pos, end_pos);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_deactivate_section_repeat(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_deactivate_section_repeat(player);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_set_play_speed(MMHandleType player, float rate)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_playspeed(player, rate);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_set_position(MMHandleType player, MMPlayerPosFormatType format, int pos)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if (format >= MM_PLAYER_POS_FORMAT_NUM)
	{
		debug_error("wrong format\n");
		return MM_ERROR_COMMON_INVALID_ARGUMENT;
	}

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_position(player, format, pos);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_get_position(MMHandleType player, MMPlayerPosFormatType format, int *pos)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(pos, MM_ERROR_COMMON_INVALID_ARGUMENT);

	if (format >= MM_PLAYER_POS_FORMAT_NUM)
	{
		debug_error("wrong format\n");
		return MM_ERROR_COMMON_INVALID_ARGUMENT;
	}

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_position(player, (int)format, (unsigned long*)pos);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_get_buffer_position(MMHandleType player, MMPlayerPosFormatType format, int  *start_pos, int  *stop_pos)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(start_pos && stop_pos, MM_ERROR_COMMON_INVALID_ARGUMENT);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_buffer_position(player, (int)format, (unsigned long*)start_pos, (unsigned long*)stop_pos );

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_adjust_subtitle_position(MMHandleType player, MMPlayerPosFormatType format, int pos)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if (format >= MM_PLAYER_POS_FORMAT_NUM)
	{
		debug_error("wrong format\n");
		return MM_ERROR_COMMON_INVALID_ARGUMENT;
	}

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_adjust_subtitle_postion(player, format, pos);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_set_subtitle_silent(MMHandleType player, int silent)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_subtitle_silent(player, silent);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_get_subtitle_silent(MMHandleType player, int* silent)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_subtitle_silent(player, silent);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}


int mm_player_set_attribute(MMHandleType player,  char **err_attr_name, const char *first_attribute_name, ...)
{
	int result = MM_ERROR_NONE;
	va_list var_args;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(first_attribute_name, MM_ERROR_COMMON_INVALID_ARGUMENT);

	va_start (var_args, first_attribute_name);
	result = _mmplayer_set_attribute(player, err_attr_name, first_attribute_name, var_args);
	va_end (var_args);

	return result;
}


int mm_player_get_attribute(MMHandleType player,  char **err_attr_name, const char *first_attribute_name, ...)
{
	int result = MM_ERROR_NONE;
	va_list var_args;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(first_attribute_name, MM_ERROR_COMMON_INVALID_ARGUMENT);

	va_start (var_args, first_attribute_name);
	result = _mmplayer_get_attribute(player, err_attr_name, first_attribute_name, var_args);
	va_end (var_args);

	return result;
}


int mm_player_get_attribute_info(MMHandleType player,  const char *attribute_name, MMPlayerAttrsInfo *info)
{
	int result = MM_ERROR_NONE;
	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(attribute_name, MM_ERROR_COMMON_INVALID_ARGUMENT);
	return_val_if_fail(info, MM_ERROR_COMMON_INVALID_ARGUMENT);

	result = _mmplayer_get_attributes_info((MMHandleType)player, attribute_name, info);

	return result;
}

int mm_player_get_pd_status(MMHandleType player, guint64 *current_pos, guint64 *total_size)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(current_pos, MM_ERROR_COMMON_INVALID_ARGUMENT);
	return_val_if_fail(total_size, MM_ERROR_COMMON_INVALID_ARGUMENT);

	result = _mmplayer_get_pd_downloader_status(player, current_pos, total_size);

	return result;
}

int mm_player_get_track_count(MMHandleType player,  MMPlayerTrackType track_type, int *count)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(count, MM_ERROR_COMMON_INVALID_ARGUMENT);

	result = _mmplayer_get_track_count(player, track_type, count);

	return result;

}
