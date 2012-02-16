/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, YeJin Cho <cho.yejin@samsung.com>,
 * Seungbae Shin <seungbae.shin@samsung.com>, YoungHwan An <younghwan_.an@samsung.com>,
 * naveen cherukuri <naveen.ch@samsung.com>
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

#include "mm_player_ahs.h"
#include "mm_player_priv.h"
#include "mm_player_utils.h"

char *state_string[] = { "STATE_STOP", "STATE_PREPARE_MANIFEST", "STATE_MEDIA_STREAMING" };

enum
{
	AHS_STATE_STOP = 0,
	AHS_STATE_PREPARE_MANIFEST,
	AHS_STATE_MEDIA_STREAMING,
} AHS_STATE;

enum
{
	STATE_STOP = 0,
	STATE_PREPARE_PLAYLIST,
	STATE_MEDIA_STREAMING
} HLS_STATE;

static gpointer manifest_update_thread(gpointer data);
static gpointer media_download_thread (gpointer data);
static gboolean ahs_client_is_live (mm_player_ahs_t *ahs_player);
static gboolean ahs_manifest_get_update_interval (mm_player_ahs_t *ahs_player, GTimeVal *next_update);
static gboolean ahs_create_manifest_download_pipeline (mm_player_ahs_t *ahs_player);
static gboolean ahs_create_key_download_pipeline (mm_player_ahs_t *ahs_player);
static gboolean ahs_create_media_download_pipeline (mm_player_ahs_t *ahs_player);
static gboolean ahs_parse_manifest_update_client (mm_player_ahs_t *ahs_player);
static gboolean ahs_manifest_download_callback(GstBus *bus, GstMessage *msg, gpointer data);
static gboolean ahs_key_download_callback(GstBus *bus, GstMessage *msg, gpointer data);
static gboolean ahs_media_download_callback(GstBus *bus, GstMessage *msg, gpointer data);
static gboolean ahs_determining_next_file_load (mm_player_ahs_t *ahs_player, gboolean *is_ready);
static gboolean ahs_set_current_manifest (mm_player_ahs_t *ahs_player);
static gchar* ahs_get_current_manifest (mm_player_ahs_t *ahs_player);
static gboolean ahs_switch_playlist (mm_player_ahs_t *ahs_player, guint download_rate);
static gboolean ahs_get_next_media_uri (mm_player_ahs_t *ahs_player, gchar **media_uri, gchar **key_uri, char **iv);
static gboolean ahs_decrypt_media (mm_player_ahs_t *ahs_player,GstBuffer *InBuf, GstBuffer **OutBuf);
static gboolean ahs_destory_manifest_download_pipeline(mm_player_ahs_t *ahs_player);
static gboolean ahs_destory_media_download_pipeline(mm_player_ahs_t *ahs_player);
static gboolean ahs_destory_key_download_pipeline(mm_player_ahs_t *ahs_player);
static gboolean ahs_is_buffer_discontinuous (mm_player_ahs_t *ahs_player);
static gboolean ahs_clear_discontinuous (mm_player_ahs_t *ahs_player);

static void
on_new_buffer_from_appsink (GstElement * appsink, void* data)
{
	mm_player_ahs_t *ahs_player = (mm_player_ahs_t*) data;
	GstBuffer *InBuf = NULL;
	GstFlowReturn fret = GST_FLOW_OK;

	InBuf = gst_app_sink_pull_buffer ((GstAppSink *)appsink);
	
	if (InBuf && ahs_player->appsrc) 
	{
		ahs_player->seg_size = ahs_player->seg_size + GST_BUFFER_SIZE (InBuf);
		
		if (ahs_player->cur_key_uri)
		{
			GstBuffer *OutBuf = NULL;
			
			ahs_decrypt_media (ahs_player, InBuf, &OutBuf);

			gst_buffer_unref (InBuf);

			/* FIXME : Reset Buffer property */
			GST_BUFFER_TIMESTAMP (OutBuf) = GST_CLOCK_TIME_NONE;
			GST_BUFFER_DURATION (OutBuf) = GST_CLOCK_TIME_NONE;
			GST_BUFFER_FLAGS(OutBuf) = 0;
			
			if (ahs_is_buffer_discontinuous(ahs_player)) 
			{
    				GST_BUFFER_FLAG_SET (OutBuf, GST_BUFFER_FLAG_DISCONT);
                            	ahs_clear_discontinuous (ahs_player);
  			}

			fret = gst_app_src_push_buffer ((GstAppSrc *)ahs_player->appsrc, OutBuf);
			if (fret != GST_FLOW_OK)
			{
				__mm_player_ahs_stop (ahs_player);
			}

		}
		else
		{
			/* FIXME : Reset Buffer property */
			GST_BUFFER_TIMESTAMP (InBuf) = GST_CLOCK_TIME_NONE;
			GST_BUFFER_DURATION (InBuf) = GST_CLOCK_TIME_NONE;
			GST_BUFFER_FLAGS(InBuf) = 0;
			
			if (ahs_is_buffer_discontinuous(ahs_player)) 
			{
    				GST_BUFFER_FLAG_SET (InBuf, GST_BUFFER_FLAG_DISCONT);
				ahs_clear_discontinuous (ahs_player);
  			}

			fret = gst_app_src_push_buffer ((GstAppSrc *)ahs_player->appsrc, InBuf);
			if (fret != GST_FLOW_OK)
			{
				__mm_player_ahs_stop (ahs_player);
			}
		}
	}
	else 
	{
		debug_warning ("Pulled buffer is not valid!!!\n");
	}
}

static gpointer 
manifest_update_thread(gpointer data)
{
	mm_player_ahs_t *ahs_player = (mm_player_ahs_t*) data;
	gboolean bret = FALSE;
	GTimeVal next_update = {0, };
	GTimeVal tmp_update = {0, };
	guint64 start = 0;
	guint64 stop = 0;
	
	debug_log ("Waiting for trigger to start downloading manifest...\n");
	g_mutex_lock (ahs_player->manifest_mutex);
	g_cond_wait (ahs_player->manifest_update_cond, ahs_player->manifest_mutex);
	g_mutex_unlock (ahs_player->manifest_mutex);

	while (1)
	{
		g_mutex_lock (ahs_player->manifest_mutex);
		if (ahs_player->manifest_thread_exit == TRUE)
		{
			g_mutex_unlock (ahs_player->manifest_mutex);
			goto exit;
		}
		g_mutex_unlock (ahs_player->manifest_mutex);

		next_update.tv_sec = 0;
		next_update.tv_usec = 0;
		
		g_get_current_time (&next_update);

		start =  (next_update.tv_sec * 1000000)+ next_update.tv_usec;
		
		/* download manifest file */
		bret = ahs_create_manifest_download_pipeline (ahs_player);
		if (FALSE == bret)
		{
			goto exit;
		}
		
		/* waiting for playlist to be downloaded */		
		g_mutex_lock (ahs_player->manifest_mutex);
		if (ahs_player->manifest_thread_exit == TRUE)
		{
			g_mutex_unlock (ahs_player->manifest_mutex);
			goto exit;
		}
		debug_log ("waiting for manifest file to be downloaded...waiting on manifest eos cond\n");
		g_cond_wait (ahs_player->manifest_eos_cond, ahs_player->manifest_mutex);
		g_mutex_unlock (ahs_player->manifest_mutex);
		
		if (ahs_client_is_live (ahs_player))
		{
			ahs_manifest_get_update_interval (ahs_player, &next_update);

			stop = (next_update.tv_sec * 1000000)+ next_update.tv_usec;
		
			g_mutex_lock (ahs_player->manifest_mutex);
			if (ahs_player->manifest_thread_exit == TRUE)
			{
				g_mutex_unlock (ahs_player->manifest_mutex);
				goto exit;
			}
			debug_log ("Next update scheduled at %s\n", g_time_val_to_iso8601 (&next_update));
			bret = g_cond_timed_wait (ahs_player->manifest_update_cond, ahs_player->manifest_mutex, &next_update);

			g_mutex_unlock (ahs_player->manifest_mutex);
			tmp_update.tv_sec = 0;
			tmp_update.tv_usec = 0;
			
			g_get_current_time (&tmp_update);

			if (bret == TRUE)
			{
				debug_log ("\n\n@@@@@@@@@ Sombody signalled manifest waiting... going to update current manifest file and diff = %d\n\n\n", 
				((next_update.tv_sec * 1000000)+ next_update.tv_usec) - ((tmp_update.tv_sec * 1000000)+ tmp_update.tv_usec));
			}
			else
			{
				debug_log ("\n\n\n~~~~~~~~~~~Timeout happened, need to update current manifest file and diff = %d\n\n\n",
				((next_update.tv_sec * 1000000)+ next_update.tv_usec) - ((tmp_update.tv_sec * 1000000)+ tmp_update.tv_usec));
			}
		}
		else
		{
			g_mutex_lock (ahs_player->manifest_mutex);
			if (ahs_player->manifest_thread_exit == TRUE)
			{
				g_mutex_unlock (ahs_player->manifest_mutex);
				goto exit;
			}
			g_cond_wait (ahs_player->manifest_update_cond, ahs_player->manifest_mutex);
			g_mutex_unlock (ahs_player->manifest_mutex);

		}
	}

exit:
	debug_log ("Exiting from manifest thread...\n");
	ahs_player->manifest_thread_exit = TRUE;
	g_thread_exit (ahs_player->manifest_thread);
	
	return NULL;

}

static gpointer
media_download_thread (gpointer data)
{
	mm_player_ahs_t *ahs_player = (mm_player_ahs_t*) data;
	gboolean bret = FALSE;
	gchar *media_uri = NULL;
	gchar *key_uri = NULL;
	GTimeVal time = {0, };
	GstFlowReturn fret = GST_FLOW_OK;
	char *iv = (char *) malloc (16);
	if (iv == NULL)
	{
		g_print ("ERRORR");
		return NULL;
	}

	g_mutex_lock (ahs_player->media_mutex);
	g_cond_wait(ahs_player->media_start_cond, ahs_player->media_mutex);
	g_mutex_unlock (ahs_player->media_mutex);

	debug_log ("Received received manifest file...Moving to media download\n");
	
	while (1)
	{
		g_mutex_lock (ahs_player->media_mutex);
		if (ahs_player->media_thread_exit)
		{
			g_mutex_unlock (ahs_player->media_mutex);
			goto exit;
		}
		g_mutex_unlock (ahs_player->media_mutex);

		if (ahs_player->need_bw_switch)
		{
			debug_log ("Need to Switch BW, start updating new switched URI...\n");
			g_cond_signal (ahs_player->manifest_update_cond);
			g_mutex_lock (ahs_player->manifest_mutex);
			if (ahs_player->media_thread_exit)
			{
				g_mutex_unlock (ahs_player->manifest_mutex);
				goto exit;
			}
			debug_log ("waiting for manifest eos in media download thread...\n");
			g_cond_wait (ahs_player->manifest_eos_cond, ahs_player->manifest_mutex);	
			g_mutex_unlock (ahs_player->manifest_mutex);
	
		}
	
		media_uri = NULL;
		key_uri = NULL;
		
		/* Get next media file to download */
		bret = ahs_get_next_media_uri (ahs_player, &media_uri, &key_uri, &iv);
		if (FALSE == bret)
		{
			ahs_player->media_thread_exit = TRUE;
			fret = gst_app_src_end_of_stream ((GstAppSrc *)ahs_player->appsrc);
			if (GST_FLOW_OK != fret)
			{
				debug_error("Error in pushing EOS to appsrc : reason - %s", gst_flow_get_name (fret));
			}
			goto exit;
		}

		if (NULL == media_uri)
		{
			if (ahs_client_is_live (ahs_player))
			{
				g_cond_signal (ahs_player->manifest_update_cond);
				g_mutex_lock (ahs_player->manifest_mutex);
				if (ahs_player->media_thread_exit)
				{
					g_mutex_unlock (ahs_player->manifest_mutex);
					goto exit;
				}
				debug_log ("waiting for manifest eos in media download thread...\n");	
				g_cond_wait (ahs_player->manifest_eos_cond, ahs_player->manifest_mutex);
				g_mutex_unlock (ahs_player->manifest_mutex);
				continue;
			}
			else
			{
				ahs_player->media_thread_exit = TRUE;
				fret = gst_app_src_end_of_stream ((GstAppSrc *)ahs_player->appsrc);
				if (GST_FLOW_OK != fret)
				{
					debug_error("Error in pushing EOS to appsrc : reason - %s", gst_flow_get_name (fret));
				}
				goto exit;
			}
		}
		
		if (key_uri)
		{	
			if (ahs_player->cur_key_uri)
			{
				g_free (ahs_player->cur_key_uri);
				ahs_player->cur_key_uri = NULL;
			}
			
			ahs_player->cur_key_uri = g_strdup (key_uri);
			g_free (key_uri);
			key_uri = NULL;
			
			memcpy (ahs_player->cur_iv, iv, 16);

			g_mutex_lock (ahs_player->media_mutex);
			ahs_create_key_download_pipeline (ahs_player);
			g_cond_wait (ahs_player->key_eos_cond, ahs_player->media_mutex);
			g_mutex_unlock (ahs_player->media_mutex);
		
			debug_log("Downloaded key url.. and key data is = %s\n", ahs_player->cur_key_data);
		}

		ahs_player->cur_media_uri = g_strdup (media_uri);
		g_free (media_uri);
		media_uri = NULL;

		/* note down segment start time */
		g_get_current_time (&time);
		ahs_player->seg_start_time = (time.tv_sec * 1000000)+ time.tv_usec;
		debug_log ("start time in usec = %"G_GUINT64_FORMAT"\n", ahs_player->seg_start_time);

		bret = ahs_create_media_download_pipeline (ahs_player);
		if (FALSE == bret)
		{
			goto exit;
		}	
	
		/* waiting for media file to be downloaded */
		g_mutex_lock (ahs_player->media_mutex);
		if (ahs_player->media_thread_exit)
		{
			g_mutex_unlock (ahs_player->media_mutex);
			goto exit;
		}
		debug_log ("waiting on media EOS....\n");
		g_cond_wait (ahs_player->media_eos_cond, ahs_player->media_mutex);
		g_mutex_unlock (ahs_player->media_mutex);
	
		debug_log ("Done with waiting on media EOS....\n");
		
	}


exit:
	debug_log ("Exiting from media thread...\n");
	ahs_player->media_thread_exit = TRUE;
	g_thread_exit (ahs_player->media_thread);
	return NULL;
}

static gboolean
ahs_create_manifest_download_pipeline (mm_player_ahs_t *ahs_player)
{
	gboolean bret = FALSE;
	GstStateChangeReturn	sret = GST_STATE_CHANGE_SUCCESS;
	gchar* manifest_dump_file_name = NULL;
	gint fd = -1;

	debug_log ("<<<\n");

	/* If pipeline exists, then cleanup first */
	if (ahs_player->manifest_download_pipeline)
		ahs_destory_manifest_download_pipeline(ahs_player);

	/* Create element */
	ahs_player->manifest_download_pipeline = gst_pipeline_new ("AHS manifest Pipeline");
	if (NULL == ahs_player->manifest_download_pipeline)
	{
		debug_error ("Can't create manifest download pipeline...");
		return FALSE;
	}
	ahs_player->manifest_download_src = gst_element_factory_make ("souphttpsrc", "AHS manifest download source");
	if (NULL == ahs_player->manifest_download_src)
	{
		debug_error ("Can't create manifest download src...");
		return FALSE;
	}
	ahs_player->manifest_download_sink = gst_element_factory_make ("filesink", "AHS manifest download sink");
	if (NULL == ahs_player->manifest_download_sink)
	{
		debug_error ("Can't create manifest download sink...");
		return FALSE;
	}
	
	/* Add to bin and link */
	gst_bin_add_many (GST_BIN (ahs_player->manifest_download_pipeline), 
					ahs_player->manifest_download_src, ahs_player->manifest_download_sink,
					NULL);
	
	bret = gst_element_link (ahs_player->manifest_download_src, ahs_player->manifest_download_sink);
	if (FALSE == bret)
	{
		debug_error ("Can't link elements src and sink...");
		return FALSE;
	}
	
	/* Set Bus */
	GstBus* bus = gst_pipeline_get_bus (GST_PIPELINE (ahs_player->manifest_download_pipeline));
	gst_bus_add_watch (bus, ahs_manifest_download_callback, ahs_player);
	gst_object_unref (bus);

	/* Set URI */
	g_object_set (G_OBJECT (ahs_player->manifest_download_src), "location", ahs_player->cur_mf_uri, NULL);

	if (ahs_player->ahs_manifest_dmp_location)
		g_unlink(ahs_player->ahs_manifest_dmp_location);

	/* set path to dump manifest file */
	manifest_dump_file_name = g_strdup(HLS_MANIFEST_DEFAULT_FILE_NAME);

	fd = g_mkstemp(manifest_dump_file_name);
	if (fd == -1)
	{
		debug_error("failed to open temporary file\n");
		MMPLAYER_FREEIF(manifest_dump_file_name);
		return FALSE;
	}
	
	sprintf (ahs_player->ahs_manifest_dmp_location, "%s.m3u8", manifest_dump_file_name);

	if (g_file_test (manifest_dump_file_name, G_FILE_TEST_EXISTS))
	{
		close(fd);
		g_unlink(manifest_dump_file_name);
	}

	MMPLAYER_FREEIF(manifest_dump_file_name);

	/* ENAMETOOLONG or not  */
	if (strlen(ahs_player->ahs_manifest_dmp_location) > HLS_POSIX_PATH_MAX)
	{
		debug_error("file name too long\n");
		return FALSE;
	}
	
	g_object_set (G_OBJECT (ahs_player->manifest_download_sink), "location", ahs_player->ahs_manifest_dmp_location, NULL);

	debug_log ("src location = %s, save location = %s\n", ahs_player->cur_mf_uri, ahs_player->ahs_manifest_dmp_location);

	/* Start to download */
	sret = gst_element_set_state (ahs_player->manifest_download_pipeline, GST_STATE_PLAYING);

	debug_log ("sret = %d\n", sret);

	debug_log (">>>\n");

	return TRUE;

}

static gboolean
ahs_destory_manifest_download_pipeline(mm_player_ahs_t *ahs_player)
{
	debug_log ("<<<\n");

	gst_element_set_state (ahs_player->manifest_download_pipeline, GST_STATE_NULL);
	gst_element_get_state (ahs_player->manifest_download_pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);

	gst_object_unref(ahs_player->manifest_download_pipeline);
	ahs_player->manifest_download_pipeline = NULL;

	debug_log (">>>\n");

	return TRUE;
}

static gboolean
ahs_create_media_download_pipeline (mm_player_ahs_t *ahs_player)
{
	gboolean bret = FALSE;
	GstBus* bus = NULL;
	
	debug_log ("<<<\n");

	/* If pipeline exists, then cleanup first */
	if (ahs_player->media_download_pipeline) 
		ahs_destory_media_download_pipeline(ahs_player);

	/* Create element */
	ahs_player->media_download_pipeline = gst_pipeline_new ("AHS media Pipeline");
	if (NULL == ahs_player->media_download_pipeline)
	{
		debug_error ("Can't create media download pipeline...");
		return FALSE;
	}
	ahs_player->media_download_src = gst_element_factory_make ("souphttpsrc", "AHS media download source");
	if (NULL == ahs_player->media_download_src)
	{
		debug_error ("Can't create media download src...");
		return FALSE;
	}
	ahs_player->media_download_sink = gst_element_factory_make ("appsink", "AHS media download sink");
	if (NULL == ahs_player->media_download_sink)
	{
		debug_error ("Can't create media download sink...");
		return FALSE;
	}
	
	/* Add to bin and link */
	gst_bin_add_many (GST_BIN (ahs_player->media_download_pipeline), 
					ahs_player->media_download_src, ahs_player->media_download_sink,
					NULL);
	
	bret = gst_element_link (ahs_player->media_download_src, ahs_player->media_download_sink);
	if (FALSE == bret)
	{
		debug_error ("Can't link elements src and sink...");
		return FALSE;
	}
	
	/* Set Bus */
	bus = gst_pipeline_get_bus (GST_PIPELINE (ahs_player->media_download_pipeline));
	gst_bus_add_watch (bus, ahs_media_download_callback, ahs_player);
	gst_object_unref (bus);

	/* Set URI on src element */
	g_object_set (G_OBJECT (ahs_player->media_download_src), "location", ahs_player->cur_media_uri, NULL);

	g_print ("Going to download media-uri -> %s\n", ahs_player->cur_media_uri);

	/* setting properties on sink element */
	g_object_set (G_OBJECT (ahs_player->media_download_sink), "emit-signals", TRUE, "sync", FALSE, NULL);
	g_signal_connect (ahs_player->media_download_sink, "new-buffer",  G_CALLBACK (on_new_buffer_from_appsink), ahs_player);

	/* Start to download */
	gst_element_set_state (ahs_player->media_download_pipeline, GST_STATE_PLAYING);

	debug_log (">>>\n");

	return TRUE;

}

static gboolean 
ahs_destory_media_download_pipeline(mm_player_ahs_t *ahs_player)
{
	debug_log ("<<<\n");

	gst_element_set_state (ahs_player->media_download_pipeline, GST_STATE_NULL);
	gst_element_get_state (ahs_player->media_download_pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);

	gst_object_unref(ahs_player->media_download_pipeline);
	ahs_player->media_download_pipeline = NULL;

	debug_log (">>>\n");

	return TRUE;
}

static gboolean
ahs_create_key_download_pipeline (mm_player_ahs_t *ahs_player)
{
	gboolean bret = FALSE;
	GstBus* bus = NULL;
	
	debug_log ("<<<\n");

	/* If pipeline exists, then cleanup first */
	if (ahs_player->key_download_pipeline) 
		ahs_destory_key_download_pipeline(ahs_player);

	/* Create element */
	ahs_player->key_download_pipeline = gst_pipeline_new ("AHS key Pipeline");
	if (NULL == ahs_player->key_download_pipeline)
	{
		debug_error ("Can't create key download pipeline...");
		return FALSE;
	}
	ahs_player->key_download_src = gst_element_factory_make ("souphttpsrc", "AHS key download source");
	if (NULL == ahs_player->key_download_src)
	{
		debug_error ("Can't create key download src...");
		return FALSE;
	}
	ahs_player->key_download_sink = gst_element_factory_make ("filesink", "AHS key download sink");
	if (NULL == ahs_player->key_download_sink)
	{
		debug_error ("Can't create key download sink...");
		return FALSE;
	}
	
	/* Add to bin and link */
	gst_bin_add_many (GST_BIN (ahs_player->key_download_pipeline), 
					ahs_player->key_download_src, ahs_player->key_download_sink,
					NULL);
	
	bret = gst_element_link (ahs_player->key_download_src, ahs_player->key_download_sink);
	if (FALSE == bret)
	{
		debug_error ("Can't link elements src and sink...");
		return FALSE;
	}
	
	/* Set Bus */
	bus = gst_pipeline_get_bus (GST_PIPELINE (ahs_player->key_download_pipeline));
	gst_bus_add_watch (bus, ahs_key_download_callback, ahs_player);
	gst_object_unref (bus);
	
	/* Set URI */
	g_object_set (G_OBJECT (ahs_player->key_download_src), "location", ahs_player->cur_key_uri, NULL);
	sprintf (ahs_player->ahs_key_dmp_location, "/opt/apps/com.samsung.video-player/data%s", strrchr(ahs_player->cur_key_uri, '/'));
	debug_log ("src location = %s, save location = %s\n", ahs_player->cur_key_uri, ahs_player->ahs_key_dmp_location);
	
	g_print ("Going to download key-uri -> %s\n", ahs_player->cur_key_uri);

	g_object_set (G_OBJECT (ahs_player->key_download_sink), "location", ahs_player->ahs_key_dmp_location, NULL);

	/* Start to download */
	gst_element_set_state (ahs_player->key_download_pipeline, GST_STATE_PLAYING);

	debug_log (">>>\n");

	return TRUE;

}

static gboolean
ahs_destory_key_download_pipeline(mm_player_ahs_t *ahs_player)
{
	debug_log ("<<<\n");

	gst_element_set_state (ahs_player->key_download_pipeline, GST_STATE_NULL);
	gst_element_get_state (ahs_player->key_download_pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);

	gst_object_unref(ahs_player->key_download_pipeline);
	ahs_player->key_download_pipeline = NULL;

	debug_log (">>>\n");

	return TRUE;
}
static gboolean
ahs_client_is_live (mm_player_ahs_t *ahs_player)
{
	if (MM_PLAYER_URI_TYPE_HLS == ahs_player->uri_type)
	{
		return (hls_client_is_live(ahs_player->ahs_client));
	}
	return TRUE;
}

static gboolean
ahs_manifest_get_update_interval (mm_player_ahs_t *ahs_player, GTimeVal *next_update)
{
	if (MM_PLAYER_URI_TYPE_HLS == ahs_player->uri_type)
	{
		return hls_playlist_update_interval(ahs_player->ahs_client, next_update);
	}
	return TRUE;
}

static gboolean
ahs_parse_manifest_update_client (mm_player_ahs_t *ahs_player)
{
	if (MM_PLAYER_URI_TYPE_HLS == ahs_player->uri_type)
	{
		return hls_parse_playlist_update_client(ahs_player->ahs_client, ahs_player->ahs_manifest_dmp_location);
	}
	else
	{
		return FALSE;
	}
}

static gboolean
ahs_manifest_download_callback(GstBus *bus, GstMessage *msg, gpointer data)
{
	mm_player_ahs_t *ahs_player = (mm_player_ahs_t*) data;
	gboolean bret = TRUE;
	
	debug_log ("<<<\n");

	if ( !ahs_player ) 
	{
		debug_error("AHS player handle is invalid\n");
		return FALSE;
	}

	switch ( GST_MESSAGE_TYPE( msg ) )
	{
		case GST_MESSAGE_EOS:
			{
				debug_log("MANIFEST EOS received, state=[%d]\n", ahs_player->ahs_state);
				
				if (ahs_player->manifest_download_pipeline)
					ahs_destory_manifest_download_pipeline(ahs_player);

				/* Parse and Update client*/
				ahs_parse_manifest_update_client (ahs_player);

				debug_log ("Current STATE = [%s]\n", state_string[ahs_player->ahs_state]);

				debug_log ("Received EOS on manifest download...broadcast manifest eos\n");
				g_cond_broadcast (ahs_player->manifest_eos_cond);

				/* state transition : MAIN PLAYLIST -> SUB PLAYLIST (optional) -> MEDIA FILE STREAMING */
				g_mutex_lock (ahs_player->state_lock);
				if (ahs_player->ahs_state == AHS_STATE_PREPARE_MANIFEST) 
				{
					if (MM_PLAYER_URI_TYPE_HLS == ahs_player->uri_type)
					{
						////////////////////////////////
						//// http live streaming 
						///////////////////////////////

						if (hls_downloaded_variant_playlist (ahs_player->ahs_client, ahs_player->cur_mf_uri))
						{
							debug_log ("Downloaded Variant playlist file\n");
							/* if set current playlist based on bandwidth */
							ahs_set_current_manifest (ahs_player);

							if (ahs_player->cur_mf_uri)
							{
								g_free (ahs_player->cur_mf_uri);
								ahs_player->cur_mf_uri = NULL;
							}

							ahs_player->cur_mf_uri = g_strdup(ahs_get_current_manifest(ahs_player));
							
							debug_log ("Signal to wakeup manifest thread...\n");
							g_cond_signal (ahs_player->manifest_update_cond);
						}
						else
						{
							ahs_player->ahs_state = AHS_STATE_MEDIA_STREAMING;
						}
					}
				}

				/* If state is for media streaming */
				debug_log ("Current STATE = [%s]\n", state_string[ahs_player->ahs_state]);
					
				if (AHS_STATE_MEDIA_STREAMING == ahs_player->ahs_state) 
				{
					debug_log ("Signal start of media download....\n");
					g_cond_signal (ahs_player->media_start_cond);
				}
				g_mutex_unlock (ahs_player->state_lock);
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
				new_msg = gst_message_new_error (GST_OBJECT_CAST ((GstAppSrc *)ahs_player->appsrc), error, debug);
				
				ret = gst_element_post_message ((GstAppSrc *)ahs_player->appsrc, new_msg);
				__mm_player_ahs_stop (ahs_player);
				//__mm_player_ahs_deinitialize(ahs_player);
				//__mm_player_ahs_destroy(&ahs_player);
				
				g_print ("\n\n\nError posting msg = %d\n\n\n\n", ret);
				//g_free( debug);
				//debug = NULL;
				//g_error_free( error);
			}
			break;

		case GST_MESSAGE_WARNING:
			{
				char* debug = NULL;
				GError* error = NULL;
				gst_message_parse_warning(msg, &error, &debug);
				debug_warning("warning : %s\n", error->message);
				debug_warning("debug : %s\n", debug);
				g_free( debug);
				debug = NULL;
				g_error_free( error);
			}
			break;

		default:
			//debug_warning("unhandled message\n");
			break;
	}

	debug_log (">>>\n");

	return bret;
}

static gboolean
ahs_key_download_callback(GstBus *bus, GstMessage *msg, gpointer data)
{
	mm_player_ahs_t *ahs_player = (mm_player_ahs_t*) data;
	gboolean bret = TRUE;
	
	debug_log ("<<<\n");

	if ( !ahs_player ) 
	{
		debug_error("AHS player handle is invalid\n");
		return FALSE;
	}

	switch ( GST_MESSAGE_TYPE( msg ) )
	{
		case GST_MESSAGE_EOS:
			{
				FILE *keyfd = NULL;
				guint bytes_read = 0;
				int i =0;
				
				debug_log("KEY EOS received, state=[%d]\n", ahs_player->ahs_state);
				if (ahs_player->key_download_pipeline) 
					ahs_destory_key_download_pipeline(ahs_player);

				debug_log ("Current STATE = [%s]\n", state_string[ahs_player->ahs_state]);

				if (MM_PLAYER_URI_TYPE_HLS == ahs_player->uri_type)
				{
					keyfd = fopen (ahs_player->ahs_key_dmp_location, "r");
					if (keyfd == NULL)
					{
						g_print ("failed to open key file...\n\n\n");
						return FALSE;
					}
					
					/* read key file */
					bytes_read = fread (ahs_player->cur_key_data, sizeof (unsigned char), 16, keyfd);
					if (sizeof(ahs_player->cur_key_data) != bytes_read)
					{
					    printf ("key file is not proper...bytes_read from key file = %d\n", bytes_read);
					    return FALSE;
					}	

					bret = hls_decryption_initialize (ahs_player->ahs_client, &ahs_player->cur_key_data, ahs_player->cur_iv);
					if (FALSE == bret)
					{
						g_print ("Failed to initialize encryption....\n\n");
					}
				}
				g_cond_signal (ahs_player->key_eos_cond);
				debug_log ("signalling key download EOS\n");
			}
			
			break;

		case GST_MESSAGE_ERROR:
			{
				GError *error = NULL;
				gchar* debug = NULL;
				/* get error code */
				//gst_message_parse_error( msg, &error, &debug );
				debug_error ("GST_MESSAGE_ERROR = %s\n", debug);
				
				gst_element_post_message ((GstAppSrc *)ahs_player->appsrc, msg);
				//g_free( debug);
				debug = NULL;
				//g_error_free( error);
			}
			break;

		case GST_MESSAGE_WARNING:
			{
				char* debug = NULL;
				GError* error = NULL;
				gst_message_parse_warning(msg, &error, &debug);
				debug_warning("warning : %s\n", error->message);
				debug_warning("debug : %s\n", debug);
				g_free( debug);
				debug = NULL;
				g_error_free( error);
			}
			break;

		default:
			//debug_warning("unhandled message\n");
			break;
	}
	return bret;
}


static gboolean
ahs_media_download_callback(GstBus *bus, GstMessage *msg, gpointer data)
{
	mm_player_ahs_t *ahs_player = (mm_player_ahs_t*) data;
	gboolean bret = TRUE;
	
	debug_log ("<<<\n");

	if ( !ahs_player ) 
	{
		debug_error("AHS player handle is invalid\n");
		return FALSE;
	}

	switch ( GST_MESSAGE_TYPE( msg ) )
	{
		case GST_MESSAGE_EOS:
			{
				glong diff = 0;
				GTimeVal time = {0, };
				
				debug_log("MEDIA EOS received, state=[%d]\n", ahs_player->ahs_state);
				
				if (ahs_player->media_download_pipeline) 
					ahs_destory_media_download_pipeline(ahs_player);
				
	  			g_get_current_time (&time);
				ahs_player->seg_end_time = (time.tv_sec * 1000000)+ time.tv_usec;
				debug_log ("end time in usec = %"G_GUINT64_FORMAT"\n", ahs_player->seg_end_time);

				diff = ahs_player->seg_end_time - ahs_player->seg_start_time;
				
				ahs_player->download_rate = (guint)((ahs_player->seg_size * 8 * 1000000) / diff);

				ahs_player->cache_frag_count++;

				
				debug_log("***********  frag_cnt = %d and download rate = %d bps **************\n", ahs_player->cache_frag_count, ahs_player->download_rate);

				/* first initial fragments go with least possible bit-rate */
				if (ahs_player->cache_frag_count == DEFAULT_FRAGMENTS_CACHE)
				{
					debug_log ("=======================================\n");
					debug_log (" \t Done with caching initial %d fragments\n", ahs_player->cache_frag_count);
					debug_log ("=======================================\n");
				}
				
				if (ahs_player->cache_frag_count >= DEFAULT_FRAGMENTS_CACHE)
				{
					ahs_switch_playlist (ahs_player, ahs_player->download_rate);				
				}

				ahs_player->seg_size = 0;

				g_mutex_lock (ahs_player->media_mutex);
				g_cond_broadcast (ahs_player->media_eos_cond);
				g_mutex_unlock (ahs_player->media_mutex);
		
				debug_log ("Signaled media ts EOS...\n");

			}
			break;

		case GST_MESSAGE_ERROR:
			{
				GError *error = NULL;
				gchar* debug = NULL;
				GstMessage *new_msg = NULL;
				gboolean ret = FALSE;
				
				/* get error code */
				gst_message_parse_error( msg, &error, &debug );
				debug_error ("GST_MESSAGE_ERROR = %s\n", debug);
				new_msg = gst_message_new_error (GST_OBJECT_CAST ((GstAppSrc *)ahs_player->appsrc), error, debug);
				
				ret = gst_element_post_message ((GstAppSrc *)ahs_player->appsrc, new_msg);
				
				//MMPLAYER_FREEIF( debug );
				//g_error_free( error );
			}
			break;

		case GST_MESSAGE_WARNING:
			{
				char* debug = NULL;
				GError* error = NULL;
				gst_message_parse_warning(msg, &error, &debug);
				debug_warning("warning : %s\n", error->message);
				debug_warning("debug : %s\n", debug);
				MMPLAYER_FREEIF( debug );
				g_error_free( error );
			}
			break;

		default:
			//debug_warning("unhandled message\n");
			break;
	}
	//debug_log (">>>\n");
	return bret;
}

static gboolean 
ahs_determining_next_file_load (mm_player_ahs_t *ahs_player, gboolean *is_ready)
{
	debug_log ("<<<\n");
	gboolean bret = TRUE;
	
	if (ahs_player->ahs_state == AHS_STATE_PREPARE_MANIFEST) 
	{
		debug_log ("STATE is not ready to download next file");
		return FALSE;
	}
		
	if (MM_PLAYER_URI_TYPE_HLS == ahs_player->uri_type)
	{
		bret = hls_determining_next_file_load (ahs_player->ahs_client, is_ready);
	}

	debug_log (">>>\n");
	return bret;
}


static gboolean
ahs_set_current_manifest (mm_player_ahs_t *ahs_player)
{
	gboolean bret = TRUE;
	
	bret = hls_set_current_playlist (ahs_player->ahs_client);
	if (FALSE == bret)
	{
		debug_error ("ERROR in setting current playlist....");
		return FALSE;
	}

	return bret;
}

static gchar* 
ahs_get_current_manifest (mm_player_ahs_t *ahs_player)
{
	return hls_get_current_playlist (ahs_player->ahs_client);
}

static gboolean
ahs_switch_playlist (mm_player_ahs_t *ahs_player, guint download_rate)
{
	if ((MM_PLAYER_URI_TYPE_HLS == ahs_player->uri_type) && (hls_can_switch (ahs_player->ahs_client)))
	{
		ahs_player->need_bw_switch = FALSE;
		
		hls_switch_playlist (ahs_player->ahs_client, download_rate, &ahs_player->need_bw_switch);

		debug_log ("Need BW Switch = %d\n", ahs_player->need_bw_switch);
		
		if (ahs_player->cur_mf_uri)
		{
			g_free (ahs_player->cur_mf_uri);
			ahs_player->cur_mf_uri = NULL;
		}

		ahs_player->cur_mf_uri = g_strdup(ahs_get_current_manifest(ahs_player));
							
		/* Start downloading sub playlist */

		g_mutex_lock (ahs_player->state_lock);
		ahs_player->ahs_state = AHS_STATE_PREPARE_MANIFEST;
		g_mutex_unlock (ahs_player->state_lock);	
			
	}

	return TRUE;
	
}

static gboolean 
ahs_get_next_media_uri (mm_player_ahs_t *ahs_player, gchar **media_uri, gchar **key_uri, char **iv)
{
	if (MM_PLAYER_URI_TYPE_HLS == ahs_player->uri_type)
	{
		return hls_get_next_media_fragment (ahs_player->ahs_client, media_uri, key_uri, iv);
	}
	
	return TRUE;
}

static gboolean
ahs_is_buffer_discontinuous (mm_player_ahs_t *ahs_player)
{
	if (MM_PLAYER_URI_TYPE_HLS == ahs_player->uri_type)
	{
		return hls_is_buffer_discontinuous (ahs_player->ahs_client);
	}
	else
	{
		return FALSE;
	}
}


static gboolean
ahs_clear_discontinuous (mm_player_ahs_t *ahs_player)
{
	if (MM_PLAYER_URI_TYPE_HLS == ahs_player->uri_type)
	{
		return hls_clear_discontinuous (ahs_player->ahs_client);
	}
	else
	{
		return FALSE;
	}
}

static gboolean
ahs_decrypt_media (mm_player_ahs_t *ahs_player,GstBuffer *InBuf, GstBuffer **OutBuf)
{
	if (MM_PLAYER_URI_TYPE_HLS == ahs_player->uri_type)
	{
		return hls_decrypt_media_fragment (ahs_player->ahs_client, InBuf, OutBuf);
	}

	return TRUE;
}

gboolean
ahs_check_allow_cache (mm_player_ahs_t *ahs_player)
{
	if (MM_PLAYER_URI_TYPE_HLS == ahs_player->uri_type)
	{
		return hls_check_allow_cache (ahs_player->ahs_client);
	}
	return FALSE;
}

gboolean
ahs_store_media_presentation (mm_player_ahs_t *ahs_player, unsigned char *buffer, unsigned int buffer_len)
{
	if (MM_PLAYER_URI_TYPE_HLS == ahs_player->uri_type)
	{
		return hls_store_media_presentation (ahs_player->ahs_client, buffer, buffer_len);
	}
}

mm_player_ahs_t * __mm_player_ahs_create ()
{
	g_print ("\n >>>>>>>>>>>CREATE AHS download\n");

	mm_player_ahs_t *ahs_player = NULL;
	
	ahs_player = (mm_player_ahs_t *) malloc (sizeof (mm_player_ahs_t));
	if (NULL == ahs_player)
	{
		debug_error ("Failed to created ahs_player handle...\n");
		goto ERROR;
	}
	
	ahs_player->manifest_mutex = g_mutex_new ();
	if (NULL == ahs_player->manifest_mutex)
	{
		goto ERROR;
	}
	ahs_player->manifest_eos_cond = g_cond_new ();
	if (NULL == ahs_player->manifest_eos_cond)
	{
		goto ERROR;
	}
	ahs_player->manifest_exit_cond = g_cond_new ();
	if (NULL == ahs_player->manifest_exit_cond)
	{
		goto ERROR;
	}
	ahs_player->manifest_update_cond = g_cond_new ();
	if (NULL == ahs_player->manifest_update_cond)
	{
		goto ERROR;
	}
	ahs_player->media_mutex = g_mutex_new ();
	if (NULL == ahs_player->media_mutex)
	{
		goto ERROR;
	}
	ahs_player->media_start_cond = g_cond_new ();
	if (NULL == ahs_player->media_start_cond)
	{
		goto ERROR;
	}
	ahs_player->media_eos_cond = g_cond_new ();
	if (NULL == ahs_player->media_eos_cond)
	{
		goto ERROR;
	}
	ahs_player->key_eos_cond = g_cond_new ();
	if (NULL == ahs_player->key_eos_cond)
	{
		goto ERROR;
	}
	ahs_player->state_lock = g_mutex_new ();
	if (NULL == ahs_player->state_lock)
	{
		goto ERROR;
	}
	
	ahs_player->uri_type = MM_PLAYER_URI_TYPE_NONE;
	ahs_player->is_initialized = FALSE;
	ahs_player->main_mf_uri = NULL;
	ahs_player->cur_mf_uri = NULL;
	ahs_player->cur_media_uri = NULL;
	ahs_player->appsrc = NULL;
	ahs_player->manifest_thread = NULL;
	ahs_player->media_thread = NULL;
	/* manifest related variables */
	ahs_player->manifest_thread = NULL;
	ahs_player->manifest_thread_exit = FALSE;
	ahs_player->manifest_download_pipeline = NULL;
	ahs_player->manifest_download_src = NULL;
	ahs_player->manifest_download_sink = NULL;	

	memset (ahs_player->ahs_manifest_dmp_location, 0, HLS_POSIX_PATH_MAX);

	/* media related variables */
	ahs_player->media_thread = NULL;
	ahs_player->media_thread_exit = FALSE;

	ahs_player->media_download_pipeline = NULL;
	ahs_player->media_download_src = NULL;
	ahs_player->media_download_sink = NULL;

	/* key related variables */
	ahs_player->cur_key_uri = NULL;
	memset (ahs_player->ahs_key_dmp_location, 0, 256);
	ahs_player->key_download_pipeline = NULL;
	ahs_player->key_download_src = NULL;
	ahs_player->key_download_sink = NULL;

	ahs_player->seg_start_time = 0;     /* segment start time in usec*/
	ahs_player->seg_end_time = 0;
	ahs_player->seg_size = 0;
	ahs_player->cache_frag_count = 0;
	
	ahs_player->hls_is_wait_for_reload = FALSE;

	return ahs_player;

ERROR:
	MMPLAYER_FREEIF(ahs_player);
   
	if (ahs_player->manifest_mutex)
		g_mutex_free(ahs_player->manifest_mutex);

	if ( ahs_player->manifest_eos_cond ) 
		g_cond_free ( ahs_player->manifest_eos_cond );
	
	if ( ahs_player->manifest_exit_cond ) 
		g_cond_free ( ahs_player->manifest_exit_cond );

	if ( ahs_player->manifest_update_cond )
		g_cond_free ( ahs_player->manifest_update_cond );

	if (ahs_player->media_mutex)
		g_mutex_free(ahs_player->media_mutex);

	if ( ahs_player->media_start_cond )
		g_cond_free ( ahs_player->media_start_cond );

	if ( ahs_player->media_eos_cond )
		g_cond_free ( ahs_player->media_eos_cond );

	if ( ahs_player->key_eos_cond )
		g_cond_free ( ahs_player->key_eos_cond );

	if (ahs_player->state_lock)
		g_mutex_free(ahs_player->state_lock);

	return NULL;
	
	
}

gboolean __mm_player_ahs_initialize (mm_player_ahs_t *ahs_player, int uri_type, char *uri, GstElement *appsrc)
{
	debug_log ("<<<\n");

	if (NULL == ahs_player)
	{
		debug_error (" Invalid argument\n");
		return FALSE;
	}

	/* initialize ahs common variables */
	ahs_player->uri_type = uri_type;
	ahs_player->main_mf_uri = g_strdup (uri);
	ahs_player->cur_mf_uri = g_strdup (uri);
	ahs_player->appsrc = appsrc;
	ahs_player->ahs_state = AHS_STATE_STOP;
	ahs_player->need_bw_switch = FALSE;
	
	if (MM_PLAYER_URI_TYPE_HLS == ahs_player->uri_type)
	{
		/* http live streaming */
		ahs_player->ahs_client = __mm_player_hls_create ();
		if (NULL == ahs_player->ahs_client)
		{
			return FALSE;
		}
		__mm_player_hls_initialize (ahs_player->ahs_client, ahs_player->main_mf_uri);
	}

	debug_log (">>>\n");
	
	ahs_player->is_initialized = TRUE;

	return TRUE;
}

gboolean __mm_player_ahs_start (mm_player_ahs_t *ahs_player)
{
	gboolean bret = TRUE;

	debug_log ("<<<\n");

	if (NULL == ahs_player)
	{
		debug_error (" Invalid argument\n");
		return FALSE;
	}
	
	/* download manifest file */
	if (!ahs_create_manifest_download_pipeline (ahs_player))
		return FALSE;

	ahs_player->ahs_state = AHS_STATE_PREPARE_MANIFEST;

	ahs_player->manifest_thread = g_thread_create (manifest_update_thread, (gpointer)ahs_player, TRUE, NULL);

	if ( !ahs_player->manifest_thread ) 
	{
		debug_error("failed to create thread : manifest\n");
		goto ERROR;
	}
	
	/* media download thread */
	ahs_player->media_thread = g_thread_create (media_download_thread, (gpointer)ahs_player, TRUE, NULL);

	if ( !ahs_player->media_thread ) 
	{
		debug_error("failed to create thread : media\n");
		goto ERROR;
	}

	debug_log (">>>\n");

	return bret;

ERROR:

	if (ahs_player->manifest_thread)
	{
		g_thread_join( ahs_player->manifest_thread);
		ahs_player->manifest_thread = NULL;
	}

	if (ahs_player->media_thread)
	{
		g_thread_join( ahs_player->media_thread);
		ahs_player->media_thread = NULL;
	}
	
	return FALSE;
}

gboolean __mm_player_ahs_pause (mm_player_ahs_t *ahs_player)
{
	gboolean bret = TRUE;

	debug_log ("<<<\n");
	
	if (NULL == ahs_player)
	{
		debug_error (" Invalid argument\n");
		return FALSE;
	}

	g_mutex_lock (ahs_player->state_lock);
	if (AHS_STATE_MEDIA_STREAMING != ahs_player->ahs_state)
	{
		g_print ("Pipeline is not in playing state...\n");
		g_mutex_unlock (ahs_player->state_lock);
		return TRUE;
	}
	g_mutex_unlock (ahs_player->state_lock);

	if (ahs_client_is_live (ahs_player))
	{
		debug_log("Live playlist flush now");
	}

	return bret;

}

gboolean __mm_player_ahs_deinitialize (mm_player_ahs_t *ahs_player)
{
	debug_log ("<<<\n");

	if (NULL == ahs_player)
	{
		debug_error (" Invalid argument\n");
		return FALSE;
	}

	if (FALSE == ahs_player->is_initialized)
	{
		debug_log ("already de-initlialized\n");

		return TRUE;
	}
	
	ahs_player->need_bw_switch = FALSE;
	ahs_player->hls_is_wait_for_reload= FALSE;

	ahs_player->appsrc = NULL;
	
	if (MM_PLAYER_URI_TYPE_HLS == ahs_player->uri_type)
	{
		if (ahs_player->ahs_client)
		{
			__mm_player_hls_destroy (ahs_player->ahs_client);
			ahs_player->ahs_client = NULL;
		}
	}

	/* manifest related variables */
	ahs_player->manifest_thread = NULL;
	ahs_player->manifest_thread_exit = TRUE;
	ahs_player->uri_type = MM_PLAYER_URI_TYPE_NONE;
	

	/* media related variables */
	ahs_player->media_thread = NULL;
	ahs_player->media_thread_exit = TRUE;

	
	ahs_player->is_initialized = FALSE;

	if (ahs_player->ahs_manifest_dmp_location)
		g_unlink(ahs_player->ahs_manifest_dmp_location);
	
	debug_log (">>>\n");

	return TRUE;
}

gboolean __mm_player_ahs_stop (mm_player_ahs_t *ahs_player)
{
	GstFlowReturn fret = GST_FLOW_OK;

	debug_fenter ();

	if (NULL == ahs_player)
	{
		debug_error ("Invalid argument...\n");
		return FALSE;
	}
	
	if (ahs_player->ahs_state == AHS_STATE_STOP)
	{
		debug_log ("Already in AHS STOP state...\n");
		return TRUE;
	}
	
	if (ahs_player->appsrc)
	{
		debug_log ("Push EOS to playback pipeline\n");
		// TODO: try to send FLUSH event instead of EOS
		fret = gst_app_src_end_of_stream ((GstAppSrc *)ahs_player->appsrc);
		if (fret != GST_FLOW_OK)
		{
			debug_error ("Error in pushing EOS to appsrc: reason - %s\n", gst_flow_get_name(fret));
		} 
	}
	
	ahs_player->ahs_state = AHS_STATE_STOP;

	if (ahs_player->media_thread)
	{
		g_mutex_lock (ahs_player->media_mutex);
		ahs_player->media_thread_exit = TRUE;
		g_mutex_unlock (ahs_player->media_mutex);

		g_cond_broadcast (ahs_player->manifest_eos_cond);		
		g_cond_broadcast (ahs_player->media_eos_cond);
		g_cond_broadcast (ahs_player->media_start_cond);
		g_cond_broadcast (ahs_player->key_eos_cond);

		debug_log ("waiting for media thread to finish\n");
		g_thread_join (ahs_player->media_thread);
		ahs_player->media_thread = NULL;
		debug_log("media thread released\n");		
	}	

	if (ahs_player->manifest_thread)
	{
		g_mutex_lock (ahs_player->manifest_mutex);
		ahs_player->manifest_thread_exit = TRUE;
		g_mutex_unlock (ahs_player->manifest_mutex);
		
		g_cond_broadcast (ahs_player->manifest_update_cond);
		g_cond_broadcast (ahs_player->manifest_eos_cond);

		debug_log ("waiting for manifest thread to finish\n");
		g_thread_join (ahs_player->manifest_thread);
		ahs_player->manifest_thread = NULL;
		debug_log("manifest thread released\n");
	}

	if (ahs_player->manifest_download_pipeline)
		ahs_destory_manifest_download_pipeline(ahs_player);

	if (ahs_player->media_download_pipeline)
		ahs_destory_media_download_pipeline(ahs_player);

	if (ahs_player->key_download_pipeline)
		ahs_destory_key_download_pipeline(ahs_player);

	debug_fleave ();

	return TRUE;

}

gboolean __mm_player_ahs_destroy (mm_player_ahs_t *ahs_player)
{
	debug_log ("<<<<<\n");

	if (NULL == ahs_player)
	{
		debug_error ("Invalid argument...\n");
		return TRUE;
	}

	debug_log ("\nBroadcasting signals from destory\n\n\n");	

	if (ahs_player->manifest_update_cond)
		g_cond_broadcast (ahs_player->manifest_update_cond);
	
	if (ahs_player->manifest_eos_cond)
		g_cond_broadcast (ahs_player->manifest_eos_cond);

	if (ahs_player->media_eos_cond)
		g_cond_broadcast (ahs_player->media_eos_cond);
	
	if (ahs_player->media_start_cond)
		g_cond_broadcast (ahs_player->media_start_cond);
	
	if (ahs_player->key_eos_cond)
		g_cond_broadcast (ahs_player->key_eos_cond);

	if (ahs_player->manifest_thread)
	{
		g_thread_join (ahs_player->manifest_thread);
	}
	
	if (ahs_player->media_thread)
	{
		g_thread_join (ahs_player->media_thread);
	}

	/* initialize ahs common variables */
	if (ahs_player->main_mf_uri)
	{
		g_free (ahs_player->main_mf_uri);
		ahs_player->main_mf_uri = NULL;
	}
	
	if (ahs_player->cur_mf_uri)
	{
		g_free (ahs_player->cur_mf_uri);
		ahs_player->cur_mf_uri = NULL;
	}
	
	if (ahs_player->cur_media_uri)
	{
		g_free (ahs_player->cur_media_uri);
		ahs_player->cur_media_uri = NULL;
	}
	if (ahs_player->manifest_mutex)
	{
		g_mutex_free (ahs_player->manifest_mutex);
		ahs_player->manifest_mutex = NULL;
	}
	if (ahs_player->manifest_eos_cond)
	{
		g_cond_free (ahs_player->manifest_eos_cond);
		ahs_player->manifest_eos_cond = NULL;
	}
	if (ahs_player->manifest_exit_cond)
	{
		g_cond_free (ahs_player->manifest_exit_cond);
		ahs_player->manifest_exit_cond = NULL;
	}	
	if (ahs_player->manifest_update_cond)
	{
		g_cond_free (ahs_player->manifest_update_cond);
		ahs_player->manifest_update_cond = NULL;
	}
	if (ahs_player->media_mutex)
	{
		g_mutex_free (ahs_player->media_mutex);
		ahs_player->media_mutex = NULL;
	}
	if (ahs_player->media_eos_cond)
	{
		g_cond_free (ahs_player->media_eos_cond);
		ahs_player->media_eos_cond = NULL;
	}
	if (ahs_player->media_start_cond)
	{
		g_cond_free (ahs_player->media_start_cond);
		ahs_player->media_start_cond = NULL;
	}	

	/* key related variables */
	if (ahs_player->key_eos_cond)
	{
		g_cond_free (ahs_player->key_eos_cond);
		ahs_player->key_eos_cond = NULL;
	}	
	if (ahs_player->cur_key_uri )
	{
		g_free (ahs_player->cur_key_uri);
		ahs_player->cur_key_uri = NULL;
	}

	if (ahs_player->ahs_client)
	{
		__mm_player_hls_destroy(ahs_player->ahs_client);
		ahs_player->ahs_client = NULL;
	}

	free (ahs_player);
	ahs_player = NULL;

	debug_log (">>>>>\n");
	
	return TRUE;
}
