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
|    LOCAL CONSTANT DEFINITIONS:                      |
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL DATA TYPE DEFINITIONS:                     |
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS:                     |
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS:                      |
---------------------------------------------------------------------------*/

/*===========================================================================================
|																							|
|  FUNCTION DEFINITIONS																		|
|  																							|
========================================================================================== */

int __gst_realize(mm_player_t* player) // @
{
	gint timeout = 0;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_READY;

	__ta__("__mmplayer_gst_create_pipeline",
		ret = __mmplayer_gst_create_pipeline(player);
		if ( ret )
		{
			debug_critical("failed to create pipeline\n");
			return ret;
		}
	)

	/* set pipeline state to READY */
	/* NOTE : state change to READY must be performed sync. */
	timeout = MMPLAYER_STATE_CHANGE_TIMEOUT(player);
	ret = __mmplayer_gst_set_state(player,
				player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_READY, FALSE, timeout);

	if ( ret != MM_ERROR_NONE )
	{
		/* return error if failed to set state */
		debug_error("failed to set READY state");
		return ret;
	}
	else
	{
		MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_READY );
	}

	/* create dot before error-return. for debugging */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-realize" );

	debug_fleave();

	return ret;
}

int __gst_unrealize(mm_player_t* player) // @
{
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_NULL;
	MMPLAYER_PRINT_STATE(player);

	/* release miscellaneous information */
	__mmplayer_release_misc( player );

	/* destroy pipeline */
	ret = __mmplayer_gst_destroy_pipeline( player );
	if ( ret != MM_ERROR_NONE )
	{
		debug_error("failed to destory pipeline\n");
		return ret;
	}

	MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_NULL );

	debug_fleave();

	return ret;
}

int __gst_pending_seek ( mm_player_t* player )
{
	MMPlayerStateType current_state = MM_PLAYER_STATE_NONE;
	MMPlayerStateType pending_state = MM_PLAYER_STATE_NONE;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );

	if ( !player->pending_seek.is_pending )
	{
		debug_log("pending seek is not reserved. nothing to do.\n" );
		return ret;
	}

	/* check player state if player could pending seek or not. */
	current_state = MMPLAYER_CURRENT_STATE(player);
	pending_state = MMPLAYER_PENDING_STATE(player);

	if ( current_state != MM_PLAYER_STATE_PAUSED && current_state != MM_PLAYER_STATE_PLAYING  )
	{
		debug_warning("try to pending seek in %s state, try next time. \n",
			MMPLAYER_STATE_GET_NAME(current_state));
		return ret;
	}

	debug_log("trying to play from (%lu) pending position\n", player->pending_seek.pos);

	ret = __gst_set_position ( player, player->pending_seek.format, player->pending_seek.pos, FALSE );

	if ( MM_ERROR_NONE != ret )
		debug_error("failed to seek pending postion. just keep staying current position.\n");

	player->pending_seek.is_pending = FALSE;

	debug_fleave();

	return ret;
}

int __gst_start(mm_player_t* player) // @
{
	gboolean sound_extraction = 0;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* get sound_extraction property */
	mm_attrs_get_int_by_name(player->attrs, "pcm_extraction", &sound_extraction);

	/* NOTE : if SetPosition was called before Start. do it now */
	/* streaming doesn't support it. so it should be always sync */
	/* !! create one more api to check if there is pending seek rather than checking variables */
	if ( (player->pending_seek.is_pending || sound_extraction) && !MMPLAYER_IS_STREAMING(player))
	{
		MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_PAUSED;
		ret = __gst_pause(player, FALSE);
		if ( ret != MM_ERROR_NONE )
		{
			debug_error("failed to set state to PAUSED for pending seek\n");
			return ret;
		}

		MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_PLAYING;

		if ( sound_extraction )
		{
			debug_log("setting pcm extraction\n");

			ret = __mmplayer_set_pcm_extraction(player);
			if ( MM_ERROR_NONE != ret )
			{
				debug_warning("failed to set pcm extraction\n");
				return ret;
			}
		}
		else
		{
			if ( MM_ERROR_NONE != __gst_pending_seek(player) )
			{
				debug_warning("failed to seek pending postion. starting from the begin of content.\n");
			}
		}
	}

	debug_log("current state before doing transition");
	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_PLAYING;
	MMPLAYER_PRINT_STATE(player);

	/* set pipeline state to PLAYING  */
	ret = __mmplayer_gst_set_state(player,
		player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_PLAYING, FALSE, MMPLAYER_STATE_CHANGE_TIMEOUT(player) );
	if (ret == MM_ERROR_NONE)
	{
		MMPLAYER_SET_STATE(player, MM_PLAYER_STATE_PLAYING);
	}
	else
	{
		debug_error("failed to set state to PLAYING");
		return ret;
	}

	/* generating debug info before returning error */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-start" );

	debug_fleave();

	return ret;
}

int __gst_stop(mm_player_t* player) // @
{
	GstStateChangeReturn change_ret = GST_STATE_CHANGE_SUCCESS;
	MMHandleType attrs = 0;
	gboolean fadewown = FALSE;
	gboolean rewind = FALSE;
	gint timeout = 0;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED);

	debug_log("current state before doing transition");
	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_READY;
	MMPLAYER_PRINT_STATE(player);

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("cannot get content attribute\n");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	mm_attrs_get_int_by_name(attrs,"sound_fadedown", &fadewown);

	/* enable fadedown */
	if (fadewown)
		__mmplayer_do_sound_fadedown(player, MM_PLAYER_FADEOUT_TIME_DEFAULT);

	/* Just set state to PAUESED and the rewind. it's usual player behavior. */
	timeout = MMPLAYER_STATE_CHANGE_TIMEOUT ( player );
	if  ( player->profile.uri_type == MM_PLAYER_URI_TYPE_BUFF || player->profile.uri_type == MM_PLAYER_URI_TYPE_HLS)
	{
		ret = __mmplayer_gst_set_state(player,
			player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_READY, FALSE, timeout );
	}
	else
	{
		ret = __mmplayer_gst_set_state( player,
			player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_PAUSED, FALSE, timeout );

		if ( !MMPLAYER_IS_STREAMING(player))
			rewind = TRUE;
	}

	/* disable fadeout */
	if (fadewown)
		__mmplayer_undo_sound_fadedown(player);


	/* return if set_state has failed */
	if ( ret != MM_ERROR_NONE )
	{
		debug_error("failed to set state.\n");
		return ret;
	}

	/* rewind */
	if ( rewind )
	{
		if ( ! __gst_seek( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, player->playback_rate,
				GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, 0,
				GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE) )
		{
			debug_warning("failed to rewind\n");
			ret = MM_ERROR_PLAYER_SEEK;
		}
	}

	/* initialize */
	player->sent_bos = FALSE;

	/* wait for seek to complete */
	change_ret = gst_element_get_state (player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, NULL, NULL, timeout * GST_SECOND);
	if ( change_ret == GST_STATE_CHANGE_SUCCESS || change_ret == GST_STATE_CHANGE_NO_PREROLL )
	{
		MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_READY );
	}
	else
	{
		debug_error("fail to stop player.\n");
		ret = MM_ERROR_PLAYER_INTERNAL;
		__mmplayer_dump_pipeline_state(player);
	}

	/* generate dot file if enabled */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-stop" );

	debug_fleave();

	return ret;
}

int __gst_pause(mm_player_t* player, gboolean async) // @
{
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail(player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(player->pipeline->mainbin, MM_ERROR_PLAYER_NOT_INITIALIZED);

	debug_log("current state before doing transition");
	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_PAUSED;
	MMPLAYER_PRINT_STATE(player);

	/* set pipeline status to PAUSED */
	ret = __mmplayer_gst_set_state(player,
		player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_PAUSED, async, MMPLAYER_STATE_CHANGE_TIMEOUT(player));

	if ( FALSE == async )
	{
		if ( ret != MM_ERROR_NONE )
		{
			GstMessage *msg = NULL;
			GTimer *timer = NULL;
			gdouble MAX_TIMEOUT_SEC = 3;

			debug_error("failed to set state to PAUSED");

			timer = g_timer_new();
			g_timer_start(timer);

			GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst));
			gboolean got_msg = FALSE;
			/* check if gst error posted or not */
			do
			{
				msg = gst_bus_timed_pop(bus, GST_SECOND /2);
				if (msg)
				{
					if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
					{
						GError *error = NULL;

						debug_error("parsing error posted from bus");
						/* parse error code */
						gst_message_parse_error(msg, &error, NULL);

						if (error->domain == GST_STREAM_ERROR)
						{
							ret = __gst_handle_stream_error( player, error, msg );
						}
						else if (error->domain == GST_RESOURCE_ERROR)
						{
							ret = __gst_handle_resource_error( player, error->code );
						}
						else if (error->domain == GST_LIBRARY_ERROR)
						{
							ret = __gst_handle_library_error( player, error->code );
						}
						else if (error->domain == GST_CORE_ERROR)
						{
							ret = __gst_handle_core_error( player, error->code );
						}
						got_msg = TRUE;
						player->msg_posted = TRUE;
					}
					gst_message_unref(msg);
				}
			} while (!got_msg && (g_timer_elapsed(timer, NULL) < MAX_TIMEOUT_SEC));

			/* clean */
			gst_object_unref(bus);
			g_timer_stop (timer);
			g_timer_destroy (timer);

			return ret;
		}
		else if ((!player->has_many_types) && (!player->pipeline->videobin) && (!player->pipeline->audiobin) )
		{
			if(MMPLAYER_IS_RTSP_STREAMING(player))
				return ret;
			player->msg_posted = TRUE; // no need to post error by message callback
			return MM_ERROR_PLAYER_CODEC_NOT_FOUND;
		}
		else if ( ret == MM_ERROR_NONE)
		{
			MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_PAUSED );
		}
	}

	/* generate dot file before returning error */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-pause" );

	debug_fleave();

	return ret;
}

int __gst_resume(mm_player_t* player, gboolean async) // @
{
	int ret = MM_ERROR_NONE;
	gint timeout = 0;
	GstBus *bus = NULL;

	debug_fenter();

	return_val_if_fail(player && player->pipeline,
		MM_ERROR_PLAYER_NOT_INITIALIZED);

	debug_log("current state before doing transition");
	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_PLAYING;
	MMPLAYER_PRINT_STATE(player);

	/* generate dot file before returning error */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-resume" );

	__mmplayer_set_antishock( player , FALSE );

	if ( async )
		debug_log("do async state transition to PLAYING.\n");

	/* clean bus sync handler because it's not needed any more */
	bus = gst_pipeline_get_bus (GST_PIPELINE(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst));
	gst_bus_set_sync_handler (bus, NULL, NULL, NULL);
	gst_object_unref(bus);

	/* set pipeline state to PLAYING */
	timeout = MMPLAYER_STATE_CHANGE_TIMEOUT(player);

	ret = __mmplayer_gst_set_state(player,
		player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_PLAYING, async, timeout );
	if (ret != MM_ERROR_NONE)
	{
		debug_error("failed to set state to PLAYING\n");

		return ret;
	}
	else
	{
		if (async == FALSE)
		{
			MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_PLAYING );
		}
	}

	/* FIXIT : analyze so called "async problem" */
	/* set async off */
	__gst_set_async_state_change( player, FALSE );

	/* generate dot file before returning error */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-resume" );

	debug_fleave();

	return ret;
}

int
__gst_set_position(mm_player_t* player, int format, unsigned long position, gboolean internal_called) // @
{
	GstFormat fmt  = GST_FORMAT_TIME;
	unsigned long dur_msec = 0;
	gint64 dur_nsec = 0;
	gint64 pos_nsec = 0;
	gboolean ret = TRUE;
	gboolean accurated = FALSE;
	GstSeekFlags seek_flags = GST_SEEK_FLAG_FLUSH;

	debug_fenter();
	return_val_if_fail ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( !MMPLAYER_IS_LIVE_STREAMING(player), MM_ERROR_PLAYER_NO_OP );

	if ( MMPLAYER_CURRENT_STATE(player) != MM_PLAYER_STATE_PLAYING
		&& MMPLAYER_CURRENT_STATE(player) != MM_PLAYER_STATE_PAUSED )
		goto PENDING;

	/* check duration */
	/* NOTE : duration cannot be zero except live streaming.
	 * 		Since some element could have some timing problemn with quering duration, try again.
	 */
	if ( !player->duration )
	{
		if ( !gst_element_query_duration( player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, fmt, &dur_nsec ))
		{
			goto SEEK_ERROR;
		}
		player->duration = dur_nsec;
	}

	if ( player->duration )
	{
		dur_msec = GST_TIME_AS_MSECONDS(player->duration);
	}
	else
	{
		debug_error("could not get the duration. fail to seek.\n");
		goto SEEK_ERROR;
	}

	debug_log("playback rate: %f\n", player->playback_rate);

	mm_attrs_get_int_by_name(player->attrs,"accurate_seek", &accurated);
	if (accurated)
	{
		seek_flags |= GST_SEEK_FLAG_ACCURATE;
	}
	else
	{
		seek_flags |= GST_SEEK_FLAG_KEY_UNIT;
	}

	/* do seek */
	switch ( format )
	{
		case MM_PLAYER_POS_FORMAT_TIME:
		{
			/* check position is valid or not */
			if ( position > dur_msec )
				goto INVALID_ARGS;

			debug_log("seeking to (%lu) msec, duration is %d msec\n", position, dur_msec);

			if (player->doing_seek)
			{
				debug_log("not completed seek");
				return MM_ERROR_PLAYER_DOING_SEEK;
			}

			if ( !internal_called)
				player->doing_seek = TRUE;

			pos_nsec = position * G_GINT64_CONSTANT(1000000);
			ret = __gst_seek ( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, player->playback_rate,
							GST_FORMAT_TIME, seek_flags,
							GST_SEEK_TYPE_SET, pos_nsec, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE );
			if ( !ret  )
			{
				debug_error("failed to set position. dur[%lu]  pos[%lu]  pos_msec[%llu]\n", dur_msec, position, pos_nsec);
				goto SEEK_ERROR;
			}
		}
		break;

		case MM_PLAYER_POS_FORMAT_PERCENT:
		{
			debug_log("seeking to (%lu)%% \n", position);

			if (player->doing_seek)
			{
				debug_log("not completed seek");
				return MM_ERROR_PLAYER_DOING_SEEK;
			}

			if ( !internal_called)
				player->doing_seek = TRUE;

			/* FIXIT : why don't we use 'GST_FORMAT_PERCENT' */
			pos_nsec = (gint64) ( ( position * player->duration ) / 100 );
			ret = __gst_seek ( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, player->playback_rate,
							GST_FORMAT_TIME, seek_flags,
							GST_SEEK_TYPE_SET, pos_nsec, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE );
			if ( !ret  )
			{
				debug_error("failed to set position. dur[%lud]  pos[%lud]  pos_msec[%llud]\n", dur_msec, position, pos_nsec);
				goto SEEK_ERROR;
			}
		}
		break;

		default:
			goto INVALID_ARGS;

	}

	/* NOTE : store last seeking point to overcome some bad operation
	  *      ( returning zero when getting current position ) of some elements
	  */
	player->last_position = pos_nsec;

	/* MSL should guarante playback rate when seek is selected during trick play of fast forward. */
	if ( player->playback_rate > 1.0 )
		_mmplayer_set_playspeed ( (MMHandleType)player, player->playback_rate );

	debug_fleave();
	return MM_ERROR_NONE;

PENDING:
	player->pending_seek.is_pending = TRUE;
	player->pending_seek.format = format;
	player->pending_seek.pos = position;

	debug_warning("player current-state : %s, pending-state : %s, just preserve pending position(%lu).\n",
		MMPLAYER_STATE_GET_NAME(MMPLAYER_CURRENT_STATE(player)), MMPLAYER_STATE_GET_NAME(MMPLAYER_PENDING_STATE(player)), player->pending_seek.pos);

	return MM_ERROR_NONE;

INVALID_ARGS:
	debug_error("invalid arguments, position : %ld  dur : %ld format : %d \n", position, dur_msec, format);
	return MM_ERROR_INVALID_ARGUMENT;

SEEK_ERROR:
	player->doing_seek = FALSE;
	return MM_ERROR_PLAYER_SEEK;
}

#define TRICKPLAY_OFFSET GST_MSECOND

int
__gst_get_position(mm_player_t* player, int format, unsigned long* position) // @
{
	MMPlayerStateType current_state = MM_PLAYER_STATE_NONE;
	GstFormat fmt = GST_FORMAT_TIME;
	signed long long pos_msec = 0;
	gboolean ret = TRUE;

	return_val_if_fail( player && position && player->pipeline && player->pipeline->mainbin,
		MM_ERROR_PLAYER_NOT_INITIALIZED );

	current_state = MMPLAYER_CURRENT_STATE(player);

	/* NOTE : query position except paused state to overcome some bad operation
	 * please refer to below comments in details
	 */
	if ( current_state != MM_PLAYER_STATE_PAUSED )
	{
		ret = gst_element_query_position(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, fmt, &pos_msec);
	}

	/* NOTE : get last point to overcome some bad operation of some elements
	 * ( returning zero when getting current position in paused state
	 * and when failed to get postion during seeking
	 */
	if ( ( current_state == MM_PLAYER_STATE_PAUSED )
		|| ( ! ret ))
		//|| ( player->last_position != 0 && pos_msec == 0 ) )
	{
		debug_warning ("pos_msec = %"GST_TIME_FORMAT" and ret = %d and state = %d", GST_TIME_ARGS (pos_msec), ret, current_state);

		if(player->playback_rate < 0.0)
			pos_msec = player->last_position - TRICKPLAY_OFFSET;
		else
			pos_msec = player->last_position;

		if (!ret)
			pos_msec = player->last_position;
		else
			player->last_position = pos_msec;

		debug_warning("returning last point : %"GST_TIME_FORMAT, GST_TIME_ARGS(pos_msec));

	}
	else
	{
		player->last_position = pos_msec;
	}

	switch (format) {
		case MM_PLAYER_POS_FORMAT_TIME:
			*position = GST_TIME_AS_MSECONDS(pos_msec);
			break;

		case MM_PLAYER_POS_FORMAT_PERCENT:
		{
			int dur = 0;
			int pos = 0;

			dur = player->duration / GST_SECOND;
			if (dur <= 0)
			{
				debug_log ("duration is [%d], so returning position 0\n",dur);
				*position = 0;
			}
			else
			{
				pos = pos_msec / GST_SECOND;
				*position = pos * 100 / dur;
			}
			break;
		}
		default:
			return MM_ERROR_PLAYER_INTERNAL;
	}

	debug_log("current position : %lu\n", *position);


	return MM_ERROR_NONE;
}

int 	__gst_get_buffer_position(mm_player_t* player, int format, unsigned long* start_pos, unsigned long* stop_pos)
{
	GstElement *element = NULL;
	GstQuery *query = NULL;

	return_val_if_fail( player &&
		player->pipeline &&
		player->pipeline->mainbin,
		MM_ERROR_PLAYER_NOT_INITIALIZED );

	return_val_if_fail( start_pos && stop_pos, MM_ERROR_INVALID_ARGUMENT );

	if ( MMPLAYER_IS_HTTP_STREAMING ( player ))
	{
		/* Note : In case of http streaming or HLS, the buffering queue [ queue2 ] could handle buffering query. */
		element = GST_ELEMENT ( player->pipeline->mainbin[MMPLAYER_M_S_BUFFER].gst );
	}
	else if ( MMPLAYER_IS_RTSP_STREAMING ( player ) )
	{
		debug_warning ( "it's not supported yet.\n" );
		return MM_ERROR_NONE;
	}
	else
	{
		debug_warning ( "it's only used for streaming case.\n" );
		return MM_ERROR_NONE;
	}

	*start_pos = 0;
	*stop_pos = 0;

	switch ( format )
	{
		case MM_PLAYER_POS_FORMAT_PERCENT :
		{
			      	query = gst_query_new_buffering ( GST_FORMAT_PERCENT );
			      	if ( gst_element_query ( element, query ) )
				{
			        	gint64 start, stop;
			        	GstFormat format;
			        	gboolean busy;
			        	gint percent;

			        	gst_query_parse_buffering_percent ( query, &busy, &percent);
			        	gst_query_parse_buffering_range ( query, &format, &start, &stop, NULL );

			        	debug_log ( "buffering start %" G_GINT64_FORMAT ", stop %" G_GINT64_FORMAT "\n",  start, stop);

			        	if ( start != -1)
			          		*start_pos = 100 * start / GST_FORMAT_PERCENT_MAX;
			        	else
			          		*start_pos = 0;

			        	if ( stop != -1)
			          		*stop_pos = 100 * stop / GST_FORMAT_PERCENT_MAX;
			        	else
			          		*stop_pos = 0;
			      	}
			      	gst_query_unref (query);
		}
		break;

		case MM_PLAYER_POS_FORMAT_TIME :
			debug_warning ( "Time format is not supported yet.\n" );
			break;

		default :
			break;
	}

debug_log("current buffer position : %lu~%lu \n", *start_pos, *stop_pos );

	return MM_ERROR_NONE;
}

int
__gst_set_message_callback(mm_player_t* player, MMMessageCallback callback, gpointer user_param) // @
{
	debug_fenter();

	if ( !player )
	{
		debug_warning("set_message_callback is called with invalid player handle\n");
		return MM_ERROR_PLAYER_NOT_INITIALIZED;
	}

	player->msg_cb = callback;
	player->msg_cb_param = user_param;

	debug_log("msg_cb : 0x%x     msg_cb_param : 0x%x\n", (guint)callback, (guint)user_param);

	debug_fleave();

	return MM_ERROR_NONE;
}

/* sending event to one of sinkelements */
gboolean
__gst_send_event_to_sink( mm_player_t* player, GstEvent* event )
{
	GstEvent * event2 = NULL;
	GList *sinks = NULL;
	gboolean res = FALSE;

	debug_fenter();

	return_val_if_fail( player, FALSE );
	return_val_if_fail ( event, FALSE );

	if ( player->play_subtitle && !player->use_textoverlay)
		event2 = gst_event_copy((const GstEvent *)event);

	sinks = player->sink_elements;
	while (sinks)
	{
		GstElement *sink = GST_ELEMENT_CAST (sinks->data);

		if (GST_IS_ELEMENT(sink))
		{
			/* in the case of some video/audio file,
			 * it's possible video sink don't consider same position seek
			 * with current postion
			 */
			if ( !MMPLAYER_IS_STREAMING(player) && player->pipeline->videobin
				&& player->pipeline->audiobin && (!g_strrstr(GST_ELEMENT_NAME(sink), "audiosink")) )
			{
				sinks = g_list_next (sinks);
				continue;
			}

			/* keep ref to the event */
			gst_event_ref (event);

			if ( (res = gst_element_send_event (sink, event)) )
			{
				debug_log("sending event[%s] to sink element [%s] success!\n",
					GST_EVENT_TYPE_NAME(event), GST_ELEMENT_NAME(sink) );
				break;
			}

			debug_log("sending event[%s] to sink element [%s] failed. try with next one.\n",
				GST_EVENT_TYPE_NAME(event), GST_ELEMENT_NAME(sink) );
		}

		sinks = g_list_next (sinks);
	}

	/* Note : Textbin is not linked to the video or audio bin.
	 * 		It needs to send the event to the text sink seperatelly.
	 */
	 if ( player->play_subtitle && !player->use_textoverlay)
	 {
	 	GstElement *text_sink = GST_ELEMENT_CAST (player->pipeline->textbin[MMPLAYER_T_SINK].gst);

		if (GST_IS_ELEMENT(text_sink))
		{
			/* keep ref to the event */
			gst_event_ref (event2);

			if ( (res != gst_element_send_event (text_sink, event2)) )
			{
				debug_error("sending event[%s] to subtitle sink element [%s] failed!\n",
					GST_EVENT_TYPE_NAME(event2), GST_ELEMENT_NAME(text_sink) );
			}
			else
			{
				debug_log("sending event[%s] to subtitle sink element [%s] success!\n",
					GST_EVENT_TYPE_NAME(event2), GST_ELEMENT_NAME(text_sink) );
			}

			gst_event_unref (event2);
		}
	 }

	gst_event_unref (event);

	debug_fleave();

	return res;
}

gboolean
__gst_seek(mm_player_t* player, GstElement * element, gdouble rate,
			GstFormat format, GstSeekFlags flags, GstSeekType cur_type,
			gint64 cur, GstSeekType stop_type, gint64 stop )
{
	GstEvent* event = NULL;
	gboolean result = FALSE;

	debug_fenter();

	return_val_if_fail( player, FALSE );

	event = gst_event_new_seek (rate, format, flags, cur_type,
		cur, stop_type, stop);

	result = __gst_send_event_to_sink( player, event );

	debug_fleave();

	return result;
}

int __gst_adjust_subtitle_position(mm_player_t* player, int format, unsigned long position)
{
	GstEvent* event = NULL;
    unsigned long current_pos = 0;
    unsigned long adusted_pos = 0;

	debug_fenter();

	/* check player and subtitlebin are created */
	return_val_if_fail ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( player->play_subtitle, MM_ERROR_NOT_SUPPORT_API );

	if (position == 0)
	{
		debug_log ("nothing to do\n");
		return MM_ERROR_NONE;
	}

	switch (format)
	{
		case MM_PLAYER_POS_FORMAT_TIME:
		{
			/* check current postion */
			if (__gst_get_position(player, MM_PLAYER_POS_FORMAT_TIME, &current_pos ))
			{
				debug_error("failed to get position");
				return MM_ERROR_PLAYER_INTERNAL;
			}

            adusted_pos = current_pos + (position * G_GINT64_CONSTANT(1000000));
			if (adusted_pos < 0)
				adusted_pos = G_GUINT64_CONSTANT(0);
			debug_log("adjust subtitle postion : %lu -> %lu [msec]\n", GST_TIME_AS_MSECONDS(current_pos), GST_TIME_AS_MSECONDS(adusted_pos));

			event = gst_event_new_seek (1.0, 	GST_FORMAT_TIME,
				( GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE ),
				GST_SEEK_TYPE_SET, adusted_pos,
				GST_SEEK_TYPE_SET, -1);
		}
		break;

		default:
		{
			debug_warning("invalid format.\n");
			return MM_ERROR_INVALID_ARGUMENT;
		}
	}

	/* keep ref to the event */
	gst_event_ref (event);

	debug_log("sending event[%s] to subparse element [%s]\n",
			GST_EVENT_TYPE_NAME(event), GST_ELEMENT_NAME(player->pipeline->mainbin[MMPLAYER_M_SUBPARSE].gst) );

	if (gst_element_send_event (player->pipeline->mainbin[MMPLAYER_M_SUBPARSE].gst, event))
	{
		debug_log("sending event[%s] to subparse element [%s] success!\n",
			GST_EVENT_TYPE_NAME(event), GST_ELEMENT_NAME(player->pipeline->mainbin[MMPLAYER_M_SUBPARSE].gst) );
	}

	/* unref to the event */
	gst_event_unref (event);

	debug_fleave();

	return MM_ERROR_NONE;
}

void
__gst_appsrc_feed_data_mem(GstElement *element, guint size, gpointer user_data) // @
{
	GstElement *appsrc = element;
	tBuffer *buf = (tBuffer *)user_data;
	GstBuffer *buffer = NULL;
	GstFlowReturn ret = GST_FLOW_OK;
	gint len = size;

	return_if_fail ( element );
	return_if_fail ( buf );

	buffer = gst_buffer_new ();

	if (buf->offset >= buf->len)
	{
		debug_log("call eos appsrc\n");
		g_signal_emit_by_name (appsrc, "end-of-stream", &ret);
		return;
	}

	if ( buf->len - buf->offset < size)
	{
		len = buf->len - buf->offset + buf->offset;
	}

	gst_buffer_insert_memory(buffer, -1, gst_memory_new_wrapped(0, (guint8*)(buf->buf + buf->offset), len, 0, len, (guint8*)(buf->buf + buf->offset), g_free));

	GST_BUFFER_OFFSET(buffer) = buf->offset;
	GST_BUFFER_OFFSET_END(buffer) = buf->offset + len;

	debug_log("feed buffer %p, offset %u-%u length %u\n", buffer, buf->offset, buf->len,len);
	g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);

	buf->offset += len;
}

gboolean
__gst_appsrc_seek_data_mem(GstElement *element, guint64 size, gpointer user_data) // @
{
	tBuffer *buf = (tBuffer *)user_data;

	return_val_if_fail ( buf, FALSE );

	buf->offset  = (int)size;

	return TRUE;
}

void
__gst_appsrc_feed_data(GstElement *element, guint size, gpointer user_data) // @
{
	mm_player_t *player  = (mm_player_t*)user_data;

	return_if_fail ( player );

	debug_msg("app-src: feed data\n");

	if(player->need_data_cb)
		player->need_data_cb(size, player->buffer_cb_user_param);
}

gboolean
__gst_appsrc_seek_data(GstElement *element, guint64 offset, gpointer user_data) // @
{
	mm_player_t *player  = (mm_player_t*)user_data;

	return_val_if_fail ( player, FALSE );

	debug_msg("app-src: seek data\n");

	if(player->seek_data_cb)
		player->seek_data_cb(offset, player->buffer_cb_user_param);

	return TRUE;
}


gboolean
__gst_appsrc_enough_data(GstElement *element, gpointer user_data) // @
{
	mm_player_t *player  = (mm_player_t*)user_data;

	return_val_if_fail ( player, FALSE );

	debug_msg("app-src: enough data:%p\n", player->enough_data_cb);

	if(player->enough_data_cb)
		player->enough_data_cb(player->buffer_cb_user_param);

	return TRUE;
}
