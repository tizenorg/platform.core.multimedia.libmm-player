/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, naveen cherukuri <naveen.ch@samsung.com>,
 * YeJin Cho <cho.yejin@samsung.com>, YoungHwan An <younghwan_.an@samsung.com>
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
#include <mm_debug.h>
#include <mm_error.h>
#include "mm_player_pd.h"
#include "mm_player_utils.h"
#include "mm_player_priv.h"

/*---------------------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:								                                                        |
---------------------------------------------------------------------------------------*/

/* It's callback to process whenever there is some changes in PD downloader. */
static gboolean __pd_download_callback(GstBus *bus, GstMessage *msg, gpointer data);

/* This function posts messages to application. */
/* Currently, MM_MESSAGE_PD_DOWNLOADER_START and MM_MESSAGE_PD_DOWNLOADER_END are used. */
static gboolean __mmplayer_pd_post_message(mm_player_t * player, enum MMMessageType msgtype, MMMessageParamType* param);

/*=======================================================================================
|  FUNCTION DEFINITIONS									                                                                                      |
=======================================================================================*/
static gboolean
__pd_download_callback(GstBus *bus, GstMessage *msg, gpointer data)
{
	mm_player_t * player = NULL;
	mm_player_pd_t *pd_downloader = NULL;
	gboolean bret = TRUE;
	
	debug_fenter();

	/* chech player handle */
	return_val_if_fail ( data, MM_ERROR_INVALID_ARGUMENT );

	player = MM_PLAYER_CAST((MMHandleType)data);

	/* get PD downloader handle */
	pd_downloader = MM_PLAYER_GET_PD((MMHandleType)data);

	return_val_if_fail ( pd_downloader, MM_ERROR_INVALID_ARGUMENT );

//	g_print("%s\n", GST_MESSAGE_TYPE_NAME(msg));

	switch ( GST_MESSAGE_TYPE( msg ) )
	{
		case GST_MESSAGE_EOS:
			{
				debug_log("PD EOS received....\n");

				g_object_set (G_OBJECT (pd_downloader->pushsrc), "eos", TRUE, NULL);

				/* notify application that download is completed */
				__mmplayer_pd_post_message(player, MM_MESSAGE_PD_DOWNLOADER_END, NULL);

				#ifdef PD_SELF_DOWNLOAD
				_mmplayer_pd_stop ((MMHandleType)data);
				#endif
			}
			break;

		case GST_MESSAGE_ERROR:
			{
				gboolean ret = FALSE;
				GError *error = NULL;
				gchar* debug = NULL;
				GstMessage *new_msg = NULL;
				
				/* get error code */
				gst_message_parse_error( msg, &error, &debug );
				debug_error ("GST_MESSAGE_ERROR = %s\n", debug);
				
				new_msg = gst_message_new_error (GST_OBJECT_CAST (pd_downloader->pushsrc), error, debug);

				/* notify application that pd has any error */
				ret = gst_element_post_message (pd_downloader->pushsrc, new_msg);

				_mmplayer_pd_stop ((MMHandleType)data);
			}
			break;

		case GST_MESSAGE_WARNING:
			{
				char* debug = NULL;
				GError* error = NULL;

				gst_message_parse_warning(msg, &error, &debug);
				debug_warning("warning : %s\n", error->message);
				debug_warning("debug : %s\n", debug);

				MMPLAYER_FREEIF(debug);
				g_error_free( error);
			}
			break;

		case GST_MESSAGE_STATE_CHANGED:
		{
			GstState old_state, new_state;
			gchar *src_name;

			/* get old and new state */
			gst_message_parse_state_changed (msg, &old_state, &new_state, NULL);

			if (old_state == new_state)
				break;

			/* we only care about pipeline state changes */
		      	if (GST_MESSAGE_SRC (msg) != GST_OBJECT (pd_downloader->download_pipe))
			  	break;

			src_name = gst_object_get_name (msg->src);
			debug_log ("%s changed state from %s to %s", src_name,
				gst_element_state_get_name (old_state),
				gst_element_state_get_name (new_state));
			g_free (src_name);

			switch(new_state)
			{
				case GST_STATE_VOID_PENDING:
				case GST_STATE_NULL:
				case GST_STATE_READY:
				case GST_STATE_PAUSED:
					break;

				case GST_STATE_PLAYING:
					/* notify application that download is stated */
					__mmplayer_pd_post_message(player, MM_MESSAGE_PD_DOWNLOADER_START, NULL);
					break;

				default:
					break;
			}
		}
		break;

		case GST_MESSAGE_DURATION:
		{
			GstFormat fmt= GST_FORMAT_BYTES;

			gint64 size = 0LL;

			/* get total size  of download file, (bytes) */
			if ( ! gst_element_query_duration( pd_downloader->download_pipe, &fmt, &size ) )
			{
				GError *err = NULL;
				GstMessage *new_msg = NULL;

				err = g_error_new (GST_STREAM_ERROR, GST_STREAM_ERROR_FAILED, "can't get total size");
				new_msg = gst_message_new_error (GST_OBJECT_CAST (pd_downloader->pushsrc), err, NULL);
				gst_element_post_message (pd_downloader->pushsrc, new_msg);

				g_error_free (err);

				// TODO: check if playback pipeline is closed well or not
				g_object_set (G_OBJECT (pd_downloader->pushsrc), "eos", TRUE, NULL);

				_mmplayer_pd_stop ((MMHandleType)data);

				debug_error("failed to query total size for download\n");
				break;
			}

			pd_downloader->total_size = size;

			debug_log("PD total size : %lld bytes\n", size);
		}
		break;

		default:
			debug_warning("unhandled message\n");
			break;
	}

	debug_fleave();

	return bret;
}

gboolean
__mmplayer_pd_post_message(mm_player_t * player, enum MMMessageType msgtype, MMMessageParamType* param)
{
	debug_fenter();

	return_val_if_fail( player, FALSE );

	if ( !player->pd_msg_cb )
	{
		debug_warning("no msg callback. can't post\n");
		return FALSE;
	}

	player->pd_msg_cb(msgtype, param, player->pd_msg_cb_param);

	debug_fleave();

	return TRUE;
}

gboolean _mmplayer_pd_get_status(MMHandleType handle, guint64 *current_pos, guint64 *total_size)
{
	debug_fenter();

	mm_player_pd_t * pd_downloader = NULL;
	guint64 bytes = 0;

	return_val_if_fail(handle, MM_ERROR_INVALID_ARGUMENT);

	pd_downloader = MM_PLAYER_GET_PD(handle);

	return_val_if_fail(pd_downloader, MM_ERROR_INVALID_ARGUMENT);
	return_val_if_fail(pd_downloader->download_pipe, MM_ERROR_INVALID_ARGUMENT);

	if ( !pd_downloader->total_size )
	{
		debug_warning("not ready to get total size\n");
		return FALSE;
	}

	g_object_get(pd_downloader->download_sink, "current-bytes", &bytes, NULL);

	debug_log("PD status : %lld / %lld\n", bytes, pd_downloader->total_size);

	*current_pos = bytes;
	*total_size = pd_downloader->total_size;

	debug_fleave();

	return TRUE;
}

mm_player_pd_t * _mmplayer_pd_create ()
{
	debug_fenter();

	mm_player_pd_t * pd_downloader = NULL;

	/* create PD handle */
	pd_downloader = (mm_player_pd_t *) malloc (sizeof (mm_player_pd_t));
	if ( !pd_downloader )
	{
		debug_error ("Failed to create pd_downloader handle...\n");
		return FALSE;
	}

	debug_fleave();

	return pd_downloader;
}

gboolean _mmplayer_pd_destroy (MMHandleType handle)
{
	debug_fenter();

	mm_player_pd_t * pd_downloader = NULL;

	return_val_if_fail ( handle, MM_ERROR_INVALID_ARGUMENT );

	pd_downloader = MM_PLAYER_GET_PD(handle);

	if (pd_downloader->download_pipe)
		_mmplayer_pd_stop (handle);

	/* release PD handle */
	MMPLAYER_FREEIF(pd_downloader);

	debug_fleave();

	return TRUE;
}

gboolean _mmplayer_pd_initialize (MMHandleType handle, gchar *src_uri, gchar *dst_uri, GstElement *pushsrc)
{
	debug_fenter();

	mm_player_pd_t * pd_downloader = NULL;

	return_val_if_fail ( handle, MM_ERROR_INVALID_ARGUMENT );
	return_val_if_fail ( src_uri, MM_ERROR_INVALID_ARGUMENT );
	return_val_if_fail ( dst_uri, MM_ERROR_INVALID_ARGUMENT );
	return_val_if_fail ( pushsrc, MM_ERROR_INVALID_ARGUMENT );

	pd_downloader = MM_PLAYER_GET_PD(handle);

	/* initialize */
	pd_downloader->uri_to_download = g_strdup (src_uri);
	pd_downloader->uri_to_save = g_strdup (dst_uri);
	pd_downloader->pushsrc = pushsrc;
	pd_downloader->total_size = 0LL;

	debug_fleave();
	
	return TRUE;
}

gboolean _mmplayer_pd_deinitialize (MMHandleType handle)
{
	debug_fenter();

	mm_player_pd_t * pd_downloader = NULL;

	return_val_if_fail ( handle, MM_ERROR_INVALID_ARGUMENT );

	pd_downloader = MM_PLAYER_GET_PD(handle);

	/* free */
	MMPLAYER_FREEIF(pd_downloader->uri_to_download);
	MMPLAYER_FREEIF(pd_downloader->uri_to_save);

	debug_fleave();	

	return TRUE;
}

gboolean _mmplayer_pd_start (MMHandleType handle)
{
	GstBus* bus = NULL;
	gboolean bret = FALSE;
	GstStateChangeReturn sret = GST_STATE_CHANGE_SUCCESS;
	GstState cur_state;
	GstState pending_state;

	debug_fenter();

	mm_player_pd_t * pd_downloader = NULL;

	return_val_if_fail ( handle, MM_ERROR_INVALID_ARGUMENT );

	pd_downloader = MM_PLAYER_GET_PD(handle);

	/* pipeline */
	pd_downloader->download_pipe = gst_pipeline_new ("PD Downloader");
	if (NULL == pd_downloader->download_pipe)
	{
		debug_error ("Can't create PD download pipeline...");
		return FALSE;
	}

	/* source */
	pd_downloader->download_src = gst_element_factory_make ("souphttpsrc", "PD HTTP download source");
	if (NULL == pd_downloader->download_src)
	{
		debug_error ("Can't create PD download src...");
		return FALSE;
	}

	/* queue */
	pd_downloader->download_queue = gst_element_factory_make ("queue", "PD download queue");
	if (NULL == pd_downloader->download_queue)
	{
		debug_error ("Can't create PD download queue...");
		return FALSE;
	}

	/* filesink */
	pd_downloader->download_sink = gst_element_factory_make ("filesink", "PD download sink");
	if (NULL == pd_downloader->download_sink)
	{
		debug_error ("Can't create PD download sink...");
		return FALSE;
	}

	g_object_set(pd_downloader->download_sink, "sync", FALSE, NULL);
	
	/* Add to bin and link */
	gst_bin_add_many (GST_BIN (pd_downloader->download_pipe), 
					pd_downloader->download_src, pd_downloader->download_queue, pd_downloader->download_sink,
					NULL);
	
	bret = gst_element_link_many (pd_downloader->download_src, pd_downloader->download_queue, pd_downloader->download_sink, NULL);
	if (FALSE == bret)
	{
		debug_error ("Can't link elements src and sink...");
		return FALSE;
	}
	
	/* Get Bus and set callback to watch */
	bus = gst_pipeline_get_bus (GST_PIPELINE (pd_downloader->download_pipe));
	gst_bus_add_watch (bus, __pd_download_callback, (gpointer)handle);
	gst_object_unref (bus);
	
	/* Set URI on HTTP source */
	g_object_set (G_OBJECT (pd_downloader->download_src), "location", pd_downloader->uri_to_download, NULL);

	/* set file download location on filesink*/
	g_object_set (G_OBJECT (pd_downloader->download_sink), "location", pd_downloader->uri_to_save, NULL);

	debug_log ("src location = %s, save location = %s\n", pd_downloader->uri_to_download, pd_downloader->uri_to_save);

	/* Start to download */
	sret = gst_element_set_state (pd_downloader->download_pipe, GST_STATE_PLAYING);
	if (GST_STATE_CHANGE_FAILURE == sret)
	{
		debug_error ("PD download pipeline failed to go to PLAYING state...");
		return FALSE;
	}

	debug_log ("set_state :: sret = %d\n", sret);

	sret = gst_element_get_state (pd_downloader->download_pipe, &cur_state, &pending_state, GST_CLOCK_TIME_NONE);
	if (GST_STATE_CHANGE_FAILURE == sret)
	{
		debug_error ("PD download pipeline failed to do get_state...");
		return FALSE;
	}

	debug_log ("get-state :: sret = %d\n", sret);

	debug_fleave();

	return TRUE;
}

gboolean _mmplayer_pd_stop (MMHandleType handle)
{
	debug_fenter();

	mm_player_pd_t * pd_downloader = NULL;

	return_val_if_fail ( handle, MM_ERROR_INVALID_ARGUMENT );

	pd_downloader = MM_PLAYER_GET_PD(handle);

	return_val_if_fail ( pd_downloader->download_pipe, MM_ERROR_INVALID_ARGUMENT );

	gst_element_set_state (pd_downloader->download_pipe, GST_STATE_NULL);
	gst_element_get_state (pd_downloader->download_pipe, NULL, NULL, GST_CLOCK_TIME_NONE);

	pd_downloader->download_pipe = NULL;

	debug_fleave();

	return TRUE;
}

gint _mm_player_set_pd_message_callback(MMHandleType handle, MMMessageCallback callback, gpointer user_param)
{
	debug_fenter();

	mm_player_t * player = NULL;

	return_val_if_fail ( handle, MM_ERROR_INVALID_ARGUMENT );

	player = MM_PLAYER_CAST((MMHandleType)handle);

	/* PD callback can be set as soon as player handle is created.
	  * So, player handle must have it.
	  */
	player->pd_msg_cb = callback;
	player->pd_msg_cb_param = user_param;

	debug_log("msg_cb : 0x%x     msg_cb_param : 0x%x\n", (guint)callback, (guint)user_param);

	debug_fleave();

	return MM_ERROR_NONE;
}
