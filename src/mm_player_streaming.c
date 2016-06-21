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

#include <sys/vfs.h>
#include <dlog.h>
#include "mm_player_utils.h"
#include "mm_player_streaming.h"

#define TO_THE_END 0

typedef struct{
	gint byte_in_rate;		// byte
	gint byte_out_rate;		// byte
	gdouble time_rate;		// second
	guint buffer_criteria;	// byte
}streaming_bitrate_info_t;

typedef struct{
	gint64 position;	// ns
	gint64 duration;	// ns
	guint64 content_size;	// bytes
}streaming_content_info_t;

typedef struct{
	guint buffering_bytes;		// bytes
	gdouble buffering_time;		// second
	gdouble percent_byte;
	gdouble percent_time;
}streaming_buffer_info_t;

static void streaming_check_buffer_percent(gdouble in_low, gdouble in_high, gdouble *out_low, gdouble *out_high);
static void streaming_set_buffer_percent(mm_player_streaming_t* streamer, BufferType type, gdouble low_percent, gdouble high_percent_byte, gdouble high_percent_time);
static void streaming_set_queue2_queue_type (mm_player_streaming_t* streamer, muxed_buffer_type_e type, gchar * file_path, guint64 content_size);
static void streaming_set_buffer_size(mm_player_streaming_t* streamer, BufferType type, guint buffering_bytes, gdouble buffering_time);
static void streaming_update_buffering_status(mm_player_streaming_t* streamer, GstMessage *buffering_msg, gint64 position);
static void streaming_get_current_bitrate_info(	mm_player_streaming_t* streamer,
												GstMessage *buffering_msg,
												streaming_content_info_t content_info,
												streaming_bitrate_info_t* bitrate_info);
static void
streaming_handle_fixed_buffering_mode(	mm_player_streaming_t* streamer,
										gint byte_out_rate,
										gdouble fixed_buffering_time,
										streaming_buffer_info_t* buffer_info);
static void
streaming_handle_adaptive_buffering_mode(	mm_player_streaming_t* streamer,
											streaming_content_info_t content_info,
											streaming_bitrate_info_t bitrate_info,
											streaming_buffer_info_t* buffer_info,
											gint expected_play_time);
static void
streaming_update_buffer_setting	(	mm_player_streaming_t* streamer,
									GstMessage *buffering_msg,
									guint64 content_size,
									gint64 position,
									gint64 duration);

mm_player_streaming_t *
__mm_player_streaming_create (void)
{
	mm_player_streaming_t *streamer = NULL;

	MMPLAYER_FENTER();

	streamer = (mm_player_streaming_t *) malloc (sizeof (mm_player_streaming_t));
	if (!streamer)
	{
		LOGE ("fail to create streaming player handle..\n");
		return NULL;
	}

	memset(streamer, 0, sizeof(mm_player_streaming_t));

	MMPLAYER_FLEAVE();

	return streamer;
}

static void
streaming_buffer_initialize (streaming_buffer_t* buffer_handle, gboolean buffer_init)
{
	if (buffer_init)
		buffer_handle->buffer = NULL;

	buffer_handle->buffering_bytes = DEFAULT_BUFFER_SIZE_BYTES;
	buffer_handle->buffering_time = DEFAULT_BUFFERING_TIME;
	buffer_handle->buffer_low_percent = DEFAULT_BUFFER_LOW_PERCENT;
	buffer_handle->buffer_high_percent = DEFAULT_BUFFER_HIGH_PERCENT;
	buffer_handle->is_live = FALSE;

}

void __mm_player_streaming_initialize (mm_player_streaming_t* streamer)
{
	MMPLAYER_FENTER();

	streamer->streaming_buffer_type = BUFFER_TYPE_DEFAULT;	// multi-queue

	streaming_buffer_initialize(&(streamer->buffer_handle[BUFFER_TYPE_MUXED]), TRUE);
	streaming_buffer_initialize(&(streamer->buffer_handle[BUFFER_TYPE_DEMUXED]), TRUE);

	streamer->buffering_req.mode = MM_PLAYER_BUFFERING_MODE_ADAPTIVE;
	streamer->buffering_req.is_pre_buffering = FALSE;
	streamer->buffering_req.initial_second = 0;
	streamer->buffering_req.runtime_second = 0;

	streamer->default_val.buffering_monitor = FALSE;
	streamer->default_val.buffering_time = DEFAULT_BUFFERING_TIME;

	streamer->buffer_avg_bitrate = 0;
	streamer->buffer_max_bitrate = 0;
	streamer->need_update = FALSE;
	streamer->need_sync = FALSE;

	streamer->is_buffering = FALSE;
	streamer->is_buffering_done = FALSE;
	streamer->is_adaptive_streaming = FALSE;
	streamer->buffering_percent = -1;

	streamer->ring_buffer_size = DEFAULT_RING_BUFFER_SIZE;
	MMPLAYER_FLEAVE();
	return;
}

void __mm_player_streaming_deinitialize (mm_player_streaming_t* streamer)
{
	MMPLAYER_FENTER();
	MMPLAYER_RETURN_IF_FAIL(streamer);

	streamer->streaming_buffer_type = BUFFER_TYPE_DEFAULT;	// multi-queue

	streaming_buffer_initialize(&(streamer->buffer_handle[BUFFER_TYPE_MUXED]), FALSE);
	streaming_buffer_initialize(&(streamer->buffer_handle[BUFFER_TYPE_DEMUXED]), FALSE);

	streamer->buffering_req.mode = MM_PLAYER_BUFFERING_MODE_ADAPTIVE;
	streamer->buffering_req.is_pre_buffering = FALSE;
	streamer->buffering_req.initial_second = 0;
	streamer->buffering_req.runtime_second = 0;

	streamer->default_val.buffering_monitor = FALSE;
	streamer->default_val.buffering_time = DEFAULT_BUFFERING_TIME;

	streamer->buffer_avg_bitrate = 0;
	streamer->buffer_max_bitrate = 0;
	streamer->need_update = FALSE;
	streamer->need_sync = FALSE;

	streamer->is_buffering = FALSE;
	streamer->is_buffering_done = FALSE;
	streamer->is_adaptive_streaming = FALSE;

	streamer->buffering_percent = -1;
	streamer->ring_buffer_size = DEFAULT_RING_BUFFER_SIZE;

	MMPLAYER_FLEAVE();
	return;
}

void __mm_player_streaming_destroy (mm_player_streaming_t* streamer)
{
	MMPLAYER_FENTER();

	if(streamer)
	{
		g_free (streamer);
		streamer = NULL;
	}

	MMPLAYER_FLEAVE();

	return;
}

void __mm_player_streaming_set_content_bitrate(mm_player_streaming_t* streamer, guint max_bitrate, guint avg_bitrate)
{
	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL(streamer);

	/* Note : Update buffering criterion bytes
	*      1. maximum bitrate is considered first.
	*      2. average bitrage * 3 is next.
	*      3. if there are no updated bitrate, use default buffering limit.
	*/
	if (max_bitrate > 0 && streamer->buffer_max_bitrate != max_bitrate)
	{
		LOGD("set maximum bitrate(%dbps).\n", max_bitrate);
		streamer->buffer_max_bitrate = max_bitrate;
		if (streamer->buffering_req.is_pre_buffering == FALSE)
		{
			streamer->need_update = TRUE;
		}
		else
		{
			LOGD("pre-buffering...\n");

			if (IS_MUXED_BUFFERING_MODE(streamer))
				streaming_update_buffer_setting(streamer, NULL, 0, 0, 0);
		}
	}

	if (avg_bitrate > 0 && streamer->buffer_avg_bitrate != avg_bitrate)
	{
		LOGD("set averate bitrate(%dbps).\n", avg_bitrate);
		streamer->buffer_avg_bitrate = avg_bitrate;

		if (streamer->buffering_req.is_pre_buffering == FALSE)
		{
			streamer->need_update = TRUE;
		}
		else
		{
			LOGD("pre-buffering...\n");

			if (IS_MUXED_BUFFERING_MODE(streamer))
				streaming_update_buffer_setting(streamer, NULL, 0, 0, 0);
		}
	}

	MMPLAYER_FLEAVE();
	return;
}

static void
streaming_check_buffer_percent(gdouble in_low, gdouble in_high, gdouble *out_low, gdouble *out_high)
{
	gdouble buffer_low_percent = DEFAULT_BUFFER_LOW_PERCENT;
	gdouble buffer_high_percent = DEFAULT_BUFFER_HIGH_PERCENT;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL(out_low && out_high);

	if (in_low <= MIN_BUFFER_PERCENT || in_low >= MAX_BUFFER_PERCENT)
	{
		LOGW("buffer low percent is out of range. use defaut value.");
	}
	else
	{
		buffer_low_percent = in_low;
	}

	if (in_high  <=  MIN_BUFFER_PERCENT || in_high  >=  MAX_BUFFER_PERCENT)
	{
		LOGW("buffer high percent is out of range. use defaut value.");
	}
	else
	{
		buffer_high_percent = in_high;
	}

	if (buffer_high_percent <= buffer_low_percent)
		buffer_high_percent =  buffer_low_percent + 1.0;

	LOGD("set buffer percent to %2.3f ~ %2.3f.",  buffer_low_percent, buffer_high_percent);

	*out_low = buffer_low_percent;
	*out_high = buffer_high_percent;
}

static void
streaming_set_buffer_percent(	mm_player_streaming_t* streamer,
								BufferType type,
								gdouble low_percent,
								gdouble high_percent_byte,
								gdouble high_percent_time)
{
	gdouble confirmed_low = DEFAULT_BUFFER_LOW_PERCENT;
	gdouble confirmed_high = DEFAULT_BUFFER_HIGH_PERCENT;
	gdouble high_percent = 0.0;

	streaming_buffer_t* buffer_handle = NULL;
	gchar* factory_name = NULL;

	MMPLAYER_FENTER();
	MMPLAYER_RETURN_IF_FAIL(streamer);
	MMPLAYER_RETURN_IF_FAIL(type < BUFFER_TYPE_MAX);

	buffer_handle = &(streamer->buffer_handle[type]);
	if (!(buffer_handle && buffer_handle->buffer))
	{
		LOGE("buffer_handle->buffer is NULL!");
		return;
	}

	factory_name = GST_OBJECT_NAME(gst_element_get_factory(buffer_handle->buffer));

	if (!factory_name)
	{
		LOGE("Fail to get factory name!");
		return;
	}

	if (type == BUFFER_TYPE_MUXED)
		high_percent = high_percent_byte;
	else
		high_percent = MAX(high_percent_time, high_percent_byte);

	streaming_check_buffer_percent(low_percent, high_percent, &confirmed_low, &confirmed_high);

	/* if use-buffering is disabled, this settings do not have any meaning. */
	LOGD("target buffer elem : %s (%2.3f ~ %2.3f)",
		GST_ELEMENT_NAME(buffer_handle->buffer), confirmed_low, confirmed_high);

	if ((confirmed_low == DEFAULT_BUFFER_LOW_PERCENT) ||
		(buffer_handle->buffer_low_percent != confirmed_low))
	{
		g_object_set (G_OBJECT(buffer_handle->buffer), "low-percent", (gint)confirmed_low, NULL);
	}

	if ((confirmed_high == DEFAULT_BUFFER_HIGH_PERCENT) ||
		(buffer_handle->buffer_high_percent != confirmed_high))
	{
		g_object_set (G_OBJECT(buffer_handle->buffer), "high-percent", (gint)confirmed_high, NULL);
	}

	buffer_handle->buffer_low_percent = confirmed_low;
	buffer_handle->buffer_high_percent = confirmed_high;

	MMPLAYER_FLEAVE();
	return;
}

static void
streaming_set_queue2_queue_type (mm_player_streaming_t* streamer, muxed_buffer_type_e type, gchar * file_path, guint64 content_size)
{
	streaming_buffer_t* buffer_handle = NULL;
	guint64 storage_available_size = 0L; //bytes
	guint64 buffer_size = 0L;  //bytes
	gchar file_buffer_name[MM_MAX_URL_LEN] = {0};
	struct statfs buf = {0};
	gchar* factory_name = NULL;

	MMPLAYER_FENTER();
	MMPLAYER_RETURN_IF_FAIL(streamer);
	MMPLAYER_RETURN_IF_FAIL(streamer->buffer_handle[BUFFER_TYPE_MUXED].buffer);

	buffer_handle = &(streamer->buffer_handle[BUFFER_TYPE_MUXED]);

	if (!(buffer_handle && buffer_handle->buffer))
	{
		LOGE("buffer_handle->buffer is NULL!");
		return;
	}

	factory_name = GST_OBJECT_NAME(gst_element_get_factory(buffer_handle->buffer));

	if (!factory_name)
	{
		LOGE("Fail to get factory name!");
		return;
	}

	LOGD("target buffer elem : %s", GST_ELEMENT_NAME(buffer_handle->buffer));

	if (!g_strrstr(factory_name, "queue2"))
	{
		LOGD("only queue2 can use file buffer. not decodebin2 or multiQ\n");
		return;
	}

	if ((type == MUXED_BUFFER_TYPE_MEM_QUEUE) || (!g_strrstr(factory_name, "queue2")))
	{
		LOGD("use memory queue for buffering. streaming is played on push-based. \n"
					"buffering position would not be updated.\n"
					"buffered data would be flushed after played.\n"
					"seeking and getting duration could be failed due to file format.");
		return;
	}

	LOGD("[Queue2] buffering type : %d. streaming is played on pull-based. \n", type);
	if (type == MUXED_BUFFER_TYPE_FILE && file_path && strlen(file_path)>0) {
		if (statfs((const char *)file_path, &buf) < 0)
		{
			LOGW ("[Queue2] fail to get available storage capacity. set mem ring buffer instead of file buffer.\n");
			buffer_size = (guint64)((streamer->ring_buffer_size>0)?(streamer->ring_buffer_size):DEFAULT_RING_BUFFER_SIZE);
		}
		else
		{
			storage_available_size = (guint64)buf.f_bavail * (guint64)buf.f_bsize; //bytes

			LOGD ("[Queue2] the number of available blocks : %"G_GUINT64_FORMAT
						", the block size is %"G_GUINT64_FORMAT".\n",
						(guint64)buf.f_bavail, (guint64)buf.f_bsize);

			LOGD ("[Queue2] calculated available storage size is %"
								G_GUINT64_FORMAT" Bytes.\n", storage_available_size);

			if (content_size <= 0 || content_size >= storage_available_size)
				buffer_size = storage_available_size;
			else
				buffer_size = 0L;

			g_snprintf(file_buffer_name, MM_MAX_URL_LEN, "%sXXXXXX", file_path);
			SECURE_LOGD("[Queue2] the buffering file name is %s.\n", file_buffer_name);

			g_object_set (G_OBJECT(buffer_handle->buffer), "temp-template", file_buffer_name, NULL);
		}
	} else {
		buffer_size = (guint64)((streamer->ring_buffer_size>0)?(streamer->ring_buffer_size):DEFAULT_RING_BUFFER_SIZE);
	}

	LOGW ("[Queue2] set ring buffer size: %lld\n", buffer_size);
	g_object_set (G_OBJECT(buffer_handle->buffer), "ring-buffer-max-size", buffer_size, NULL);

	MMPLAYER_FLEAVE();
	return;
}

static void
streaming_set_buffer_size(mm_player_streaming_t* streamer, BufferType type, guint buffering_bytes, gdouble buffering_time)
{
	streaming_buffer_t* buffer_handle = NULL;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL(streamer);
	MMPLAYER_RETURN_IF_FAIL(buffering_bytes > 0);
	MMPLAYER_RETURN_IF_FAIL(type < BUFFER_TYPE_MAX);

	buffer_handle = &(streamer->buffer_handle[type]);

	if (buffer_handle && buffer_handle->buffer)
	{
		if (g_strrstr(GST_ELEMENT_NAME(buffer_handle->buffer), "multiqueue"))
		{
			if (buffering_time <= 0)
				buffering_time = GET_CURRENT_BUFFERING_TIME(buffer_handle);

			g_object_set (G_OBJECT(buffer_handle->buffer),
							"max-size-bytes", GET_MAX_BUFFER_BYTES(streamer), /* mq size is fixed, control it with high/low percent value*/
							"max-size-time", ((guint)ceil(buffering_time) * GST_SECOND),
							"max-size-buffers", 0, NULL);  					  /* disable */

			buffer_handle->buffering_time = buffering_time;
			buffer_handle->buffering_bytes = GET_MAX_BUFFER_BYTES(streamer);

			LOGD("max-size-time : %f", buffering_time);
		}
		else	/* queue2 */
		{
			if (buffer_handle->is_live)
			{
				g_object_set (G_OBJECT(buffer_handle->buffer),
								"max-size-bytes", buffering_bytes,
								"max-size-time", (guint64)(buffering_time*GST_SECOND),
								"max-size-buffers", 0,
								"use-rate-estimate", TRUE, NULL);
			}
			else
			{
				g_object_set (G_OBJECT(buffer_handle->buffer),
								"max-size-bytes", buffering_bytes,
								"max-size-time", (guint64)0,
								"max-size-buffers", 0,
								"use-rate-estimate", FALSE, NULL);
			}

			buffer_handle->buffering_bytes = buffering_bytes;
			buffer_handle->buffering_time = buffering_time;

			LOGD("max-size-bytes : %d", buffering_bytes);
		}
	}

	MMPLAYER_FLEAVE();
	return;
}

void __mm_player_streaming_set_queue2( 	mm_player_streaming_t* streamer,
										GstElement* buffer,
										gboolean use_buffering,
										guint buffering_bytes,
										gdouble buffering_time,
										gdouble low_percent,
										gdouble high_percent,
										muxed_buffer_type_e type,
										gchar* file_path,
										guint64 content_size)
{
	MMPLAYER_FENTER();
	MMPLAYER_RETURN_IF_FAIL(streamer);

	if (buffer)
	{
		LOGD("USE-BUFFERING : %s", (use_buffering)?"OOO":"XXX");

		streamer->buffer_handle[BUFFER_TYPE_MUXED].buffer = buffer;

		if (use_buffering)
		{
			streamer->streaming_buffer_type = BUFFER_TYPE_MUXED;

			if (content_size > 0)
			{
				if (streamer->buffering_req.initial_second > 0)
				 	streamer->buffering_req.is_pre_buffering = TRUE;
				else
					streamer->buffering_req.initial_second = (gint)ceil(buffering_time);
			}
			else
			{
				LOGD("live streaming without mq");

				streamer->buffer_handle[BUFFER_TYPE_MUXED].is_live = TRUE;
				streamer->buffering_req.initial_second = buffering_time = DEFAULT_BUFFERING_TIME;
			}
		}

		g_object_set ( G_OBJECT (streamer->buffer_handle[BUFFER_TYPE_MUXED].buffer), "use-buffering", use_buffering, NULL );
	}

	streaming_set_buffer_size		(streamer, BUFFER_TYPE_MUXED, buffering_bytes, buffering_time);
	streaming_set_buffer_percent	(streamer, BUFFER_TYPE_MUXED, low_percent, high_percent, 0);
	streaming_set_queue2_queue_type (streamer, type, file_path, content_size);

	MMPLAYER_FLEAVE();
	return;
}

void __mm_player_streaming_sync_property(mm_player_streaming_t* streamer, GstElement* decodebin)
{
	streaming_buffer_t* buffer_handle = NULL;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL ( streamer && decodebin );

	buffer_handle = &(streamer->buffer_handle[BUFFER_TYPE_DEMUXED]);

	if ((streamer->need_sync) && (streamer->streaming_buffer_type == BUFFER_TYPE_DEMUXED))
	{
		g_object_set (G_OBJECT(decodebin),
					"max-size-bytes", buffer_handle->buffering_bytes,
					"max-size-time", (guint64)(ceil(buffer_handle->buffering_time) * GST_SECOND),
					"low-percent", (gint)buffer_handle->buffer_low_percent,
					"high-percent", (gint)buffer_handle->buffer_high_percent, NULL);

	}

	streamer->need_sync = FALSE;
}

void __mm_player_streaming_set_multiqueue( 	mm_player_streaming_t* streamer,
										GstElement* buffer,
										gboolean use_buffering,
										gdouble buffering_time,
										gdouble low_percent,
										gdouble high_percent)
{
	streaming_buffer_t* buffer_handle = NULL;
	gdouble pre_buffering_time = 0.0;

	MMPLAYER_FENTER();
	MMPLAYER_RETURN_IF_FAIL(streamer);

	buffer_handle = &(streamer->buffer_handle[BUFFER_TYPE_DEMUXED]);
	pre_buffering_time = (gdouble)streamer->buffering_req.initial_second;

	if (buffer)
	{
		buffer_handle->buffer = buffer;

		if (use_buffering)
		{
			streamer->streaming_buffer_type = BUFFER_TYPE_DEMUXED;

			// during prebuffering by requirement, buffer setting should not be changed.
			if (pre_buffering_time > 0)
				streamer->buffering_req.is_pre_buffering = TRUE;
		}

		g_object_set ( G_OBJECT (buffer_handle->buffer), "use-buffering", use_buffering, NULL );
	}

	LOGD ("pre_buffering: %2.2f, during playing: %2.2f\n", pre_buffering_time, buffering_time);

	if (pre_buffering_time <= 0.0)
	{
		pre_buffering_time = GET_DEFAULT_PLAYING_TIME(streamer);
		streamer->buffering_req.initial_second = (gint)ceil(buffering_time);
	}

	high_percent = (pre_buffering_time * 100) / GET_MAX_BUFFER_TIME(streamer);
	LOGD ("high_percent %2.3f %%\n", high_percent);

	streaming_set_buffer_size (streamer, BUFFER_TYPE_DEMUXED, GET_MAX_BUFFER_BYTES(streamer), GET_MAX_BUFFER_TIME(streamer));
	streaming_set_buffer_percent (streamer, BUFFER_TYPE_DEMUXED, low_percent, 0, high_percent);

	streamer->need_sync = TRUE;

	MMPLAYER_FLEAVE();
	return;
}

static void
streaming_get_current_bitrate_info(	mm_player_streaming_t* streamer,
									GstMessage *buffering_msg,
									streaming_content_info_t content_info,
									streaming_bitrate_info_t* bitrate_info)
{

	GstQuery *query = NULL;
	GstBufferingMode mode = GST_BUFFERING_STREAM;
	gint in_rate = 0;
	gint out_rate = 0;
	gint64 buffering_left = -1;

	guint buffer_criteria = 0;
	guint estimated_content_bitrate = 0;

	gdouble buffer_buffering_time = DEFAULT_BUFFERING_TIME;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL(streamer);
	MMPLAYER_RETURN_IF_FAIL(bitrate_info);

	if ((buffering_msg == NULL) ||
		((streamer->buffer_handle[BUFFER_TYPE_DEMUXED].buffer != NULL) &&
		(streamer->buffer_handle[BUFFER_TYPE_MUXED].buffer != NULL) &&
		(buffering_msg->src == (GstObject *)streamer->buffer_handle[BUFFER_TYPE_DEMUXED].buffer)))
	{
		query = gst_query_new_buffering (GST_FORMAT_PERCENT);

		if (gst_element_query ((streamer->buffer_handle[BUFFER_TYPE_MUXED].buffer), query))
		{
			gst_query_parse_buffering_stats (query, &mode, &in_rate, &out_rate, &buffering_left);
		}

		gst_query_unref (query);
	}
	else
	{
		gst_message_parse_buffering_stats (buffering_msg, &mode, &in_rate, &out_rate, &buffering_left);
	}

	LOGD ("Streaming Info : in %d, out %d, left %lld\n", in_rate, out_rate, buffering_left);

	if ((content_info.content_size > 0) && (content_info.duration > 0) && ((content_info.duration/GST_SECOND) > 0))
		estimated_content_bitrate = GET_BIT_FROM_BYTE((guint)(content_info.content_size / (content_info.duration/GST_SECOND)));

	if (streamer->buffer_max_bitrate > 0)
	{
		streamer->buffer_max_bitrate = MAX(streamer->buffer_max_bitrate, streamer->buffer_avg_bitrate);
		streamer->buffer_max_bitrate = MAX(streamer->buffer_max_bitrate, estimated_content_bitrate);

		buffer_criteria = GET_BYTE_FROM_BIT(streamer->buffer_max_bitrate);

		if (streamer->buffer_avg_bitrate > estimated_content_bitrate)
			out_rate = GET_BYTE_FROM_BIT(streamer->buffer_avg_bitrate);
		else if (estimated_content_bitrate != 0)
			out_rate = GET_BYTE_FROM_BIT(estimated_content_bitrate);
		else
			out_rate = GET_BYTE_FROM_BIT(streamer->buffer_max_bitrate/3);

		LOGD ("(max)content_max_byte_rate %d, byte_out_rate %d\n", buffer_criteria, out_rate);
	}
	else if (streamer->buffer_avg_bitrate > 0)
	{
		buffer_criteria = GET_BYTE_FROM_BIT(streamer->buffer_avg_bitrate * 3);
		out_rate = GET_BYTE_FROM_BIT(MAX(streamer->buffer_avg_bitrate,estimated_content_bitrate));

		LOGD ("(avg)content_max_byte_rate %d, byte_out_rate %d\n", buffer_criteria, out_rate);
	}
	else
	{
		LOGW ("There is no content bitrate information\n");
	}

	if ((in_rate > 0) && (out_rate > 0))
		buffer_buffering_time =  (gdouble)out_rate / (gdouble)in_rate;
	else if ((in_rate <= 0) && (out_rate > 0))
		buffer_buffering_time = MAX_BUFFERING_TIME;
	else
		buffer_buffering_time = DEFAULT_BUFFERING_TIME;

	(*bitrate_info).byte_in_rate = in_rate;
	(*bitrate_info).byte_out_rate = out_rate;
	(*bitrate_info).time_rate = buffer_buffering_time;
	(*bitrate_info).buffer_criteria = buffer_criteria;
}

static void
streaming_handle_fixed_buffering_mode(	mm_player_streaming_t* streamer,
										gint byte_out_rate,
										gdouble fixed_buffering_time,
										streaming_buffer_info_t* buffer_info)
{
	streaming_buffer_t* buffer_handle = NULL;

	guint buffering_bytes = 0;
	gdouble buffering_time = 0.0;
	gdouble per_byte = 0.0;
	gdouble per_time = 0.0;

	MMPLAYER_RETURN_IF_FAIL(streamer);
	MMPLAYER_RETURN_IF_FAIL(buffer_info);

	buffer_handle = &(streamer->buffer_handle[streamer->streaming_buffer_type]);
	buffering_time = fixed_buffering_time;

	LOGD ("buffering time: %2.2f sec, out rate: %d\n", buffering_time, byte_out_rate);

	if ((buffering_time > 0) && (byte_out_rate > 0))
	{
		buffering_bytes = GET_NEW_BUFFERING_BYTE(byte_out_rate * buffering_time);
	}
	else
	{
		if (buffering_time <= 0)
			buffering_time = GET_CURRENT_BUFFERING_TIME(buffer_handle);

		LOGW ("content bitrate is not updated yet.\n");
		buffering_bytes = GET_CURRENT_BUFFERING_BYTE(buffer_handle);
	}

	GET_PERCENT(buffering_time, GET_CURRENT_BUFFERING_TIME(buffer_handle), buffer_handle->buffer_high_percent, per_time);
	GET_PERCENT(buffering_bytes, GET_CURRENT_BUFFERING_BYTE(buffer_handle), buffer_handle->buffer_high_percent, per_byte);

	LOGD ("bytes %d, time %f, per_byte %f, per_time %f\n",
										buffering_bytes, buffering_time, per_byte, per_time);

	(*buffer_info).buffering_bytes = buffering_bytes;
	(*buffer_info).buffering_time = buffering_time;
	(*buffer_info).percent_byte = per_byte;
	(*buffer_info).percent_time = per_time;
}

static void
streaming_handle_adaptive_buffering_mode(	mm_player_streaming_t* streamer,
										streaming_content_info_t content_info,
										streaming_bitrate_info_t bitrate_info,
										streaming_buffer_info_t* buffer_info,
										gint expected_play_time)
{
	streaming_buffer_t* buffer_handle = NULL;

	gint buffering_bytes = 0;
	gint adj_buffering_bytes = 0;
	gdouble buffer_buffering_time = 0.0;
	gdouble per_byte = 0.0;
	gdouble per_time = 0.0;
	gdouble portion = 0.0;
	gdouble default_buffering_time = 0.0;

	MMPLAYER_RETURN_IF_FAIL(streamer);
	MMPLAYER_RETURN_IF_FAIL(buffer_info);

	LOGD ("pos %lld, dur %lld, size %lld, in/out:%d/%d, buffer_criteria:%d, time_rate:%f, need:%d sec\n",
							content_info.position, content_info.duration, content_info.content_size,
							bitrate_info.byte_in_rate, bitrate_info.byte_out_rate,
							bitrate_info.buffer_criteria, bitrate_info.time_rate, expected_play_time);

	if (((expected_play_time == TO_THE_END) && (content_info.position <= 0)) ||
		(content_info.duration <= 0) ||
		(content_info.content_size <= 0))
	{
		LOGW ("keep previous setting.\n");
		return;
	}

	if ((bitrate_info.byte_out_rate <= 0) || (bitrate_info.buffer_criteria == 0))
	{
		LOGW ("keep previous setting.\n");
		return;
	}

	buffer_handle = &(streamer->buffer_handle[streamer->streaming_buffer_type]);

	if (bitrate_info.byte_in_rate < bitrate_info.byte_out_rate)
	{
		if (expected_play_time != TO_THE_END)
			portion = (double)(expected_play_time * GST_SECOND) / (double)content_info.duration;
		else
			portion = (1 - (double)content_info.position/(double)content_info.duration);

		buffering_bytes = GET_NEW_BUFFERING_BYTE(((double)content_info.content_size * portion)	\
															* (1 - (double)bitrate_info.byte_in_rate/(double)bitrate_info.byte_out_rate));
	}
	else
	{
		/* buffering_bytes will be set as streamer->default_val.buffering_time *
		 * receiving rate is bigger than avg content bitrate
		 * so there is no reason to buffering. if the buffering msg is posted
		 * in-rate or contents bitrate has wrong value. */
		LOGW ("don't need to do buffering.\n");
	}

	if (buffering_bytes > 0)
		buffer_buffering_time = (gdouble)buffering_bytes / (gdouble)bitrate_info.byte_out_rate;

	if (content_info.position <= 0)
	{
		/* if the buffer is filled under 50%, MSL use the original default buffering time.
		   if not, MSL use just 2 sec as a default buffering time. (to reduce initial buffering time) */
		default_buffering_time = streamer->default_val.buffering_time - ((gdouble)streamer->buffering_percent/50);
	}
	else
	{
		default_buffering_time = streamer->default_val.buffering_time;
	}

	if (buffer_buffering_time < default_buffering_time)
	{
		LOGD ("adjusted time: %2.2f -> %2.2f\n", buffer_buffering_time, default_buffering_time);
		LOGD ("adjusted bytes : %d or %d or %d\n",
			buffering_bytes,
			(gint)(bitrate_info.byte_out_rate * buffer_buffering_time),
			(gint)(bitrate_info.buffer_criteria * buffer_buffering_time));

		if (content_info.position > 0)
		{
			/* start monitoring the abmormal state */
			streamer->default_val.buffering_monitor = TRUE;
		}

		buffer_buffering_time = default_buffering_time;
		adj_buffering_bytes = GET_NEW_BUFFERING_BYTE(bitrate_info.byte_out_rate * (gint)ceil(buffer_buffering_time));
		buffering_bytes = MAX(buffering_bytes, adj_buffering_bytes);
	}

	GET_PERCENT(buffering_bytes, GET_CURRENT_BUFFERING_BYTE(buffer_handle), buffer_handle->buffer_high_percent, per_byte);
	GET_PERCENT(buffer_buffering_time, GET_CURRENT_BUFFERING_TIME(buffer_handle), buffer_handle->buffer_high_percent, per_time);

	LOGD ("monitor %d, bytes %d, time %f, per_byte %f, per_time %f\n",
										streamer->default_val.buffering_monitor,
										buffering_bytes, buffer_buffering_time, per_byte, per_time);

	(*buffer_info).buffering_bytes = buffering_bytes;
	(*buffer_info).buffering_time = buffer_buffering_time;
	(*buffer_info).percent_byte = per_byte;
	(*buffer_info).percent_time = per_time;

}

static void
streaming_update_buffer_setting	(	mm_player_streaming_t* streamer,
									GstMessage *buffering_msg,	// can be null
									guint64 content_size,
									gint64 position,
									gint64 duration)
{
	streaming_buffer_t* buffer_handle = NULL;
	MMPlayerBufferingMode buffering_mode = MM_PLAYER_BUFFERING_MODE_ADAPTIVE;

	streaming_buffer_info_t	buffer_info;
	streaming_content_info_t content_info;
	streaming_bitrate_info_t bitrate_info;

	gdouble low_percent = 0.0;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL ( streamer );

	memset(&buffer_info, 0x00, sizeof(streaming_buffer_info_t));
	memset(&content_info, 0x00, sizeof(streaming_content_info_t));
	memset(&bitrate_info, 0x00, sizeof(streaming_bitrate_info_t));

	buffer_handle = &(streamer->buffer_handle[streamer->streaming_buffer_type]);

	if (streamer->buffering_req.is_pre_buffering == TRUE)
		buffering_mode = MM_PLAYER_BUFFERING_MODE_FIXED;
	else
		buffering_mode = streamer->buffering_req.mode;

	buffer_info.buffering_bytes = buffer_handle->buffering_bytes;
	buffer_info.buffering_time = buffer_handle->buffering_time;
	buffer_info.percent_byte = buffer_handle->buffer_high_percent;
	buffer_info.percent_time = buffer_handle->buffer_high_percent;

	content_info.position = position;
	content_info.duration = duration;
	content_info.content_size = content_size;

	streaming_get_current_bitrate_info(streamer, buffering_msg, content_info, &bitrate_info);

	LOGD ("buffering mode %d, new info in_r:%d, out_r:%d, cb:%d, bt:%f\n",
					buffering_mode, bitrate_info.byte_in_rate, bitrate_info.byte_out_rate,
					bitrate_info.buffer_criteria, bitrate_info.time_rate);

	/* calculate buffer low/high percent */
	low_percent = DEFAULT_BUFFER_LOW_PERCENT;

	/********************
	 * (1) fixed mode   *
	 ********************/

	if (buffering_mode == MM_PLAYER_BUFFERING_MODE_FIXED)
	{
		gdouble buffering_time = 0.0;

		if (streamer->buffering_req.is_pre_buffering == TRUE)
			buffering_time = (gdouble)streamer->buffering_req.initial_second;
		else
			buffering_time = (gdouble)streamer->buffering_req.runtime_second;

		streaming_handle_fixed_buffering_mode(streamer, bitrate_info.byte_out_rate, buffering_time, &buffer_info);
	}

	/***********************************
	 * (2) once mode for samsung link  *
	 ***********************************/
	else if (buffering_mode == MM_PLAYER_BUFFERING_MODE_SLINK)
	{
		streaming_handle_adaptive_buffering_mode(streamer, content_info, bitrate_info, &buffer_info, TO_THE_END);
	}

	/*********************************
	 * (3) adaptive mode (default)   *
	 *********************************/
	else
	{
		gint expected_play_time = DEFAULT_PLAYING_TIME;

		if (streamer->buffering_req.runtime_second > 0)
		{
			expected_play_time = streamer->buffering_req.runtime_second;
		}
		else if ((position == 0) && (streamer->is_buffering))
		{
			expected_play_time = streamer->buffering_req.initial_second;
		}

		if (expected_play_time <= 0)
			expected_play_time = DEFAULT_PLAYING_TIME;

		streaming_handle_adaptive_buffering_mode(streamer, content_info, bitrate_info, &buffer_info, expected_play_time);

		if (IS_MUXED_BUFFERING_MODE(streamer))	// even if new byte size is smaller than the previous one, time need to be updated.
			buffer_handle->buffering_time = buffer_info.buffering_time;
	}

	LOGD ("adj buffer(%d) %d->%d bytes/%2.2f->%2.2f sec\n",
					streamer->streaming_buffer_type,
					GET_CURRENT_BUFFERING_BYTE(buffer_handle), buffer_info.buffering_bytes,
					GET_CURRENT_BUFFERING_TIME(buffer_handle), buffer_info.buffering_time);

	/* queue2 : bytes, multiqueue : time */
	if (((GET_CURRENT_BUFFERING_BYTE(buffer_handle) < buffer_info.buffering_bytes) && IS_MUXED_BUFFERING_MODE(streamer)) ||
		((GET_CURRENT_BUFFERING_TIME(buffer_handle) < buffer_info.buffering_time) && IS_DEMUXED_BUFFERING_MODE(streamer)))
	{
		if (duration > 0 && position > 0)
		{
			gdouble buffering_time_limit = (gdouble)(duration - position)/GST_SECOND;

			if (buffer_info.buffering_time > buffering_time_limit)
				buffer_info.buffering_time = buffering_time_limit;
		}

		streaming_set_buffer_size(streamer, streamer->streaming_buffer_type, buffer_info.buffering_bytes, buffer_info.buffering_time);
	}

	streaming_set_buffer_percent(streamer, streamer->streaming_buffer_type, low_percent, buffer_info.percent_byte, buffer_info.percent_time);

	LOGD("buffer setting: size %d, time %f, per %f\n",
							GET_CURRENT_BUFFERING_BYTE(buffer_handle),
							GET_CURRENT_BUFFERING_TIME(buffer_handle),
							buffer_handle->buffer_high_percent);

	streamer->need_sync = TRUE;
}

static void
streaming_adjust_min_threshold(mm_player_streaming_t* streamer, gint64 position)
{
#define DEFAULT_TIME_PAD 1	/* sec */
	gint playing_time = 0;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL(streamer);

	playing_time = (gint)((position - streamer->default_val.prev_pos) / GST_SECOND);

	LOGD ("buffering monitor = %s\n", (streamer->default_val.buffering_monitor)?"ON":"OFF");
	LOGD ("playing_time ( %d sec) = %lld - %lld \n", playing_time, position, streamer->default_val.prev_pos);
	LOGD ("default time : %2.3f, prev buffering t : %2.3f\n",
					streamer->default_val.buffering_time, streamer->buffer_handle[streamer->streaming_buffer_type].buffering_time);

	if ((streamer->default_val.buffering_monitor) && (playing_time <= (gint)streamer->default_val.buffering_time))
	{
		gint time_gap = 0;
		time_gap = (gint)(streamer->default_val.buffering_time - DEFAULT_BUFFERING_TIME);
		if (time_gap <= 0)
			time_gap = DEFAULT_TIME_PAD;

		streamer->default_val.buffering_time += time_gap*2;
		streamer->default_val.buffering_time = MIN(streamer->default_val.buffering_time, MAX_BUFFERING_TIME);
	}
	else
	{
		streamer->default_val.buffering_time = DEFAULT_BUFFERING_TIME;
	}

	LOGD ("new default min value %2.3f \n", streamer->default_val.buffering_time);

	streamer->default_val.buffering_monitor = FALSE;
	streamer->default_val.prev_pos = position;
}

static void
streaming_update_buffering_status(mm_player_streaming_t* streamer, GstMessage *buffering_msg, gint64 position)
{
	gint buffer_percent = 0;
	gboolean increased_per = TRUE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL(streamer);
	MMPLAYER_RETURN_IF_FAIL(buffering_msg);

	/* update when buffering has started. */
	if ( !streamer->is_buffering )
	{
		streamer->is_buffering = TRUE;
		streamer->is_buffering_done = FALSE;
		streamer->buffering_percent = -1;

		if (!streamer->buffering_req.is_pre_buffering)
		{
			streamer->need_update = TRUE;
			streaming_adjust_min_threshold(streamer, position);
		}
	}

	/* update buffer percent */
	gst_message_parse_buffering (buffering_msg, &buffer_percent);

	if (streamer->buffering_percent < buffer_percent)
	{
		LOGD ("[%s] buffering %d%%....\n",
			GST_OBJECT_NAME(GST_MESSAGE_SRC(buffering_msg)), buffer_percent);
		streamer->buffering_percent = buffer_percent;
	}
	else
	{
		increased_per = FALSE;
	}

	if ((streamer->buffering_percent == MAX_BUFFER_PERCENT) || (streamer->is_buffering_done == TRUE))
	{
		streamer->is_buffering = FALSE;
		streamer->buffering_req.is_pre_buffering = FALSE;
		if (streamer->buffering_percent == MAX_BUFFER_PERCENT)
			streamer->is_buffering_done = FALSE;
		else
			streamer->buffering_percent = MAX_BUFFER_PERCENT;
	}
	else
	{
		/* need to update periodically in case of slink mode */
		if ((increased_per == TRUE) &&
			(buffer_percent%10 == 0) &&
			(streamer->buffering_req.mode == MM_PLAYER_BUFFERING_MODE_SLINK) &&
			(streamer->buffering_req.is_pre_buffering == FALSE))
		{
			/* Update buffer setting to reflect data receiving rate for slink mode */
			streamer->need_update = TRUE;
		}
	}
}

void __mm_player_streaming_buffering( mm_player_streaming_t* streamer,
									  GstMessage *buffering_msg,
									  guint64 content_size,
									  gint64 position,
									  gint64 duration)
{
	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL ( streamer );
	MMPLAYER_RETURN_IF_FAIL ( buffering_msg );
	MMPLAYER_RETURN_IF_FAIL ( GST_IS_MESSAGE ( buffering_msg ) );
	MMPLAYER_RETURN_IF_FAIL ( (GST_MESSAGE_TYPE ( buffering_msg ) == GST_MESSAGE_BUFFERING) );

	if (buffering_msg)
	{
		if (position > (gint64)(streamer->buffering_req.initial_second * GST_SECOND))
			streamer->buffering_req.is_pre_buffering = FALSE;

		streaming_update_buffering_status(streamer, buffering_msg, position);

		if (!streamer->need_update)
		{
			//LOGD ("don't need to update buffering stats during buffering.\n");
			return;
		}

		streamer->need_update = FALSE;
	}

	streaming_update_buffer_setting (streamer, buffering_msg, content_size, position, duration);

	return;
}

