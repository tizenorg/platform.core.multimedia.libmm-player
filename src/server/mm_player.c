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
#include "mm_player_tracks.h"
#include "mm_player_es.h"

int mm_player_create(MMHandleType *player)
{
	int result = MM_ERROR_NONE;
	mm_player_t* new_player = NULL;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);


	/* alloc player structure */
	new_player = g_malloc(sizeof(mm_player_t));
	if ( ! new_player )
	{
		debug_error("Cannot allocate memory for player\n");
		goto ERROR;
	}
	memset(new_player, 0, sizeof(mm_player_t));

	/* create player lock */
	g_mutex_init(&new_player->cmd_lock);

	/* create player lock */
	g_mutex_init(&new_player->playback_lock);


	/* create msg callback lock */
	g_mutex_init(&new_player->msg_cb_lock);

	/* load ini files */
	result = mm_player_ini_load(&new_player->ini);
	if(result != MM_ERROR_NONE)
	{
		debug_error("can't load ini");
		goto ERROR;
	}

	result = mm_player_audio_effect_ini_load(&new_player->ini);
	if(result != MM_ERROR_NONE)
	{
		debug_error("can't load audio ini");
		goto ERROR;
	}


	/* create player */
	result = _mmplayer_create_player((MMHandleType)new_player);

	if(result != MM_ERROR_NONE)
	{
		debug_error("failed to create player");
		goto ERROR;
	}

	*player = (MMHandleType)new_player;

	return result;

ERROR:

	if ( new_player )
	{
		_mmplayer_destroy( (MMHandleType)new_player );
		g_mutex_clear(&new_player->cmd_lock);
		g_mutex_clear(&new_player->playback_lock);

		MMPLAYER_FREEIF( new_player );
	}

	*player = (MMHandleType)0;
	return MM_ERROR_PLAYER_NO_FREE_SPACE; // are you sure?
}

int  mm_player_destroy(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_destroy(player);

	MMPLAYER_CMD_UNLOCK( player );

	g_mutex_clear(&((mm_player_t*)player)->cmd_lock);
	g_mutex_clear(&((mm_player_t*)player)->playback_lock);

	memset( (mm_player_t*)player, 0x00, sizeof(mm_player_t) );

	/* free player */
	g_free( (void*)player );

	return result;
}

int mm_player_sound_register(MMHandleType player, int pid)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_sound_register_with_pid(player, pid);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_realize(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_realize(player);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_unrealize(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_unrealize(player);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_message_callback(MMHandleType player, MMMessageCallback callback, void *user_param)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_message_callback(player, callback, user_param);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_pd_message_callback(MMHandleType player, MMMessageCallback callback, void *user_param)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	result = _mm_player_set_pd_downloader_message_cb(player, callback, user_param);

	return result;
}

int mm_player_set_audio_stream_callback(MMHandleType player, mm_player_audio_stream_callback callback, void *user_param)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_audiostream_cb(player, callback, user_param);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_audio_stream_callback_ex(MMHandleType player, bool sync, mm_player_audio_stream_callback_ex callback, void *user_param)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_audiostream_cb_ex(player, sync, callback, user_param);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_video_stream_callback(MMHandleType player, mm_player_video_stream_callback callback, void *user_param)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_videostream_cb(player, callback, user_param);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_video_frame_render_error_callback(MMHandleType player, mm_player_video_frame_render_error_callback callback, void *user_param)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_videoframe_render_error_cb(player, callback, user_param);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_do_video_capture(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_do_video_capture(player);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_prepare_buffering_time(MMHandleType player, int second)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_prepare_buffering_time(player, second);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_runtime_buffering_mode(MMHandleType player, MMPlayerBufferingMode mode, int second)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_runtime_buffering_mode(player, mode, second);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_volume(MMHandleType player, MMPlayerVolumeType *volume)
{
	int result = MM_ERROR_NONE;

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

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_mute(player, mute);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_get_mute(MMHandleType player, int *mute)
{
	int result = MM_ERROR_NONE;

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

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(state, MM_ERROR_COMMON_INVALID_ARGUMENT);

	*state = MM_PLAYER_STATE_NULL;

	result = _mmplayer_get_state(player, (int*)state);

	return result;
}

/* NOTE : It does not support some use cases, eg using colorspace converter */
int mm_player_change_videosink(MMHandleType player, MMDisplaySurfaceType display_surface_type, void *display_overlay)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_change_videosink(player, display_surface_type, display_overlay);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_push_buffer(MMHandleType player, unsigned char *buf, int size)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	//MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_push_buffer(player, buf, size);

	//MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_start(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_start(player);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int  mm_player_stop(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_stop(player);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_pause(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_pause(player);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_resume(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_resume(player);

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

int mm_player_gst_set_audio_channel(MMHandleType player, MMPlayerAudioChannel ch)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_gst_set_audio_channel(player, ch);

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

int mm_player_get_position(MMHandleType player, MMPlayerPosFormatType format, unsigned long *pos)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(pos, MM_ERROR_COMMON_INVALID_ARGUMENT);

	if (format >= MM_PLAYER_POS_FORMAT_NUM)
	{
		debug_error("wrong format\n");
		return MM_ERROR_COMMON_INVALID_ARGUMENT;
	}

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_position(player, (int)format, pos);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_get_buffer_position(MMHandleType player, MMPlayerPosFormatType format, unsigned long *start_pos, unsigned long *stop_pos)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(start_pos && stop_pos, MM_ERROR_COMMON_INVALID_ARGUMENT);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_buffer_position(player, (int)format, start_pos, stop_pos );

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_external_subtitle_path(MMHandleType player, const char* path)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_external_subtitle_path(player, path);

	MMPLAYER_CMD_UNLOCK( player );
	return result;
}

int mm_player_adjust_subtitle_position(MMHandleType player, MMPlayerPosFormatType format, int pos)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if (format >= MM_PLAYER_POS_FORMAT_NUM)
	{
		debug_error("wrong format(%d) \n", format);
		return MM_ERROR_INVALID_ARGUMENT;
	}

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_adjust_subtitle_postion(player, format, pos);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_adjust_video_position(MMHandleType player, int offset)
{
	int result = MM_ERROR_NONE;
	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_adjust_video_postion(player, offset);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_subtitle_silent(MMHandleType player, int silent)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_subtitle_silent(player, silent);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_get_subtitle_silent(MMHandleType player, int* silent)
{
	int result = MM_ERROR_NONE;

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


	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(attribute_name, MM_ERROR_COMMON_INVALID_ARGUMENT);
	return_val_if_fail(info, MM_ERROR_COMMON_INVALID_ARGUMENT);

	result = _mmplayer_get_attributes_info((MMHandleType)player, attribute_name, info);

	return result;
}

int mm_player_get_pd_status(MMHandleType player, guint64 *current_pos, guint64 *total_size)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(current_pos, MM_ERROR_COMMON_INVALID_ARGUMENT);
	return_val_if_fail(total_size, MM_ERROR_COMMON_INVALID_ARGUMENT);

	result = _mmplayer_get_pd_downloader_status(player, current_pos, total_size);

	return result;
}

int mm_player_get_track_count(MMHandleType player, MMPlayerTrackType type, int *count)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(count, MM_ERROR_COMMON_INVALID_ARGUMENT);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_track_count(player, type, count);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_select_track(MMHandleType player, MMPlayerTrackType type, int index)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_select_track(player, type, index);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}
#ifdef _MULTI_TRACK
int mm_player_track_add_subtitle_language(MMHandleType player, int index)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_track_add_subtitle_language(player, index);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_track_remove_subtitle_language(MMHandleType player, int index)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_track_remove_subtitle_language(player, index);

	MMPLAYER_CMD_UNLOCK( player );

	return result;

}
#endif
int mm_player_get_current_track(MMHandleType player, MMPlayerTrackType type, int *index)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(index, MM_ERROR_COMMON_INVALID_ARGUMENT);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_current_track(player, type, index);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_get_track_language_code(MMHandleType player,  MMPlayerTrackType type, int index, char **code)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_track_language_code(player, type, index, code);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_display_zoom(MMHandleType player, float level, int x, int y)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_display_zoom(player, level, x, y);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_get_display_zoom(MMHandleType player, float *level, int *x, int *y)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(level, MM_ERROR_COMMON_INVALID_ARGUMENT);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_display_zoom(player, level, x, y);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_video_share_master_clock(MMHandleType player,
						long long clock,
						long long clock_delta,
						long long video_time,
						long long media_clock,
						long long audio_time)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_video_share_master_clock(player, clock, clock_delta, video_time, media_clock, audio_time);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_get_video_share_master_clock(MMHandleType player,
						long long *video_time,
						long long *media_clock,
						long long *audio_time)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(video_time, MM_ERROR_COMMON_INVALID_ARGUMENT);
	return_val_if_fail(media_clock, MM_ERROR_COMMON_INVALID_ARGUMENT);
	return_val_if_fail(audio_time, MM_ERROR_COMMON_INVALID_ARGUMENT);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_video_share_master_clock(player, video_time, media_clock, audio_time);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_get_video_rotate_angle(MMHandleType player, int *angle)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(angle, MM_ERROR_COMMON_INVALID_ARGUMENT);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_video_rotate_angle(player, angle);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_video_hub_download_mode(MMHandleType player, bool mode)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_video_hub_download_mode(player, mode);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_enable_sync_handler(MMHandleType player, bool enable)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_enable_sync_handler(player, enable);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_uri(MMHandleType player, const char *uri)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_uri(player, uri);

	MMPLAYER_CMD_UNLOCK( player );

	return result;

}

int mm_player_set_next_uri(MMHandleType player, const char *uri)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_next_uri(player, uri, FALSE);

	MMPLAYER_CMD_UNLOCK( player );

	return result;

}

int mm_player_get_next_uri(MMHandleType player, char **uri)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_next_uri(player, uri);

	MMPLAYER_CMD_UNLOCK( player );

	return result;

}
#ifdef _MULTI_TRACK
int mm_player_track_foreach_selected_subtitle_language(MMHandleType player, mm_player_track_selected_subtitle_language_callback callback, void *user_param)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_track_foreach_selected_subtitle_language(player, callback, user_param);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}
#endif

int mm_player_has_closed_caption(MMHandleType player, bool *exist)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(exist, MM_ERROR_INVALID_ARGUMENT);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_has_closed_caption(player, exist);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_enable_media_packet_video_stream(MMHandleType player, bool enable)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(enable, MM_ERROR_INVALID_ARGUMENT);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_enable_media_packet_video_stream(player, enable);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

void * mm_player_media_packet_video_stream_internal_buffer_ref(void *buffer)
{
	void * result;
	result = _mm_player_media_packet_video_stream_internal_buffer_ref(buffer);

	return result;
}

void mm_player_media_packet_video_stream_internal_buffer_unref(void *buffer)
{
	_mm_player_media_packet_video_stream_internal_buffer_unref(buffer);
}

int mm_player_submit_packet(MMHandleType player, media_packet_h packet)
{

	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	/* no lock here, otherwise callback for the "need-data" signal of appsrc will be blocking */
	//MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_submit_packet(player, packet);

	//MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_video_info (MMHandleType player, media_format_h format)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_video_info(player, format);

	MMPLAYER_CMD_UNLOCK( player );

	return result;

}

int mm_player_set_audio_info (MMHandleType player, media_format_h format)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_audio_info(player, format);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_subtitle_info (MMHandleType player, MMPlayerSubtitleStreamInfo *subtitle_stream_info)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_subtitle_info(player, subtitle_stream_info);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_media_stream_buffer_max_size(MMHandleType player, MMPlayerStreamType type, unsigned long long max_size)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_media_stream_max_size(player, type, max_size);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_get_media_stream_buffer_max_size(MMHandleType player, MMPlayerStreamType type, unsigned long long *max_size)
{
	int result = MM_ERROR_NONE;
	guint64 _max_size = 0;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(max_size, MM_ERROR_INVALID_ARGUMENT);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_media_stream_max_size(player, type, &_max_size);
	*max_size = _max_size;

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_media_stream_buffer_min_percent(MMHandleType player, MMPlayerStreamType type, unsigned min_percent)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_media_stream_min_percent(player, type, min_percent);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_get_media_stream_buffer_min_percent(MMHandleType player, MMPlayerStreamType type, unsigned int *min_percent)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(min_percent, MM_ERROR_INVALID_ARGUMENT);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_media_stream_min_percent(player, type, min_percent);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_media_stream_buffer_status_callback(MMHandleType player, MMPlayerStreamType type, mm_player_media_stream_buffer_status_callback callback, void * user_param)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_media_stream_buffer_status_cb(player, type, callback, user_param);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_media_stream_seek_data_callback(MMHandleType player, MMPlayerStreamType type, mm_player_media_stream_seek_data_callback callback, void * user_param)
{
	int result = MM_ERROR_NONE;

	debug_log("\n");

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_media_stream_seek_data_cb(player, type, callback, user_param);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_audio_stream_changed_callback(MMHandleType player, mm_player_stream_changed_callback callback, void *user_param)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_audiostream_changed_cb(player, callback, user_param);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_video_stream_changed_callback(MMHandleType player, mm_player_stream_changed_callback callback, void *user_param)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_videostream_changed_cb(player, callback, user_param);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_pcm_spec(MMHandleType player, int samplerate, int channel)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_pcm_spec(player, samplerate, channel);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_set_shm_stream_path(MMHandleType player, const char *path)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(path, MM_ERROR_INVALID_ARGUMENT);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_shm_stream_path(player, path);

	MMPLAYER_CMD_UNLOCK( player );

	return result;

}

int mm_player_get_raw_video_caps(MMHandleType player, char **caps)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(caps, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_raw_video_caps(player, caps);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}
