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

#ifndef __MM_PLAYER_PRIV_INTERNAL_H__
#define	__MM_PLAYER_PRIV_INTERNAL_H__

#define MM_PLAYER_FADEOUT_TIME_DEFAULT	700000 // 700 msec


/*---------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:												|
---------------------------------------------------------------------------*/
void		__mmplayer_release_signal_connection(mm_player_t* player);
gboolean	__mmplayer_dump_pipeline_state(mm_player_t* player);
int			__mmplayer_gst_set_state (mm_player_t* player, GstElement * element,  GstState state, gboolean async, gint timeout);
void		__mmplayer_cancel_delayed_eos(mm_player_t* player);
gboolean	__mmplayer_check_subtitle(mm_player_t* player);
int			__mmplayer_handle_missed_plugin(mm_player_t* player);
gboolean	__mmplayer_link_decoder(mm_player_t* player, GstPad *srcpad);
gboolean	__mmplayer_link_sink(mm_player_t* player , GstPad *srcpad);
gint		__gst_handle_core_error(mm_player_t* player, int code);
gint		__gst_handle_library_error(mm_player_t* player, int code);
gint		__gst_handle_resource_error(mm_player_t* player, int code);
gint		__gst_handle_stream_error(mm_player_t* player, GError* error, GstMessage * message);
gint		__gst_transform_gsterror( mm_player_t* player, GstMessage * message, GError* error );
gboolean	__mmplayer_handle_gst_error ( mm_player_t* player, GstMessage * message, GError* error );
gboolean	__mmplayer_handle_streaming_error  ( mm_player_t* player, GstMessage * message );
void		__mmplayer_add_sink( mm_player_t* player, GstElement* sink );
void		__mmplayer_del_sink( mm_player_t* player, GstElement* sink );
gboolean	__is_rtsp_streaming ( mm_player_t* player );
gboolean	__is_http_streaming ( mm_player_t* player );
gboolean	__is_streaming ( mm_player_t* player );
gboolean	__is_live_streaming ( mm_player_t* player );
gboolean	__is_http_live_streaming( mm_player_t* player );
gboolean	__is_http_progressive_down(mm_player_t* player);

#endif	/* __MM_PLAYER_PRIV_INTERNAL_H__ */

