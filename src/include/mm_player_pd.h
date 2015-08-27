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

#ifndef __MM_PLAYER_PD_H__
#define	__MM_PLAYER_PD_H__

#include <glib.h>
#include <gst/gst.h>
#include <string.h>
#include <mm_types.h>
#include <mm_message.h>

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct
{
	gchar *path_read_from;		// path for download and playback
	gchar *location_to_save; 		// path for saving to local
	gint64 total_size;				// size of file to download (bytes)

	GstElement *playback_pipeline_src;  // src element of playback pipeline
	GstElement *downloader_pipeline;
	GstElement *downloader_src;
	GstElement *downloader_queue;
	GstElement *downloader_sink;
}mm_player_pd_t;

/**
 * This function allocates handle of progressive download.
 *
 * @return	This function returns allocated handle.
 * @remarks
 * @see		_mmplayer_destroy_pd_downloader()
 *
 */
mm_player_pd_t * _mmplayer_create_pd_downloader ();
/**
 * This function destroy progressive download.
 *
 * @param[in]	handle	Handle of player.
 * @return	This function returns true on success, or false on failure.
 * @remarks
 * @see		_mmplayer_create_pd_downloader()
 *
 */
gboolean _mmplayer_destroy_pd_downloader  (MMHandleType handle);
/**
 * This function realize progressive download.
 *
 * @param[in]	handle	Handle of player.
 * @param[in]	src_uri	path to download.
 * @param[in]	dst_uri	path to save in local system.
 * @param[in]	pushsrc	source element of playback pipeline
 * @return	This function returns true on success, or false on failure.
 * @remarks
 * @see
 *
 */
gboolean _mmplayer_realize_pd_downloader (MMHandleType handle, gchar *src_uri, gchar *dst_uri, GstElement *pushsrc);
/**
 * This function unrealize progressive download.
 *
 * @param[in]	handle	Handle of player.
 * @return	This function returns true on success, or false on failure.
 * @remarks
 * @see		_mmplayer_realize_pd_downloader()
 *
 */
gboolean _mmplayer_unrealize_pd_downloader (MMHandleType handle);
/**
 * This function start progressive download.
 *
 * @param[in]	handle	Handle of player.
 * @return	This function returns true on success, or false on failure.
 * @remarks
 * @see
 *
 */
gboolean _mmplayer_start_pd_downloader (MMHandleType handle);
/**
 * This function get pd current status.
 *
 * @param[in]	handle	Handle of player.
 * @param[in]	current_pos	current downloaded size
 * @param[in]	total_size		total file size to download
 * @return	This function returns true on success, or false on failure.
 * @remarks
 * @see
 *
 */
gboolean _mmplayer_get_pd_downloader_status(MMHandleType handle, guint64 *current_pos, guint64 *total_size);
/**
 * This function set message callback of PD downloader.
 *
 * @param[in]	handle	Handle of player.
 * @param[in]	MMMessageCallback	Message callback function
 * @param[in]	user_param		User parameter which is passed to callback function.
 * @return	This function returns true on success, or false on failure.
 * @remarks
 * @see
 *
 */
gint _mm_player_set_pd_downloader_message_cb(MMHandleType player, MMMessageCallback callback, gpointer user_param);

#ifdef __cplusplus
	}
#endif

#endif
