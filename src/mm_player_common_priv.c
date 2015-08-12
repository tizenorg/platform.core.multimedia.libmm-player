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
#include <glib.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/videooverlay.h>
#ifdef HAVE_WAYLAND
#include <gst/wayland/wayland.h>
#endif
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>

#include <mm_error.h>
#include <mm_attrs.h>
#include <mm_attrs_private.h>
#include <mm_debug.h>

#include "mm_player_priv.h"
#include "mm_player_ini.h"
#include "mm_player_attrs.h"
#include "mm_player_utils.h"
#include <sched.h>

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
static gint __gst_transform_gsterror( mm_player_t* player, GstMessage * message, GError* error);

/*===========================================================================================
|																							|
|  FUNCTION DEFINITIONS																		|
|  																							|
========================================================================================== */
int
__mmplayer_check_state(mm_player_t* player, enum PlayerCommandState command)
{
	MMPlayerStateType current_state = MM_PLAYER_STATE_NUM;
	MMPlayerStateType pending_state = MM_PLAYER_STATE_NUM;
//	MMPlayerStateType target_state = MM_PLAYER_STATE_NUM;
//	MMPlayerStateType prev_state = MM_PLAYER_STATE_NUM;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	//debug_log("incomming command : %d \n", command );

	current_state = MMPLAYER_CURRENT_STATE(player);
	pending_state = MMPLAYER_PENDING_STATE(player);
//	target_state = MMPLAYER_TARGET_STATE(player);
//	prev_state = MMPLAYER_PREV_STATE(player);

	MMPLAYER_PRINT_STATE(player);

	switch( command )
	{
		case MMPLAYER_COMMAND_CREATE:
		{
			MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_NULL;

			if ( current_state == MM_PLAYER_STATE_NULL ||
				current_state == MM_PLAYER_STATE_READY ||
				current_state == MM_PLAYER_STATE_PAUSED ||
				current_state == MM_PLAYER_STATE_PLAYING )
				goto NO_OP;
		}
		break;

		case MMPLAYER_COMMAND_DESTROY:
		{
			/* destroy can called anytime */

			MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_NONE;
		}
		break;

		case MMPLAYER_COMMAND_REALIZE:
		{
			MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_READY;

			if ( pending_state != MM_PLAYER_STATE_NONE )
			{
				goto INVALID_STATE;
			}
			else
			{
				/* need ready state to realize */
				if ( current_state == MM_PLAYER_STATE_READY )
					goto NO_OP;

				if ( current_state != MM_PLAYER_STATE_NULL )
					goto INVALID_STATE;
			}
		}
		break;

		case MMPLAYER_COMMAND_UNREALIZE:
		{
			MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_NULL;

			if ( current_state == MM_PLAYER_STATE_NULL )
				goto NO_OP;
		}
		break;

		case MMPLAYER_COMMAND_START:
		{
			MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_PLAYING;

			if ( pending_state == MM_PLAYER_STATE_NONE )
			{
				if ( current_state == MM_PLAYER_STATE_PLAYING )
					goto NO_OP;
				else if ( current_state  != MM_PLAYER_STATE_READY &&
					current_state != MM_PLAYER_STATE_PAUSED )
					goto INVALID_STATE;
			}
			else if ( pending_state == MM_PLAYER_STATE_PLAYING )
			{
				goto ALREADY_GOING;
			}
			else if ( pending_state == MM_PLAYER_STATE_PAUSED )
			{
				debug_log("player is going to paused state, just change the pending state as playing");
			}
			else
			{
				goto INVALID_STATE;
			}
		}
		break;

		case MMPLAYER_COMMAND_STOP:
		{
			MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_READY;

			if ( current_state == MM_PLAYER_STATE_READY )
				goto NO_OP;

			/* need playing/paused state to stop */
			if ( current_state != MM_PLAYER_STATE_PLAYING &&
				 current_state != MM_PLAYER_STATE_PAUSED )
				goto INVALID_STATE;
		}
		break;

		case MMPLAYER_COMMAND_PAUSE:
		{
			if ( MMPLAYER_IS_LIVE_STREAMING( player ) )
				goto NO_OP;

			if (player->doing_seek)
				goto NOT_COMPLETED_SEEK;

			MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_PAUSED;

			if ( pending_state == MM_PLAYER_STATE_NONE )
			{
				if ( current_state == MM_PLAYER_STATE_PAUSED )
					goto NO_OP;
				else if ( current_state != MM_PLAYER_STATE_PLAYING && current_state != MM_PLAYER_STATE_READY ) // support loading state of broswer
					goto INVALID_STATE;
			}
			else if ( pending_state == MM_PLAYER_STATE_PAUSED )
			{
				goto ALREADY_GOING;
			}
			else if ( pending_state == MM_PLAYER_STATE_PLAYING )
			{
				if ( current_state == MM_PLAYER_STATE_PAUSED ) {
					debug_log("player is PAUSED going to PLAYING, just change the pending state as PAUSED");
				} else {
					goto INVALID_STATE;
				}
			}
		}
		break;

		case MMPLAYER_COMMAND_RESUME:
		{

			if (player->doing_seek)
				goto NOT_COMPLETED_SEEK;

			MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_PLAYING;

			if ( pending_state == MM_PLAYER_STATE_NONE )
			{
				if ( current_state == MM_PLAYER_STATE_PLAYING )
					goto NO_OP;
				else if (  current_state != MM_PLAYER_STATE_PAUSED )
					goto INVALID_STATE;
			}
			else if ( pending_state == MM_PLAYER_STATE_PLAYING )
			{
				goto ALREADY_GOING;
			}
			else if ( pending_state == MM_PLAYER_STATE_PAUSED )
			{
				debug_log("player is going to paused state, just change the pending state as playing");
			}
			else
			{
				goto INVALID_STATE;
			}
		}
		break;

		default:
		break;
	}
	player->cmd = command;

	return MM_ERROR_NONE;

INVALID_STATE:
	debug_warning("since player is in wrong state(%s). it's not able to apply the command(%d)",
		MMPLAYER_STATE_GET_NAME(current_state), command);
	return MM_ERROR_PLAYER_INVALID_STATE;

NOT_COMPLETED_SEEK:
	debug_warning("not completed seek");
	return MM_ERROR_PLAYER_DOING_SEEK;

NO_OP:
	debug_warning("player is in the desired state(%s). doing noting", MMPLAYER_STATE_GET_NAME(current_state));
	return MM_ERROR_PLAYER_NO_OP;

ALREADY_GOING:
	debug_warning("player is already going to %s, doing nothing", MMPLAYER_STATE_GET_NAME(pending_state));
	return MM_ERROR_PLAYER_NO_OP;
}

int
__mmplayer_gst_set_state (mm_player_t* player, GstElement * element,  GstState state, gboolean async, gint timeout) // @
{
	GstState element_state = GST_STATE_VOID_PENDING;
	GstState element_pending_state = GST_STATE_VOID_PENDING;
	GstStateChangeReturn ret = GST_STATE_CHANGE_FAILURE;

	MMPLAYER_FENTER();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( element, MM_ERROR_INVALID_ARGUMENT );

	debug_log("setting [%s] element state to : %s\n", GST_ELEMENT_NAME(element), gst_element_state_get_name(state));

	/* set state */
	ret = gst_element_set_state(element, state);

	if ( ret == GST_STATE_CHANGE_FAILURE )
	{
		debug_error("failed to set [%s] state\n", GST_ELEMENT_NAME(element));

		/* dump state of all element */
		__mmplayer_dump_pipeline_state( player );

		return MM_ERROR_PLAYER_INTERNAL;
	}

	/* return here so state transition to be done in async mode */
	if ( async )
	{
		debug_log("async state transition. not waiting for state complete.\n");
		return MM_ERROR_NONE;
	}

	/* wait for state transition */
	ret = gst_element_get_state( element, &element_state, &element_pending_state, timeout * GST_SECOND );

	if ( ret == GST_STATE_CHANGE_FAILURE || ( state != element_state ) )
	{
		debug_error("failed to change [%s] element state to [%s] within %d sec\n",
			GST_ELEMENT_NAME(element),
			gst_element_state_get_name(state), timeout );

		debug_error(" [%s] state : %s   pending : %s \n",
			GST_ELEMENT_NAME(element),
			gst_element_state_get_name(element_state),
			gst_element_state_get_name(element_pending_state) );

		/* dump state of all element */
		__mmplayer_dump_pipeline_state( player );

		return MM_ERROR_PLAYER_INTERNAL;
	}

	debug_log("[%s] element state has changed\n", GST_ELEMENT_NAME(element));

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

void __mmplayer_remove_g_source_from_context(GMainContext *context, guint source_id)
{
	GSource *source = NULL;

	MMPLAYER_FENTER();

	source = g_main_context_find_source_by_id (context, source_id);

	if (source != NULL)
	{
		debug_warning("context: %p, source id: %d, source: %p", context, source_id, source);
		g_source_destroy(source);
	}

	MMPLAYER_FLEAVE();
}

gboolean
__mmplayer_dump_pipeline_state( mm_player_t* player )
{
	GstIterator*iter = NULL;
	gboolean done = FALSE;

	GValue item = {0, };
	GstElement *element = NULL;
	GstElementFactory *factory = NULL;

	GstState state = GST_STATE_VOID_PENDING;
	GstState pending = GST_STATE_VOID_PENDING;
	GstClockTime time = 200*GST_MSECOND;

	MMPLAYER_FENTER();

	return_val_if_fail ( player &&
		player->pipeline &&
		player->pipeline->mainbin,
		FALSE );

	iter = gst_bin_iterate_recurse(GST_BIN(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst) );

	if ( iter != NULL )
	{
		while (!done) {
			 switch ( gst_iterator_next (iter, &item) )
			 {
			   case GST_ITERATOR_OK:
				element = g_value_get_object(&item);
				gst_element_get_state(element,&state, &pending,time);

				factory = gst_element_get_factory (element) ;
				if (factory)
				{
					debug_error("%s:%s : From:%s To:%s   refcount : %d\n", GST_OBJECT_NAME(factory) , GST_ELEMENT_NAME(element) ,
						gst_element_state_get_name(state), gst_element_state_get_name(pending) , GST_OBJECT_REFCOUNT_VALUE(element));
				}
				 g_value_reset (&item);
				 break;
			   case GST_ITERATOR_RESYNC:
				 gst_iterator_resync (iter);
				 break;
			   case GST_ITERATOR_ERROR:
				 done = TRUE;
				 break;
			   case GST_ITERATOR_DONE:
				 done = TRUE;
				 break;
			 }
		}
	}

	element = GST_ELEMENT(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst);

	gst_element_get_state(element,&state, &pending,time);

	factory = gst_element_get_factory (element) ;

	if (factory)
	{
		debug_error("%s:%s : From:%s To:%s  refcount : %d\n",
			GST_OBJECT_NAME(factory),
			GST_ELEMENT_NAME(element),
			gst_element_state_get_name(state),
			gst_element_state_get_name(pending),
			GST_OBJECT_REFCOUNT_VALUE(element) );
	}

	g_value_unset(&item);

	if ( iter )
		gst_iterator_free (iter);

	MMPLAYER_FLEAVE();

	return FALSE;
}

const gchar *
__get_state_name ( int state )
{
	switch ( state )
	{
		case MM_PLAYER_STATE_NULL:
			return "NULL";
		case MM_PLAYER_STATE_READY:
			return "READY";
		case MM_PLAYER_STATE_PAUSED:
			return "PAUSED";
		case MM_PLAYER_STATE_PLAYING:
			return "PLAYING";
		case MM_PLAYER_STATE_NONE:
			return "NONE";
		default:
			return "INVAID";
	}
}

gboolean
__is_rtsp_streaming ( mm_player_t* player )
{
	return_val_if_fail ( player, FALSE );

	return ( player->profile.uri_type == MM_PLAYER_URI_TYPE_URL_RTSP ) ? TRUE : FALSE;
}

gboolean
__is_wfd_streaming ( mm_player_t* player )
{
  return_val_if_fail ( player, FALSE );

  return ( player->profile.uri_type == MM_PLAYER_URI_TYPE_URL_WFD ) ? TRUE : FALSE;
}

gboolean
__is_http_streaming ( mm_player_t* player )
{
	return_val_if_fail ( player, FALSE );

	return ( player->profile.uri_type == MM_PLAYER_URI_TYPE_URL_HTTP ) ? TRUE : FALSE;
}

gboolean
__is_streaming ( mm_player_t* player )
{
	return_val_if_fail ( player, FALSE );

  return ( __is_http_progressive_down( player ) || __is_rtsp_streaming ( player ) || __is_wfd_streaming ( player ) || __is_http_streaming ( player )
          || __is_http_live_streaming ( player ) || __is_dash_streaming ( player ) || __is_smooth_streaming(player) ) ? TRUE : FALSE;
}

gboolean
__is_live_streaming ( mm_player_t* player )
{
	return_val_if_fail ( player, FALSE );

	return ( __is_rtsp_streaming ( player ) && player->streaming_type == STREAMING_SERVICE_LIVE ) ? TRUE : FALSE;
}

gboolean
__is_http_live_streaming( mm_player_t* player )
{
	return_val_if_fail( player, FALSE );

	return ( player->profile.uri_type == MM_PLAYER_URI_TYPE_HLS ) ? TRUE : FALSE;
}

gboolean
__is_dash_streaming ( mm_player_t* player )
{
	return_val_if_fail ( player, FALSE );

	return ( player->profile.uri_type == MM_PLAYER_URI_TYPE_DASH ) ? TRUE : FALSE;
}

gboolean
__is_smooth_streaming ( mm_player_t* player )
{
	return_val_if_fail ( player, FALSE );

	return ( player->profile.uri_type == MM_PLAYER_URI_TYPE_SS ) ? TRUE : FALSE;
}


gboolean
__is_http_progressive_down(mm_player_t* player)
{
	return_val_if_fail( player, FALSE );

	return ((player->pd_mode) ? TRUE:FALSE);
}
/* if retval is FALSE, it will be dropped for perfomance. */
gboolean
__mmplayer_check_useful_message(mm_player_t *player, GstMessage * message)
{
	gboolean retval = FALSE;

	if ( !(player->pipeline && player->pipeline->mainbin) )
	{
		debug_error("player pipeline handle is null");
		return TRUE;
	}

	switch (GST_MESSAGE_TYPE (message))
	{
		case GST_MESSAGE_TAG:
		case GST_MESSAGE_EOS:
		case GST_MESSAGE_ERROR:
		case GST_MESSAGE_WARNING:
		case GST_MESSAGE_CLOCK_LOST:
		case GST_MESSAGE_NEW_CLOCK:
		case GST_MESSAGE_ELEMENT:
		case GST_MESSAGE_DURATION_CHANGED:
		case GST_MESSAGE_ASYNC_START:
			retval = TRUE;
			break;
		case GST_MESSAGE_ASYNC_DONE:
		case GST_MESSAGE_STATE_CHANGED:
			/* we only handle messages from pipeline */
			if(( message->src == (GstObject *)player->pipeline->mainbin[MMPLAYER_M_PIPE].gst ) && (!player->gapless.reconfigure))
				retval = TRUE;
			else
				retval = FALSE;
			break;
		case GST_MESSAGE_BUFFERING:
		{
			gint buffer_percent = 0;

			gst_message_parse_buffering (message, &buffer_percent);

			if ((MMPLAYER_IS_STREAMING(player)) &&
				(player->streamer) &&
				(player->streamer->is_buffering == TRUE) &&
				(buffer_percent == MAX_BUFFER_PERCENT))
			{
				debug_log (">>> [%s] Buffering DONE is detected !!\n", GST_OBJECT_NAME(GST_MESSAGE_SRC(message)));
				player->streamer->is_buffering_done = TRUE;
			}

			retval = TRUE;
			break;
		}
		default:
			retval = FALSE;
			break;
	}

	return retval;
}

gboolean
__mmplayer_post_message(mm_player_t* player, enum MMMessageType msgtype, MMMessageParamType* param) // @
{
	return_val_if_fail( player, FALSE );

	if ( !player->msg_cb )
	{
		return FALSE;
	}

	//debug_log("Message (type : %d)  will be posted using msg-cb(%p). \n", msgtype, player->msg_cb);

	player->msg_cb(msgtype, param, player->msg_cb_param);

	return TRUE;
}

gboolean
__mmplayer_handle_gst_error ( mm_player_t* player, GstMessage * message, GError* error )
{
	MMMessageParamType msg_param;
	gchar *msg_src_element;

	MMPLAYER_FENTER();

	return_val_if_fail( player, FALSE );
	return_val_if_fail( error, FALSE );

	/* NOTE : do somthing necessary inside of __gst_handle_XXX_error. not here */

	memset (&msg_param, 0, sizeof(MMMessageParamType));

	if ( error->domain == GST_CORE_ERROR )
	{
		msg_param.code = __gst_handle_core_error( player, error->code );
	}
	else if ( error->domain == GST_LIBRARY_ERROR )
	{
		msg_param.code = __gst_handle_library_error( player, error->code );
	}
	else if ( error->domain == GST_RESOURCE_ERROR )
	{
		msg_param.code = __gst_handle_resource_error( player, error->code );
	}
	else if ( error->domain == GST_STREAM_ERROR )
	{
		msg_param.code = __gst_handle_stream_error( player, error, message );
	}
	else
	{
		debug_warning("This error domain is not defined.\n");

		/* we treat system error as an internal error */
		msg_param.code = MM_ERROR_PLAYER_INVALID_STREAM;
	}

	if ( message->src )
	{
		msg_src_element = GST_ELEMENT_NAME( GST_ELEMENT_CAST( message->src ) );

		msg_param.data = (void *) error->message;

		debug_error("-Msg src : [%s]	Domain : [%s]   Error : [%s]  Code : [%d] is tranlated to error code : [0x%x]\n",
			msg_src_element, g_quark_to_string (error->domain), error->message, error->code, msg_param.code);
	}

	/* no error */
	if (msg_param.code == MM_ERROR_NONE)
		return TRUE;

	/* post error to application */
	if ( ! player->msg_posted )
	{
		MMPLAYER_POST_MSG( player, MM_MESSAGE_ERROR, &msg_param );
		/* don't post more if one was sent already */
		player->msg_posted = TRUE;
	}
	else
	{
		debug_log("skip error post because it's sent already.\n");
	}

	MMPLAYER_FLEAVE();

	return TRUE;
}

gint
__gst_handle_core_error( mm_player_t* player, int code )
{
	gint trans_err = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	switch ( code )
	{
		case GST_CORE_ERROR_MISSING_PLUGIN:
			return MM_ERROR_PLAYER_NOT_SUPPORTED_FORMAT;
		case GST_CORE_ERROR_STATE_CHANGE:
		case GST_CORE_ERROR_SEEK:
        	case GST_CORE_ERROR_NOT_IMPLEMENTED:
		case GST_CORE_ERROR_FAILED:
		case GST_CORE_ERROR_TOO_LAZY:
		case GST_CORE_ERROR_PAD:
		case GST_CORE_ERROR_THREAD:
		case GST_CORE_ERROR_NEGOTIATION:
		case GST_CORE_ERROR_EVENT:
		case GST_CORE_ERROR_CAPS:
		case GST_CORE_ERROR_TAG:
		case GST_CORE_ERROR_CLOCK:
		case GST_CORE_ERROR_DISABLED:
		default:
			trans_err = MM_ERROR_PLAYER_INVALID_STREAM;
		break;
	}

	MMPLAYER_FLEAVE();

	return trans_err;
}

gint
__gst_handle_library_error( mm_player_t* player, int code )
{
	gint trans_err = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	switch ( code )
	{
		case GST_LIBRARY_ERROR_FAILED:
		case GST_LIBRARY_ERROR_TOO_LAZY:
		case GST_LIBRARY_ERROR_INIT:
		case GST_LIBRARY_ERROR_SHUTDOWN:
		case GST_LIBRARY_ERROR_SETTINGS:
		case GST_LIBRARY_ERROR_ENCODE:
		default:
			trans_err =  MM_ERROR_PLAYER_INVALID_STREAM;
		break;
	}

	MMPLAYER_FLEAVE();

	return trans_err;
}


gint
__gst_handle_resource_error( mm_player_t* player, int code )
{
	gint trans_err = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	switch ( code )
	{
		case GST_RESOURCE_ERROR_NO_SPACE_LEFT:
			trans_err = MM_ERROR_PLAYER_NO_FREE_SPACE;
			break;
		case GST_RESOURCE_ERROR_NOT_FOUND:
		case GST_RESOURCE_ERROR_OPEN_READ:
			if ( MMPLAYER_IS_HTTP_STREAMING(player) || MMPLAYER_IS_HTTP_LIVE_STREAMING ( player )
				|| MMPLAYER_IS_RTSP_STREAMING(player))
			{
				trans_err = MM_ERROR_PLAYER_STREAMING_CONNECTION_FAIL;
				break;
			}
		case GST_RESOURCE_ERROR_READ:
			if ( MMPLAYER_IS_HTTP_STREAMING(player) ||  MMPLAYER_IS_HTTP_LIVE_STREAMING ( player )
				|| MMPLAYER_IS_RTSP_STREAMING(player))
			{
				trans_err = MM_ERROR_PLAYER_STREAMING_FAIL;
				break;
			}
		case GST_RESOURCE_ERROR_WRITE:
		case GST_RESOURCE_ERROR_FAILED:
		case GST_RESOURCE_ERROR_SEEK:
		case GST_RESOURCE_ERROR_TOO_LAZY:
		case GST_RESOURCE_ERROR_BUSY:
		case GST_RESOURCE_ERROR_OPEN_WRITE:
		case GST_RESOURCE_ERROR_OPEN_READ_WRITE:
		case GST_RESOURCE_ERROR_CLOSE:
		case GST_RESOURCE_ERROR_SYNC:
		case GST_RESOURCE_ERROR_SETTINGS:
		default:
			trans_err = MM_ERROR_PLAYER_INTERNAL;
		break;
	}

	MMPLAYER_FLEAVE();

	return trans_err;
}


gint
__gst_handle_stream_error( mm_player_t* player, GError* error, GstMessage * message )
{
	gint trans_err = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail( error, MM_ERROR_INVALID_ARGUMENT );
	return_val_if_fail ( message, MM_ERROR_INVALID_ARGUMENT );

	switch ( error->code )
	{
		case GST_STREAM_ERROR_FAILED:
		case GST_STREAM_ERROR_TYPE_NOT_FOUND:
		case GST_STREAM_ERROR_DECODE:
		case GST_STREAM_ERROR_WRONG_TYPE:
		case GST_STREAM_ERROR_DECRYPT:
		case GST_STREAM_ERROR_DECRYPT_NOKEY:
		case GST_STREAM_ERROR_CODEC_NOT_FOUND:
			 trans_err = __gst_transform_gsterror( player, message, error );
		break;

		case GST_STREAM_ERROR_NOT_IMPLEMENTED:
		case GST_STREAM_ERROR_TOO_LAZY:
		case GST_STREAM_ERROR_ENCODE:
		case GST_STREAM_ERROR_DEMUX:
		case GST_STREAM_ERROR_MUX:
		case GST_STREAM_ERROR_FORMAT:
		default:
			trans_err = MM_ERROR_PLAYER_INVALID_STREAM;
		break;
	}

	MMPLAYER_FLEAVE();

	return trans_err;
}

/* NOTE : decide gstreamer state whether there is some playable track or not. */
static gint
__gst_transform_gsterror( mm_player_t* player, GstMessage * message, GError* error )
{
	gchar *src_element_name = NULL;
	GstElement *src_element = NULL;
	GstElementFactory *factory = NULL;
	const gchar* klass = NULL;

	MMPLAYER_FENTER();

	/* FIXIT */
	return_val_if_fail ( message, MM_ERROR_INVALID_ARGUMENT );
	return_val_if_fail ( message->src, MM_ERROR_INVALID_ARGUMENT );
	return_val_if_fail ( error, MM_ERROR_INVALID_ARGUMENT );

	src_element = GST_ELEMENT_CAST(message->src);
	if ( !src_element )
		goto INTERNAL_ERROR;

	src_element_name = GST_ELEMENT_NAME(src_element);
	if ( !src_element_name )
		goto INTERNAL_ERROR;

	factory = gst_element_get_factory(src_element);
	if ( !factory )
		goto INTERNAL_ERROR;

	klass = gst_element_factory_get_metadata (factory, GST_ELEMENT_METADATA_KLASS);
	if ( !klass )
		goto INTERNAL_ERROR;

	debug_log("error code=%d, msg=%s, src element=%s, class=%s\n",
			error->code, error->message, src_element_name, klass);

	//<-
	{
		if (player->selector) {
			int msg_src_pos = 0;
			gint active_pad_index = player->selector[MM_PLAYER_TRACK_TYPE_AUDIO].active_pad_index;
			debug_log ("current  active pad index  -%d", active_pad_index);

			if  (src_element_name) {
				int idx = 0;

				if (player->audio_decoders) {
					GList *adec = player->audio_decoders;
					for ( ;adec ; adec = g_list_next(adec)) {
						gchar *name = adec->data;

						debug_log("found audio decoder name  = %s", name);
						if (g_strrstr(name, src_element_name)) {
							msg_src_pos = idx;
							break;
						}
						idx++;
					}
				}
				debug_log("active pad = %d, error src index = %d", active_pad_index,  msg_src_pos);
			}

			if (active_pad_index != msg_src_pos) {
				debug_log("skip error because error is posted from no activated track");
				return MM_ERROR_NONE;
			}
		}
	}
	//-> temp code

	switch ( error->code )
	{
		case GST_STREAM_ERROR_DECODE:
		{
			/* Demuxer can't parse one track because it's corrupted.
			 * So, the decoder for it is not linked.
			 * But, it has one playable track.
			 */
			if ( g_strrstr(klass, "Demux") )
			{
				if ( player->can_support_codec == FOUND_PLUGIN_VIDEO )
				{
					return MM_ERROR_PLAYER_AUDIO_CODEC_NOT_FOUND;
				}
				else if ( player->can_support_codec == FOUND_PLUGIN_AUDIO )
				{
					return MM_ERROR_PLAYER_VIDEO_CODEC_NOT_FOUND;
				}
				else
				{
					if ( player->pipeline->audiobin ) // PCM
					{
						return MM_ERROR_PLAYER_VIDEO_CODEC_NOT_FOUND;
					}
					else
					{
						goto CODEC_NOT_FOUND;
					}
				}
			}
			return MM_ERROR_PLAYER_INVALID_STREAM;
		}
		break;

		case GST_STREAM_ERROR_CODEC_NOT_FOUND:
		case GST_STREAM_ERROR_TYPE_NOT_FOUND:
		case GST_STREAM_ERROR_WRONG_TYPE:
			return MM_ERROR_PLAYER_NOT_SUPPORTED_FORMAT;

		case GST_STREAM_ERROR_FAILED:
		{
			/* Decoder Custom Message */
			if ( strstr(error->message, "ongoing") )
			{
				if ( strncasecmp(klass, "audio", 5) )
				{
					if ( ( player->can_support_codec & FOUND_PLUGIN_VIDEO ) )
					{
						debug_log("Video can keep playing.\n");
						return MM_ERROR_PLAYER_AUDIO_CODEC_NOT_FOUND;
					}
					else
					{
						goto CODEC_NOT_FOUND;
					}

				}
				else if ( strncasecmp(klass, "video", 5) )
				{
					if ( ( player->can_support_codec & FOUND_PLUGIN_AUDIO ) )
					{
						debug_log("Audio can keep playing.\n");
						return MM_ERROR_PLAYER_VIDEO_CODEC_NOT_FOUND;
					}
					else
					{
						goto CODEC_NOT_FOUND;
					}
				}
			}
			return MM_ERROR_PLAYER_NOT_SUPPORTED_FORMAT;
		}
		break;

		case GST_STREAM_ERROR_DECRYPT:
		case GST_STREAM_ERROR_DECRYPT_NOKEY:
		{
			debug_error("decryption error, [%s] failed, reason : [%s]\n", src_element_name, error->message);

			if ( strstr(error->message, "rights expired") )
			{
				return MM_ERROR_PLAYER_DRM_EXPIRED;
			}
			else if ( strstr(error->message, "no rights") )
			{
				return MM_ERROR_PLAYER_DRM_NO_LICENSE;
			}
			else if ( strstr(error->message, "has future rights") )
			{
				return MM_ERROR_PLAYER_DRM_FUTURE_USE;
			}
			else if ( strstr(error->message, "opl violation") )
			{
				return MM_ERROR_PLAYER_DRM_OUTPUT_PROTECTION;
			}
			return MM_ERROR_PLAYER_DRM_NOT_AUTHORIZED;
		}
		break;

		default:
		break;
	}

	MMPLAYER_FLEAVE();

	return MM_ERROR_PLAYER_INVALID_STREAM;

INTERNAL_ERROR:
	return MM_ERROR_PLAYER_INTERNAL;

CODEC_NOT_FOUND:
	debug_log("not found any available codec. Player should be destroyed.\n");
	return MM_ERROR_PLAYER_CODEC_NOT_FOUND;
}

static gboolean
__mmplayer_get_property_value_for_rotation(mm_player_t* player, int rotation_angle, int *value)
{
	int pro_value = 0; // in the case of expection, default will be returned.
	int dest_angle = rotation_angle;
	int rotation_type = -1;
	#define ROTATION_USING_SINK 0
	#define ROTATION_USING_CUSTOM 1
	#define ROTATION_USING_FLIP	2

	return_val_if_fail(player, FALSE);
	return_val_if_fail(value, FALSE);
	return_val_if_fail(rotation_angle >= 0, FALSE);

	if (rotation_angle >= 360)
	{
		dest_angle = rotation_angle - 360;
	}

	/* chech if supported or not */
	if ( dest_angle % 90 )
	{
		debug_log("not supported rotation angle = %d", rotation_angle);
		return FALSE;
	}

	/*
	  * xvimagesink only 	 (A)
	  * custom_convert - no xv (e.g. memsink, evasimagesink	 (B)
	  * videoflip - avsysmemsink (C)
	  */
	if (player->set_mode.video_zc)
	{
		if (player->pipeline->videobin[MMPLAYER_V_CONV].gst) // B
		{
			rotation_type = ROTATION_USING_CUSTOM;
		}
		else // A
		{
			rotation_type = ROTATION_USING_SINK;
		}
	}
	else
	{
		int surface_type = 0;
		rotation_type = ROTATION_USING_FLIP;

		mm_attrs_get_int_by_name(player->attrs, "display_surface_type", &surface_type);
		debug_log("check display surface type attribute: %d", surface_type);

		if ((surface_type == MM_DISPLAY_SURFACE_X) ||
			(surface_type == MM_DISPLAY_SURFACE_EVAS && !strcmp(player->ini.videosink_element_evas, "evaspixmapsink")))
		{
			rotation_type = ROTATION_USING_SINK;
		}
		else
		{
			rotation_type = ROTATION_USING_FLIP; //C
		}

		debug_log("using %d type for rotation", rotation_type);
	}

	/* get property value for setting */
	switch(rotation_type)
	{
		case ROTATION_USING_SINK: // xvimagesink, pixmap
			{
				switch (dest_angle)
				{
					case 0:
						break;
					case 90:
						pro_value = 3; // clockwise 90
						break;
					case 180:
						pro_value = 2;
						break;
					case 270:
						pro_value = 1; // counter-clockwise 90
						break;
				}
			}
			break;
		case ROTATION_USING_CUSTOM:
			{
				gchar *ename = NULL;
				ename = GST_OBJECT_NAME(gst_element_get_factory(player->pipeline->videobin[MMPLAYER_V_CONV].gst));

				if (g_strrstr(ename, "fimcconvert"))
				{
					switch (dest_angle)
					{
						case 0:
							break;
						case 90:
							pro_value = 90; // clockwise 90
							break;
						case 180:
							pro_value = 180;
							break;
						case 270:
							pro_value = 270; // counter-clockwise 90
							break;
					}
				}
			}
			break;
		case ROTATION_USING_FLIP: // videoflip
			{
					switch (dest_angle)
					{

						case 0:
							break;
						case 90:
							pro_value = 1; // clockwise 90
							break;
						case 180:
							pro_value = 2;
							break;
						case 270:
							pro_value = 3; // counter-clockwise 90
							break;
					}
			}
			break;
	}

	debug_log("setting rotation property value : %d, used rotation type : %d", pro_value, rotation_type);

	*value = pro_value;

	return TRUE;
}

int
_mmplayer_update_video_param(mm_player_t* player) // @
{
	MMHandleType attrs = 0;
	int surface_type = 0;
	int org_angle = 0; // current supported angle values are 0, 90, 180, 270
	int user_angle = 0;
	int user_angle_type= 0;
	int rotation_value = 0;
	gchar *org_orient = NULL;

	MMPLAYER_FENTER();

	/* check video sinkbin is created */
	return_val_if_fail ( player &&
		player->pipeline &&
		player->pipeline->videobin &&
		player->pipeline->videobin[MMPLAYER_V_BIN].gst &&
		player->pipeline->videobin[MMPLAYER_V_SINK].gst,
		MM_ERROR_PLAYER_NOT_INITIALIZED );

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("cannot get content attribute");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	/* update user roation */
	mm_attrs_get_int_by_name(attrs, "display_rotation", &user_angle_type);

	/* get angle with user type */
	switch(user_angle_type)
	{
		case MM_DISPLAY_ROTATION_NONE:
			user_angle = 0;
			break;
		case MM_DISPLAY_ROTATION_90: // counter-clockwise 90
			user_angle = 270;
			break;
		case MM_DISPLAY_ROTATION_180:
			user_angle = 180;
			break;
		case MM_DISPLAY_ROTATION_270: // clockwise 90
			user_angle = 90;
			break;
	}

	/* get original orientation */
	mm_attrs_get_string_by_name(attrs, "content_video_orientation", &org_orient);

	if (org_orient)
	{
		if (!strcmp (org_orient, "rotate-90"))
			org_angle = 90;
		else if (!strcmp (org_orient, "rotate-180"))
			org_angle = 180;
		else if (!strcmp (org_orient, "rotate-270"))
			org_angle = 270;
		else
			debug_log ("original rotation is %s", org_orient);
	}
	else
	{
		debug_log ("content_video_orientation get fail");
	}

	debug_log("check user angle: %d, orientation: %d", user_angle, org_angle);

	/* check video stream callback is used */
	if(!player->set_mode.media_packet_video_stream && player->use_video_stream )
	{
		if (player->set_mode.video_zc)
		{
			gchar *ename = NULL;
			int width = 0;
			int height = 0;

			mm_attrs_get_int_by_name(attrs, "display_width", &width);
			mm_attrs_get_int_by_name(attrs, "display_height", &height);

			/* resize video frame with requested values for fimcconvert */
			ename = GST_OBJECT_NAME(gst_element_get_factory(player->pipeline->videobin[MMPLAYER_V_CONV].gst));

			if (ename && g_strrstr(ename, "fimcconvert"))
			{
				if (width)
					g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "dst-width", width, NULL);

				if (height)
					g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "dst-height", height, NULL);

				/* NOTE: fimcconvert does not manage index of src buffer from upstream src-plugin, decoder gives frame information in output buffer with no ordering */
				g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "src-rand-idx", TRUE, NULL);

				/* get rotation value to set */
				__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);

				g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "rotate", rotation_value, NULL);

				debug_log("updating fimcconvert - r[%d], w[%d], h[%d]", rotation_value, width, height);
			}
		}
		else
		{
			debug_log("using video stream callback with memsink. player handle : [%p]", player);

			/* get rotation value to set */
			__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);

			g_object_set(player->pipeline->videobin[MMPLAYER_V_FLIP].gst, "method", rotation_value, NULL);
		}

		return MM_ERROR_NONE;
	}

	/* update display surface */
	mm_attrs_get_int_by_name(attrs, "display_surface_type", &surface_type);
	debug_log("check display surface type attribute: %d", surface_type);

	/* configuring display */
	switch ( surface_type )
	{
		case MM_DISPLAY_SURFACE_X:
		{
			/* ximagesink or xvimagesink */
			void *surface = NULL;
			int display_method = 0;
			int roi_x = 0;
			int roi_y = 0;
			int roi_w = 0;
			int roi_h = 0;
			int src_crop_x = 0;
			int src_crop_y = 0;
			int src_crop_w = 0;
			int src_crop_h = 0;
			int force_aspect_ratio = 0;
			gboolean visible = TRUE;

#ifdef HAVE_WAYLAND
			/*set wl_display*/
			void* wl_display = NULL;
			GstContext *context = NULL;
			int wl_window_x = 0;
			int wl_window_y = 0;
			int wl_window_width = 0;
			int wl_window_height = 0;

			mm_attrs_get_data_by_name(attrs, "wl_display", &wl_display);
			if (wl_display)
				context = gst_wayland_display_handle_context_new(wl_display);
			if (context)
				gst_element_set_context(GST_ELEMENT(player->pipeline->videobin[MMPLAYER_V_SINK].gst), context);

			/*It should be set after setting window*/
			mm_attrs_get_int_by_name(attrs, "wl_window_render_x", &wl_window_x);
			mm_attrs_get_int_by_name(attrs, "wl_window_render_y", &wl_window_y);
			mm_attrs_get_int_by_name(attrs, "wl_window_render_width", &wl_window_width);
			mm_attrs_get_int_by_name(attrs, "wl_window_render_height", &wl_window_height);
#endif
			/* common case if using x surface */
			mm_attrs_get_data_by_name(attrs, "display_overlay", &surface);
			if ( surface )
			{
#ifdef HAVE_WAYLAND
				guintptr wl_surface = (guintptr)surface;
				debug_log("set video param : wayland surface %p", surface);
				gst_video_overlay_set_window_handle(
						GST_VIDEO_OVERLAY( player->pipeline->videobin[MMPLAYER_V_SINK].gst ),
						wl_surface );
				/* After setting window handle, set render	rectangle */
				gst_video_overlay_set_render_rectangle(
					 GST_VIDEO_OVERLAY( player->pipeline->videobin[MMPLAYER_V_SINK].gst ),
					 wl_window_x,wl_window_y,wl_window_width,wl_window_height);
#else // HAVE_X11
				int xwin_id = 0;
				xwin_id = *(int*)surface;
				debug_log("set video param : xid %p", *(int*)surface);
				if (xwin_id)
				{
					gst_video_overlay_set_window_handle( GST_VIDEO_OVERLAY( player->pipeline->videobin[MMPLAYER_V_SINK].gst ), *(int*)surface );
				}
#endif
			}
			else
			{
				/* FIXIT : is it error case? */
				debug_warning("still we don't have xid on player attribute. create it's own surface.");
			}

			/* if xvimagesink */
			if (!strcmp(player->ini.videosink_element_x,"xvimagesink"))
			{
				mm_attrs_get_int_by_name(attrs, "display_force_aspect_ration", &force_aspect_ratio);
				mm_attrs_get_int_by_name(attrs, "display_method", &display_method);
				mm_attrs_get_int_by_name(attrs, "display_src_crop_x", &src_crop_x);
				mm_attrs_get_int_by_name(attrs, "display_src_crop_y", &src_crop_y);
				mm_attrs_get_int_by_name(attrs, "display_src_crop_width", &src_crop_w);
				mm_attrs_get_int_by_name(attrs, "display_src_crop_height", &src_crop_h);
				mm_attrs_get_int_by_name(attrs, "display_roi_x", &roi_x);
				mm_attrs_get_int_by_name(attrs, "display_roi_y", &roi_y);
				mm_attrs_get_int_by_name(attrs, "display_roi_width", &roi_w);
				mm_attrs_get_int_by_name(attrs, "display_roi_height", &roi_h);
				mm_attrs_get_int_by_name(attrs, "display_visible", &visible);
				#define DEFAULT_DISPLAY_MODE	2	// TV only, PRI_VIDEO_OFF_AND_SEC_VIDEO_FULL_SCREEN

				/* setting for cropping media source */
				if (src_crop_w && src_crop_h)
				{
					g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
						"src-crop-x", src_crop_x,
						"src-crop-y", src_crop_y,
						"src-crop-w", src_crop_w,
						"src-crop-h", src_crop_h,
						NULL );
				}

				/* setting for ROI mode */
				if (display_method == 5)	// 5 for ROI mode
				{
					int roi_mode = 0;
					mm_attrs_get_int_by_name(attrs, "display_roi_mode", &roi_mode);
					g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
						"dst-roi-mode", roi_mode,
						"dst-roi-x", roi_x,
						"dst-roi-y", roi_y,
						"dst-roi-w", roi_w,
						"dst-roi-h", roi_h,
						NULL );
					/* get rotation value to set,
					   do not use org_angle because ROI mode in xvimagesink needs both a rotation value and an orientation value */
					__mmplayer_get_property_value_for_rotation(player, user_angle, &rotation_value);
				}
				else
				{
					/* get rotation value to set */
					__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);
				}

				g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
					"force-aspect-ratio", force_aspect_ratio,
					"orientation", org_angle/90, // setting for orientation of media, it is used for ROI/ZOOM feature in xvimagesink
					"rotate", rotation_value,
					"handle-events", TRUE,
					"display-geometry-method", display_method,
					"draw-borders", FALSE,
					"handle-expose", FALSE,
					"visible", visible,
					"display-mode", DEFAULT_DISPLAY_MODE,
					NULL );

				debug_log("set video param : rotate %d, method %d visible %d", rotation_value, display_method, visible);
				debug_log("set video param : dst-roi-x: %d, dst-roi-y: %d, dst-roi-w: %d, dst-roi-h: %d", roi_x, roi_y, roi_w, roi_h );
				debug_log("set video param : force aspect ratio %d, display mode %d", force_aspect_ratio, DEFAULT_DISPLAY_MODE);
			}
		}
		break;
		case MM_DISPLAY_SURFACE_EVAS:
		{
			void *object = NULL;
			int scaling = 0;
			gboolean visible = TRUE;
			int display_method = 0;

			/* common case if using evas surface */
			mm_attrs_get_data_by_name(attrs, "display_overlay", &object);
			mm_attrs_get_int_by_name(attrs, "display_visible", &visible);
			mm_attrs_get_int_by_name(attrs, "display_evas_do_scaling", &scaling);
			mm_attrs_get_int_by_name(attrs, "display_method", &display_method);

			/* if evasimagesink */
			if (!strcmp(player->ini.videosink_element_evas,"evasimagesink"))
			{
				if (object)
				{
					/* if it is evasimagesink, we are not supporting rotation */
					if (user_angle_type!=MM_DISPLAY_ROTATION_NONE)
					{
						mm_attrs_set_int_by_name(attrs, "display_rotation", MM_DISPLAY_ROTATION_NONE);
						if (mmf_attrs_commit (attrs)) /* return -1 if error */
							debug_error("failed to commit\n");
						debug_warning("unsupported feature");
						return MM_ERROR_NOT_SUPPORT_API;
					}
					__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);
					g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
							"evas-object", object,
							"visible", visible,
							"display-geometry-method", display_method,
							"rotate", rotation_value,
							NULL);
					debug_log("set video param : method %d", display_method);
					debug_log("set video param : evas-object %x, visible %d", object, visible);
					debug_log("set video param : evas-object %x, rotate %d", object, rotation_value);
				}
				else
				{
					debug_error("no evas object");
					return MM_ERROR_PLAYER_INTERNAL;
				}


				/* if evasimagesink using converter */
				if (player->set_mode.video_zc && player->pipeline->videobin[MMPLAYER_V_CONV].gst)
				{
					int width = 0;
					int height = 0;
					int no_scaling = !scaling;

					mm_attrs_get_int_by_name(attrs, "display_width", &width);
					mm_attrs_get_int_by_name(attrs, "display_height", &height);

					/* NOTE: fimcconvert does not manage index of src buffer from upstream src-plugin, decoder gives frame information in output buffer with no ordering */
					g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "src-rand-idx", TRUE, NULL);
					g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "dst-buffer-num", 5, NULL);

					if (no_scaling)
					{
						/* no-scaling order to fimcconvert, original width, height size of media src will be passed to sink plugin */
						g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst,
								"dst-width", 0, /* setting 0, output video width will be media src's width */
								"dst-height", 0, /* setting 0, output video height will be media src's height */
								NULL);
					}
					else
					{
						/* scaling order to fimcconvert */
						if (width)
						{
							g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "dst-width", width, NULL);
						}
						if (height)
						{
							g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "dst-height", height, NULL);
						}
						debug_log("set video param : video frame scaling down to width(%d) height(%d)", width, height);
					}
					debug_log("set video param : display_evas_do_scaling %d", scaling);
				}
			}

			/* if evaspixmapsink */
			if (!strcmp(player->ini.videosink_element_evas,"evaspixmapsink"))
			{
				if (object)
				{
					__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);
					g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
							"evas-object", object,
							"visible", visible,
							"display-geometry-method", display_method,
							"rotate", rotation_value,
							NULL);
					debug_log("set video param : method %d", display_method);
					debug_log("set video param : evas-object %x, visible %d", object, visible);
					debug_log("set video param : evas-object %x, rotate %d", object, rotation_value);
				}
				else
				{
					debug_error("no evas object");
					return MM_ERROR_PLAYER_INTERNAL;
				}

				int display_method = 0;
				int roi_x = 0;
				int roi_y = 0;
				int roi_w = 0;
				int roi_h = 0;
				int origin_size = !scaling;

				mm_attrs_get_int_by_name(attrs, "display_method", &display_method);
				mm_attrs_get_int_by_name(attrs, "display_roi_x", &roi_x);
				mm_attrs_get_int_by_name(attrs, "display_roi_y", &roi_y);
				mm_attrs_get_int_by_name(attrs, "display_roi_width", &roi_w);
				mm_attrs_get_int_by_name(attrs, "display_roi_height", &roi_h);

				/* get rotation value to set */
				__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);

				g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
					"origin-size", origin_size,
					"rotate", rotation_value,
					"dst-roi-x", roi_x,
					"dst-roi-y", roi_y,
					"dst-roi-w", roi_w,
					"dst-roi-h", roi_h,
					"display-geometry-method", display_method,
					NULL );

				debug_log("set video param : method %d", display_method);
				debug_log("set video param : dst-roi-x: %d, dst-roi-y: %d, dst-roi-w: %d, dst-roi-h: %d",
								roi_x, roi_y, roi_w, roi_h );
				debug_log("set video param : display_evas_do_scaling %d (origin-size %d)", scaling, origin_size);
			}
		}
		break;
		case MM_DISPLAY_SURFACE_X_EXT:	/* NOTE : this surface type is used for the videoTexture(canvasTexture) overlay */
		{
			void *pixmap_id_cb = NULL;
			void *pixmap_id_cb_user_data = NULL;
			int display_method = 0;
			gboolean visible = TRUE;

			/* if xvimagesink */
			if (strcmp(player->ini.videosink_element_x,"xvimagesink"))
			{
				debug_error("videosink is not xvimagesink");
				return MM_ERROR_PLAYER_INTERNAL;
			}

			/* get information from attributes */
			mm_attrs_get_data_by_name(attrs, "display_overlay", &pixmap_id_cb);
			mm_attrs_get_data_by_name(attrs, "display_overlay_user_data", &pixmap_id_cb_user_data);
			mm_attrs_get_int_by_name(attrs, "display_method", &display_method);
			mm_attrs_get_int_by_name(attrs, "display_visible", &visible);

			if ( pixmap_id_cb )
			{
				debug_log("set video param : display_overlay(0x%x)", pixmap_id_cb);
				if (pixmap_id_cb_user_data)
				{
					debug_log("set video param : display_overlay_user_data(0x%x)", pixmap_id_cb_user_data);
				}
			}
			else
			{
				debug_error("failed to set pixmap-id-callback");
				return MM_ERROR_PLAYER_INTERNAL;
			}
			/* get rotation value to set */
			__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);

			debug_log("set video param : rotate %d, method %d, visible %d", rotation_value, display_method, visible);

			/* set properties of videosink plugin */
			g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
				"display-geometry-method", display_method,
				"draw-borders", FALSE,
				"visible", visible,
				"rotate", rotation_value,
				"pixmap-id-callback", pixmap_id_cb,
				"pixmap-id-callback-userdata", pixmap_id_cb_user_data,
				NULL );
		}
		break;
		case MM_DISPLAY_SURFACE_NULL:
		{
			/* do nothing */
		}
		break;
		case MM_DISPLAY_SURFACE_REMOTE:
		{
			/* do nothing */
		}
		break;
	}

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}



int _mmplayer_set_shm_stream_path(MMHandleType hplayer, const char *path)
{
	mm_player_t* player = (mm_player_t*) hplayer;
	int result;

	MMPLAYER_FENTER();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail(path, MM_ERROR_INVALID_ARGUMENT);

	result = mm_attrs_set_string_by_name(player->attrs, "shm_stream_path", path)

	MMPLAYER_FLEAVE();
	return result;
}