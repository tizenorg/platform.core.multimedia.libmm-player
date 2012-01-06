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

#include "mm_player_pd.h"

static gboolean
pd_download_callback(GstBus *bus, GstMessage *msg, gpointer data)
{
	mm_player_pd_t *pd_downloader = (mm_player_pd_t*) data;
	gboolean bret = TRUE;
	
	debug_log ("<<<\n");

	if ( !pd_downloader ) 
	{
		debug_error("PD player handle is invalid\n");
		return FALSE;
	}

	switch ( GST_MESSAGE_TYPE( msg ) )
	{
		case GST_MESSAGE_EOS:
			{
				debug_log("PD EOS received....\n");
				g_object_set (G_OBJECT (pd_downloader->pushsrc), "eos", TRUE, NULL); 
				#ifdef PD_SELF_DOWNLOAD
				__mm_player_pd_stop (pd_downloader);
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
				
				ret = gst_element_post_message (pd_downloader->pushsrc, new_msg);
				__mm_player_pd_stop (pd_downloader);
			
				g_print ("\n\n\nError posting msg = %d\n\n\n\n", ret);
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


mm_player_pd_t * __mm_player_pd_create ()
{
	g_print (">>>>>>>>>>>CREATE PD downloader\n");

	mm_player_pd_t *pd_downloader = NULL;
	
	pd_downloader = (mm_player_pd_t *) malloc (sizeof (mm_player_pd_t));
	if (NULL == pd_downloader)
	{
		debug_error ("Failed to create pd_downloader handle...\n");
		return NULL;
	}
	g_print (">>>>>>>>>>>CREATE PD downloader DONE\n");

	return pd_downloader;
	
}

gboolean __mm_player_pd_destroy (mm_player_pd_t *pd_downloader)
{
	g_print ("\n >>>>>>>>>>>Destroying PD download\n");
	debug_log ("<<<<<\n");

	if (NULL == pd_downloader)
	{
		debug_error ("Invalid argument...\n");
		g_print ("Invalid argument...\n");
		return TRUE;
	}

	if (pd_downloader->download_pipe)
		__mm_player_pd_stop (pd_downloader);

	free (pd_downloader);
	pd_downloader = NULL;

	g_print ("\n >>>>>>>>>>>Destroying PD download DONE\n");
	debug_log (">>>>>\n");
	
	return TRUE;
}

gboolean __mm_player_pd_initialize (mm_player_pd_t *pd_downloader, gchar *src_uri, gchar *dst_uri, GstElement *pushsrc)
{
	debug_log ("<<<\n");

	if (NULL == pd_downloader)
	{
		debug_error (" Invalid argument\n");
		return FALSE;
	}

	g_print (">>>>>>> PD Initialize...\n");

	pd_downloader->download_uri = g_strdup (src_uri);
	pd_downloader->pd_file_dmp_location = g_strdup (dst_uri);
	pd_downloader->pushsrc = pushsrc;
	
	return TRUE;
}

gboolean __mm_player_pd_deinitialize (mm_player_pd_t *pd_downloader)
{

	if (pd_downloader->download_uri)
	{
		g_free (pd_downloader->download_uri);
		pd_downloader->download_uri = NULL;
	}
	return TRUE;
}

gboolean __mm_player_pd_start (mm_player_pd_t *pd_downloader)
{
	GstBus* bus = NULL;
	gboolean bret = FALSE;
	GstStateChangeReturn	sret = GST_STATE_CHANGE_SUCCESS;
	GstState cur_state;
	GstState pending_state;

	debug_log ("<<<\n");
	pd_downloader->download_pipe = gst_pipeline_new ("PD Downloader");
	if (NULL == pd_downloader->download_pipe)
	{
		debug_error ("Can't create PD download pipeline...");
		return FALSE;
	}
	pd_downloader->download_src = gst_element_factory_make ("souphttpsrc", "PD HTTP download source");
	if (NULL == pd_downloader->download_src)
	{
		debug_error ("Can't create PD download src...");
		return FALSE;
	}
	pd_downloader->download_queue = gst_element_factory_make ("queue", "PD download queue");
	if (NULL == pd_downloader->download_queue)
	{
		debug_error ("Can't create PD download queue...");
		return FALSE;
	}
	pd_downloader->download_sink = gst_element_factory_make ("filesink", "PD download sink");
	if (NULL == pd_downloader->download_sink)
	{
		debug_error ("Can't create PD download sink...");
		return FALSE;
	}
	
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
	
	/* Set Bus */
	bus = gst_pipeline_get_bus (GST_PIPELINE (pd_downloader->download_pipe));
	gst_bus_add_watch (bus, pd_download_callback, pd_downloader);
	gst_object_unref (bus);
	
	/* Set URI on HTTP source */
	g_object_set (G_OBJECT (pd_downloader->download_src), "location", pd_downloader->download_uri, NULL);

	/* set file download location on filesink*/
	g_object_set (G_OBJECT (pd_downloader->download_sink), "location", pd_downloader->pd_file_dmp_location, NULL);

	debug_log ("src location = %s, save location = %s\n", pd_downloader->download_uri, pd_downloader->pd_file_dmp_location);

	g_print ("Going to download PD-uri -> %s\n", pd_downloader->download_uri);

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

	g_print ("\n\n\nPD downloader :: cur_state = %d and pending_state = %d\n\n\n", cur_state, pending_state);
	
	debug_log (">>>\n");

	return TRUE;
}

gboolean __mm_player_pd_stop (mm_player_pd_t *pd_downloader)
{
	debug_log ("<<<\n");

	gst_element_set_state (pd_downloader->download_pipe, GST_STATE_NULL);
	gst_element_get_state (pd_downloader->download_pipe, NULL, NULL, GST_CLOCK_TIME_NONE);

	gst_object_unref(pd_downloader->download_pipe);
	pd_downloader->download_pipe = NULL;

	debug_log (">>>\n");

	return TRUE;
}

