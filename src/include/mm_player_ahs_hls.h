/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, YeJin Cho <cho.yejin@samsung.com>,
 * YoungHwan An <younghwan_.an@samsung.com>, naveen cherukuri <naveen.ch@samsung.com>
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

#ifndef __MM_PLAYER_AHS_HLS_H__
#define	__MM_PLAYER_AHS_HLS_H__

#include "mm_player_m3u8.h"
#include <glib.h>
#include <gst/gst.h>
#include "mm_debug.h"
#include <string.h>
#include <openssl/evp.h>
#include <openssl/aes.h>

typedef struct
{
	gchar *uri;
	EVP_CIPHER_CTX decrypt;
	GstM3U8Client *client;
	GstBuffer *remained; /* remained data in aes decryption */
	gboolean discontinuity; /* discontinuity flag */
	GList *list_to_switch;
	FILE *allow_cache_fd;
}mm_player_hls_t;
#define DEFAULT_FRAGMENTS_CACHE 2


void * __mm_player_hls_create ();
gboolean __mm_player_hls_destroy (void *hls_handle);
gboolean __mm_player_hls_initialize (void *hls_handle, gchar *uri);
gboolean hls_decryption_initialize (void *hls_handle, gchar *key_data, unsigned char *iv);
gboolean hls_decrypt_media_fragment (void *hls_handle,GstBuffer *InBuf, GstBuffer **OutBuf);
gboolean hls_get_next_media_fragment (void *hls_handle, gchar **media_uri, gchar **key_uri, char **iv);
gboolean hls_switch_playlist (void *hls_handle, guint download_rate, gboolean *need_bw_switch);
gboolean hls_playlist_update_interval (void *hls_handle, GTimeVal *next_update);
gboolean hls_has_variant_playlist (void *hls_handle);
gchar  *hls_get_current_playlist (mm_player_hls_t *hls_player);
gboolean hls_set_current_playlist (mm_player_hls_t *hls_player);
gboolean hls_parse_playlist_update_client (void *hls_handle, char* playlist);
gboolean hls_client_is_live (void *hls_handle);
gboolean hls_determining_next_file_load (void *hls_handle, gboolean *is_ready);
gboolean hls_is_buffer_discontinuous (void *hls_handle);
gboolean hls_downloaded_variant_playlist (void *hls_handle, gchar *cur_mf_uri);
gboolean hls_clear_discontinuous (void *hls_handle);
gboolean hls_check_allow_cache (void *hls_handle);
gboolean hls_store_media_presentation (void *hls_handle, unsigned char *buffer, unsigned int buffer_len);
#endif

