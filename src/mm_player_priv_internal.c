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

/*===========================================================================================
|																							|
|  FUNCTION DEFINITIONS																		|
|  																							|
========================================================================================== */
/* NOTE : be careful with calling this api. please refer to below glib comment
 * glib comment : Note that there is a bug in GObject that makes this function much
 * less useful than it might seem otherwise. Once gobject is disposed, the callback
 * will no longer be called, but, the signal handler is not currently disconnected.
 * If the instance is itself being freed at the same time than this doesn't matter,
 * since the signal will automatically be removed, but if instance persists,
 * then the signal handler will leak. You should not remove the signal yourself
 * because in a future versions of GObject, the handler will automatically be
 * disconnected.
 *
 * It's possible to work around this problem in a way that will continue to work
 * with future versions of GObject by checking that the signal handler is still
 * connected before disconnected it:
 *
 *	if (g_signal_handler_is_connected (instance, id))
 *	  g_signal_handler_disconnect (instance, id);
 */
void
__mmplayer_release_signal_connection(mm_player_t* player)
{
	GList* sig_list = player->signals;
	MMPlayerSignalItem* item = NULL;

	debug_fenter();

	return_if_fail( player );
	return_if_fail( player->signals );

	for ( ; sig_list; sig_list = sig_list->next )
	{
		item = sig_list->data;

		if ( item && item->obj && GST_IS_ELEMENT(item->obj) )
		{
			debug_log("checking signal connection : [%lud] from [%s]\n", item->sig, GST_OBJECT_NAME( item->obj ));

			if ( g_signal_handler_is_connected ( item->obj, item->sig ) )
			{
				debug_log("signal disconnecting : [%lud] from [%s]\n", item->sig, GST_OBJECT_NAME( item->obj ));
				g_signal_handler_disconnect ( item->obj, item->sig );
			}
		}

		MMPLAYER_FREEIF( item );

	}
	g_list_free ( player->signals );
	player->signals = NULL;

	debug_fleave();

	return;
}

gboolean
__mmplayer_dump_pipeline_state( mm_player_t* player )
{
	GstIterator*iter = NULL;
	gboolean done = FALSE;

	GValue item = { 0, };
	GstElement *element;

	GstElementFactory *factory = NULL;

	GstState state = GST_STATE_VOID_PENDING;
	GstState pending = GST_STATE_VOID_PENDING;
	GstClockTime time = 200*GST_MSECOND;

	debug_fenter();

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
				element = g_value_get_object (&item);
				gst_element_get_state(element,&state, &pending,time);

				factory = gst_element_get_factory (element) ;

				if (factory)
				{
				debug_error("%s:%s : From:%s To:%s   refcount : %d\n", GST_OBJECT_NAME(factory) , GST_ELEMENT_NAME(element) ,
					 	gst_element_state_get_name(state), gst_element_state_get_name(pending) , GST_OBJECT_REFCOUNT_VALUE(element));
				}
				g_value_reset(&item); 
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

	if (!factory)
	{
		debug_error("%s:%s : From:%s To:%s  refcount : %d\n",
			GST_OBJECT_NAME(factory),
			GST_ELEMENT_NAME(element),
			gst_element_state_get_name(state),
			gst_element_state_get_name(pending),
			GST_OBJECT_REFCOUNT_VALUE(element) );
	}

	g_value_unset (&item);

	if ( iter )
		gst_iterator_free (iter);

	debug_fleave();

	return FALSE;
}

int
__mmplayer_gst_set_state (mm_player_t* player, GstElement * element,  GstState state, gboolean async, gint timeout) // @
{
	GstState element_state = GST_STATE_VOID_PENDING;
	GstState element_pending_state = GST_STATE_VOID_PENDING;
	GstStateChangeReturn ret = GST_STATE_CHANGE_FAILURE;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( element, MM_ERROR_INVALID_ARGUMENT );

	debug_log("setting [%s] element state to : %d\n", GST_ELEMENT_NAME(element),  state);

	/* set state */
	ret = gst_element_set_state(element, state);

	if ( ret == GST_STATE_CHANGE_FAILURE )
	{
		debug_error("failed to set  [%s] state to [%d]\n", GST_ELEMENT_NAME(element), state);

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
		if (MMPLAYER_CURRENT_STATE(player) == MM_PLAYER_STATE_READY)
			__mmplayer_release_signal_connection( player );

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

	debug_log("[%s] element state has changed to %s \n",
		GST_ELEMENT_NAME(element),
		gst_element_state_get_name(element_state));

	debug_fleave();

	return MM_ERROR_NONE;
}

void
__mmplayer_cancel_delayed_eos( mm_player_t* player )
{
	debug_fenter();

	return_if_fail( player );

	if ( player->eos_timer )
	{
		__mmplayer_remove_g_source_from_context( player->eos_timer );
	}

	player->eos_timer = 0;

	debug_fleave();

	return;
}

gboolean
__mmplayer_check_subtitle( mm_player_t* player )
{
	MMHandleType attrs = 0;
	char *subtitle_uri = NULL;

	debug_fenter();

	return_val_if_fail( player, FALSE );

	/* get subtitle attribute */
	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
		return FALSE;

	mm_attrs_get_string_by_name(attrs, "subtitle_uri", &subtitle_uri);
	if ( !subtitle_uri || !strlen(subtitle_uri))
		return FALSE;

	debug_log ("subtite uri is %s[%d]\n", subtitle_uri, strlen(subtitle_uri));

	debug_fleave();

	return TRUE;
}

/* NOTE: post "not supported codec message" to application
 * when one codec is not found during AUTOPLUGGING in MSL.
 * So, it's separated with error of __mmplayer_gst_callback().
 * And, if any codec is not found, don't send message here.
 * Because GST_ERROR_MESSAGE is posted by other plugin internally.
 */
int
__mmplayer_handle_missed_plugin(mm_player_t* player)
{
	MMMessageParamType msg_param;
	memset (&msg_param, 0, sizeof(MMMessageParamType));
	gboolean post_msg_direct = FALSE;

	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	debug_log("not_supported_codec = 0x%02x, can_support_codec = 0x%02x\n",
			player->not_supported_codec, player->can_support_codec);

	if( player->not_found_demuxer )
	{
		msg_param.code = MM_ERROR_PLAYER_CODEC_NOT_FOUND;
		msg_param.data = g_strdup_printf("%s", player->unlinked_demuxer_mime);

		MMPLAYER_POST_MSG( player, MM_MESSAGE_ERROR, &msg_param );
		MMPLAYER_FREEIF(msg_param.data);

		return MM_ERROR_NONE;
	}

	if (player->not_supported_codec)
	{
		if ( player->can_support_codec ) // There is one codec to play
		{
			post_msg_direct = TRUE;
		}
		else
		{
			if ( player->pipeline->audiobin ) // Some content has only PCM data in container.
				post_msg_direct = TRUE;
		}

		if ( post_msg_direct )
		{
			MMMessageParamType msg_param;
			memset (&msg_param, 0, sizeof(MMMessageParamType));

			if ( player->not_supported_codec ==  MISSING_PLUGIN_AUDIO )
			{
				debug_warning("not found AUDIO codec, posting error code to application.\n");

				msg_param.code = MM_ERROR_PLAYER_AUDIO_CODEC_NOT_FOUND;
				msg_param.data = g_strdup_printf("%s", player->unlinked_audio_mime);
			}
			else if ( player->not_supported_codec ==  MISSING_PLUGIN_VIDEO )
			{
				debug_warning("not found VIDEO codec, posting error code to application.\n");

				msg_param.code = MM_ERROR_PLAYER_VIDEO_CODEC_NOT_FOUND;
				msg_param.data = g_strdup_printf("%s", player->unlinked_video_mime);
			}

			MMPLAYER_POST_MSG( player, MM_MESSAGE_ERROR, &msg_param );

			MMPLAYER_FREEIF(msg_param.data);

			return MM_ERROR_NONE;
		}
		else // no any supported codec case
		{
			debug_warning("not found any codec, posting error code to application.\n");

			if ( player->not_supported_codec ==  MISSING_PLUGIN_AUDIO )
			{
				msg_param.code = MM_ERROR_PLAYER_AUDIO_CODEC_NOT_FOUND;
				msg_param.data = g_strdup_printf("%s", player->unlinked_audio_mime);
			}
			else
			{
				msg_param.code = MM_ERROR_PLAYER_CODEC_NOT_FOUND;
				msg_param.data = g_strdup_printf("%s, %s", player->unlinked_video_mime, player->unlinked_audio_mime);
			}

			MMPLAYER_POST_MSG( player, MM_MESSAGE_ERROR, &msg_param );

			MMPLAYER_FREEIF(msg_param.data);
		}
	}

	debug_fleave();

	return MM_ERROR_NONE;
}

gboolean
__mmplayer_link_decoder( mm_player_t* player, GstPad *srcpad)
{
	const gchar* name = NULL;
	GstStructure* str = NULL;
	GstCaps* srccaps = NULL;

	debug_fenter();

	return_val_if_fail( player, FALSE );
	return_val_if_fail ( srcpad, FALSE );

	/* to check any of the decoder (video/audio) need to be linked  to parser*/
    srccaps = gst_pad_query_caps( srcpad, NULL );
	if ( !srccaps )
		goto ERROR;

	str = gst_caps_get_structure( srccaps, 0 );
	if ( ! str )
		goto ERROR;

	name = gst_structure_get_name(str);
	if ( ! name )
		goto ERROR;

	if (strstr(name, "video"))
	{
		if(player->videodec_linked)
		{
		    debug_msg("Video decoder already linked\n");
			return FALSE;
		}
	}
	if (strstr(name, "audio"))
	{
		if(player->audiodec_linked)
		{
		    debug_msg("Audio decoder already linked\n");
			return FALSE;
		}
	}

	gst_caps_unref( srccaps );

	debug_fleave();

	return TRUE;

ERROR:
	if ( srccaps )
		gst_caps_unref( srccaps );

	return FALSE;
}

gboolean
__mmplayer_link_sink( mm_player_t* player , GstPad *srcpad)
{
	const gchar* name = NULL;
	GstStructure* str = NULL;
	GstCaps* srccaps = NULL;

	debug_fenter();

	return_val_if_fail ( player, FALSE );
	return_val_if_fail ( srcpad, FALSE );

	/* to check any of the decoder (video/audio) need to be linked	to parser*/
    srccaps = gst_pad_query_caps( srcpad, NULL );
	if ( !srccaps )
		goto ERROR;

	str = gst_caps_get_structure( srccaps, 0 );
	if ( ! str )
		goto ERROR;

	name = gst_structure_get_name(str);
	if ( ! name )
		goto ERROR;

	if (strstr(name, "video"))
	{
		if(player->videosink_linked)
		{
			debug_msg("Video Sink already linked\n");
			return FALSE;
		}
	}
	if (strstr(name, "audio"))
	{
		if(player->audiosink_linked)
		{
			debug_msg("Audio Sink already linked\n");
			return FALSE;
		}
	}
	if (strstr(name, "text"))
	{
		if(player->textsink_linked)
		{
			debug_msg("Text Sink already linked\n");
			return FALSE;
		}
	}

	gst_caps_unref( srccaps );

	debug_fleave();

	return TRUE;
	//return (!player->videosink_linked || !player->audiosink_linked);

ERROR:
	if ( srccaps )
		gst_caps_unref( srccaps );

	return FALSE;
}

gint
__gst_handle_core_error( mm_player_t* player, int code )
{
	gint trans_err = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	switch ( code )
	{
		case GST_CORE_ERROR_STATE_CHANGE:
		case GST_CORE_ERROR_MISSING_PLUGIN:
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

	debug_fleave();

	return trans_err;
}

gint
__gst_handle_library_error( mm_player_t* player, int code )
{
	gint trans_err = MM_ERROR_NONE;

	debug_fenter();

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

	debug_fleave();

	return trans_err;
}

gint
__gst_handle_resource_error( mm_player_t* player, int code )
{
	gint trans_err = MM_ERROR_NONE;

	debug_fenter();

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
			trans_err = MM_ERROR_PLAYER_INTERNAL;
			break;

		case GST_RESOURCE_ERROR_SEEK:
		case GST_RESOURCE_ERROR_TOO_LAZY:
		case GST_RESOURCE_ERROR_BUSY:
		case GST_RESOURCE_ERROR_OPEN_WRITE:
		case GST_RESOURCE_ERROR_OPEN_READ_WRITE:
		case GST_RESOURCE_ERROR_CLOSE:
		case GST_RESOURCE_ERROR_SYNC:
		case GST_RESOURCE_ERROR_SETTINGS:
		default:
			trans_err = MM_ERROR_PLAYER_FILE_NOT_FOUND;
		break;
	}

	debug_fleave();

	return trans_err;
}

gint
__gst_handle_stream_error( mm_player_t* player, GError* error, GstMessage * message )
{
	gint trans_err = MM_ERROR_NONE;

	debug_fenter();

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
			 trans_err = __gst_transform_gsterror( player, message, error );
		break;

		case GST_STREAM_ERROR_CODEC_NOT_FOUND:
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

	debug_fleave();

	return trans_err;
}

/* NOTE : decide gstreamer state whether there is some playable track or not. */
gint
__gst_transform_gsterror( mm_player_t* player, GstMessage * message, GError* error )
{
	gchar *src_element_name = NULL;
	GstElement *src_element = NULL;
	GstElementFactory *factory = NULL;
	const gchar* klass = NULL;

	debug_fenter();

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

	klass = gst_element_factory_get_klass(factory);
	if ( !klass )
		goto INTERNAL_ERROR;

	debug_log("error code=%d, msg=%s, src element=%s, class=%s\n",
			error->code, error->message, src_element_name, klass);

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

		case GST_STREAM_ERROR_WRONG_TYPE:
		{
			return MM_ERROR_PLAYER_NOT_SUPPORTED_FORMAT;
		}
		break;

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

		case GST_STREAM_ERROR_TYPE_NOT_FOUND:
			return MM_ERROR_PLAYER_NOT_SUPPORTED_FORMAT;
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

	debug_fleave();

	return MM_ERROR_PLAYER_INVALID_STREAM;

INTERNAL_ERROR:
	return MM_ERROR_PLAYER_INTERNAL;

CODEC_NOT_FOUND:
	debug_log("not found any available codec. Player should be destroyed.\n");
	return MM_ERROR_PLAYER_CODEC_NOT_FOUND;
}

gboolean
__mmplayer_handle_gst_error ( mm_player_t* player, GstMessage * message, GError* error )
{
	MMMessageParamType msg_param;
       gchar *msg_src_element;

	debug_fenter();

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

	debug_fleave();

	return TRUE;
}

gboolean
__mmplayer_handle_streaming_error  ( mm_player_t* player, GstMessage * message )
{
	debug_log("\n");
	MMMessageParamType msg_param;
	gchar *msg_src_element = NULL;
	GstStructure *s = NULL;
	guint error_id = 0;
	gchar *error_string = NULL;

	debug_fenter();

	return_val_if_fail ( player, FALSE );
	return_val_if_fail ( message, FALSE );

	s = malloc( sizeof(GstStructure) );
	memcpy ( s, gst_message_get_structure ( message ), sizeof(GstStructure));

	if ( !gst_structure_get_uint (s, "error_id", &error_id) )
		error_id = MMPLAYER_STREAMING_ERROR_NONE;

	switch ( error_id )
	{
		case MMPLAYER_STREAMING_ERROR_UNSUPPORTED_AUDIO:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_UNSUPPORTED_AUDIO;
			break;
		case MMPLAYER_STREAMING_ERROR_UNSUPPORTED_VIDEO:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_UNSUPPORTED_VIDEO;
			break;
		case MMPLAYER_STREAMING_ERROR_CONNECTION_FAIL:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_CONNECTION_FAIL;
			break;
		case MMPLAYER_STREAMING_ERROR_DNS_FAIL:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_DNS_FAIL;
			break;
		case MMPLAYER_STREAMING_ERROR_SERVER_DISCONNECTED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_SERVER_DISCONNECTED;
			break;
		case MMPLAYER_STREAMING_ERROR_BAD_SERVER:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_BAD_SERVER;
			break;
		case MMPLAYER_STREAMING_ERROR_INVALID_PROTOCOL:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_INVALID_PROTOCOL;
			break;
		case MMPLAYER_STREAMING_ERROR_INVALID_URL:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_INVALID_URL;
			break;
		case MMPLAYER_STREAMING_ERROR_UNEXPECTED_MSG:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_UNEXPECTED_MSG;
			break;
		case MMPLAYER_STREAMING_ERROR_OUT_OF_MEMORIES:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_OUT_OF_MEMORIES;
			break;
		case MMPLAYER_STREAMING_ERROR_RTSP_TIMEOUT:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_RTSP_TIMEOUT;
			break;
		case MMPLAYER_STREAMING_ERROR_BAD_REQUEST:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_BAD_REQUEST;
			break;
		case MMPLAYER_STREAMING_ERROR_NOT_AUTHORIZED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_NOT_AUTHORIZED;
			break;
		case MMPLAYER_STREAMING_ERROR_PAYMENT_REQUIRED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_PAYMENT_REQUIRED;
			break;
		case MMPLAYER_STREAMING_ERROR_FORBIDDEN:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_FORBIDDEN;
			break;
		case MMPLAYER_STREAMING_ERROR_CONTENT_NOT_FOUND:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_CONTENT_NOT_FOUND;
			break;
		case MMPLAYER_STREAMING_ERROR_METHOD_NOT_ALLOWED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_METHOD_NOT_ALLOWED;
			break;
		case MMPLAYER_STREAMING_ERROR_NOT_ACCEPTABLE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_NOT_ACCEPTABLE;
			break;
		case MMPLAYER_STREAMING_ERROR_PROXY_AUTHENTICATION_REQUIRED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_PROXY_AUTHENTICATION_REQUIRED;
			break;
		case MMPLAYER_STREAMING_ERROR_SERVER_TIMEOUT:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_SERVER_TIMEOUT;
			break;
		case MMPLAYER_STREAMING_ERROR_GONE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_GONE;
			break;
		case MMPLAYER_STREAMING_ERROR_LENGTH_REQUIRED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_LENGTH_REQUIRED;
			break;
		case MMPLAYER_STREAMING_ERROR_PRECONDITION_FAILED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_PRECONDITION_FAILED;
			break;
		case MMPLAYER_STREAMING_ERROR_REQUEST_ENTITY_TOO_LARGE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_REQUEST_ENTITY_TOO_LARGE;
			break;
		case MMPLAYER_STREAMING_ERROR_REQUEST_URI_TOO_LARGE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_REQUEST_URI_TOO_LARGE;
			break;
		case MMPLAYER_STREAMING_ERROR_UNSUPPORTED_MEDIA_TYPE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_UNSUPPORTED_MEDIA_TYPE;
			break;
		case MMPLAYER_STREAMING_ERROR_PARAMETER_NOT_UNDERSTOOD:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_PARAMETER_NOT_UNDERSTOOD;
			break;
		case MMPLAYER_STREAMING_ERROR_CONFERENCE_NOT_FOUND:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_CONFERENCE_NOT_FOUND;
			break;
		case MMPLAYER_STREAMING_ERROR_NOT_ENOUGH_BANDWIDTH:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_NOT_ENOUGH_BANDWIDTH;
			break;
		case MMPLAYER_STREAMING_ERROR_NO_SESSION_ID:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_NO_SESSION_ID;
			break;
		case MMPLAYER_STREAMING_ERROR_METHOD_NOT_VALID_IN_THIS_STATE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_METHOD_NOT_VALID_IN_THIS_STATE;
			break;
		case MMPLAYER_STREAMING_ERROR_HEADER_FIELD_NOT_VALID_FOR_SOURCE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_HEADER_FIELD_NOT_VALID_FOR_SOURCE;
			break;
		case MMPLAYER_STREAMING_ERROR_INVALID_RANGE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_INVALID_RANGE;
			break;
		case MMPLAYER_STREAMING_ERROR_PARAMETER_IS_READONLY:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_PARAMETER_IS_READONLY;
			break;
		case MMPLAYER_STREAMING_ERROR_AGGREGATE_OP_NOT_ALLOWED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_AGGREGATE_OP_NOT_ALLOWED;
			break;
		case MMPLAYER_STREAMING_ERROR_ONLY_AGGREGATE_OP_ALLOWED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_ONLY_AGGREGATE_OP_ALLOWED;
			break;
		case MMPLAYER_STREAMING_ERROR_BAD_TRANSPORT:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_BAD_TRANSPORT;
			break;
		case MMPLAYER_STREAMING_ERROR_DESTINATION_UNREACHABLE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_DESTINATION_UNREACHABLE;
			break;
		case MMPLAYER_STREAMING_ERROR_INTERNAL_SERVER_ERROR:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_INTERNAL_SERVER_ERROR;
			break;
		case MMPLAYER_STREAMING_ERROR_NOT_IMPLEMENTED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_NOT_IMPLEMENTED;
			break;
		case MMPLAYER_STREAMING_ERROR_BAD_GATEWAY:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_BAD_GATEWAY;
			break;
		case MMPLAYER_STREAMING_ERROR_SERVICE_UNAVAILABLE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_SERVICE_UNAVAILABLE;
			break;
		case MMPLAYER_STREAMING_ERROR_GATEWAY_TIME_OUT:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_GATEWAY_TIME_OUT;
			break;
		case MMPLAYER_STREAMING_ERROR_RTSP_VERSION_NOT_SUPPORTED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_RTSP_VERSION_NOT_SUPPORTED;
			break;
		case MMPLAYER_STREAMING_ERROR_OPTION_NOT_SUPPORTED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_OPTION_NOT_SUPPORTED;
			break;
		default:
			return MM_ERROR_PLAYER_STREAMING_FAIL;
	}

	error_string = g_strdup(gst_structure_get_string (s, "error_string"));
	if ( error_string )
		msg_param.data = (void *) error_string;

	if ( message->src )
	{
		msg_src_element = GST_ELEMENT_NAME( GST_ELEMENT_CAST( message->src ) );

		debug_error("-Msg src : [%s] Code : [%x] Error : [%s]  \n",
			msg_src_element, msg_param.code, (char*)msg_param.data );
	}

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

	debug_fleave();

	return TRUE;

}
void
__mmplayer_add_sink( mm_player_t* player, GstElement* sink )
{
	debug_fenter();

	return_if_fail ( player );
	return_if_fail ( sink );

	player->sink_elements =
		g_list_append(player->sink_elements, sink);

	debug_fleave();
}
void
__mmplayer_del_sink( mm_player_t* player, GstElement* sink )
{
	debug_fenter();

	return_if_fail ( player );
	return_if_fail ( sink );

	player->sink_elements =
			g_list_remove(player->sink_elements, sink);

	debug_fleave();
}

gboolean
__is_rtsp_streaming ( mm_player_t* player )
{
	return_val_if_fail ( player, FALSE );

	return ( player->profile.uri_type == MM_PLAYER_URI_TYPE_URL_RTSP ) ? TRUE : FALSE;
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

	return ( __is_rtsp_streaming ( player ) || __is_http_streaming ( player ) || __is_http_live_streaming ( player )) ? TRUE : FALSE;
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
__is_http_progressive_down(mm_player_t* player)
{
	return_val_if_fail( player, FALSE );

	return ((player->pd_mode) ? TRUE:FALSE);
}

