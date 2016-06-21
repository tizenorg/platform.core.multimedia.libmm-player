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

#ifndef __MM_PLAYER_STREAMING_H__
#define	__MM_PLAYER_STREAMING_H__

#include <glib.h>
#include <gst/gst.h>
#include <string.h>
#include <math.h>
#include <dlog.h>
#include "mm_player.h"

#define MAX_FILE_BUFFER_NAME_LEN 256

#define MIN_BUFFER_PERCENT 0.0
#define MAX_BUFFER_PERCENT 100.0
#define MIN_BUFFERING_TIME 3.0
#define MAX_BUFFERING_TIME 10.0

#define MAX_DECODEBIN_BUFFER_BYTES	(32 * 1024 * 1024) /* byte */
#define MAX_DECODEBIN_BUFFER_TIME	15                 /* sec */
#define MAX_DECODEBIN_ADAPTIVE_BUFFER_BYTES	(2 * 1024 * 1024) /* byte */
#define MAX_DECODEBIN_ADAPTIVE_BUFFER_TIME	5                 /* sec */

#define DEFAULT_BUFFER_SIZE_BYTES 4194304   /* 4 MBytes */
#define DEFAULT_PLAYING_TIME 10             /* 10 sec   */
#define DEFAULT_ADAPTIVE_PLAYING_TIME 3     /* 3 sec    */

#define DEFAULT_BUFFERING_TIME 3.0          /* 3sec     */
#define DEFAULT_BUFFER_LOW_PERCENT 1.0      /* 1%       */
#define DEFAULT_BUFFER_HIGH_PERCENT 99.0    /* 15%      */
#define DEFAULT_RING_BUFFER_SIZE (20*1024*1024) /* 20MBytes */

#define STREAMING_USE_FILE_BUFFER
#define STREAMING_USE_MEMORY_BUFFER

#define GET_BYTE_FROM_BIT(bit) (bit/8)
#define GET_BIT_FROM_BYTE(byte) (byte*8)
#define CALC_PERCENT(a,b) ((gdouble)(a) * 100 / (gdouble)(b))
#define GET_PERCENT(a, b, c, d) \
do \
{ \
	if (((a) > 0) && ((b) > 0))		\
	{	\
		d = CALC_PERCENT(a, b);	\
	}	\
	else	\
	{	\
		LOGW ("set default per info\n"); 	\
		d = c;	\
	} \
} while ( 0 );


#define PLAYER_BUFFER_CAST(handle) 	((streaming_buffer_t *)(handle))
#define PLAYER_STREAM_CAST(sr) 		((mm_player_streaming_t *)(sr))

#define GET_CURRENT_BUFFERING_BYTE(handle)	(PLAYER_BUFFER_CAST(handle)->buffering_bytes)
#define GET_CURRENT_BUFFERING_TIME(handle)	(PLAYER_BUFFER_CAST(handle)->buffering_time)

#define IS_MUXED_BUFFERING_MODE(sr)		(PLAYER_STREAM_CAST(sr)->streaming_buffer_type == BUFFER_TYPE_MUXED)?(TRUE):(FALSE)
#define IS_DEMUXED_BUFFERING_MODE(sr)	(PLAYER_STREAM_CAST(sr)->streaming_buffer_type == BUFFER_TYPE_DEMUXED)?(TRUE):(FALSE)

#define GET_NEW_BUFFERING_BYTE(size)	((size) < MAX_DECODEBIN_BUFFER_BYTES)?(size):(MAX_DECODEBIN_BUFFER_BYTES)
#define GET_MAX_BUFFER_BYTES(sr)		((PLAYER_STREAM_CAST(sr)->is_adaptive_streaming)?(MAX_DECODEBIN_ADAPTIVE_BUFFER_BYTES):(MAX_DECODEBIN_BUFFER_BYTES))
#define GET_MAX_BUFFER_TIME(sr)			((PLAYER_STREAM_CAST(sr)->is_adaptive_streaming)?(MAX_DECODEBIN_ADAPTIVE_BUFFER_TIME):(MAX_DECODEBIN_BUFFER_TIME))
#define GET_DEFAULT_PLAYING_TIME(sr)	((PLAYER_STREAM_CAST(sr)->is_adaptive_streaming)?(DEFAULT_ADAPTIVE_PLAYING_TIME):(DEFAULT_PLAYING_TIME))

typedef enum {
	BUFFER_TYPE_DEFAULT,
	BUFFER_TYPE_MUXED = BUFFER_TYPE_DEFAULT,	/* queue2 */
	BUFFER_TYPE_DEMUXED, 		/* multi Q in decodebin */
	BUFFER_TYPE_MAX,
} BufferType;

typedef enum {
	MUXED_BUFFER_TYPE_MEM_QUEUE, /* push mode in queue2 */
	MUXED_BUFFER_TYPE_MEM_RING_BUFFER, /* pull mode in queue2 */
	MUXED_BUFFER_TYPE_FILE, /* pull mode in queue2 */
} muxed_buffer_type_e;

typedef struct
{
	MMPlayerBufferingMode mode;
	gboolean is_pre_buffering;
	gint initial_second;
	gint runtime_second;

}streaming_requirement_t;

typedef struct
{
	GstElement* buffer; 		/* buffering element of playback pipeline */

	guint buffering_bytes;
	gdouble buffering_time;		// mq : max buffering time value till now
	gdouble buffer_high_percent;
	gdouble buffer_low_percent;

	gboolean is_live;
}streaming_buffer_t;

typedef struct
{
	gboolean buffering_monitor;
	gint64 	prev_pos;
	gdouble	buffering_time;	// DEFAULT_BUFFERING_TIME
}streaming_default_t;

typedef struct
{
	BufferType	streaming_buffer_type;
	streaming_buffer_t buffer_handle[BUFFER_TYPE_MAX]; 	/* front buffer : queue2 */

	streaming_requirement_t buffering_req;
	streaming_default_t default_val;

	gboolean	is_buffering;
	gboolean	is_buffering_done;	/* get info from bus sync callback */
	gboolean 	is_adaptive_streaming;

	gint		buffering_percent;

	guint		buffer_max_bitrate;
	guint		buffer_avg_bitrate;
	gboolean	need_update;
	gboolean	need_sync;
	gint		ring_buffer_size;
}mm_player_streaming_t;


mm_player_streaming_t *__mm_player_streaming_create (void);
void __mm_player_streaming_initialize (mm_player_streaming_t* streaming_player);
void __mm_player_streaming_deinitialize (mm_player_streaming_t* streaming_player);
void __mm_player_streaming_destroy(mm_player_streaming_t* streaming_player);
void __mm_player_streaming_set_queue2( 	mm_player_streaming_t* streamer,
										GstElement* buffer,
										gboolean use_buffering,
										guint buffering_bytes,
										gdouble buffering_time,
										gdouble low_percent,
										gdouble high_percent,
										muxed_buffer_type_e type,
										gchar* file_path,
										guint64 content_size);
void __mm_player_streaming_set_multiqueue( 	mm_player_streaming_t* streamer,
										GstElement* buffer,
										gboolean use_buffering,
										gdouble buffering_time,
										gdouble low_percent,
										gdouble high_percent);
void __mm_player_streaming_sync_property(mm_player_streaming_t* streamer, GstElement* decodebin);
void __mm_player_streaming_buffering( mm_player_streaming_t* streamer,
									  GstMessage *buffering_msg,
									  guint64 content_size,
									  gint64 position,
									  gint64 duration);
void __mm_player_streaming_set_content_bitrate(mm_player_streaming_t* streaming_player, guint max_bitrate, guint avg_bitrate);

#endif
