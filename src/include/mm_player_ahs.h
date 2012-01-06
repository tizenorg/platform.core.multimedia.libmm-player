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

#ifndef __MM_PLAYER_AHS_H__
#define	__MM_PLAYER_AHS_H__

#include <glib.h>
#include "mm_player_ahs_hls.h"
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <string.h>
#include "mm_debug.h"

#define HLS_POSIX_PATH_MAX				256
#define HLS_MANIFEST_DEFAULT_PATH 		"/opt/media/"
#define HLS_MANIFEST_DEFAULT_FILE_NAME	HLS_MANIFEST_DEFAULT_PATH"XXXXXX"

typedef struct
{
	void *ahs_client;
	GstElement *appsrc; /* appsrc to push data */

	int uri_type;
	gboolean need_bw_switch;
	
	/* manifest file (mf) uris */
	gchar *main_mf_uri;
	
	gchar *cur_mf_uri;
	gchar *cur_media_uri;
	gchar *cur_key_uri;
	gchar cur_key_data[16];
	char cur_iv[16];
	
	char ahs_manifest_dmp_location[HLS_POSIX_PATH_MAX];  /* manifest file dumped location */
	char ahs_key_dmp_location [HLS_POSIX_PATH_MAX];
	
	int ahs_state;
	GMutex* state_lock;

	/* manifest/playlist download */
	GThread *manifest_thread;
	gboolean manifest_thread_exit;
	GstElement *manifest_download_pipeline;
	GstElement *manifest_download_src;
	GstElement *manifest_download_sink;
	GCond* manifest_start_cond;
	GCond* manifest_update_cond;
	GCond* manifest_eos_cond;
	GCond* manifest_exit_cond;
	GMutex* manifest_mutex;

	/* media download */
	GThread *media_thread;
	GMutex *media_mutex;
	gboolean media_thread_exit;
	GstElement *media_download_pipeline;
	GstElement *media_download_src;
	GstElement *media_download_sink;
	GCond *media_eos_cond;
	GCond *media_start_cond;

	/* key download */
	GstElement *key_download_pipeline;
	GstElement *key_download_src;
	GstElement *key_download_sink;
	GCond *key_eos_cond;

	guint64 seg_start_time;     /* segment start time in usec*/
	guint64 seg_end_time;     /* segment end time usec*/
	guint download_rate;
	guint64 seg_size;
	gint cache_frag_count;

	gboolean hls_is_wait_for_reload;

	GCond* tmp_cond;

	gboolean is_initialized;
}mm_player_ahs_t;



mm_player_ahs_t * __mm_player_ahs_create ();
gboolean __mm_player_ahs_initialize (mm_player_ahs_t *ahs_player, int uri_type, char *uri, GstElement *appsrc);
gboolean __mm_player_ahs_start (mm_player_ahs_t *ahs_player);
gboolean  __mm_player_ahs_deinitialize (mm_player_ahs_t *ahs_player);
gboolean __mm_player_ahs_stop (mm_player_ahs_t *ahs_player);
gboolean __mm_player_ahs_destroy (mm_player_ahs_t *ahs_player);
gboolean
ahs_store_media_presentation (mm_player_ahs_t *ahs_player, unsigned char *buffer, unsigned int buffer_len);
gboolean ahs_check_allow_cache (mm_player_ahs_t *ahs_player);
#endif

