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

#include <sys/vfs.h>
#include "mm_player_utils.h"

#include "mm_player_streaming.h"

static void streaming_set_buffer_size(mm_player_streaming_t* streamer, guint buffer_size);
static void streaming_set_buffer_percent(mm_player_streaming_t* streamer, gdouble low_percent, gdouble high_percent);
static void streaming_set_buffer_type (mm_player_streaming_t* streamer, gboolean use_file, gchar * file_path, guint64 content_size);
static void streaming_set_buffering_time(mm_player_streaming_t* streamer, gdouble buffering_time);


mm_player_streaming_t *
__mm_player_streaming_create ()
{
	mm_player_streaming_t *streamer = NULL;

	debug_fenter();

	streamer = (mm_player_streaming_t *) malloc (sizeof (mm_player_streaming_t));
	if (!streamer)
	{
		debug_error ("fail to create streaming player handle..\n");
		return NULL;
	}

	debug_fleave();

	return streamer;
}

void __mm_player_streaming_initialize (mm_player_streaming_t* streamer)
{
	debug_fenter();

	streamer->buffer = NULL;
	streamer->buffer_size = DEFAULT_BUFFER_SIZE;
	streamer->buffer_low_percent = DEFAULT_BUFFER_LOW_PERCENT;
	streamer->buffer_high_percent = DEFAULT_BUFFER_HIGH_PERCENT;
	streamer->buffer_avg_bitrate = 0;
	streamer->buffer_max_bitrate = 0;
	streamer->need_update = FALSE;

	streamer->is_buffering = FALSE;
	streamer->buffering_percent = -1;
	streamer->buffering_time = DEFAULT_BUFFERING_TIME;

	debug_fleave();

	return;
}

void __mm_player_streaming_deinitialize (mm_player_streaming_t* streamer)
{
	debug_fenter();

	return_if_fail(streamer);

	streamer->buffer_size = DEFAULT_BUFFER_SIZE;
	streamer->buffer_low_percent = DEFAULT_BUFFER_LOW_PERCENT;
	streamer->buffer_high_percent = DEFAULT_BUFFER_HIGH_PERCENT;
	streamer->buffer_avg_bitrate = 0;
	streamer->buffer_max_bitrate = 0;
	streamer->need_update = FALSE;

	streamer->is_buffering = FALSE;
	streamer->buffering_percent = -1;
	streamer->buffering_time = DEFAULT_BUFFERING_TIME;

	debug_fleave();

	return;
}


void __mm_player_streaming_destroy (mm_player_streaming_t* streamer)
{
	debug_fenter();

	if(streamer)
	{
		free (streamer);
		streamer = NULL;
	}

	debug_fleave();

	return;
}


void __mm_player_streaming_set_buffer(mm_player_streaming_t* streamer, GstElement * buffer,
	gboolean use_buffering, guint buffer_size, gdouble low_percent, gdouble high_percent, gdouble buffering_time,
	gboolean use_file, gchar * file_path, guint64 content_size)
{
	debug_fenter();

	return_if_fail(streamer);

	if (buffer)
	{
		streamer->buffer = buffer;

		debug_log("buffer element is %s.", GST_ELEMENT_NAME(buffer));

		g_object_set ( G_OBJECT (streamer->buffer), "use-buffering", use_buffering, NULL );
	}

	streaming_set_buffer_size(streamer, buffer_size);
	streaming_set_buffer_percent(streamer, low_percent, high_percent);
	streaming_set_buffer_type (streamer, use_file, file_path, content_size);
	streaming_set_buffering_time(streamer, buffering_time);

	debug_fleave();

	return;
}


void __mm_player_streaming_set_content_bitrate(mm_player_streaming_t* streamer, guint max_bitrate, guint avg_bitrate)
{
	debug_fenter();

	return_if_fail(streamer);

       /* Note : Update buffering criterion bytes
         *      1. maximum bitrate is considered first.
         *      2. average bitrage * 3 is next.
         *      3. if there are no updated bitrate, use default buffering limit.
         */
        if (max_bitrate > 0 && streamer->buffer_max_bitrate != max_bitrate)
        {
              debug_log("set maximum bitrate(%dbps).\n", max_bitrate);
              streamer->buffer_max_bitrate = max_bitrate;

		streamer->need_update = TRUE;
        }

        if (avg_bitrate > 0 && streamer->buffer_avg_bitrate != avg_bitrate)
	{
              debug_log("set averate bitrate(%dbps).\n", avg_bitrate);
              streamer->buffer_avg_bitrate = avg_bitrate;

		streamer->need_update = TRUE;
	}

	debug_fleave();

	return;
}

static void
streaming_set_buffer_size(mm_player_streaming_t* streamer, guint buffer_size)
{
	debug_fenter();

	return_if_fail(streamer);
	return_if_fail(buffer_size>0);

	debug_log("set buffer size to %d.", buffer_size);

	streamer->buffer_size = buffer_size;

	if (streamer->buffer)
		g_object_set (G_OBJECT(streamer->buffer), "max-size-bytes", buffer_size, NULL);

	debug_fleave();

	return;
}

static void
streaming_set_buffer_percent(mm_player_streaming_t* streamer, gdouble low_percent, gdouble high_percent)
{
	gdouble buffer_low_percent = DEFAULT_BUFFER_LOW_PERCENT;
	gdouble buffer_high_percent = DEFAULT_BUFFER_HIGH_PERCENT;

	debug_fenter();

	return_if_fail(streamer);

	if (low_percent <= MIN_BUFFER_PERCENT || low_percent >= MAX_BUFFER_PERCENT)
	{
		debug_warning("buffer low percent is out of range. use defaut value.");
		buffer_low_percent = DEFAULT_BUFFER_LOW_PERCENT;
	}
	else
	{
		buffer_low_percent = low_percent;
	}

	if (high_percent  <=  MIN_BUFFER_PERCENT || high_percent  >=  MAX_BUFFER_PERCENT)
	{
		debug_warning("buffer high percent is out of range. use defaut value.");
		buffer_high_percent = DEFAULT_BUFFER_HIGH_PERCENT;
	}
	else
	{
		buffer_high_percent = high_percent;
	}

	if (buffer_high_percent <= buffer_low_percent)
		buffer_high_percent =  buffer_low_percent + 1.0;

	debug_log("set buffer percent to %2.3f ~ %2.3f.",  streamer->buffer_low_percent, streamer->buffer_high_percent);

	if (streamer->buffer)
	{
		if ( streamer->buffer_low_percent != buffer_low_percent )
			g_object_set (G_OBJECT(streamer->buffer), "low-percent", streamer->buffer_low_percent, NULL);

		if ( streamer->buffer_high_percent != buffer_high_percent )
			g_object_set (G_OBJECT(streamer->buffer), "high-percent", streamer->buffer_high_percent, NULL);
	}

	streamer->buffer_low_percent = buffer_low_percent;
	streamer->buffer_high_percent = buffer_high_percent;

	debug_fleave();

	return;
}

static void
streaming_set_buffering_time(mm_player_streaming_t* streamer, gdouble buffering_time)
{
	gdouble buffer_buffering_time = DEFAULT_BUFFERING_TIME;

	debug_fenter();

	return_if_fail(streamer);

	if (buffering_time < MIN_BUFFERING_TIME)
		buffer_buffering_time = MIN_BUFFERING_TIME;
	else if (buffering_time > MAX_BUFFERING_TIME)
		buffer_buffering_time = MAX_BUFFERING_TIME;
	else
		buffer_buffering_time = buffering_time;

	if (streamer->buffering_time != buffer_buffering_time)
	{
		debug_log("set buffer buffering time from %2.1f to %2.1f.", streamer->buffering_time, buffer_buffering_time);

		streamer->buffering_time = buffer_buffering_time;
	}

	debug_fleave();

	return;
}

static void
streaming_set_buffer_type (mm_player_streaming_t* streamer, gboolean use_file, gchar * file_path, guint64 content_size)
{
	guint64 storage_available_size = 0L; //bytes
	guint64 file_buffer_size = 0L;  //bytes
	gchar file_buffer_name[MAX_FILE_BUFFER_NAME_LEN] = {0};
	struct statfs buf = {0};

	debug_fenter();

	return_if_fail(streamer && streamer->buffer);

	if (!use_file)
	{
		debug_log("use memory for buffering. streaming is played on push-based. \n"
				"buffering position would not be updated.\n"
				"buffered data would be flushed after played.\n"
				"seeking and getting duration could be failed due to file format.");
		return;
	}

	debug_log("use file for buffering. streaming is played on pull-based. \n");

	if (!file_path || strlen(file_path) <= 0)
		file_path = g_strdup(DEFAULT_FILE_BUFFER_PATH);

	sprintf( file_buffer_name, "%s/XXXXXX", file_path );
	debug_log("the buffering file name is %s.\n", file_buffer_name);

	if (statfs((const char *)file_path, &buf) < 0)
	{
		debug_warning ("fail to get availabe storage capacity. just use file buffer.\n");
		file_buffer_size = 0L;
	}
	else
	{
		storage_available_size = (guint64)buf.f_bavail * (guint64)buf.f_bsize; //bytes

		debug_log ("the number of available blocks : %"G_GUINT64_FORMAT", the block size is %"G_GUINT64_FORMAT".\n",
			(guint64)buf.f_bavail, (guint64)buf.f_bsize);
		debug_log ("calculated availabe storage size is %"G_GUINT64_FORMAT" Bytes.\n", storage_available_size);

		if (content_size <= 0 || content_size >= storage_available_size)
			file_buffer_size = storage_available_size;
		else
			file_buffer_size = 0L;
	}

	if (file_buffer_size>0)
		debug_log("use file ring buffer for buffering.");

	g_object_set (G_OBJECT(streamer->buffer), "temp-template", file_buffer_name, NULL);
	g_object_set (G_OBJECT(streamer->buffer), "file-buffer-max-size", file_buffer_size, NULL);

	debug_fleave();

	return;
}

#define GET_BYTE_FROM_BIT(bit) (bit/8)
void __mm_player_streaming_buffering(mm_player_streaming_t* streamer, GstMessage *buffering_msg)
{
	GstBufferingMode mode = GST_BUFFERING_STREAM;
	gint byte_in_rate = 0;
	gint byte_out_rate = 0;
	gint64 buffering_left = -1;
	gdouble buffering_time = DEFAULT_BUFFERING_TIME;
	gdouble low_percent = 0.0;
	gdouble high_percent = 0.0;
	guint high_percent_byte = 0;
	gint buffer_percent = 0;
	guint buffer_criteria = 0;

	return_if_fail ( streamer );
	return_if_fail ( buffering_msg );
	return_if_fail ( GST_IS_MESSAGE ( buffering_msg ) );
	return_if_fail ( GST_MESSAGE_TYPE ( buffering_msg ) == GST_MESSAGE_BUFFERING );

	/* update when buffering has started. */
	if ( !streamer->is_buffering )
	{
		debug_log ( "buffering has started.\n" );

		streamer->is_buffering = TRUE;
		streamer->buffering_percent = -1;
		streamer->need_update = TRUE;
	}

	/* update buffer percent */
	gst_message_parse_buffering ( buffering_msg, &buffer_percent );

	if ( streamer->buffering_percent < buffer_percent )
	{
		debug_log ( "buffering %d%%....\n", buffer_percent );
		streamer->buffering_percent = buffer_percent;
	}

	if ( streamer->buffering_percent == MAX_BUFFER_PERCENT )
	{
		debug_log ( "buffering had done.\n" );
		streamer->is_buffering = FALSE;
	}

	if (!streamer->need_update)
	{
		debug_log ( "don't need to update buffering stats during buffering.\n" );
		return;
	}

        /* Note : Parse the buffering message to get the in/out throughput.
         *     avg_in is the network throughput and avg_out is the consumed throughtput by the linkded element.
         */
	gst_message_parse_buffering_stats ( buffering_msg, &mode, &byte_in_rate, &byte_out_rate, &buffering_left );

	if (streamer->buffer_max_bitrate > 0)
	{
		buffer_criteria = GET_BYTE_FROM_BIT(streamer->buffer_max_bitrate);
		byte_out_rate = GET_BYTE_FROM_BIT(streamer->buffer_max_bitrate /3);
	}
	else if (streamer->buffer_avg_bitrate > 0)
	{
		buffer_criteria = GET_BYTE_FROM_BIT(streamer->buffer_avg_bitrate * 3);
		byte_out_rate = GET_BYTE_FROM_BIT(streamer->buffer_avg_bitrate);
	}

	debug_log ( "in rate is %d, out rate is %d (bytes/sec).\n", byte_in_rate, byte_out_rate );

	if ( byte_in_rate > 0  &&  byte_out_rate > 0)
		buffering_time =  byte_out_rate / byte_in_rate;
	else if (byte_in_rate <= 0 && byte_out_rate > 0)
		buffering_time = MAX_BUFFERING_TIME;
	else
		buffering_time = DEFAULT_BUFFERING_TIME;

	streaming_set_buffering_time(streamer, buffering_time);

	/* calculate buffer low/high percent */
	low_percent = DEFAULT_BUFFER_LOW_PERCENT;

	if ( buffer_criteria > 0 )
	{
		high_percent_byte = buffer_criteria * streamer->buffering_time;
		high_percent = ( (gdouble)high_percent_byte * 100.0 )  / (gdouble)streamer->buffer_size;
	}
	else
	{
		high_percent_byte = streamer->buffer_high_percent * streamer->buffer_size / 100;
		high_percent= streamer->buffer_high_percent;
	}

	if ( streamer->buffer_size < high_percent_byte )
	{
		debug_log ( "buffer size[%d bytes] is smaller than high threshold[%d bytes]. update it. \n",
			streamer->buffer_size, high_percent_byte );

		streaming_set_buffer_size(streamer, high_percent_byte * 1.1);
	}

	streaming_set_buffer_percent(streamer, low_percent, high_percent);

	streamer->need_update = FALSE;

	return;
}

