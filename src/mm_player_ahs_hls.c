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

#include "mm_player_ahs_hls.h"

static const float update_interval_factor[] = { 1, 0.5, 1.5, 3 };
#define AES_BLOCK_SIZE 16
#define BITRATE_SWITCH_UPPER_THRESHOLD 0.4
#define BITRATE_SWITCH_LOWER_THRESHOLD 0.1

static gboolean hls_switch_to_lowerband (mm_player_hls_t * player, guint download_rate);
static gboolean hls_switch_to_upperband (mm_player_hls_t * ahs_player, GList *next_bw_lists, guint download_rate);

/* utility function for removing '\r' character in string */
char* string_replace (char* text, int len)
{
	int i=0;
	char *dst_ptr = NULL;
	char* tmp = malloc (len);
	if (NULL == tmp)
	{
		debug_error ("Failed to allocate memory...\n");
		return NULL;
	}
	memset (tmp, 0, len);
	dst_ptr = tmp;

	for (i=0; i<len; i++) 	{
		if (text[i] != '\r') {
			*dst_ptr++ = text[i];
		}
	}
	return tmp;
}

gint my_compare (gconstpointer a,  gconstpointer b)
{
	int first  = ((GstM3U8*)a)->bandwidth;
	int second = ((GstM3U8*)b)->bandwidth;
	return second - first;
}

void hls_dump_mediafile (GstM3U8MediaFile* mediafile)
{
	debug_log ("[%d][%d][%s][%s]\n", mediafile->sequence, mediafile->duration, mediafile->title, mediafile->uri);
}

void hls_dump_m3u8 (GstM3U8* m3u8)
{
	debug_log ("uri = [%s]\n", m3u8->uri);
	debug_log ("version = [%d], endlist = [%d], bandwidth = [%d], target duration = [%d], mediasequence = [%d]\n",
			m3u8->version, m3u8->endlist, m3u8->bandwidth, m3u8->targetduration, m3u8->mediasequence);
	debug_log ("allow cache = %s, program_id = %d, codecs = %s\n", m3u8->allowcache, m3u8->program_id, m3u8->codecs);
	debug_log ("width = %d, height = %d\n", m3u8->width, m3u8->height);
	debug_log ("last data = [%s], parent = [%p]\n", m3u8->last_data, m3u8->parent);

	GList* tmp = m3u8->lists;
	while (tmp) {
		debug_log ("################### SUB playlist ##################\n");
		hls_dump_m3u8 (tmp->data);
		tmp = g_list_next(tmp);
	}

	tmp = m3u8->files;
	while (tmp) {
		hls_dump_mediafile (tmp->data);
		tmp = g_list_next(tmp);
	}
}

void hls_dump_playlist (void *hls_handle)
{
	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;

	debug_log ("============================== DUMP PLAYLIST =================================\n");
	debug_log ("update_failed_count = %d, seq = %d\n", hls_player->client->update_failed_count, hls_player->client->sequence);

	if (hls_player->client->current)	
	{
		debug_log ("################### CURRENT playlist ##################\n");
		hls_dump_m3u8 (hls_player->client->current);
	}
	debug_log ("================================    E N D    =================================\n");
}

gboolean hls_parse_playlist_update_client (void *hls_handle, char* playlist)
{
	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;
	GError *error = NULL;
	guint8 *data = NULL;
	
	debug_log ("<<<  Playlist =  [%s] \n" ,playlist);

	/* prepare MMAP */
	GMappedFile *file = g_mapped_file_new (playlist, TRUE, &error);
	if (error) 
	{
		g_print ("failed to open file: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}
	
	data = (guint8 *) g_mapped_file_get_contents (file);

	/* Get replaced text (remove '\r') */
	char* replaced_text = string_replace (data, g_mapped_file_get_length (file));

	/* update with replaced text */
	if (gst_m3u8_client_update (hls_player->client, replaced_text))	
	{
		/* DUMP */
		hls_dump_playlist (hls_player);
	}
	else 
	{
		g_print ("\n\n!!!!!!!!!!!!!!!! RELOADED but NO changes!!!!!!\n\n");
	}

	/* clean up */
	g_mapped_file_unref (file);
	
	return TRUE;

	debug_log (">>>\n");
}


gboolean hls_set_current_playlist (mm_player_hls_t *hls_player)
{
	GList* copied = NULL;
	GList* tmp = NULL;
	
	debug_log ("<<<\n");

	if (hls_player->client->main->endlist)	
	{
		debug_log ("main playlist has endlist. No need to set current playlist\n");
		return FALSE;
	}

	copied = g_list_copy (hls_player->client->main->lists);
	tmp = g_list_sort (copied, my_compare);
	
	/* Set initial bandwidth */
	if (tmp) 
	{
		GstM3U8* data = (GstM3U8*)tmp->data;
		
		if (data) 
		{
			debug_log ("Set Initial bandwidth\n");
			gst_m3u8_client_set_current (hls_player->client, data);
			g_print ("Setting to initial BW = %d\n", data->bandwidth);
			tmp = g_list_next(tmp);
		}
		else 
		{
			debug_error ("Initial media data is not valid!!!\n");
			g_list_free (copied);
			return FALSE;
		}
	}
	g_list_free (copied);

	debug_log (">>>\n");

	return TRUE;
}

gchar  *hls_get_current_playlist (mm_player_hls_t *hls_player)
{
	return hls_player->client->current->uri;
}

gboolean hls_has_variant_playlist (void *hls_handle)
{
	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;
	
	if ( gst_m3u8_client_has_variant_playlist (hls_player->client)) 
	{
		return TRUE;
	}
	else
	{
		debug_log ("playlist is simple playlist file....");
		return FALSE;
	}
}

gboolean hls_is_variant_playlist (void *hls_handle)
{
	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;
	
	if ( gst_m3u8_client_has_variant_playlist (hls_player->client) && (hls_player->client->current->files == NULL)) 
	{
		return TRUE;
	}
	else
	{
		debug_log ("playlist is simple playlist file....");
		return FALSE;
	}
}

gboolean
hls_downloaded_variant_playlist (void *hls_handle, gchar *cur_mf_uri)
{
	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;

	if ( gst_m3u8_client_has_variant_playlist (hls_player->client)) 
	{
		if (!strcmp(hls_player->client->main->uri, cur_mf_uri))
		{
			debug_log ("downloaded variant master playlist...\n");
			return TRUE;
		}
		else
		{
			debug_log ("downloaded slave playlist file...\n");
			return FALSE;
		}
	}
	else
	{
		debug_log ("playlist is simple one...\n");
		return FALSE;
	}

}

gboolean hls_can_switch (void *hls_handle)
{
	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;

	return gst_m3u8_client_has_variant_playlist (hls_player->client);
}


gboolean hls_client_is_live (void *hls_handle)
{
	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;
	
	return gst_m3u8_client_is_live(hls_player->client);
}

gboolean hls_playlist_update_interval (void *hls_handle, GTimeVal *next_update)
{
	gfloat update_factor;
	gint count;
	guint64 update = 0;
	
	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;

	count = hls_player->client->update_failed_count;
  	if (count < 3)
    		update_factor = update_interval_factor[count];
  	else
    		update_factor = update_interval_factor[3];

	if (hls_player->client->current->targetduration == 0)
	{
		hls_player->client->current->targetduration = 10; //hack
	}
	g_time_val_add (next_update, hls_player->client->current->targetduration * update_factor * 1000000);

	update = (next_update->tv_sec * 1000000)+ next_update->tv_usec;


	return TRUE;
}

gboolean 
hls_determining_next_file_load (void *hls_handle, gboolean *is_ready)
{
	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;

	debug_log ("<<<\n");
	
	if (gst_m3u8_client_is_live(hls_player->client)) 
	{
		/* If next file is playable, then let it start */
		if (gst_m3u8_client_check_next_fragment (hls_player->client)) 	
		{
			*is_ready = TRUE;
		} 
		else 
		{
			*is_ready = FALSE;
 			debug_log ("Not ready to play next file!!!!! wait for reloading........\n");	
		}
	}
	else
	{
		*is_ready = TRUE;
		debug_log ("No available media and ENDLIST exists, so Do nothing!!!! ");	
	}

	debug_log (">>>\n");
	return FALSE;
}



static gboolean
hls_switch_to_upperband (mm_player_hls_t * ahs_player, GList *next_bw_lists, guint download_rate)
{
	GstM3U8 *next_client = NULL;
	GList *final_bw_lists = NULL;

	while (1)
	{
	       next_client = (GstM3U8 *)next_bw_lists->data;
		if (download_rate > (next_client->bandwidth + (BITRATE_SWITCH_UPPER_THRESHOLD * next_client->bandwidth)))
		{
			final_bw_lists = next_bw_lists;
			next_bw_lists = g_list_next (next_bw_lists);
			if (NULL == next_bw_lists)
			{
				debug_log ("\n****** reached max BW possible.... *********\n");
				ahs_player->client->main->lists = final_bw_lists;
				gst_m3u8_client_set_current (ahs_player->client, final_bw_lists->data);
				debug_log("Client is MAX FAST, switching to bitrate %d\n", ahs_player->client->current->bandwidth);	
				break;				
			}
		}
		else
		{
			// TODO: Wrong condition.. need to check
			#if 0
			ahs_player->client->main->lists = final_bw_lists;
			gst_m3u8_client_set_current (ahs_player->client, final_bw_lists->data);
			#endif
			debug_log("Client is FAST, switching to bitrate %d\n", ahs_player->client->current->bandwidth);	
			break;
  		}
	}

	return TRUE;
}

static gboolean
hls_switch_to_lowerband (mm_player_hls_t * player, guint download_rate)
{
	GList *cur_bw_lists = player->client->main->lists;
	GList *prev_bw_lists = NULL;
	GList *present_bw_lists = player->client->main->lists;
	GstM3U8 *cur_client = (GstM3U8 *)cur_bw_lists->data;;
	GstM3U8 *present_client = NULL;
	
	while (1)
	{
		present_client = (GstM3U8 *)present_bw_lists->data;
		
		if (download_rate < (present_client->bandwidth + (BITRATE_SWITCH_LOWER_THRESHOLD * present_client->bandwidth)))
		{
			prev_bw_lists = present_bw_lists;
			
			if(NULL == present_bw_lists->prev) 
			{
				if (cur_client->bandwidth == present_client->bandwidth)
				{
					debug_log ("\n\n\n We are at same lower BW\n\n\n");
					return TRUE;
				}
				debug_log ("\n****** reached min BW possible.... *********\n\n");
				player->client->main->lists = prev_bw_lists;
				gst_m3u8_client_set_current (player->client, prev_bw_lists->data);
				debug_log("Client is MAX SLOW, switching to bitrate %d\n", player->client->current->bandwidth);	
				return TRUE;				
			}

			present_bw_lists = g_list_previous (present_bw_lists);

		}
		else
		{
			player->client->main->lists = present_bw_lists;
			gst_m3u8_client_set_current (player->client, present_bw_lists->data);
			debug_log ("Client is SLOW, switching to bitrate %d\n", player->client->current->bandwidth);	
			break;
  		}
	}

	return TRUE;
}

gboolean hls_switch_playlist (void *hls_handle, guint download_rate, gboolean *need_bw_switch)
{
	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;

	GList *next_bw_lists = NULL;
	GstM3U8 *next_client = NULL;
	GList *present_bw_lists = NULL;
	GstM3U8 *present_client = NULL;
	GList *prev_bw_lists = NULL;
	GstM3U8 *prev_client = NULL;

	present_bw_lists = hls_player->client->main->lists;
	present_client = (GstM3U8 *)present_bw_lists->data;
	if (NULL == present_client)
	{
		g_print ("\n\n ERROR : Present client is NULL\n\n");
		return FALSE;
	}

	next_bw_lists = g_list_next (hls_player->client->main->lists);
	if (next_bw_lists)
	{
		next_client = (GstM3U8 *)next_bw_lists->data;
	}
	
	prev_bw_lists = g_list_previous (hls_player->client->main->lists);
	if (prev_bw_lists)
	{
		prev_client = (GstM3U8 *)prev_bw_lists->data;
	}
	
	if (next_bw_lists && (download_rate > (next_client->bandwidth + (BITRATE_SWITCH_UPPER_THRESHOLD * next_client->bandwidth))))
	{
		/* our download rate is 40% more than next bandwidth available */
		hls_switch_to_upperband (hls_player, next_bw_lists, download_rate);
		g_print (">>>>>>> Need to switch to UPPER BW [ %d ]\n", hls_player->client->current->bandwidth);

		*need_bw_switch = TRUE;
	}
	if (prev_bw_lists && (download_rate < (present_client->bandwidth + (BITRATE_SWITCH_LOWER_THRESHOLD * present_client->bandwidth))))
	{
		hls_switch_to_lowerband (hls_player, download_rate);	
		g_print ("<<<<< Need to switch to LOWER BW [ %d ]\n", hls_player->client->current->bandwidth);
		*need_bw_switch = TRUE;
	}

	return TRUE;
}

gboolean hls_get_next_media_fragment (void *hls_handle, gchar **media_uri, gchar **key_uri,  char **iv)
{
	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;
	GstM3U8MediaFile *next_fragment_file = NULL;
	gboolean discontinuity = FALSE;
       char *tmp_iv = NULL;
	   
	/* Get Next file to download */
	next_fragment_file = gst_m3u8_client_get_next_fragment (hls_player->client, &discontinuity);
	if (next_fragment_file) 
	{
		hls_player->discontinuity = discontinuity;
		if (discontinuity)
			g_print ("\n\n\n!!!!!!!!! Discontinuity in fragments.....!!!!!!!!\n\n\n");
		
		debug_log ("media uri = %s and key_url = %s\n",next_fragment_file->uri, next_fragment_file->key_url);
		if (next_fragment_file->key_url)
		{
			tmp_iv = *iv;
			int i;

			for (i = 0; i< 16; i++)
			{
				debug_log ("%x", next_fragment_file->iv[i]);
				tmp_iv[i] = next_fragment_file->iv[i];
			}
			*iv = tmp_iv;
			*key_uri = g_strdup (next_fragment_file->key_url);
		}		
		if (next_fragment_file->uri)
		{
			*media_uri = g_strdup (next_fragment_file->uri);
		}
	}
	else
	{
		debug_log ("\n\n ++++++++ No fragment remaining..... +++++++++\n\n");
		return TRUE;
	}
	return TRUE;
}

gboolean
hls_is_buffer_discontinuous (void *hls_handle)
{
	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;

	return hls_player->discontinuity;
}

gboolean
hls_clear_discontinuous (void *hls_handle)
{
	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;

	hls_player->discontinuity = 0;

	return TRUE;
}

gboolean
hls_check_allow_cache (void *hls_handle)
{
	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;

	// TODO: doing intentioanlly for testing
	return FALSE;

       /* if allow_cache tag exists and if it is NO.. then dont cache media file. Otherwise, client can cache */	   
	if (gst_m3u8_client_allow_cache(hls_player->client) &&
		(!strncmp (gst_m3u8_client_allow_cache(hls_player->client), "NO", 2)))
	{
		debug_log ("\nClient SHOULD NOT cache media content...\n");
		return FALSE;
	}
	else
	{
		debug_log ("\nClient MAY cache media content...\n");
		return TRUE;
	}

}

gboolean
hls_store_media_presentation (void *hls_handle, unsigned char *buffer, unsigned int buffer_len)
{
 	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;
	gint fd = -1;
	gint written = 0;
	
	if (NULL == hls_player->allow_cache_fd)
	{
		gchar *name = NULL;
			
		/* make copy of the template, we don't want to change this */
		name = g_strdup ("/tmp/XXXXXX");

		g_print ("\n========> Cached filename = %s\n", name);
		
		fd = g_mkstemp (name);

		g_free(name);
		
		if (fd == -1)
		{
	  		g_print ("\n\n\nFailed to create tmp file for writing...\n\n\n");
	  		return FALSE;
		}
		
		/* open the file for update/writing */
    		hls_player->allow_cache_fd = fdopen (fd, "wb+");
		if (NULL == hls_player->allow_cache_fd)
		{
			g_print ("Failed to create allow cache file ptr\n\n\n");
			return FALSE;
		}
	}


	written = fwrite (buffer, 1, buffer_len, hls_player->allow_cache_fd);
	debug_log ("Cache file written %d bytes ....", written);

	return TRUE;

}

gboolean hls_decryption_initialize (void *hls_handle, gchar *key_data, unsigned char *iv)
{
	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;

	EVP_CIPHER_CTX_init(&(hls_player->decrypt));
	EVP_DecryptInit_ex(&(hls_player->decrypt), EVP_aes_128_cbc(), NULL, key_data, iv);
	
	return TRUE;
}

gboolean
hls_decrypt_media_fragment (void *hls_handle,GstBuffer *InBuf, GstBuffer **OutBuf)
{
	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;

	unsigned int inbuf_size = 0;
	unsigned char *inbuf_data = NULL;
	int remain = 0;
	unsigned int out_len = 0;
	GstBuffer *buffer = NULL;
						
	if (hls_player->remained)
	{
		buffer = gst_buffer_merge (hls_player->remained, InBuf);
		if (NULL == buffer)
		{
			g_print ("ERROR: Failed to merge...\n");
			return FALSE;
		}
				
		gst_buffer_unref (hls_player->remained);
		hls_player->remained = NULL;
	}
	else
	{
		buffer = InBuf;
	}

	inbuf_size = GST_BUFFER_SIZE (buffer);
	inbuf_data = GST_BUFFER_DATA (buffer);
	
	remain = inbuf_size % AES_BLOCK_SIZE;
			
	if (remain)
	{
		hls_player->remained = gst_buffer_new_and_alloc (remain);
		if (NULL == hls_player->remained)
		{
			g_print ("Error: failed to allocate memory...\n");
			return FALSE;
		}
				
		/* copy remained portion to buffer */
		memcpy (GST_BUFFER_DATA(hls_player->remained), inbuf_data + inbuf_size- remain, remain);
	}
			
	out_len = inbuf_size - remain + AES_BLOCK_SIZE;

	*OutBuf = gst_buffer_new_and_alloc (out_len);
	if (NULL == *OutBuf)
	{
		g_print ("Failed to allocate memory for OutBuf...\n");
		return FALSE;
	}
				
	EVP_DecryptUpdate(&(hls_player->decrypt), GST_BUFFER_DATA(*OutBuf), &out_len, inbuf_data, inbuf_size - remain);

	GST_BUFFER_SIZE (*OutBuf) = out_len;

	return TRUE;

}

void * __mm_player_hls_create ()
{
	mm_player_hls_t *hls_player = NULL;
	
	hls_player = (mm_player_hls_t *) malloc (sizeof (mm_player_hls_t));
	if (NULL == hls_player)
	{
		debug_error ("Failed to created ahs_player handle...\n");
		return NULL;
	}

	return (void *)hls_player;
}

gboolean __mm_player_hls_destroy (void *hls_handle)
{
	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;

	if (hls_player->uri)
	{
		g_free (hls_player->uri);
		hls_player->uri = NULL;
	}
	
	if (hls_player->client)
	{
		gst_m3u8_client_free (hls_player->client);
		hls_player->client = NULL;
	}

	free (hls_player);
	
	return TRUE;
	
}

gboolean __mm_player_hls_initialize (void *hls_handle, gchar *uri)
{
	mm_player_hls_t *hls_player = (mm_player_hls_t *) hls_handle;

	hls_player->uri = g_strdup (uri);

	hls_player->client = gst_m3u8_client_new (hls_player->uri);
	if (NULL == hls_player->client)
	{
		debug_error ("Failed to create HLS client\n");
		return FALSE;
	}
	
	hls_player->remained = NULL;
	hls_player->allow_cache_fd = NULL;
	
	return TRUE;
}


