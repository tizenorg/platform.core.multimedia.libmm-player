/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, Heechul Jeon <heechul.jeon@samsung.com>
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
#if 0
#include <glib.h>
#include <gst/gst.h>
#ifndef GST_API_VERSION_1
#include <gst/interfaces/xoverlay.h>
#else
#include <gst/video/videooverlay.h>
#endif
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdlib.h>

#include <mm_error.h>
//#include <mm_attrs.h>
//#include <mm_attrs_private.h>
#include <mm_debug.h>

#include "mm_player_ini.h"
#include "mm_player_attrs.h"
#include "mm_player_capture.h"
#endif
#include <gst/app/gstappsrc.h>

#include "mm_player_priv.h"
#include "mm_player_priv_internal.h"
#include "mm_player_priv_locl_func.h"

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
/* video capture callback*/
gulong ahs_appsrc_cb_probe_id = 0;

/*---------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:												|
---------------------------------------------------------------------------*/

/*===========================================================================================
|																							|
|  FUNCTION DEFINITIONS																		|
|  																							|
========================================================================================== */

gboolean
__mmplayer_gst_callback(GstBus *bus, GstMessage *msg, gpointer data) // @
{
	mm_player_t* player = (mm_player_t*) data;
	gboolean ret = TRUE;
	static gboolean async_done = FALSE;

	return_val_if_fail ( player, FALSE );
	return_val_if_fail ( msg && GST_IS_MESSAGE(msg), FALSE );

	switch ( GST_MESSAGE_TYPE( msg ) )
	{
		case GST_MESSAGE_UNKNOWN:
			debug_warning("unknown message received\n");
		break;

		case GST_MESSAGE_EOS:
		{
			MMHandleType attrs = 0;
			gint count = 0;

			debug_log("GST_MESSAGE_EOS received\n");

			/* NOTE : EOS event is comming multiple time. watch out it */
			/* check state. we only process EOS when pipeline state goes to PLAYING */
			if ( ! (player->cmd == MMPLAYER_COMMAND_START || player->cmd == MMPLAYER_COMMAND_RESUME) )
			{
				debug_warning("EOS received on non-playing state. ignoring it\n");
				break;
			}

			if ( (player->audio_stream_cb) && (player->is_sound_extraction) )
			{
				GstPad *pad = NULL;

				pad = gst_element_get_static_pad (player->pipeline->audiobin[MMPLAYER_A_SINK].gst, "sink");

				debug_error("release audio callback\n");

				/* release audio callback */
				gst_pad_remove_probe (pad, player->audio_cb_probe_id);
				player->audio_cb_probe_id = 0;
				/* audio callback should be free because it can be called even though probe remove.*/
				player->audio_stream_cb = NULL;
				player->audio_stream_cb_user_param = NULL;

			}

			/* rewind if repeat count is greater then zero */
			/* get play count */
			attrs = MMPLAYER_GET_ATTRS(player);

			if ( attrs )
			{
				gboolean smooth_repeat = FALSE;

				mm_attrs_get_int_by_name(attrs, "profile_play_count", &count);
				mm_attrs_get_int_by_name(attrs, "profile_smooth_repeat", &smooth_repeat);

				debug_log("remaining play count: %d, playback rate: %f\n", count, player->playback_rate);

				if ( count > 1 || count == -1 || player->playback_rate < 0.0 ) /* default value is 1 */
				{
					if ( smooth_repeat )
					{
						debug_log("smooth repeat enabled. seeking operation will be excuted in new thread\n");

						g_cond_signal( player->repeat_thread_cond );

						break;
					}
					else
					{
						gint ret_value = 0;

						if ( player->section_repeat )
						{
							ret_value = _mmplayer_activate_section_repeat((MMHandleType)player, player->section_repeat_start, player->section_repeat_end);
						}
						else
						{

							if ( player->playback_rate < 0.0 )
							{
								player->resumed_by_rewind = TRUE;
								_mmplayer_set_mute((MMHandleType)player, 0);
								MMPLAYER_POST_MSG( player, MM_MESSAGE_RESUMED_BY_REW, NULL );
							}

							ret_value = __gst_set_position( player, MM_PLAYER_POS_FORMAT_TIME, 0, TRUE);

							/* initialize */
							player->sent_bos = FALSE;
						}

						if ( MM_ERROR_NONE != ret_value )
						{
							debug_error("failed to set position to zero for rewind\n");
						}
						else
						{
							if ( count > 1 )
							{
								/* we successeded to rewind. update play count and then wait for next EOS */
								count--;

								mm_attrs_set_int_by_name(attrs, "profile_play_count", count);

								if ( mmf_attrs_commit ( attrs ) )
									debug_error("failed to commit attrs\n");
							}
						}

						break;
					}
				}
			}

			MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-eos" );

			/* post eos message to application */
			__mmplayer_post_delayed_eos( player, PLAYER_INI()->eos_delay );

			/* reset last position */
			player->last_position = 0;
		}
		break;

		case GST_MESSAGE_ERROR:
		{
			GError *error = NULL;
			gchar* debug = NULL;
			gchar *msg_src_element = NULL;

			/* generating debug info before returning error */
			MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-error" );

			/* get error code */
			gst_message_parse_error( msg, &error, &debug );

			msg_src_element = GST_ELEMENT_NAME( GST_ELEMENT_CAST( msg->src ) );
			if ( gst_structure_has_name ( gst_message_get_structure(msg), "streaming_error" ) )
			{
				/* Note : the streaming error from the streaming source is handled
				 *   using __mmplayer_handle_streaming_error.
				 */
				__mmplayer_handle_streaming_error ( player, msg );

				/* dump state of all element */
				__mmplayer_dump_pipeline_state( player );
			}
			else
			{
				/* traslate gst error code to msl error code. then post it
				 * to application if needed
				 */
				__mmplayer_handle_gst_error( player, msg, error );

				/* dump state of all element */
				__mmplayer_dump_pipeline_state( player );

			}

			if (MMPLAYER_IS_HTTP_PD(player))
			{
				_mmplayer_unrealize_pd_downloader ((MMHandleType)player);
			}

			MMPLAYER_FREEIF( debug );
			g_error_free( error );
		}
		break;

		case GST_MESSAGE_WARNING:
		{
			char* debug = NULL;
			GError* error = NULL;

			gst_message_parse_warning(msg, &error, &debug);

			debug_warning("warning : %s\n", error->message);
			debug_warning("debug : %s\n", debug);

			MMPLAYER_POST_MSG( player, MM_MESSAGE_WARNING, NULL );

			MMPLAYER_FREEIF( debug );
			g_error_free( error );
		}
		break;

		case GST_MESSAGE_INFO:				debug_log("GST_MESSAGE_STATE_DIRTY\n"); break;

		case GST_MESSAGE_TAG:
		{
			debug_log("GST_MESSAGE_TAG\n");
			if ( ! __mmplayer_gst_extract_tag_from_msg( player, msg ) )
			{
				debug_warning("failed to extract tags from gstmessage\n");
			}
		}
		break;

		case GST_MESSAGE_BUFFERING:
		{
			MMMessageParamType msg_param = {0, };
			gboolean update_buffering_percent = TRUE;

			if ( !MMPLAYER_IS_STREAMING(player) || (player->profile.uri_type == MM_PLAYER_URI_TYPE_HLS) ) // pure hlsdemux case, don't consider buffering of msl currently
				break;

			__mm_player_streaming_buffering (player->streamer, msg);

			__mmplayer_handle_buffering_message ( player );

			update_buffering_percent = (player->pipeline_is_constructed || MMPLAYER_IS_RTSP_STREAMING(player) );
			if (update_buffering_percent)
			{
				msg_param.connection.buffering = player->streamer->buffering_percent;
				MMPLAYER_POST_MSG ( player, MM_MESSAGE_BUFFERING, &msg_param );
			}
		}
		break;

		case GST_MESSAGE_STATE_CHANGED:
		{
			MMPlayerGstElement *mainbin;
			const GValue *voldstate, *vnewstate, *vpending;
			GstState oldstate, newstate, pending;

			if ( ! ( player->pipeline && player->pipeline->mainbin ) )
			{
				debug_error("player pipeline handle is null");
				break;
			}

			mainbin = player->pipeline->mainbin;

			/* we only handle messages from pipeline */
			if( msg->src != (GstObject *)mainbin[MMPLAYER_M_PIPE].gst )
				break;

			/* get state info from msg */
			voldstate = gst_structure_get_value (gst_message_get_structure(msg), "old-state");
			vnewstate = gst_structure_get_value (gst_message_get_structure(msg), "new-state");
			vpending = gst_structure_get_value (gst_message_get_structure(msg), "pending-state");

			oldstate = (GstState)voldstate->data[0].v_int;
			newstate = (GstState)vnewstate->data[0].v_int;
			pending = (GstState)vpending->data[0].v_int;

			debug_log("state changed [%s] : %s ---> %s     final : %s\n",
				GST_OBJECT_NAME(GST_MESSAGE_SRC(msg)),
				gst_element_state_get_name( (GstState)oldstate ),
				gst_element_state_get_name( (GstState)newstate ),
				gst_element_state_get_name( (GstState)pending ) );

			if (oldstate == newstate)
			{
				debug_warning("pipeline reports state transition to old state");
				break;
			}

			switch(newstate)
			{
				case GST_STATE_VOID_PENDING:
				break;

				case GST_STATE_NULL:
				break;

				case GST_STATE_READY:
				break;

				case GST_STATE_PAUSED:
				{
					gboolean prepare_async = FALSE;

					if ( ! player->audio_cb_probe_id && player->is_sound_extraction )
						__mmplayer_configure_audio_callback(player);

					if ( ! player->sent_bos && oldstate == GST_STATE_READY) // managed prepare async case
					{
						mm_attrs_get_int_by_name(player->attrs, "profile_prepare_async", &prepare_async);
						debug_log("checking prepare mode for async transition - %d", prepare_async);
					}

					if ( MMPLAYER_IS_STREAMING(player) || prepare_async )
					{
						MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_PAUSED );

						if (player->streamer)
						{
							__mm_player_streaming_set_content_bitrate(player->streamer,
								player->total_maximum_bitrate, player->total_bitrate);
						}
					}
				}
				break;

				case GST_STATE_PLAYING:
				{
					if (player->doing_seek && async_done)
					{
						player->doing_seek = FALSE;
						async_done = FALSE;
						MMPLAYER_POST_MSG ( player, MM_MESSAGE_SEEK_COMPLETED, NULL );
					}

					if ( MMPLAYER_IS_STREAMING(player) ) // managed prepare async case when buffering is completed
					{
						// pending state should be reset oyherwise, it's still playing even though it's resumed after bufferging.
						MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_PLAYING);
					}
				}
				break;

				default:
				break;
			}
		}
		break;

		case GST_MESSAGE_STATE_DIRTY:		debug_log("GST_MESSAGE_STATE_DIRTY\n"); break;
		case GST_MESSAGE_STEP_DONE:			debug_log("GST_MESSAGE_STEP_DONE\n"); break;
		case GST_MESSAGE_CLOCK_PROVIDE:		debug_log("GST_MESSAGE_CLOCK_PROVIDE\n"); break;

		case GST_MESSAGE_CLOCK_LOST:
			{
				GstClock *clock = NULL;
				gst_message_parse_clock_lost (msg, &clock);
				debug_log("GST_MESSAGE_CLOCK_LOST : %s\n", (clock ? GST_OBJECT_NAME (clock) : "NULL"));
				g_print ("GST_MESSAGE_CLOCK_LOST : %s\n", (clock ? GST_OBJECT_NAME (clock) : "NULL"));

				if (PLAYER_INI()->provide_clock)
				{
					debug_log ("Provide clock is TRUE, do pause->resume\n");
					__gst_pause(player, FALSE);
					__gst_resume(player, FALSE);
				}
			}
			break;

		case GST_MESSAGE_NEW_CLOCK:
			{
				GstClock *clock = NULL;
				gst_message_parse_new_clock (msg, &clock);
				debug_log("GST_MESSAGE_NEW_CLOCK : %s\n", (clock ? GST_OBJECT_NAME (clock) : "NULL"));
			}
			break;

		case GST_MESSAGE_STRUCTURE_CHANGE:	debug_log("GST_MESSAGE_STRUCTURE_CHANGE\n"); break;
		case GST_MESSAGE_STREAM_STATUS:		debug_log("GST_MESSAGE_STREAM_STATUS\n"); break;
		case GST_MESSAGE_APPLICATION:		debug_log("GST_MESSAGE_APPLICATION\n"); break;

		case GST_MESSAGE_ELEMENT:
		{
			debug_log("GST_MESSAGE_ELEMENT\n");
		}
		break;

		case GST_MESSAGE_SEGMENT_START:		debug_log("GST_MESSAGE_SEGMENT_START\n"); break;
		case GST_MESSAGE_SEGMENT_DONE:		debug_log("GST_MESSAGE_SEGMENT_DONE\n"); break;

		case GST_MESSAGE_DURATION:
		{
			debug_log("GST_MESSAGE_DURATION\n");
			ret = __mmplayer_gst_handle_duration(player, msg);
			if (!ret)
			{
				debug_warning("failed to update duration");
			}
		}

		break;


		case GST_MESSAGE_LATENCY:				debug_log("GST_MESSAGE_LATENCY\n"); break;
		case GST_MESSAGE_ASYNC_START:		debug_log("GST_MESSAGE_ASYNC_DONE : %s\n", gst_element_get_name(GST_MESSAGE_SRC(msg))); break;

		case GST_MESSAGE_ASYNC_DONE:
		{
			debug_log("GST_MESSAGE_ASYNC_DONE : %s\n", gst_element_get_name(GST_MESSAGE_SRC(msg)));

			/* we only handle message from pipeline */
			if (msg->src != (GstObject *)player->pipeline->mainbin[MMPLAYER_M_PIPE].gst)
				break;

			if (player->doing_seek)
			{
				if (MMPLAYER_TARGET_STATE(player) == MM_PLAYER_STATE_PAUSED)
				{
					player->doing_seek = FALSE;
					MMPLAYER_POST_MSG ( player, MM_MESSAGE_SEEK_COMPLETED, NULL );
				}
				else if (MMPLAYER_TARGET_STATE(player) == MM_PLAYER_STATE_PLAYING)
				{
					async_done = TRUE;
				}
			}
		}
		break;

		case GST_MESSAGE_REQUEST_STATE:		debug_log("GST_MESSAGE_REQUEST_STATE\n"); break;
		case GST_MESSAGE_STEP_START:		debug_log("GST_MESSAGE_STEP_START\n"); break;
		case GST_MESSAGE_QOS:					debug_log("GST_MESSAGE_QOS\n"); break;
		case GST_MESSAGE_PROGRESS:			debug_log("GST_MESSAGE_PROGRESS\n"); break;
		case GST_MESSAGE_ANY:				debug_log("GST_MESSAGE_ANY\n"); break;

		default:
			debug_warning("unhandled message\n");
		break;
	}

	/* FIXIT : this cause so many warnings/errors from glib/gstreamer. we should not call it since
	 * gst_element_post_message api takes ownership of the message.
	 */
	//gst_message_unref( msg );

	return ret;
}

gboolean
__mmplayer_gst_handle_duration(mm_player_t* player, GstMessage* msg)
{
	GstFormat format;
	gint64 bytes = 0;

	debug_fenter();

	return_val_if_fail(player, FALSE);
	return_val_if_fail(msg, FALSE);

	gst_message_parse_duration (msg, &format, &bytes);

	if (MMPLAYER_IS_HTTP_STREAMING(player) && format == GST_FORMAT_BYTES )
	{
		debug_log("data total size of http content: %lld", bytes);
		player->http_content_size = bytes;
	}
	else if (format == GST_FORMAT_TIME)
	{
		/* handling audio clip which has vbr. means duration is keep changing */
		_mmplayer_update_content_attrs (player, ATTR_DURATION );
	}
	else
	{
		debug_warning("duration is neither BYTES or TIME");
		return FALSE;
	}

	debug_fleave();

	return TRUE;
}

gboolean
__mmplayer_gst_extract_tag_from_msg(mm_player_t* player, GstMessage* msg) // @
{

/* macro for better code readability */
#define MMPLAYER_UPDATE_TAG_STRING(gsttag, attribute, playertag) \
if (gst_tag_list_get_string(tag_list, gsttag, &string)) \
{\
	if (string != NULL)\
	{\
		debug_log ( "update tag string : %s\n", string); \
		mm_attrs_set_string_by_name(attribute, playertag, string); \
		g_free(string);\
		string = NULL;\
	}\
}

#define MMPLAYER_UPDATE_TAG_IMAGE(gsttag, attribute, playertag) \
value = gst_tag_list_get_value_index(tag_list, gsttag, index); \
if (value) \
{\
	GstMapInfo info; \
	gst_buffer_map (buffer, &info, GST_MAP_WRITE); \
	buffer = gst_value_get_buffer (value); \
	debug_log ( "update album cover data : %p, size : %d\n", info.data, info.size); \
	player->album_art = (gchar *)g_malloc(info.size); \
	if (player->album_art); \
	{ \
		memcpy(player->album_art, info.data, info.size); \
		mm_attrs_set_data_by_name(attribute, playertag, (void *)player->album_art, info.size); \
	} \
gst_buffer_unmap (buffer, &info); \
}

#define MMPLAYER_UPDATE_TAG_UINT(gsttag, attribute, playertag) \
if (gst_tag_list_get_uint(tag_list, gsttag, &v_uint))\
{\
	if(v_uint)\
	{\
		if(strcmp(gsttag, GST_TAG_BITRATE) == 0)\
		{\
			if (player->updated_bitrate_count == 0) \
				mm_attrs_set_int_by_name(attribute, "content_audio_bitrate", v_uint); \
			if (player->updated_bitrate_count<MM_PLAYER_STREAM_COUNT_MAX) \
			{\
				player->bitrate[player->updated_bitrate_count] = v_uint;\
				player->total_bitrate += player->bitrate[player->updated_maximum_bitrate_count]; \
				player->updated_bitrate_count++; \
				mm_attrs_set_int_by_name(attribute, playertag, player->total_bitrate);\
				debug_log ( "update bitrate %d[bps] of stream #%d.\n", v_uint, player->updated_bitrate_count);\
			}\
		}\
		else if (strcmp(gsttag, GST_TAG_MAXIMUM_BITRATE))\
		{\
			if (player->updated_maximum_bitrate_count<MM_PLAYER_STREAM_COUNT_MAX) \
			{\
				player->maximum_bitrate[player->updated_maximum_bitrate_count] = v_uint;\
				player->total_maximum_bitrate += player->maximum_bitrate[player->updated_maximum_bitrate_count]; \
				player->updated_maximum_bitrate_count++; \
				mm_attrs_set_int_by_name(attribute, playertag, player->total_maximum_bitrate); \
				debug_log ( "update maximum bitrate %d[bps] of stream #%d\n", v_uint, player->updated_maximum_bitrate_count);\
			}\
		}\
		else\
		{\
			mm_attrs_set_int_by_name(attribute, playertag, v_uint); \
		}\
		v_uint = 0;\
	}\
}

#define MMPLAYER_UPDATE_TAG_DATE(gsttag, attribute, playertag) \
if (gst_tag_list_get_date(tag_list, gsttag, &date))\
{\
	if (date != NULL)\
	{\
		string = g_strdup_printf("%d", g_date_get_year(date));\
		mm_attrs_set_string_by_name(attribute, playertag, string);\
		debug_log ( "metainfo year : %s\n", string);\
		MMPLAYER_FREEIF(string);\
		g_date_free(date);\
	}\
}

#define MMPLAYER_UPDATE_TAG_UINT64(gsttag, attribute, playertag) \
if(gst_tag_list_get_uint64(tag_list, gsttag, &v_uint64))\
{\
	if(v_uint64)\
	{\
		/* FIXIT : don't know how to store date */\
		g_assert(1);\
		v_uint64 = 0;\
	}\
}

#define MMPLAYER_UPDATE_TAG_DOUBLE(gsttag, attribute, playertag) \
if(gst_tag_list_get_double(tag_list, gsttag, &v_double))\
{\
	if(v_double)\
	{\
		/* FIXIT : don't know how to store date */\
		g_assert(1);\
		v_double = 0;\
	}\
}

	/* function start */
	GstTagList* tag_list = NULL;

	MMHandleType attrs = 0;

	char *string = NULL;
	guint v_uint = 0;
	GDate *date = NULL;
	/* album cover */
	GstBuffer *buffer = NULL;
	gint index = 0;
	const GValue *value;

	/* currently not used. but those are needed for above macro */
	//guint64 v_uint64 = 0;
	//gdouble v_double = 0;

	return_val_if_fail( player && msg, FALSE );

	attrs = MMPLAYER_GET_ATTRS(player);

	return_val_if_fail( attrs, FALSE );

	/* get tag list from gst message */
	gst_message_parse_tag(msg, &tag_list);

	/* store tags to player attributes */
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_TITLE, attrs, "tag_title");
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_TITLE_SORTNAME, ?, ?); */
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_ARTIST, attrs, "tag_artist");
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_ARTIST_SORTNAME, ?, ?); */
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_ALBUM, attrs, "tag_album");
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_ALBUM_SORTNAME, ?, ?); */
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_COMPOSER, attrs, "tag_author");
	MMPLAYER_UPDATE_TAG_DATE(GST_TAG_DATE, attrs, "tag_date");
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_GENRE, attrs, "tag_genre");
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_COMMENT, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_EXTENDED_COMMENT, ?, ?); */
	MMPLAYER_UPDATE_TAG_UINT(GST_TAG_TRACK_NUMBER, attrs, "tag_track_num");
	/* MMPLAYER_UPDATE_TAG_UINT(GST_TAG_TRACK_COUNT, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_UINT(GST_TAG_ALBUM_VOLUME_NUMBER, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_UINT(GST_TAG_ALBUM_VOLUME_COUNT, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_LOCATION, ?, ?); */
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_DESCRIPTION, attrs, "tag_description");
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_VERSION, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_ISRC, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_ORGANIZATION, ?, ?); */
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_COPYRIGHT, attrs, "tag_copyright");
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_COPYRIGHT_URI, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_CONTACT, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_LICENSE, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_LICENSE_URI, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_PERFORMER, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_UINT64(GST_TAG_DURATION, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_CODEC, ?, ?); */
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_VIDEO_CODEC, attrs, "content_video_codec");
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_AUDIO_CODEC, attrs, "content_audio_codec");
	MMPLAYER_UPDATE_TAG_UINT(GST_TAG_BITRATE, attrs, "content_bitrate");
	MMPLAYER_UPDATE_TAG_UINT(GST_TAG_MAXIMUM_BITRATE, attrs, "content_max_bitrate");
	MMPLAYER_UPDATE_TAG_IMAGE(GST_TAG_IMAGE, attrs, "tag_album_cover");
	/* MMPLAYER_UPDATE_TAG_UINT(GST_TAG_NOMINAL_BITRATE, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_UINT(GST_TAG_MINIMUM_BITRATE, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_UINT(GST_TAG_SERIAL, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_ENCODER, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_UINT(GST_TAG_ENCODER_VERSION, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_DOUBLE(GST_TAG_TRACK_GAIN, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_DOUBLE(GST_TAG_TRACK_PEAK, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_DOUBLE(GST_TAG_ALBUM_GAIN, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_DOUBLE(GST_TAG_ALBUM_PEAK, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_DOUBLE(GST_TAG_REFERENCE_LEVEL, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_LANGUAGE_CODE, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_DOUBLE(GST_TAG_BEATS_PER_MINUTE, ?, ?); */

	if ( mmf_attrs_commit ( attrs ) )
		debug_error("failed to commit.\n");

	gst_tag_list_free(tag_list);

	return TRUE;
}

void
__mmplayer_gst_rtp_no_more_pads (GstElement *element,  gpointer data)  // @
{
	mm_player_t* player = (mm_player_t*) data;

	debug_fenter();

	/* NOTE : we can remove fakesink here if there's no rtp_dynamic_pad. because whenever
	  * we connect autoplugging element to the pad which is just added to rtspsrc, we increase
	  * num_dynamic_pad. and this is no-more-pad situation which means mo more pad will be added.
	  * So we can say this. if num_dynamic_pad is zero, it must be one of followings

	  * [1] audio and video will be dumped with filesink.
	  * [2] autoplugging is done by just using pad caps.
	  * [3] typefinding has happend in audio but audiosink is created already before no-more-pad signal
	  * and the video will be dumped via filesink.
	  */
	if ( player->num_dynamic_pad == 0 )
	{
		debug_log("it seems pad caps is directely used for autoplugging. removing fakesink now\n");

		if ( ! __mmplayer_gst_remove_fakesink( player,
			&player->pipeline->mainbin[MMPLAYER_M_SRC_FAKESINK]) )
		{
			/* NOTE : __mmplayer_pipeline_complete() can be called several time. because
			 * signaling mechanism ( pad-added, no-more-pad, new-decoded-pad ) from various
			 * source element are not same. To overcome this situation, this function will called
			 * several places and several times. Therefore, this is not an error case.
			 */
			return;
		}
	}

	/* create dot before error-return. for debugging */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-no-more-pad" );

	/* NOTE : if rtspsrc goes to PLAYING before adding it's src pads, a/v sink elements will
	 * not goes to PLAYING. they will just remain in PAUSED state. simply we are giving
	 * PLAYING state again.
	 */
	__mmplayer_gst_set_state(player,
		player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_PLAYING, TRUE, 5000 );

	player->no_more_pad = TRUE;

	debug_fleave();
}

gboolean
__mmplayer_gst_remove_fakesink(mm_player_t* player, MMPlayerGstElement* fakesink) // @
{
	GstElement* parent = NULL;

	return_val_if_fail(player && player->pipeline && fakesink, FALSE);

	/* lock */
	g_mutex_lock( player->fsink_lock );

	if ( ! fakesink->gst )
	{
		goto ERROR;
	}

	/* get parent of fakesink */
	parent = (GstElement*)gst_object_get_parent( (GstObject*)fakesink->gst );
	if ( ! parent )
	{
		debug_log("fakesink already removed\n");
		goto ERROR;
	}

	gst_element_set_locked_state( fakesink->gst, TRUE );

	/* setting the state to NULL never returns async
	 * so no need to wait for completion of state transiton
	 */
	if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state (fakesink->gst, GST_STATE_NULL) )
	{
		debug_error("fakesink state change failure!\n");

		/* FIXIT : should I return here? or try to proceed to next? */
		/* return FALSE; */
	}

	/* remove fakesink from it's parent */
	if ( ! gst_bin_remove( GST_BIN( parent ), fakesink->gst ) )
	{
		debug_error("failed to remove fakesink\n");

		gst_object_unref( parent );

		goto ERROR;
	}

	gst_object_unref( parent );

	debug_log("state-holder removed\n");

	gst_element_set_locked_state( fakesink->gst, FALSE );

	g_mutex_unlock( player->fsink_lock );
	return TRUE;

ERROR:
	if ( fakesink->gst )
	{
		gst_element_set_locked_state( fakesink->gst, FALSE );
	}

	g_mutex_unlock( player->fsink_lock );
	return FALSE;
}

void
__mmplayer_gst_rtp_dynamic_pad (GstElement *element, GstPad *pad, gpointer data) // @
{
	GstPad *sinkpad = NULL;
	GstCaps* caps = NULL;
	GstElement* new_element = NULL;

	mm_player_t* player = (mm_player_t*) data;

	debug_fenter();

	return_if_fail( element && pad );
	return_if_fail(	player &&
					player->pipeline &&
					player->pipeline->mainbin );


	/* payload type is recognizable. increase num_dynamic and wait for sinkbin creation.
	 * num_dynamic_pad will decreased after creating a sinkbin.
	 */
	player->num_dynamic_pad++;
	debug_log("stream count inc : %d\n", player->num_dynamic_pad);

	/* perform autoplugging if dump is disabled */
	if ( PLAYER_INI()->rtsp_do_typefinding )
	{
		/* create typefind */
		new_element = gst_element_factory_make( "typefind", NULL );
		if ( ! new_element )
		{
			debug_error("failed to create typefind\n");
			goto ERROR;
		}

		MMPLAYER_SIGNAL_CONNECT( 	player,
									G_OBJECT(new_element),
									"have-type",
									G_CALLBACK(__mmplayer_typefind_have_type),
									(gpointer)player);

		/* FIXIT : try to remove it */
		player->have_dynamic_pad = FALSE;
	}
	else  /* NOTE : use pad's caps directely. if enabled. what I am assuming is there's no elemnt has dynamic pad */
	{
		debug_log("using pad caps to autopluging instead of doing typefind\n");

		caps = gst_pad_query_caps( pad, NULL );

		MMPLAYER_CHECK_NULL( caps );

		/* clear  previous result*/
		player->have_dynamic_pad = FALSE;

		if ( ! __mmplayer_try_to_plug( player, pad, caps ) )
		{
			debug_error("failed to autoplug for caps : %s\n", gst_caps_to_string( caps ) );
			goto ERROR;
		}

		/* check if there's dynamic pad*/
		if( player->have_dynamic_pad )
		{
			debug_error("using pad caps assums there's no dynamic pad !\n");
			debug_error("try with enalbing rtsp_do_typefinding\n");
			goto ERROR;
		}

		gst_caps_unref( caps );
		caps = NULL;
	}

	/* excute new_element if created*/
	if ( new_element )
	{
		debug_log("adding new element to pipeline\n");

		/* set state to READY before add to bin */
		MMPLAYER_ELEMENT_SET_STATE( new_element, GST_STATE_READY );

		/* add new element to the pipeline */
		if ( FALSE == gst_bin_add( GST_BIN(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst), new_element)  )
		{
			debug_error("failed to add autoplug element to bin\n");
			goto ERROR;
		}

		/* get pad from element */
		sinkpad = gst_element_get_static_pad ( GST_ELEMENT(new_element), "sink" );
		if ( !sinkpad )
		{
			debug_error("failed to get sinkpad from autoplug element\n");
			goto ERROR;
		}

		/* link it */
		if ( GST_PAD_LINK_OK != GST_PAD_LINK(pad, sinkpad) )
		{
			debug_error("failed to link autoplug element\n");
			goto ERROR;
		}

		gst_object_unref (sinkpad);
		sinkpad = NULL;

		/* run. setting PLAYING here since streamming source is live source */
		MMPLAYER_ELEMENT_SET_STATE( new_element, GST_STATE_PLAYING );
	}

	debug_fleave();

	return;

STATE_CHANGE_FAILED:
ERROR:
	/* FIXIT : take care if new_element has already added to pipeline */
	if ( new_element )
		gst_object_unref(GST_OBJECT(new_element));

	if ( sinkpad )
		gst_object_unref(GST_OBJECT(sinkpad));

	if ( caps )
		gst_object_unref(GST_OBJECT(caps));

	/* FIXIT : how to inform this error to MSL ????? */
	/* FIXIT : I think we'd better to use g_idle_add() to destroy pipeline and
	 * then post an error to application
	 */
}

void
__mmplayer_gst_decode_callback(GstElement *decodebin, GstPad *pad, gboolean last, gpointer data) // @
{
	mm_player_t* player = NULL;
	MMHandleType attrs = 0;
	GstElement* pipeline = NULL;
	GstCaps* caps = NULL;
	GstStructure* str = NULL;
	const gchar* name = NULL;
	GstPad* sinkpad = NULL;
	GstElement* sinkbin = NULL;

	/* check handles */
	player = (mm_player_t*) data;

	return_if_fail( decodebin && pad );
	return_if_fail(player && player->pipeline && player->pipeline->mainbin);

	pipeline = player->pipeline->mainbin[MMPLAYER_M_PIPE].gst;

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("cannot get content attribute\n");
		goto ERROR;
	}

	/* get mimetype from caps */
	caps = gst_pad_query_caps( pad, NULL );
	if ( !caps )
	{
		debug_error("cannot get caps from pad.\n");
		goto ERROR;
	}

	str = gst_caps_get_structure( caps, 0 );
	if ( ! str )
	{
		debug_error("cannot get structure from capse.\n");
		goto ERROR;
	}

	name = gst_structure_get_name(str);
	if ( ! name )
	{
		debug_error("cannot get mimetype from structure.\n");
		goto ERROR;
	}

	debug_log("detected mimetype : %s\n", name);

	if (strstr(name, "audio"))
	{
		if (player->pipeline->audiobin == NULL)
		{
			__ta__("__mmplayer_gst_create_audio_pipeline",
				if (MM_ERROR_NONE !=  __mmplayer_gst_create_audio_pipeline(player))
				{
					debug_error("failed to create audiobin. continuing without audio\n");
					goto ERROR;
				}
			)

			sinkbin = player->pipeline->audiobin[MMPLAYER_A_BIN].gst;
			debug_log("creating audiosink bin success\n");
		}
		else
		{
			sinkbin = player->pipeline->audiobin[MMPLAYER_A_BIN].gst;
			debug_log("re-using audiobin\n");
		}

		/* FIXIT : track number shouldn't be hardcoded */
		mm_attrs_set_int_by_name(attrs, "content_audio_track_num", 1);
		player->audiosink_linked  = 1;

		sinkpad = gst_element_get_static_pad( GST_ELEMENT(sinkbin), "sink" );
		if ( !sinkpad )
		{
			debug_error("failed to get pad from sinkbin\n");
			goto ERROR;
		}
	}
	else if (strstr(name, "video"))
	{
		if (player->pipeline->videobin == NULL)
		{
			/* NOTE : not make videobin because application dose not want to play it even though file has video stream. */
			/* get video surface type */
			int surface_type = 0;
			mm_attrs_get_int_by_name (player->attrs, "display_surface_type", &surface_type);

			if (surface_type == MM_DISPLAY_SURFACE_NULL)
			{
				debug_log("not make videobin because it dose not want\n");
				goto ERROR;
			}

			__ta__("__mmplayer_gst_create_video_pipeline",
			if (MM_ERROR_NONE !=  __mmplayer_gst_create_video_pipeline(player, caps, surface_type) )
			{
				debug_error("failed to create videobin. continuing without video\n");
				goto ERROR;
			}
			)

			sinkbin = player->pipeline->videobin[MMPLAYER_V_BIN].gst;
			debug_log("creating videosink bin success\n");
		}
		else
		{
			sinkbin = player->pipeline->videobin[MMPLAYER_V_BIN].gst;
			debug_log("re-using videobin\n");
		}

		/* FIXIT : track number shouldn't be hardcoded */
		mm_attrs_set_int_by_name(attrs, "content_video_track_num", 1);
		player->videosink_linked  = 1;

		sinkpad = gst_element_get_static_pad( GST_ELEMENT(sinkbin), "sink" );
		if ( !sinkpad )
		{
			debug_error("failed to get pad from sinkbin\n");
			goto ERROR;
		}
	}
	else if (strstr(name, "text"))
	{
		if (player->pipeline->textbin == NULL)
		{
			__ta__("__mmplayer_gst_create_text_pipeline",
				if (MM_ERROR_NONE !=  __mmplayer_gst_create_text_pipeline(player))
				{
					debug_error("failed to create textbin. continuing without text\n");
					goto ERROR;
				}
			)

			sinkbin = player->pipeline->textbin[MMPLAYER_T_BIN].gst;
			debug_log("creating textink bin success\n");
		}
		else
		{
			sinkbin = player->pipeline->textbin[MMPLAYER_T_BIN].gst;
			debug_log("re-using textbin\n");
		}

			/* FIXIT : track number shouldn't be hardcoded */
			mm_attrs_set_int_by_name(attrs, "content_text_track_num", 1);

			player->textsink_linked  = 1;
			debug_msg("player->textsink_linked set to 1\n");

		sinkpad = gst_element_get_static_pad( GST_ELEMENT(sinkbin), "text_sink" );
		if ( !sinkpad )
		{
			debug_error("failed to get pad from sinkbin\n");
			goto ERROR;
		}
	}
	else
	{
		debug_warning("unknown type of elementary stream! ignoring it...\n");
		goto ERROR;
	}

	if ( sinkbin )
	{
		/* warm up */
		if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state( sinkbin, GST_STATE_READY ) )
		{
			debug_error("failed to set state(READY) to sinkbin\n");
			goto ERROR;
		}

		/* add */
		if ( FALSE == gst_bin_add( GST_BIN(pipeline), sinkbin ) )
		{
			debug_error("failed to add sinkbin to pipeline\n");
			goto ERROR;
		}

		/* link */
		if ( GST_PAD_LINK_OK != GST_PAD_LINK(pad, sinkpad) )
		{
			debug_error("failed to get pad from sinkbin\n");
			goto ERROR;
		}

		/* run */
		if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state( sinkbin, GST_STATE_PAUSED ) )
		{
			debug_error("failed to set state(PLAYING) to sinkbin\n");
			goto ERROR;
		}

		gst_object_unref( sinkpad );
		sinkpad = NULL;
	}

	debug_log("linking sink bin success\n");

	/* FIXIT : we cannot hold callback for 'no-more-pad' signal because signal was emitted in
	 * streaming task. if the task blocked, then buffer will not flow to the next element
	 * ( autoplugging element ). so this is special hack for streaming. please try to remove it
	 */
	/* dec stream count. we can remove fakesink if it's zero */
	player->num_dynamic_pad--;

	debug_log("stream count dec : %d (num of dynamic pad)\n", player->num_dynamic_pad);

	if ( ( player->no_more_pad ) && ( player->num_dynamic_pad == 0 ) )
	{
		__mmplayer_pipeline_complete( NULL, player );
	}

ERROR:
	if ( caps )
		gst_caps_unref( caps );

	if ( sinkpad )
		gst_object_unref(GST_OBJECT(sinkpad));

	/* flusing out new attributes */
	if (  mmf_attrs_commit ( attrs ) )
	{
		debug_error("failed to comit attributes\n");
	}

	return;
}

int
__mmplayer_gst_element_link_bucket(GList* element_bucket) // @
{
	GList* bucket = element_bucket;
	MMPlayerGstElement* element = NULL;
	MMPlayerGstElement* prv_element = NULL;
	gint successful_link_count = 0;

	debug_fenter();

	return_val_if_fail(element_bucket, -1);

	prv_element = (MMPlayerGstElement*)bucket->data;
	bucket = bucket->next;

	for ( ; bucket; bucket = bucket->next )
	{
		element = (MMPlayerGstElement*)bucket->data;

		if ( element && element->gst )
		{
			if ( GST_ELEMENT_LINK(GST_ELEMENT(prv_element->gst), GST_ELEMENT(element->gst)) )
			{
				debug_log("linking [%s] to [%s] success\n",
					GST_ELEMENT_NAME(GST_ELEMENT(prv_element->gst)),
					GST_ELEMENT_NAME(GST_ELEMENT(element->gst)) );
				successful_link_count ++;
			}
			else
			{
				debug_log("linking [%s] to [%s] failed\n",
					GST_ELEMENT_NAME(GST_ELEMENT(prv_element->gst)),
					GST_ELEMENT_NAME(GST_ELEMENT(element->gst)) );
				return -1;
			}
		}

		prv_element = element;
	}

	debug_fleave();

	return successful_link_count;
}

int
__mmplayer_gst_element_add_bucket_to_bin(GstBin* bin, GList* element_bucket) // @
{
	GList* bucket = element_bucket;
	MMPlayerGstElement* element = NULL;
	int successful_add_count = 0;

	debug_fenter();

	return_val_if_fail(element_bucket, 0);
	return_val_if_fail(bin, 0);

	for ( ; bucket; bucket = bucket->next )
	{
		element = (MMPlayerGstElement*)bucket->data;

		if ( element && element->gst )
		{
			if( !gst_bin_add(bin, GST_ELEMENT(element->gst)) )
			{
				debug_log("__mmplayer_gst_element_link_bucket : Adding element [%s]  to bin [%s] failed\n",
					GST_ELEMENT_NAME(GST_ELEMENT(element->gst)),
					GST_ELEMENT_NAME(GST_ELEMENT(bin) ) );
				return 0;
			}
			successful_add_count ++;
		}
	}

	debug_fleave();

	return successful_add_count;
}

/**
 * This function is to create audio pipeline for playing.
 *
 * @param	player		[in]	handle of player
 *
 * @return	This function returns zero on success.
 * @remark
 * @see		__mmplayer_gst_create_midi_pipeline, __mmplayer_gst_create_video_pipeline
 */
#define MMPLAYER_CREATEONLY_ELEMENT(x_bin, x_id, x_factory, x_name) \
x_bin[x_id].id = x_id;\
x_bin[x_id].gst = gst_element_factory_make(x_factory, x_name);\
if ( ! x_bin[x_id].gst )\
{\
	debug_critical("failed to create %s \n", x_factory);\
	goto ERROR;\
}\

#define MMPLAYER_CREATE_ELEMENT_ADD_BIN(x_bin, x_id, x_factory, x_name, y_bin) \
x_bin[x_id].id = x_id;\
x_bin[x_id].gst = gst_element_factory_make(x_factory, x_name);\
if ( ! x_bin[x_id].gst )\
{\
	debug_critical("failed to create %s \n", x_factory);\
	goto ERROR;\
}\
if( !gst_bin_add(GST_BIN(y_bin), GST_ELEMENT(x_bin[x_id].gst)))\
{\
	debug_log("__mmplayer_gst_element_link_bucket : Adding element [%s]  to bin [%s] failed\n",\
		GST_ELEMENT_NAME(GST_ELEMENT(x_bin[x_id].gst)),\
		GST_ELEMENT_NAME(GST_ELEMENT(y_bin) ) );\
	goto ERROR;\
}\

/* macro for code readability. just for sinkbin-creation functions */
#define MMPLAYER_CREATE_ELEMENT(x_bin, x_id, x_factory, x_name, x_add_bucket) \
do \
{ \
	x_bin[x_id].id = x_id;\
	x_bin[x_id].gst = gst_element_factory_make(x_factory, x_name);\
	if ( ! x_bin[x_id].gst )\
	{\
		debug_critical("failed to create %s \n", x_factory);\
		goto ERROR;\
	}\
	if ( x_add_bucket )\
		element_bucket = g_list_append(element_bucket, &x_bin[x_id]);\
} while(0);

/**
  * AUDIO PIPELINE
  * - Local playback 	: audioconvert !volume ! capsfilter ! audioeq ! audiosink
  * - Streaming 		: audioconvert !volume ! audiosink
  * - PCM extraction 	: audioconvert ! audioresample ! capsfilter ! fakesink
  */
int
__mmplayer_gst_create_audio_pipeline(mm_player_t* player)
{
	MMPlayerGstElement* first_element = NULL;
	MMPlayerGstElement* audiobin = NULL;
	MMHandleType attrs = 0;
	GstPad *pad = NULL;
	GstPad *ghostpad = NULL;
	GList* element_bucket = NULL;
	char *device_name = NULL;
	gboolean link_audio_sink_now = TRUE;
	int i =0;

	debug_fenter();

	return_val_if_fail( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* alloc handles */
	audiobin = (MMPlayerGstElement*)g_malloc0(sizeof(MMPlayerGstElement) * MMPLAYER_A_NUM);
	if ( ! audiobin )
	{
		debug_error("failed to allocate memory for audiobin\n");
		return MM_ERROR_PLAYER_NO_FREE_SPACE;
	}

	attrs = MMPLAYER_GET_ATTRS(player);

	/* create bin */
	audiobin[MMPLAYER_A_BIN].id = MMPLAYER_A_BIN;
	audiobin[MMPLAYER_A_BIN].gst = gst_bin_new("audiobin");
	if ( !audiobin[MMPLAYER_A_BIN].gst )
	{
		debug_critical("failed to create audiobin\n");
		goto ERROR;
	}

	/* take it */
	player->pipeline->audiobin = audiobin;

	player->is_sound_extraction = __mmplayer_can_extract_pcm(player);

	/* Adding audiotp plugin for reverse trickplay feature */
	MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_TP, "audiotp", "audiotrickplay", TRUE);

	/* converter */
	MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_CONV, "audioconvert", "audioconverter", TRUE);

	/* resampler */
	MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_RESAMPLER, "audioresample", "resampler", TRUE);

	if ( ! player->is_sound_extraction )
	{
		GstCaps* caps = NULL;
		gint channels = 0;

		/* for logical volume control */
		MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_VOL, "volume", "volume", TRUE);
		g_object_set(G_OBJECT (audiobin[MMPLAYER_A_VOL].gst), "volume", player->sound.volume, NULL);

		if (player->sound.mute)
		{
			debug_log("mute enabled\n");
			g_object_set(G_OBJECT (audiobin[MMPLAYER_A_VOL].gst), "mute", player->sound.mute, NULL);
		}

		/*capsfilter */
		MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_CAPS_DEFAULT, "capsfilter", "audiocapsfilter", TRUE);

        caps = gst_caps_from_string( "audio/x-raw,"
                             "format = (string)S16LE,"
                             "layout = (string)interleaved" );

		g_object_set (GST_ELEMENT(audiobin[MMPLAYER_A_CAPS_DEFAULT].gst), "caps", caps, NULL );

		gst_caps_unref( caps );

		/* chech if multi-chennels */
		if (player->pipeline->mainbin && player->pipeline->mainbin[MMPLAYER_M_DEMUX].gst)
		{
			GstPad *srcpad = NULL;
			GstCaps *caps = NULL;

			if ((srcpad = gst_element_get_static_pad(player->pipeline->mainbin[MMPLAYER_M_DEMUX].gst, "src")))
			{
				if ((caps = gst_pad_query_caps(srcpad,NULL)))
				{
					MMPLAYER_LOG_GST_CAPS_TYPE(caps);
					GstStructure *str = gst_caps_get_structure(caps, 0);
					if (str)
						gst_structure_get_int (str, "channels", &channels);
					gst_caps_unref(caps);
				}
				gst_object_unref(srcpad);
			}
		}

		/* audio effect element. if audio effect is enabled */
		if ( channels <= 2 && (PLAYER_INI()->use_audio_effect_preset || PLAYER_INI()->use_audio_effect_custom) )
		{
			MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_FILTER, PLAYER_INI()->name_of_audio_effect, "audiofilter", TRUE);
		}

		/* create audio sink */
		MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_SINK, PLAYER_INI()->name_of_audiosink,
			"audiosink", link_audio_sink_now);

		/* sync on */
		if (MMPLAYER_IS_RTSP_STREAMING(player))
			g_object_set (G_OBJECT (audiobin[MMPLAYER_A_SINK].gst), "sync", FALSE, NULL);	/* sync off */
		else
			g_object_set (G_OBJECT (audiobin[MMPLAYER_A_SINK].gst), "sync", TRUE, NULL);	/* sync on */

		/* qos on */
		g_object_set (G_OBJECT (audiobin[MMPLAYER_A_SINK].gst), "qos", TRUE, NULL); 	/* qos on */

		/* FIXIT : using system clock. isn't there another way? */
		g_object_set (G_OBJECT (audiobin[MMPLAYER_A_SINK].gst), "provide-clock", PLAYER_INI()->provide_clock,  NULL);

		__mmplayer_add_sink( player, audiobin[MMPLAYER_A_SINK].gst );

			if(player->audio_buffer_cb)
			{
				g_object_set(audiobin[MMPLAYER_A_SINK].gst, "audio-handle", player->audio_buffer_cb_user_param, NULL);
				g_object_set(audiobin[MMPLAYER_A_SINK].gst, "audio-callback", player->audio_buffer_cb, NULL);
			}

		if ( g_strrstr(PLAYER_INI()->name_of_audiosink, "avsysaudiosink") )
		{
			gint volume_type = 0;
			gint audio_route = 0;
			gint sound_priority = FALSE;
			gint is_spk_out_only = 0;
			gint latency_mode = 0;

			/* set volume table
			 * It should be set after player creation through attribute.
			 * But, it can not be changed during playing.
			 */
			mm_attrs_get_int_by_name(attrs, "sound_volume_type", &volume_type);
			mm_attrs_get_int_by_name(attrs, "sound_route", &audio_route);
			mm_attrs_get_int_by_name(attrs, "sound_priority", &sound_priority);
			mm_attrs_get_int_by_name(attrs, "sound_spk_out_only", &is_spk_out_only);
			mm_attrs_get_int_by_name(attrs, "audio_latency_mode", &latency_mode);

			/* hook sound_type if emergency case */
			if ( player->sm.event == ASM_EVENT_EMERGENCY)
			{
				debug_log ("This is emergency session, hook sound_type from [%d] to [%d]\n", volume_type, MM_SOUND_VOLUME_TYPE_EMERGENCY);
				volume_type = MM_SOUND_VOLUME_TYPE_EMERGENCY;
			}

			g_object_set(audiobin[MMPLAYER_A_SINK].gst,
					"volumetype", volume_type,
					"audio-route", audio_route,
					"priority", sound_priority,
					"user-route", is_spk_out_only,
					"latency", latency_mode,
					NULL);

			debug_log("audiosink property status...volume type:%d, route:%d, priority=%d, user-route=%d, latency=%d\n",
				volume_type, audio_route, sound_priority, is_spk_out_only, latency_mode);
		}

		/* Antishock can be enabled when player is resumed by soundCM.
		 * But, it's not used in MMS, setting and etc.
		 * Because, player start seems like late.
		 */
		__mmplayer_set_antishock( player , FALSE );
	}
	else // pcm extraction only and no sound output
	{
		int dst_samplerate = 0;
		int dst_channels = 0;
		int dst_depth = 0;
		char *caps_type = NULL;
		GstCaps* caps = NULL;

		/* get conf. values */
		mm_attrs_multiple_get(player->attrs,
					NULL,
					"pcm_extraction_samplerate", &dst_samplerate,
					"pcm_extraction_channels", &dst_channels,
					"pcm_extraction_depth", &dst_depth,
					NULL);
		/* capsfilter */
		MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_CAPS_DEFAULT, "capsfilter", "audiocapsfilter", TRUE);

		caps = gst_caps_new_simple ("audio/x-raw",
						"format", G_TYPE_STRING, "S16LE",
						"rate", G_TYPE_INT, dst_samplerate,
						"channels", G_TYPE_INT, dst_channels,
						"layout", G_TYPE_STRING, "interleaved",
						NULL);

		caps_type = gst_caps_to_string(caps);
		debug_log("resampler new caps : %s\n", caps_type);

		g_object_set (GST_ELEMENT(audiobin[MMPLAYER_A_CAPS_DEFAULT].gst), "caps", caps, NULL );

		/* clean */
		gst_caps_unref( caps );
		MMPLAYER_FREEIF( caps_type );

		/* fake sink */
		MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_SINK, "fakesink", "fakesink", TRUE);

		/* set sync */
		g_object_set (G_OBJECT (audiobin[MMPLAYER_A_SINK].gst), "sync", FALSE, NULL);

		__mmplayer_add_sink( player, audiobin[MMPLAYER_A_SINK].gst );
	}

	/* adding created elements to bin */
	debug_log("adding created elements to bin\n");
	if( !__mmplayer_gst_element_add_bucket_to_bin( GST_BIN(audiobin[MMPLAYER_A_BIN].gst), element_bucket ))
	{
		debug_error("failed to add elements\n");
		goto ERROR;
	}

	/* linking elements in the bucket by added order. */
	debug_log("Linking elements in the bucket by added order.\n");
	if ( __mmplayer_gst_element_link_bucket(element_bucket) == -1 )
	{
		debug_error("failed to link elements\n");
		goto ERROR;
	}

		/* get first element's sinkpad for creating ghostpad */
		first_element = (MMPlayerGstElement *)element_bucket->data;

		pad = gst_element_get_static_pad(GST_ELEMENT(first_element->gst), "sink");
	if ( ! pad )
	{
		debug_error("failed to get pad from first element of audiobin\n");
		goto ERROR;
	}

	ghostpad = gst_ghost_pad_new("sink", pad);
	if ( ! ghostpad )
	{
		debug_error("failed to create ghostpad\n");
		goto ERROR;
	}

	if ( FALSE == gst_element_add_pad(audiobin[MMPLAYER_A_BIN].gst, ghostpad) )
	{
		debug_error("failed to add ghostpad to audiobin\n");
		goto ERROR;
	}

	gst_object_unref(pad);

	if ( !player->bypass_audio_effect && (PLAYER_INI()->use_audio_effect_preset || PLAYER_INI()->use_audio_effect_custom) )
	{
		if ( player->audio_effect_info.effect_type == MM_AUDIO_EFFECT_TYPE_PRESET )
		{
			if (!_mmplayer_audio_effect_preset_apply(player, player->audio_effect_info.preset))
			{
				debug_msg("apply audio effect(preset:%d) setting success\n",player->audio_effect_info.preset);
			}
		}
		else if ( player->audio_effect_info.effect_type == MM_AUDIO_EFFECT_TYPE_CUSTOM )
		{
			if (!_mmplayer_audio_effect_custom_apply(player))
			{
				debug_msg("apply audio effect(custom) setting success\n");
			}
		}
	}

	/* done. free allocated variables */
	MMPLAYER_FREEIF( device_name );
	g_list_free(element_bucket);

	mm_attrs_set_int_by_name(attrs, "content_audio_found", TRUE);

	debug_fleave();

	return MM_ERROR_NONE;

ERROR:

	debug_log("ERROR : releasing audiobin\n");

	MMPLAYER_FREEIF( device_name );

	if ( pad )
		gst_object_unref(GST_OBJECT(pad));

	if ( ghostpad )
		gst_object_unref(GST_OBJECT(ghostpad));

	g_list_free( element_bucket );


	/* release element which are not added to bin */
	for ( i = 1; i < MMPLAYER_A_NUM; i++ )	/* NOTE : skip bin */
	{
		if ( audiobin[i].gst )
		{
			GstObject* parent = NULL;
			parent = gst_element_get_parent( audiobin[i].gst );

			if ( !parent )
			{
				gst_object_unref(GST_OBJECT(audiobin[i].gst));
				audiobin[i].gst = NULL;
			}
			else
			{
				gst_object_unref(GST_OBJECT(parent));
			}
		}
	}

	/* release audiobin with it's childs */
	if ( audiobin[MMPLAYER_A_BIN].gst )
	{
		gst_object_unref(GST_OBJECT(audiobin[MMPLAYER_A_BIN].gst));
	}

	MMPLAYER_FREEIF( audiobin );

	player->pipeline->audiobin = NULL;

	return MM_ERROR_PLAYER_INTERNAL;
}

/**
 * This function is to create video pipeline.
 *
 * @param	player		[in]	handle of player
 *		caps 		[in]	src caps of decoder
 *		surface_type	[in]	surface type for video rendering
 *
 * @return	This function returns zero on success.
 * @remark
 * @see		__mmplayer_gst_create_audio_pipeline, __mmplayer_gst_create_midi_pipeline
 */
/**
  * VIDEO PIPELINE
  * - x surface (arm/x86) : videoflip ! xvimagesink
  * - evas surface  (arm) : fimcconvert ! evasimagesink
  * - evas surface  (x86) : videoconvertor ! videoflip ! evasimagesink
  */
int
__mmplayer_gst_create_video_pipeline(mm_player_t* player, GstCaps* caps, MMDisplaySurfaceType surface_type)
{
	GstPad *pad = NULL;
	MMHandleType attrs;
	GList*element_bucket = NULL;
	MMPlayerGstElement* first_element = NULL;
	MMPlayerGstElement* videobin = NULL;
	gchar* vconv_factory = NULL;
	gchar *videosink_element = NULL;

	debug_fenter();

	return_val_if_fail(player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED);

	/* alloc handles */
	videobin = (MMPlayerGstElement*)g_malloc0(sizeof(MMPlayerGstElement) * MMPLAYER_V_NUM);
	if ( !videobin )
	{
		return MM_ERROR_PLAYER_NO_FREE_SPACE;
	}

	player->pipeline->videobin = videobin;

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("cannot get content attribute");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	/* create bin */
	videobin[MMPLAYER_V_BIN].id = MMPLAYER_V_BIN;
	videobin[MMPLAYER_V_BIN].gst = gst_bin_new("videobin");
	if ( !videobin[MMPLAYER_V_BIN].gst )
	{
		debug_critical("failed to create videobin");
		goto ERROR;
	}

	if( player->use_video_stream ) // video stream callback, so send raw video data to application
	{
		GstStructure *str = NULL;
		gint ret = 0;

		debug_log("using memsink\n");

		/* first, create colorspace convert */
		if (player->is_nv12_tiled)
		{
			vconv_factory = "fimcconvert";
		}
		else // get video converter from player ini file
		{
			if (strlen(PLAYER_INI()->name_of_video_converter) > 0)
			{
				vconv_factory = PLAYER_INI()->name_of_video_converter;
			}
		}

		if (vconv_factory)
		{
			MMPLAYER_CREATE_ELEMENT(videobin, MMPLAYER_V_CONV, vconv_factory, "video converter", TRUE);
		}

		if ( !player->is_nv12_tiled)
		{
			gint width = 0;		//width of video
			gint height = 0;		//height of video
			GstCaps* video_caps = NULL;

			/* rotator, scaler and capsfilter */
            if (strncmp(PLAYER_INI()->videosink_element_x, "vaapisink", strlen("vaapisink"))){
			MMPLAYER_CREATE_ELEMENT(videobin, MMPLAYER_V_FLIP, "videoflip", "video rotator", TRUE);
			MMPLAYER_CREATE_ELEMENT(videobin, MMPLAYER_V_SCALE, "videoscale", "video scaler", TRUE);
			MMPLAYER_CREATE_ELEMENT(videobin, MMPLAYER_V_CAPS, "capsfilter", "videocapsfilter", TRUE);
            }

			/* get video stream caps parsed by demuxer */
			str = gst_caps_get_structure (player->v_stream_caps, 0);
			if ( !str )
			{
				debug_error("cannot get structure");
				goto ERROR;
			}

			mm_attrs_get_int_by_name(attrs, "display_width", &width);
			mm_attrs_get_int_by_name(attrs, "display_height", &height);
			if (!width || !height) {
				/* we set width/height of original media's size  to capsfilter for scaling video */
				ret = gst_structure_get_int (str, "width", &width);
				if ( !ret )
				{
					debug_error("cannot get width");
					goto ERROR;
				}

				ret = gst_structure_get_int(str, "height", &height);
				if ( !ret )
				{
					debug_error("cannot get height");
					goto ERROR;
				}
			}

            video_caps = gst_caps_new_simple( "video/x-raw",
											"width", G_TYPE_INT, width,
											"height", G_TYPE_INT, height,
											NULL);

			g_object_set (GST_ELEMENT(videobin[MMPLAYER_V_CAPS].gst), "caps", video_caps, NULL );

			gst_caps_unref( video_caps );
		}

		/* finally, create video sink. output will be BGRA8888. */
		MMPLAYER_CREATE_ELEMENT(videobin, MMPLAYER_V_SINK, "avsysmemsink", "videosink", TRUE);

		MMPLAYER_SIGNAL_CONNECT( player,
									 videobin[MMPLAYER_V_SINK].gst,
									 "video-stream",
									 G_CALLBACK(__mmplayer_videostream_cb),
									 player );
	}
	else // render video data using sink plugin like xvimagesink
	{
		debug_log("using videosink");

		/* set video converter */
		if (strlen(PLAYER_INI()->name_of_video_converter) > 0)
		{
			vconv_factory = PLAYER_INI()->name_of_video_converter;

			if ( (player->is_nv12_tiled && (surface_type == MM_DISPLAY_SURFACE_EVAS) &&
				!strcmp(PLAYER_INI()->videosink_element_evas, "evasimagesink") ) )
			{
				vconv_factory = "fimcconvert";
			}
			else if (player->is_nv12_tiled)
			{
				vconv_factory = NULL;
			}

			if (vconv_factory)
			{
				MMPLAYER_CREATE_ELEMENT(videobin, MMPLAYER_V_CONV, vconv_factory, "video converter", TRUE);
				debug_log("using video converter: %s", vconv_factory);
			}
		}

        if (strncmp(PLAYER_INI()->videosink_element_x,"vaapisink", strlen("vaapisink"))){
		/* set video rotator */
		if ( !player->is_nv12_tiled )
			MMPLAYER_CREATE_ELEMENT(videobin, MMPLAYER_V_FLIP, "videoflip", "video rotator", TRUE);

		/* videoscaler */
		#if !defined(__arm__)
		MMPLAYER_CREATE_ELEMENT(videobin, MMPLAYER_V_SCALE, "videoscale", "videoscaler", TRUE);
		#endif
        }

		/* set video sink */
		switch (surface_type)
		{
		case MM_DISPLAY_SURFACE_X:
			if (strlen(PLAYER_INI()->videosink_element_x) > 0)
				videosink_element = PLAYER_INI()->videosink_element_x;
			else
				goto ERROR;
			break;
		case MM_DISPLAY_SURFACE_EVAS:
			if (strlen(PLAYER_INI()->videosink_element_evas) > 0)
				videosink_element = PLAYER_INI()->videosink_element_evas;
			else
				goto ERROR;
			break;
		case MM_DISPLAY_SURFACE_X_EXT:
		{
			void *pixmap_id_cb = NULL;
			mm_attrs_get_data_by_name(attrs, "display_overlay", &pixmap_id_cb);
			if (pixmap_id_cb) /* this is used for the videoTextue(canvasTexture) overlay */
			{
				videosink_element = PLAYER_INI()->videosink_element_x;
			}
			else
			{
				debug_error("something wrong.. callback function for getting pixmap id is null");
				goto ERROR;
			}
			break;
		}
		case MM_DISPLAY_SURFACE_NULL:
			if (strlen(PLAYER_INI()->videosink_element_fake) > 0)
				videosink_element = PLAYER_INI()->videosink_element_fake;
			else
				goto ERROR;
			break;
		default:
			debug_error("unidentified surface type");
			goto ERROR;
		}

		MMPLAYER_CREATE_ELEMENT(videobin, MMPLAYER_V_SINK, videosink_element, videosink_element, TRUE);
		debug_log("selected videosink name: %s", videosink_element);

		/* connect signal handlers for sink plug-in */
		switch (surface_type) {
		case MM_DISPLAY_SURFACE_X_EXT:
			MMPLAYER_SIGNAL_CONNECT( player,
									player->pipeline->videobin[MMPLAYER_V_SINK].gst,
									"frame-render-error",
									G_CALLBACK(__mmplayer_videoframe_render_error_cb),
									player );
			debug_log("videoTexture usage, connect a signal handler for pixmap rendering error");
			break;
		default:
			break;
		}
	}

	if ( _mmplayer_update_video_param(player) != MM_ERROR_NONE)
		goto ERROR;

	/* qos on */
	g_object_set (G_OBJECT (videobin[MMPLAYER_V_SINK].gst), "qos", TRUE, NULL);

	/* store it as it's sink element */
	__mmplayer_add_sink( player, videobin[MMPLAYER_V_SINK].gst );

	/* adding created elements to bin */
	if( ! __mmplayer_gst_element_add_bucket_to_bin(GST_BIN(videobin[MMPLAYER_V_BIN].gst), element_bucket) )
	{
		debug_error("failed to add elements\n");
		goto ERROR;
	}

	/* Linking elements in the bucket by added order */
	if ( __mmplayer_gst_element_link_bucket(element_bucket) == -1 )
	{
		debug_error("failed to link elements\n");
		goto ERROR;
	}

	/* get first element's sinkpad for creating ghostpad */
	first_element = (MMPlayerGstElement *)element_bucket->data;
	if ( !first_element )
	{
		debug_error("failed to get first element from bucket\n");
		goto ERROR;
	}

	pad = gst_element_get_static_pad(GST_ELEMENT(first_element->gst), "sink");
	if ( !pad )
	{
		debug_error("failed to get pad from first element\n");
		goto ERROR;
	}

	/* create ghostpad */
	if (FALSE == gst_element_add_pad(videobin[MMPLAYER_V_BIN].gst, gst_ghost_pad_new("sink", pad)))
	{
		debug_error("failed to add ghostpad to videobin\n");
		goto ERROR;
	}
	gst_object_unref(pad);

	/* done. free allocated variables */
	g_list_free(element_bucket);

	mm_attrs_set_int_by_name(attrs, "content_video_found", TRUE);

	debug_fleave();

	return MM_ERROR_NONE;

ERROR:
	debug_error("ERROR : releasing videobin\n");

	g_list_free( element_bucket );

	if (pad)
		gst_object_unref(GST_OBJECT(pad));

	/* release videobin with it's childs */
	if ( videobin[MMPLAYER_V_BIN].gst )
	{
		gst_object_unref(GST_OBJECT(videobin[MMPLAYER_V_BIN].gst));
	}


	MMPLAYER_FREEIF( videobin );

	player->pipeline->videobin = NULL;

	return MM_ERROR_PLAYER_INTERNAL;
}

int __mmplayer_gst_create_text_pipeline(mm_player_t* player)
{
	MMPlayerGstElement* textbin = NULL;
	GList* element_bucket = NULL;
	GstPad *pad = NULL;
	GstPad *ghostpad = NULL;
	gint i = 0;

	debug_fenter();

	return_val_if_fail( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* alloc handles */
	textbin = (MMPlayerGstElement*)g_malloc0(sizeof(MMPlayerGstElement) * MMPLAYER_T_NUM);
	if ( ! textbin )
	{
		debug_error("failed to allocate memory for textbin\n");
		return MM_ERROR_PLAYER_NO_FREE_SPACE;
	}

	/* create bin */
	textbin[MMPLAYER_T_BIN].id = MMPLAYER_T_BIN;
	textbin[MMPLAYER_T_BIN].gst = gst_bin_new("textbin");
	if ( !textbin[MMPLAYER_T_BIN].gst )
	{
		debug_critical("failed to create textbin\n");
		goto ERROR;
	}

	/* take it */
	player->pipeline->textbin = textbin;

	/* fakesink */
	if (player->use_textoverlay)
	{
		debug_log ("use textoverlay for displaying \n");

		MMPLAYER_CREATE_ELEMENT_ADD_BIN(textbin, MMPLAYER_T_TEXT_QUEUE, "queue", "text_t_queue", textbin[MMPLAYER_T_BIN].gst);

		MMPLAYER_CREATE_ELEMENT_ADD_BIN(textbin, MMPLAYER_T_VIDEO_QUEUE, "queue", "text_v_queue", textbin[MMPLAYER_T_BIN].gst);

		MMPLAYER_CREATE_ELEMENT_ADD_BIN(textbin, MMPLAYER_T_VIDEO_CONVERTER, "fimcconvert", "text_v_converter", textbin[MMPLAYER_T_BIN].gst);

		MMPLAYER_CREATE_ELEMENT_ADD_BIN(textbin, MMPLAYER_T_OVERLAY, "textoverlay", "text_overlay", textbin[MMPLAYER_T_BIN].gst);

		if (!gst_element_link_pads (textbin[MMPLAYER_T_VIDEO_QUEUE].gst, "src", textbin[MMPLAYER_T_VIDEO_CONVERTER].gst, "sink"))
		{
			debug_error("failed to link queue and converter\n");
			goto ERROR;
		}

		if (!gst_element_link_pads (textbin[MMPLAYER_T_VIDEO_CONVERTER].gst, "src", textbin[MMPLAYER_T_OVERLAY].gst, "video_sink"))
		{
			debug_error("failed to link queue and textoverlay\n");
			goto ERROR;
		}

		if (!gst_element_link_pads (textbin[MMPLAYER_T_TEXT_QUEUE].gst, "src", textbin[MMPLAYER_T_OVERLAY].gst, "text_sink"))
		{
			debug_error("failed to link queue and textoverlay\n");
			goto ERROR;
		}

	}
	else
	{
		debug_log ("use subtitle message for displaying \n");

		MMPLAYER_CREATE_ELEMENT(textbin, MMPLAYER_T_TEXT_QUEUE, "queue", "text_queue", TRUE);

		MMPLAYER_CREATE_ELEMENT(textbin, MMPLAYER_T_SINK, "fakesink", "text_sink", TRUE);

		g_object_set (G_OBJECT (textbin[MMPLAYER_T_SINK].gst), "sync", TRUE, NULL);
		g_object_set (G_OBJECT (textbin[MMPLAYER_T_SINK].gst), "async", FALSE, NULL);
		g_object_set (G_OBJECT (textbin[MMPLAYER_T_SINK].gst), "signal-handoffs", TRUE, NULL);

		MMPLAYER_SIGNAL_CONNECT( player,
								G_OBJECT(textbin[MMPLAYER_T_SINK].gst),
								"handoff",
								G_CALLBACK(__mmplayer_update_subtitle),
								(gpointer)player );

		if (!player->play_subtitle)
		{
			debug_log ("add textbin sink as sink element of whole pipeline.\n");
			__mmplayer_add_sink (player, GST_ELEMENT(textbin[MMPLAYER_T_SINK].gst));
		}

		/* adding created elements to bin */
		debug_log("adding created elements to bin\n");
		if( !__mmplayer_gst_element_add_bucket_to_bin( GST_BIN(textbin[MMPLAYER_T_BIN].gst), element_bucket ))
		{
			debug_error("failed to add elements\n");
			goto ERROR;
		}

		/* linking elements in the bucket by added order. */
		debug_log("Linking elements in the bucket by added order.\n");
		if ( __mmplayer_gst_element_link_bucket(element_bucket) == -1 )
		{
			debug_error("failed to link elements\n");
			goto ERROR;
		}

		/* done. free allocated variables */
		g_list_free(element_bucket);
	}

	if (textbin[MMPLAYER_T_TEXT_QUEUE].gst)
	{
	    	pad = gst_element_get_static_pad(GST_ELEMENT(textbin[MMPLAYER_T_TEXT_QUEUE].gst), "sink");
		if (!pad)
		{
			debug_error("failed to get text pad of textbin\n");
			goto ERROR;
		}

		ghostpad = gst_ghost_pad_new("text_sink", pad);
		if (!ghostpad)
		{
			debug_error("failed to create ghostpad of textbin\n");
			goto ERROR;
		}

		if ( FALSE == gst_element_add_pad(textbin[MMPLAYER_T_BIN].gst, ghostpad) )
		{
			debug_error("failed to add ghostpad to textbin\n");
			goto ERROR;
		}
	}

	if (textbin[MMPLAYER_T_VIDEO_QUEUE].gst)
	{
	    	pad = gst_element_get_static_pad(GST_ELEMENT(textbin[MMPLAYER_T_VIDEO_QUEUE].gst), "sink");
		if (!pad)
		{
			debug_error("failed to get video pad of textbin\n");
			goto ERROR;
		}

		ghostpad = gst_ghost_pad_new("video_sink", pad);
		if (!ghostpad)
		{
			debug_error("failed to create ghostpad of textbin\n");
			goto ERROR;
		}

		if (!gst_element_add_pad(textbin[MMPLAYER_T_BIN].gst, ghostpad))
		{
			debug_error("failed to add ghostpad to textbin\n");
			goto ERROR;
		}
	}

	if (textbin[MMPLAYER_T_OVERLAY].gst)
	{
	    	pad = gst_element_get_static_pad(GST_ELEMENT(textbin[MMPLAYER_T_OVERLAY].gst), "src");
		if (!pad)
		{
			debug_error("failed to get src pad of textbin\n");
			goto ERROR;
		}

		ghostpad = gst_ghost_pad_new("src", pad);
		if (!ghostpad)
		{
			debug_error("failed to create ghostpad of textbin\n");
			goto ERROR;
		}

		if (!gst_element_add_pad(textbin[MMPLAYER_T_BIN].gst, ghostpad))
		{
			debug_error("failed to add ghostpad to textbin\n");
			goto ERROR;
		}
	}

	gst_object_unref(pad);

	debug_fleave();

	return MM_ERROR_NONE;

ERROR:

	debug_log("ERROR : releasing textbin\n");

	if ( pad )
		gst_object_unref(GST_OBJECT(pad));

	if ( ghostpad )
		gst_object_unref(GST_OBJECT(ghostpad));

	g_list_free( element_bucket );


	/* release element which are not added to bin */
	for ( i = 1; i < MMPLAYER_T_NUM; i++ ) 	/* NOTE : skip bin */
	{
		if ( textbin[i].gst )
		{
			GstObject* parent = NULL;
			parent = gst_element_get_parent( textbin[i].gst );

			if ( !parent )
			{
				gst_object_unref(GST_OBJECT(textbin[i].gst));
				textbin[i].gst = NULL;
			}
			else
			{
				gst_object_unref(GST_OBJECT(parent));
			}
		}
	}

	/* release textbin with it's childs */
	if ( textbin[MMPLAYER_T_BIN].gst )
	{
		gst_object_unref(GST_OBJECT(textbin[MMPLAYER_T_BIN].gst));
	}

	MMPLAYER_FREEIF( textbin );

	player->pipeline->textbin = NULL;

	return MM_ERROR_PLAYER_INTERNAL;
}

int
__mmplayer_gst_create_subtitle_src(mm_player_t* player)
{
	MMPlayerGstElement* mainbin = NULL;
	MMHandleType attrs = 0;
	GstElement * pipeline = NULL;
	GstElement *subsrc = NULL;
	GstElement *subparse = NULL;
	gchar *subtitle_uri =NULL;
	gchar *charset = NULL;

	debug_fenter();

	/* get mainbin */
	return_val_if_fail ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED);

	pipeline = player->pipeline->mainbin[MMPLAYER_M_PIPE].gst;
	mainbin = player->pipeline->mainbin;

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("cannot get content attribute\n");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	mm_attrs_get_string_by_name ( attrs, "subtitle_uri", &subtitle_uri );
	if ( !subtitle_uri || strlen(subtitle_uri) < 1)
	{
		debug_error("subtitle uri is not proper filepath.\n");
		return MM_ERROR_PLAYER_INVALID_URI;
	}
	debug_log("subtitle file path is [%s].\n", subtitle_uri);


	/* create the subtitle source */
	subsrc = gst_element_factory_make("filesrc", "subtitle_source");
	if ( !subsrc )
	{
		debug_error ( "failed to create filesrc element\n" );
		goto ERROR;
	}
	g_object_set(G_OBJECT (subsrc), "location", subtitle_uri, NULL);

	mainbin[MMPLAYER_M_SUBSRC].id = MMPLAYER_M_SUBSRC;
	mainbin[MMPLAYER_M_SUBSRC].gst = subsrc;

	if (!gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), subsrc))
	{
		debug_warning("failed to add queue\n");
		goto ERROR;
	}

	/* subparse */
	subparse = gst_element_factory_make("subparse", "subtitle_parser");
	if ( !subparse )
	{
		debug_error ( "failed to create subparse element\n" );
		goto ERROR;
	}

	charset = util_get_charset(subtitle_uri);
	if (charset)
	{
	    	debug_log ("detected charset is %s\n", charset );
		g_object_set (G_OBJECT (subparse), "subtitle-encoding", charset, NULL);
	}

	mainbin[MMPLAYER_M_SUBPARSE].id = MMPLAYER_M_SUBPARSE;
	mainbin[MMPLAYER_M_SUBPARSE].gst = subparse;

	if (!gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), subparse))
	{
		debug_warning("failed to add subparse\n");
		goto ERROR;
	}

	if (!gst_element_link_pads (subsrc, "src", subparse, "sink"))
	{
		debug_warning("failed to link subsrc and subparse\n");
		goto ERROR;
	}

	player->play_subtitle = TRUE;
	debug_log ("play subtitle using subtitle file\n");

	if (MM_ERROR_NONE !=  __mmplayer_gst_create_text_pipeline(player))
	{
		debug_error("failed to create textbin. continuing without text\n");
		goto ERROR;
	}

	if (!gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), GST_ELEMENT(player->pipeline->textbin[MMPLAYER_T_BIN].gst)))
	{
		debug_warning("failed to add textbin\n");
		goto ERROR;
	}

	if (!gst_element_link_pads (subparse, "src", player->pipeline->textbin[MMPLAYER_T_BIN].gst, "text_sink"))
	{
		debug_warning("failed to link subparse and textbin\n");
		goto ERROR;
	}

	debug_fleave();

	return MM_ERROR_NONE;


ERROR:
	return MM_ERROR_PLAYER_INTERNAL;
}

/**
 * This function is to create  audio or video pipeline for playing.
 *
 * @param	player		[in]	handle of player
 *
 * @return	This function returns zero on success.
 * @remark
 * @see
 */
int
__mmplayer_gst_create_pipeline(mm_player_t* player) // @
{
	GstBus	*bus = NULL;
	MMPlayerGstElement *mainbin = NULL;
	MMHandleType attrs = 0;
	GstElement* element = NULL;
	GList* element_bucket = NULL;
	gboolean need_state_holder = TRUE;
	gint i = 0;

	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	/* get profile attribute */
	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("cannot get content attribute\n");
		goto INIT_ERROR;
	}

	/* create pipeline handles */
	if ( player->pipeline )
	{
		debug_warning("pipeline should be released before create new one\n");
		goto INIT_ERROR;
	}

	player->pipeline = (MMPlayerGstPipelineInfo*) g_malloc0( sizeof(MMPlayerGstPipelineInfo) );
	if (player->pipeline == NULL)
		goto INIT_ERROR;

	memset( player->pipeline, 0, sizeof(MMPlayerGstPipelineInfo) );


	/* create mainbin */
	mainbin = (MMPlayerGstElement*) g_malloc0( sizeof(MMPlayerGstElement) * MMPLAYER_M_NUM );
	if (mainbin == NULL)
		goto INIT_ERROR;

	memset( mainbin, 0, sizeof(MMPlayerGstElement) * MMPLAYER_M_NUM);


	/* create pipeline */
	mainbin[MMPLAYER_M_PIPE].id = MMPLAYER_M_PIPE;
	mainbin[MMPLAYER_M_PIPE].gst = gst_pipeline_new("player");
	if ( ! mainbin[MMPLAYER_M_PIPE].gst )
	{
		debug_error("failed to create pipeline\n");
		goto INIT_ERROR;
	}


	/* create source element */
	switch ( player->profile.uri_type )
	{
		/* rtsp streamming */
		case MM_PLAYER_URI_TYPE_URL_RTSP:
		{
			gint network_bandwidth;
			gchar *user_agent, *wap_profile;

			element = gst_element_factory_make(PLAYER_INI()->name_of_rtspsrc, "streaming_source");

			if ( !element )
			{
				debug_critical("failed to create streaming source element\n");
				break;
			}

			debug_log("using streamming source [%s].\n", PLAYER_INI()->name_of_rtspsrc);

			/* make it zero */
			network_bandwidth = 0;
			user_agent = wap_profile = NULL;

			/* get attribute */
			mm_attrs_get_string_by_name ( attrs, "streaming_user_agent", &user_agent );
			mm_attrs_get_string_by_name ( attrs,"streaming_wap_profile", &wap_profile );
			mm_attrs_get_int_by_name ( attrs, "streaming_network_bandwidth", &network_bandwidth );

			debug_log("setting streaming source ----------------\n");
			debug_log("user_agent : %s\n", user_agent);
			debug_log("wap_profile : %s\n", wap_profile);
			debug_log("network_bandwidth : %d\n", network_bandwidth);
			debug_log("buffering time : %d\n", PLAYER_INI()->rtsp_buffering_time);
			debug_log("rebuffering time : %d\n", PLAYER_INI()->rtsp_rebuffering_time);
			debug_log("-----------------------------------------\n");

			/* setting property to streaming source */
			g_object_set(G_OBJECT(element), "location", player->profile.uri, NULL);
			g_object_set(G_OBJECT(element), "bandwidth", network_bandwidth, NULL);
			g_object_set(G_OBJECT(element), "buffering_time", PLAYER_INI()->rtsp_buffering_time, NULL);
			g_object_set(G_OBJECT(element), "rebuffering_time", PLAYER_INI()->rtsp_rebuffering_time, NULL);
			if ( user_agent )
				g_object_set(G_OBJECT(element), "user_agent", user_agent, NULL);
			if ( wap_profile )
				g_object_set(G_OBJECT(element), "wap_profile", wap_profile, NULL);

			MMPLAYER_SIGNAL_CONNECT ( player, G_OBJECT(element), "pad-added",
				G_CALLBACK (__mmplayer_gst_rtp_dynamic_pad), player );
			MMPLAYER_SIGNAL_CONNECT ( player, G_OBJECT(element), "no-more-pads",
				G_CALLBACK (__mmplayer_gst_rtp_no_more_pads), player );

			player->no_more_pad = FALSE;
			player->num_dynamic_pad = 0;

			/* NOTE : we cannot determine it yet. this filed will be filled by
			 * _mmplayer_update_content_attrs() after START.
			 */
			player->streaming_type = STREAMING_SERVICE_NONE;
		}
		break;

		/* http streaming*/
		case MM_PLAYER_URI_TYPE_URL_HTTP:
		{
			gchar *user_agent, *proxy, *cookies, **cookie_list;
			user_agent = proxy = cookies = NULL;
			cookie_list = NULL;
			gint mode = MM_PLAYER_PD_MODE_NONE;

			mm_attrs_get_int_by_name ( attrs, "pd_mode", &mode );

			player->pd_mode = mode;

			debug_log("http playback, PD mode : %d\n", player->pd_mode);

			if ( ! MMPLAYER_IS_HTTP_PD(player) )
			{
				element = gst_element_factory_make(PLAYER_INI()->name_of_httpsrc, "http_streaming_source");
				if ( !element )
				{
					debug_critical("failed to create http streaming source element[%s].\n", PLAYER_INI()->name_of_httpsrc);
					break;
				}
				debug_log("using http streamming source [%s].\n", PLAYER_INI()->name_of_httpsrc);

				/* get attribute */
				mm_attrs_get_string_by_name ( attrs, "streaming_cookie", &cookies );
				mm_attrs_get_string_by_name ( attrs, "streaming_user_agent", &user_agent );
				mm_attrs_get_string_by_name ( attrs, "streaming_proxy", &proxy );

				/* get attribute */
				debug_log("setting http streaming source ----------------\n");
				debug_log("location : %s\n", player->profile.uri);
				debug_log("cookies : %s\n", cookies);
				debug_log("proxy : %s\n", proxy);
				debug_log("user_agent :  %s\n",  user_agent);
				debug_log("timeout : %d\n",  PLAYER_INI()->http_timeout);
				debug_log("-----------------------------------------\n");

				/* setting property to streaming source */
				g_object_set(G_OBJECT(element), "location", player->profile.uri, NULL);
				g_object_set(G_OBJECT(element), "timeout", PLAYER_INI()->http_timeout, NULL);
				/* check if prosy is vailid or not */
				if ( util_check_valid_url ( proxy ) )
					g_object_set(G_OBJECT(element), "proxy", proxy, NULL);
				/* parsing cookies */
				if ( ( cookie_list = util_get_cookie_list ((const char*)cookies) ) )
					g_object_set(G_OBJECT(element), "cookies", cookie_list, NULL);
				if ( user_agent )
					g_object_set(G_OBJECT(element), "user_agent", user_agent, NULL);
			}
			else // progressive download
			{
				if (player->pd_mode == MM_PLAYER_PD_MODE_URI)
				{
					gchar *path = NULL;

					mm_attrs_get_string_by_name ( attrs, "pd_location", &path );

					MMPLAYER_FREEIF(player->pd_file_save_path);

					debug_log("PD Location : %s\n", path);

					if ( path )
					{
						player->pd_file_save_path = g_strdup(path);
					}
					else
					{
						debug_error("can't find pd location so, it should be set \n");
						return MM_ERROR_PLAYER_FILE_NOT_FOUND;
					}
				}

				element = gst_element_factory_make("pdpushsrc", "PD pushsrc");
				if ( !element )
				{
					debug_critical("failed to create PD push source element[%s].\n", "pdpushsrc");
					break;
				}

				g_object_set(G_OBJECT(element), "location", player->pd_file_save_path, NULL);
			}

			player->streaming_type = STREAMING_SERVICE_NONE;
		}
		break;

		/* file source */
		case MM_PLAYER_URI_TYPE_FILE:
		{
			char* drmsrc = PLAYER_INI()->name_of_drmsrc;

			debug_log("using [%s] for 'file://' handler.\n", drmsrc);

			element = gst_element_factory_make(drmsrc, "source");
			if ( !element )
			{
				debug_critical("failed to create %s\n", drmsrc);
				break;
			}

			g_object_set(G_OBJECT(element), "location", (player->profile.uri)+7, NULL);	/* uri+7 -> remove "file:// */
			//g_object_set(G_OBJECT(element), "use-mmap", TRUE, NULL);
		}
		break;

		/* appsrc */
		case MM_PLAYER_URI_TYPE_BUFF:
		{
			guint64 stream_type = GST_APP_STREAM_TYPE_STREAM;

			debug_log("mem src is selected\n");

			element = gst_element_factory_make("appsrc", "buff-source");
			if ( !element )
			{
				debug_critical("failed to create appsrc element\n");
				break;
			}

			g_object_set( element, "stream-type", stream_type, NULL );
			//g_object_set( element, "size", player->mem_buf.len, NULL );
			//g_object_set( element, "blocksize", (guint64)20480, NULL );

			MMPLAYER_SIGNAL_CONNECT( player, element, "seek-data",
				G_CALLBACK(__gst_appsrc_seek_data), player);
			MMPLAYER_SIGNAL_CONNECT( player, element, "need-data",
				G_CALLBACK(__gst_appsrc_feed_data), player);
			MMPLAYER_SIGNAL_CONNECT( player, element, "enough-data",
				G_CALLBACK(__gst_appsrc_enough_data), player);
		}
		break;

		/* appsrc */
		case MM_PLAYER_URI_TYPE_MEM:
		{
			guint64 stream_type = GST_APP_STREAM_TYPE_RANDOM_ACCESS;

			debug_log("mem src is selected\n");

			element = gst_element_factory_make("appsrc", "mem-source");
			if ( !element )
			{
				debug_critical("failed to create appsrc element\n");
				break;
			}

			g_object_set( element, "stream-type", stream_type, NULL );
			g_object_set( element, "size", player->mem_buf.len, NULL );
			g_object_set( element, "blocksize", (guint64)20480, NULL );

			MMPLAYER_SIGNAL_CONNECT( player, element, "seek-data",
				G_CALLBACK(__gst_appsrc_seek_data_mem), &player->mem_buf );
			MMPLAYER_SIGNAL_CONNECT( player, element, "need-data",
				G_CALLBACK(__gst_appsrc_feed_data_mem), &player->mem_buf );
		}
		break;
		case MM_PLAYER_URI_TYPE_URL:
		break;

		case MM_PLAYER_URI_TYPE_TEMP:
		break;

		case MM_PLAYER_URI_TYPE_NONE:
		default:
		break;
	}

	/* check source element is OK */
	if ( ! element )
	{
		debug_critical("no source element was created.\n");
		goto INIT_ERROR;
	}

	/* take source element */
	mainbin[MMPLAYER_M_SRC].id = MMPLAYER_M_SRC;
	mainbin[MMPLAYER_M_SRC].gst = element;
	element_bucket = g_list_append(element_bucket, &mainbin[MMPLAYER_M_SRC]);

	if (MMPLAYER_IS_STREAMING(player))
	{
		player->streamer = __mm_player_streaming_create();
		__mm_player_streaming_initialize(player->streamer);
	}

	if ( MMPLAYER_IS_HTTP_PD(player) )
	{
	       debug_log ("Picked queue2 element....\n");
		element = gst_element_factory_make("queue2", "hls_stream_buffer");
		if ( !element )
		{
			debug_critical ( "failed to create http streaming buffer element\n" );
			goto INIT_ERROR;
		}

		/* take it */
		mainbin[MMPLAYER_M_S_BUFFER].id = MMPLAYER_M_S_BUFFER;
		mainbin[MMPLAYER_M_S_BUFFER].gst = element;
		element_bucket = g_list_append(element_bucket, &mainbin[MMPLAYER_M_S_BUFFER]);

		__mm_player_streaming_set_buffer(player->streamer,
				element,
				TRUE,
				PLAYER_INI()->http_max_size_bytes,
				1.0,
				PLAYER_INI()->http_buffering_limit,
				PLAYER_INI()->http_buffering_time,
				FALSE,
				NULL,
				0);
	}

	/* create autoplugging element if src element is not a streamming src */
	if ( player->profile.uri_type != MM_PLAYER_URI_TYPE_URL_RTSP )
	{
		element = NULL;

		if( PLAYER_INI()->use_decodebin )
		{
			/* create decodebin */
			element = gst_element_factory_make("decodebin", "decodebin");

			g_object_set(G_OBJECT(element), "async-handling", TRUE, NULL);

			/* set signal handler */
			MMPLAYER_SIGNAL_CONNECT( player, G_OBJECT(element), "new-decoded-pad",
					G_CALLBACK(__mmplayer_gst_decode_callback), player);

			/* we don't need state holder, bcz decodebin is doing well by itself */
			need_state_holder = FALSE;
		}
		else
		{
			element = gst_element_factory_make("typefind", "typefinder");
			MMPLAYER_SIGNAL_CONNECT( player, element, "have-type",
				G_CALLBACK(__mmplayer_typefind_have_type), (gpointer)player );
		}

		/* check autoplug element is OK */
		if ( ! element )
		{
			debug_critical("can not create autoplug element\n");
			goto INIT_ERROR;
		}

		mainbin[MMPLAYER_M_AUTOPLUG].id = MMPLAYER_M_AUTOPLUG;
		mainbin[MMPLAYER_M_AUTOPLUG].gst = element;

		element_bucket = g_list_append(element_bucket, &mainbin[MMPLAYER_M_AUTOPLUG]);
	}


	/* add elements to pipeline */
	if( !__mmplayer_gst_element_add_bucket_to_bin(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), element_bucket))
	{
		debug_error("Failed to add elements to pipeline\n");
		goto INIT_ERROR;
	}


	/* linking elements in the bucket by added order. */
	if ( __mmplayer_gst_element_link_bucket(element_bucket) == -1 )
	{
		debug_error("Failed to link some elements\n");
		goto INIT_ERROR;
	}


	/* create fakesink element for keeping the pipeline state PAUSED. if needed */
	if ( need_state_holder )
	{
		/* create */
		mainbin[MMPLAYER_M_SRC_FAKESINK].id = MMPLAYER_M_SRC_FAKESINK;
		mainbin[MMPLAYER_M_SRC_FAKESINK].gst = gst_element_factory_make ("fakesink", "state-holder");

		if (!mainbin[MMPLAYER_M_SRC_FAKESINK].gst)
		{
			debug_error ("fakesink element could not be created\n");
			goto INIT_ERROR;
		}
		GST_OBJECT_FLAG_UNSET (mainbin[MMPLAYER_M_SRC_FAKESINK].gst, GST_ELEMENT_FLAG_SINK);

		/* take ownership of fakesink. we are reusing it */
		gst_object_ref( mainbin[MMPLAYER_M_SRC_FAKESINK].gst );

		/* add */
		if ( FALSE == gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst),
			mainbin[MMPLAYER_M_SRC_FAKESINK].gst) )
		{
			debug_error("failed to add fakesink to bin\n");
			goto INIT_ERROR;
		}
	}

	/* now we have completed mainbin. take it */
	player->pipeline->mainbin = mainbin;

	/* connect bus callback */
	bus = gst_pipeline_get_bus(GST_PIPELINE(mainbin[MMPLAYER_M_PIPE].gst));
	if ( !bus )
	{
		debug_error ("cannot get bus from pipeline.\n");
		goto INIT_ERROR;
	}
	player->bus_watcher = gst_bus_add_watch(bus, (GstBusFunc)__mmplayer_gst_callback, player);

	/* Note : check whether subtitle atrribute uri is set. If uri is set, then tyr to play subtitle file */
	if ( __mmplayer_check_subtitle ( player ) )
	{
		if ( MM_ERROR_NONE != __mmplayer_gst_create_subtitle_src(player) )
			debug_error("fail to create subtitle src\n");
	}

	/* set sync handler to get tag synchronously */
	gst_bus_set_sync_handler(bus, __mmplayer_bus_sync_callback, player, NULL);

	/* finished */
	gst_object_unref(GST_OBJECT(bus));
	g_list_free(element_bucket);

	debug_fleave();

	return MM_ERROR_NONE;

INIT_ERROR:

	__mmplayer_gst_destroy_pipeline(player);
	g_list_free(element_bucket);

	/* release element which are not added to bin */
	for ( i = 1; i < MMPLAYER_M_NUM; i++ ) 	/* NOTE : skip pipeline */
	{
		if ( mainbin[i].gst )
		{
			GstObject* parent = NULL;
			parent = gst_element_get_parent( mainbin[i].gst );

			if ( !parent )
			{
				gst_object_unref(GST_OBJECT(mainbin[i].gst));
				mainbin[i].gst = NULL;
			}
			else
			{
				gst_object_unref(GST_OBJECT(parent));
			}
		}
	}

	/* release pipeline with it's childs */
	if ( mainbin[MMPLAYER_M_PIPE].gst )
	{
		gst_object_unref(GST_OBJECT(mainbin[MMPLAYER_M_PIPE].gst));
	}

	MMPLAYER_FREEIF( player->pipeline );
	MMPLAYER_FREEIF( mainbin );

	return MM_ERROR_PLAYER_INTERNAL;
}

int
__mmplayer_gst_destroy_pipeline(mm_player_t* player) // @
{
	gint timeout = 0;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_INVALID_HANDLE );

	/* cleanup stuffs */
	MMPLAYER_FREEIF(player->type);
	player->have_dynamic_pad = FALSE;
	player->no_more_pad = FALSE;
	player->num_dynamic_pad = 0;

	if (player->v_stream_caps)
	{
		gst_caps_unref(player->v_stream_caps);
		player->v_stream_caps = NULL;
	}

	if (ahs_appsrc_cb_probe_id )
	{
		GstPad *pad = NULL;
		pad = gst_element_get_static_pad(player->pipeline->mainbin[MMPLAYER_M_SRC].gst, "src" );

		gst_pad_remove_probe (pad, ahs_appsrc_cb_probe_id);
		gst_object_unref(pad);
		pad = NULL;
		ahs_appsrc_cb_probe_id = 0;
	}

	if ( player->sink_elements )
		g_list_free ( player->sink_elements );
	player->sink_elements = NULL;

	/* cleanup unlinked mime type */
	MMPLAYER_FREEIF(player->unlinked_audio_mime);
	MMPLAYER_FREEIF(player->unlinked_video_mime);
	MMPLAYER_FREEIF(player->unlinked_demuxer_mime);

	/* cleanup running stuffs */
	__mmplayer_cancel_delayed_eos( player );

	/* cleanup gst stuffs */
	if ( player->pipeline )
	{
		MMPlayerGstElement* mainbin = player->pipeline->mainbin;
		GstTagList* tag_list = player->pipeline->tag_list;

		/* first we need to disconnect all signal hander */
		__mmplayer_release_signal_connection( player );

		/* disconnecting bus watch */
		if ( player->bus_watcher )
			__mmplayer_remove_g_source_from_context(player->bus_watcher);
		player->bus_watcher = 0;

		if ( mainbin )
		{
			MMPlayerGstElement* audiobin = player->pipeline->audiobin;
			MMPlayerGstElement* videobin = player->pipeline->videobin;
			MMPlayerGstElement* textbin = player->pipeline->textbin;
			GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (mainbin[MMPLAYER_M_PIPE].gst));
			gst_bus_set_sync_handler (bus, NULL, NULL, NULL);
			gst_object_unref(bus);

			debug_log("pipeline status before set state to NULL\n");
			__mmplayer_dump_pipeline_state( player );

			timeout = MMPLAYER_STATE_CHANGE_TIMEOUT(player);
			ret = __mmplayer_gst_set_state ( player, mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_NULL, FALSE, timeout );
			if ( ret != MM_ERROR_NONE )
			{
				debug_error("fail to change state to NULL\n");
				return MM_ERROR_PLAYER_INTERNAL;
			}

			debug_log("pipeline status before unrefering pipeline\n");
			__mmplayer_dump_pipeline_state( player );

			gst_object_unref(GST_OBJECT(mainbin[MMPLAYER_M_PIPE].gst));

			/* free fakesink */
			if ( mainbin[MMPLAYER_M_SRC_FAKESINK].gst )
				gst_object_unref(GST_OBJECT(mainbin[MMPLAYER_M_SRC_FAKESINK].gst));

			/* free avsysaudiosink
			   avsysaudiosink should be unref when destory pipeline just after start play with BT.
			   Because audiosink is created but never added to bin, and therefore it will not be unref when pipeline is destroyed.
			*/
			MMPLAYER_FREEIF( audiobin );
			MMPLAYER_FREEIF( videobin );
			MMPLAYER_FREEIF( textbin );
			MMPLAYER_FREEIF( mainbin );
		}

		if ( tag_list )
			gst_tag_list_free(tag_list);

		MMPLAYER_FREEIF( player->pipeline );
	}

	player->pipeline_is_constructed = FALSE;

	debug_fleave();

	return ret;
}

