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
|																							|
========================================================================================== */
#ifdef HAVE_WAYLAND
#include <gst/wayland/wayland.h>
#endif
#include <gst/video/videooverlay.h>

#include <dlog.h>
#include <mm_error.h>
#include <mm_attrs_private.h>

#include "mm_player_priv.h"
#include "mm_player_attrs.h"
#include "mm_player_utils.h"

/*===========================================================================================
|																							|
|  LOCAL DEFINITIONS AND DECLARATIONS FOR MODULE											|
|																							|
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

	MMPLAYER_RETURN_VAL_IF_FAIL(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	//LOGD("incomming command : %d \n", command );

	current_state = MMPLAYER_CURRENT_STATE(player);
	pending_state = MMPLAYER_PENDING_STATE(player);

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
				LOGD("player is going to paused state, just change the pending state as playing");
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
					LOGD("player is PAUSED going to PLAYING, just change the pending state as PAUSED");
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
				LOGD("player is going to paused state, just change the pending state as playing");
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
	LOGW("since player is in wrong state(%s). it's not able to apply the command(%d)",
		MMPLAYER_STATE_GET_NAME(current_state), command);
	return MM_ERROR_PLAYER_INVALID_STATE;

NOT_COMPLETED_SEEK:
	LOGW("not completed seek");
	return MM_ERROR_PLAYER_DOING_SEEK;

NO_OP:
	LOGW("player is in the desired state(%s). doing noting", MMPLAYER_STATE_GET_NAME(current_state));
	return MM_ERROR_PLAYER_NO_OP;

ALREADY_GOING:
	LOGW("player is already going to %s, doing nothing", MMPLAYER_STATE_GET_NAME(pending_state));
	return MM_ERROR_PLAYER_NO_OP;
}

int
__mmplayer_gst_set_state (mm_player_t* player, GstElement * element,  GstState state, gboolean async, gint timeout)
{
	GstState element_state = GST_STATE_VOID_PENDING;
	GstState element_pending_state = GST_STATE_VOID_PENDING;
	GstStateChangeReturn ret = GST_STATE_CHANGE_FAILURE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	MMPLAYER_RETURN_VAL_IF_FAIL ( element, MM_ERROR_INVALID_ARGUMENT );

	LOGD("setting [%s] element state to : %s\n", GST_ELEMENT_NAME(element), gst_element_state_get_name(state));

	/* set state */
	ret = gst_element_set_state(element, state);

	if ( ret == GST_STATE_CHANGE_FAILURE )
	{
		LOGE("failed to set [%s] state\n", GST_ELEMENT_NAME(element));

		/* dump state of all element */
		__mmplayer_dump_pipeline_state( player );

		return MM_ERROR_PLAYER_INTERNAL;
	}

	/* return here so state transition to be done in async mode */
	if ( async )
	{
		LOGD("async state transition. not waiting for state complete.\n");
		return MM_ERROR_NONE;
	}

	/* wait for state transition */
	ret = gst_element_get_state( element, &element_state, &element_pending_state, timeout * GST_SECOND );

	if ( ret == GST_STATE_CHANGE_FAILURE || ( state != element_state ) )
	{
		LOGE("failed to change [%s] element state to [%s] within %d sec\n",
			GST_ELEMENT_NAME(element),
			gst_element_state_get_name(state), timeout );

		LOGE(" [%s] state : %s   pending : %s \n",
			GST_ELEMENT_NAME(element),
			gst_element_state_get_name(element_state),
			gst_element_state_get_name(element_pending_state) );

		/* dump state of all element */
		__mmplayer_dump_pipeline_state( player );

		return MM_ERROR_PLAYER_INTERNAL;
	}

	LOGD("[%s] element state has changed\n", GST_ELEMENT_NAME(element));

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
		LOGW("context: %p, source id: %d, source: %p", context, source_id, source);
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

	MMPLAYER_RETURN_VAL_IF_FAIL ( player &&
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
					LOGE("%s:%s : From:%s To:%s   refcount : %d\n", GST_OBJECT_NAME(factory) , GST_ELEMENT_NAME(element) ,
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
		LOGE("%s:%s : From:%s To:%s  refcount : %d\n",
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
	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );

	return ( player->profile.uri_type == MM_PLAYER_URI_TYPE_URL_RTSP ) ? TRUE : FALSE;
}

gboolean
__is_wfd_streaming ( mm_player_t* player )
{
  MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );

  return ( player->profile.uri_type == MM_PLAYER_URI_TYPE_URL_WFD ) ? TRUE : FALSE;
}

gboolean
__is_http_streaming ( mm_player_t* player )
{
	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );

	return ( player->profile.uri_type == MM_PLAYER_URI_TYPE_URL_HTTP ) ? TRUE : FALSE;
}

gboolean
__is_streaming ( mm_player_t* player )
{
	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );

  return ( __is_http_progressive_down( player ) || __is_rtsp_streaming ( player ) || __is_wfd_streaming ( player ) || __is_http_streaming ( player )
          || __is_http_live_streaming ( player ) || __is_dash_streaming ( player ) || __is_smooth_streaming(player) ) ? TRUE : FALSE;
}

gboolean
__is_live_streaming ( mm_player_t* player )
{
	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );

	return ( __is_rtsp_streaming ( player ) && player->streaming_type == STREAMING_SERVICE_LIVE ) ? TRUE : FALSE;
}

gboolean
__is_http_live_streaming( mm_player_t* player )
{
	MMPLAYER_RETURN_VAL_IF_FAIL( player, FALSE );

	return ( player->profile.uri_type == MM_PLAYER_URI_TYPE_HLS ) ? TRUE : FALSE;
}

gboolean
__is_dash_streaming ( mm_player_t* player )
{
	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );

	return ( player->profile.uri_type == MM_PLAYER_URI_TYPE_DASH ) ? TRUE : FALSE;
}

gboolean
__is_smooth_streaming ( mm_player_t* player )
{
	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );

	return ( player->profile.uri_type == MM_PLAYER_URI_TYPE_SS ) ? TRUE : FALSE;
}


gboolean
__is_http_progressive_down(mm_player_t* player)
{
	MMPLAYER_RETURN_VAL_IF_FAIL( player, FALSE );

	return ((player->pd_mode) ? TRUE:FALSE);
}

/* if retval is FALSE, it will be dropped for perfomance. */
gboolean
__mmplayer_check_useful_message(mm_player_t *player, GstMessage * message)
{
	gboolean retval = FALSE;

	if ( !(player->pipeline && player->pipeline->mainbin) )
	{
		LOGE("player pipeline handle is null");
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
				LOGD (">>> [%s] Buffering DONE is detected !!\n", GST_OBJECT_NAME(GST_MESSAGE_SRC(message)));
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
__mmplayer_post_message(mm_player_t* player, enum MMMessageType msgtype, MMMessageParamType* param)
{
	MMPLAYER_RETURN_VAL_IF_FAIL( player, FALSE );

	if ( !player->msg_cb )
	{
		return FALSE;
	}

	//LOGD("Message (type : %d)  will be posted using msg-cb(%p). \n", msgtype, player->msg_cb);

	player->msg_cb(msgtype, param, player->msg_cb_param);

	return TRUE;
}

gboolean
__mmplayer_handle_gst_error ( mm_player_t* player, GstMessage * message, GError* error )
{
	MMMessageParamType msg_param;
	gchar *msg_src_element;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL( player, FALSE );
	MMPLAYER_RETURN_VAL_IF_FAIL( error, FALSE );

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
		LOGW("This error domain is not defined.\n");

		/* we treat system error as an internal error */
		msg_param.code = MM_ERROR_PLAYER_INVALID_STREAM;
	}

	if ( message->src )
	{
		msg_src_element = GST_ELEMENT_NAME( GST_ELEMENT_CAST( message->src ) );

		msg_param.data = (void *) error->message;

		LOGE("-Msg src : [%s]	Domain : [%s]   Error : [%s]  Code : [%d] is tranlated to error code : [0x%x]\n",
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
		LOGD("skip error post because it's sent already.\n");
	}

	MMPLAYER_FLEAVE();

	return TRUE;
}

gint
__gst_handle_core_error( mm_player_t* player, int code )
{
	gint trans_err = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

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

	MMPLAYER_RETURN_VAL_IF_FAIL( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

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

	MMPLAYER_RETURN_VAL_IF_FAIL( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

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

	MMPLAYER_RETURN_VAL_IF_FAIL( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	MMPLAYER_RETURN_VAL_IF_FAIL( error, MM_ERROR_INVALID_ARGUMENT );
	MMPLAYER_RETURN_VAL_IF_FAIL ( message, MM_ERROR_INVALID_ARGUMENT );

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

	MMPLAYER_RETURN_VAL_IF_FAIL ( message, MM_ERROR_INVALID_ARGUMENT );
	MMPLAYER_RETURN_VAL_IF_FAIL ( message->src, MM_ERROR_INVALID_ARGUMENT );
	MMPLAYER_RETURN_VAL_IF_FAIL ( error, MM_ERROR_INVALID_ARGUMENT );

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

	LOGD("error code=%d, msg=%s, src element=%s, class=%s\n",
			error->code, error->message, src_element_name, klass);

	//<-
	{
		if (player->selector) {
			int msg_src_pos = 0;
			gint active_pad_index = player->selector[MM_PLAYER_TRACK_TYPE_AUDIO].active_pad_index;
			LOGD ("current  active pad index  -%d", active_pad_index);

			if  (src_element_name) {
				int idx = 0;

				if (player->audio_decoders) {
					GList *adec = player->audio_decoders;
					for ( ;adec ; adec = g_list_next(adec)) {
						gchar *name = adec->data;

						LOGD("found audio decoder name  = %s", name);
						if (g_strrstr(name, src_element_name)) {
							msg_src_pos = idx;
							break;
						}
						idx++;
					}
				}
				LOGD("active pad = %d, error src index = %d", active_pad_index,  msg_src_pos);
			}

			if (active_pad_index != msg_src_pos) {
				LOGD("skip error because error is posted from no activated track");
				return MM_ERROR_NONE;
			}
		}
	}
	//-> check!! if needed or not

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
						LOGD("Video can keep playing.\n");
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
						LOGD("Audio can keep playing.\n");
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
			LOGE("decryption error, [%s] failed, reason : [%s]\n", src_element_name, error->message);

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
	LOGD("not found any available codec. Player should be destroyed.\n");
	return MM_ERROR_PLAYER_CODEC_NOT_FOUND;
}

int
__mmplayer_get_video_angle(mm_player_t* player, int *user_angle, int *org_angle)
{
	int user_angle_type= 0;
	gchar *org_orient = NULL;
	MMHandleType attrs = MMPLAYER_GET_ATTRS(player);

	if ( !attrs )
	{
		LOGE("cannot get content attribute");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	/* update user roation */
	mm_attrs_get_int_by_name(attrs, "display_rotation", &user_angle_type);

	/* get angle with user type */
	switch(user_angle_type)
	{
		case MM_DISPLAY_ROTATION_NONE:
			*user_angle = 0;
			break;
		case MM_DISPLAY_ROTATION_90: // counter-clockwise 90
			*user_angle = 270;
			break;
		case MM_DISPLAY_ROTATION_180:
			*user_angle = 180;
			break;
		case MM_DISPLAY_ROTATION_270: // clockwise 90
			*user_angle = 90;
			break;
	}

	/* get original orientation */
	mm_attrs_get_string_by_name(attrs, "content_video_orientation", &org_orient);

	if (org_orient)
	{
		if (!strcmp (org_orient, "rotate-90"))
			*org_angle = 90;
		else if (!strcmp (org_orient, "rotate-180"))
			*org_angle = 180;
		else if (!strcmp (org_orient, "rotate-270"))
			*org_angle = 270;
		else
			LOGD ("original rotation is %s", org_orient);
	}
	else
	{
		LOGD ("content_video_orientation get fail");
	}

	LOGD("check user angle: %d, orientation: %d", *user_angle, *org_angle);

	return MM_ERROR_NONE;
}

int _mmplayer_set_shm_stream_path(MMHandleType hplayer, const char *path)
{
	mm_player_t* player = (mm_player_t*) hplayer;
	int result;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	MMPLAYER_RETURN_VAL_IF_FAIL(path, MM_ERROR_INVALID_ARGUMENT);

	result = mm_attrs_set_string_by_name(player->attrs, "shm_stream_path", path)

	MMPLAYER_FLEAVE();
	return result;
}
