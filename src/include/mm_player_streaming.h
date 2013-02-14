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

#ifndef __MM_PLAYER_STREAMING_H__
#define	__MM_PLAYER_STREAMING_H__

#include <glib.h>
#include <gst/gst.h>
#include <string.h>
#include "mm_debug.h"

#define MAX_FILE_BUFFER_NAME_LEN 256

#define MIN_BUFFER_PERCENT 0.0
#define MAX_BUFFER_PERCENT 100.0
#define MIN_BUFFERING_TIME 2.0
#define MAX_BUFFERING_TIME 10.0

#define DEFAULT_BUFFER_SIZE 4194304	 // 4 MBytes
#define DEFAULT_BUFFER_LOW_PERCENT 1.0 	// 1%
#define DEFAULT_BUFFER_HIGH_PERCENT 99.0 	// 15%
#define DEFAULT_BUFFERING_TIME 3.0		// about 3sec

#define DEFAULT_FILE_BUFFER_PATH "/opt/media"

#define STREAMING_USE_FILE_BUFFER
#define STREAMING_USE_MEMORY_BUFFER

typedef struct
{
	GstElement *buffer; /* buffering element of playback pipeline */

	gboolean is_buffering;
	gint buffering_percent;

	gboolean need_update;
	guint buffer_size;
	gdouble buffer_high_percent;
	gdouble buffer_low_percent;
	gdouble buffering_time;
	guint buffer_max_bitrate;
	guint buffer_avg_bitrate;
}mm_player_streaming_t;


mm_player_streaming_t *__mm_player_streaming_create ();
void __mm_player_streaming_initialize (mm_player_streaming_t* streaming_player);
void __mm_player_streaming_deinitialize (mm_player_streaming_t* streaming_player);
void __mm_player_streaming_destroy(mm_player_streaming_t* streaming_player);

void __mm_player_streaming_set_buffer(mm_player_streaming_t* streaming_player, GstElement * buffer,
	gboolean use_buffering, guint buffer_size, gdouble low_percent, gdouble high_percent, gdouble buffering_time,
	gboolean use_file, gchar * file_path, guint64 content_size);
void __mm_player_streaming_set_content_bitrate(mm_player_streaming_t* streaming_player, guint max_bitrate, guint avg_bitrate);
void __mm_player_streaming_buffering (mm_player_streaming_t* streaming_player , GstMessage *buffering_msg);

#endif
