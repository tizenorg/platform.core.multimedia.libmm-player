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
|    LOCAL FUNCTION PROTOTYPES:															|
---------------------------------------------------------------------------------------*/

/* It's callback to process whenever there is some changes in PD downloader. */
static gboolean __pd_downloader_callback(GstBus *bus, GstMessage *msg, gpointer data);

/* This function posts messages to application. */
/* Currently, MM_MESSAGE_PD_DOWNLOADER_START and MM_MESSAGE_PD_DOWNLOADER_END are used. */
static gboolean __pd_downloader_post_message(mm_player_t * player, enum MMMessageType msgtype, MMMessageParamType* param);

/*=======================================================================================
|  FUNCTION DEFINITIONS																	|
=======================================================================================*/
static gboolean
__pd_downloader_callback(GstBus *bus, GstMessage *msg, gpointer data)
{
	mm_player_t * player = NULL;
	mm_player_pd_t *pd = NULL;
	gboolean bret = TRUE;

	MMPLAYER_FENTER();

	/* chech player handle */
	return_val_if_fail ( data, MM_ERROR_INVALID_ARGUMENT );

	player = MM_PLAYER_CAST((MMHandleType)data);

	/* get PD downloader handle */
	pd = MM_PLAYER_GET_PD((MMHandleType)data);

	return_val_if_fail ( pd, MM_ERROR_INVALID_ARGUMENT );

//	g_print("%s\n", GST_MESSAGE_TYPE_NAME(msg));

	switch ( GST_MESSAGE_TYPE( msg ) )
	{
		case GST_MESSAGE_EOS:
			{
				debug_log("PD Downloader EOS received....\n");

				g_object_set (G_OBJECT (pd->playback_pipeline_src), "eos", TRUE, NULL);

				/* notify application that download is completed */
				__pd_downloader_post_message(player, MM_MESSAGE_PD_DOWNLOADER_END, NULL);

				#ifdef PD_SELF_DOWNLOAD
				_mmplayer_unrealize_pd_downloader ((MMHandleType)data);
				#endif
			}
			break;

		case GST_MESSAGE_ERROR:
			{
				GError *error = NULL;
				gchar* debug = NULL;
				GstMessage *new_msg = NULL;

				/* get error code */
				gst_message_parse_error( msg, &error, &debug );
				debug_error ("GST_MESSAGE_ERROR = %s\n", debug);

				new_msg = gst_message_new_error (GST_OBJECT_CAST (pd->playback_pipeline_src), error, debug);

				/* notify application that pd has any error */
				gst_element_post_message (pd->playback_pipeline_src, new_msg);

				_mmplayer_unrealize_pd_downloader ((MMHandleType)data);
				MMPLAYER_FREEIF(debug);
				g_error_free( error);
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
			if (GST_MESSAGE_SRC (msg) != GST_OBJECT (pd->downloader_pipeline))
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
					__pd_downloader_post_message(player, MM_MESSAGE_PD_DOWNLOADER_START, NULL);
					break;

				default:
					break;
			}
		}
		break;

		case GST_MESSAGE_DURATION:
		{
			gint64 size = 0LL;

			/* get total size  of download file, (bytes) */
			if ( ! gst_element_query_duration( pd->downloader_pipeline, GST_FORMAT_BYTES, &size ) )
			{
				GError *err = NULL;
				GstMessage *new_msg = NULL;

				err = g_error_new (GST_STREAM_ERROR, GST_STREAM_ERROR_FAILED, "can't get total size");
				new_msg = gst_message_new_error (GST_OBJECT_CAST (pd->playback_pipeline_src), err, NULL);
				gst_element_post_message (pd->playback_pipeline_src, new_msg);

				g_error_free (err);

				// TODO: check if playback pipeline is closed well or not
				g_object_set (G_OBJECT (pd->playback_pipeline_src), "eos", TRUE, NULL);

				_mmplayer_unrealize_pd_downloader ((MMHandleType)data);

				debug_error("failed to query total size for download\n");
				break;
			}

			pd->total_size = size;

			debug_log("PD total size : %lld bytes\n", size);
		}
		break;

		default:
			debug_warning("unhandled message\n");
			break;
	}

	MMPLAYER_FLEAVE();

	return bret;
}


gboolean __pd_downloader_post_message(mm_player_t * player, enum MMMessageType msgtype, MMMessageParamType* param)
{
	MMPLAYER_FENTER();

	return_val_if_fail( player, FALSE );

	if ( !player->pd_msg_cb )
	{
		debug_warning("no msg callback. can't post\n");
		return FALSE;
	}

	player->pd_msg_cb(msgtype, param, player->pd_msg_cb_param);

	MMPLAYER_FLEAVE();

	return TRUE;
}


int _mmplayer_get_pd_downloader_status(MMHandleType handle, guint64 *current_pos, guint64 *total_size)
{
	MMPLAYER_FENTER();

	mm_player_pd_t * pd = NULL;
	guint64 bytes = 0;

	return_val_if_fail(handle, MM_ERROR_INVALID_ARGUMENT);

	pd = MM_PLAYER_GET_PD(handle);

	return_val_if_fail(pd, MM_ERROR_INVALID_ARGUMENT);
	return_val_if_fail(pd->downloader_pipeline, MM_ERROR_PLAYER_INVALID_STATE);

	if ( !pd->total_size )
	{
		debug_warning("not ready to get total size\n");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	g_object_get(pd->downloader_sink, "current-bytes", &bytes, NULL);

	debug_log("PD status : %lld / %lld\n", bytes, pd->total_size);

	*current_pos = bytes;
	*total_size = pd->total_size;

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}


mm_player_pd_t * _mmplayer_create_pd_downloader()
{
	MMPLAYER_FENTER();

	mm_player_pd_t * pd = NULL;

	/* create PD handle */
	pd = (mm_player_pd_t *) malloc (sizeof (mm_player_pd_t));
	if ( !pd )
	{
		debug_error ("Failed to create pd downloader handle...\n");
		return FALSE;
	}
	memset( pd, 0, sizeof (mm_player_pd_t));

	MMPLAYER_FLEAVE();

	return pd;
}


gboolean _mmplayer_destroy_pd_downloader (MMHandleType handle)
{
	MMPLAYER_FENTER();

	mm_player_pd_t * pd = NULL;

	return_val_if_fail ( handle, MM_ERROR_INVALID_ARGUMENT );

	pd = MM_PLAYER_GET_PD(handle);

	if (pd && pd->downloader_pipeline)
		_mmplayer_unrealize_pd_downloader (handle);

	/* release PD handle */
	MMPLAYER_FREEIF(pd);

	MMPLAYER_FLEAVE();

	return TRUE;
}


gboolean _mmplayer_realize_pd_downloader (MMHandleType handle, gchar *src_uri, gchar *dst_uri, GstElement *pushsrc)
{
	MMPLAYER_FENTER();

	mm_player_pd_t * pd = NULL;

	return_val_if_fail ( handle, MM_ERROR_INVALID_ARGUMENT );
	return_val_if_fail ( src_uri, MM_ERROR_INVALID_ARGUMENT );
	return_val_if_fail ( dst_uri, MM_ERROR_INVALID_ARGUMENT );
	return_val_if_fail ( pushsrc, MM_ERROR_INVALID_ARGUMENT );

	pd = MM_PLAYER_GET_PD(handle);

	/* initialize */
	pd->path_read_from = g_strdup (src_uri);
	pd->location_to_save = g_strdup (dst_uri);
	pd->playback_pipeline_src = pushsrc;
	pd->total_size = 0LL;

	MMPLAYER_FLEAVE();

	return TRUE;
}


gboolean _mmplayer_start_pd_downloader (MMHandleType handle)
{
	GstBus* bus = NULL;
	gboolean bret = FALSE;
	GstStateChangeReturn sret = GST_STATE_CHANGE_SUCCESS;
	GstState cur_state;
	GstState pending_state;

	MMPLAYER_FENTER();

	mm_player_pd_t * pd = NULL;

	return_val_if_fail ( handle, MM_ERROR_INVALID_ARGUMENT );

	pd = MM_PLAYER_GET_PD(handle);

	/* pipeline */
	pd->downloader_pipeline = gst_pipeline_new ("PD Downloader");
	if (NULL == pd->downloader_pipeline)
	{
		debug_error ("Can't create PD download pipeline...");
		return FALSE;
	}

	/* source */
	pd->downloader_src = gst_element_factory_make ("souphttpsrc", "PD HTTP download source");
	if (NULL == pd->downloader_src)
	{
		debug_error ("Can't create PD download src...");
		return FALSE;
	}

	/* queue */
	pd->downloader_queue = gst_element_factory_make ("queue", "PD download queue");
	if (NULL == pd->downloader_queue)
	{
		debug_error ("Can't create PD download queue...");
		return FALSE;
	}

	/* filesink */
	pd->downloader_sink = gst_element_factory_make ("filesink", "PD download sink");
	if (NULL == pd->downloader_sink)
	{
		debug_error ("Can't create PD download sink...");
		return FALSE;
	}

	g_object_set(pd->downloader_sink, "sync", FALSE, NULL);

	/* Add to bin and link */
	gst_bin_add_many (GST_BIN (pd->downloader_pipeline),
					pd->downloader_src, pd->downloader_queue, pd->downloader_sink,
					NULL);

	bret = gst_element_link_many (pd->downloader_src, pd->downloader_queue, pd->downloader_sink, NULL);
	if (FALSE == bret)
	{
		debug_error ("Can't link elements src and sink...");
		return FALSE;
	}

	/* Get Bus and set callback to watch */
	bus = gst_pipeline_get_bus (GST_PIPELINE (pd->downloader_pipeline));
	gst_bus_add_watch (bus, __pd_downloader_callback, (gpointer)handle);
	gst_object_unref (bus);

	/* Set URI on HTTP source */
	g_object_set (G_OBJECT (pd->downloader_src), "location", pd->path_read_from, NULL);

	/* set file download location on filesink*/
	g_object_set (G_OBJECT (pd->downloader_sink), "location", pd->location_to_save, NULL);

	secure_debug_log ("src location = %s, save location = %s\n", pd->path_read_from, pd->location_to_save);

	/* Start to download */
	sret = gst_element_set_state (pd->downloader_pipeline, GST_STATE_PLAYING);
	if (GST_STATE_CHANGE_FAILURE == sret)
	{
		debug_error ("PD download pipeline failed to go to PLAYING state...");
		return FALSE;
	}

	debug_log ("set_state :: sret = %d\n", sret);

	sret = gst_element_get_state (pd->downloader_pipeline, &cur_state, &pending_state, GST_CLOCK_TIME_NONE);
	if (GST_STATE_CHANGE_FAILURE == sret)
	{
		debug_error ("PD download pipeline failed to do get_state...");
		return FALSE;
	}

	debug_log ("get-state :: sret = %d\n", sret);

	MMPLAYER_FLEAVE();

	return TRUE;
}


gboolean _mmplayer_unrealize_pd_downloader (MMHandleType handle)
{
	MMPLAYER_FENTER();

	mm_player_pd_t * pd = NULL;

	return_val_if_fail ( handle, FALSE );

	pd = MM_PLAYER_GET_PD(handle);

	return_val_if_fail ( pd && pd->downloader_pipeline, FALSE );

	gst_element_set_state (pd->downloader_pipeline, GST_STATE_NULL);
	gst_element_get_state (pd->downloader_pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);

	gst_object_unref (G_OBJECT (pd->downloader_pipeline));
	pd->downloader_pipeline = NULL;

	/* free */
	MMPLAYER_FREEIF(pd->path_read_from);
	MMPLAYER_FREEIF(pd->location_to_save);

	MMPLAYER_FLEAVE();

	return TRUE;
}


gint _mm_player_set_pd_downloader_message_cb(MMHandleType handle, MMMessageCallback callback, gpointer user_param)
{
	MMPLAYER_FENTER();

	mm_player_t * player = NULL;

	return_val_if_fail ( handle, MM_ERROR_INVALID_ARGUMENT );

	player = MM_PLAYER_CAST((MMHandleType)handle);

	/* PD callback can be set as soon as player handle is created.
	  * So, player handle must have it.
	  */
	player->pd_msg_cb = callback;
	player->pd_msg_cb_param = user_param;

	debug_log("msg_cb : %p     msg_cb_param : %p\n", callback, user_param);

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}
