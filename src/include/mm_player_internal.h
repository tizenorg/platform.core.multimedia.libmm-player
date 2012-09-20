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

#ifndef __MM_PLAYER_INTERNAL_H__
#define	__MM_PLAYER_INTERNAL_H__

#include <mm_types.h>

#ifdef __cplusplus
	extern "C" {
#endif

/**
    	@addtogroup PLAYER-INTERNAL
	@{

	@par
	<div><table>
	<tr>
	<td>PROPERTY</td>
	<td>TYPE</td>
	<td>VALID TYPE</td>
	<td>DEFAULT VALUE</td>
	</tr>
	<tr>
	<td>"display_roi_x"</td>
	<td>int</td>
	<td>range</td>
	<td>0</td>
	</tr>
	<tr>
	<td>"display_roi_y"</td>
	<td>int</td>
	<td>range</td>
	<td>0</td>
	</tr>
	<tr>
	<td>"display_roi_width"</td>
	<td>int</td>
	<td>range</td>
	<td>640</td>
	</tr>
	<tr>
	<td>"display_roi_height"</td>
	<td>int</td>
	<td>range</td>
	<td>480</td>
	</tr>
	<tr>
	<td>"display_method"</td>
	<td>int</td>
	<td>range</td>
	<td>MM_DISPLAY_METHOD_LETTER_BOX</td>
	</tr>
	<tr>
	<td>"sound_volume_type"</td>
	<td>int</td>
	<td>range</td>
	<td>MM_SOUND_VOLUME_TYPE_CALL</td>
	</tr>
	<tr>
	<td>"sound_route"</td>
	<td>int</td>
	<td>range</td>
	<td>MM_AUDIOROUTE_USE_EXTERNAL_SETTING</td>
	</tr>
	<tr>
	<td>"sound_stop_when_unplugged"</td>
	<td>int</td>
	<td>range</td>
	</tr>
	</table></div>

*/

/*
 * Enumerations of video colorspace
 */
typedef enum {
    MM_PLAYER_COLORSPACE_I420 = 0, 		/**< I420 format - planer */
    MM_PLAYER_COLORSPACE_RGB888,			/**< RGB888 pixel format */
    MM_PLAYER_COLORSPACE_NV12_TILED,		/**< Customized color format in s5pc110 */
}MMPlayerVideoColorspace;

typedef struct
{
	unsigned char *data;					/* capture image buffer */
	int size;								/* capture image size */
	MMPlayerVideoColorspace fmt;			/* color space type */	
} MMPlayerVideoCapture;

/**
 * Buffer need data callback function type.
 *
 * @param	size		[in]	size required for the buffer playback
 * @param	user_param	[in]	User defined parameter which is passed when set
 *								to need data callback
 *
 * @return	This callback function have to return MM_ERROR_NONE.
 */
typedef bool	(*mm_player_buffer_need_data_callback) (unsigned int size, void *user_param);

/**
 * Buffer enough data callback function type.
 *
 * @param	user_param	[in]	User defined parameter which is passed when set
 *								to enough data callback
 *
 * @return	This callback function have to return MM_ERROR_NONE.
 */
typedef bool	(*mm_player_buffer_enough_data_callback) (void *user_param);

/**
 * Buffer seek data callback function type.
 *
 * @param	offset			[in]	offset for the buffer playback
 * @param	user_param		[in]	User defined parameter which is passed when set
 *									to seek data callback
 *
 * @return	This callback function have to return MM_ERROR_NONE.
 */
typedef bool	(*mm_player_buffer_seek_data_callback) (unsigned long long offset, void *user_param);

/**
 * Video stream callback function type.
 *
 * @param	stream		[in]	Reference pointer to video frame data
 * @param	stream_size	[in]	Size of video frame data
 * @param	user_param	[in]	User defined parameter which is passed when set
 *								video stream callback
 * @param	width		[in]	width of video frame
 * @param	height		[in]	height of video frame							
 *
 * @return	This callback function have to return MM_ERROR_NONE.
 */
typedef bool	(*mm_player_video_stream_callback) (void *stream, int stream_size, void *user_param, int width, int height);

/**
 * Audio stream callback function type.
 *
 * @param	stream		[in]	Reference pointer to audio frame data
 * @param	stream_size	[in]	Size of audio frame data
 * @param	user_param	[in]	User defined parameter which is passed when set
 *								audio stream callback
 *
 * @return	This callback function have to return MM_ERROR_NONE.
 */
typedef bool	(*mm_player_video_capture_callback) (void *stream, int stream_size, void *user_param);

/**
 * This function is to set play speed for playback.
 *
 * @param	player		[in]	Handle of player.
 * @param	ratio		[in]	Speed for playback.
 *
 * @return	This function returns zero on success, or negative value with error
 *			code
 * @remark	The current supported range is from -64x to 64x. 
 * 		But, the quailty is dependent on codec performance.
 * 		And, the sound is muted under normal speed and more than double speed.  
 * @see
 * @since
 */
int mm_player_set_play_speed(MMHandleType player, float rate);

/**
 * This function set callback function for receiving video stream from player.
 *
 * @param	player		[in]	Handle of player.
 * @param	callback	[in]	Video stream callback function.
 * @param	user_param	[in]	User parameter.
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 * @remark
 * @see		mm_player_video_stream_callback mm_player_set_audio_stream_callback
 * @since
 */
int mm_player_set_video_stream_callback(MMHandleType player, mm_player_video_stream_callback callback, void *user_param);

/**
 * This function set callback function for receiving audio stream from player.
 *
 * @param       player          [in]    Handle of player.
 * @param       callback                [in]    Audio buffer callback function.
 * @param       user_param      [in]    User parameter.
 *
 * @return      This function returns zero on success, or negative value with error
 *                      code.
 * @remark      It can be used for audio playback only.
 * @see         mm_player_audio_stream_callback
 * @since
 */
int mm_player_set_audio_buffer_callback(MMHandleType player, mm_player_audio_stream_callback callback, void *user_param);

/**
 * This function is to capture video frame. 
 *
 * @param	player		[in]	Handle of player.
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 *
 * @remark	Captured buffer is sent asynchronously through message callback with MM_MESSAGE_VIDEO_CAPTURED. 
 *			And, application should free the captured buffer directly. 
 * @see		MM_MESSAGE_VIDEO_CAPTURED
 * @since
 */
int mm_player_do_video_capture(MMHandleType player);

/**
 * This function set callback function for receiving need data message from player.
 *
 * @param	player		[in]	Handle of player.
 * @param	callback	[in]	Need data callback function.
 * @param	user_param	[in]	User parameter.
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 * @remark
 * @see		mm_player_set_buffer_enough_data_callback
 * @since
 */
int mm_player_set_buffer_need_data_callback(MMHandleType player, mm_player_buffer_need_data_callback callback , void *user_param);

/**
 * This function set callback function for receiving enough data message from player.
 *
 * @param	player		[in]	Handle of player.
 * @param	callback	[in]	Enough data callback function.
 * @param	user_param	[in]	User parameter.
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 * @remark
 * @see		mm_player_set_buffer_need_data_callback
 * @since
 */
int mm_player_set_buffer_enough_data_callback(MMHandleType player, mm_player_buffer_enough_data_callback callback, void *user_param);

/**
 * This function set callback function for receiving seek data message from player.
 *
 * @param	player		[in]	Handle of player.
 * @param	callback	[in]	Seek data callback function.
 * @param	user_param	[in]	User parameter.
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 * @remark
 * @see
 * @since
 */
int mm_player_set_buffer_seek_data_callback(MMHandleType player, mm_player_buffer_seek_data_callback callback, void *user_param);

/**
 * This function set callback function for putting data into player.
 *
 * @param	player		[in]	Handle of player.
 * @param	buf			[in]	data to push into player
 * @param	size			[in]	buffer size to push
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 * @remark
 * @see
 * @since
 */
int mm_player_push_buffer(MMHandleType player, unsigned char *buf, int size);


/**
	@}
 */

#ifdef __cplusplus
	}
#endif

#endif	/* __MM_PLAYER_INTERNAL_H__ */
