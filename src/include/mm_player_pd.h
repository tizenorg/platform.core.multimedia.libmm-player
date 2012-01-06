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

#ifndef __MM_PLAYER_PD_H__
#define	__MM_PLAYER_PD_H__

#include <glib.h>
#include <gst/gst.h>
#include <string.h>
#include "mm_debug.h"

typedef struct
{
	gchar *download_uri;
	gchar *pd_file_dmp_location;

	GstElement *pushsrc; /* src element of playback pipeline */

	GstElement *download_pipe;
	GstElement *download_src;
	GstElement *download_queue;
	GstElement *download_sink;

}mm_player_pd_t;


mm_player_pd_t * __mm_player_pd_create ();
gboolean __mm_player_pd_destroy (mm_player_pd_t *pd_downloader);
gboolean __mm_player_pd_initialize (mm_player_pd_t *pd_downloader, gchar *src_uri, gchar *dst_uri, GstElement *pushsrc);
gboolean __mm_player_pd_deinitialize (mm_player_pd_t *pd_downloader);
gboolean __mm_player_pd_start (mm_player_pd_t *pd_downloader);
gboolean __mm_player_pd_stop (mm_player_pd_t *pd_downloader);

#endif
