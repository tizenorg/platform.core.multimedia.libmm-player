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

#ifndef __MM_PLAYER_LOCAL_FUNCTION_DEF_H_
#define __MM_PLAYER_LOCAL_FUNCTION_DEF_H_


/*---------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:												|
---------------------------------------------------------------------------*/
/* mm_player_priv.c */
gboolean	__mmplayer_set_state(mm_player_t* player, int state);
void		__mmplayer_typefind_have_type(  GstElement *tf, guint probability, GstCaps *caps, gpointer data);
gboolean	__mmplayer_try_to_plug(mm_player_t* player, GstPad *pad, const GstCaps *caps);
void		__mmplayer_pipeline_complete(GstElement *decodebin,  gpointer data);
gboolean	__mmplayer_update_subtitle( GstElement* object, GstBuffer *buffer, GstPad *pad, gpointer data);
void		__mmplayer_release_misc(mm_player_t* player);
gboolean	__mmplayer_configure_audio_callback(mm_player_t* player);
void		__mmplayer_set_antishock( mm_player_t* player, gboolean disable_by_force);
gboolean	_mmplayer_update_content_attrs(mm_player_t* player, enum content_attr_flag flag);
void		__mmplayer_videostream_cb(GstElement *element, void *stream, int width, int height, gpointer data);
void		__mmplayer_videoframe_render_error_cb(GstElement *element, void *error_id, gpointer data);
void		__mmplayer_handle_buffering_message ( mm_player_t* player );
int			__mmplayer_set_pcm_extraction(mm_player_t* player);
gboolean	__mmplayer_can_extract_pcm( mm_player_t* player );
void		__mmplayer_do_sound_fadedown(mm_player_t* player, unsigned int time);
void		__mmplayer_undo_sound_fadedown(mm_player_t* player);
const gchar *	__get_state_name ( int state );
GstBusSyncReply	__mmplayer_bus_sync_callback (GstBus * bus, GstMessage * message, gpointer data);
void		__mmplayer_post_delayed_eos( mm_player_t* player, int delay_in_ms );
void		__gst_set_async_state_change(mm_player_t* player, gboolean async);
gboolean	__mmplayer_post_message(mm_player_t* player, enum MMMessageType msgtype, MMMessageParamType* param);

/* mm_player_priv_wrapper.c */
gboolean	__mmplayer_gst_callback(GstBus *bus, GstMessage *msg, gpointer data);
gboolean	__mmplayer_gst_handle_duration(mm_player_t* player, GstMessage* msg);
gboolean	__mmplayer_gst_extract_tag_from_msg(mm_player_t* player, GstMessage* msg);
void		__mmplayer_gst_rtp_no_more_pads (GstElement *element,  gpointer data);
gboolean	__mmplayer_gst_remove_fakesink(mm_player_t* player, MMPlayerGstElement* fakesink);
void		__mmplayer_gst_rtp_dynamic_pad (GstElement *element, GstPad *pad, gpointer data);
void		__mmplayer_gst_decode_callback(GstElement *decodebin, GstPad *pad, gboolean last, gpointer data);
int			__mmplayer_gst_element_link_bucket(GList* element_bucket);
int			__mmplayer_gst_element_add_bucket_to_bin(GstBin* bin, GList* element_bucket);
int			__mmplayer_gst_create_audio_pipeline(mm_player_t* player);
int			__mmplayer_gst_create_video_pipeline(mm_player_t* player, GstCaps* caps, MMDisplaySurfaceType surface_type);
int			__mmplayer_gst_create_text_pipeline(mm_player_t* player);
int			__mmplayer_gst_create_subtitle_src(mm_player_t* player);
int			__mmplayer_gst_create_pipeline(mm_player_t* player);
int			__mmplayer_gst_destroy_pipeline(mm_player_t* player);

/* mm_player_priv_gst.c */
int		__gst_realize(mm_player_t* player);
int		__gst_unrealize(mm_player_t* player);
int		__gst_pending_seek(mm_player_t* player);
int		__gst_start(mm_player_t* player);
int		__gst_stop(mm_player_t* player);
int		__gst_pause(mm_player_t* player, gboolean async);
int		__gst_resume(mm_player_t* player, gboolean async);
int		__gst_set_position(mm_player_t* player, int format, unsigned long position, gboolean internal_called);
int		__gst_get_position(mm_player_t* player, int format, unsigned long* position);
int		__gst_get_buffer_position(mm_player_t* player, int format, unsigned long* start_pos, unsigned long* stop_pos);
int		__gst_set_message_callback(mm_player_t* player, MMMessageCallback callback, gpointer user_param);
gboolean	__gst_send_event_to_sink( mm_player_t* player, GstEvent* event );
gboolean	__gst_seek(mm_player_t* player, GstElement * element, gdouble rate,
						GstFormat format, GstSeekFlags flags, GstSeekType cur_type,
						gint64 cur, GstSeekType stop_type, gint64 stop);
int		__gst_adjust_subtitle_position(mm_player_t* player, int format, int position);
void	__gst_appsrc_feed_data_mem(GstElement *element, guint size, gpointer user_data);
gboolean	__gst_appsrc_seek_data_mem(GstElement *element, guint64 size, gpointer user_data);
void	__gst_appsrc_feed_data(GstElement *element, guint size, gpointer user_data);
gboolean	__gst_appsrc_seek_data(GstElement *element, guint64 offset, gpointer user_data);
gboolean	__gst_appsrc_enough_data(GstElement *element, gpointer user_data);

#endif /*#ifndef __MM_PLAYER_LOCAL_FUNCTION_DEF_H_*/

