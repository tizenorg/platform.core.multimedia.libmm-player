/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, YeJin Cho <cho.yejin@samsung.com>,
 * Seungbae Shin <seungbae.shin@samsung.com>, YoungHwan An <younghwan_.an@samsung.com>
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

/*===========================================================================================
|																							|
|  INCLUDE FILES																			|
|																							|
========================================================================================== */
#include <glib.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/videooverlay.h>
#ifdef HAVE_WAYLAND
#include <gst/wayland/wayland.h>
#endif
#include <gst/audio/gstaudiobasesink.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>
#include <dlog.h>

#include <mm_error.h>
#include <mm_attrs.h>
#include <mm_attrs_private.h>
#include <mm_sound.h>
#include <mm_sound_focus.h>

#include "mm_player_priv.h"
#include "mm_player_ini.h"
#include "mm_player_attrs.h"
#include "mm_player_capture.h"
#include "mm_player_utils.h"
#include "mm_player_tracks.h"

/*===========================================================================================
|																							|
|  LOCAL DEFINITIONS AND DECLARATIONS FOR MODULE											|
|																							|
========================================================================================== */

/*---------------------------------------------------------------------------
|    GLOBAL CONSTANT DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    IMPORTED VARIABLE DECLARATIONS:										|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    IMPORTED FUNCTION DECLARATIONS:										|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL #defines:														|
---------------------------------------------------------------------------*/
#define TRICK_PLAY_MUTE_THRESHOLD_MAX	2.0
#define TRICK_PLAY_MUTE_THRESHOLD_MIN	0.0

#define MM_VOLUME_FACTOR_DEFAULT		1.0
#define MM_VOLUME_FACTOR_MIN			0
#define MM_VOLUME_FACTOR_MAX			1.0

#define MM_PLAYER_FADEOUT_TIME_DEFAULT	700000 // 700 msec

#define MM_PLAYER_MPEG_VNAME			"mpegversion"
#define MM_PLAYER_DIVX_VNAME			"divxversion"
#define MM_PLAYER_WMV_VNAME				"wmvversion"
#define MM_PLAYER_WMA_VNAME				"wmaversion"

#define DEFAULT_PLAYBACK_RATE			1.0
#define PLAYBACK_RATE_EX_AUDIO_MIN		0.5
#define PLAYBACK_RATE_EX_AUDIO_MAX		2.0
#define PLAYBACK_RATE_EX_VIDEO_MIN		0.5
#define PLAYBACK_RATE_EX_VIDEO_MAX		1.5

#define GST_QUEUE_DEFAULT_TIME			4
#define GST_QUEUE_HLS_TIME				8

#define MMPLAYER_USE_FILE_FOR_BUFFERING(player) (((player)->profile.uri_type != MM_PLAYER_URI_TYPE_HLS) && (player->ini.http_file_buffer_path) && (strlen(player->ini.http_file_buffer_path) > 0) )
#define MM_PLAYER_NAME	"mmplayer"

/*---------------------------------------------------------------------------
|    LOCAL CONSTANT DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL DATA TYPE DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:												|
---------------------------------------------------------------------------*/
static int		__mmplayer_gst_create_video_pipeline(mm_player_t* player, GstCaps *caps, MMDisplaySurfaceType surface_type);
static int		__mmplayer_gst_create_audio_pipeline(mm_player_t* player);
static int		__mmplayer_gst_create_text_pipeline(mm_player_t* player);
static int		__mmplayer_gst_create_subtitle_src(mm_player_t* player);
static int		__mmplayer_gst_create_pipeline(mm_player_t* player);
static int		__mmplayer_gst_destroy_pipeline(mm_player_t* player);
static int		__mmplayer_gst_element_link_bucket(GList* element_bucket);

static GstPadProbeReturn	__mmplayer_gst_selector_blocked(GstPad* pad, GstPadProbeInfo *info, gpointer data);
static void		__mmplayer_gst_decode_pad_added(GstElement* elem, GstPad* pad, gpointer data);
static void		__mmplayer_gst_decode_no_more_pads(GstElement* elem, gpointer data);
static void		__mmplayer_gst_decode_callback(GstElement *decodebin, GstPad *pad, gpointer data);
static void		__mmplayer_gst_decode_unknown_type(GstElement *elem,  GstPad* pad, GstCaps *caps, gpointer data);
static gboolean __mmplayer_gst_decode_autoplug_continue(GstElement *bin,  GstPad* pad, GstCaps * caps,  gpointer data);
static gint		__mmplayer_gst_decode_autoplug_select(GstElement *bin,  GstPad* pad, GstCaps * caps, GstElementFactory* factory, gpointer data);
//static GValueArray* __mmplayer_gst_decode_autoplug_factories(GstElement *bin,  GstPad* pad, GstCaps * caps,  gpointer data);
static void __mmplayer_gst_decode_pad_removed(GstElement *elem,  GstPad* new_pad, gpointer data);
static void __mmplayer_gst_decode_drained(GstElement *bin, gpointer data);
static void	__mmplayer_gst_element_added(GstElement* bin, GstElement* element, gpointer data);
static GstElement * __mmplayer_create_decodebin(mm_player_t* player);
static gboolean __mmplayer_try_to_plug_decodebin(mm_player_t* player, GstPad *srcpad, const GstCaps *caps);

static void	__mmplayer_typefind_have_type(  GstElement *tf, guint probability, GstCaps *caps, gpointer data);
static gboolean __mmplayer_try_to_plug(mm_player_t* player, GstPad *pad, const GstCaps *caps);
static void	__mmplayer_pipeline_complete(GstElement *decodebin,  gpointer data);
static gboolean __mmplayer_is_midi_type(gchar* str_caps);
static gboolean __mmplayer_is_only_mp3_type (gchar *str_caps);
static void	__mmplayer_set_audio_attrs(mm_player_t* player, GstCaps* caps);
//static void	__mmplayer_check_video_zero_cpoy(mm_player_t* player, GstElementFactory* factory);

static gboolean	__mmplayer_close_link(mm_player_t* player, GstPad *srcpad, GstElement *sinkelement, const char *padname, const GList *templlist);
static gboolean __mmplayer_feature_filter(GstPluginFeature *feature, gpointer data);
static void		__mmplayer_add_new_pad(GstElement *element, GstPad *pad, gpointer data);

static void		__mmplayer_gst_rtp_no_more_pads (GstElement *element,  gpointer data);
//static void    __mmplayer_gst_wfd_dynamic_pad (GstElement *element, GstPad *pad, gpointer data);
static void		__mmplayer_gst_rtp_dynamic_pad (GstElement *element, GstPad *pad, gpointer data);
static gboolean	__mmplayer_get_stream_service_type( mm_player_t* player );
static gboolean	__mmplayer_update_subtitle( GstElement* object, GstBuffer *buffer, GstPad *pad, gpointer data);


static void		__mmplayer_init_factories(mm_player_t* player);
static void		__mmplayer_release_factories(mm_player_t* player);
static void		__mmplayer_release_misc(mm_player_t* player);
static void		__mmplayer_release_misc_post(mm_player_t* player);
static gboolean	__mmplayer_init_gstreamer(mm_player_t* player);
static GstBusSyncReply __mmplayer_bus_sync_callback (GstBus * bus, GstMessage * message, gpointer data);
static gboolean __mmplayer_gst_callback(GstBus *bus, GstMessage *msg, gpointer data);

static gboolean	__mmplayer_gst_extract_tag_from_msg(mm_player_t* player, GstMessage *msg);
static gboolean      __mmplayer_gst_handle_duration(mm_player_t* player, GstMessage* msg);

int		__mmplayer_switch_audio_sink (mm_player_t* player);
static gboolean __mmplayer_gst_remove_fakesink(mm_player_t* player, MMPlayerGstElement* fakesink);
static GstPadProbeReturn __mmplayer_audio_stream_probe (GstPad *pad, GstPadProbeInfo *info, gpointer u_data);
static GstPadProbeReturn __mmplayer_video_stream_probe (GstPad *pad, GstPadProbeInfo *info, gpointer u_data);
static GstPadProbeReturn __mmplayer_subtitle_adjust_position_probe (GstPad *pad, GstPadProbeInfo *info, gpointer u_data);
static int __mmplayer_change_selector_pad (mm_player_t* player, MMPlayerTrackType type, int index);

static gboolean __mmplayer_check_subtitle( mm_player_t* player );
static gboolean __mmplayer_handle_streaming_error  ( mm_player_t* player, GstMessage * message );
static void		__mmplayer_handle_eos_delay( mm_player_t* player, int delay_in_ms );
static void		__mmplayer_cancel_eos_timer( mm_player_t* player );
static gboolean	__mmplayer_eos_timer_cb(gpointer u_data);
static gboolean __mmplayer_link_decoder( mm_player_t* player,GstPad *srcpad);
static gboolean __mmplayer_link_sink( mm_player_t* player,GstPad *srcpad);
static int		__mmplayer_handle_missed_plugin(mm_player_t* player);
static int		__mmplayer_check_not_supported_codec(mm_player_t* player, const gchar* factory_class, const gchar* mime);
static gboolean __mmplayer_configure_audio_callback(mm_player_t* player);
static void		__mmplayer_add_sink( mm_player_t* player, GstElement* sink);
static void		__mmplayer_del_sink( mm_player_t* player, GstElement* sink);
static void		__mmplayer_release_signal_connection(mm_player_t* player, MMPlayerSignalType type);
static gpointer __mmplayer_next_play_thread(gpointer data);
static gpointer __mmplayer_repeat_thread(gpointer data);
static gboolean _mmplayer_update_content_attrs(mm_player_t* player, enum content_attr_flag flag);


static gboolean __mmplayer_add_dump_buffer_probe(mm_player_t *player, GstElement *element);
static GstPadProbeReturn __mmplayer_dump_buffer_probe_cb(GstPad *pad,  GstPadProbeInfo *info, gpointer u_data);
static void __mmplayer_release_dump_list (GList *dump_list);

static int		__gst_realize(mm_player_t* player);
static int		__gst_unrealize(mm_player_t* player);
static int		__gst_start(mm_player_t* player);
static int		__gst_stop(mm_player_t* player);
static int		__gst_pause(mm_player_t* player, gboolean async);
static int		__gst_resume(mm_player_t* player, gboolean async);
static gboolean	__gst_seek(mm_player_t* player, GstElement * element, gdouble rate,
					GstFormat format, GstSeekFlags flags, GstSeekType cur_type,
					gint64 cur, GstSeekType stop_type, gint64 stop );
static int __gst_pending_seek ( mm_player_t* player );

static int		__gst_set_position(mm_player_t* player, int format, unsigned long position, gboolean internal_called);
static int		__gst_get_position(mm_player_t* player, int format, unsigned long *position);
static int		__gst_get_buffer_position(mm_player_t* player, int format, unsigned long* start_pos, unsigned long* stop_pos);
static int		__gst_adjust_subtitle_position(mm_player_t* player, int format, int position);
static int		__gst_set_message_callback(mm_player_t* player, MMMessageCallback callback, gpointer user_param);

static gboolean __gst_send_event_to_sink( mm_player_t* player, GstEvent* event );

static int __mmplayer_set_pcm_extraction(mm_player_t* player);
static gboolean __mmplayer_can_extract_pcm( mm_player_t* player );

/*fadeout */
static void __mmplayer_do_sound_fadedown(mm_player_t* player, unsigned int time);
static void __mmplayer_undo_sound_fadedown(mm_player_t* player);

static void	__mmplayer_add_new_caps(GstPad* pad, GParamSpec* unused, gpointer data);
static void __mmplayer_set_unlinked_mime_type(mm_player_t* player, GstCaps *caps);

/* util */
static gboolean __is_ms_buff_src(mm_player_t* player);
static gboolean __has_suffix(mm_player_t * player, const gchar * suffix);

static int  __mmplayer_realize_streaming_ext(mm_player_t* player);
static int __mmplayer_unrealize_streaming_ext(mm_player_t *player);
static int __mmplayer_start_streaming_ext(mm_player_t *player);
static int __mmplayer_destroy_streaming_ext(mm_player_t* player);
static int __mmplayer_do_change_videosink(mm_player_t* player, const int dec_index, const char *videosink_element, MMDisplaySurfaceType surface_type, void *display_overlay);

static gboolean __mmplayer_verify_next_play_path(mm_player_t *player);
static void __mmplayer_activate_next_source(mm_player_t *player, GstState target);
static void __mmplayer_check_pipeline(mm_player_t* player);
static gboolean __mmplayer_deactivate_selector(mm_player_t *player, MMPlayerTrackType type);
static void __mmplayer_deactivate_old_path(mm_player_t *player);
#if 0 // We'll need this in future.
static int __mmplayer_gst_switching_element(mm_player_t *player, GstElement *search_from, const gchar *removal_name, const gchar *new_element_name);
#endif

static void __mmplayer_update_buffer_setting(mm_player_t *player, GstMessage *buffering_msg);
static GstElement *__mmplayer_element_create_and_link(mm_player_t *player, GstPad* pad, const char* name);

/* device change post proc */
void __mmplayer_device_change_post_process(gpointer user);
void __mmplayer_set_required_cb_score(mm_player_t* player, guint score);
void __mmplayer_inc_cb_score(mm_player_t* player);
void __mmplayer_post_proc_reset(mm_player_t* player);
void __mmplayer_device_change_trigger_post_process(mm_player_t* player);
static int __mmplayer_gst_create_plain_text_elements(mm_player_t* player);
static guint32 _mmplayer_convert_fourcc_string_to_value(const gchar* format_name);
static void		__gst_appsrc_feed_audio_data(GstElement *element, guint size, gpointer user_data);
static void		__gst_appsrc_feed_video_data(GstElement *element, guint size, gpointer user_data);
static void     __gst_appsrc_feed_subtitle_data(GstElement *element, guint size, gpointer user_data);
static void		__gst_appsrc_enough_audio_data(GstElement *element, gpointer user_data);
static void		__gst_appsrc_enough_video_data(GstElement *element, gpointer user_data);
static gboolean	__gst_seek_audio_data (GstElement * appsrc, guint64 position, gpointer user_data);
static gboolean	__gst_seek_video_data (GstElement * appsrc, guint64 position, gpointer user_data);
static gboolean	__gst_seek_subtitle_data (GstElement * appsrc, guint64 position, gpointer user_data);
/*===========================================================================================
|																							|
|  FUNCTION DEFINITIONS																		|
|																							|
========================================================================================== */

#if 0 //debug
static void
print_tag (const GstTagList * list, const gchar * tag, gpointer unused)
{
  gint i, count;

  count = gst_tag_list_get_tag_size (list, tag);

  LOGD("count = %d", count);

  for (i = 0; i < count; i++) {
    gchar *str;

    if (gst_tag_get_type (tag) == G_TYPE_STRING) {
      if (!gst_tag_list_get_string_index (list, tag, i, &str))
        g_assert_not_reached ();
    } else {
      str =
          g_strdup_value_contents (gst_tag_list_get_value_index (list, tag, i));
    }

    if (i == 0) {
      g_print ("  %15s: %s\n", gst_tag_get_nick (tag), str);
    } else {
      g_print ("                 : %s\n", str);
    }

    g_free (str);
  }
}
#endif

static void
__mmplayer_videostream_cb(GstElement *element, void *data,
int width, int height, gpointer user_data) // @
{
	mm_player_t* player = (mm_player_t*)user_data;

	MMPLAYER_RETURN_IF_FAIL ( player );

	MMPLAYER_FENTER();

	if (player->is_drm_file)
	{
		MMMessageParamType msg_param = { 0, };
		LOGW("not supported in drm file");
		msg_param.code = MM_ERROR_PLAYER_DRM_OUTPUT_PROTECTION;
		MMPLAYER_POST_MSG( player, MM_MESSAGE_ERROR, &msg_param );
	}
	else if ( !player->set_mode.media_packet_video_stream && player->video_stream_cb)
	{
		MMPlayerVideoStreamDataType stream;

		/* clear stream data structure */
		memset(&stream, 0x0, sizeof(MMPlayerVideoStreamDataType));

		stream.data[0] = data;
		stream.length_total = width * height * 4; // for rgb 32bit
		stream.height = height;
		stream.width = width;
		player->video_stream_cb(&stream, player->video_stream_cb_user_param);
	}

	MMPLAYER_FLEAVE();
}

static void
__mmplayer_videoframe_render_error_cb(GstElement *element, void *error_id, gpointer data)
{
	mm_player_t* player = (mm_player_t*)data;

	MMPLAYER_RETURN_IF_FAIL ( player );

	MMPLAYER_FENTER();

	if (player->video_frame_render_error_cb )
	{
		if (player->attrs)
		{
			int surface_type = 0;
			mm_attrs_get_int_by_name (player->attrs, "display_surface_type", &surface_type);
			switch (surface_type)
			{
			case MM_DISPLAY_SURFACE_X_EXT:
				player->video_frame_render_error_cb((unsigned int*)error_id, player->video_frame_render_error_cb_user_param);
				LOGD("display surface type(X_EXT) : render error callback(%p) is finished", player->video_frame_render_error_cb);
				break;
			default:
				LOGE("video_frame_render_error_cb was set, but this surface type(%d) is not supported", surface_type);
				break;
			}
		}
		else
		{
			LOGE("could not get surface type");
		}
	}
	else
	{
		LOGW("video_frame_render_error_cb was not set");
	}

	MMPLAYER_FLEAVE();
}

void
__mmplayer_device_change_post_process(gpointer user)
{
	mm_player_t* player = (mm_player_t*)user;
	unsigned long position = 0;
	MMPlayerStateType current_state = MM_PLAYER_STATE_NONE;
	MMPlayerStateType pending_state = MM_PLAYER_STATE_NONE;

	MMPLAYER_FENTER();

	if (! player ||
		! player->pipeline ||
		! player->pipeline->mainbin ||
		! player->pipeline->mainbin[MMPLAYER_M_PIPE].gst )
	{
		goto EXIT;
	}

	current_state = MMPLAYER_CURRENT_STATE(player);
	pending_state = MMPLAYER_PENDING_STATE(player);

	if (player->post_proc.need_pause_and_resume)
	{
		LOGD("pausing");
		if ((pending_state == MM_PLAYER_STATE_PLAYING) ||
			((pending_state == MM_PLAYER_STATE_NONE) && (current_state != MM_PLAYER_STATE_PAUSED)))
			gst_element_set_state(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_PAUSED);
	}

	/* seek should be done within pause and resume */
	if (player->post_proc.need_seek)
	{
		LOGD("seeking");
		__gst_get_position(player, MM_PLAYER_POS_FORMAT_TIME, &position);
		LOGD(">> seek to current position = %ld ms", position);
		__gst_set_position(player, MM_PLAYER_POS_FORMAT_TIME, position, TRUE);
	}

	if (player->post_proc.need_pause_and_resume)
	{
		LOGD("resuming");
		if ((pending_state == MM_PLAYER_STATE_PLAYING) ||
			((pending_state == MM_PLAYER_STATE_NONE) && (current_state != MM_PLAYER_STATE_PAUSED)))
			gst_element_set_state(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_PLAYING);
	}

	/* async */
	if (player->post_proc.need_async)
	{
		LOGD("setting async");

		/* TODO : need some comment here */
		if (player->pipeline->textbin && player->pipeline->textbin[MMPLAYER_T_FAKE_SINK].gst)
			g_object_set (G_OBJECT (player->pipeline->textbin[MMPLAYER_T_FAKE_SINK].gst), "async", TRUE, NULL);
	}

EXIT:
	/* reset all */
	__mmplayer_post_proc_reset(player);
	return;
}

void __mmplayer_set_required_cb_score(mm_player_t* player, guint score)
{
	MMPLAYER_RETURN_IF_FAIL(player);
	player->post_proc.required_cb_score = score;
	LOGD("set required score to : %d", score);
}

void __mmplayer_inc_cb_score(mm_player_t* player)
{
	MMPLAYER_RETURN_IF_FAIL(player);
	player->post_proc.cb_score++;
	LOGD("post proc cb score increased to %d", player->post_proc.cb_score);
}

void __mmplayer_post_proc_reset(mm_player_t* player)
{
	MMPLAYER_RETURN_IF_FAIL(player);

	/* check if already triggered */
	if (player->post_proc.id)
	{
		/* TODO : need to consider multiple main context. !!!! */
		if (FALSE == g_source_remove(player->post_proc.id) )
		{
			LOGE("failed to remove exist post_proc item");
		}
		player->post_proc.id = 0;
	}

	memset(&player->post_proc, 0, sizeof(mm_player_post_proc_t));

	/* set default required cb score 1 as only audio device has changed in this case.
	   if display status is changed with audio device, required cb score is set 2 in display status callback.
	   this logic bases on the assumption which audio device callback is called after calling display status callback. */
	player->post_proc.required_cb_score = 1;
}

void
__mmplayer_device_change_trigger_post_process(mm_player_t* player)
{
	MMPLAYER_RETURN_IF_FAIL(player);

	/* check score */
	if ( player->post_proc.cb_score < player->post_proc.required_cb_score )
	{
		/* wait for next turn */
		LOGD("wait for next turn. required cb score : %d   current score : %d\n",
			player->post_proc.required_cb_score, player->post_proc.cb_score);
		return;
	}

	/* check if already triggered */
	if (player->post_proc.id)
	{
		/* TODO : need to consider multiple main context. !!!! */
		if (FALSE == g_source_remove(player->post_proc.id) )
		{
			LOGE("failed to remove exist post_proc item");
		}
		player->post_proc.id = 0;
	}

	player->post_proc.id = g_idle_add((GSourceFunc)__mmplayer_device_change_post_process, (gpointer)player);
}
#if 0
/* NOTE : Sound module has different latency according to output device So,
 * synchronization problem can be happened whenever device is changed.
 * To avoid this issue, we do reset avsystem or seek as workaroud.
 */
static void
__mmplayer_sound_device_info_changed_cb_func (MMSoundDevice_t device_h, int changed_info_type, void *user_data)
{
    int ret;
    mm_sound_device_type_e device_type;
	mm_player_t* player = (mm_player_t*) user_data;

	MMPLAYER_RETURN_IF_FAIL( player );

	LOGW("device_info_changed_cb is called, device_h[0x%x], changed_info_type[%d]\n", device_h, changed_info_type);

	__mmplayer_inc_cb_score(player);

	/* get device type with device_h*/
	ret = mm_sound_get_device_type(device_h, &device_type);
	if (ret) {
		LOGE("failed to mm_sound_get_device_type()\n");
	}

	/* do pause and resume only if video is playing  */
	if ( player->videodec_linked && MMPLAYER_CURRENT_STATE(player) == MM_PLAYER_STATE_PLAYING )
	{
		switch (device_type)
		{
			case MM_SOUND_DEVICE_TYPE_BLUETOOTH:
			case MM_SOUND_DEVICE_TYPE_AUDIOJACK:
			case MM_SOUND_DEVICE_TYPE_BUILTIN_SPEAKER:
			case MM_SOUND_DEVICE_TYPE_HDMI:
			case MM_SOUND_DEVICE_TYPE_MIRRORING:
			{
				player->post_proc.need_pause_and_resume = TRUE;
			}
			break;

			default:
				LOGD("do nothing");
		}
	}
	LOGW("dispatched");

	__mmplayer_device_change_trigger_post_process(player);
}
#endif
/* This function should be called after the pipeline goes PAUSED or higher
state. */
gboolean
_mmplayer_update_content_attrs(mm_player_t* player, enum content_attr_flag flag) // @
{
	static gboolean has_duration = FALSE;
	static gboolean has_video_attrs = FALSE;
	static gboolean has_audio_attrs = FALSE;
	static gboolean has_bitrate = FALSE;
	gboolean missing_only = FALSE;
	gboolean all = FALSE;
	gint64 dur_nsec = 0;
	GstStructure* p = NULL;
	MMHandleType attrs = 0;
	gchar *path = NULL;
	gint stream_service_type = STREAMING_SERVICE_NONE;
	struct stat sb;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );

	/* check player state here */
	if ( MMPLAYER_CURRENT_STATE(player) != MM_PLAYER_STATE_PAUSED &&
		MMPLAYER_CURRENT_STATE(player) != MM_PLAYER_STATE_PLAYING )
	{
		/* give warning now only */
		LOGW("be careful. content attributes may not available in this state ");
	}

	/* get content attribute first */
	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		LOGE("cannot get content attribute");
		return FALSE;
	}

	/* get update flag */

	if ( flag & ATTR_MISSING_ONLY )
	{
		missing_only = TRUE;
		LOGD("updating missed attr only");
	}

	if ( flag & ATTR_ALL )
	{
		all = TRUE;
		has_duration = FALSE;
		has_video_attrs = FALSE;
		has_audio_attrs = FALSE;
		has_bitrate = FALSE;

		LOGD("updating all attrs");
	}

	if ( missing_only && all )
	{
		LOGW("cannot use ATTR_MISSING_ONLY and ATTR_ALL. ignoring ATTR_MISSING_ONLY flag!");
		missing_only = FALSE;
	}

	if (  (flag & ATTR_DURATION) ||	(!has_duration && missing_only) || all )
	{
		LOGD("try to update duration");
		has_duration = FALSE;

		if (gst_element_query_duration(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_FORMAT_TIME, &dur_nsec ))
		{
			player->duration = dur_nsec;
			LOGW("duration : %lld msec", GST_TIME_AS_MSECONDS(dur_nsec));
		}

		/* try to get streaming service type */
		stream_service_type = __mmplayer_get_stream_service_type( player );
		mm_attrs_set_int_by_name ( attrs, "streaming_type", stream_service_type );

		/* check duration is OK */
		if ( dur_nsec == 0 && !MMPLAYER_IS_LIVE_STREAMING( player ) )
		{
			/* FIXIT : find another way to get duration here. */
			LOGE("finally it's failed to get duration from pipeline. progressbar will not work correctely!");
		}
		else
		{
			/*update duration */
			mm_attrs_set_int_by_name(attrs, "content_duration", GST_TIME_AS_MSECONDS(dur_nsec));
			has_duration = TRUE;
		}
	}

	if (  (flag & ATTR_AUDIO) || (!has_audio_attrs && missing_only) || all )
	{
		/* update audio params
		NOTE : We need original audio params and it can be only obtained from src pad of audio
		decoder. Below code only valid when we are not using 'resampler' just before
		'audioconverter'. */

		LOGD("try to update audio attrs");
		has_audio_attrs = FALSE;

		if ( player->pipeline->audiobin &&
			 player->pipeline->audiobin[MMPLAYER_A_SINK].gst )
		{
			GstCaps *caps_a = NULL;
			GstPad* pad = NULL;
			gint samplerate = 0, channels = 0;

			pad = gst_element_get_static_pad(
					player->pipeline->audiobin[MMPLAYER_A_CONV].gst, "sink" );

			if ( pad )
			{
				caps_a = gst_pad_get_current_caps( pad );

				if ( caps_a )
				{
					p = gst_caps_get_structure (caps_a, 0);

					mm_attrs_get_int_by_name(attrs, "content_audio_samplerate", &samplerate);

					gst_structure_get_int (p, "rate", &samplerate);
					mm_attrs_set_int_by_name(attrs, "content_audio_samplerate", samplerate);

					gst_structure_get_int (p, "channels", &channels);
					mm_attrs_set_int_by_name(attrs, "content_audio_channels", channels);

					SECURE_LOGD("samplerate : %d	channels : %d", samplerate, channels);

					gst_caps_unref( caps_a );
					caps_a = NULL;

					has_audio_attrs = TRUE;
				}
				else
				{
					LOGW("not ready to get audio caps");
				}

				gst_object_unref( pad );
			}
			else
			{
				LOGW("failed to get pad from audiosink");
			}
		}
	}

	if ( (flag & ATTR_VIDEO) || (!has_video_attrs && missing_only) || all )
	{
		LOGD("try to update video attrs");
		has_video_attrs = FALSE;

		if ( player->pipeline->videobin &&
			 player->pipeline->videobin[MMPLAYER_V_SINK].gst )
		{
			GstCaps *caps_v = NULL;
			GstPad* pad = NULL;
			gint tmpNu, tmpDe;
			gint width, height;

			pad = gst_element_get_static_pad( player->pipeline->videobin[MMPLAYER_V_SINK].gst, "sink" );
			if ( pad )
			{
				caps_v = gst_pad_get_current_caps( pad );

				/* Use v_stream_caps, if fail to get video_sink sink pad*/
				if (!caps_v && player->v_stream_caps)
				{
					caps_v = player->v_stream_caps;
					gst_caps_ref(caps_v);
				}

				if (caps_v)
				{
					p = gst_caps_get_structure (caps_v, 0);
					gst_structure_get_int (p, "width", &width);
					mm_attrs_set_int_by_name(attrs, "content_video_width", width);

					gst_structure_get_int (p, "height", &height);
					mm_attrs_set_int_by_name(attrs, "content_video_height", height);

					gst_structure_get_fraction (p, "framerate", &tmpNu, &tmpDe);

					SECURE_LOGD("width : %d     height : %d", width, height );

					gst_caps_unref( caps_v );
					caps_v = NULL;

					if (tmpDe > 0)
					{
						mm_attrs_set_int_by_name(attrs, "content_video_fps", tmpNu / tmpDe);
						SECURE_LOGD("fps : %d", tmpNu / tmpDe);
					}

					has_video_attrs = TRUE;
				}
				else
				{
					LOGD("no negitiated caps from videosink");
				}
				gst_object_unref( pad );
				pad = NULL;
			}
			else
			{
				LOGD("no videosink sink pad");
			}
		}
	}


	if ( (flag & ATTR_BITRATE) || (!has_bitrate && missing_only) || all )
	{
		has_bitrate = FALSE;

		/* FIXIT : please make it clear the dependancy with duration/codec/uritype */
		if (player->duration)
		{
			guint64 data_size = 0;

			if (!MMPLAYER_IS_STREAMING(player) && (player->can_support_codec & FOUND_PLUGIN_VIDEO))
			{
				mm_attrs_get_string_by_name(attrs, "profile_uri", &path);

				if (stat(path, &sb) == 0)
				{
					data_size = (guint64)sb.st_size;
				}
			}
			else if (MMPLAYER_IS_HTTP_STREAMING(player))
			{
				data_size = player->http_content_size;
			}
			LOGD("try to update bitrate : data_size = %lld", data_size);

			if (data_size)
			{
				guint64 bitrate = 0;
				guint64 msec_dur = 0;

				msec_dur = GST_TIME_AS_MSECONDS(player->duration);
				bitrate = data_size * 8 * 1000 / msec_dur;
				SECURE_LOGD("file size : %u, video bitrate = %llu", data_size, bitrate);
				mm_attrs_set_int_by_name(attrs, "content_video_bitrate", bitrate);

				has_bitrate = TRUE;
			}

			if (MMPLAYER_IS_RTSP_STREAMING(player))
			{
				if(player->total_bitrate)
				{
					mm_attrs_set_int_by_name(attrs, "content_video_bitrate", player->total_bitrate);
					has_bitrate = TRUE;
				}
			}
		}
	}

	/* validate all */
	if (  mmf_attrs_commit ( attrs ) )
	{
		LOGE("failed to update attributes\n");
		return FALSE;
	}

	MMPLAYER_FLEAVE();

	return TRUE;
}

static gboolean __mmplayer_get_stream_service_type( mm_player_t* player )
{
	gint streaming_type = STREAMING_SERVICE_NONE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player &&
			player->pipeline &&
			player->pipeline->mainbin &&
			player->pipeline->mainbin[MMPLAYER_M_SRC].gst,
			FALSE );

	/* streaming service type if streaming */
	if ( ! MMPLAYER_IS_STREAMING(player) )
		return STREAMING_SERVICE_NONE;

	if (MMPLAYER_IS_HTTP_STREAMING(player))
	{
		streaming_type = (player->duration == 0) ?
			STREAMING_SERVICE_LIVE : STREAMING_SERVICE_VOD;
	}

	switch ( streaming_type )
	{
		case STREAMING_SERVICE_LIVE:
			LOGD("it's live streaming");
		break;
		case STREAMING_SERVICE_VOD:
			LOGD("it's vod streaming");
		break;
		case STREAMING_SERVICE_NONE:
			LOGE("should not get here");
		break;
		default:
			LOGE("should not get here");
	}

	player->streaming_type = streaming_type;
	MMPLAYER_FLEAVE();

	return streaming_type;
}


/* this function sets the player state and also report
 * it to applicaton by calling callback function
 */
int
__mmplayer_set_state(mm_player_t* player, int state) // @
{
	MMMessageParamType msg = {0, };
	int sound_result = MM_ERROR_NONE;
	gboolean post_bos = FALSE;
	gboolean interrupted_by_focus = FALSE;
	gboolean interrupted_by_resource = FALSE;
	int ret = MM_ERROR_NONE;

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );

	if ( MMPLAYER_CURRENT_STATE(player) == state )
	{
		LOGW("already same state(%s)\n", MMPLAYER_STATE_GET_NAME(state));
		MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_NONE;
		return ret;
	}

	/* update player states */
	MMPLAYER_PREV_STATE(player) = MMPLAYER_CURRENT_STATE(player);
	MMPLAYER_CURRENT_STATE(player) = state;

	/* FIXIT : it's better to do like below code
	if ( MMPLAYER_CURRENT_STATE(player) == MMPLAYER_TARGET_STATE(player) )
			MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_NONE;
	and add more code to handling PENDING_STATE.
	*/
	if ( MMPLAYER_CURRENT_STATE(player) == MMPLAYER_PENDING_STATE(player) )
		MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_NONE;

	/* print state */
	MMPLAYER_PRINT_STATE(player);

	/* do some FSM stuffs before posting new state to application  */
	interrupted_by_focus = player->sound_focus.by_asm_cb;
	interrupted_by_resource = player->resource_manager.by_rm_cb;

	switch ( MMPLAYER_CURRENT_STATE(player) )
	{
		case MM_PLAYER_STATE_NULL:
		case MM_PLAYER_STATE_READY:
		{
			if (player->cmd == MMPLAYER_COMMAND_STOP)
			{
				sound_result = _mmplayer_sound_release_focus(&player->sound_focus);
				if ( sound_result != MM_ERROR_NONE )
				{
					LOGE("failed to release sound focus\n");
					return MM_ERROR_POLICY_INTERNAL;
				}
			}
		}
		break;

		case MM_PLAYER_STATE_PAUSED:
		{
			 if ( ! player->sent_bos )
			 {
				int found = 0;
				#define MMPLAYER_MAX_SOUND_PRIORITY     3

				/* it's first time to update all content attrs. */
				_mmplayer_update_content_attrs( player, ATTR_ALL );
				/* set max sound priority to keep own sound and not to mute other's one */
				mm_attrs_get_int_by_name(player->attrs, "content_video_found", &found);
				if (found)
				{
					mm_attrs_get_int_by_name(player->attrs, "content_audio_found", &found);
					if (found)
					{
						LOGD("set max audio priority");
						g_object_set(player->pipeline->audiobin[MMPLAYER_A_SINK].gst, "priority", MMPLAYER_MAX_SOUND_PRIORITY, NULL);
					}
				}

			 }

			/* add audio callback probe if condition is satisfied */
			if ( ! player->audio_cb_probe_id && player->set_mode.pcm_extraction && !player->audio_stream_render_cb_ex)
			{
				__mmplayer_configure_audio_callback(player);
				/* FIXIT : handle return value */
			}

			if (!MMPLAYER_IS_STREAMING(player) || (player->streamer && !player->streamer->is_buffering))
			{
				sound_result = _mmplayer_sound_release_focus(&player->sound_focus);
				if ( sound_result != MM_ERROR_NONE )
				{
					LOGE("failed to release sound focus\n");
					return MM_ERROR_POLICY_INTERNAL;
				}
			}
		}
		break;

		case MM_PLAYER_STATE_PLAYING:
		{
			/* try to get content metadata */
			if ( ! player->sent_bos )
			{
				/* NOTE : giving ATTR_MISSING_ONLY may have dependency with
				 * c-api since c-api doesn't use _start() anymore. It may not work propery with
				 * legacy mmfw-player api */
				_mmplayer_update_content_attrs( player, ATTR_MISSING_ONLY);
			}

			if ( (player->cmd == MMPLAYER_COMMAND_START) || (player->cmd == MMPLAYER_COMMAND_RESUME) )
			{
				if (!player->sent_bos)
				{
					__mmplayer_handle_missed_plugin ( player );
				}
				sound_result = _mmplayer_sound_acquire_focus(&player->sound_focus);
				if (sound_result != MM_ERROR_NONE)
				{
					// FIXME : need to check history
					if (player->pipeline->videobin)
					{
						MMMessageParamType msg = {0, };

						LOGE("failed to go ahead because of video conflict\n");

						msg.union_type = MM_MSG_UNION_CODE;
						msg.code = MM_ERROR_POLICY_INTERRUPTED;
						MMPLAYER_POST_MSG( player, MM_MESSAGE_STATE_INTERRUPTED, &msg);

						_mmplayer_unrealize((MMHandleType)player);
					}
					else
					{
						LOGE("failed to play by sound focus error : 0x%X\n", sound_result);
						_mmplayer_pause((MMHandleType)player);
						return sound_result;
					}

					return MM_ERROR_POLICY_INTERNAL;
				}
			}

			if ( player->resumed_by_rewind && player->playback_rate < 0.0 )
			{
				/* initialize because auto resume is done well. */
				player->resumed_by_rewind = FALSE;
				player->playback_rate = 1.0;
			}

			if ( !player->sent_bos )
			{
				/* check audio codec field is set or not
				 * we can get it from typefinder or codec's caps.
				 */
				gchar *audio_codec = NULL;
				mm_attrs_get_string_by_name(player->attrs, "content_audio_codec", &audio_codec);

				/* The codec format can't be sent for audio only case like amr, mid etc.
				 * Because, parser don't make related TAG.
				 * So, if it's not set yet, fill it with found data.
				 */
				if ( ! audio_codec )
				{
					if ( g_strrstr(player->type, "audio/midi"))
					{
						audio_codec = g_strdup("MIDI");

					}
					else if ( g_strrstr(player->type, "audio/x-amr"))
					{
						audio_codec = g_strdup("AMR");
					}
					else if ( g_strrstr(player->type, "audio/mpeg") && !g_strrstr(player->type, "mpegversion=(int)1"))
					{
						audio_codec = g_strdup("AAC");
					}
					else
					{
						audio_codec = g_strdup("unknown");
					}
					mm_attrs_set_string_by_name(player->attrs, "content_audio_codec", audio_codec);

					MMPLAYER_FREEIF(audio_codec);
					mmf_attrs_commit(player->attrs);
					LOGD("set audio codec type with caps\n");
				}

				post_bos = TRUE;
			}
		}
		break;

		case MM_PLAYER_STATE_NONE:
		default:
			LOGW("invalid target state, there is nothing to do.\n");
			break;
	}


	/* post message to application */
	if (MMPLAYER_TARGET_STATE(player) == state)
	{
		/* fill the message with state of player */
		msg.state.previous = MMPLAYER_PREV_STATE(player);
		msg.state.current = MMPLAYER_CURRENT_STATE(player);

		LOGD ("player reach the target state (%s)", MMPLAYER_STATE_GET_NAME(MMPLAYER_TARGET_STATE(player)));

		/* state changed by focus or resource callback */
		if ( interrupted_by_focus || interrupted_by_resource )
		{
			msg.union_type = MM_MSG_UNION_CODE;
			if (interrupted_by_focus)
				msg.code = player->sound_focus.focus_changed_msg;	/* FIXME: player.c convert function have to be modified. */
			else if (interrupted_by_resource)
				msg.code = MM_MSG_CODE_INTERRUPTED_BY_RESOURCE_CONFLICT;
			MMPLAYER_POST_MSG( player, MM_MESSAGE_STATE_INTERRUPTED, &msg );
		}
		/* state changed by usecase */
		else
		{
			MMPLAYER_POST_MSG( player, MM_MESSAGE_STATE_CHANGED, &msg );
		}
	}
	else
	{
		LOGD ("intermediate state, do nothing.\n");
		MMPLAYER_PRINT_STATE(player);
		return ret;
	}

	if ( post_bos )
	{
		MMPLAYER_POST_MSG ( player, MM_MESSAGE_BEGIN_OF_STREAM, NULL );
		player->sent_bos = TRUE;
	}

	return ret;
}

static gpointer __mmplayer_next_play_thread(gpointer data)
{
	mm_player_t* player = (mm_player_t*) data;
	MMPlayerGstElement *mainbin = NULL;

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, NULL );

	g_mutex_lock(&player->next_play_thread_mutex);
	while ( ! player->next_play_thread_exit )
	{
		LOGD("next play thread started. waiting for signal.\n");
		g_cond_wait(&player->next_play_thread_cond, &player->next_play_thread_mutex );

		LOGD("reconfigure pipeline for gapless play.\n");

		if ( player->next_play_thread_exit )
		{
			if(player->gapless.reconfigure)
			{
				player->gapless.reconfigure = false;
				MMPLAYER_PLAYBACK_UNLOCK(player);
			}
			LOGD("exiting gapless play thread\n");
			break;
		}

		mainbin = player->pipeline->mainbin;

		MMPLAYER_RELEASE_ELEMENT(player, mainbin, MMPLAYER_M_MUXED_S_BUFFER);
		MMPLAYER_RELEASE_ELEMENT(player, mainbin, MMPLAYER_M_ID3DEMUX);
		MMPLAYER_RELEASE_ELEMENT(player, mainbin, MMPLAYER_M_AUTOPLUG);
		MMPLAYER_RELEASE_ELEMENT(player, mainbin, MMPLAYER_M_TYPEFIND);
		MMPLAYER_RELEASE_ELEMENT(player, mainbin, MMPLAYER_M_SRC);

		__mmplayer_activate_next_source(player, GST_STATE_PLAYING);
	}
	g_mutex_unlock(&player->next_play_thread_mutex);

	return NULL;
}

static gpointer __mmplayer_repeat_thread(gpointer data)
{
	mm_player_t* player = (mm_player_t*) data;
	gboolean ret_value = FALSE;
	MMHandleType attrs = 0;
	gint count = 0;

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, NULL );

	g_mutex_lock(&player->repeat_thread_mutex);
	while ( ! player->repeat_thread_exit )
	{
		LOGD("repeat thread started. waiting for signal.\n");
		g_cond_wait(&player->repeat_thread_cond, &player->repeat_thread_mutex );

		if ( player->repeat_thread_exit )
		{
			LOGD("exiting repeat thread\n");
			break;
		}


		/* lock */
		g_mutex_lock(&player->cmd_lock);

		attrs = MMPLAYER_GET_ATTRS(player);

		if (mm_attrs_get_int_by_name(attrs, "profile_play_count", &count) != MM_ERROR_NONE)
		{
			LOGE("can not get play count\n");
			break;
		}

		if ( player->section_repeat )
		{
			ret_value = _mmplayer_activate_section_repeat((MMHandleType)player, player->section_repeat_start, player->section_repeat_end);
		}
		else
		{
			if ( player->playback_rate < 0.0 )
			{
				player->resumed_by_rewind = TRUE;
				_mmplayer_set_mute((MMHandleType)player, 0);
				MMPLAYER_POST_MSG( player, MM_MESSAGE_RESUMED_BY_REW, NULL );
			}

			ret_value = __gst_seek( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, player->playback_rate,
				GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET,
				0, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);

			/* initialize */
			player->sent_bos = FALSE;
		}

		if ( ! ret_value )
		{
			LOGE("failed to set position to zero for rewind\n");
			continue;
		}

		/* decrease play count */
		if ( count > 1 )
		{
			/* we successeded to rewind. update play count and then wait for next EOS */
			count--;

			mm_attrs_set_int_by_name(attrs, "profile_play_count", count);

			/* commit attribute */
			if ( mmf_attrs_commit ( attrs ) )
			{
				LOGE("failed to commit attribute\n");
			}
		}

		/* unlock */
		g_mutex_unlock(&player->cmd_lock);
	}

	g_mutex_unlock(&player->repeat_thread_mutex);
	return NULL;
}

static void
__mmplayer_update_buffer_setting(mm_player_t *player, GstMessage *buffering_msg)
{
	MMHandleType attrs = 0;
	guint64 data_size = 0;
	gchar* path = NULL;
	unsigned long pos_msec = 0;
	struct stat sb;

	MMPLAYER_RETURN_IF_FAIL( player && player->pipeline && player->pipeline->mainbin);

	__gst_get_position(player, MM_PLAYER_POS_FORMAT_TIME, &pos_msec);	// update last_position

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		LOGE("fail to get attributes.\n");
		return;
	}

	if (!MMPLAYER_IS_STREAMING(player) && (player->can_support_codec & FOUND_PLUGIN_VIDEO))
	{
		mm_attrs_get_string_by_name(attrs, "profile_uri", &path);

		if (stat(path, &sb) == 0)
		{
			data_size = (guint64)sb.st_size;
		}
	}
	else if (MMPLAYER_IS_HTTP_STREAMING(player))
	{
		data_size = player->http_content_size;
	}

	__mm_player_streaming_buffering(	player->streamer,
										buffering_msg,
										data_size,
										player->last_position,
										player->duration);

	__mm_player_streaming_sync_property(player->streamer, player->pipeline->mainbin[MMPLAYER_M_AUTOPLUG].gst);

	return;
}

static int
__mmplayer_handle_buffering_message ( mm_player_t* player )
{
	int ret = MM_ERROR_NONE;
	MMPlayerStateType prev_state = MM_PLAYER_STATE_NONE;
	MMPlayerStateType current_state = MM_PLAYER_STATE_NONE;
	MMPlayerStateType target_state = MM_PLAYER_STATE_NONE;
	MMPlayerStateType pending_state = MM_PLAYER_STATE_NONE;

	MMPLAYER_CMD_LOCK( player );

	if( !player || !player->streamer || (MMPLAYER_IS_LIVE_STREAMING(player) && MMPLAYER_IS_RTSP_STREAMING(player)))
	{
		LOGW("do nothing for buffering msg\n");
		ret = MM_ERROR_PLAYER_INVALID_STATE;
		goto unlock_exit;
	}

	prev_state = MMPLAYER_PREV_STATE(player);
	current_state = MMPLAYER_CURRENT_STATE(player);
	target_state = MMPLAYER_TARGET_STATE(player);
	pending_state = MMPLAYER_PENDING_STATE(player);

	LOGD( "player state : prev %s, current %s, pending %s, target %s, buffering %d",
		MMPLAYER_STATE_GET_NAME(prev_state),
		MMPLAYER_STATE_GET_NAME(current_state),
		MMPLAYER_STATE_GET_NAME(pending_state),
		MMPLAYER_STATE_GET_NAME(target_state),
		player->streamer->is_buffering);

	if ( !player->streamer->is_buffering )
	{
		/* NOTE : if buffering has done, player has to go to target state. */
		switch ( target_state )
		{
			case MM_PLAYER_STATE_PAUSED :
			{
				switch ( pending_state )
				{
					case MM_PLAYER_STATE_PLAYING:
					{
						__gst_pause ( player, TRUE );
					}
					break;

					case MM_PLAYER_STATE_PAUSED:
					{
						LOGD("player is already going to paused state, there is nothing to do.\n");
					}
					break;

					case MM_PLAYER_STATE_NONE:
					case MM_PLAYER_STATE_NULL:
					case MM_PLAYER_STATE_READY:
					default :
					{
						LOGW("invalid pending state [%s].\n", MMPLAYER_STATE_GET_NAME(pending_state) );
					}
						break;
				}
			}
			break;

			case MM_PLAYER_STATE_PLAYING :
			{
				switch ( pending_state )
				{
					case MM_PLAYER_STATE_NONE:
					{
						if (current_state != MM_PLAYER_STATE_PLAYING)
							__gst_resume ( player, TRUE );
					}
					break;

					case MM_PLAYER_STATE_PAUSED:
					{
						/* NOTE: It should be worked as asynchronously.
						 * Because, buffering can be completed during autoplugging when pipeline would try to go playing state directly.
						 */
						__gst_resume ( player, TRUE );
					}
					break;

					case MM_PLAYER_STATE_PLAYING:
					{
						LOGD("player is already going to playing state, there is nothing to do.\n");
					}
					break;

					case MM_PLAYER_STATE_NULL:
					case MM_PLAYER_STATE_READY:
					default :
					{
						LOGW("invalid pending state [%s].\n", MMPLAYER_STATE_GET_NAME(pending_state) );
					}
						break;
				}
			}
			break;

			case MM_PLAYER_STATE_NULL :
			case MM_PLAYER_STATE_READY :
			case MM_PLAYER_STATE_NONE :
			default:
			{
				LOGW("invalid target state [%s].\n", MMPLAYER_STATE_GET_NAME(target_state) );
			}
				break;
		}
	}
	else
	{
		/* NOTE : during the buffering, pause the player for stopping pipeline clock.
		 *	it's for stopping the pipeline clock to prevent dropping the data in sink element.
		 */
		switch ( pending_state )
		{
			case MM_PLAYER_STATE_NONE:
			{
				if (current_state != MM_PLAYER_STATE_PAUSED)
				{
					LOGD("set pause state during buffering\n");
					__gst_pause ( player, TRUE );

					// to cover the weak-signal environment.
					if (MMPLAYER_IS_RTSP_STREAMING(player))
					{
						unsigned long position = 0;
						gint64 pos_msec = 0;

						LOGD("[RTSP] seek to the buffering start point\n");

						if (__gst_get_position(player, MM_PLAYER_POS_FORMAT_TIME, &position ))
						{
							LOGE("failed to get position\n");
							break;
						}

						/* key unit seek */
						pos_msec = position * G_GINT64_CONSTANT(1000000);

						__gst_seek(player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, 1.0,
									GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET,
									pos_msec, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
					}
				}
			}
			break;

			case MM_PLAYER_STATE_PLAYING:
			{
				__gst_pause ( player, TRUE );
			}
			break;

			case MM_PLAYER_STATE_PAUSED:
			{
			}
			break;

			case MM_PLAYER_STATE_NULL:
			case MM_PLAYER_STATE_READY:
			default :
			{
				LOGW("invalid pending state [%s].\n", MMPLAYER_STATE_GET_NAME(pending_state) );
			}
				break;
		}
	}

unlock_exit:
	MMPLAYER_CMD_UNLOCK( player );
	return ret;
}

static void
__mmplayer_drop_subtitle(mm_player_t* player, gboolean is_drop)
{
	MMPlayerGstElement *textbin;
	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL ( player &&
					player->pipeline &&
					player->pipeline->textbin);

	MMPLAYER_RETURN_IF_FAIL (player->pipeline->textbin[MMPLAYER_T_IDENTITY].gst);

	textbin = player->pipeline->textbin;

	if (is_drop)
	{
		LOGD("Drop subtitle text after getting EOS\n");

		g_object_set(textbin[MMPLAYER_T_FAKE_SINK].gst, "async", FALSE, NULL);
		g_object_set(textbin[MMPLAYER_T_IDENTITY].gst, "drop-probability", (gfloat)1.0, NULL);

		player->is_subtitle_force_drop = TRUE;
	}
	else
	{
		if (player->is_subtitle_force_drop == TRUE)
		{
			LOGD("Enable subtitle data path without drop\n");

			g_object_set(textbin[MMPLAYER_T_IDENTITY].gst, "drop-probability", (gfloat)0.0, NULL);
			g_object_set(textbin[MMPLAYER_T_FAKE_SINK].gst, "async", TRUE, NULL);

			LOGD ("non-connected with external display");

			player->is_subtitle_force_drop = FALSE;
		}
	}
}

static gboolean
__mmplayer_gst_callback(GstBus *bus, GstMessage *msg, gpointer data) // @
{
	mm_player_t* player = (mm_player_t*) data;
	gboolean ret = TRUE;
	static gboolean async_done = FALSE;

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );
	MMPLAYER_RETURN_VAL_IF_FAIL ( msg && GST_IS_MESSAGE(msg), FALSE );

	switch ( GST_MESSAGE_TYPE( msg ) )
	{
		case GST_MESSAGE_UNKNOWN:
			LOGD("unknown message received\n");
		break;

		case GST_MESSAGE_EOS:
		{
			MMHandleType attrs = 0;
			gint count = 0;

			LOGD("GST_MESSAGE_EOS received\n");

			/* NOTE : EOS event is comming multiple time. watch out it */
			/* check state. we only process EOS when pipeline state goes to PLAYING */
			if ( ! (player->cmd == MMPLAYER_COMMAND_START || player->cmd == MMPLAYER_COMMAND_RESUME) )
			{
				LOGD("EOS received on non-playing state. ignoring it\n");
				break;
			}

			__mmplayer_drop_subtitle(player, TRUE);

			if ( (player->audio_stream_cb) && (player->set_mode.pcm_extraction) && (!player->audio_stream_render_cb_ex))
			{
				GstPad *pad = NULL;

				pad = gst_element_get_static_pad (player->pipeline->audiobin[MMPLAYER_A_SINK].gst, "sink");

				LOGD("release audio callback\n");

				/* release audio callback */
				gst_pad_remove_probe (pad, player->audio_cb_probe_id);
				player->audio_cb_probe_id = 0;
				/* audio callback should be free because it can be called even though probe remove.*/
				player->audio_stream_cb = NULL;
				player->audio_stream_cb_user_param = NULL;

			}

			/* rewind if repeat count is greater then zero */
			/* get play count */
			attrs = MMPLAYER_GET_ATTRS(player);

			if ( attrs )
			{
				gboolean smooth_repeat = FALSE;

				mm_attrs_get_int_by_name(attrs, "profile_play_count", &count);
				mm_attrs_get_int_by_name(attrs, "profile_smooth_repeat", &smooth_repeat);

				player->play_count = count;

				LOGD("remaining play count: %d, playback rate: %f\n", count, player->playback_rate);

				if ( count > 1 || count == -1 || player->playback_rate < 0.0 ) /* default value is 1 */
				{
					if ( smooth_repeat )
					{
						LOGD("smooth repeat enabled. seeking operation will be excuted in new thread\n");

						g_cond_signal( &player->repeat_thread_cond );

						break;
					}
					else
					{
						gint ret_value = 0;

						if ( player->section_repeat )
						{
							ret_value = _mmplayer_activate_section_repeat((MMHandleType)player, player->section_repeat_start, player->section_repeat_end);
						}
						else
						{
							if ( player->playback_rate < 0.0 )
							{
								player->resumed_by_rewind = TRUE;
								_mmplayer_set_mute((MMHandleType)player, 0);
								MMPLAYER_POST_MSG( player, MM_MESSAGE_RESUMED_BY_REW, NULL );
							}

							__mmplayer_handle_eos_delay( player, player->ini.delay_before_repeat );

							/* initialize */
							player->sent_bos = FALSE;
						}

						if ( MM_ERROR_NONE != ret_value )
						{
							LOGE("failed to set position to zero for rewind\n");
						}

						/* not posting eos when repeating */
						break;
					}
				}
			}

			MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-eos" );

			/* post eos message to application */
			__mmplayer_handle_eos_delay( player, player->ini.eos_delay );

			/* reset last position */
			player->last_position = 0;
		}
		break;

		case GST_MESSAGE_ERROR:
		{
			GError *error = NULL;
			gchar* debug = NULL;

			/* generating debug info before returning error */
			MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-error" );

			/* get error code */
			gst_message_parse_error( msg, &error, &debug );

			if ( gst_structure_has_name ( gst_message_get_structure(msg), "streaming_error" ) )
			{
				/* Note : the streaming error from the streaming source is handled
				 *   using __mmplayer_handle_streaming_error.
				 */
				__mmplayer_handle_streaming_error ( player, msg );

				/* dump state of all element */
				__mmplayer_dump_pipeline_state( player );
			}
			else
			{
				/* traslate gst error code to msl error code. then post it
				 * to application if needed
				 */
				__mmplayer_handle_gst_error( player, msg, error );

				if (debug)
				{
					LOGE ("error debug : %s", debug);
				}

			}

			if (MMPLAYER_IS_HTTP_PD(player))
			{
				_mmplayer_unrealize_pd_downloader ((MMHandleType)player);
			}

			MMPLAYER_FREEIF( debug );
			g_error_free( error );
		}
		break;

		case GST_MESSAGE_WARNING:
		{
			char* debug = NULL;
			GError* error = NULL;

			gst_message_parse_warning(msg, &error, &debug);

			LOGD("warning : %s\n", error->message);
			LOGD("debug : %s\n", debug);

			MMPLAYER_POST_MSG( player, MM_MESSAGE_WARNING, NULL );

			MMPLAYER_FREEIF( debug );
			g_error_free( error );
		}
		break;

		case GST_MESSAGE_TAG:
		{
			LOGD("GST_MESSAGE_TAG\n");
			if ( ! __mmplayer_gst_extract_tag_from_msg( player, msg ) )
			{
				LOGW("failed to extract tags from gstmessage\n");
			}
		}
		break;

		case GST_MESSAGE_BUFFERING:
		{
			MMMessageParamType msg_param = {0, };

			if (!MMPLAYER_IS_STREAMING(player))
				break;

			/* ignore the prev buffering message */
			if ((player->streamer) && (player->streamer->is_buffering == FALSE) && (player->streamer->is_buffering_done == TRUE))
			{
				gint buffer_percent = 0;

				gst_message_parse_buffering (msg, &buffer_percent);

				if (buffer_percent == MAX_BUFFER_PERCENT)
				{
					LOGD ("Ignored all the previous buffering msg! (got %d%%)\n", buffer_percent);
					player->streamer->is_buffering_done = FALSE;
				}

				break;
			}

			__mmplayer_update_buffer_setting(player, msg);

			if(__mmplayer_handle_buffering_message ( player ) == MM_ERROR_NONE) {

				msg_param.connection.buffering = player->streamer->buffering_percent;
				MMPLAYER_POST_MSG ( player, MM_MESSAGE_BUFFERING, &msg_param );
				if (MMPLAYER_IS_RTSP_STREAMING(player) &&
						(player->streamer->buffering_percent >= MAX_BUFFER_PERCENT))
				{
					if (player->doing_seek)
					{
						if (MMPLAYER_TARGET_STATE(player) == MM_PLAYER_STATE_PAUSED)
						{
							player->doing_seek = FALSE;
							MMPLAYER_POST_MSG ( player, MM_MESSAGE_SEEK_COMPLETED, NULL );
						}
						else if (MMPLAYER_TARGET_STATE(player) == MM_PLAYER_STATE_PLAYING)
						{
							async_done = TRUE;
						}
					}
				}
			}
		}
		break;

		case GST_MESSAGE_STATE_CHANGED:
		{
			MMPlayerGstElement *mainbin;
			const GValue *voldstate, *vnewstate, *vpending;
			GstState oldstate, newstate, pending;

			if ( ! ( player->pipeline && player->pipeline->mainbin ) )
			{
				LOGE("player pipeline handle is null");
				break;
			}

			mainbin = player->pipeline->mainbin;

			/* we only handle messages from pipeline */
			if( msg->src != (GstObject *)mainbin[MMPLAYER_M_PIPE].gst )
				break;

			/* get state info from msg */
			voldstate = gst_structure_get_value (gst_message_get_structure(msg), "old-state");
			vnewstate = gst_structure_get_value (gst_message_get_structure(msg), "new-state");
			vpending = gst_structure_get_value (gst_message_get_structure(msg), "pending-state");

			oldstate = (GstState)voldstate->data[0].v_int;
			newstate = (GstState)vnewstate->data[0].v_int;
			pending = (GstState)vpending->data[0].v_int;

			LOGD("state changed [%s] : %s ---> %s     final : %s\n",
				GST_OBJECT_NAME(GST_MESSAGE_SRC(msg)),
				gst_element_state_get_name( (GstState)oldstate ),
				gst_element_state_get_name( (GstState)newstate ),
				gst_element_state_get_name( (GstState)pending ) );

			if (oldstate == newstate)
			{
				LOGD("pipeline reports state transition to old state");
				break;
			}

			switch(newstate)
			{
				case GST_STATE_VOID_PENDING:
				break;

				case GST_STATE_NULL:
				break;

				case GST_STATE_READY:
				break;

				case GST_STATE_PAUSED:
				{
					gboolean prepare_async = FALSE;
					gboolean is_drm = FALSE;

					if ( ! player->audio_cb_probe_id && player->set_mode.pcm_extraction && !player->audio_stream_render_cb_ex)
						__mmplayer_configure_audio_callback(player);

					if ( ! player->sent_bos && oldstate == GST_STATE_READY) // managed prepare async case
					{
						mm_attrs_get_int_by_name(player->attrs, "profile_prepare_async", &prepare_async);
						LOGD("checking prepare mode for async transition - %d", prepare_async);
					}

					if ( MMPLAYER_IS_STREAMING(player) || prepare_async )
					{
						MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_PAUSED );

						if (MMPLAYER_IS_STREAMING(player) && (player->streamer))
						{
							__mm_player_streaming_set_content_bitrate(player->streamer,
								player->total_maximum_bitrate, player->total_bitrate);
						}
					}

					/* NOTE : should consider streaming case */
					/* check if drm file */
					if ((player->pipeline->mainbin[MMPLAYER_M_SRC].gst) &&
						(g_object_class_find_property(G_OBJECT_GET_CLASS(player->pipeline->mainbin[MMPLAYER_M_SRC].gst), "is-drm")))
					{
						g_object_get(G_OBJECT(player->pipeline->mainbin[MMPLAYER_M_SRC].gst), "is-drm", &is_drm, NULL);

						if (is_drm)
						{
							player->is_drm_file = TRUE;
						}
					}
				}
				break;

				case GST_STATE_PLAYING:
				{
/* for audio tunning */
#ifndef IS_SDK
					if (player->can_support_codec == 0x03) {
						gint volume_type;
						mm_attrs_get_int_by_name(player->attrs, "sound_volume_type", &volume_type);
						volume_type |= MM_SOUND_VOLUME_GAIN_VIDEO;
						g_object_set(player->pipeline->audiobin[MMPLAYER_A_SINK].gst, "volumetype", volume_type, NULL);
					}
#endif
					if ( MMPLAYER_IS_STREAMING(player) ) // managed prepare async case when buffering is completed
					{
						// pending state should be reset oyherwise, it's still playing even though it's resumed after bufferging.
						if ((MMPLAYER_CURRENT_STATE(player) != MM_PLAYER_STATE_PLAYING) ||
							(MMPLAYER_PENDING_STATE(player) == MM_PLAYER_STATE_PLAYING))
						{
							MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_PLAYING);
						}
					}

					if (player->gapless.stream_changed)
					{
						_mmplayer_update_content_attrs(player, ATTR_ALL);
					}

					if (player->doing_seek && async_done)
					{
						player->doing_seek = FALSE;
						async_done = FALSE;
						MMPLAYER_POST_MSG ( player, MM_MESSAGE_SEEK_COMPLETED, NULL );
					}
				}
				break;

				default:
				break;
			}
		}
		break;

		case GST_MESSAGE_CLOCK_LOST:
			{
				GstClock *clock = NULL;
				gboolean need_new_clock = FALSE;

				gst_message_parse_clock_lost (msg, &clock);
				LOGD("GST_MESSAGE_CLOCK_LOST : %s\n", (clock ? GST_OBJECT_NAME (clock) : "NULL"));

				if (!player->videodec_linked)
				{
					need_new_clock = TRUE;
				}
				else if (!player->ini.use_system_clock)
				{
					need_new_clock = TRUE;
				}

				if (need_new_clock) {
					LOGD ("Provide clock is TRUE, do pause->resume\n");
					__gst_pause(player, FALSE);
					__gst_resume(player, FALSE);
				}
			}
			break;

		case GST_MESSAGE_NEW_CLOCK:
			{
				GstClock *clock = NULL;
				gst_message_parse_new_clock (msg, &clock);
				LOGD("GST_MESSAGE_NEW_CLOCK : %s\n", (clock ? GST_OBJECT_NAME (clock) : "NULL"));
			}
			break;

		case GST_MESSAGE_ELEMENT:
			{
				const gchar *structure_name;
				gint count = 0;
				MMHandleType attrs = 0;

				attrs = MMPLAYER_GET_ATTRS(player);
				if ( !attrs )
				{
					LOGE("cannot get content attribute");
					ret = FALSE;
					break;
				}

				if(gst_message_get_structure(msg) == NULL)
					break;

				structure_name = gst_structure_get_name(gst_message_get_structure(msg));
				if(!strcmp(structure_name, "Language_list"))
				{
					const GValue *lang_list = NULL;
					lang_list = gst_structure_get_value (gst_message_get_structure(msg), "lang_list");
					if(lang_list != NULL)
					{
						count = g_list_length((GList *)g_value_get_pointer (lang_list));
						if (count > 1)
							LOGD("Total audio tracks (from parser) = %d \n",count);
					}
				}

				if (!strcmp (structure_name, "Ext_Sub_Language_List"))
				{
					const GValue *lang_list = NULL;
					MMPlayerLangStruct *temp = NULL;

					lang_list = gst_structure_get_value (gst_message_get_structure(msg), "lang_list");
					if (lang_list != NULL)
					{
						count = g_list_length ((GList *)g_value_get_pointer (lang_list));
						if (count)
						{
							player->subtitle_language_list = (GList *)g_value_get_pointer (lang_list);
							mm_attrs_set_int_by_name(attrs, "content_text_track_num", (gint)count);
							if (mmf_attrs_commit (attrs))
							  LOGE("failed to commit.\n");
							LOGD("Total subtitle tracks = %d \n", count);
						}
						while (count)
						{
							temp = g_list_nth_data (player->subtitle_language_list, count - 1);
							LOGD ("value of lang_key is %s and lang_code is %s",
										temp->language_key, temp->language_code);
							count--;
						}
					}
				}

				/* custom message */
				if (!strcmp (structure_name, "audio_codec_not_supported")) {
					MMMessageParamType msg_param = {0,};
					msg_param.code = MM_ERROR_PLAYER_AUDIO_CODEC_NOT_FOUND;
					MMPLAYER_POST_MSG(player, MM_MESSAGE_ERROR, &msg_param);
				}
			}
			break;

		case GST_MESSAGE_DURATION_CHANGED:
		{
			LOGD("GST_MESSAGE_DURATION_CHANGED\n");
			ret = __mmplayer_gst_handle_duration(player, msg);
			if (!ret)
			{
				LOGW("failed to update duration");
			}
		}

		break;

		case GST_MESSAGE_ASYNC_START:
		{
			LOGD("GST_MESSAGE_ASYNC_START : %s\n", GST_ELEMENT_NAME(GST_MESSAGE_SRC(msg)));
		}
		break;

		case GST_MESSAGE_ASYNC_DONE:
		{
			LOGD("GST_MESSAGE_ASYNC_DONE : %s\n", GST_ELEMENT_NAME(GST_MESSAGE_SRC(msg)));

			/* we only handle messages from pipeline */
			if( msg->src != (GstObject *)player->pipeline->mainbin[MMPLAYER_M_PIPE].gst )
				break;

			if (player->doing_seek)
			{
				if (MMPLAYER_TARGET_STATE(player) == MM_PLAYER_STATE_PAUSED)
				{
					player->doing_seek = FALSE;
					MMPLAYER_POST_MSG ( player, MM_MESSAGE_SEEK_COMPLETED, NULL );
				}
				else if (MMPLAYER_TARGET_STATE(player) == MM_PLAYER_STATE_PLAYING)
				{
					if ((MMPLAYER_IS_HTTP_STREAMING(player)) &&
						(player->streamer) &&
						(player->streamer->streaming_buffer_type == BUFFER_TYPE_MUXED) &&
						(player->streamer->is_buffering == FALSE))
					{
						GstQuery *query = NULL;
						gboolean busy = FALSE;
						gint percent = 0;

						if (player->streamer->buffer_handle[BUFFER_TYPE_MUXED].buffer)
						{
							query = gst_query_new_buffering ( GST_FORMAT_PERCENT );
							if ( gst_element_query (player->streamer->buffer_handle[BUFFER_TYPE_MUXED].buffer, query ) )
							{
								gst_query_parse_buffering_percent ( query, &busy, &percent);
							}
							gst_query_unref (query);

							LOGD("buffered percent(%s): %d\n",
								GST_ELEMENT_NAME(player->streamer->buffer_handle[BUFFER_TYPE_MUXED].buffer), percent);
						}

						if (percent >= 100)
						{
							player->streamer->is_buffering = FALSE;
							__mmplayer_handle_buffering_message(player);
						}
					}

					async_done = TRUE;
				}
			}
		}
		break;

		#if 0 /* delete unnecessary logs */
		case GST_MESSAGE_REQUEST_STATE:		LOGD("GST_MESSAGE_REQUEST_STATE\n"); break;
		case GST_MESSAGE_STEP_START:		LOGD("GST_MESSAGE_STEP_START\n"); break;
		case GST_MESSAGE_QOS:				LOGD("GST_MESSAGE_QOS\n"); break;
		case GST_MESSAGE_PROGRESS:			LOGD("GST_MESSAGE_PROGRESS\n"); break;
		case GST_MESSAGE_ANY:				LOGD("GST_MESSAGE_ANY\n"); break;
		case GST_MESSAGE_INFO:				LOGD("GST_MESSAGE_STATE_DIRTY\n"); break;
		case GST_MESSAGE_STATE_DIRTY:		LOGD("GST_MESSAGE_STATE_DIRTY\n"); break;
		case GST_MESSAGE_STEP_DONE:			LOGD("GST_MESSAGE_STEP_DONE\n"); break;
		case GST_MESSAGE_CLOCK_PROVIDE:		LOGD("GST_MESSAGE_CLOCK_PROVIDE\n"); break;
		case GST_MESSAGE_STRUCTURE_CHANGE:	LOGD("GST_MESSAGE_STRUCTURE_CHANGE\n"); break;
		case GST_MESSAGE_STREAM_STATUS:		LOGD("GST_MESSAGE_STREAM_STATUS\n"); break;
		case GST_MESSAGE_APPLICATION:		LOGD("GST_MESSAGE_APPLICATION\n"); break;
		case GST_MESSAGE_SEGMENT_START:		LOGD("GST_MESSAGE_SEGMENT_START\n"); break;
		case GST_MESSAGE_SEGMENT_DONE:		LOGD("GST_MESSAGE_SEGMENT_DONE\n"); break;
		case GST_MESSAGE_LATENCY:				LOGD("GST_MESSAGE_LATENCY\n"); break;
		#endif

		default:
		break;
	}

	/* FIXIT : this cause so many warnings/errors from glib/gstreamer. we should not call it since
	 * gst_element_post_message api takes ownership of the message.
	 */
	//gst_message_unref( msg );

	return ret;
}

static gboolean
__mmplayer_gst_handle_duration(mm_player_t* player, GstMessage* msg)
{
	gint64 bytes = 0;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL(player, FALSE);
	MMPLAYER_RETURN_VAL_IF_FAIL(msg, FALSE);

	if ((MMPLAYER_IS_HTTP_STREAMING(player)) &&
		(msg->src) && (msg->src == (GstObject *)player->pipeline->mainbin[MMPLAYER_M_SRC].gst))
	{
		LOGD("msg src : [%s]", GST_ELEMENT_NAME(GST_ELEMENT_CAST(msg->src)));

		if (gst_element_query_duration(GST_ELEMENT_CAST(msg->src), GST_FORMAT_BYTES, &bytes))
		{
			LOGD("data total size of http content: %lld", bytes);
			player->http_content_size = bytes;
		}
	}
	else
	{
		/* handling audio clip which has vbr. means duration is keep changing */
		_mmplayer_update_content_attrs (player, ATTR_DURATION );
	}

	MMPLAYER_FLEAVE();

	return TRUE;
}


static gboolean
__mmplayer_gst_extract_tag_from_msg(mm_player_t* player, GstMessage* msg) // @
{

/* macro for better code readability */
#define MMPLAYER_UPDATE_TAG_STRING(gsttag, attribute, playertag) \
if (gst_tag_list_get_string(tag_list, gsttag, &string)) \
{\
	if (string != NULL)\
	{\
		SECURE_LOGD ( "update tag string : %s\n", string); \
		mm_attrs_set_string_by_name(attribute, playertag, string); \
		g_free(string);\
		string = NULL;\
	}\
}

#define MMPLAYER_UPDATE_TAG_IMAGE(gsttag, attribute, playertag) \
GstSample *sample = NULL;\
if (gst_tag_list_get_sample_index(tag_list, gsttag, index, &sample))\
{\
	GstMapInfo info = GST_MAP_INFO_INIT;\
	buffer = gst_sample_get_buffer(sample);\
	if (!gst_buffer_map(buffer, &info, GST_MAP_READ)){\
		LOGD("failed to get image data from tag");\
		return FALSE;\
	}\
	SECURE_LOGD ( "update album cover data : %p, size : %d\n", info.data, info.size);\
	MMPLAYER_FREEIF(player->album_art); \
	player->album_art = (gchar *)g_malloc(info.size); \
	if (player->album_art) \
	{ \
		memcpy(player->album_art, info.data, info.size); \
		mm_attrs_set_data_by_name(attribute, playertag, (void *)player->album_art, info.size); \
		if (MMPLAYER_IS_HTTP_LIVE_STREAMING(player)) \
		{ \
			msg_param.data = (void *)player->album_art; \
			msg_param.size = info.size; \
			MMPLAYER_POST_MSG (player, MM_MESSAGE_IMAGE_BUFFER, &msg_param); \
			SECURE_LOGD ( "post message image buffer data : %p, size : %d\n", info.data, info.size); \
		} \
	} \
	gst_buffer_unmap(buffer, &info); \
}

#define MMPLAYER_UPDATE_TAG_UINT(gsttag, attribute, playertag) \
if (gst_tag_list_get_uint(tag_list, gsttag, &v_uint))\
{\
	if(v_uint)\
	{\
		if (!strncmp(gsttag, GST_TAG_BITRATE, strlen(GST_TAG_BITRATE))) \
		{\
			if (player->updated_bitrate_count == 0) \
				mm_attrs_set_int_by_name(attribute, "content_audio_bitrate", v_uint); \
			if (player->updated_bitrate_count<MM_PLAYER_STREAM_COUNT_MAX) \
			{\
				player->bitrate[player->updated_bitrate_count] = v_uint;\
				player->total_bitrate += player->bitrate[player->updated_maximum_bitrate_count]; \
				player->updated_bitrate_count++; \
				mm_attrs_set_int_by_name(attribute, playertag, player->total_bitrate);\
				SECURE_LOGD ( "update bitrate %d[bps] of stream #%d.\n", v_uint, player->updated_bitrate_count);\
			}\
		}\
		else if (!strncmp(gsttag, GST_TAG_MAXIMUM_BITRATE, strlen(GST_TAG_MAXIMUM_BITRATE))) \
		{\
			if (player->updated_maximum_bitrate_count<MM_PLAYER_STREAM_COUNT_MAX) \
			{\
				player->maximum_bitrate[player->updated_maximum_bitrate_count] = v_uint;\
				player->total_maximum_bitrate += player->maximum_bitrate[player->updated_maximum_bitrate_count]; \
				player->updated_maximum_bitrate_count++; \
				mm_attrs_set_int_by_name(attribute, playertag, player->total_maximum_bitrate); \
				SECURE_LOGD ( "update maximum bitrate %d[bps] of stream #%d\n", v_uint, player->updated_maximum_bitrate_count);\
			}\
		}\
		else\
		{\
			mm_attrs_set_int_by_name(attribute, playertag, v_uint); \
		}\
		v_uint = 0;\
	}\
}

#define MMPLAYER_UPDATE_TAG_DATE(gsttag, attribute, playertag) \
if (gst_tag_list_get_date(tag_list, gsttag, &date))\
{\
	if (date != NULL)\
	{\
		string = g_strdup_printf("%d", g_date_get_year(date));\
		mm_attrs_set_string_by_name(attribute, playertag, string);\
		SECURE_LOGD ( "metainfo year : %s\n", string);\
		MMPLAYER_FREEIF(string);\
		g_date_free(date);\
	}\
}

#define MMPLAYER_UPDATE_TAG_UINT64(gsttag, attribute, playertag) \
if(gst_tag_list_get_uint64(tag_list, gsttag, &v_uint64))\
{\
	if(v_uint64)\
	{\
		/* FIXIT : don't know how to store date */\
		g_assert(1);\
		v_uint64 = 0;\
	}\
}

#define MMPLAYER_UPDATE_TAG_DOUBLE(gsttag, attribute, playertag) \
if(gst_tag_list_get_double(tag_list, gsttag, &v_double))\
{\
	if(v_double)\
	{\
		/* FIXIT : don't know how to store date */\
		g_assert(1);\
		v_double = 0;\
	}\
}

	/* function start */
	GstTagList* tag_list = NULL;

	MMHandleType attrs = 0;

	char *string = NULL;
	guint v_uint = 0;
	GDate *date = NULL;
	/* album cover */
	GstBuffer *buffer = NULL;
	gint index = 0;
	MMMessageParamType msg_param = {0, };

	/* currently not used. but those are needed for above macro */
	//guint64 v_uint64 = 0;
	//gdouble v_double = 0;

	MMPLAYER_RETURN_VAL_IF_FAIL( player && msg, FALSE );

	attrs = MMPLAYER_GET_ATTRS(player);

	MMPLAYER_RETURN_VAL_IF_FAIL( attrs, FALSE );

	/* get tag list from gst message */
	gst_message_parse_tag(msg, &tag_list);

	/* store tags to player attributes */
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_TITLE, attrs, "tag_title");
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_TITLE_SORTNAME, ?, ?); */
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_ARTIST, attrs, "tag_artist");
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_ARTIST_SORTNAME, ?, ?); */
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_ALBUM, attrs, "tag_album");
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_ALBUM_SORTNAME, ?, ?); */
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_COMPOSER, attrs, "tag_author");
	MMPLAYER_UPDATE_TAG_DATE(GST_TAG_DATE, attrs, "tag_date");
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_GENRE, attrs, "tag_genre");
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_COMMENT, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_EXTENDED_COMMENT, ?, ?); */
	MMPLAYER_UPDATE_TAG_UINT(GST_TAG_TRACK_NUMBER, attrs, "tag_track_num");
	/* MMPLAYER_UPDATE_TAG_UINT(GST_TAG_TRACK_COUNT, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_UINT(GST_TAG_ALBUM_VOLUME_NUMBER, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_UINT(GST_TAG_ALBUM_VOLUME_COUNT, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_LOCATION, ?, ?); */
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_DESCRIPTION, attrs, "tag_description");
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_VERSION, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_ISRC, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_ORGANIZATION, ?, ?); */
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_COPYRIGHT, attrs, "tag_copyright");
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_COPYRIGHT_URI, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_CONTACT, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_LICENSE, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_LICENSE_URI, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_PERFORMER, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_UINT64(GST_TAG_DURATION, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_CODEC, ?, ?); */
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_VIDEO_CODEC, attrs, "content_video_codec");
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_AUDIO_CODEC, attrs, "content_audio_codec");
	MMPLAYER_UPDATE_TAG_UINT(GST_TAG_BITRATE, attrs, "content_bitrate");
	MMPLAYER_UPDATE_TAG_UINT(GST_TAG_MAXIMUM_BITRATE, attrs, "content_max_bitrate");
	MMPLAYER_UPDATE_TAG_IMAGE(GST_TAG_IMAGE, attrs, "tag_album_cover");
	/* MMPLAYER_UPDATE_TAG_UINT(GST_TAG_NOMINAL_BITRATE, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_UINT(GST_TAG_MINIMUM_BITRATE, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_UINT(GST_TAG_SERIAL, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_ENCODER, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_UINT(GST_TAG_ENCODER_VERSION, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_DOUBLE(GST_TAG_TRACK_GAIN, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_DOUBLE(GST_TAG_TRACK_PEAK, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_DOUBLE(GST_TAG_ALBUM_GAIN, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_DOUBLE(GST_TAG_ALBUM_PEAK, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_DOUBLE(GST_TAG_REFERENCE_LEVEL, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_STRING(GST_TAG_LANGUAGE_CODE, ?, ?); */
	/* MMPLAYER_UPDATE_TAG_DOUBLE(GST_TAG_BEATS_PER_MINUTE, ?, ?); */
	MMPLAYER_UPDATE_TAG_STRING(GST_TAG_IMAGE_ORIENTATION, attrs, "content_video_orientation");

	if ( mmf_attrs_commit ( attrs ) )
		LOGE("failed to commit.\n");

	gst_tag_list_free(tag_list);

	return TRUE;
}

static void
__mmplayer_gst_rtp_no_more_pads (GstElement *element,  gpointer data)  // @
{
	mm_player_t* player = (mm_player_t*) data;

	MMPLAYER_FENTER();

	/* NOTE : we can remove fakesink here if there's no rtp_dynamic_pad. because whenever
	  * we connect autoplugging element to the pad which is just added to rtspsrc, we increase
	  * num_dynamic_pad. and this is no-more-pad situation which means mo more pad will be added.
	  * So we can say this. if num_dynamic_pad is zero, it must be one of followings

	  * [1] audio and video will be dumped with filesink.
	  * [2] autoplugging is done by just using pad caps.
	  * [3] typefinding has happend in audio but audiosink is created already before no-more-pad signal
	  * and the video will be dumped via filesink.
	  */
	if ( player->num_dynamic_pad == 0 )
	{
		LOGD("it seems pad caps is directely used for autoplugging. removing fakesink now\n");

		if ( ! __mmplayer_gst_remove_fakesink( player,
			&player->pipeline->mainbin[MMPLAYER_M_SRC_FAKESINK]) )
		{
			/* NOTE : __mmplayer_pipeline_complete() can be called several time. because
			 * signaling mechanism ( pad-added, no-more-pad, new-decoded-pad ) from various
			 * source element are not same. To overcome this situation, this function will called
			 * several places and several times. Therefore, this is not an error case.
			 */
			return;
		}
	}

	/* create dot before error-return. for debugging */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-no-more-pad" );

	player->no_more_pad = TRUE;

	MMPLAYER_FLEAVE();
}

static gboolean
__mmplayer_gst_remove_fakesink(mm_player_t* player, MMPlayerGstElement* fakesink) // @
{
	GstElement* parent = NULL;

	MMPLAYER_RETURN_VAL_IF_FAIL(player && player->pipeline, FALSE);

	/* if we have no fakesink. this meas we are using decodebin which doesn'
	t need to add extra fakesink */
	MMPLAYER_RETURN_VAL_IF_FAIL(fakesink, TRUE);

	/* lock */
	g_mutex_lock(&player->fsink_lock );

	if ( ! fakesink->gst )
	{
		goto ERROR;
	}

	/* get parent of fakesink */
	parent = (GstElement*)gst_object_get_parent( (GstObject*)fakesink->gst );
	if ( ! parent )
	{
		LOGD("fakesink already removed\n");
		goto ERROR;
	}

	gst_element_set_locked_state( fakesink->gst, TRUE );

	/* setting the state to NULL never returns async
	 * so no need to wait for completion of state transiton
	 */
	if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state (fakesink->gst, GST_STATE_NULL) )
	{
		LOGE("fakesink state change failure!\n");

		/* FIXIT : should I return here? or try to proceed to next? */
		/* return FALSE; */
	}

	/* remove fakesink from it's parent */
	if ( ! gst_bin_remove( GST_BIN( parent ), fakesink->gst ) )
	{
		LOGE("failed to remove fakesink\n");

		gst_object_unref( parent );

		goto ERROR;
	}

	gst_object_unref( parent );

	LOGD("state-holder removed\n");

	gst_element_set_locked_state( fakesink->gst, FALSE );

	g_mutex_unlock( &player->fsink_lock );
	return TRUE;

ERROR:
	if ( fakesink->gst )
	{
		gst_element_set_locked_state( fakesink->gst, FALSE );
	}

	g_mutex_unlock( &player->fsink_lock );
	return FALSE;
}


static void
__mmplayer_gst_rtp_dynamic_pad (GstElement *element, GstPad *pad, gpointer data) // @
{
	GstPad *sinkpad = NULL;
	GstCaps* caps = NULL;
	GstElement* new_element = NULL;
	GstStructure* str = NULL;
	const gchar* name = NULL;

	mm_player_t* player = (mm_player_t*) data;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL( element && pad );
	MMPLAYER_RETURN_IF_FAIL(	player &&
					player->pipeline &&
					player->pipeline->mainbin );


	/* payload type is recognizable. increase num_dynamic and wait for sinkbin creation.
	 * num_dynamic_pad will decreased after creating a sinkbin.
	 */
	player->num_dynamic_pad++;
	LOGD("stream count inc : %d\n", player->num_dynamic_pad);

	caps = gst_pad_query_caps( pad, NULL );

	MMPLAYER_CHECK_NULL( caps );

	/* clear  previous result*/
	player->have_dynamic_pad = FALSE;

	str = gst_caps_get_structure(caps, 0);

	if ( ! str )
	{
		LOGE ("cannot get structure from caps.\n");
		goto ERROR;
	}

	name = gst_structure_get_name (str);
	if ( ! name )
	{
		LOGE ("cannot get mimetype from structure.\n");
		goto ERROR;
	}

	if (strstr(name, "video"))
	{
		gint stype = 0;
		mm_attrs_get_int_by_name (player->attrs, "display_surface_type", &stype);

		if (stype == MM_DISPLAY_SURFACE_NULL)
		{
			if (player->v_stream_caps)
			{
				gst_caps_unref(player->v_stream_caps);
				player->v_stream_caps = NULL;
			}

			new_element = gst_element_factory_make("fakesink", NULL);
			player->num_dynamic_pad--;
			goto NEW_ELEMENT;
		}
	}

	/* clear  previous result*/
	player->have_dynamic_pad = FALSE;

	if ( !__mmplayer_try_to_plug_decodebin(player, pad, caps))
	{
		LOGE("failed to autoplug for caps");
		goto ERROR;
	}

	/* check if there's dynamic pad*/
	if( player->have_dynamic_pad )
	{
		LOGE("using pad caps assums there's no dynamic pad !\n");
		goto ERROR;
	}

	gst_caps_unref( caps );
	caps = NULL;

NEW_ELEMENT:

	/* excute new_element if created*/
	if ( new_element )
	{
		LOGD("adding new element to pipeline\n");

		/* set state to READY before add to bin */
		MMPLAYER_ELEMENT_SET_STATE( new_element, GST_STATE_READY );

		/* add new element to the pipeline */
		if ( FALSE == gst_bin_add( GST_BIN(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst), new_element)  )
		{
			LOGE("failed to add autoplug element to bin\n");
			goto ERROR;
		}

		/* get pad from element */
		sinkpad = gst_element_get_static_pad ( GST_ELEMENT(new_element), "sink" );
		if ( !sinkpad )
		{
			LOGE("failed to get sinkpad from autoplug element\n");
			goto ERROR;
		}

		/* link it */
		if ( GST_PAD_LINK_OK != GST_PAD_LINK(pad, sinkpad) )
		{
			LOGE("failed to link autoplug element\n");
			goto ERROR;
		}

		gst_object_unref (sinkpad);
		sinkpad = NULL;

		/* run. setting PLAYING here since streamming source is live source */
		MMPLAYER_ELEMENT_SET_STATE( new_element, GST_STATE_PLAYING );
	}

	MMPLAYER_FLEAVE();

	return;

STATE_CHANGE_FAILED:
ERROR:
	/* FIXIT : take care if new_element has already added to pipeline */
	if ( new_element )
		gst_object_unref(GST_OBJECT(new_element));

	if ( sinkpad )
		gst_object_unref(GST_OBJECT(sinkpad));

	if ( caps )
		gst_object_unref(GST_OBJECT(caps));

	/* FIXIT : how to inform this error to MSL ????? */
	/* FIXIT : I think we'd better to use g_idle_add() to destroy pipeline and
	 * then post an error to application
	 */
}



/* FIXIT : check indent */
#if 0
static void
__mmplayer_gst_wfd_dynamic_pad (GstElement *element, GstPad *pad, gpointer data) // @
{
  GstPad *sinkpad = NULL;
  GstCaps* caps = NULL;
  GstElement* new_element = NULL;
  enum MainElementID element_id = MMPLAYER_M_NUM;

  mm_player_t* player = (mm_player_t*) data;

  MMPLAYER_FENTER();

  MMPLAYER_RETURN_IF_FAIL( element && pad );
  MMPLAYER_RETURN_IF_FAIL(  player &&
          player->pipeline &&
          player->pipeline->mainbin );

  LOGD("stream count inc : %d\n", player->num_dynamic_pad);

  {
    LOGD("using pad caps to autopluging instead of doing typefind\n");
    caps = gst_pad_query_caps( pad );
    MMPLAYER_CHECK_NULL( caps );
    /* clear  previous result*/
    player->have_dynamic_pad = FALSE;
    new_element = gst_element_factory_make("rtpmp2tdepay", "wfd_rtp_depay");
    if ( !new_element )
    {
      LOGE ( "failed to create wfd rtp depay element\n" );
      goto ERROR;
    }
    MMPLAYER_ELEMENT_SET_STATE( new_element, GST_STATE_READY );
    /* add new element to the pipeline */
    if ( FALSE == gst_bin_add( GST_BIN(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst), new_element)  )
    {
      LOGD("failed to add autoplug element to bin\n");
      goto ERROR;
    }
    /* get pad from element */
    sinkpad = gst_element_get_static_pad ( GST_ELEMENT(new_element), "sink" );
    if ( !sinkpad )
    {
      LOGD("failed to get sinkpad from autoplug element\n");
      goto ERROR;
    }
    /* link it */
    if ( GST_PAD_LINK_OK != GST_PAD_LINK(pad, sinkpad) )
    {
      LOGD("failed to link autoplug element\n");
      goto ERROR;
    }
    gst_object_unref (sinkpad);
    sinkpad = NULL;
    pad = gst_element_get_static_pad ( GST_ELEMENT(new_element), "src" );
    caps = gst_pad_query_caps( pad );
    MMPLAYER_CHECK_NULL( caps );
    MMPLAYER_ELEMENT_SET_STATE( new_element, GST_STATE_PLAYING );
    /* create typefind */
    new_element = gst_element_factory_make( "typefind", NULL );
    if ( ! new_element )
    {
      LOGD("failed to create typefind\n");
      goto ERROR;
    }

    MMPLAYER_SIGNAL_CONNECT(   player,
                G_OBJECT(new_element),
                MM_PLAYER_SIGNAL_TYPE_AUTOPLUG,
                "have-type",
                G_CALLBACK(__mmplayer_typefind_have_type),
                (gpointer)player);

    player->have_dynamic_pad = FALSE;
  }

  /* excute new_element if created*/
  if ( new_element )
  {
    LOGD("adding new element to pipeline\n");

    /* set state to READY before add to bin */
    MMPLAYER_ELEMENT_SET_STATE( new_element, GST_STATE_READY );

    /* add new element to the pipeline */
    if ( FALSE == gst_bin_add( GST_BIN(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst), new_element)  )
    {
      LOGD("failed to add autoplug element to bin\n");
      goto ERROR;
    }

    /* get pad from element */
    sinkpad = gst_element_get_static_pad ( GST_ELEMENT(new_element), "sink" );
    if ( !sinkpad )
    {
      LOGD("failed to get sinkpad from autoplug element\n");
      goto ERROR;
    }

    /* link it */
    if ( GST_PAD_LINK_OK != GST_PAD_LINK(pad, sinkpad) )
    {
      LOGD("failed to link autoplug element\n");
      goto ERROR;
    }

    gst_object_unref (sinkpad);
    sinkpad = NULL;

    /* run. setting PLAYING here since streamming source is live source */
    MMPLAYER_ELEMENT_SET_STATE( new_element, GST_STATE_PLAYING );
  }

  /* store handle to futher manipulation */
  player->pipeline->mainbin[element_id].id = element_id;
  player->pipeline->mainbin[element_id].gst = new_element;

  MMPLAYER_FLEAVE();

  return;

STATE_CHANGE_FAILED:
ERROR:
  /* FIXIT : take care if new_element has already added to pipeline */
  if ( new_element )
    gst_object_unref(GST_OBJECT(new_element));

  if ( sinkpad )
    gst_object_unref(GST_OBJECT(sinkpad));

  if ( caps )
    gst_object_unref(GST_OBJECT(caps));

  /* FIXIT : how to inform this error to MSL ????? */
  /* FIXIT : I think we'd better to use g_idle_add() to destroy pipeline and
   * then post an error to application
   */
}
#endif

static GstPadProbeReturn
__mmplayer_gst_selector_blocked(GstPad* pad, GstPadProbeInfo *info, gpointer data)
{
	LOGD ("pad(%s:%s) is blocked", GST_DEBUG_PAD_NAME(pad));
	return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn
__mmplayer_gapless_sinkbin_data_probe (GstPad *pad, GstPadProbeInfo *info, gpointer u_data)
{
	mm_player_t* player = (mm_player_t*) u_data;
	GstBuffer *pad_buffer = gst_pad_probe_info_get_buffer(info);

	/* TO_CHECK: performance */
	MMPLAYER_RETURN_VAL_IF_FAIL (player && GST_IS_BUFFER(pad_buffer), GST_PAD_PROBE_OK);

	if (GST_BUFFER_PTS_IS_VALID(pad_buffer) && GST_BUFFER_DURATION_IS_VALID(pad_buffer)) {
		/* keep next buffer pts for sychronization of gapless playback */
		/* see : __mmplayer_gst_selector_event_probe() */
		/* next buffer start position = current buffer pts + current buffer duration*/
		player->gapless.next_pts = GST_BUFFER_PTS(pad_buffer) + GST_BUFFER_DURATION(pad_buffer);
	}

	return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn
__mmplayer_gst_selector_event_probe (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
	GstPadProbeReturn ret = GST_PAD_PROBE_OK;
	GstEvent *event = GST_PAD_PROBE_INFO_DATA (info);
	mm_player_t* player = (mm_player_t*)data;

	switch (GST_EVENT_TYPE (event)) {
		case GST_EVENT_STREAM_START:
		break;
		case GST_EVENT_SEGMENT: {
			GstSegment segment;
			GstEvent *tmpev;

			if (!player->gapless.running)
				break;

			if (player->gapless.stream_changed) {
				/* FIXME: need to set max(duraion, next_pts)? */
				player->gapless.start_time += player->gapless.next_pts;
				player->gapless.stream_changed = FALSE;
			}

			LOGD ("event: %" GST_PTR_FORMAT, event);
			gst_event_copy_segment (event, &segment);

			if (segment.format == GST_FORMAT_TIME)
			{
				segment.base = player->gapless.start_time;
				LOGD ("base of segment: %" GST_TIME_FORMAT, GST_TIME_ARGS (segment.base));

				tmpev = gst_event_new_segment (&segment);
				gst_event_set_seqnum (tmpev, gst_event_get_seqnum (event));
				gst_event_unref (event);
				GST_PAD_PROBE_INFO_DATA(info) = tmpev;
			}
			break;
		}
		default:
		break;
	}
	return ret;
}

static void
__mmplayer_gst_decode_pad_added (GstElement *elem, GstPad *pad, gpointer data)
{
	mm_player_t* player = NULL;
	GstElement* pipeline = NULL;
	GstElement* selector = NULL;
	GstElement* fakesink = NULL;
	GstCaps* caps = NULL;
	GstStructure* str = NULL;
	const gchar* name = NULL;
	GstPad* sinkpad = NULL;
	GstPad* srcpad = NULL;
	gboolean first_track = FALSE;

	enum MainElementID elemId = MMPLAYER_M_NUM;
	MMPlayerTrackType stream_type = MM_PLAYER_TRACK_TYPE_AUDIO;

	/* check handles */
	player = (mm_player_t*)data;

	MMPLAYER_RETURN_IF_FAIL (elem && pad);
	MMPLAYER_RETURN_IF_FAIL (player && player->pipeline && player->pipeline->mainbin);

	//LOGD ("pad-added signal handling\n");

	pipeline = player->pipeline->mainbin[MMPLAYER_M_PIPE].gst;

	/* get mimetype from caps */
	caps = gst_pad_query_caps (pad, NULL);
	if ( !caps )
	{
		LOGE ("cannot get caps from pad.\n");
		goto ERROR;
	}

	str = gst_caps_get_structure (caps, 0);
	if ( ! str )
	{
		LOGE ("cannot get structure from caps.\n");
		goto ERROR;
	}

	name = gst_structure_get_name (str);
	if ( ! name )
	{
		LOGE ("cannot get mimetype from structure.\n");
		goto ERROR;
	}

	MMPLAYER_LOG_GST_CAPS_TYPE(caps);
	//LOGD ("detected mimetype : %s\n", name);

	if (strstr(name, "video"))
	{
		gint stype = 0;
		mm_attrs_get_int_by_name (player->attrs, "display_surface_type", &stype);

		/* don't make video because of not required, and not support multiple track */
		if (stype == MM_DISPLAY_SURFACE_NULL)
		{
			LOGD ("no video sink by null surface or multiple track");
			gchar *caps_str = gst_caps_to_string(caps);
			if (strstr(caps_str, "ST12") || strstr(caps_str, "SN12"))
			{
				player->set_mode.video_zc = TRUE;
			}
			MMPLAYER_FREEIF( caps_str );

			if (player->v_stream_caps)
			{
				gst_caps_unref(player->v_stream_caps);
				player->v_stream_caps = NULL;
			}

			LOGD ("create fakesink instead of videobin");

			/* fake sink */
			fakesink = gst_element_factory_make ("fakesink", NULL);
			if (fakesink == NULL)
			{
				LOGE ("ERROR : fakesink create error\n");
				goto ERROR;
			}

			player->video_fakesink = fakesink;

			gst_bin_add (GST_BIN(pipeline), fakesink);

			// link
			sinkpad = gst_element_get_static_pad (fakesink, "sink");

			if (GST_PAD_LINK_OK != gst_pad_link(pad, sinkpad))
			{
				LOGW ("failed to link fakesink\n");
				gst_object_unref (GST_OBJECT(fakesink));
				goto ERROR;
			}

			if (player->set_mode.media_packet_video_stream)
				player->video_cb_probe_id = gst_pad_add_probe (sinkpad, GST_PAD_PROBE_TYPE_BUFFER, __mmplayer_video_stream_probe, player, NULL);

			g_object_set (G_OBJECT (fakesink), "async", TRUE, NULL);
			g_object_set (G_OBJECT (fakesink), "sync", TRUE, NULL);
			gst_element_set_state (fakesink, GST_STATE_PAUSED);

			goto DONE;
		}

		if (MMPLAYER_IS_MS_BUFF_SRC(player))
		{
			__mmplayer_gst_decode_callback (elem, pad, player);
			return;
		}

		LOGD ("video selector \n");
		elemId = MMPLAYER_M_V_INPUT_SELECTOR;
		stream_type = MM_PLAYER_TRACK_TYPE_VIDEO;
	}
	else
	{
		if (strstr(name, "audio"))
		{
			gint samplerate = 0;
			gint channels = 0;

			if (MMPLAYER_IS_MS_BUFF_SRC(player))
			{
				__mmplayer_gst_decode_callback (elem, pad, player);
				return;
			}

			LOGD ("audio selector \n");
			elemId = MMPLAYER_M_A_INPUT_SELECTOR;
			stream_type = MM_PLAYER_TRACK_TYPE_AUDIO;

			gst_structure_get_int (str, "rate", &samplerate);
			gst_structure_get_int (str, "channels", &channels);

			if ((channels > 0 && samplerate == 0)) {//exclude audio decoding
	  			/* fake sink */
	  			fakesink = gst_element_factory_make ("fakesink", NULL);
	  			if (fakesink == NULL)
	  			{
	  				LOGE ("ERROR : fakesink create error\n");
	  				goto ERROR;
	  			}

				gst_bin_add (GST_BIN(pipeline), fakesink);

				/* link */
				sinkpad = gst_element_get_static_pad (fakesink, "sink");

				if (GST_PAD_LINK_OK != gst_pad_link(pad, sinkpad))
				{
					LOGW ("failed to link fakesink\n");
					gst_object_unref (GST_OBJECT(fakesink));
					goto ERROR;
				}

				g_object_set (G_OBJECT (fakesink), "async", TRUE, NULL);
				g_object_set (G_OBJECT (fakesink), "sync", TRUE, NULL);
				gst_element_set_state (fakesink, GST_STATE_PAUSED);

				goto DONE;
			}
		}
		else if (strstr(name, "text"))
		{
			LOGD ("text selector \n");
			elemId = MMPLAYER_M_T_INPUT_SELECTOR;
			stream_type = MM_PLAYER_TRACK_TYPE_TEXT;
		}
		else
		{
			LOGE ("wrong elem id \n");
			goto ERROR;
		}
	}

	selector = player->pipeline->mainbin[elemId].gst;
	if (selector == NULL)
	{
		selector = gst_element_factory_make ("input-selector", NULL);
		LOGD ("Creating input-selector\n");
		if (selector == NULL)
		{
			LOGE ("ERROR : input-selector create error\n");
			goto ERROR;
		}
		g_object_set (selector, "sync-streams", TRUE, NULL);

		player->pipeline->mainbin[elemId].id = elemId;
		player->pipeline->mainbin[elemId].gst = selector;

		first_track = TRUE;
		// player->selector[stream_type].active_pad_index = DEFAULT_TRACK;	// default

		srcpad = gst_element_get_static_pad (selector, "src");

		LOGD ("blocking %s:%s", GST_DEBUG_PAD_NAME(srcpad));
		player->selector[stream_type].block_id = gst_pad_add_probe(srcpad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
			__mmplayer_gst_selector_blocked, NULL, NULL);
		player->selector[stream_type].event_probe_id = gst_pad_add_probe(srcpad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
			__mmplayer_gst_selector_event_probe, player, NULL);

		gst_element_set_state (selector, GST_STATE_PAUSED);
		gst_bin_add (GST_BIN(pipeline), selector);
	}
	else
	{
		LOGD ("input-selector is already created.\n");
		selector = player->pipeline->mainbin[elemId].gst;
	}

	// link
	LOGD ("Calling request pad with selector %p \n", selector);
	sinkpad = gst_element_get_request_pad (selector, "sink_%u");

	LOGD ("got pad %s:%s from selector", GST_DEBUG_PAD_NAME (sinkpad));

	if (GST_PAD_LINK_OK != gst_pad_link(pad, sinkpad))
	{
		LOGW ("failed to link selector\n");
		gst_object_unref (GST_OBJECT(selector));
		goto ERROR;
	}

	if (first_track)
	{
		LOGD ("this is first track --> active track \n");
		g_object_set (selector, "active-pad", sinkpad, NULL);
	}

	_mmplayer_track_update_info(player, stream_type, sinkpad);


DONE:
ERROR:

	if (caps)
	{
		gst_caps_unref (caps);
	}

	if (sinkpad)
	{
		gst_object_unref (GST_OBJECT(sinkpad));
		sinkpad = NULL;
	}

	if (srcpad)
	{
		gst_object_unref (GST_OBJECT(srcpad));
		srcpad = NULL;
	}

	return;
}

static void __mmplayer_handle_text_decode_path(mm_player_t* player, GstElement* text_selector)
{
	GstPad* srcpad = NULL;
	MMHandleType attrs = 0;
	gint active_index = 0;

	// [link] input-selector :: textbin
	srcpad = gst_element_get_static_pad (text_selector, "src");
	if (!srcpad)
	{
		LOGE("failed to get srcpad from selector\n");
		return;
	}

	LOGD ("got pad %s:%s from text selector\n", GST_DEBUG_PAD_NAME(srcpad));

	active_index = player->selector[MM_PLAYER_TRACK_TYPE_TEXT].active_pad_index;
	if ((active_index != DEFAULT_TRACK) &&
		(__mmplayer_change_selector_pad(player, MM_PLAYER_TRACK_TYPE_TEXT, active_index) != MM_ERROR_NONE))
	{
		LOGW("failed to change text track\n");
		player->selector[MM_PLAYER_TRACK_TYPE_TEXT].active_pad_index = DEFAULT_TRACK;
	}

	player->no_more_pad = TRUE;
	__mmplayer_gst_decode_callback (text_selector, srcpad, player);

	LOGD ("unblocking %s:%s", GST_DEBUG_PAD_NAME(srcpad));
	if (player->selector[MM_PLAYER_TRACK_TYPE_TEXT].block_id)
	{
		gst_pad_remove_probe (srcpad, player->selector[MM_PLAYER_TRACK_TYPE_TEXT].block_id);
		player->selector[MM_PLAYER_TRACK_TYPE_TEXT].block_id = 0;
	}

	LOGD("Total text tracks = %d \n", player->selector[MM_PLAYER_TRACK_TYPE_TEXT].total_track_num);

	if (player->selector[MM_PLAYER_TRACK_TYPE_TEXT].total_track_num > 0)
		player->has_closed_caption = TRUE;

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( attrs )
	{
		mm_attrs_set_int_by_name(attrs, "content_text_track_num",(gint)player->selector[MM_PLAYER_TRACK_TYPE_TEXT].total_track_num);
		if (mmf_attrs_commit (attrs))
			LOGE("failed to commit.\n");
	}
	else
	{
		LOGE("cannot get content attribute");
	}

	if (srcpad)
	{
		gst_object_unref ( GST_OBJECT(srcpad) );
		srcpad = NULL;
	}
}

int _mmplayer_gst_set_audio_channel(MMHandleType hplayer, MMPlayerAudioChannel ch_idx)
{
	int result = MM_ERROR_NONE;

	mm_player_t* player = (mm_player_t*)hplayer;
	MMPlayerGstElement* mainbin = NULL;
	gchar* change_pad_name = NULL;
	GstPad* sinkpad = NULL;
	GstCaps* caps = NULL;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL (player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	LOGD ("Change Audio mode to %d\n", ch_idx);
	player->use_deinterleave = TRUE;

	if ((!player->pipeline) || (!player->pipeline->mainbin))
	{
		LOGD ("pre setting : %d\n", ch_idx);

		player->audio_mode.active_pad_index = ch_idx;
		return result;
	}

	mainbin = player->pipeline->mainbin;

	if (mainbin[MMPLAYER_M_A_SELECTOR].gst == NULL)
	{
		if (player->max_audio_channels < 2)
		{
			LOGD ("mono channel track only\n");
			return result;
		}

		LOGW ("selector doesn't exist\n");
		return result;	/* keep playing */
	}

	LOGD ("total_ch_num : %d\n", player->audio_mode.total_track_num);

	if (player->audio_mode.total_track_num < 2)
	{
		LOGW ("there is no another audio path\n");
		return result;	/* keep playing */
	}

	if ((ch_idx < 0) || (ch_idx >= player->audio_mode.total_track_num))
	{
		LOGW ("Not a proper ch_idx : %d \n", ch_idx);
		return result;	/* keep playing */
	}

	/*To get the new pad from the selector*/
	change_pad_name = g_strdup_printf ("sink%d", ch_idx);
	if (change_pad_name == NULL)
	{
		LOGW ("Pad does not exists\n");
		goto ERROR;	/* keep playing */
	}

	LOGD ("new active pad name: %s\n", change_pad_name);

	sinkpad = gst_element_get_static_pad (mainbin[MMPLAYER_M_A_SELECTOR].gst, change_pad_name);
	if (sinkpad == NULL)
	{
		//result = MM_ERROR_PLAYER_INTERNAL;
		goto ERROR;	/* keep playing */
	}

	LOGD ("Set Active Pad - %s:%s\n", GST_DEBUG_PAD_NAME(sinkpad));
	g_object_set (mainbin[MMPLAYER_M_A_SELECTOR].gst, "active-pad", sinkpad, NULL);

	caps = gst_pad_get_current_caps(sinkpad);
	MMPLAYER_LOG_GST_CAPS_TYPE(caps);

	__mmplayer_set_audio_attrs (player, caps);
	player->audio_mode.active_pad_index = ch_idx;

ERROR:

	if (sinkpad)
		gst_object_unref (sinkpad);

	MMPLAYER_FREEIF(change_pad_name);

	MMPLAYER_FLEAVE();
	return result;
}



static void
__mmplayer_gst_deinterleave_pad_added(GstElement *elem, GstPad *pad, gpointer data)
{
	mm_player_t* player = (mm_player_t*)data;
	GstElement* selector = NULL;
	GstElement* queue = NULL;

	GstPad* srcpad = NULL;
	GstPad* sinkpad = NULL;
	gchar* caps_str= NULL;

	MMPLAYER_FENTER();
	MMPLAYER_RETURN_IF_FAIL (player && player->pipeline && player->pipeline->mainbin);

	caps_str = gst_caps_to_string(gst_pad_get_current_caps(pad));
	LOGD ("deinterleave new caps : %s\n", caps_str);
	MMPLAYER_FREEIF(caps_str);

	if ((queue = __mmplayer_element_create_and_link(player, pad, "queue")) == NULL)
	{
		LOGE ("ERROR : queue create error\n");
		goto ERROR;
	}

	g_object_set(G_OBJECT(queue),
				"max-size-buffers", 10,
				"max-size-bytes", 0,
				"max-size-time", (guint64)0,
				NULL);

	selector = player->pipeline->mainbin[MMPLAYER_M_A_SELECTOR].gst;

	if (!selector)
	{
		LOGE("there is no audio channel selector.\n");
		goto ERROR;
	}

	srcpad = gst_element_get_static_pad (queue, "src");
	sinkpad = gst_element_get_request_pad (selector, "sink_%u");

	LOGD ("link (%s:%s - %s:%s)\n", GST_DEBUG_PAD_NAME(srcpad), GST_DEBUG_PAD_NAME(sinkpad));

	if (GST_PAD_LINK_OK != gst_pad_link(srcpad, sinkpad))
	{
		LOGW ("failed to link deinterleave - selector\n");
		goto ERROR;
	}

	gst_element_set_state (queue, GST_STATE_PAUSED);
	player->audio_mode.total_track_num++;

ERROR:

	if (srcpad)
	{
		gst_object_unref ( GST_OBJECT(srcpad) );
		srcpad = NULL;
	}

	if (sinkpad)
	{
		gst_object_unref ( GST_OBJECT(sinkpad) );
		sinkpad = NULL;
	}

	MMPLAYER_FLEAVE();
	return;
}

static void
__mmplayer_gst_deinterleave_no_more_pads (GstElement *elem, gpointer data)
{
	mm_player_t* player = NULL;
	GstElement* selector = NULL;
	GstPad* sinkpad = NULL;
	gint active_index = 0;
	gchar* change_pad_name = NULL;
	GstCaps* caps = NULL;	// no need to unref
	gint default_audio_ch = 0;

	MMPLAYER_FENTER();
	player = (mm_player_t*) data;

	selector = player->pipeline->mainbin[MMPLAYER_M_A_SELECTOR].gst;

	if (!selector)
	{
		LOGE("there is no audio channel selector.\n");
		goto ERROR;
	}

	active_index = player->audio_mode.active_pad_index;

	if (active_index != default_audio_ch)
	{
		gint audio_ch = default_audio_ch;

		/*To get the new pad from the selector*/
		change_pad_name = g_strdup_printf ("sink%d", active_index);
		if (change_pad_name != NULL)
		{
			sinkpad = gst_element_get_static_pad (selector, change_pad_name);
			if (sinkpad != NULL)
			{
				LOGD ("Set Active Pad - %s:%s\n", GST_DEBUG_PAD_NAME(sinkpad));
				g_object_set (selector, "active-pad", sinkpad, NULL);

				audio_ch = active_index;

				caps = gst_pad_get_current_caps(sinkpad);
				MMPLAYER_LOG_GST_CAPS_TYPE(caps);

				__mmplayer_set_audio_attrs (player, caps);
			}
		}

		player->audio_mode.active_pad_index = audio_ch;
		LOGD("audio LR info (0:stereo) = %d\n", player->audio_mode.active_pad_index);
	}

ERROR:

	if (sinkpad)
		gst_object_unref (sinkpad);

	MMPLAYER_FLEAVE();
	return;
}

static void
__mmplayer_gst_build_deinterleave_path (GstElement *elem, GstPad *pad, gpointer data)
{
	mm_player_t* player = NULL;
	MMPlayerGstElement *mainbin = NULL;

	GstElement* tee = NULL;
	GstElement* stereo_queue = NULL;
	GstElement* mono_queue = NULL;
	GstElement* conv = NULL;
	GstElement* filter = NULL;
	GstElement* deinterleave = NULL;
	GstElement* selector = NULL;

	GstPad* srcpad = NULL;
	GstPad* selector_srcpad = NULL;
	GstPad* sinkpad = NULL;
	GstCaps* caps = NULL;
	gulong block_id = 0;

	MMPLAYER_FENTER();

	/* check handles */
	player = (mm_player_t*) data;

	MMPLAYER_RETURN_IF_FAIL( elem && pad );
	MMPLAYER_RETURN_IF_FAIL( player && player->pipeline && player->pipeline->mainbin );

	mainbin = player->pipeline->mainbin;

	/* tee */
	if ((tee = __mmplayer_element_create_and_link(player, pad, "tee")) == NULL)
	{
		LOGE ("ERROR : tee create error\n");
		goto ERROR;
	}

	mainbin[MMPLAYER_M_A_TEE].id = MMPLAYER_M_A_TEE;
	mainbin[MMPLAYER_M_A_TEE].gst = tee;

	gst_element_set_state (tee, GST_STATE_PAUSED);

	/* queue */
	srcpad = gst_element_get_request_pad (tee, "src_%u");
	if ((stereo_queue = __mmplayer_element_create_and_link(player, srcpad, "queue")) == NULL)
	{
		LOGE ("ERROR : stereo queue create error\n");
		goto ERROR;
	}

	g_object_set(G_OBJECT(stereo_queue),
				"max-size-buffers", 10,
				"max-size-bytes", 0,
				"max-size-time", (guint64)0,
				NULL);

	player->pipeline->mainbin[MMPLAYER_M_A_Q1].id = MMPLAYER_M_A_Q1;
	player->pipeline->mainbin[MMPLAYER_M_A_Q1].gst = stereo_queue;

	if (srcpad)
	{
		gst_object_unref (GST_OBJECT(srcpad));
		srcpad = NULL;
	}

	srcpad = gst_element_get_request_pad (tee, "src_%u");

	if ((mono_queue = __mmplayer_element_create_and_link(player, srcpad, "queue")) == NULL)
	{
		LOGE ("ERROR : mono queue create error\n");
		goto ERROR;
	}

	g_object_set(G_OBJECT(mono_queue),
				"max-size-buffers", 10,
				"max-size-bytes", 0,
				"max-size-time", (guint64)0,
				NULL);

	player->pipeline->mainbin[MMPLAYER_M_A_Q2].id = MMPLAYER_M_A_Q2;
	player->pipeline->mainbin[MMPLAYER_M_A_Q2].gst = mono_queue;

	gst_element_set_state (stereo_queue, GST_STATE_PAUSED);
	gst_element_set_state (mono_queue, GST_STATE_PAUSED);

	/* audioconvert */
	srcpad = gst_element_get_static_pad (mono_queue, "src");
	if ((conv = __mmplayer_element_create_and_link(player, srcpad, "audioconvert")) == NULL)
	{
		LOGE ("ERROR : audioconvert create error\n");
		goto ERROR;
	}

	player->pipeline->mainbin[MMPLAYER_M_A_CONV].id = MMPLAYER_M_A_CONV;
	player->pipeline->mainbin[MMPLAYER_M_A_CONV].gst = conv;

	/* caps filter */
	if (srcpad)
	{
		gst_object_unref (GST_OBJECT(srcpad));
		srcpad = NULL;
	}
	srcpad = gst_element_get_static_pad (conv, "src");

	if ((filter = __mmplayer_element_create_and_link(player, srcpad, "capsfilter")) == NULL)
	{
		LOGE ("ERROR : capsfilter create error\n");
		goto ERROR;
	}

	player->pipeline->mainbin[MMPLAYER_M_A_FILTER].id = MMPLAYER_M_A_FILTER;
	player->pipeline->mainbin[MMPLAYER_M_A_FILTER].gst = filter;

	caps = gst_caps_from_string( "audio/x-raw-int, "
				"width = (int) 16, "
				"depth = (int) 16, "
				"channels = (int) 2");

	g_object_set (GST_ELEMENT(player->pipeline->mainbin[MMPLAYER_M_A_FILTER].gst), "caps", caps, NULL );
	gst_caps_unref( caps );

	gst_element_set_state (conv, GST_STATE_PAUSED);
	gst_element_set_state (filter, GST_STATE_PAUSED);

	/* deinterleave */
	if (srcpad)
	{
		gst_object_unref (GST_OBJECT(srcpad));
		srcpad = NULL;
	}
	srcpad = gst_element_get_static_pad (filter, "src");

	if ((deinterleave = __mmplayer_element_create_and_link(player, srcpad, "deinterleave")) == NULL)
	{
		LOGE ("ERROR : deinterleave create error\n");
		goto ERROR;
	}

	g_object_set (deinterleave, "keep-positions", TRUE, NULL);

	MMPLAYER_SIGNAL_CONNECT (player, deinterleave, MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "pad-added",
							G_CALLBACK (__mmplayer_gst_deinterleave_pad_added), player);

	MMPLAYER_SIGNAL_CONNECT (player, deinterleave, MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "no-more-pads",
							G_CALLBACK (__mmplayer_gst_deinterleave_no_more_pads), player);

	player->pipeline->mainbin[MMPLAYER_M_A_DEINTERLEAVE].id = MMPLAYER_M_A_DEINTERLEAVE;
	player->pipeline->mainbin[MMPLAYER_M_A_DEINTERLEAVE].gst = deinterleave;

	/* selector */
	selector = gst_element_factory_make ("input-selector", "audio-channel-selector");
	if (selector == NULL)
	{
		LOGE ("ERROR : audio-selector create error\n");
		goto ERROR;
	}

	g_object_set (selector, "sync-streams", TRUE, NULL);
	gst_bin_add (GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), selector);

	player->pipeline->mainbin[MMPLAYER_M_A_SELECTOR].id = MMPLAYER_M_A_SELECTOR;
	player->pipeline->mainbin[MMPLAYER_M_A_SELECTOR].gst = selector;

	selector_srcpad = gst_element_get_static_pad (selector, "src");

	LOGD ("blocking %s:%s", GST_DEBUG_PAD_NAME(selector_srcpad));
	block_id =
		gst_pad_add_probe(selector_srcpad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
			__mmplayer_gst_selector_blocked, NULL, NULL);

	if (srcpad)
	{
		gst_object_unref (GST_OBJECT(srcpad));
		srcpad = NULL;
	}

	srcpad = gst_element_get_static_pad(stereo_queue, "src");
	sinkpad = gst_element_get_request_pad (selector, "sink_%u");

	if (GST_PAD_LINK_OK != gst_pad_link(srcpad, sinkpad))
	{
		LOGW ("failed to link queue_stereo - selector\n");
		goto ERROR;
	}

	player->audio_mode.total_track_num++;

	g_object_set (selector, "active-pad", sinkpad, NULL);
	gst_element_set_state (deinterleave, GST_STATE_PAUSED);
	gst_element_set_state (selector, GST_STATE_PAUSED);

	__mmplayer_gst_decode_callback (selector, selector_srcpad, player);

ERROR:

	LOGD ("unblocking %s:%s", GST_DEBUG_PAD_NAME(selector_srcpad));
	if (block_id != 0)
	{
		gst_pad_remove_probe (selector_srcpad, block_id);
		block_id = 0;
	}

	if (sinkpad)
	{
		gst_object_unref (GST_OBJECT(sinkpad));
		sinkpad = NULL;
	}

	if (srcpad)
	{
		gst_object_unref (GST_OBJECT(srcpad));
		srcpad = NULL;
	}

	if (selector_srcpad)
	{
		gst_object_unref (GST_OBJECT(selector_srcpad));
		selector_srcpad = NULL;
	}

	MMPLAYER_FLEAVE();
	return;
}

static void
__mmplayer_gst_decode_no_more_pads (GstElement *elem, gpointer data)
{
	mm_player_t* player = NULL;
	GstPad* srcpad = NULL;
	GstElement* video_selector = NULL;
	GstElement* audio_selector = NULL;
	GstElement* text_selector = NULL;
	MMHandleType attrs = 0;
	gint active_index = 0;
	gint64 dur_bytes = 0L;

	player = (mm_player_t*) data;

	LOGD("no-more-pad signal handling\n");

	if ((player->cmd == MMPLAYER_COMMAND_DESTROY) ||
		(player->cmd == MMPLAYER_COMMAND_UNREALIZE))
	{
		LOGW("no need to go more");

		if (player->gapless.reconfigure)
		{
			player->gapless.reconfigure = FALSE;
			MMPLAYER_PLAYBACK_UNLOCK(player);
		}

		return;
	}

	if ((!MMPLAYER_IS_HTTP_PD(player)) &&
		(MMPLAYER_IS_HTTP_STREAMING(player)) &&
		(!player->pipeline->mainbin[MMPLAYER_M_DEMUXED_S_BUFFER].gst) &&
		(player->pipeline->mainbin[MMPLAYER_M_MUXED_S_BUFFER].gst))
	{
		#define ESTIMATED_BUFFER_UNIT (1*1024*1024)

		if (NULL == player->streamer)
		{
			LOGW("invalid state for buffering");
			goto ERROR;
		}

		gdouble init_buffering_time = (gdouble)player->streamer->buffering_req.initial_second;
		guint buffer_bytes = init_buffering_time * ESTIMATED_BUFFER_UNIT;

		buffer_bytes = MAX(buffer_bytes, player->streamer->buffer_handle[BUFFER_TYPE_MUXED].buffering_bytes);
		LOGD("[Decodebin2] set use-buffering on Q2 (pre buffer time: %d sec, buffer size : %d)\n", (gint)init_buffering_time, buffer_bytes);

		init_buffering_time = (init_buffering_time != 0)?(init_buffering_time):(player->ini.http_buffering_time);

		if ( !gst_element_query_duration(player->pipeline->mainbin[MMPLAYER_M_SRC].gst, GST_FORMAT_BYTES, &dur_bytes))
			LOGE("fail to get duration.\n");

		// enable use-buffering on queue2 instead of multiqueue (ex)audio only streaming
		// use file information was already set on Q2 when it was created.
		__mm_player_streaming_set_queue2(player->streamer,
						player->pipeline->mainbin[MMPLAYER_M_MUXED_S_BUFFER].gst,
						TRUE,								// use_buffering
						buffer_bytes,
						init_buffering_time,
						1.0,								// low percent
						player->ini.http_buffering_limit,	// high percent
						FALSE,
						NULL,
						((dur_bytes>0)?((guint64)dur_bytes):0));
	}

	video_selector = player->pipeline->mainbin[MMPLAYER_M_V_INPUT_SELECTOR].gst;
	audio_selector = player->pipeline->mainbin[MMPLAYER_M_A_INPUT_SELECTOR].gst;
	text_selector = player->pipeline->mainbin[MMPLAYER_M_T_INPUT_SELECTOR].gst;
	if (video_selector)
	{
		// [link] input-selector :: videobin
		srcpad = gst_element_get_static_pad (video_selector, "src");
		if (!srcpad)
		{
			LOGE("failed to get srcpad from video selector\n");
			goto ERROR;
		}

		LOGD ("got pad %s:%s from video selector\n", GST_DEBUG_PAD_NAME(srcpad));
		if (!text_selector && !audio_selector)
			player->no_more_pad = TRUE;

		__mmplayer_gst_decode_callback (video_selector, srcpad, player);

		LOGD ("unblocking %s:%s", GST_DEBUG_PAD_NAME(srcpad));
		if (player->selector[MM_PLAYER_TRACK_TYPE_VIDEO].block_id)
		{
			gst_pad_remove_probe (srcpad, player->selector[MM_PLAYER_TRACK_TYPE_VIDEO].block_id);
			player->selector[MM_PLAYER_TRACK_TYPE_VIDEO].block_id = 0;
		}
	}

	if (audio_selector)
	{
		active_index = player->selector[MM_PLAYER_TRACK_TYPE_AUDIO].active_pad_index;
		if ((active_index != DEFAULT_TRACK) &&
			(__mmplayer_change_selector_pad(player, MM_PLAYER_TRACK_TYPE_AUDIO, active_index) != MM_ERROR_NONE))
		{
			LOGW("failed to change audio track\n");
			player->selector[MM_PLAYER_TRACK_TYPE_AUDIO].active_pad_index = DEFAULT_TRACK;
		}

		// [link] input-selector :: audiobin
		srcpad = gst_element_get_static_pad (audio_selector, "src");
		if (!srcpad)
		{
			LOGE("failed to get srcpad from selector\n");
			goto ERROR;
		}

		LOGD ("got pad %s:%s from selector\n", GST_DEBUG_PAD_NAME(srcpad));
		if (!text_selector)
			player->no_more_pad = TRUE;

		if ((player->use_deinterleave == TRUE) && (player->max_audio_channels >= 2))
		{
			LOGD ("unblocking %s:%s", GST_DEBUG_PAD_NAME(srcpad));
			if (player->selector[MM_PLAYER_TRACK_TYPE_AUDIO].block_id)
			{
				gst_pad_remove_probe (srcpad, player->selector[MM_PLAYER_TRACK_TYPE_AUDIO].block_id);
				player->selector[MM_PLAYER_TRACK_TYPE_AUDIO].block_id = 0;
			}

			__mmplayer_gst_build_deinterleave_path(audio_selector, srcpad, player);
		}
		else
		{
			__mmplayer_gst_decode_callback (audio_selector, srcpad, player);

			LOGD ("unblocking %s:%s", GST_DEBUG_PAD_NAME(srcpad));
			if (player->selector[MM_PLAYER_TRACK_TYPE_AUDIO].block_id)
			{
				gst_pad_remove_probe (srcpad, player->selector[MM_PLAYER_TRACK_TYPE_AUDIO].block_id);
				player->selector[MM_PLAYER_TRACK_TYPE_AUDIO].block_id = 0;
			}
		}

		LOGD("Total audio tracks = %d \n", player->selector[MM_PLAYER_TRACK_TYPE_AUDIO].total_track_num);

		attrs = MMPLAYER_GET_ATTRS(player);
		if ( attrs )
		{
			mm_attrs_set_int_by_name(attrs, "content_audio_track_num",(gint)player->selector[MM_PLAYER_TRACK_TYPE_AUDIO].total_track_num);
			if (mmf_attrs_commit (attrs))
				LOGE("failed to commit.\n");
		}
		else
		{
			LOGE("cannot get content attribute");
		}
	}
	else
	{
		if ((player->pipeline->audiobin) && (player->pipeline->audiobin[MMPLAYER_A_BIN].gst))
		{
			LOGD ("There is no audio track : remove audiobin");

			__mmplayer_release_signal_connection( player, MM_PLAYER_SIGNAL_TYPE_AUDIOBIN );
			__mmplayer_del_sink ( player, player->pipeline->audiobin[MMPLAYER_A_SINK].gst );

			MMPLAYER_RELEASE_ELEMENT ( player, player->pipeline->audiobin, MMPLAYER_A_BIN );
			MMPLAYER_FREEIF ( player->pipeline->audiobin )
		}

		if (player->num_dynamic_pad == 0)
		{
			__mmplayer_pipeline_complete (NULL, player);
		}
	}

	if (!MMPLAYER_IS_MS_BUFF_SRC(player))
	{
		if (text_selector)
		{
			__mmplayer_handle_text_decode_path(player, text_selector);
		}
	}

	MMPLAYER_FLEAVE();

ERROR:
	if (srcpad)
	{
		gst_object_unref ( GST_OBJECT(srcpad) );
		srcpad = NULL;
	}

	if (player->gapless.reconfigure)
	{
		player->gapless.reconfigure = FALSE;
		MMPLAYER_PLAYBACK_UNLOCK(player);
	}
}

static void
__mmplayer_gst_decode_callback(GstElement *elem, GstPad *pad, gpointer data) // @
{
	mm_player_t* player = NULL;
	MMHandleType attrs = 0;
	GstElement* pipeline = NULL;
	GstCaps* caps = NULL;
	gchar* caps_str = NULL;
	GstStructure* str = NULL;
	const gchar* name = NULL;
	GstPad* sinkpad = NULL;
	GstElement* sinkbin = NULL;
	gboolean reusing = FALSE;
	GstElement *text_selector = NULL;

	/* check handles */
	player = (mm_player_t*) data;

	MMPLAYER_RETURN_IF_FAIL( elem && pad );
	MMPLAYER_RETURN_IF_FAIL(player && player->pipeline && player->pipeline->mainbin);

	pipeline = player->pipeline->mainbin[MMPLAYER_M_PIPE].gst;

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		LOGE("cannot get content attribute\n");
		goto ERROR;
	}

	/* get mimetype from caps */
	caps = gst_pad_query_caps( pad, NULL );
	if ( !caps )
	{
		LOGE("cannot get caps from pad.\n");
		goto ERROR;
	}
	caps_str = gst_caps_to_string(caps);

	str = gst_caps_get_structure( caps, 0 );
	if ( ! str )
	{
		LOGE("cannot get structure from caps.\n");
		goto ERROR;
	}

	name = gst_structure_get_name(str);
	if ( ! name )
	{
		LOGE("cannot get mimetype from structure.\n");
		goto ERROR;
	}

	//LOGD("detected mimetype : %s\n", name);

	if (strstr(name, "audio"))
	{
		if (player->pipeline->audiobin == NULL)
		{
			if (MM_ERROR_NONE !=  __mmplayer_gst_create_audio_pipeline(player))
			{
				LOGE("failed to create audiobin. continuing without audio\n");
				goto ERROR;
			}

			sinkbin = player->pipeline->audiobin[MMPLAYER_A_BIN].gst;
			LOGD("creating audiosink bin success\n");
		}
		else
		{
			reusing = TRUE;
			sinkbin = player->pipeline->audiobin[MMPLAYER_A_BIN].gst;
			LOGD("reusing audiobin\n");
			_mmplayer_update_content_attrs( player, ATTR_AUDIO);
		}

		if (player->selector[MM_PLAYER_TRACK_TYPE_AUDIO].total_track_num <= 0) // should not update if content have multi audio tracks
			mm_attrs_set_int_by_name(attrs, "content_audio_track_num", 1);

		player->audiosink_linked  = 1;

		sinkpad = gst_element_get_static_pad( GST_ELEMENT(sinkbin), "sink" );
		if ( !sinkpad )
		{
			LOGE("failed to get pad from sinkbin\n");
			goto ERROR;
		}
	}
	else if (strstr(name, "video"))
	{
		if (strstr(caps_str, "ST12") || strstr(caps_str, "SN12"))
		{
			player->set_mode.video_zc = TRUE;
		}

		if (player->pipeline->videobin == NULL)
		{
			/* NOTE : not make videobin because application dose not want to play it even though file has video stream. */
			/* get video surface type */
			int surface_type = 0;
			int surface_client_type = 0;
			mm_attrs_get_int_by_name(player->attrs, "display_surface_type", &surface_type);
			mm_attrs_get_int_by_name(player->attrs, "display_surface_client_type", &surface_client_type);
			LOGD("display_surface_type : server(%d), client(%d)\n", surface_type, surface_client_type);

			if (surface_type == MM_DISPLAY_SURFACE_NULL)
			{
				LOGD("not make videobin because it dose not want\n");
				goto ERROR;
			}
			if (surface_client_type == MM_DISPLAY_SURFACE_X)
			{
				/* prepare resource manager for video overlay */
				if((_mmplayer_resource_manager_prepare(&player->resource_manager, RESOURCE_TYPE_VIDEO_OVERLAY)))
				{
					LOGE("could not prepare for video_overlay resource\n");
					goto ERROR;
				}
			}

			/* acquire resources for video playing */
			if((player->resource_manager.rset && _mmplayer_resource_manager_acquire(&player->resource_manager)))
			{
				LOGE("could not acquire resources for video playing\n");
				_mmplayer_resource_manager_unprepare(&player->resource_manager);
				goto ERROR;
			}

			if (MM_ERROR_NONE !=  __mmplayer_gst_create_video_pipeline(player, caps, surface_type) )
			{
				LOGE("failed to create videobin. continuing without video\n");
				goto ERROR;
			}

			sinkbin = player->pipeline->videobin[MMPLAYER_V_BIN].gst;
			LOGD("creating videosink bin success\n");
		}
		else
		{
			reusing = TRUE;
			sinkbin = player->pipeline->videobin[MMPLAYER_V_BIN].gst;
			LOGD("re-using videobin\n");
			_mmplayer_update_content_attrs( player, ATTR_VIDEO);
		}

		/* FIXIT : track number shouldn't be hardcoded */
		mm_attrs_set_int_by_name(attrs, "content_video_track_num", 1);
		player->videosink_linked  = 1;

		/* NOTE : intermediate code before doing H/W subtitle compositon */
		if ( player->use_textoverlay && player->play_subtitle )
		{
			LOGD("using textoverlay for external subtitle");
			/* check text bin has created well */
			if ( player->pipeline && player->pipeline->textbin )
			{
				/* get sinkpad from textoverlay */
				sinkpad = gst_element_get_static_pad(
					GST_ELEMENT(player->pipeline->textbin[MMPLAYER_T_BIN].gst),
					"video_sink" );
				if ( ! sinkpad )
				{
					LOGE("failed to get sink pad from textoverlay");
					goto ERROR;
				}

				/* link new pad with textoverlay first */
				if ( GST_PAD_LINK_OK != GST_PAD_LINK(pad, sinkpad) )
				{
					LOGE("failed to get pad from sinkbin\n");
					goto ERROR;
				}

				gst_object_unref(sinkpad);
				sinkpad = NULL;

				/* alright, override pad to textbin.src for futher link */
				pad = gst_element_get_static_pad(
					GST_ELEMENT(player->pipeline->textbin[MMPLAYER_T_BIN].gst),
					"src" );
				if ( ! pad )
				{
					LOGE("failed to get sink pad from textoverlay");
					goto ERROR;
				}
			}
			else
			{
				LOGE("should not reach here.");
				goto ERROR;
			}
		}

		sinkpad = gst_element_get_static_pad( GST_ELEMENT(sinkbin), "sink" );
		if ( !sinkpad )
		{
			LOGE("failed to get pad from sinkbin\n");
			goto ERROR;
		}
	}
	else if (strstr(name, "text"))
	{
		if (player->pipeline->textbin == NULL)
		{
			MMPlayerGstElement* mainbin = NULL;

			if (MM_ERROR_NONE !=  __mmplayer_gst_create_text_pipeline(player))
			{
				LOGE("failed to create textbin. continuing without text\n");
				goto ERROR;
			}

			sinkbin = player->pipeline->textbin[MMPLAYER_T_BIN].gst;
			LOGD("creating textsink bin success\n");

			/* FIXIT : track number shouldn't be hardcoded */
			mm_attrs_set_int_by_name(attrs, "content_text_track_num", 1);

			player->textsink_linked  = 1;
			LOGI("player->textsink_linked set to 1\n");

			sinkpad = gst_element_get_static_pad( GST_ELEMENT(sinkbin), "text_sink" );
			if ( !sinkpad )
			{
				LOGE("failed to get pad from sinkbin\n");
				goto ERROR;
			}

			mainbin = player->pipeline->mainbin;

			if (!mainbin[MMPLAYER_M_T_INPUT_SELECTOR].gst)
			{
			  /* input selector */
			  text_selector = gst_element_factory_make("input-selector", "subtitle_inselector");
			  if ( !text_selector )
			  {
			    LOGE ( "failed to create subtitle input selector element\n" );
			    goto ERROR;
			  }
			  g_object_set (text_selector, "sync-streams", TRUE, NULL);

			  mainbin[MMPLAYER_M_T_INPUT_SELECTOR].id = MMPLAYER_M_T_INPUT_SELECTOR;
			  mainbin[MMPLAYER_M_T_INPUT_SELECTOR].gst = text_selector;

			  /* warm up */
			  if (GST_STATE_CHANGE_FAILURE == gst_element_set_state (text_selector, GST_STATE_READY))
			  {
			    LOGE("failed to set state(READY) to sinkbin\n");
			    goto ERROR;
			  }

			  if (!gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), text_selector))
			  {
			    LOGW("failed to add subtitle input selector\n");
			    goto ERROR;
			  }

			  LOGD ("created element input-selector");

			}
			else
			{
			  LOGD ("already having subtitle input selector");
			  text_selector = mainbin[MMPLAYER_M_T_INPUT_SELECTOR].gst;
			}
		}

		else
		{
			if (!player->textsink_linked)
			{
				LOGD("re-using textbin\n");

				reusing = TRUE;
				sinkbin = player->pipeline->textbin[MMPLAYER_T_BIN].gst;

				player->textsink_linked  = 1;
				LOGI("player->textsink_linked set to 1\n");
			}
			else
			{
				LOGD("ignoring internal subtutle since external subtitle is available");
			}
		}
	}
	else
	{
		LOGW("unknown type of elementary stream! ignoring it...\n");
		goto ERROR;
	}

	if ( sinkbin )
	{
		if(!reusing)
		{
			/* warm up */
			if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state( sinkbin, GST_STATE_READY ) )
			{
				LOGE("failed to set state(READY) to sinkbin\n");
				goto ERROR;
			}

			/* Added for multi audio support to avoid adding audio bin again*/
			/* add */
			if ( FALSE == gst_bin_add( GST_BIN(pipeline), sinkbin ) )
			{
				LOGE("failed to add sinkbin to pipeline\n");
				goto ERROR;
			}
		}

		/* link */
		if (GST_PAD_LINK_OK != GST_PAD_LINK (pad, sinkpad))
		{
			LOGE("failed to get pad from sinkbin\n");
			goto ERROR;
		}

		if (!reusing)
		{
			/* run */
			if (GST_STATE_CHANGE_FAILURE == gst_element_set_state (sinkbin, GST_STATE_PAUSED))
			{
				LOGE("failed to set state(PAUSED) to sinkbin\n");
				goto ERROR;
			}

			if (text_selector)
			{
			  if (GST_STATE_CHANGE_FAILURE == gst_element_set_state (text_selector, GST_STATE_PAUSED))
			  {
			    LOGE("failed to set state(PAUSED) to sinkbin\n");
			    goto ERROR;
			  }
			}
		}

		gst_object_unref (sinkpad);
		sinkpad = NULL;
	}

	LOGD ("linking sink bin success\n");

	/* FIXIT : we cannot hold callback for 'no-more-pad' signal because signal was emitted in
 	 * streaming task. if the task blocked, then buffer will not flow to the next element
 	 * ( autoplugging element ). so this is special hack for streaming. please try to remove it
 	 */
	/* dec stream count. we can remove fakesink if it's zero */
	if (player->num_dynamic_pad)
		player->num_dynamic_pad--;

	LOGD ("no more pads: %d stream count dec : %d (num of dynamic pad)\n", player->no_more_pad, player->num_dynamic_pad);

	if ((player->no_more_pad) && (player->num_dynamic_pad == 0))
	{
		__mmplayer_pipeline_complete (NULL, player);
	}

	/* FIXIT : please leave a note why this code is needed */
	if(MMPLAYER_IS_WFD_STREAMING( player ))
	{
		player->no_more_pad = TRUE;
	}

ERROR:

	MMPLAYER_FREEIF(caps_str);

	if ( caps )
		gst_caps_unref( caps );

	if ( sinkpad )
		gst_object_unref(GST_OBJECT(sinkpad));

	/* flusing out new attributes */
	if (  mmf_attrs_commit ( attrs ) )
	{
		LOGE("failed to comit attributes\n");
	}

	return;
}

static gboolean
__mmplayer_get_property_value_for_rotation(mm_player_t* player, int rotation_angle, int *value)
{
	int pro_value = 0; // in the case of expection, default will be returned.
	int dest_angle = rotation_angle;
	int rotation_type = -1;

	MMPLAYER_RETURN_VAL_IF_FAIL(player, FALSE);
	MMPLAYER_RETURN_VAL_IF_FAIL(value, FALSE);
	MMPLAYER_RETURN_VAL_IF_FAIL(rotation_angle >= 0, FALSE);

	if (rotation_angle >= 360)
	{
		dest_angle = rotation_angle - 360;
	}

	/* chech if supported or not */
	if ( dest_angle % 90 )
	{
		LOGD("not supported rotation angle = %d", rotation_angle);
		return FALSE;
	}

	/*
	  * xvimagesink only 	 (A)
	  * custom_convert - no xv (e.g. memsink, evasimagesink	 (B)
	  * videoflip - avsysmemsink (C)
	  */
	if (player->set_mode.video_zc)
	{
		if (player->pipeline->videobin[MMPLAYER_V_CONV].gst) // B
		{
			rotation_type = ROTATION_USING_CUSTOM;
		}
		else // A
		{
			rotation_type = ROTATION_USING_SINK;
		}
	}
	else
	{
		int surface_type = 0;
		rotation_type = ROTATION_USING_FLIP;

		mm_attrs_get_int_by_name(player->attrs, "display_surface_type", &surface_type);
		LOGD("check display surface type attribute: %d", surface_type);

		if ((surface_type == MM_DISPLAY_SURFACE_X) ||
			(surface_type == MM_DISPLAY_SURFACE_EVAS && !strcmp(player->ini.videosink_element_evas, "evaspixmapsink")))
		{
			rotation_type = ROTATION_USING_SINK;
		}
		else
		{
			rotation_type = ROTATION_USING_FLIP; //C
		}

		LOGD("using %d type for rotation", rotation_type);
	}

	/* get property value for setting */
	switch(rotation_type)
	{
		case ROTATION_USING_SINK: // xvimagesink, pixmap
			{
				switch (dest_angle)
				{
					case 0:
						break;
					case 90:
						pro_value = 3; // clockwise 90
						break;
					case 180:
						pro_value = 2;
						break;
					case 270:
						pro_value = 1; // counter-clockwise 90
						break;
				}
			}
			break;
		case ROTATION_USING_CUSTOM:
			{
				gchar *ename = NULL;
				ename = GST_OBJECT_NAME(gst_element_get_factory(player->pipeline->videobin[MMPLAYER_V_CONV].gst));

				if (g_strrstr(ename, "fimcconvert"))
				{
					switch (dest_angle)
					{
						case 0:
							break;
						case 90:
							pro_value = 90; // clockwise 90
							break;
						case 180:
							pro_value = 180;
							break;
						case 270:
							pro_value = 270; // counter-clockwise 90
							break;
					}
				}
			}
			break;
		case ROTATION_USING_FLIP: // videoflip
			{
					switch (dest_angle)
					{

						case 0:
							break;
						case 90:
							pro_value = 1; // clockwise 90
							break;
						case 180:
							pro_value = 2;
							break;
						case 270:
							pro_value = 3; // counter-clockwise 90
							break;
					}
			}
			break;
	}

	LOGD("setting rotation property value : %d, used rotation type : %d", pro_value, rotation_type);

	*value = pro_value;

	return TRUE;
}

int
_mmplayer_update_video_param(mm_player_t* player) // @
{
	MMHandleType attrs = 0;
	int surface_type = 0;
	int org_angle = 0; // current supported angle values are 0, 90, 180, 270
	int user_angle = 0;
	int rotation_value = 0;

	MMPLAYER_FENTER();

	/* check video sinkbin is created */
	MMPLAYER_RETURN_VAL_IF_FAIL ( player &&
		player->pipeline &&
		player->pipeline->videobin &&
		player->pipeline->videobin[MMPLAYER_V_BIN].gst &&
		player->pipeline->videobin[MMPLAYER_V_SINK].gst,
		MM_ERROR_PLAYER_NOT_INITIALIZED );
	attrs = MMPLAYER_GET_ATTRS(player);

	if ( !attrs )
	{
		LOGE("cannot get content attribute");
		return MM_ERROR_PLAYER_INTERNAL;
	}
	__mmplayer_get_video_angle(player, &user_angle, &org_angle);

	/* check video stream callback is used */
	if(!player->set_mode.media_packet_video_stream && player->use_video_stream )
	{
		if (player->set_mode.video_zc)
		{
			gchar *ename = NULL;
			int width = 0;
			int height = 0;

			mm_attrs_get_int_by_name(attrs, "display_width", &width);
			mm_attrs_get_int_by_name(attrs, "display_height", &height);

			/* resize video frame with requested values for fimcconvert */
			ename = GST_OBJECT_NAME(gst_element_get_factory(player->pipeline->videobin[MMPLAYER_V_CONV].gst));

			if (ename && g_strrstr(ename, "fimcconvert"))
			{
				if (width)
					g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "dst-width", width, NULL);

				if (height)
					g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "dst-height", height, NULL);

				/* NOTE: fimcconvert does not manage index of src buffer from upstream src-plugin, decoder gives frame information in output buffer with no ordering */
				g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "src-rand-idx", TRUE, NULL);

				/* get rotation value to set */
				__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);

				g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "rotate", rotation_value, NULL);

				LOGD("updating fimcconvert - r[%d], w[%d], h[%d]", rotation_value, width, height);
			}
		}
		else
		{
			LOGD("using video stream callback with memsink. player handle : [%p]", player);

			/* get rotation value to set */
			__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);

			g_object_set(player->pipeline->videobin[MMPLAYER_V_FLIP].gst, "method", rotation_value, NULL);
		}

		return MM_ERROR_NONE;
	}

	/* update display surface */
	mm_attrs_get_int_by_name(attrs, "display_surface_type", &surface_type);
	LOGD("check display surface type attribute: %d", surface_type);

	/* configuring display */
	switch ( surface_type )
	{
		case MM_DISPLAY_SURFACE_X:
		{
			/* ximagesink or xvimagesink */
			void *surface = NULL;
			int display_method = 0;
			int roi_x = 0;
			int roi_y = 0;
			int roi_w = 0;
			int roi_h = 0;
			int src_crop_x = 0;
			int src_crop_y = 0;
			int src_crop_w = 0;
			int src_crop_h = 0;
			int force_aspect_ratio = 0;
			gboolean visible = TRUE;

#ifdef HAVE_WAYLAND
			/*set wl_display*/
			void* wl_display = NULL;
			GstContext *context = NULL;
			int wl_window_x = 0;
			int wl_window_y = 0;
			int wl_window_width = 0;
			int wl_window_height = 0;

			mm_attrs_get_data_by_name(attrs, "wl_display", &wl_display);
			if (wl_display)
				context = gst_wayland_display_handle_context_new(wl_display);
			if (context)
				gst_element_set_context(GST_ELEMENT(player->pipeline->videobin[MMPLAYER_V_SINK].gst), context);

			/*It should be set after setting window*/
			mm_attrs_get_int_by_name(attrs, "wl_window_render_x", &wl_window_x);
			mm_attrs_get_int_by_name(attrs, "wl_window_render_y", &wl_window_y);
			mm_attrs_get_int_by_name(attrs, "wl_window_render_width", &wl_window_width);
			mm_attrs_get_int_by_name(attrs, "wl_window_render_height", &wl_window_height);
#endif
			/* common case if using x surface */
			mm_attrs_get_data_by_name(attrs, "display_overlay", &surface);
			if ( surface )
			{
#ifdef HAVE_WAYLAND
				guintptr wl_surface = (guintptr)surface;
				LOGD("set video param : wayland surface %p", surface);
				gst_video_overlay_set_window_handle(
						GST_VIDEO_OVERLAY( player->pipeline->videobin[MMPLAYER_V_SINK].gst ),
						wl_surface );
				/* After setting window handle, set render	rectangle */
				gst_video_overlay_set_render_rectangle(
					 GST_VIDEO_OVERLAY( player->pipeline->videobin[MMPLAYER_V_SINK].gst ),
					 wl_window_x,wl_window_y,wl_window_width,wl_window_height);
#else // HAVE_X11
				int xwin_id = 0;
				xwin_id = *(int*)surface;
				LOGD("set video param : xid %p", *(int*)surface);
				if (xwin_id)
				{
					gst_video_overlay_set_window_handle( GST_VIDEO_OVERLAY( player->pipeline->videobin[MMPLAYER_V_SINK].gst ), *(int*)surface );
				}
#endif
			}
			else
			{
				/* FIXIT : is it error case? */
				LOGW("still we don't have xid on player attribute. create it's own surface.");
			}

			/* if xvimagesink */
			if (!strcmp(player->ini.videosink_element_x,"xvimagesink"))
			{
				mm_attrs_get_int_by_name(attrs, "display_force_aspect_ration", &force_aspect_ratio);
				mm_attrs_get_int_by_name(attrs, "display_method", &display_method);
				mm_attrs_get_int_by_name(attrs, "display_src_crop_x", &src_crop_x);
				mm_attrs_get_int_by_name(attrs, "display_src_crop_y", &src_crop_y);
				mm_attrs_get_int_by_name(attrs, "display_src_crop_width", &src_crop_w);
				mm_attrs_get_int_by_name(attrs, "display_src_crop_height", &src_crop_h);
				mm_attrs_get_int_by_name(attrs, "display_roi_x", &roi_x);
				mm_attrs_get_int_by_name(attrs, "display_roi_y", &roi_y);
				mm_attrs_get_int_by_name(attrs, "display_roi_width", &roi_w);
				mm_attrs_get_int_by_name(attrs, "display_roi_height", &roi_h);
				mm_attrs_get_int_by_name(attrs, "display_visible", &visible);
				#define DEFAULT_DISPLAY_MODE	2	// TV only, PRI_VIDEO_OFF_AND_SEC_VIDEO_FULL_SCREEN

				/* setting for cropping media source */
				if (src_crop_w && src_crop_h)
				{
					g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
						"src-crop-x", src_crop_x,
						"src-crop-y", src_crop_y,
						"src-crop-w", src_crop_w,
						"src-crop-h", src_crop_h,
						NULL );
				}

				/* setting for ROI mode */
				if (display_method == 5)	// 5 for ROI mode
				{
					int roi_mode = 0;
					mm_attrs_get_int_by_name(attrs, "display_roi_mode", &roi_mode);
					g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
						"dst-roi-mode", roi_mode,
						"dst-roi-x", roi_x,
						"dst-roi-y", roi_y,
						"dst-roi-w", roi_w,
						"dst-roi-h", roi_h,
						NULL );
					/* get rotation value to set,
					   do not use org_angle because ROI mode in xvimagesink needs both a rotation value and an orientation value */
					__mmplayer_get_property_value_for_rotation(player, user_angle, &rotation_value);
				}
				else
				{
					/* get rotation value to set */
					__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);
				}

				g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
					"force-aspect-ratio", force_aspect_ratio,
					"orientation", org_angle/90, // setting for orientation of media, it is used for ROI/ZOOM feature in xvimagesink
					"rotate", rotation_value,
					"handle-events", TRUE,
					"display-geometry-method", display_method,
					"draw-borders", FALSE,
					"handle-expose", FALSE,
					"visible", visible,
					"display-mode", DEFAULT_DISPLAY_MODE,
					NULL );

				LOGD("set video param : rotate %d, method %d visible %d", rotation_value, display_method, visible);
				LOGD("set video param : dst-roi-x: %d, dst-roi-y: %d, dst-roi-w: %d, dst-roi-h: %d", roi_x, roi_y, roi_w, roi_h );
				LOGD("set video param : force aspect ratio %d, display mode %d", force_aspect_ratio, DEFAULT_DISPLAY_MODE);
			}
		}
		break;
		case MM_DISPLAY_SURFACE_EVAS:
		{
			void *object = NULL;
			int scaling = 0;
			gboolean visible = TRUE;
			int display_method = 0;

			/* common case if using evas surface */
			mm_attrs_get_data_by_name(attrs, "display_overlay", &object);
			mm_attrs_get_int_by_name(attrs, "display_visible", &visible);
			mm_attrs_get_int_by_name(attrs, "display_evas_do_scaling", &scaling);
			mm_attrs_get_int_by_name(attrs, "display_method", &display_method);

			/* if evasimagesink */
			if (!strcmp(player->ini.videosink_element_evas,"evasimagesink"))
			{
				if (object)
				{
					/* if it is evasimagesink, we are not supporting rotation */
					if (user_angle != 0)
					{
						mm_attrs_set_int_by_name(attrs, "display_rotation", MM_DISPLAY_ROTATION_NONE);
						if (mmf_attrs_commit (attrs)) /* return -1 if error */
							LOGE("failed to commit\n");
						LOGW("unsupported feature");
						return MM_ERROR_NOT_SUPPORT_API;
					}
					__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);
					g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
							"evas-object", object,
							"visible", visible,
							"display-geometry-method", display_method,
							"rotate", rotation_value,
							NULL);
					LOGD("set video param : method %d", display_method);
					LOGD("set video param : evas-object %x, visible %d", object, visible);
					LOGD("set video param : evas-object %x, rotate %d", object, rotation_value);
				}
				else
				{
					LOGE("no evas object");
					return MM_ERROR_PLAYER_INTERNAL;
				}


				/* if evasimagesink using converter */
				if (player->set_mode.video_zc && player->pipeline->videobin[MMPLAYER_V_CONV].gst)
				{
					int width = 0;
					int height = 0;
					int no_scaling = !scaling;

					mm_attrs_get_int_by_name(attrs, "display_width", &width);
					mm_attrs_get_int_by_name(attrs, "display_height", &height);

					/* NOTE: fimcconvert does not manage index of src buffer from upstream src-plugin, decoder gives frame information in output buffer with no ordering */
					g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "src-rand-idx", TRUE, NULL);
					g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "dst-buffer-num", 5, NULL);

					if (no_scaling)
					{
						/* no-scaling order to fimcconvert, original width, height size of media src will be passed to sink plugin */
						g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst,
								"dst-width", 0, /* setting 0, output video width will be media src's width */
								"dst-height", 0, /* setting 0, output video height will be media src's height */
								NULL);
					}
					else
					{
						/* scaling order to fimcconvert */
						if (width)
						{
							g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "dst-width", width, NULL);
						}
						if (height)
						{
							g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "dst-height", height, NULL);
						}
						LOGD("set video param : video frame scaling down to width(%d) height(%d)", width, height);
					}
					LOGD("set video param : display_evas_do_scaling %d", scaling);
				}
			}

			/* if evaspixmapsink */
			if (!strcmp(player->ini.videosink_element_evas,"evaspixmapsink"))
			{
				if (object)
				{
					__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);
					g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
							"evas-object", object,
							"visible", visible,
							"display-geometry-method", display_method,
							"rotate", rotation_value,
							NULL);
					LOGD("set video param : method %d", display_method);
					LOGD("set video param : evas-object %x, visible %d", object, visible);
					LOGD("set video param : evas-object %x, rotate %d", object, rotation_value);
				}
				else
				{
					LOGE("no evas object");
					return MM_ERROR_PLAYER_INTERNAL;
				}

				int display_method = 0;
				int roi_x = 0;
				int roi_y = 0;
				int roi_w = 0;
				int roi_h = 0;
				int origin_size = !scaling;

				mm_attrs_get_int_by_name(attrs, "display_method", &display_method);
				mm_attrs_get_int_by_name(attrs, "display_roi_x", &roi_x);
				mm_attrs_get_int_by_name(attrs, "display_roi_y", &roi_y);
				mm_attrs_get_int_by_name(attrs, "display_roi_width", &roi_w);
				mm_attrs_get_int_by_name(attrs, "display_roi_height", &roi_h);

				/* get rotation value to set */
				__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);

				g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
					"origin-size", origin_size,
					"rotate", rotation_value,
					"dst-roi-x", roi_x,
					"dst-roi-y", roi_y,
					"dst-roi-w", roi_w,
					"dst-roi-h", roi_h,
					"display-geometry-method", display_method,
					NULL );

				LOGD("set video param : method %d", display_method);
				LOGD("set video param : dst-roi-x: %d, dst-roi-y: %d, dst-roi-w: %d, dst-roi-h: %d",
								roi_x, roi_y, roi_w, roi_h );
				LOGD("set video param : display_evas_do_scaling %d (origin-size %d)", scaling, origin_size);
			}
		}
		break;
		case MM_DISPLAY_SURFACE_X_EXT:	/* NOTE : this surface type is used for the videoTexture(canvasTexture) overlay */
		{
			void *pixmap_id_cb = NULL;
			void *pixmap_id_cb_user_data = NULL;
			int display_method = 0;
			gboolean visible = TRUE;

			/* if xvimagesink */
			if (strcmp(player->ini.videosink_element_x,"xvimagesink"))
			{
				LOGE("videosink is not xvimagesink");
				return MM_ERROR_PLAYER_INTERNAL;
			}

			/* get information from attributes */
			mm_attrs_get_data_by_name(attrs, "display_overlay", &pixmap_id_cb);
			mm_attrs_get_data_by_name(attrs, "display_overlay_user_data", &pixmap_id_cb_user_data);
			mm_attrs_get_int_by_name(attrs, "display_method", &display_method);
			mm_attrs_get_int_by_name(attrs, "display_visible", &visible);

			if ( pixmap_id_cb )
			{
				LOGD("set video param : display_overlay(0x%x)", pixmap_id_cb);
				if (pixmap_id_cb_user_data)
				{
					LOGD("set video param : display_overlay_user_data(0x%x)", pixmap_id_cb_user_data);
				}
			}
			else
			{
				LOGE("failed to set pixmap-id-callback");
				return MM_ERROR_PLAYER_INTERNAL;
			}
			/* get rotation value to set */
			__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);

			LOGD("set video param : rotate %d, method %d, visible %d", rotation_value, display_method, visible);

			/* set properties of videosink plugin */
			g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
				"display-geometry-method", display_method,
				"draw-borders", FALSE,
				"visible", visible,
				"rotate", rotation_value,
				"pixmap-id-callback", pixmap_id_cb,
				"pixmap-id-callback-userdata", pixmap_id_cb_user_data,
				NULL );
		}
		break;
		case MM_DISPLAY_SURFACE_NULL:
		{
			/* do nothing */
		}
		break;
		case MM_DISPLAY_SURFACE_REMOTE:
		{
			/* do nothing */
		}
		break;
	}

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

static int
__mmplayer_gst_element_link_bucket(GList* element_bucket) // @
{
	GList* bucket = element_bucket;
	MMPlayerGstElement* element = NULL;
	MMPlayerGstElement* prv_element = NULL;
	gint successful_link_count = 0;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL(element_bucket, -1);

	prv_element = (MMPlayerGstElement*)bucket->data;
	bucket = bucket->next;

	for ( ; bucket; bucket = bucket->next )
	{
		element = (MMPlayerGstElement*)bucket->data;

		if ( element && element->gst )
		{
			/* If next element is audio appsrc then make a seprate audio pipeline */
			if (!strcmp(GST_ELEMENT_NAME(GST_ELEMENT(element->gst)),"audio_appsrc") ||
				!strcmp(GST_ELEMENT_NAME(GST_ELEMENT(element->gst)),"subtitle_appsrc"))
			{
				prv_element = element;
				continue;
			}

			if ( GST_ELEMENT_LINK(GST_ELEMENT(prv_element->gst), GST_ELEMENT(element->gst)) )
			{
				LOGD("linking [%s] to [%s] success\n",
					GST_ELEMENT_NAME(GST_ELEMENT(prv_element->gst)),
					GST_ELEMENT_NAME(GST_ELEMENT(element->gst)) );
				successful_link_count ++;
			}
			else
			{
				LOGD("linking [%s] to [%s] failed\n",
					GST_ELEMENT_NAME(GST_ELEMENT(prv_element->gst)),
					GST_ELEMENT_NAME(GST_ELEMENT(element->gst)) );
				return -1;
			}
		}

		prv_element = element;
	}

	MMPLAYER_FLEAVE();

	return successful_link_count;
}

static int
__mmplayer_gst_element_add_bucket_to_bin(GstBin* bin, GList* element_bucket) // @
{
	GList* bucket = element_bucket;
	MMPlayerGstElement* element = NULL;
	int successful_add_count = 0;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL(element_bucket, 0);
	MMPLAYER_RETURN_VAL_IF_FAIL(bin, 0);

	for ( ; bucket; bucket = bucket->next )
	{
		element = (MMPlayerGstElement*)bucket->data;

		if ( element && element->gst )
		{
			if( !gst_bin_add(bin, GST_ELEMENT(element->gst)) )
			{
				LOGD("__mmplayer_gst_element_link_bucket : Adding element [%s]  to bin [%s] failed\n",
					GST_ELEMENT_NAME(GST_ELEMENT(element->gst)),
					GST_ELEMENT_NAME(GST_ELEMENT(bin) ) );
				return 0;
			}
			successful_add_count ++;
		}
	}

	MMPLAYER_FLEAVE();

	return successful_add_count;
}

static void __mmplayer_gst_caps_notify_cb (GstPad * pad, GParamSpec * unused, gpointer data)
{
	mm_player_t* player = (mm_player_t*) data;
	GstCaps *caps = NULL;
	GstStructure *str = NULL;
	const char *name;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL ( pad )
	MMPLAYER_RETURN_IF_FAIL ( unused )
	MMPLAYER_RETURN_IF_FAIL ( data )

	caps = gst_pad_get_current_caps( pad );
	if ( !caps )
	{
		return;
	}

	str = gst_caps_get_structure(caps, 0);
	if ( !str )
	{
		goto ERROR;
	}

	name = gst_structure_get_name(str);
	if ( !name )
	{
		goto ERROR;
	}

	LOGD("name = %s\n", name);

	if (strstr(name, "audio"))
	{
		_mmplayer_update_content_attrs (player, ATTR_AUDIO);

		if (player->audio_stream_changed_cb)
		{
			LOGE("call the audio stream changed cb\n");
			player->audio_stream_changed_cb(player->audio_stream_changed_cb_user_param);
		}
	}
	else if (strstr(name, "video"))
	{
		_mmplayer_update_content_attrs (player, ATTR_VIDEO);

		if (player->video_stream_changed_cb)
		{
			LOGE("call the video stream changed cb\n");
			player->video_stream_changed_cb(player->video_stream_changed_cb_user_param);
		}
	}
	else
	{
		goto ERROR;
	}

ERROR:

	gst_caps_unref(caps);

	MMPLAYER_FLEAVE();

	return;
}



/**
 * This function is to create audio pipeline for playing.
 *
 * @param	player		[in]	handle of player
 *
 * @return	This function returns zero on success.
 * @remark
 * @see		__mmplayer_gst_create_midi_pipeline, __mmplayer_gst_create_video_pipeline
 */
#define MMPLAYER_CREATEONLY_ELEMENT(x_bin, x_id, x_factory, x_name) \
x_bin[x_id].id = x_id;\
x_bin[x_id].gst = gst_element_factory_make(x_factory, x_name);\
if ( ! x_bin[x_id].gst )\
{\
	LOGE("failed to create %s \n", x_factory);\
	goto ERROR;\
}\

#define MMPLAYER_CREATE_ELEMENT_ADD_BIN(x_bin, x_id, x_factory, x_name, y_bin, x_player) \
x_bin[x_id].id = x_id;\
x_bin[x_id].gst = gst_element_factory_make(x_factory, x_name);\
if ( ! x_bin[x_id].gst )\
{\
	LOGE("failed to create %s \n", x_factory);\
	goto ERROR;\
}\
else\
{\
	if (x_player->ini.set_dump_element_flag)\
		__mmplayer_add_dump_buffer_probe(x_player, x_bin[x_id].gst);\
}\
if( !gst_bin_add(GST_BIN(y_bin), GST_ELEMENT(x_bin[x_id].gst)))\
{\
	LOGD("__mmplayer_gst_element_link_bucket : Adding element [%s]  to bin [%s] failed\n",\
		GST_ELEMENT_NAME(GST_ELEMENT(x_bin[x_id].gst)),\
		GST_ELEMENT_NAME(GST_ELEMENT(y_bin) ) );\
	goto ERROR;\
}\

/* macro for code readability. just for sinkbin-creation functions */
#define MMPLAYER_CREATE_ELEMENT(x_bin, x_id, x_factory, x_name, x_add_bucket, x_player) \
do \
{ \
	x_bin[x_id].id = x_id;\
	x_bin[x_id].gst = gst_element_factory_make(x_factory, x_name);\
	if ( ! x_bin[x_id].gst )\
	{\
		LOGE("failed to create %s \n", x_factory);\
		goto ERROR;\
	}\
	else\
	{\
		if (x_player->ini.set_dump_element_flag)\
			__mmplayer_add_dump_buffer_probe(x_player, x_bin[x_id].gst);\
	}\
	if ( x_add_bucket )\
		element_bucket = g_list_append(element_bucket, &x_bin[x_id]);\
} while(0);

static void
__mmplayer_audio_stream_decoded_render_cb(GstElement* object, GstBuffer *buffer, GstPad *pad, gpointer data)
{
	mm_player_t* player = (mm_player_t*) data;

	gint channel = 0;
	gint rate = 0;
	gint depth = 0;
	gint endianness = 0;
	guint64 channel_mask = 0;

	MMPlayerAudioStreamDataType audio_stream = { 0, };
	GstMapInfo mapinfo = GST_MAP_INFO_INIT;

	MMPLAYER_FENTER();
	MMPLAYER_RETURN_IF_FAIL(player->audio_stream_render_cb_ex);

	LOGD ("__mmplayer_audio_stream_decoded_render_cb new pad: %s", GST_PAD_NAME (pad));

	gst_buffer_map(buffer, &mapinfo, GST_MAP_READ);
	audio_stream.data = mapinfo.data;
	audio_stream.data_size = mapinfo.size;

	GstCaps *caps = gst_pad_get_current_caps( pad );
	GstStructure *structure = gst_caps_get_structure (caps, 0);

	MMPLAYER_LOG_GST_CAPS_TYPE(caps);
	gst_structure_get_int (structure, "rate", &rate);
	gst_structure_get_int (structure, "channels", &channel);
	gst_structure_get_int (structure, "depth", &depth);
	gst_structure_get_int (structure, "endianness", &endianness);
	gst_structure_get (structure, "channel-mask", GST_TYPE_BITMASK, &channel_mask, NULL);

	gst_caps_unref(GST_CAPS(caps));

	audio_stream.bitrate = rate;
	audio_stream.channel = channel;
	audio_stream.depth = depth;
	audio_stream.is_little_endian = (endianness == 1234 ? 1 : 0);
	audio_stream.channel_mask = channel_mask;
	LOGD ("bitrate : %d channel : %d depth: %d ls_little_endian : %d channel_mask: %d, %p", rate, channel, depth, endianness, channel_mask, player->audio_stream_cb_user_param);
	player->audio_stream_render_cb_ex(&audio_stream, player->audio_stream_cb_user_param);
	gst_buffer_unmap(buffer, &mapinfo);

	MMPLAYER_FLEAVE();
}

static void
__mmplayer_gst_audio_deinterleave_pad_added (GstElement *elem, GstPad *pad, gpointer data)
{
	mm_player_t* player = (mm_player_t*)data;
	MMPlayerGstElement* audiobin = player->pipeline->audiobin;
	GstPad* sinkpad = NULL;
	GstElement *queue = NULL, *sink = NULL;

	MMPLAYER_FENTER();
	MMPLAYER_RETURN_IF_FAIL (player && player->pipeline && player->pipeline->mainbin);

	queue = gst_element_factory_make ("queue", NULL);
	if (queue == NULL)
	{
		LOGD ("fail make queue\n");
		goto ERROR;
	}

	sink = gst_element_factory_make ("fakesink", NULL);
	if (sink == NULL)
	{
		LOGD ("fail make fakesink\n");
		goto ERROR;
	}

	gst_bin_add_many (GST_BIN(audiobin[MMPLAYER_A_BIN].gst), queue, sink, NULL);

	if (!gst_element_link_pads_full (queue, "src", sink, "sink", GST_PAD_LINK_CHECK_NOTHING))
	{
		LOGW("failed to link queue & sink\n");
		goto ERROR;
	}

	sinkpad = gst_element_get_static_pad (queue, "sink");

	if (GST_PAD_LINK_OK != gst_pad_link(pad, sinkpad))
	{
		LOGW ("failed to link [%s:%s] to queue\n", GST_DEBUG_PAD_NAME(pad));
		goto ERROR;
	}

	LOGE("player->audio_stream_sink_sync: %d\n", player->audio_stream_sink_sync);

	gst_object_unref (sinkpad);
	g_object_set (sink, "sync", player->audio_stream_sink_sync, NULL);
	g_object_set (sink, "signal-handoffs", TRUE, NULL);

	gst_element_set_state (sink, GST_STATE_PAUSED);
	gst_element_set_state (queue, GST_STATE_PAUSED);

	MMPLAYER_SIGNAL_CONNECT( player,
		G_OBJECT(sink),
		MM_PLAYER_SIGNAL_TYPE_AUDIOBIN,
		"handoff",
		G_CALLBACK(__mmplayer_audio_stream_decoded_render_cb),
		(gpointer)player );

	MMPLAYER_FLEAVE();
	return ;

ERROR:
	LOGE("__mmplayer_gst_audio_deinterleave_pad_added ERROR\n");
	if (queue)
	{
		gst_object_unref(GST_OBJECT(queue));
		queue = NULL;
	}
	if (sink)
	{
		gst_object_unref(GST_OBJECT(sink));
		sink = NULL;
	}
	if (sinkpad)
	{
		gst_object_unref ( GST_OBJECT(sinkpad) );
		sinkpad = NULL;
	}

	return;
}

void __mmplayer_gst_set_audiosink_property(mm_player_t* player, MMHandleType attrs)
{
	#define MAX_PROPS_LEN 64
	gint volume_type = 0;
	gint latency_mode = 0;
	gchar *stream_type = NULL;
	gchar *latency = NULL;
	gint stream_id = 0;
	gchar stream_props[MAX_PROPS_LEN] = {0,};
	GstStructure *props = NULL;

	/* set volume table
	 * It should be set after player creation through attribute.
	 * But, it can not be changed during playing.
	 */
	MMPLAYER_FENTER();
	mm_attrs_get_int_by_name(attrs, "sound_stream_index", &stream_id);
	mm_attrs_get_string_by_name (attrs, "sound_stream_type", &stream_type );

	if (!stream_type)
	{
		LOGE("stream_type is null.\n");
	}
	else
	{
		snprintf(stream_props, sizeof(stream_props)-1, "props,media.role=%s, media.parent_id=%d", stream_type, stream_id);
		props = gst_structure_from_string(stream_props, NULL);
		g_object_set(player->pipeline->audiobin[MMPLAYER_A_SINK].gst, "stream-properties", props, NULL);
		LOGD("stream_id[%d], stream_type[%s], result[%s].\n", stream_id, stream_type, stream_props);
	}

	mm_attrs_get_int_by_name(attrs, "sound_latency_mode", &latency_mode);
	mm_attrs_get_int_by_name(attrs, "sound_volume_type", &volume_type);

	switch (latency_mode)
	{
		case AUDIO_LATENCY_MODE_LOW:
			latency = g_strndup("low", 3);
			break;
		case AUDIO_LATENCY_MODE_MID:
			latency = g_strndup("mid", 3);
			break;
		case AUDIO_LATENCY_MODE_HIGH:
			latency = g_strndup("high", 4);
			break;
	};

	/* hook sound_type if emergency case */
	if (player->sound_focus.focus_changed_msg == MM_PLAYER_FOCUS_CHANGED_BY_EMERGENCY)
	{
		LOGD ("emergency session, hook sound_type from [%d] to [%d]\n", volume_type, MM_SOUND_VOLUME_TYPE_EMERGENCY);
		volume_type = MM_SOUND_VOLUME_TYPE_EMERGENCY;
	}
#if 0 //need to check
	if (player->sound_focus.user_route_policy != 0)
	{
		route_path = player->sound_focus.user_route_policy;
	}

	g_object_set(player->pipeline->audiobin[MMPLAYER_A_SINK].gst,
			"latency", latency_mode,
			NULL);

	LOGD("audiosink property status...volume type:%d, user-route=%d, latency=%d \n",
		volume_type, route_path, latency_mode);
	MMPLAYER_FLEAVE();

#endif

	g_object_set(player->pipeline->audiobin[MMPLAYER_A_SINK].gst,
			"latency", latency,
			NULL);

	LOGD("audiosink property - volume type=%d, latency=%s \n",
		volume_type, latency);

	g_free(latency);

	MMPLAYER_FLEAVE();
}

static int
__mmplayer_gst_create_audio_pipeline(mm_player_t* player)
{
	MMPlayerGstElement* first_element = NULL;
	MMPlayerGstElement* audiobin = NULL;
	MMHandleType attrs = 0;
	GstPad *pad = NULL;
	GstPad *ghostpad = NULL;
	GList* element_bucket = NULL;
	gboolean link_audio_sink_now = TRUE;
	int i =0;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* alloc handles */
	audiobin = (MMPlayerGstElement*)g_malloc0(sizeof(MMPlayerGstElement) * MMPLAYER_A_NUM);
	if ( ! audiobin )
	{
		LOGE("failed to allocate memory for audiobin\n");
		return MM_ERROR_PLAYER_NO_FREE_SPACE;
	}

	attrs = MMPLAYER_GET_ATTRS(player);

	/* create bin */
	audiobin[MMPLAYER_A_BIN].id = MMPLAYER_A_BIN;
	audiobin[MMPLAYER_A_BIN].gst = gst_bin_new("audiobin");
	if ( !audiobin[MMPLAYER_A_BIN].gst )
	{
		LOGE("failed to create audiobin\n");
		goto ERROR;
	}

	/* take it */
	player->pipeline->audiobin = audiobin;

	player->set_mode.pcm_extraction = __mmplayer_can_extract_pcm(player);

	/* Adding audiotp plugin for reverse trickplay feature */
//	MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_TP, "audiotp", "audio trickplay", TRUE, player);

	/* converter */
	MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_CONV, "audioconvert", "audio converter", TRUE, player);

	/* resampler */
	MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_RESAMPLER,  player->ini.audioresampler_element, "audio resampler", TRUE, player);

	if (player->set_mode.pcm_extraction) // pcm extraction only and no sound output
	{
		if(player->audio_stream_render_cb_ex)
		{
			char *caps_str = NULL;
			GstCaps* caps = NULL;
			gchar *format = NULL;

			/* capsfilter */
			MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_CAPS_DEFAULT, "capsfilter", "audio capsfilter", TRUE, player);

			mm_attrs_get_string_by_name (player->attrs, "pcm_audioformat", &format );

			LOGD("contents : format: %s samplerate : %d pcm_channel: %d", format, player->pcm_samplerate, player->pcm_channel);

			caps = gst_caps_new_simple ("audio/x-raw",
					"format", G_TYPE_STRING, format,
					"rate", G_TYPE_INT, player->pcm_samplerate,
					"channels", G_TYPE_INT, player->pcm_channel,
					NULL);
			caps_str = gst_caps_to_string(caps);
			LOGD("new caps : %s\n", caps_str);

			g_object_set (GST_ELEMENT(audiobin[MMPLAYER_A_CAPS_DEFAULT].gst), "caps", caps, NULL );

			/* clean */
			gst_caps_unref( caps );
			MMPLAYER_FREEIF( caps_str );

			MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_DEINTERLEAVE, "deinterleave", "deinterleave", TRUE, player);

			g_object_set (G_OBJECT (audiobin[MMPLAYER_A_DEINTERLEAVE].gst), "keep-positions", TRUE, NULL);
			/* raw pad handling signal */
			MMPLAYER_SIGNAL_CONNECT( player,
				(audiobin[MMPLAYER_A_DEINTERLEAVE].gst),
				MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "pad-added",
												G_CALLBACK(__mmplayer_gst_audio_deinterleave_pad_added), player);
		}
		else
		{
			int dst_samplerate = 0;
			int dst_channels = 0;
			int dst_depth = 0;
			char *caps_str = NULL;
			GstCaps* caps = NULL;

			/* get conf. values */
			mm_attrs_multiple_get(player->attrs,
						NULL,
						"pcm_extraction_samplerate", &dst_samplerate,
						"pcm_extraction_channels", &dst_channels,
						"pcm_extraction_depth", &dst_depth,
						NULL);

			/* capsfilter */
			MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_CAPS_DEFAULT, "capsfilter", "audio capsfilter", TRUE, player);
			caps = gst_caps_new_simple ("audio/x-raw",
					"rate", G_TYPE_INT, dst_samplerate,
					"channels", G_TYPE_INT, dst_channels,
					"depth", G_TYPE_INT, dst_depth,
					NULL);
			caps_str = gst_caps_to_string(caps);
			LOGD("new caps : %s\n", caps_str);

			g_object_set (GST_ELEMENT(audiobin[MMPLAYER_A_CAPS_DEFAULT].gst), "caps", caps, NULL );

			/* clean */
			gst_caps_unref( caps );
			MMPLAYER_FREEIF( caps_str );

			/* fake sink */
			MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_SINK, "fakesink", "fakesink", TRUE, player);

			/* set sync */
			g_object_set (G_OBJECT (audiobin[MMPLAYER_A_SINK].gst), "sync", FALSE, NULL);
		}
	}
	else // normal playback
	{
		//GstCaps* caps = NULL;
		gint channels = 0;

		/* for logical volume control */
		MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_VOL, "volume", "volume", TRUE, player);
		g_object_set(G_OBJECT (audiobin[MMPLAYER_A_VOL].gst), "volume", player->sound.volume, NULL);

		if (player->sound.mute)
		{
			LOGD("mute enabled\n");
			g_object_set(G_OBJECT (audiobin[MMPLAYER_A_VOL].gst), "mute", player->sound.mute, NULL);
		}

#if 0
		/*capsfilter */
		MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_CAPS_DEFAULT, "capsfilter", "audiocapsfilter", TRUE, player);
		caps = gst_caps_from_string( "audio/x-raw-int, "
					"endianness = (int) LITTLE_ENDIAN, "
					"signed = (boolean) true, "
					"width = (int) 16, "
					"depth = (int) 16" );
		g_object_set (GST_ELEMENT(audiobin[MMPLAYER_A_CAPS_DEFAULT].gst), "caps", caps, NULL );
		gst_caps_unref( caps );
#endif

		/* chech if multi-chennels */
		if (player->pipeline->mainbin && player->pipeline->mainbin[MMPLAYER_M_DEMUX].gst)
		{
			GstPad *srcpad = NULL;
			GstCaps *caps = NULL;

			if ((srcpad = gst_element_get_static_pad(player->pipeline->mainbin[MMPLAYER_M_DEMUX].gst, "src")))
			{
				if ((caps = gst_pad_query_caps(srcpad, NULL)))
				{
					//MMPLAYER_LOG_GST_CAPS_TYPE(caps);
					GstStructure *str = gst_caps_get_structure(caps, 0);
					if (str)
						gst_structure_get_int (str, "channels", &channels);
					gst_caps_unref(caps);
				}
				gst_object_unref(srcpad);
			}
		}

		/* audio effect element. if audio effect is enabled */
		if ( (strcmp(player->ini.audioeffect_element, ""))
			&& (channels <= 2)
			&& (player->ini.use_audio_effect_preset || player->ini.use_audio_effect_custom))
		{
			MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_FILTER, player->ini.audioeffect_element, "audio effect filter", TRUE, player);

			LOGD("audio effect config. bypass = %d, effect type  = %d", player->bypass_audio_effect, player->audio_effect_info.effect_type);

			if ( (!player->bypass_audio_effect)
				&& (player->ini.use_audio_effect_preset || player->ini.use_audio_effect_custom) )
			{
				if ( MM_AUDIO_EFFECT_TYPE_CUSTOM == player->audio_effect_info.effect_type )
				{
					if (!_mmplayer_audio_effect_custom_apply(player))
					{
						LOGI("apply audio effect(custom) setting success\n");
					}
				}
			}

			if ( (strcmp(player->ini.audioeffect_element_custom, ""))
				&& (player->set_mode.rich_audio) )
			{
				MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_FILTER_SEC, player->ini.audioeffect_element_custom, "audio effect filter custom", TRUE, player);
			}
		}
		if (!MMPLAYER_IS_RTSP_STREAMING(player))
		{
			if (player->set_mode.rich_audio && channels <= 2)
				MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_VSP, "audiovsp", "x-speed", TRUE, player);
		}

		/* create audio sink */
		MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_SINK, player->ini.audiosink_element, "audiosink", link_audio_sink_now, player);

		/* qos on */
		g_object_set (G_OBJECT (audiobin[MMPLAYER_A_SINK].gst), "qos", TRUE, NULL); 	/* qos on */
		g_object_set (G_OBJECT (audiobin[MMPLAYER_A_SINK].gst), "slave-method", GST_AUDIO_BASE_SINK_SLAVE_NONE, NULL);

		if (player->videodec_linked && player->ini.use_system_clock)
		{
			LOGD("system clock will be used.\n");
			g_object_set (G_OBJECT (audiobin[MMPLAYER_A_SINK].gst), "provide-clock", FALSE,  NULL);
		}

		if (g_strrstr(player->ini.audiosink_element, "pulsesink"))
			__mmplayer_gst_set_audiosink_property(player, attrs);
	}

	if (audiobin[MMPLAYER_A_SINK].gst)
	{
		GstPad *sink_pad = NULL;
		sink_pad = gst_element_get_static_pad(audiobin[MMPLAYER_A_SINK].gst, "sink");
		MMPLAYER_SIGNAL_CONNECT (player, sink_pad, MM_PLAYER_SIGNAL_TYPE_AUDIOBIN,
					"notify::caps", G_CALLBACK(__mmplayer_gst_caps_notify_cb), player);
		gst_object_unref (GST_OBJECT(sink_pad));
	}

	__mmplayer_add_sink( player, audiobin[MMPLAYER_A_SINK].gst );

	/* adding created elements to bin */
	LOGD("adding created elements to bin\n");
	if( !__mmplayer_gst_element_add_bucket_to_bin( GST_BIN(audiobin[MMPLAYER_A_BIN].gst), element_bucket ))
	{
		LOGE("failed to add elements\n");
		goto ERROR;
	}

	/* linking elements in the bucket by added order. */
	LOGD("Linking elements in the bucket by added order.\n");
	if ( __mmplayer_gst_element_link_bucket(element_bucket) == -1 )
	{
		LOGE("failed to link elements\n");
		goto ERROR;
	}

	/* get first element's sinkpad for creating ghostpad */
	first_element = (MMPlayerGstElement *)element_bucket->data;

	pad = gst_element_get_static_pad(GST_ELEMENT(first_element->gst), "sink");
	if ( ! pad )
	{
		LOGE("failed to get pad from first element of audiobin\n");
		goto ERROR;
	}

	ghostpad = gst_ghost_pad_new("sink", pad);
	if ( ! ghostpad )
	{
		LOGE("failed to create ghostpad\n");
		goto ERROR;
	}

	if ( FALSE == gst_element_add_pad(audiobin[MMPLAYER_A_BIN].gst, ghostpad) )
	{
		LOGE("failed to add ghostpad to audiobin\n");
		goto ERROR;
	}

	player->gapless.audio_data_probe_id = gst_pad_add_probe(ghostpad, GST_PAD_PROBE_TYPE_BUFFER,
			__mmplayer_gapless_sinkbin_data_probe, player, NULL);

	gst_object_unref(pad);

	g_list_free(element_bucket);

	mm_attrs_set_int_by_name(attrs, "content_audio_found", TRUE);

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;

ERROR:

	LOGD("ERROR : releasing audiobin\n");

	if ( pad )
		gst_object_unref(GST_OBJECT(pad));

	if ( ghostpad )
		gst_object_unref(GST_OBJECT(ghostpad));

	g_list_free( element_bucket );

	/* release element which are not added to bin */
	for ( i = 1; i < MMPLAYER_A_NUM; i++ ) 	/* NOTE : skip bin */
	{
		if ( audiobin[i].gst )
		{
			GstObject* parent = NULL;
			parent = gst_element_get_parent( audiobin[i].gst );

			if ( !parent )
			{
				gst_object_unref(GST_OBJECT(audiobin[i].gst));
				audiobin[i].gst = NULL;
			}
			else
			{
				gst_object_unref(GST_OBJECT(parent));
			}
		}
	}

	/* release audiobin with it's childs */
	if ( audiobin[MMPLAYER_A_BIN].gst )
	{
		gst_object_unref(GST_OBJECT(audiobin[MMPLAYER_A_BIN].gst));
	}

	MMPLAYER_FREEIF( audiobin );

	player->pipeline->audiobin = NULL;

	return MM_ERROR_PLAYER_INTERNAL;
}

static GstPadProbeReturn
__mmplayer_audio_stream_probe (GstPad *pad, GstPadProbeInfo *info, gpointer u_data)
{
	mm_player_t* player = (mm_player_t*) u_data;
	GstBuffer *pad_buffer = gst_pad_probe_info_get_buffer(info);
	GstMapInfo probe_info = GST_MAP_INFO_INIT;

	gst_buffer_map(pad_buffer, &probe_info, GST_MAP_READ);

	if (player->audio_stream_cb && probe_info.size && probe_info.data)
		player->audio_stream_cb((void *)probe_info.data, probe_info.size, player->audio_stream_cb_user_param);

	return GST_PAD_PROBE_OK;
}

static guint32 _mmplayer_convert_fourcc_string_to_value(const gchar* format_name)
{
    return format_name[0] | (format_name[1] << 8) | (format_name[2] << 16) | (format_name[3] << 24);
}

static GstPadProbeReturn
__mmplayer_video_stream_probe (GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
	GstCaps *caps = NULL;
	MMPlayerVideoStreamDataType stream;
	MMVideoBuffer *video_buffer = NULL;
	GstMemory *dataBlock = NULL;
	GstMemory *metaBlock = NULL;
	GstMapInfo mapinfo = GST_MAP_INFO_INIT;
	GstStructure *structure = NULL;
	const gchar *string_format = NULL;
	unsigned int fourcc = 0;
	mm_player_t* player = (mm_player_t*)user_data;
	GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);

	MMPLAYER_RETURN_VAL_IF_FAIL(buffer, GST_PAD_PROBE_DROP);
	MMPLAYER_RETURN_VAL_IF_FAIL(gst_buffer_n_memory(buffer)  , GST_PAD_PROBE_DROP);

	caps = gst_pad_get_current_caps(pad);
	if (caps == NULL) {
		LOGE( "Caps is NULL." );
		return GST_PAD_PROBE_OK;
	}

	MMPLAYER_LOG_GST_CAPS_TYPE(caps);

	/* clear stream data structure */
	memset(&stream, 0x0, sizeof(MMPlayerVideoStreamDataType));

	structure = gst_caps_get_structure( caps, 0 );
	gst_structure_get_int(structure, "width", &(stream.width));
	gst_structure_get_int(structure, "height", &(stream.height));
	string_format = gst_structure_get_string(structure, "format");
	if(string_format) {
		fourcc = _mmplayer_convert_fourcc_string_to_value(string_format);
	}
	stream.format = util_get_pixtype(fourcc);
	gst_caps_unref( caps );
	caps = NULL;

    /*
	LOGD( "Call video steramCb, data[%p], Width[%d],Height[%d], Format[%d]",
	                GST_BUFFER_DATA(buffer), stream.width, stream.height, stream.format );
    */

	if (stream.width == 0 || stream.height == 0 || stream.format == MM_PIXEL_FORMAT_INVALID) {
		LOGE("Wrong condition!!");
		return TRUE;
	}

	/* set size and timestamp */
	dataBlock = gst_buffer_peek_memory(buffer, 0);
	stream.length_total = gst_memory_get_sizes(dataBlock, NULL, NULL);
	stream.timestamp = (unsigned int)(GST_BUFFER_PTS(buffer)/1000000); /* nano sec -> mili sec */

	/* check zero-copy */
	if (player->set_mode.video_zc &&
		player->set_mode.media_packet_video_stream &&
		gst_buffer_n_memory(buffer) > 1) {
		metaBlock = gst_buffer_peek_memory(buffer, 1);
		gst_memory_map(metaBlock, &mapinfo, GST_MAP_READ);
		video_buffer = (MMVideoBuffer *)mapinfo.data;
	}

	if (video_buffer) {
		/* set tbm bo */
		if (video_buffer->type == MM_VIDEO_BUFFER_TYPE_TBM_BO) {
			/* copy pointer of tbm bo, stride, elevation */
			memcpy(stream.bo, video_buffer->handle.bo,
					sizeof(void *) * MM_VIDEO_BUFFER_PLANE_MAX);
		}
		else if (video_buffer->type == MM_VIDEO_BUFFER_TYPE_PHYSICAL_ADDRESS) {
			memcpy(stream.data, video_buffer->data,
					sizeof(void *) * MM_VIDEO_BUFFER_PLANE_MAX);
		}
		memcpy(stream.stride, video_buffer->stride_width,
				sizeof(int) * MM_VIDEO_BUFFER_PLANE_MAX);
		memcpy(stream.elevation, video_buffer->stride_height,
				sizeof(int) * MM_VIDEO_BUFFER_PLANE_MAX);
		/* set gst buffer */
		stream.internal_buffer = buffer;
	} else {
		tbm_bo_handle thandle;
		int stride = ((stream.width + 3) & (~3));
		int elevation = stream.height;
		int size = stride * elevation * 3 / 2;
		gboolean gst_ret;
		gst_ret = gst_memory_map(dataBlock, &mapinfo, GST_MAP_READWRITE);
		if(!gst_ret) {
			LOGE("fail to gst_memory_map");
			return GST_PAD_PROBE_OK;
		}

		stream.stride[0] = stride;
		stream.elevation[0] = elevation;
		if(stream.format == MM_PIXEL_FORMAT_I420) {
			stream.stride[1] = stream.stride[2] = stride / 2;
			stream.elevation[1] = stream.elevation[2] = elevation / 2;
		}
		else {
			LOGE("Not support format %d", stream.format);
			gst_memory_unmap(dataBlock, &mapinfo);
			return GST_PAD_PROBE_OK;
		}

		stream.bo[0] = tbm_bo_alloc(player->bufmgr, size, TBM_BO_DEFAULT);
		if(!stream.bo[0]) {
			LOGE("Fail to tbm_bo_alloc!!");
			gst_memory_unmap(dataBlock, &mapinfo);
			return GST_PAD_PROBE_OK;
		}
		thandle = tbm_bo_map(stream.bo[0], TBM_DEVICE_CPU, TBM_OPTION_WRITE);
		if(thandle.ptr && mapinfo.data)
			memcpy(thandle.ptr, mapinfo.data, size);
		else
			LOGE("data pointer is wrong. dest : %p, src : %p",
					thandle.ptr, mapinfo.data);

		tbm_bo_unmap(stream.bo[0]);
	}

	if (player->video_stream_cb) {
		player->video_stream_cb(&stream, player->video_stream_cb_user_param);
	}

	if (metaBlock) {
		gst_memory_unmap(metaBlock, &mapinfo);
	}else {
		gst_memory_unmap(dataBlock, &mapinfo);
		tbm_bo_unref(stream.bo[0]);
	}

	return GST_PAD_PROBE_OK;
}

static int
__mmplayer_gst_create_video_filters(mm_player_t* player, GList** bucket, gboolean use_video_stream)
{
	gchar* video_csc = "videoconvert"; // default colorspace converter
	GList* element_bucket = *bucket;

	MMPLAYER_RETURN_VAL_IF_FAIL(player && player->pipeline && player->pipeline->videobin, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_FENTER();

	if (!player->set_mode.media_packet_video_stream && use_video_stream)
	{
		if (player->set_mode.video_zc && strlen(player->ini.videoconverter_element) > 0)
		{
			video_csc = player->ini.videoconverter_element;
		}

		MMPLAYER_CREATE_ELEMENT(player->pipeline->videobin, MMPLAYER_V_CONV, video_csc, "video converter", TRUE, player);
		LOGD("using video converter: %s", video_csc);

		if ( !player->set_mode.video_zc)
		{
			gint width = 0;		//width of video
			gint height = 0;		//height of video
			GstCaps* video_caps = NULL;
			GstStructure *structure = NULL;

			/* rotator, scaler and capsfilter */
			MMPLAYER_CREATE_ELEMENT(player->pipeline->videobin, MMPLAYER_V_FLIP, "videoflip", "video rotator", TRUE, player);
			MMPLAYER_CREATE_ELEMENT(player->pipeline->videobin, MMPLAYER_V_SCALE, "videoscale", "video scaler", TRUE, player);
			MMPLAYER_CREATE_ELEMENT(player->pipeline->videobin, MMPLAYER_V_CAPS, "capsfilter", "videocapsfilter", TRUE, player);

			/* get video stream caps parsed by demuxer */

			mm_attrs_get_int_by_name(player->attrs, "display_width", &width);

			if(width)
				structure = gst_structure_new("video/x-raw", "width", G_TYPE_INT, width, NULL);

			mm_attrs_get_int_by_name(player->attrs, "display_height", &height);

			if(structure && height) {
				gst_structure_set (structure, "height", G_TYPE_INT, height, NULL);

				video_caps = gst_caps_new_full(structure, NULL);
				g_object_set (GST_ELEMENT(player->pipeline->videobin[MMPLAYER_V_CAPS].gst), "caps", video_caps, NULL );
				MMPLAYER_LOG_GST_CAPS_TYPE(video_caps);
				gst_caps_unref(video_caps);
			}
			else
				LOGE("fail to set capsfilter %p, width %d, height %d", structure, width, height);

			if(structure)
				gst_structure_free(structure);

		}
	}
	else
	{
		MMDisplaySurfaceType surface_type = MM_DISPLAY_SURFACE_NULL;
		mm_attrs_get_int_by_name (player->attrs, "display_surface_type", (int *)&surface_type);

		if (player->set_mode.video_zc)
		{
			if ( (surface_type == MM_DISPLAY_SURFACE_EVAS) && ( !strcmp(player->ini.videosink_element_evas, "evasimagesink")) )
			{
				video_csc = player->ini.videoconverter_element;
			}
			else
			{
				video_csc = "";
			}
		}

		if (video_csc && (strcmp(video_csc, "")))
		{
			MMPLAYER_CREATE_ELEMENT(player->pipeline->videobin, MMPLAYER_V_CONV, video_csc, "video converter", TRUE, player);
			LOGD("using video converter: %s", video_csc);
		}

		/* set video rotator */
		if ( !player->set_mode.video_zc )
			MMPLAYER_CREATE_ELEMENT(player->pipeline->videobin, MMPLAYER_V_FLIP, "videoflip", "video rotator", TRUE, player);

		/* videoscaler */
		#if !defined(__arm__)
		MMPLAYER_CREATE_ELEMENT(player->pipeline->videobin, MMPLAYER_V_SCALE, "videoscale", "videoscaler", TRUE, player);
		#endif
	}

	*bucket = element_bucket;
	MMPLAYER_FLEAVE();
	return MM_ERROR_NONE;

ERROR:
	*bucket = NULL;
	MMPLAYER_FLEAVE();
	return MM_ERROR_PLAYER_INTERNAL;
}

/**
 * This function is to create video pipeline.
 *
 * @param	player		[in]	handle of player
 *		caps 		[in]	src caps of decoder
 *		surface_type	[in]	surface type for video rendering
 *
 * @return	This function returns zero on success.
 * @remark
 * @see		__mmplayer_gst_create_audio_pipeline, __mmplayer_gst_create_midi_pipeline
 */
/**
  * VIDEO PIPELINE
  * - x surface (arm/x86) : xvimagesink
  * - evas surface  (arm) : evaspixmapsink
  *                         fimcconvert ! evasimagesink
  * - evas surface  (x86) : videoconvertor ! videoflip ! evasimagesink
  */
static int
__mmplayer_gst_create_video_pipeline(mm_player_t* player, GstCaps* caps, MMDisplaySurfaceType surface_type)
{
	GstPad *pad = NULL;
	MMHandleType attrs;
	GList*element_bucket = NULL;
	MMPlayerGstElement* first_element = NULL;
	MMPlayerGstElement* videobin = NULL;
	gchar *videosink_element = NULL;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL(player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED);

	/* alloc handles */
	videobin = (MMPlayerGstElement*)g_malloc0(sizeof(MMPlayerGstElement) * MMPLAYER_V_NUM);
	if ( !videobin )
	{
		return MM_ERROR_PLAYER_NO_FREE_SPACE;
	}

	player->pipeline->videobin = videobin;

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		LOGE("cannot get content attribute");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	/* create bin */
	videobin[MMPLAYER_V_BIN].id = MMPLAYER_V_BIN;
	videobin[MMPLAYER_V_BIN].gst = gst_bin_new("videobin");
	if ( !videobin[MMPLAYER_V_BIN].gst )
	{
		LOGE("failed to create videobin");
		goto ERROR;
	}

	if( player->use_video_stream ) // video stream callback, so send raw video data to application
	{
		LOGD("using memsink\n");

		if ( __mmplayer_gst_create_video_filters(player, &element_bucket, TRUE) != MM_ERROR_NONE)
			goto ERROR;

		/* finally, create video sink. output will be BGRA8888. */
		MMPLAYER_CREATE_ELEMENT(videobin, MMPLAYER_V_SINK, "avsysmemsink", "videosink", TRUE, player);

		MMPLAYER_SIGNAL_CONNECT( player,
									 videobin[MMPLAYER_V_SINK].gst,
									 MM_PLAYER_SIGNAL_TYPE_VIDEOBIN,
									 "video-stream",
									 G_CALLBACK(__mmplayer_videostream_cb),
									 player );
	}
	else // render video data using sink plugin like xvimagesink
	{
		if ( __mmplayer_gst_create_video_filters(player, &element_bucket, FALSE) != MM_ERROR_NONE)
			goto ERROR;

		/* set video sink */
		switch (surface_type)
		{
			case MM_DISPLAY_SURFACE_X:
				if (strlen(player->ini.videosink_element_x) > 0)
					videosink_element = player->ini.videosink_element_x;
				else
					goto ERROR;
				break;
			case MM_DISPLAY_SURFACE_EVAS:
				if (strlen(player->ini.videosink_element_evas) > 0)
					videosink_element = player->ini.videosink_element_evas;
				else
					goto ERROR;
				break;
			case MM_DISPLAY_SURFACE_X_EXT:
			{
				void *pixmap_id_cb = NULL;
				mm_attrs_get_data_by_name(attrs, "display_overlay", &pixmap_id_cb);
				if (pixmap_id_cb) /* this is used for the videoTextue(canvasTexture) overlay */
				{
					videosink_element = player->ini.videosink_element_x;
				}
				else
				{
					LOGE("something wrong.. callback function for getting pixmap id is null");
					goto ERROR;
				}
				break;
			}
			case MM_DISPLAY_SURFACE_NULL:
				if (strlen(player->ini.videosink_element_fake) > 0)
					videosink_element = player->ini.videosink_element_fake;
				else
					goto ERROR;
				break;
			case MM_DISPLAY_SURFACE_REMOTE:
				if (strlen(player->ini.videosink_element_remote) > 0)
					videosink_element = player->ini.videosink_element_remote;
				else
					goto ERROR;
				break;
			default:
				LOGE("unidentified surface type");
				goto ERROR;
		}

		MMPLAYER_CREATE_ELEMENT(videobin, MMPLAYER_V_SINK, videosink_element, videosink_element, TRUE, player);
		LOGD("selected videosink name: %s", videosink_element);

		/* additional setting for sink plug-in */
		switch (surface_type) {
			case MM_DISPLAY_SURFACE_X_EXT:
				MMPLAYER_SIGNAL_CONNECT( player,
										player->pipeline->videobin[MMPLAYER_V_SINK].gst,
										MM_PLAYER_SIGNAL_TYPE_VIDEOBIN,
										"frame-render-error",
										G_CALLBACK(__mmplayer_videoframe_render_error_cb),
										player );
				LOGD("videoTexture usage, connect a signal handler for pixmap rendering error");
				break;
			case MM_DISPLAY_SURFACE_REMOTE:
			{
				char *stream_path = NULL;
				/* viceo_zc is the result of check ST12/SN12 */
				bool use_tbm = player->set_mode.video_zc;
				int attr_ret = mm_attrs_get_string_by_name (
						attrs, "shm_stream_path", &stream_path );
				if(attr_ret == MM_ERROR_NONE && stream_path) {
					g_object_set(G_OBJECT(player->pipeline->videobin[MMPLAYER_V_SINK].gst),
							"socket-path", stream_path,
							"wait-for-connection", FALSE,
							"sync", TRUE,
							"perms", 0777,
							"use-tbm", use_tbm,
							NULL);
					LOGD("set path \"%s\" for shmsink", stream_path);
				} else {
					LOGE("Not set attribute of shm_stream_path");
					goto ERROR;
				}
				break;
			}
			default:
				break;
		}
	}

	if (_mmplayer_update_video_param(player) != MM_ERROR_NONE)
		goto ERROR;

	if (videobin[MMPLAYER_V_SINK].gst)
	{
		GstPad *sink_pad = NULL;
		sink_pad = gst_element_get_static_pad(videobin[MMPLAYER_V_SINK].gst, "sink");
		if (sink_pad)
		{
			MMPLAYER_SIGNAL_CONNECT (player, sink_pad, MM_PLAYER_SIGNAL_TYPE_VIDEOBIN,
					"notify::caps", G_CALLBACK(__mmplayer_gst_caps_notify_cb), player);
			gst_object_unref (GST_OBJECT(sink_pad));
		}
		else
		{
			LOGW("failed to get sink pad from videosink\n");
		}
	}

	/* store it as it's sink element */
	__mmplayer_add_sink( player, videobin[MMPLAYER_V_SINK].gst );

	/* adding created elements to bin */
	if( ! __mmplayer_gst_element_add_bucket_to_bin(GST_BIN(videobin[MMPLAYER_V_BIN].gst), element_bucket) )
	{
		LOGE("failed to add elements\n");
		goto ERROR;
	}

	/* Linking elements in the bucket by added order */
	if ( __mmplayer_gst_element_link_bucket(element_bucket) == -1 )
	{
		LOGE("failed to link elements\n");
		goto ERROR;
	}

	/* get first element's sinkpad for creating ghostpad */
	first_element = (MMPlayerGstElement *)element_bucket->data;
	if ( !first_element )
	{
		LOGE("failed to get first element from bucket\n");
		goto ERROR;
	}

	pad = gst_element_get_static_pad(GST_ELEMENT(first_element->gst), "sink");
	if ( !pad )
	{
		LOGE("failed to get pad from first element\n");
		goto ERROR;
	}

	/* create ghostpad */
	player->ghost_pad_for_videobin = gst_ghost_pad_new("sink", pad);
	if ( FALSE == gst_element_add_pad(videobin[MMPLAYER_V_BIN].gst, player->ghost_pad_for_videobin) )
	{
		LOGE("failed to add ghostpad to videobin\n");
		goto ERROR;
	}
	gst_object_unref(pad);

	player->gapless.video_data_probe_id = gst_pad_add_probe(player->ghost_pad_for_videobin, GST_PAD_PROBE_TYPE_BUFFER,
			__mmplayer_gapless_sinkbin_data_probe, player, NULL);

	/* done. free allocated variables */
	g_list_free(element_bucket);

	mm_attrs_set_int_by_name(attrs, "content_video_found", TRUE);

	if(surface_type == MM_DISPLAY_SURFACE_REMOTE &&
			MMPLAYER_IS_HTTP_PD(player) )
	{
		MMMessageParamType msg = {0, };
		msg.data = gst_caps_to_string(caps);
		MMPLAYER_POST_MSG ( player, MM_MESSAGE_VIDEO_BIN_CREATED, &msg );
	}

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;

ERROR:
	LOGE("ERROR : releasing videobin\n");

	g_list_free( element_bucket );

	if (pad)
		gst_object_unref(GST_OBJECT(pad));

	/* release videobin with it's childs */
	if ( videobin[MMPLAYER_V_BIN].gst )
	{
		gst_object_unref(GST_OBJECT(videobin[MMPLAYER_V_BIN].gst));
	}


	MMPLAYER_FREEIF( videobin );

	player->pipeline->videobin = NULL;

	return MM_ERROR_PLAYER_INTERNAL;
}

static int __mmplayer_gst_create_plain_text_elements(mm_player_t* player)
{
	GList *element_bucket = NULL;
	MMPlayerGstElement *textbin = player->pipeline->textbin;

	MMPLAYER_CREATE_ELEMENT(textbin, MMPLAYER_T_QUEUE, "queue", "text_queue", TRUE, player);
	MMPLAYER_CREATE_ELEMENT(textbin, MMPLAYER_T_IDENTITY, "identity", "text_identity", TRUE, player);
	g_object_set (G_OBJECT (textbin[MMPLAYER_T_IDENTITY].gst),
							"signal-handoffs", FALSE,
							NULL);

	MMPLAYER_CREATE_ELEMENT(textbin, MMPLAYER_T_FAKE_SINK, "fakesink", "text_fakesink", TRUE, player);
	MMPLAYER_SIGNAL_CONNECT( player,
							G_OBJECT(textbin[MMPLAYER_T_FAKE_SINK].gst),
							MM_PLAYER_SIGNAL_TYPE_TEXTBIN,
							"handoff",
							G_CALLBACK(__mmplayer_update_subtitle),
							(gpointer)player );

	g_object_set (G_OBJECT (textbin[MMPLAYER_T_FAKE_SINK].gst), "async", TRUE, NULL);
	g_object_set (G_OBJECT (textbin[MMPLAYER_T_FAKE_SINK].gst), "sync", TRUE, NULL);
	g_object_set (G_OBJECT (textbin[MMPLAYER_T_FAKE_SINK].gst), "signal-handoffs", TRUE, NULL);

	if (!player->play_subtitle)
	{
		LOGD ("add textbin sink as sink element of whole pipeline.\n");
		__mmplayer_add_sink (player, GST_ELEMENT(textbin[MMPLAYER_T_FAKE_SINK].gst));
	}

	/* adding created elements to bin */
	LOGD("adding created elements to bin\n");
	if( !__mmplayer_gst_element_add_bucket_to_bin( GST_BIN(textbin[MMPLAYER_T_BIN].gst), element_bucket ))
	{
		LOGE("failed to add elements\n");
		goto ERROR;
	}

	/* unset sink flag from textbin. not to hold eos when video data is shorter than subtitle */
	GST_OBJECT_FLAG_UNSET (textbin[MMPLAYER_T_BIN].gst, GST_ELEMENT_FLAG_SINK);
	GST_OBJECT_FLAG_UNSET (textbin[MMPLAYER_T_FAKE_SINK].gst, GST_ELEMENT_FLAG_SINK);

	/* linking elements in the bucket by added order. */
	LOGD("Linking elements in the bucket by added order.\n");
	if ( __mmplayer_gst_element_link_bucket(element_bucket) == -1 )
	{
		LOGE("failed to link elements\n");
		goto ERROR;
	}

	/* done. free allocated variables */
	g_list_free(element_bucket);

	if (textbin[MMPLAYER_T_QUEUE].gst)
	{
		GstPad *pad = NULL;
		GstPad *ghostpad = NULL;

		pad = gst_element_get_static_pad(GST_ELEMENT(textbin[MMPLAYER_T_QUEUE].gst), "sink");
		if (!pad)
		{
			LOGE("failed to get video pad of textbin\n");
			return MM_ERROR_PLAYER_INTERNAL;
		}

		ghostpad = gst_ghost_pad_new("text_sink", pad);
		gst_object_unref(pad);

		if (!ghostpad)
		{
			LOGE("failed to create ghostpad of textbin\n");
			goto ERROR;
		}

		if (!gst_element_add_pad(textbin[MMPLAYER_T_BIN].gst, ghostpad))
		{
			LOGE("failed to add ghostpad to textbin\n");
			goto ERROR;
		}
	}

	return MM_ERROR_NONE;

ERROR:
	g_list_free(element_bucket);

	return MM_ERROR_PLAYER_INTERNAL;
}

static int __mmplayer_gst_create_text_pipeline(mm_player_t* player)
{
	MMPlayerGstElement *textbin = NULL;
	GList *element_bucket = NULL;
	gint i = 0;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* alloc handles */
	textbin = (MMPlayerGstElement*)g_malloc0(sizeof(MMPlayerGstElement) * MMPLAYER_T_NUM);
	if ( ! textbin )
	{
		LOGE("failed to allocate memory for textbin\n");
		return MM_ERROR_PLAYER_NO_FREE_SPACE;
	}

	/* create bin */
	textbin[MMPLAYER_T_BIN].id = MMPLAYER_T_BIN;
	textbin[MMPLAYER_T_BIN].gst = gst_bin_new("textbin");
	if ( !textbin[MMPLAYER_T_BIN].gst )
	{
		LOGE("failed to create textbin\n");
		goto ERROR;
	}

	/* take it */
	player->pipeline->textbin = textbin;

	/* fakesink */
	if (player->use_textoverlay)
	{
		LOGD ("use textoverlay for displaying \n");

		MMPLAYER_CREATE_ELEMENT_ADD_BIN(textbin, MMPLAYER_T_QUEUE, "queue", "text_t_queue", textbin[MMPLAYER_T_BIN].gst, player);

		MMPLAYER_CREATE_ELEMENT_ADD_BIN(textbin, MMPLAYER_T_VIDEO_QUEUE, "queue", "text_v_queue", textbin[MMPLAYER_T_BIN].gst, player);

		MMPLAYER_CREATE_ELEMENT_ADD_BIN(textbin, MMPLAYER_T_VIDEO_CONVERTER, "fimcconvert", "text_v_converter", textbin[MMPLAYER_T_BIN].gst, player);

		MMPLAYER_CREATE_ELEMENT_ADD_BIN(textbin, MMPLAYER_T_OVERLAY, "textoverlay", "text_overlay", textbin[MMPLAYER_T_BIN].gst, player);

		if (!gst_element_link_pads (textbin[MMPLAYER_T_VIDEO_QUEUE].gst, "src", textbin[MMPLAYER_T_VIDEO_CONVERTER].gst, "sink"))
		{
			LOGE("failed to link queue and converter\n");
			goto ERROR;
		}

		if (!gst_element_link_pads (textbin[MMPLAYER_T_VIDEO_CONVERTER].gst, "src", textbin[MMPLAYER_T_OVERLAY].gst, "video_sink"))
		{
			LOGE("failed to link queue and textoverlay\n");
			goto ERROR;
		}

		if (!gst_element_link_pads (textbin[MMPLAYER_T_QUEUE].gst, "src", textbin[MMPLAYER_T_OVERLAY].gst, "text_sink"))
		{
			LOGE("failed to link queue and textoverlay\n");
			goto ERROR;
		}
	}
	else
	{
		int surface_type = 0;

		LOGD ("use subtitle message for displaying \n");

		mm_attrs_get_int_by_name (player->attrs, "display_surface_type", &surface_type);

		switch(surface_type)
		{
			case MM_DISPLAY_SURFACE_X:
			case MM_DISPLAY_SURFACE_EVAS:
			case MM_DISPLAY_SURFACE_GL:
			case MM_DISPLAY_SURFACE_NULL:
			case MM_DISPLAY_SURFACE_X_EXT:
			case MM_DISPLAY_SURFACE_REMOTE:
				if (__mmplayer_gst_create_plain_text_elements(player) != MM_ERROR_NONE)
				{
					LOGE("failed to make plain text elements\n");
					goto ERROR;
				}
				break;

			default:
				break;
		}
	}

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;

ERROR:

	LOGD("ERROR : releasing textbin\n");

	g_list_free( element_bucket );

	/* release element which are not added to bin */
	for ( i = 1; i < MMPLAYER_T_NUM; i++ ) 	/* NOTE : skip bin */
	{
		if ( textbin[i].gst )
		{
			GstObject* parent = NULL;
			parent = gst_element_get_parent( textbin[i].gst );

			if ( !parent )
			{
				gst_object_unref(GST_OBJECT(textbin[i].gst));
				textbin[i].gst = NULL;
			}
			else
			{
				gst_object_unref(GST_OBJECT(parent));
			}
		}
	}

	/* release textbin with it's childs */
	if ( textbin[MMPLAYER_T_BIN].gst )
	{
		gst_object_unref(GST_OBJECT(textbin[MMPLAYER_T_BIN].gst));
	}

	MMPLAYER_FREEIF( textbin );

	player->pipeline->textbin = NULL;

	return MM_ERROR_PLAYER_INTERNAL;
}


static int
__mmplayer_gst_create_subtitle_src(mm_player_t* player)
{
	MMPlayerGstElement* mainbin = NULL;
	MMHandleType attrs = 0;
	GstElement *subsrc = NULL;
	GstElement *subparse = NULL;
	gchar *subtitle_uri =NULL;
	const gchar *charset = NULL;
	GstPad *pad = NULL;

	MMPLAYER_FENTER();

	/* get mainbin */
	MMPLAYER_RETURN_VAL_IF_FAIL ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED);

	mainbin = player->pipeline->mainbin;

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		LOGE("cannot get content attribute\n");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	mm_attrs_get_string_by_name ( attrs, "subtitle_uri", &subtitle_uri );
	if ( !subtitle_uri || strlen(subtitle_uri) < 1)
	{
		LOGE("subtitle uri is not proper filepath.\n");
		return MM_ERROR_PLAYER_INVALID_URI;
	}
	LOGD("subtitle file path is [%s].\n", subtitle_uri);


	/* create the subtitle source */
	subsrc = gst_element_factory_make("filesrc", "subtitle_source");
	if ( !subsrc )
	{
		LOGE ( "failed to create filesrc element\n" );
		goto ERROR;
	}
	g_object_set(G_OBJECT (subsrc), "location", subtitle_uri, NULL);

	mainbin[MMPLAYER_M_SUBSRC].id = MMPLAYER_M_SUBSRC;
	mainbin[MMPLAYER_M_SUBSRC].gst = subsrc;

	if (!gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), subsrc))
	{
		LOGW("failed to add queue\n");
		goto ERROR;
	}

	/* subparse */
	subparse = gst_element_factory_make("subparse", "subtitle_parser");
	if ( !subparse )
	{
		LOGE ( "failed to create subparse element\n" );
		goto ERROR;
	}

	charset = util_get_charset(subtitle_uri);
	if (charset)
	{
		LOGD ("detected charset is %s\n", charset );
		g_object_set (G_OBJECT (subparse), "subtitle-encoding", charset, NULL);
	}

	mainbin[MMPLAYER_M_SUBPARSE].id = MMPLAYER_M_SUBPARSE;
	mainbin[MMPLAYER_M_SUBPARSE].gst = subparse;

	if (!gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), subparse))
	{
		LOGW("failed to add subparse\n");
		goto ERROR;
	}

	if (!gst_element_link_pads (subsrc, "src", subparse, "sink"))
	{
		LOGW("failed to link subsrc and subparse\n");
		goto ERROR;
	}

	player->play_subtitle = TRUE;
	player->adjust_subtitle_pos = 0;

	LOGD ("play subtitle using subtitle file\n");

	if (player->pipeline->textbin == NULL)
	{
		if (MM_ERROR_NONE !=  __mmplayer_gst_create_text_pipeline(player))
		{
			LOGE("failed to create textbin. continuing without text\n");
			goto ERROR;
		}

		if (!gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), GST_ELEMENT(player->pipeline->textbin[MMPLAYER_T_BIN].gst)))
		{
			LOGW("failed to add textbin\n");
			goto ERROR;
		}

		LOGD ("link text input selector and textbin ghost pad");

		player->textsink_linked = 1;
		player->external_text_idx = 0;
		LOGI("player->textsink_linked set to 1\n");
	}
	else
	{
		LOGD("text bin has been created. reuse it.");
		player->external_text_idx = 1;
	}

	if (!gst_element_link_pads (subparse, "src", player->pipeline->textbin[MMPLAYER_T_BIN].gst, "text_sink"))
	{
		LOGW("failed to link subparse and textbin\n");
		goto ERROR;
	}

	pad = gst_element_get_static_pad (player->pipeline->textbin[MMPLAYER_T_FAKE_SINK].gst, "sink");

	if (!pad)
	{
		LOGE("failed to get sink pad from textsink to probe data");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
				__mmplayer_subtitle_adjust_position_probe, player, NULL);

	gst_object_unref(pad);
	pad=NULL;

	/* create dot. for debugging */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-with-subtitle" );
	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;

ERROR:
	player->textsink_linked = 0;
	return MM_ERROR_PLAYER_INTERNAL;
}

gboolean
__mmplayer_update_subtitle( GstElement* object, GstBuffer *buffer, GstPad *pad, gpointer data)
{
	mm_player_t* player = (mm_player_t*) data;
	MMMessageParamType msg = {0, };
	GstClockTime duration = 0;
	gpointer text = NULL;
	guint text_size = 0;
	gboolean ret = TRUE;
	GstMapInfo mapinfo = GST_MAP_INFO_INIT;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );
	MMPLAYER_RETURN_VAL_IF_FAIL ( buffer, FALSE );

	gst_buffer_map(buffer, &mapinfo, GST_MAP_READ);
	text = mapinfo.data;
	text_size = mapinfo.size;
	duration = GST_BUFFER_DURATION(buffer);

	if ( player->set_mode.subtitle_off )
	{
		LOGD("subtitle is OFF.\n" );
		return TRUE;
	}

	if ( !text || (text_size == 0))
	{
		LOGD("There is no subtitle to be displayed.\n" );
		return TRUE;
	}

	msg.data = (void *) text;
	msg.subtitle.duration = GST_TIME_AS_MSECONDS(duration);

	LOGD("update subtitle : [%ld msec] %s\n'", msg.subtitle.duration, (char*)msg.data );

	MMPLAYER_POST_MSG( player, MM_MESSAGE_UPDATE_SUBTITLE, &msg );
	gst_buffer_unmap(buffer, &mapinfo);

	MMPLAYER_FLEAVE();

	return ret;
}

static GstPadProbeReturn
__mmplayer_subtitle_adjust_position_probe (GstPad *pad, GstPadProbeInfo *info, gpointer u_data)

{
	mm_player_t *player = (mm_player_t *) u_data;
	GstClockTime cur_timestamp = 0;
	gint64 adjusted_timestamp = 0;
	GstBuffer *buffer = gst_pad_probe_info_get_buffer(info);

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );

	if ( player->set_mode.subtitle_off )
	{
		LOGD("subtitle is OFF.\n" );
		return TRUE;
	}

	if (player->adjust_subtitle_pos == 0 )
	{
		LOGD("nothing to do");
		return TRUE;
	}

	cur_timestamp = GST_BUFFER_TIMESTAMP(buffer);
	adjusted_timestamp = (gint64) cur_timestamp + ((gint64) player->adjust_subtitle_pos * G_GINT64_CONSTANT(1000000));

	if ( adjusted_timestamp < 0)
	{
		LOGD("adjusted_timestamp under zero");
		MMPLAYER_FLEAVE();
		return FALSE;
	}

	GST_BUFFER_TIMESTAMP(buffer) = (GstClockTime) adjusted_timestamp;
	LOGD("buffer timestamp changed %" GST_TIME_FORMAT " -> %" GST_TIME_FORMAT "",
				GST_TIME_ARGS(cur_timestamp),
				GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buffer)));

	return GST_PAD_PROBE_OK;
}
static int 	__gst_adjust_subtitle_position(mm_player_t* player, int format, int position)
{
	MMPLAYER_FENTER();

	/* check player and subtitlebin are created */
	MMPLAYER_RETURN_VAL_IF_FAIL ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );
	MMPLAYER_RETURN_VAL_IF_FAIL ( player->play_subtitle, MM_ERROR_NOT_SUPPORT_API );

	if (position == 0)
	{
		LOGD ("nothing to do\n");
		MMPLAYER_FLEAVE();
		return MM_ERROR_NONE;
	}

	switch (format)
	{
		case MM_PLAYER_POS_FORMAT_TIME:
		{
			/* check current postion */
			player->adjust_subtitle_pos = position;

			LOGD("save adjust_subtitle_pos in player") ;
		}
		break;

		default:
		{
			LOGW("invalid format.\n");
			MMPLAYER_FLEAVE();
			return MM_ERROR_INVALID_ARGUMENT;
		}
	}

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}
static int __gst_adjust_video_position(mm_player_t* player, int offset)
{
	MMPLAYER_FENTER();
	LOGD("adjusting video_pos in player") ;
	int current_pos = 0;
	/* check player and videobin are created */
	MMPLAYER_RETURN_VAL_IF_FAIL ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );
	if ( !player->pipeline->videobin ||
			!player->pipeline->videobin[MMPLAYER_V_SINK].gst )
	{
		LOGD("no video pipeline or sink is there");
		return MM_ERROR_PLAYER_INVALID_STATE ;
	}
	if (offset == 0)
	{
		LOGD ("nothing to do\n");
		MMPLAYER_FLEAVE();
		return MM_ERROR_NONE;
	}
	if(__gst_get_position ( player, MM_PLAYER_POS_FORMAT_TIME, (unsigned long*)&current_pos ) != MM_ERROR_NONE )
	{
		LOGD("failed to get current position");
		return MM_ERROR_PLAYER_INTERNAL;
	}
	if ( (current_pos - offset ) < GST_TIME_AS_MSECONDS(player->duration) )
	{
		LOGD("enter video delay is valid");
	}
	else {
		LOGD("enter video delay is crossing content boundary");
		return MM_ERROR_INVALID_ARGUMENT ;
	}
	g_object_set (G_OBJECT (player->pipeline->videobin[MMPLAYER_V_SINK].gst),"ts-offset",((gint64) offset * G_GINT64_CONSTANT(1000000)),NULL);
	LOGD("video delay has been done");
	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

static void
__gst_appsrc_feed_data_mem(GstElement *element, guint size, gpointer user_data) // @
{
	GstElement *appsrc = element;
	tBuffer *buf = (tBuffer *)user_data;
	GstBuffer *buffer = NULL;
	GstFlowReturn ret = GST_FLOW_OK;
	gint len = size;

	MMPLAYER_RETURN_IF_FAIL ( element );
	MMPLAYER_RETURN_IF_FAIL ( buf );

	buffer = gst_buffer_new ();

	if (buf->offset >= buf->len)
	{
		LOGD("call eos appsrc\n");
		g_signal_emit_by_name (appsrc, "end-of-stream", &ret);
		return;
	}

	if ( buf->len - buf->offset < size)
	{
		len = buf->len - buf->offset + buf->offset;
	}

	gst_buffer_insert_memory(buffer, -1, gst_memory_new_wrapped(0, (guint8 *)(buf->buf + buf->offset), len, 0, len, (guint8*)(buf->buf + buf->offset), g_free));
	GST_BUFFER_OFFSET(buffer) = buf->offset;
	GST_BUFFER_OFFSET_END(buffer) = buf->offset + len;

	//LOGD("feed buffer %p, offset %u-%u length %u\n", buffer, buf->offset, buf->len,len);
	g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);

	buf->offset += len;
}

static gboolean
__gst_appsrc_seek_data_mem(GstElement *element, guint64 size, gpointer user_data) // @
{
	tBuffer *buf = (tBuffer *)user_data;

	MMPLAYER_RETURN_VAL_IF_FAIL ( buf, FALSE );

	buf->offset  = (int)size;

    	return TRUE;
}

static void
__gst_appsrc_feed_data(GstElement *element, guint size, gpointer user_data) // @
{
	mm_player_t *player  = (mm_player_t*)user_data;
	MMPlayerStreamType type = MM_PLAYER_STREAM_TYPE_DEFAULT;

	MMPLAYER_RETURN_IF_FAIL ( player );

	LOGI("app-src: feed data\n");

	if (player->media_stream_buffer_status_cb[type])
		player->media_stream_buffer_status_cb[type](type, MM_PLAYER_MEDIA_STREAM_BUFFER_UNDERRUN, player->buffer_cb_user_param);
}

static gboolean
__gst_appsrc_seek_data(GstElement *element, guint64 offset, gpointer user_data) // @
{
	mm_player_t *player  = (mm_player_t*)user_data;
	MMPlayerStreamType type = MM_PLAYER_STREAM_TYPE_DEFAULT;

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );

	LOGI("app-src: seek data\n");

	if(player->media_stream_seek_data_cb[type])
		player->media_stream_seek_data_cb[type](type, offset, player->buffer_cb_user_param);

	return TRUE;
}


static gboolean
__gst_appsrc_enough_data(GstElement *element, gpointer user_data) // @
{
	mm_player_t *player  = (mm_player_t*)user_data;
	MMPlayerStreamType type = MM_PLAYER_STREAM_TYPE_DEFAULT;

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );

	LOGI("app-src: enough data:%p\n", player->media_stream_buffer_status_cb[type]);

	if (player->media_stream_buffer_status_cb[type])
		player->media_stream_buffer_status_cb[type](type, MM_PLAYER_MEDIA_STREAM_BUFFER_OVERFLOW, player->buffer_cb_user_param);

	return TRUE;
}

int
_mmplayer_push_buffer(MMHandleType hplayer, unsigned char *buf, int size) // @
{
	mm_player_t* player = (mm_player_t*)hplayer;
    	GstBuffer *buffer = NULL;
    	GstFlowReturn gst_ret = GST_FLOW_OK;
	int ret = MM_ERROR_NONE;
//	gint len = size;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check current state */
//	MMPLAYER_CHECK_STATE( player, MMPLAYER_COMMAND_START );


	/* NOTE : we should check and create pipeline again if not created as we destroy
	 * whole pipeline when stopping in streamming playback
	 */
	if ( ! player->pipeline )
	{
		if ( MM_ERROR_NONE != __gst_realize( player ) )
		{
			LOGE("failed to realize before starting. only in streamming\n");
			return MM_ERROR_PLAYER_INTERNAL;
		}
	}

     	LOGI("app-src: pushing data\n");

    	if ( buf == NULL )
    	{
        	LOGE("buf is null\n");
        	return MM_ERROR_NONE;
    	}

    	buffer = gst_buffer_new ();

    	if (size <= 0)
    	{
        	LOGD("call eos appsrc\n");
        	g_signal_emit_by_name (player->pipeline->mainbin[MMPLAYER_M_SRC].gst, "end-of-stream", &gst_ret);
        	return MM_ERROR_NONE;
    	}

	//gst_buffer_insert_memory(buffer, -1, gst_memory_new_wrapped(0, (guint8 *)(buf->buf + buf->offset), len, 0, len, (guint8*)(buf->buf + buf->offset), g_free));

    	LOGD("feed buffer %p, length %u\n", buf, size);
    	g_signal_emit_by_name (player->pipeline->mainbin[MMPLAYER_M_SRC].gst, "push-buffer", buffer, &gst_ret);

	MMPLAYER_FLEAVE();

	return ret;
}

static GstBusSyncReply
__mmplayer_bus_sync_callback (GstBus * bus, GstMessage * message, gpointer data)
{
	mm_player_t *player = (mm_player_t *)data;
	GstBusSyncReply reply = GST_BUS_DROP;

	if ( ! ( player->pipeline && player->pipeline->mainbin ) )
	{
		LOGE("player pipeline handle is null");
		return GST_BUS_PASS;
	}

	if (!__mmplayer_check_useful_message(player, message))
	{
		gst_message_unref (message);
		return GST_BUS_DROP;
	}

	switch (GST_MESSAGE_TYPE (message))
	{
		case GST_MESSAGE_STATE_CHANGED:
			/* post directly for fast launch */
			if (player->sync_handler) {
				__mmplayer_gst_callback(NULL, message, player);
				reply = GST_BUS_DROP;
			}
			else {
				reply = GST_BUS_PASS;
			}
			break;
		case GST_MESSAGE_TAG:
			__mmplayer_gst_extract_tag_from_msg(player, message);

			#if 0 // debug
			{
				GstTagList *tags = NULL;

				gst_message_parse_tag (message, &tags);
				if (tags) {
					LOGE("TAGS received from element \"%s\".\n",
					GST_STR_NULL (GST_ELEMENT_NAME (GST_MESSAGE_SRC (message))));

					gst_tag_list_foreach (tags, print_tag, NULL);
					gst_tag_list_free (tags);
					tags = NULL;
				}
				break;
			}
			#endif
			break;

		case GST_MESSAGE_DURATION_CHANGED:
			__mmplayer_gst_handle_duration(player, message);
			break;
		case GST_MESSAGE_ASYNC_DONE:
			/* NOTE:Don't call gst_callback directly
			 * because previous frame can be showed even though this message is received for seek.
			 */
		default:
			reply = GST_BUS_PASS;
			break;
	}

	if (reply == GST_BUS_DROP)
		gst_message_unref (message);

	return reply;
}

static gboolean
__mmplayer_gst_create_decoder ( mm_player_t *player,
								MMPlayerTrackType track,
								GstPad* srcpad,
								enum MainElementID elemId,
								const gchar* name)
{
	gboolean ret = TRUE;
	GstPad *sinkpad = NULL;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL( player &&
						player->pipeline &&
						player->pipeline->mainbin, FALSE);
	MMPLAYER_RETURN_VAL_IF_FAIL((track == MM_PLAYER_TRACK_TYPE_AUDIO || track == MM_PLAYER_TRACK_TYPE_VIDEO), FALSE);
	MMPLAYER_RETURN_VAL_IF_FAIL(srcpad, FALSE);
	MMPLAYER_RETURN_VAL_IF_FAIL((player->pipeline->mainbin[elemId].gst == NULL), FALSE);

	GstElement *decodebin = NULL;
	GstCaps *dec_caps = NULL;

	/* create decodebin */
	decodebin = gst_element_factory_make("decodebin", name);

	if (!decodebin)
	{
		LOGE("error : fail to create decodebin for %d decoder\n", track);
		ret = FALSE;
		goto ERROR;
	}

	/* raw pad handling signal */
	MMPLAYER_SIGNAL_CONNECT( player, G_OBJECT(decodebin), MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "pad-added",
										G_CALLBACK(__mmplayer_gst_decode_pad_added), player);

	/* This signal is emitted whenever decodebin finds a new stream. It is emitted
	before looking for any elements that can handle that stream.*/
	MMPLAYER_SIGNAL_CONNECT( player, G_OBJECT(decodebin), MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "autoplug-select",
										G_CALLBACK(__mmplayer_gst_decode_autoplug_select), player);

	/* This signal is emitted when a element is added to the bin.*/
	MMPLAYER_SIGNAL_CONNECT( player, G_OBJECT(decodebin), MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "element-added",
										G_CALLBACK(__mmplayer_gst_element_added), player);

	if (!gst_bin_add(GST_BIN(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst), decodebin))
	{
		LOGE("failed to add new decodebin\n");
		ret = FALSE;
		goto ERROR;
	}

	dec_caps = gst_pad_query_caps (srcpad, NULL);
	if (dec_caps)
	{
		//LOGD ("got pad %s:%s , dec_caps %" GST_PTR_FORMAT, GST_DEBUG_PAD_NAME(srcpad), dec_caps);
		g_object_set(G_OBJECT(decodebin), "sink-caps", dec_caps, NULL);
		gst_caps_unref(dec_caps);
	}

	player->pipeline->mainbin[elemId].id = elemId;
	player->pipeline->mainbin[elemId].gst = decodebin;

	sinkpad = gst_element_get_static_pad (decodebin, "sink");

	if (GST_PAD_LINK_OK != gst_pad_link(srcpad, sinkpad))
	{
		LOGW ("failed to link [%s:%s] to decoder\n", GST_DEBUG_PAD_NAME(srcpad));
		gst_object_unref (GST_OBJECT(decodebin));
	}

	if (GST_STATE_CHANGE_FAILURE == gst_element_sync_state_with_parent (decodebin))
	{
		LOGE("failed to sync second level decodebin state with parent\n");
	}

	LOGD("Total num of %d tracks = %d \n", track, player->selector[track].total_track_num);

ERROR:
	if (sinkpad)
	{
		gst_object_unref ( GST_OBJECT(sinkpad) );
		sinkpad = NULL;
	}
	MMPLAYER_FLEAVE();

	return ret;
}

/**
 * This function is to create  audio or video pipeline for playing.
 *
 * @param	player		[in]	handle of player
 *
 * @return	This function returns zero on success.
 * @remark
 * @see
 */
static int
__mmplayer_gst_create_pipeline(mm_player_t* player) // @
{
	GstBus	*bus = NULL;
	MMPlayerGstElement *mainbin = NULL;
	MMHandleType attrs = 0;
	GstElement* element = NULL;
	GstElement* elem_src_audio = NULL;
	GstElement* elem_src_subtitle = NULL;
	GstElement* es_video_queue = NULL;
	GstElement* es_audio_queue = NULL;
	GstElement* es_subtitle_queue = NULL;
	GList* element_bucket = NULL;
	gboolean need_state_holder = TRUE;
	gint i = 0;
#ifdef SW_CODEC_ONLY
	int surface_type = 0;
#endif
	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	/* get profile attribute */
	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		LOGE("cannot get content attribute\n");
		goto INIT_ERROR;
	}

	/* create pipeline handles */
	if ( player->pipeline )
	{
		LOGW("pipeline should be released before create new one\n");
		goto INIT_ERROR;
	}

	player->pipeline = (MMPlayerGstPipelineInfo*) g_malloc0( sizeof(MMPlayerGstPipelineInfo) );
	if (player->pipeline == NULL)
		goto INIT_ERROR;

	memset( player->pipeline, 0, sizeof(MMPlayerGstPipelineInfo) );


	/* create mainbin */
	mainbin = (MMPlayerGstElement*) g_malloc0( sizeof(MMPlayerGstElement) * MMPLAYER_M_NUM );
	if (mainbin == NULL)
		goto INIT_ERROR;

	memset( mainbin, 0, sizeof(MMPlayerGstElement) * MMPLAYER_M_NUM);

	/* create pipeline */
	mainbin[MMPLAYER_M_PIPE].id = MMPLAYER_M_PIPE;
	mainbin[MMPLAYER_M_PIPE].gst = gst_pipeline_new("player");
	if ( ! mainbin[MMPLAYER_M_PIPE].gst )
	{
		LOGE("failed to create pipeline\n");
		goto INIT_ERROR;
	}
	player->demux_pad_index = 0;
	player->subtitle_language_list = NULL;

	player->is_subtitle_force_drop = FALSE;
	player->last_multiwin_status = FALSE;

	_mmplayer_track_initialize(player);

	/* create source element */
	switch ( player->profile.uri_type )
	{
		/* rtsp streamming */
		case MM_PLAYER_URI_TYPE_URL_RTSP:
		{
			gint network_bandwidth;
			gchar *user_agent, *wap_profile;

			element = gst_element_factory_make("rtspsrc", "rtsp source");

			if ( !element )
			{
				LOGE("failed to create streaming source element\n");
				break;
			}

			/* make it zero */
			network_bandwidth = 0;
			user_agent = wap_profile = NULL;

			/* get attribute */
			mm_attrs_get_string_by_name ( attrs, "streaming_user_agent", &user_agent );
			mm_attrs_get_string_by_name ( attrs,"streaming_wap_profile", &wap_profile );
			mm_attrs_get_int_by_name ( attrs, "streaming_network_bandwidth", &network_bandwidth );

			SECURE_LOGD("user_agent : %s\n", user_agent);
			SECURE_LOGD("wap_profile : %s\n", wap_profile);

			/* setting property to streaming source */
			g_object_set(G_OBJECT(element), "location", player->profile.uri, NULL);
			if ( user_agent )
				g_object_set(G_OBJECT(element), "user-agent", user_agent, NULL);
			if ( wap_profile )
				g_object_set(G_OBJECT(element), "wap_profile", wap_profile, NULL);

			MMPLAYER_SIGNAL_CONNECT ( player, G_OBJECT(element), MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "pad-added",
				G_CALLBACK (__mmplayer_gst_rtp_dynamic_pad), player );
			MMPLAYER_SIGNAL_CONNECT ( player, G_OBJECT(element), MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "no-more-pads",
				G_CALLBACK (__mmplayer_gst_rtp_no_more_pads), player );

			player->use_decodebin = FALSE;
		}
		break;

		/* http streaming*/
		case MM_PLAYER_URI_TYPE_URL_HTTP:
		{
			gchar *user_agent, *proxy, *cookies, **cookie_list;
			gint http_timeout = DEFAULT_HTTP_TIMEOUT;
			user_agent = proxy = cookies = NULL;
			cookie_list = NULL;
			gint mode = MM_PLAYER_PD_MODE_NONE;

			mm_attrs_get_int_by_name ( attrs, "pd_mode", &mode );

			player->pd_mode = mode;

			LOGD("http playback, PD mode : %d\n", player->pd_mode);

			if ( ! MMPLAYER_IS_HTTP_PD(player) )
			{
				element = gst_element_factory_make(player->ini.httpsrc_element, "http_streaming_source");
				if ( !element )
				{
					LOGE("failed to create http streaming source element[%s].\n", player->ini.httpsrc_element);
					break;
				}
				LOGD("using http streamming source [%s].\n", player->ini.httpsrc_element);

				/* get attribute */
				mm_attrs_get_string_by_name ( attrs, "streaming_cookie", &cookies );
				mm_attrs_get_string_by_name ( attrs, "streaming_user_agent", &user_agent );
				mm_attrs_get_string_by_name ( attrs, "streaming_proxy", &proxy );
				mm_attrs_get_int_by_name ( attrs, "streaming_timeout", &http_timeout );

				if ((http_timeout == DEFAULT_HTTP_TIMEOUT) &&
					(player->ini.http_timeout != DEFAULT_HTTP_TIMEOUT))
				{
					LOGD("get timeout from ini\n");
					http_timeout = player->ini.http_timeout;
				}

				/* get attribute */
				SECURE_LOGD("location : %s\n", player->profile.uri);
				SECURE_LOGD("cookies : %s\n", cookies);
				SECURE_LOGD("proxy : %s\n", proxy);
				SECURE_LOGD("user_agent :  %s\n",  user_agent);
				LOGD("timeout : %d\n",  http_timeout);

				/* setting property to streaming source */
				g_object_set(G_OBJECT(element), "location", player->profile.uri, NULL);
				g_object_set(G_OBJECT(element), "timeout", http_timeout, NULL);
				g_object_set(G_OBJECT(element), "blocksize", (unsigned long)(64*1024), NULL);

				/* check if prosy is vailid or not */
				if ( util_check_valid_url ( proxy ) )
					g_object_set(G_OBJECT(element), "proxy", proxy, NULL);
				/* parsing cookies */
				if ( ( cookie_list = util_get_cookie_list ((const char*)cookies) ) )
					g_object_set(G_OBJECT(element), "cookies", cookie_list, NULL);
				if ( user_agent )
					g_object_set(G_OBJECT(element), "user-agent", user_agent, NULL);

				if ( MMPLAYER_URL_HAS_DASH_SUFFIX(player) )
				{
					LOGW("it's dash. and it's still experimental feature.");
				}
			}
			else // progressive download
			{
				gchar* location = NULL;

				if (player->pd_mode == MM_PLAYER_PD_MODE_URI)
				{
					gchar *path = NULL;

					mm_attrs_get_string_by_name ( attrs, "pd_location", &path );

					MMPLAYER_FREEIF(player->pd_file_save_path);

					LOGD("PD Location : %s\n", path);

					if ( path )
					{
						player->pd_file_save_path = g_strdup(path);
					}
					else
					{
						LOGE("can't find pd location so, it should be set \n");
						return MM_ERROR_PLAYER_FILE_NOT_FOUND;
					}
				}

				element = gst_element_factory_make("pdpushsrc", "PD pushsrc");
				if ( !element )
				{
					LOGE("failed to create PD push source element[%s].\n", "pdpushsrc");
					break;
				}

				if (player->pd_mode == MM_PLAYER_PD_MODE_URI)
					g_object_set(G_OBJECT(element), "location", player->pd_file_save_path, NULL);
				else
					g_object_set(G_OBJECT(element), "location", player->profile.uri, NULL);

				g_object_get(element, "location", &location, NULL);
				LOGD("PD_LOCATION [%s].\n", location);
				if (location)
					g_free (location);
			}
		}
		break;

		/* file source */
		case MM_PLAYER_URI_TYPE_FILE:
		{

			LOGD("using filesrc for 'file://' handler.\n");

			element = gst_element_factory_make("filesrc", "source");

			if ( !element )
			{
				LOGE("failed to create filesrc\n");
				break;
			}

			g_object_set(G_OBJECT(element), "location", (player->profile.uri)+7, NULL);	/* uri+7 -> remove "file:// */
			//g_object_set(G_OBJECT(element), "use-mmap", TRUE, NULL);
		}
		break;

		case MM_PLAYER_URI_TYPE_SS:
		{
			gint http_timeout = DEFAULT_HTTP_TIMEOUT;
			element = gst_element_factory_make("souphttpsrc", "http streaming source");
			if ( !element )
			{
				LOGE("failed to create http streaming source element[%s]", player->ini.httpsrc_element);
				break;
			}

			mm_attrs_get_int_by_name ( attrs, "streaming_timeout", &http_timeout );

			if ((http_timeout == DEFAULT_HTTP_TIMEOUT) &&
				(player->ini.http_timeout != DEFAULT_HTTP_TIMEOUT))
			{
				LOGD("get timeout from ini\n");
				http_timeout = player->ini.http_timeout;
			}

			/* setting property to streaming source */
			g_object_set(G_OBJECT(element), "location", player->profile.uri, NULL);
			g_object_set(G_OBJECT(element), "timeout", http_timeout, NULL);
		}
		break;

		/* appsrc */
		case MM_PLAYER_URI_TYPE_BUFF:
		{
			guint64 stream_type = GST_APP_STREAM_TYPE_STREAM;

			LOGD("mem src is selected\n");

			element = gst_element_factory_make("appsrc", "buff-source");
			if ( !element )
			{
				LOGE("failed to create appsrc element\n");
				break;
			}

			g_object_set( element, "stream-type", stream_type, NULL );
			//g_object_set( element, "size", player->mem_buf.len, NULL );
			//g_object_set( element, "blocksize", (guint64)20480, NULL );

			MMPLAYER_SIGNAL_CONNECT( player, element, MM_PLAYER_SIGNAL_TYPE_OTHERS, "seek-data",
				G_CALLBACK(__gst_appsrc_seek_data), player);
			MMPLAYER_SIGNAL_CONNECT( player, element, MM_PLAYER_SIGNAL_TYPE_OTHERS, "need-data",
				G_CALLBACK(__gst_appsrc_feed_data), player);
			MMPLAYER_SIGNAL_CONNECT( player, element, MM_PLAYER_SIGNAL_TYPE_OTHERS, "enough-data",
				G_CALLBACK(__gst_appsrc_enough_data), player);
		}
		break;
		case MM_PLAYER_URI_TYPE_MS_BUFF:
		{
			LOGD("MS buff src is selected\n");

			if (player->v_stream_caps)
			{
				element = gst_element_factory_make("appsrc", "video_appsrc");
				if ( !element )
				{
					LOGF("failed to create video app source element[appsrc].\n" );
					break;
				}

				if ( player->a_stream_caps )
				{
					elem_src_audio = gst_element_factory_make("appsrc", "audio_appsrc");
					if ( !elem_src_audio )
					{
						LOGF("failed to create audio app source element[appsrc].\n" );
						break;
					}
				}
			}
			else if ( player->a_stream_caps )
			{
				/* no video, only audio pipeline*/
				element = gst_element_factory_make("appsrc", "audio_appsrc");
				if ( !element )
				{
					LOGF("failed to create audio app source element[appsrc].\n" );
					break;
				}
			}

			if ( player->s_stream_caps )
			{
				elem_src_subtitle = gst_element_factory_make("appsrc", "subtitle_appsrc");
				if ( !elem_src_subtitle )
				{
					LOGF("failed to create subtitle app source element[appsrc].\n" );
					break;
				}
			}

			LOGD("setting app sources properties.\n");
			LOGD("location : %s\n", player->profile.uri);

			if ( player->v_stream_caps && element )
			{
				g_object_set(G_OBJECT(element), "format", GST_FORMAT_TIME,
											    "blocksize", (guint)1048576,	/* size of many video frames are larger than default blocksize as 4096 */
												"caps", player->v_stream_caps, NULL);

				if ( player->media_stream_buffer_max_size[MM_PLAYER_STREAM_TYPE_VIDEO] > 0)
					g_object_set(G_OBJECT(element), "max-bytes", player->media_stream_buffer_max_size[MM_PLAYER_STREAM_TYPE_VIDEO], NULL);
				if ( player->media_stream_buffer_min_percent[MM_PLAYER_STREAM_TYPE_VIDEO] > 0)
					g_object_set(G_OBJECT(element), "min-percent", player->media_stream_buffer_min_percent[MM_PLAYER_STREAM_TYPE_VIDEO], NULL);

				/*Fix Seek External Demuxer:  set audio and video appsrc as seekable */
				gst_app_src_set_stream_type((GstAppSrc*)G_OBJECT(element), GST_APP_STREAM_TYPE_SEEKABLE);
				MMPLAYER_SIGNAL_CONNECT( player, element, MM_PLAYER_SIGNAL_TYPE_OTHERS, "seek-data",
														G_CALLBACK(__gst_seek_video_data), player);

				if (player->a_stream_caps && elem_src_audio)
				{
					g_object_set(G_OBJECT(elem_src_audio), "format", GST_FORMAT_TIME,
															"caps", player->a_stream_caps, NULL);

					if ( player->media_stream_buffer_max_size[MM_PLAYER_STREAM_TYPE_AUDIO] > 0)
						g_object_set(G_OBJECT(elem_src_audio), "max-bytes", player->media_stream_buffer_max_size[MM_PLAYER_STREAM_TYPE_AUDIO], NULL);
					if ( player->media_stream_buffer_min_percent[MM_PLAYER_STREAM_TYPE_AUDIO] > 0)
						g_object_set(G_OBJECT(elem_src_audio), "min-percent", player->media_stream_buffer_min_percent[MM_PLAYER_STREAM_TYPE_AUDIO], NULL);

					/*Fix Seek External Demuxer:  set audio and video appsrc as seekable */
					gst_app_src_set_stream_type((GstAppSrc*)G_OBJECT(elem_src_audio), GST_APP_STREAM_TYPE_SEEKABLE);
					MMPLAYER_SIGNAL_CONNECT( player, elem_src_audio, MM_PLAYER_SIGNAL_TYPE_OTHERS, "seek-data",
														G_CALLBACK(__gst_seek_audio_data), player);
				}
			}
			else if (player->a_stream_caps && element)
			{
				g_object_set(G_OBJECT(element), "format", GST_FORMAT_TIME,
												"caps", player->a_stream_caps, NULL);

				if ( player->media_stream_buffer_max_size[MM_PLAYER_STREAM_TYPE_AUDIO] > 0)
					g_object_set(G_OBJECT(element), "max-bytes", player->media_stream_buffer_max_size[MM_PLAYER_STREAM_TYPE_AUDIO], NULL);
				if ( player->media_stream_buffer_min_percent[MM_PLAYER_STREAM_TYPE_AUDIO] > 0)
					g_object_set(G_OBJECT(element), "min-percent", player->media_stream_buffer_min_percent[MM_PLAYER_STREAM_TYPE_AUDIO], NULL);

				/*Fix Seek External Demuxer:  set audio and video appsrc as seekable */
				gst_app_src_set_stream_type((GstAppSrc*)G_OBJECT(element), GST_APP_STREAM_TYPE_SEEKABLE);
				MMPLAYER_SIGNAL_CONNECT( player, element, MM_PLAYER_SIGNAL_TYPE_OTHERS, "seek-data",
															G_CALLBACK(__gst_seek_audio_data), player);
			}

			if (player->s_stream_caps && elem_src_subtitle)
			{
				g_object_set(G_OBJECT(elem_src_subtitle), "format", GST_FORMAT_TIME,
														 "caps", player->s_stream_caps, NULL);

				if ( player->media_stream_buffer_max_size[MM_PLAYER_STREAM_TYPE_TEXT] > 0)
					g_object_set(G_OBJECT(elem_src_subtitle), "max-bytes", player->media_stream_buffer_max_size[MM_PLAYER_STREAM_TYPE_TEXT], NULL);
				if ( player->media_stream_buffer_min_percent[MM_PLAYER_STREAM_TYPE_TEXT] > 0)
					g_object_set(G_OBJECT(elem_src_subtitle), "min-percent", player->media_stream_buffer_min_percent[MM_PLAYER_STREAM_TYPE_TEXT], NULL);

				gst_app_src_set_stream_type((GstAppSrc*)G_OBJECT(elem_src_subtitle), GST_APP_STREAM_TYPE_SEEKABLE);

				MMPLAYER_SIGNAL_CONNECT( player, elem_src_subtitle, MM_PLAYER_SIGNAL_TYPE_OTHERS, "seek-data",
																G_CALLBACK(__gst_seek_subtitle_data), player);
			}

			if (player->v_stream_caps && element)
			{
				MMPLAYER_SIGNAL_CONNECT( player, element, MM_PLAYER_SIGNAL_TYPE_OTHERS, "need-data",
														G_CALLBACK(__gst_appsrc_feed_video_data), player);
				MMPLAYER_SIGNAL_CONNECT( player, element, MM_PLAYER_SIGNAL_TYPE_OTHERS, "enough-data",
														G_CALLBACK(__gst_appsrc_enough_video_data), player);

				if (player->a_stream_caps && elem_src_audio)
				{
					MMPLAYER_SIGNAL_CONNECT( player, elem_src_audio, MM_PLAYER_SIGNAL_TYPE_OTHERS, "need-data",
														G_CALLBACK(__gst_appsrc_feed_audio_data), player);
					MMPLAYER_SIGNAL_CONNECT( player, element, MM_PLAYER_SIGNAL_TYPE_OTHERS, "enough-data",
														G_CALLBACK(__gst_appsrc_enough_audio_data), player);
				}
			}
			else if (player->a_stream_caps && element)
			{
				MMPLAYER_SIGNAL_CONNECT( player, element, MM_PLAYER_SIGNAL_TYPE_OTHERS, "need-data",
														G_CALLBACK(__gst_appsrc_feed_audio_data), player);
				MMPLAYER_SIGNAL_CONNECT( player, element, MM_PLAYER_SIGNAL_TYPE_OTHERS, "enough-data",
														G_CALLBACK(__gst_appsrc_enough_audio_data), player);
			}

			if (player->s_stream_caps && elem_src_subtitle)
			{
				MMPLAYER_SIGNAL_CONNECT( player, elem_src_subtitle, MM_PLAYER_SIGNAL_TYPE_OTHERS, "need-data",
														G_CALLBACK(__gst_appsrc_feed_subtitle_data), player);
			}

			need_state_holder = FALSE;
		}
		break;
		/* appsrc */
		case MM_PLAYER_URI_TYPE_MEM:
		{
			guint64 stream_type = GST_APP_STREAM_TYPE_RANDOM_ACCESS;

			LOGD("mem src is selected\n");

			element = gst_element_factory_make("appsrc", "mem-source");
			if ( !element )
			{
				LOGE("failed to create appsrc element\n");
				break;
			}

			g_object_set( element, "stream-type", stream_type, NULL );
			g_object_set( element, "size", player->mem_buf.len, NULL );
			g_object_set( element, "blocksize", (guint64)20480, NULL );

			MMPLAYER_SIGNAL_CONNECT( player, element, MM_PLAYER_SIGNAL_TYPE_OTHERS, "seek-data",
				G_CALLBACK(__gst_appsrc_seek_data_mem), &player->mem_buf );
			MMPLAYER_SIGNAL_CONNECT( player, element, MM_PLAYER_SIGNAL_TYPE_OTHERS, "need-data",
				G_CALLBACK(__gst_appsrc_feed_data_mem), &player->mem_buf );
		}
		break;
		case MM_PLAYER_URI_TYPE_URL:
		break;

		case MM_PLAYER_URI_TYPE_TEMP:
		break;

		case MM_PLAYER_URI_TYPE_NONE:
		default:
		break;
	}

	/* check source element is OK */
	if ( ! element )
	{
		LOGE("no source element was created.\n");
		goto INIT_ERROR;
	}

	/* take source element */
	mainbin[MMPLAYER_M_SRC].id = MMPLAYER_M_SRC;
	mainbin[MMPLAYER_M_SRC].gst = element;
	element_bucket = g_list_append(element_bucket, &mainbin[MMPLAYER_M_SRC]);

	if ((MMPLAYER_IS_STREAMING(player)) && (player->streamer == NULL))
	{
		player->streamer = __mm_player_streaming_create();
		__mm_player_streaming_initialize(player->streamer);
	}

	if ( MMPLAYER_IS_HTTP_PD(player) )
	{
		gdouble pre_buffering_time = (gdouble)player->streamer->buffering_req.initial_second;

		LOGD ("Picked queue2 element(pre buffer : %d sec)....\n", pre_buffering_time);
		element = gst_element_factory_make("queue2", "queue2");
		if ( !element )
		{
			LOGE ( "failed to create http streaming buffer element\n" );
			goto INIT_ERROR;
		}

		/* take it */
		mainbin[MMPLAYER_M_MUXED_S_BUFFER].id = MMPLAYER_M_MUXED_S_BUFFER;
		mainbin[MMPLAYER_M_MUXED_S_BUFFER].gst = element;
		element_bucket = g_list_append(element_bucket, &mainbin[MMPLAYER_M_MUXED_S_BUFFER]);

		pre_buffering_time = (pre_buffering_time > 0)?(pre_buffering_time):(player->ini.http_buffering_time);

		__mm_player_streaming_set_queue2(player->streamer,
				element,
				TRUE,
				player->ini.http_max_size_bytes,
				pre_buffering_time,
				1.0,
				player->ini.http_buffering_limit,
				FALSE,
				NULL,
				0);
	}
	if (MMPLAYER_IS_MS_BUFF_SRC(player))
	{
		if (player->v_stream_caps)
		{
			es_video_queue = gst_element_factory_make("queue2", "video_queue");
			if (!es_video_queue)
			{
				LOGE ("create es_video_queue for es player failed\n");
				goto INIT_ERROR;
			}
			mainbin[MMPLAYER_M_V_BUFFER].id = MMPLAYER_M_V_BUFFER;
			mainbin[MMPLAYER_M_V_BUFFER].gst = es_video_queue;
			element_bucket = g_list_append(element_bucket, &mainbin[MMPLAYER_M_V_BUFFER]);

			/* Adding audio appsrc to bucket */
			if (player->a_stream_caps && elem_src_audio)
			{
				mainbin[MMPLAYER_M_2ND_SRC].id = MMPLAYER_M_2ND_SRC;
				mainbin[MMPLAYER_M_2ND_SRC].gst = elem_src_audio;
				element_bucket = g_list_append(element_bucket, &mainbin[MMPLAYER_M_2ND_SRC]);

				es_audio_queue = gst_element_factory_make("queue2", "audio_queue");
				if (!es_audio_queue)
				{
					LOGE ("create es_audio_queue for es player failed\n");
					goto INIT_ERROR;
				}
				mainbin[MMPLAYER_M_A_BUFFER].id = MMPLAYER_M_A_BUFFER;
				mainbin[MMPLAYER_M_A_BUFFER].gst = es_audio_queue;
				element_bucket = g_list_append(element_bucket, &mainbin[MMPLAYER_M_A_BUFFER]);
			}
		}
		/* Only audio stream, no video */
		else if (player->a_stream_caps)
		{
			es_audio_queue = gst_element_factory_make("queue2", "audio_queue");
			if (!es_audio_queue)
			{
				LOGE ("create es_audio_queue for es player failed\n");
				goto INIT_ERROR;
			}
			mainbin[MMPLAYER_M_A_BUFFER].id = MMPLAYER_M_A_BUFFER;
			mainbin[MMPLAYER_M_A_BUFFER].gst = es_audio_queue;
			element_bucket = g_list_append(element_bucket, &mainbin[MMPLAYER_M_A_BUFFER]);
		}

		if (player->s_stream_caps && elem_src_subtitle)
		{
			mainbin[MMPLAYER_M_SUBSRC].id = MMPLAYER_M_SUBSRC;
			mainbin[MMPLAYER_M_SUBSRC].gst = elem_src_subtitle;
			element_bucket = g_list_append(element_bucket, &mainbin[MMPLAYER_M_SUBSRC]);

			es_subtitle_queue = gst_element_factory_make("queue2", "subtitle_queue");
			if (!es_subtitle_queue)
			{
				LOGE ("create es_subtitle_queue for es player failed\n");
				goto INIT_ERROR;
			}
			mainbin[MMPLAYER_M_S_BUFFER].id = MMPLAYER_M_V_BUFFER;
			mainbin[MMPLAYER_M_S_BUFFER].gst = es_subtitle_queue;
			element_bucket = g_list_append(element_bucket, &mainbin[MMPLAYER_M_S_BUFFER]);
		}
	}

	/* create autoplugging element if src element is not a rtsp src */
	if ((player->profile.uri_type != MM_PLAYER_URI_TYPE_URL_RTSP) &&
		(player->profile.uri_type != MM_PLAYER_URI_TYPE_URL_WFD) &&
		(player->profile.uri_type != MM_PLAYER_URI_TYPE_MS_BUFF))
	{
		element = NULL;
		enum MainElementID elemId = MMPLAYER_M_NUM;

		if ((player->use_decodebin) &&
			((MMPLAYER_IS_HTTP_PD(player)) ||
			 (!MMPLAYER_IS_HTTP_STREAMING(player))))
		{
			elemId = MMPLAYER_M_AUTOPLUG;
			element = __mmplayer_create_decodebin(player);
			need_state_holder = FALSE;
		}
		else
		{
			elemId = MMPLAYER_M_TYPEFIND;
			element = gst_element_factory_make("typefind", "typefinder");
			MMPLAYER_SIGNAL_CONNECT( player, element, MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "have-type",
				G_CALLBACK(__mmplayer_typefind_have_type), (gpointer)player );
		}


		/* check autoplug element is OK */
		if ( ! element )
		{
			LOGE("can not create element (%d)\n", elemId);
			goto INIT_ERROR;
		}

		mainbin[elemId].id = elemId;
		mainbin[elemId].gst = element;

		element_bucket = g_list_append(element_bucket, &mainbin[elemId]);
	}

	/* add elements to pipeline */
	if( !__mmplayer_gst_element_add_bucket_to_bin(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), element_bucket))
	{
		LOGE("Failed to add elements to pipeline\n");
		goto INIT_ERROR;
	}


	/* linking elements in the bucket by added order. */
	if ( __mmplayer_gst_element_link_bucket(element_bucket) == -1 )
	{
		LOGE("Failed to link some elements\n");
		goto INIT_ERROR;
	}


	/* create fakesink element for keeping the pipeline state PAUSED. if needed */
	if ( need_state_holder )
	{
		/* create */
		mainbin[MMPLAYER_M_SRC_FAKESINK].id = MMPLAYER_M_SRC_FAKESINK;
		mainbin[MMPLAYER_M_SRC_FAKESINK].gst = gst_element_factory_make ("fakesink", "state-holder");

		if (!mainbin[MMPLAYER_M_SRC_FAKESINK].gst)
		{
			LOGE ("fakesink element could not be created\n");
			goto INIT_ERROR;
		}
		GST_OBJECT_FLAG_UNSET (mainbin[MMPLAYER_M_SRC_FAKESINK].gst, GST_ELEMENT_FLAG_SINK);

		/* take ownership of fakesink. we are reusing it */
		gst_object_ref( mainbin[MMPLAYER_M_SRC_FAKESINK].gst );

		/* add */
		if ( FALSE == gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst),
			mainbin[MMPLAYER_M_SRC_FAKESINK].gst) )
		{
			LOGE("failed to add fakesink to bin\n");
			goto INIT_ERROR;
		}
	}

	/* now we have completed mainbin. take it */
	player->pipeline->mainbin = mainbin;

	if (MMPLAYER_IS_MS_BUFF_SRC(player))
	{
		GstPad *srcpad = NULL;

		if (mainbin[MMPLAYER_M_V_BUFFER].gst)
		{
			srcpad = gst_element_get_static_pad(mainbin[MMPLAYER_M_V_BUFFER].gst, "src");
			if (srcpad)
			{
				__mmplayer_gst_create_decoder ( player,
												MM_PLAYER_TRACK_TYPE_VIDEO,
												srcpad,
												MMPLAYER_M_AUTOPLUG_V_DEC,
												"video_decodebin");

				gst_object_unref ( GST_OBJECT(srcpad) );
				srcpad = NULL;
			}
		}

		if ((player->a_stream_caps) && (mainbin[MMPLAYER_M_A_BUFFER].gst))
		{
			srcpad = gst_element_get_static_pad(mainbin[MMPLAYER_M_A_BUFFER].gst, "src");
			if (srcpad)
			{
				__mmplayer_gst_create_decoder ( player,
												MM_PLAYER_TRACK_TYPE_AUDIO,
												srcpad,
												MMPLAYER_M_AUTOPLUG_A_DEC,
												"audio_decodebin");

				gst_object_unref ( GST_OBJECT(srcpad) );
				srcpad = NULL;
			} // else error
		} //  else error

		if (mainbin[MMPLAYER_M_S_BUFFER].gst)
		{
			__mmplayer_try_to_plug_decodebin(player, gst_element_get_static_pad(mainbin[MMPLAYER_M_S_BUFFER].gst, "src"), player->s_stream_caps);
		}
	}

	/* connect bus callback */
	bus = gst_pipeline_get_bus(GST_PIPELINE(mainbin[MMPLAYER_M_PIPE].gst));
	if ( !bus )
	{
		LOGE ("cannot get bus from pipeline.\n");
		goto INIT_ERROR;
	}

	player->bus_watcher = gst_bus_add_watch(bus, (GstBusFunc)__mmplayer_gst_callback, player);

	player->context.thread_default = g_main_context_get_thread_default();

	if (NULL == player->context.thread_default)
	{
		player->context.thread_default = g_main_context_default();
		LOGD("thread-default context is the global default context");
	}
	LOGW("bus watcher thread context = %p, watcher : %d", player->context.thread_default, player->bus_watcher);

	/* Note : check whether subtitle atrribute uri is set. If uri is set, then try to play subtitle file */
	if ( __mmplayer_check_subtitle ( player ) )
	{
		if ( MM_ERROR_NONE != __mmplayer_gst_create_subtitle_src(player) )
			LOGE("fail to create subtitle src\n");
	}

	/* set sync handler to get tag synchronously */
	gst_bus_set_sync_handler(bus, __mmplayer_bus_sync_callback, player, NULL);

	/* finished */
	gst_object_unref(GST_OBJECT(bus));
	g_list_free(element_bucket);

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;

INIT_ERROR:

	__mmplayer_gst_destroy_pipeline(player);
	g_list_free(element_bucket);

	/* release element which are not added to bin */
	for ( i = 1; i < MMPLAYER_M_NUM; i++ ) 	/* NOTE : skip pipeline */
	{
		if ( mainbin[i].gst )
		{
			GstObject* parent = NULL;
			parent = gst_element_get_parent( mainbin[i].gst );

			if ( !parent )
			{
				gst_object_unref(GST_OBJECT(mainbin[i].gst));
				mainbin[i].gst = NULL;
			}
			else
			{
				gst_object_unref(GST_OBJECT(parent));
			}
		}
	}

	/* release pipeline with it's childs */
	if ( mainbin[MMPLAYER_M_PIPE].gst )
	{
		gst_object_unref(GST_OBJECT(mainbin[MMPLAYER_M_PIPE].gst));
	}

	MMPLAYER_FREEIF( player->pipeline );
	MMPLAYER_FREEIF( mainbin );

	return MM_ERROR_PLAYER_INTERNAL;
}

static void
__mmplayer_reset_gapless_state(mm_player_t* player)
{
	MMPLAYER_FENTER();
	MMPLAYER_RETURN_IF_FAIL(player
		&& player->pipeline
		&& player->pipeline->audiobin
		&& player->pipeline->audiobin[MMPLAYER_A_BIN].gst);

	if (player->gapless.audio_data_probe_id != 0)
	{
		GstPad *sinkpad;
		sinkpad = gst_element_get_static_pad(player->pipeline->audiobin[MMPLAYER_A_BIN].gst, "sink");
		gst_pad_remove_probe (sinkpad, player->gapless.audio_data_probe_id);
		gst_object_unref (sinkpad);
	}

	if (player->gapless.video_data_probe_id != 0)
	{
		GstPad *sinkpad;
		sinkpad = gst_element_get_static_pad(player->pipeline->videobin[MMPLAYER_V_BIN].gst, "sink");
		gst_pad_remove_probe (sinkpad, player->gapless.video_data_probe_id);
		gst_object_unref (sinkpad);
	}
	memset(&player->gapless, 0, sizeof(mm_player_gapless_t));

	MMPLAYER_FLEAVE();
	return;
}

static int
__mmplayer_gst_destroy_pipeline(mm_player_t* player) // @
{
	gint timeout = 0;
	int ret = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_INVALID_HANDLE );

	/* cleanup stuffs */
	MMPLAYER_FREEIF(player->type);
	player->have_dynamic_pad = FALSE;
	player->no_more_pad = FALSE;
	player->num_dynamic_pad = 0;
	player->demux_pad_index = 0;
	player->subtitle_language_list = NULL;
	player->use_deinterleave = FALSE;
	player->max_audio_channels = 0;
	player->video_share_api_delta = 0;
	player->video_share_clock_delta = 0;
	player->video_hub_download_mode = 0;
	__mmplayer_reset_gapless_state(player);
	__mmplayer_post_proc_reset(player);

	if (player->streamer)
	{
		__mm_player_streaming_deinitialize (player->streamer);
		__mm_player_streaming_destroy(player->streamer);
		player->streamer = NULL;
	}

	/* cleanup unlinked mime type */
	MMPLAYER_FREEIF(player->unlinked_audio_mime);
	MMPLAYER_FREEIF(player->unlinked_video_mime);
	MMPLAYER_FREEIF(player->unlinked_demuxer_mime);

	/* cleanup running stuffs */
	__mmplayer_cancel_eos_timer( player );
#if 0 //need to change and test
	/* remove sound cb */
	if ( MM_ERROR_NONE != mm_sound_remove_device_information_changed_callback())
	{
		LOGE("failed to mm_sound_remove_device_information_changed_callback()");
	}
#endif
	/* cleanup gst stuffs */
	if ( player->pipeline )
	{
		MMPlayerGstElement* mainbin = player->pipeline->mainbin;
		GstTagList* tag_list = player->pipeline->tag_list;

		/* first we need to disconnect all signal hander */
		__mmplayer_release_signal_connection( player, MM_PLAYER_SIGNAL_TYPE_ALL );

		/* disconnecting bus watch */
		if ( player->bus_watcher )
			__mmplayer_remove_g_source_from_context(player->context.thread_default, player->bus_watcher);
		player->bus_watcher = 0;

		if ( mainbin )
		{
			MMPlayerGstElement* audiobin = player->pipeline->audiobin;
			MMPlayerGstElement* videobin = player->pipeline->videobin;
			MMPlayerGstElement* textbin = player->pipeline->textbin;
			GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (mainbin[MMPLAYER_M_PIPE].gst));
			gst_bus_set_sync_handler (bus, NULL, NULL, NULL);
			gst_object_unref(bus);

			timeout = MMPLAYER_STATE_CHANGE_TIMEOUT(player);
			ret = __mmplayer_gst_set_state ( player, mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_NULL, FALSE, timeout );
			if ( ret != MM_ERROR_NONE )
			{
				LOGE("fail to change state to NULL\n");
				return MM_ERROR_PLAYER_INTERNAL;
			}

			LOGW("succeeded in chaning state to NULL\n");

			gst_object_unref(GST_OBJECT(mainbin[MMPLAYER_M_PIPE].gst));

			/* free fakesink */
			if ( mainbin[MMPLAYER_M_SRC_FAKESINK].gst )
				gst_object_unref(GST_OBJECT(mainbin[MMPLAYER_M_SRC_FAKESINK].gst));

			/* free avsysaudiosink
			   avsysaudiosink should be unref when destory pipeline just after start play with BT.
			   Because audiosink is created but never added to bin, and therefore it will not be unref when pipeline is destroyed.
			*/
			MMPLAYER_FREEIF( audiobin );
			MMPLAYER_FREEIF( videobin );
			MMPLAYER_FREEIF( textbin );
			MMPLAYER_FREEIF( mainbin );
		}

		if ( tag_list )
			gst_tag_list_free(tag_list);

		MMPLAYER_FREEIF( player->pipeline );
	}
	MMPLAYER_FREEIF(player->album_art);

	if (player->v_stream_caps)
	{
		gst_caps_unref(player->v_stream_caps);
		player->v_stream_caps = NULL;
	}
	if (player->a_stream_caps)
	{
		gst_caps_unref(player->a_stream_caps);
		player->a_stream_caps = NULL;
	}
	if (player->s_stream_caps)
	{
		gst_caps_unref(player->s_stream_caps);
		player->s_stream_caps = NULL;
	}
	_mmplayer_track_destroy(player);

	if ( player->sink_elements )
		g_list_free ( player->sink_elements );
	player->sink_elements = NULL;

	LOGW("finished destroy pipeline\n");

	MMPLAYER_FLEAVE();

	return ret;
}

static int __gst_realize(mm_player_t* player) // @
{
	gint timeout = 0;
	int ret = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_READY;

	ret = __mmplayer_gst_create_pipeline(player);
	if ( ret )
	{
		LOGE("failed to create pipeline\n");
		return ret;
	}

	/* set pipeline state to READY */
	/* NOTE : state change to READY must be performed sync. */
	timeout = MMPLAYER_STATE_CHANGE_TIMEOUT(player);
	ret = __mmplayer_gst_set_state(player,
				player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_READY, FALSE, timeout);

	if ( ret != MM_ERROR_NONE )
	{
		/* return error if failed to set state */
		LOGE("failed to set READY state");
		return ret;
	}
	else
	{
		MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_READY );
	}

	/* create dot before error-return. for debugging */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-realize" );

	MMPLAYER_FLEAVE();

	return ret;
}

static int __gst_unrealize(mm_player_t* player) // @
{
	int ret = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_NULL;
	MMPLAYER_PRINT_STATE(player);

	/* release miscellaneous information */
	__mmplayer_release_misc( player );

	/* destroy pipeline */
	ret = __mmplayer_gst_destroy_pipeline( player );
	if ( ret != MM_ERROR_NONE )
	{
		LOGE("failed to destory pipeline\n");
		return ret;
	}

	/* release miscellaneous information.
	   these info needs to be released after pipeline is destroyed. */
	__mmplayer_release_misc_post( player );

	MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_NULL );

	MMPLAYER_FLEAVE();

	return ret;
}

static int __gst_pending_seek ( mm_player_t* player )
{
	MMPlayerStateType current_state = MM_PLAYER_STATE_NONE;
	int ret = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );

	if ( !player->pending_seek.is_pending )
	{
		LOGD("pending seek is not reserved. nothing to do.\n" );
		return ret;
	}

	/* check player state if player could pending seek or not. */
	current_state = MMPLAYER_CURRENT_STATE(player);

	if ( current_state != MM_PLAYER_STATE_PAUSED && current_state != MM_PLAYER_STATE_PLAYING  )
	{
		LOGW("try to pending seek in %s state, try next time. \n",
			MMPLAYER_STATE_GET_NAME(current_state));
		return ret;
	}

	LOGD("trying to play from (%lu) pending position\n", player->pending_seek.pos);

	ret = __gst_set_position ( player, player->pending_seek.format, player->pending_seek.pos, FALSE );

	if ( MM_ERROR_NONE != ret )
		LOGE("failed to seek pending postion. just keep staying current position.\n");

	player->pending_seek.is_pending = FALSE;

	MMPLAYER_FLEAVE();

	return ret;
}

static int __gst_start(mm_player_t* player) // @
{
	gboolean sound_extraction = 0;
	int ret = MM_ERROR_NONE;
	gboolean async = FALSE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* get sound_extraction property */
	mm_attrs_get_int_by_name(player->attrs, "pcm_extraction", &sound_extraction);

	/* NOTE : if SetPosition was called before Start. do it now */
	/* streaming doesn't support it. so it should be always sync */
	/* !! create one more api to check if there is pending seek rather than checking variables */
	if ( (player->pending_seek.is_pending || sound_extraction) && !MMPLAYER_IS_STREAMING(player))
	{
		MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_PAUSED;
		ret = __gst_pause(player, FALSE);
		if ( ret != MM_ERROR_NONE )
		{
			LOGE("failed to set state to PAUSED for pending seek\n");
			return ret;
		}

		MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_PLAYING;

		if ( sound_extraction )
		{
			LOGD("setting pcm extraction\n");

			ret = __mmplayer_set_pcm_extraction(player);
			if ( MM_ERROR_NONE != ret )
			{
				LOGW("failed to set pcm extraction\n");
				return ret;
			}
		}
		else
		{
			if ( MM_ERROR_NONE != __gst_pending_seek(player) )
			{
				LOGW("failed to seek pending postion. starting from the begin of content.\n");
			}
		}
	}

	LOGD("current state before doing transition");
	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_PLAYING;
	MMPLAYER_PRINT_STATE(player);

	/* set pipeline state to PLAYING  */
	if (player->es_player_push_mode)
	{
		async = TRUE;
	}
	/* set pipeline state to PLAYING  */
	ret = __mmplayer_gst_set_state(player,
		player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_PLAYING, async, MMPLAYER_STATE_CHANGE_TIMEOUT(player) );

	if (ret == MM_ERROR_NONE)
	{
		MMPLAYER_SET_STATE(player, MM_PLAYER_STATE_PLAYING);
	}
	else
	{
		LOGE("failed to set state to PLAYING");
		return ret;
	}

	/* generating debug info before returning error */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-start" );

	MMPLAYER_FLEAVE();

	return ret;
}

static void __mmplayer_do_sound_fadedown(mm_player_t* player, unsigned int time)
{
	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL(player
		&& player->pipeline
		&& player->pipeline->audiobin
		&& player->pipeline->audiobin[MMPLAYER_A_SINK].gst);

	g_object_set(G_OBJECT(player->pipeline->audiobin[MMPLAYER_A_SINK].gst), "mute", 2, NULL);

	usleep(time);

	MMPLAYER_FLEAVE();
}

static void __mmplayer_undo_sound_fadedown(mm_player_t* player)
{
	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL(player
		&& player->pipeline
		&& player->pipeline->audiobin
		&& player->pipeline->audiobin[MMPLAYER_A_SINK].gst);

	g_object_set(G_OBJECT(player->pipeline->audiobin[MMPLAYER_A_SINK].gst), "mute", 0, NULL);

	MMPLAYER_FLEAVE();
}

static int __gst_stop(mm_player_t* player) // @
{
	GstStateChangeReturn change_ret = GST_STATE_CHANGE_SUCCESS;
	MMHandleType attrs = 0;
	gboolean fadedown = FALSE;
	gboolean rewind = FALSE;
	gint timeout = 0;
	int ret = MM_ERROR_NONE;
	GstState state;
	gboolean async = FALSE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED);
	MMPLAYER_RETURN_VAL_IF_FAIL ( player->pipeline->mainbin, MM_ERROR_PLAYER_NOT_INITIALIZED);

	LOGD("current state before doing transition");
	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_READY;
	MMPLAYER_PRINT_STATE(player);

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		LOGE("cannot get content attribute\n");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	mm_attrs_get_int_by_name(attrs, "sound_fadedown", &fadedown);

	/* enable fadedown */
	if (fadedown || player->sound_focus.by_asm_cb)
		__mmplayer_do_sound_fadedown(player, MM_PLAYER_FADEOUT_TIME_DEFAULT);

	/* Just set state to PAUESED and the rewind. it's usual player behavior. */
	timeout = MMPLAYER_STATE_CHANGE_TIMEOUT ( player );

	if (player->profile.uri_type == MM_PLAYER_URI_TYPE_BUFF ||
		player->profile.uri_type == MM_PLAYER_URI_TYPE_HLS)
	{
		state = GST_STATE_READY;
	}
	else
	{
		state = GST_STATE_PAUSED;

		if ( ! MMPLAYER_IS_STREAMING(player) ||
			(player->streaming_type == STREAMING_SERVICE_VOD && player->videodec_linked)) {
			rewind = TRUE;
		}
	}

	if (player->es_player_push_mode)
	{
		async = TRUE;
	}
	/* set gst state */
	ret = __mmplayer_gst_set_state( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, state, async, timeout );

	/* disable fadeout */
	if (fadedown || player->sound_focus.by_asm_cb)
		__mmplayer_undo_sound_fadedown(player);

	/* return if set_state has failed */
	if ( ret != MM_ERROR_NONE )
	{
		LOGE("failed to set state.\n");
		return ret;
	}

	/* rewind */
	if ( rewind )
	{
		if ( ! __gst_seek( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, player->playback_rate,
				GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, 0,
				GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE) )
		{
			LOGW("failed to rewind\n");
			ret = MM_ERROR_PLAYER_SEEK;
		}
	}

	/* initialize */
	player->sent_bos = FALSE;

	if (player->es_player_push_mode) //for cloudgame
	{
		timeout = 0;
	}

	/* wait for seek to complete */
	change_ret = gst_element_get_state (player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, NULL, NULL, timeout * GST_SECOND);
	if ( change_ret == GST_STATE_CHANGE_SUCCESS || change_ret == GST_STATE_CHANGE_NO_PREROLL )
	{
		MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_READY );
	}
	else
	{
		LOGE("fail to stop player.\n");
		ret = MM_ERROR_PLAYER_INTERNAL;
		__mmplayer_dump_pipeline_state(player);
	}

	/* generate dot file if enabled */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-stop" );

	MMPLAYER_FLEAVE();

	return ret;
}

int __gst_pause(mm_player_t* player, gboolean async) // @
{
	int ret = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL(player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED);
	MMPLAYER_RETURN_VAL_IF_FAIL(player->pipeline->mainbin, MM_ERROR_PLAYER_NOT_INITIALIZED);

	LOGD("current state before doing transition");
	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_PAUSED;
	MMPLAYER_PRINT_STATE(player);

	/* set pipeline status to PAUSED */
	player->ignore_asyncdone = TRUE;

	ret = __mmplayer_gst_set_state(player,
		player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_PAUSED, async, MMPLAYER_STATE_CHANGE_TIMEOUT(player));

	player->ignore_asyncdone = FALSE;

	if ( FALSE == async )
	{
		if ( ret != MM_ERROR_NONE )
		{
			GstMessage *msg = NULL;
			GTimer *timer = NULL;
			gdouble MAX_TIMEOUT_SEC = 3;

			LOGE("failed to set state to PAUSED");

			timer = g_timer_new();
			g_timer_start(timer);

			GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst));
			gboolean got_msg = FALSE;
			/* check if gst error posted or not */
			do
			{
				msg = gst_bus_timed_pop(bus, GST_SECOND /2);
				if (msg)
				{
					if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
					{
						GError *error = NULL;

						/* parse error code */
						gst_message_parse_error(msg, &error, NULL);

						if ( gst_structure_has_name ( gst_message_get_structure(msg), "streaming_error" ) )
						{
							/* Note : the streaming error from the streaming source is handled
							 *   using __mmplayer_handle_streaming_error.
							 */
							__mmplayer_handle_streaming_error ( player, msg );

						}
						else if (error)
						{
							LOGE("paring error posted from bus, domain : %s, code : %d", g_quark_to_string(error->domain), error->code);

							if (error->domain == GST_STREAM_ERROR)
							{
								ret = __gst_handle_stream_error( player, error, msg );
							}
							else if (error->domain == GST_RESOURCE_ERROR)
							{
								ret = __gst_handle_resource_error( player, error->code );
							}
							else if (error->domain == GST_LIBRARY_ERROR)
							{
								ret = __gst_handle_library_error( player, error->code );
							}
							else if (error->domain == GST_CORE_ERROR)
							{
								ret = __gst_handle_core_error( player, error->code );
							}
						}

						got_msg = TRUE;
						player->msg_posted = TRUE;
					}
					gst_message_unref(msg);
				}
			} while (!got_msg && (g_timer_elapsed(timer, NULL) < MAX_TIMEOUT_SEC));
			/* clean */
			gst_object_unref(bus);
			g_timer_stop (timer);
			g_timer_destroy (timer);

			return ret;
		}
		else if ( !player->video_stream_cb && (!player->pipeline->videobin) && (!player->pipeline->audiobin) )
		{
			if (MMPLAYER_IS_RTSP_STREAMING(player))
				return ret;
			return MM_ERROR_PLAYER_CODEC_NOT_FOUND;
		}
		else if ( ret== MM_ERROR_NONE)
		{
			MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_PAUSED );
		}
	}

	/* generate dot file before returning error */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-pause" );

	MMPLAYER_FLEAVE();

	return ret;
}

int __gst_resume(mm_player_t* player, gboolean async) // @
{
	int ret = MM_ERROR_NONE;
	gint timeout = 0;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL(player && player->pipeline,
		MM_ERROR_PLAYER_NOT_INITIALIZED);

	LOGD("current state before doing transition");
	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_PLAYING;
	MMPLAYER_PRINT_STATE(player);

	/* generate dot file before returning error */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-resume" );

	if ( async )
		LOGD("do async state transition to PLAYING.\n");

	/* set pipeline state to PLAYING */
	timeout = MMPLAYER_STATE_CHANGE_TIMEOUT(player);

	ret = __mmplayer_gst_set_state(player,
		player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_PLAYING, async, timeout );
	if (ret != MM_ERROR_NONE)
	{
		LOGE("failed to set state to PLAYING\n");
		return ret;
	}
	else
	{
		if (async == FALSE)
		{
			// MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_PLAYING );
			LOGD("update state machine to %d\n", MM_PLAYER_STATE_PLAYING);
			ret = __mmplayer_set_state(player, MM_PLAYER_STATE_PLAYING);
		}
	}

	/* generate dot file before returning error */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-resume" );

	MMPLAYER_FLEAVE();

	return ret;
}

static int
__gst_set_position(mm_player_t* player, int format, unsigned long position, gboolean internal_called) // @
{
	unsigned long dur_msec = 0;
	gint64 dur_nsec = 0;
	gint64 pos_nsec = 0;
	gboolean ret = TRUE;
	gboolean accurated = FALSE;
	GstSeekFlags seek_flags = GST_SEEK_FLAG_FLUSH;

	MMPLAYER_FENTER();
	MMPLAYER_RETURN_VAL_IF_FAIL ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );
	MMPLAYER_RETURN_VAL_IF_FAIL ( !MMPLAYER_IS_LIVE_STREAMING(player), MM_ERROR_PLAYER_NO_OP );

	if ( MMPLAYER_CURRENT_STATE(player) != MM_PLAYER_STATE_PLAYING
		&& MMPLAYER_CURRENT_STATE(player) != MM_PLAYER_STATE_PAUSED )
		goto PENDING;

	if( !MMPLAYER_IS_MS_BUFF_SRC(player) )
	{
		/* check duration */
		/* NOTE : duration cannot be zero except live streaming.
		 * 		Since some element could have some timing problemn with quering duration, try again.
		 */
		if ( !player->duration )
		{
			if ( !gst_element_query_duration( player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_FORMAT_TIME, &dur_nsec ))
			{
				goto SEEK_ERROR;
			}
			player->duration = dur_nsec;
		}

		if ( player->duration )
		{
			dur_msec = GST_TIME_AS_MSECONDS(player->duration);
		}
		else
		{
			LOGE("could not get the duration. fail to seek.\n");
			goto SEEK_ERROR;
		}
	}
	LOGD("playback rate: %f\n", player->playback_rate);

	mm_attrs_get_int_by_name(player->attrs, "accurate_seek", &accurated);
	if (accurated)
	{
		seek_flags |= GST_SEEK_FLAG_ACCURATE;
	}
	else
	{
		seek_flags |= GST_SEEK_FLAG_KEY_UNIT;
	}

	/* do seek */
	switch ( format )
	{
		case MM_PLAYER_POS_FORMAT_TIME:
		{
			if( !MMPLAYER_IS_MS_BUFF_SRC(player) )
			{
				/* check position is valid or not */
				if ( position > dur_msec )
					goto INVALID_ARGS;

				LOGD("seeking to (%lu) msec, duration is %d msec\n", position, dur_msec);

				if ( player->doing_seek )
				{
					LOGD("not completed seek");
					return MM_ERROR_PLAYER_DOING_SEEK;
				}
			}

			if ( !internal_called )
				player->doing_seek = TRUE;

			pos_nsec = position * G_GINT64_CONSTANT(1000000);

			if ((MMPLAYER_IS_HTTP_STREAMING(player)) && (!player->videodec_linked))
			{
				gint64 cur_time = 0;

				/* get current position */
				gst_element_query_position(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_FORMAT_TIME, &cur_time);

				/* flush */
				GstEvent *event = gst_event_new_seek (1.0,
								GST_FORMAT_TIME,
								(GstSeekFlags)GST_SEEK_FLAG_FLUSH,
								GST_SEEK_TYPE_SET, cur_time,
								GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
				if(event) {
					__gst_send_event_to_sink(player, event);
				}

				__gst_pause( player, FALSE );
			}

			ret = __gst_seek ( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, player->playback_rate,
							GST_FORMAT_TIME, seek_flags,
							GST_SEEK_TYPE_SET, pos_nsec, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE );
			if ( !ret  )
			{
				LOGE("failed to set position. dur[%lu]  pos[%lu]  pos_msec[%llu]\n", dur_msec, position, pos_nsec);
				goto SEEK_ERROR;
			}
		}
		break;

		case MM_PLAYER_POS_FORMAT_PERCENT:
		{
			LOGD("seeking to (%lu)%% \n", position);

			if (player->doing_seek)
			{
				LOGD("not completed seek");
				return MM_ERROR_PLAYER_DOING_SEEK;
			}

			if ( !internal_called)
				player->doing_seek = TRUE;

			/* FIXIT : why don't we use 'GST_FORMAT_PERCENT' */
			pos_nsec = (gint64) ( ( position * player->duration ) / 100 );
			ret = __gst_seek ( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, player->playback_rate,
							GST_FORMAT_TIME, seek_flags,
							GST_SEEK_TYPE_SET, pos_nsec, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE );
			if ( !ret  )
			{
				LOGE("failed to set position. dur[%lud]  pos[%lud]  pos_msec[%"G_GUINT64_FORMAT"]\n", dur_msec, position, pos_nsec);
				goto SEEK_ERROR;
			}
		}
		break;

		default:
			goto INVALID_ARGS;

	}

	/* NOTE : store last seeking point to overcome some bad operation
	  *      ( returning zero when getting current position ) of some elements
	  */
	player->last_position = pos_nsec;

	/* MSL should guarante playback rate when seek is selected during trick play of fast forward. */
	if ( player->playback_rate > 1.0 )
		_mmplayer_set_playspeed ((MMHandleType)player, player->playback_rate, FALSE);

	MMPLAYER_FLEAVE();
	return MM_ERROR_NONE;

PENDING:
	player->pending_seek.is_pending = TRUE;
	player->pending_seek.format = format;
	player->pending_seek.pos = position;

	LOGW("player current-state : %s, pending-state : %s, just preserve pending position(%lu).\n",
		MMPLAYER_STATE_GET_NAME(MMPLAYER_CURRENT_STATE(player)), MMPLAYER_STATE_GET_NAME(MMPLAYER_PENDING_STATE(player)), player->pending_seek.pos);

	return MM_ERROR_NONE;

INVALID_ARGS:
	LOGE("invalid arguments, position : %ld  dur : %ld format : %d \n", position, dur_msec, format);
	return MM_ERROR_INVALID_ARGUMENT;

SEEK_ERROR:
	player->doing_seek = FALSE;
	return MM_ERROR_PLAYER_SEEK;
}

#define TRICKPLAY_OFFSET GST_MSECOND

static int
__gst_get_position(mm_player_t* player, int format, unsigned long* position) // @
{
	MMPlayerStateType current_state = MM_PLAYER_STATE_NONE;
	gint64 pos_msec = 0;
	gboolean ret = TRUE;

	MMPLAYER_RETURN_VAL_IF_FAIL( player && position && player->pipeline && player->pipeline->mainbin,
		MM_ERROR_PLAYER_NOT_INITIALIZED );

	current_state = MMPLAYER_CURRENT_STATE(player);

	/* NOTE : query position except paused state to overcome some bad operation
	 * please refer to below comments in details
	 */
	if ( current_state != MM_PLAYER_STATE_PAUSED )
	{
		ret = gst_element_query_position(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_FORMAT_TIME, &pos_msec);
	}

	/* NOTE : get last point to overcome some bad operation of some elements
	 * ( returning zero when getting current position in paused state
	 * and when failed to get postion during seeking
	 */
	if ( ( current_state == MM_PLAYER_STATE_PAUSED )
		|| ( ! ret ))
		//|| ( player->last_position != 0 && pos_msec == 0 ) )
	{
		LOGD ("pos_msec = %"GST_TIME_FORMAT" and ret = %d and state = %d", GST_TIME_ARGS (pos_msec), ret, current_state);

		if(player->playback_rate < 0.0)
			pos_msec = player->last_position - TRICKPLAY_OFFSET;
		else
			pos_msec = player->last_position;

		if (!ret)
			pos_msec = player->last_position;
		else
			player->last_position = pos_msec;

		LOGD("returning last point : %"GST_TIME_FORMAT, GST_TIME_ARGS(pos_msec));

	}
	else
	{
		if (player->duration > 0 && pos_msec > player->duration) {
			pos_msec = player->duration;
		}

		if (player->sound_focus.keep_last_pos) {
			LOGD("return last pos as stop by asm, %"GST_TIME_FORMAT, GST_TIME_ARGS(player->last_position));
			pos_msec = player->last_position;
		}
		else {
			player->last_position = pos_msec;
		}
	}

	switch (format) {
		case MM_PLAYER_POS_FORMAT_TIME:
			*position = GST_TIME_AS_MSECONDS(pos_msec);
			break;

		case MM_PLAYER_POS_FORMAT_PERCENT:
		{
			gint64 dur = 0;
			gint64 pos = 0;

			dur = player->duration / GST_SECOND;
			if (dur <= 0)
			{
				LOGD ("duration is [%d], so returning position 0\n",dur);
				*position = 0;
			}
			else
			{
				pos = pos_msec / GST_SECOND;
				*position = pos * 100 / dur;
			}
			break;
		}
		default:
			return MM_ERROR_PLAYER_INTERNAL;
	}

	return MM_ERROR_NONE;
}


static int 	__gst_get_buffer_position(mm_player_t* player, int format, unsigned long* start_pos, unsigned long* stop_pos)
{
#define STREAMING_IS_FINISHED	0
#define BUFFERING_MAX_PER	100

	GstQuery *query = NULL;

	MMPLAYER_RETURN_VAL_IF_FAIL( player &&
						player->pipeline &&
						player->pipeline->mainbin,
						MM_ERROR_PLAYER_NOT_INITIALIZED );

	MMPLAYER_RETURN_VAL_IF_FAIL( start_pos && stop_pos, MM_ERROR_INVALID_ARGUMENT );

	if (!MMPLAYER_IS_HTTP_STREAMING ( player ))
	{
		/* and rtsp is not ready yet. */
		LOGW ( "it's only used for http streaming case.\n" );
		return MM_ERROR_NONE;
	}

	*start_pos = 0;
	*stop_pos = 0;

	switch ( format )
	{
		case MM_PLAYER_POS_FORMAT_PERCENT :
		{
			gint start_per = -1, stop_per = -1;
			gint64 buffered_total = 0;

			unsigned long position = 0;
			guint curr_size_bytes = 0;
			gint64 buffering_left = -1;
			gint buffered_sec = -1;

			gint64 content_duration = player->duration;
			guint64 content_size = player->http_content_size;

			if (content_duration > 0)
			{
				if (!__gst_get_position(player, MM_PLAYER_POS_FORMAT_TIME, &position))
				{
					LOGD ("[Time] pos %d ms / dur %d sec / %lld bytes", position, (guint)(content_duration/GST_SECOND), content_size);
					start_per = 100 * (position*GST_MSECOND) / content_duration;

					/* buffered size info from multiqueue */
					if (player->pipeline->mainbin[MMPLAYER_M_DEMUXED_S_BUFFER].gst)
					{
						g_object_get(G_OBJECT(player->pipeline->mainbin[MMPLAYER_M_DEMUXED_S_BUFFER].gst), "curr-size-bytes", &curr_size_bytes, NULL);
						LOGD ("[MQ] curr_size_bytes = %d", curr_size_bytes);

						buffered_total += curr_size_bytes;
					}

					/* buffered size info from queue2 */
					if (player->pipeline->mainbin[MMPLAYER_M_MUXED_S_BUFFER].gst)
					{
						query = gst_query_new_buffering ( GST_FORMAT_BYTES );
						if (gst_element_query(player->pipeline->mainbin[MMPLAYER_M_MUXED_S_BUFFER].gst, query))
						{
							GstBufferingMode mode;
							gint byte_in_rate = 0, byte_out_rate = 0;
							gint64 start_byte = 0, stop_byte = 0;
							guint num_of_ranges = 0;
							guint idx = 0;

							num_of_ranges = gst_query_get_n_buffering_ranges(query);
							for ( idx=0 ; idx<num_of_ranges ; idx++ )
							{
								gst_query_parse_nth_buffering_range (query, idx, &start_byte, &stop_byte);
								LOGD ("[Q2][range %d] %lld ~ %lld\n", idx, start_byte, stop_byte);

								buffered_total += (stop_byte - start_byte);
							}

							gst_query_parse_buffering_stats(query, &mode, &byte_in_rate, &byte_out_rate, &buffering_left);
							LOGD ("[Q2] in_rate %d, out_rate %d, left %lld\n", byte_in_rate, byte_out_rate, buffering_left);
						}
						gst_query_unref (query);
					}

					if (buffering_left == STREAMING_IS_FINISHED)
					{
						stop_per = BUFFERING_MAX_PER;
					}
					else
					{
						guint dur_sec = (guint)(content_duration/GST_SECOND);
						guint avg_byterate = (dur_sec>0)?((guint)(content_size/dur_sec)):(0);

						if (avg_byterate > 0)
							buffered_sec = (gint)(buffered_total/avg_byterate);
						else if (player->total_maximum_bitrate > 0)
							buffered_sec = (gint)(GET_BIT_FROM_BYTE(buffered_total)/(gint64)player->total_maximum_bitrate);
						else if (player->total_bitrate > 0)
							buffered_sec = (gint)(GET_BIT_FROM_BYTE(buffered_total)/(gint64)player->total_bitrate);

						if ((buffered_sec >= 0) && (dur_sec > 0))
							stop_per = start_per + (100 * buffered_sec / dur_sec);
					}

					LOGD ("[Buffered Total] %lld bytes, %d sec, per %d~%d\n", buffered_total, buffered_sec, start_per, stop_per);
				}
			}

			if (((buffered_total == 0) || (start_per < 0) || (stop_per < 0)) &&
				(player->pipeline->mainbin[MMPLAYER_M_MUXED_S_BUFFER].gst))
			{
				query = gst_query_new_buffering ( GST_FORMAT_PERCENT );
				if ( gst_element_query ( player->pipeline->mainbin[MMPLAYER_M_MUXED_S_BUFFER].gst, query ) )
				{
					GstFormat format;
					gint64 range_start_per = -1, range_stop_per = -1;

					gst_query_parse_buffering_range ( query, &format, &range_start_per, &range_stop_per, NULL );

					LOGD ("[Q2] range start %" G_GINT64_FORMAT " ~ stop %" G_GINT64_FORMAT "\n",  range_start_per , range_stop_per);

					if (range_start_per != -1)
						start_per = (gint)(100 * range_start_per / GST_FORMAT_PERCENT_MAX);

					if (range_stop_per != -1)
						stop_per = (gint)(100 * range_stop_per / GST_FORMAT_PERCENT_MAX);
				}
				gst_query_unref (query);
			}

			if ( start_per > 0)
				*start_pos = (start_per < 100)?(start_per):(100);
			else
				*start_pos = 0;

			if ( stop_per > 0)
				*stop_pos = (stop_per < 100)?(stop_per):(100);
			else
				*stop_pos = 0;

			break;
		}
		case MM_PLAYER_POS_FORMAT_TIME :
			LOGW ( "Time format is not supported yet.\n" );
			break;

		default :
			break;
	}

  	LOGD("current buffer position : %lu~%lu \n", *start_pos, *stop_pos );

	return MM_ERROR_NONE;
}

static int
__gst_set_message_callback(mm_player_t* player, MMMessageCallback callback, gpointer user_param) // @
{
	MMPLAYER_FENTER();

	if ( !player )
	{
		LOGW("set_message_callback is called with invalid player handle\n");
		return MM_ERROR_PLAYER_NOT_INITIALIZED;
	}

	player->msg_cb = callback;
	player->msg_cb_param = user_param;

	LOGD("msg_cb : %p     msg_cb_param : %p\n", callback, user_param);

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

static int __mmfplayer_parse_profile(const char *uri, void *param, MMPlayerParseProfile* data) // @
{
	int ret = MM_ERROR_PLAYER_INVALID_URI;
	char *path = NULL;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( uri , FALSE);
	MMPLAYER_RETURN_VAL_IF_FAIL ( data , FALSE);
	MMPLAYER_RETURN_VAL_IF_FAIL ( ( strlen(uri) <= MM_MAX_URL_LEN ), FALSE );

	memset(data, 0, sizeof(MMPlayerParseProfile));

	if ((path = strstr(uri, "file://")))
	{
		int file_stat = MM_ERROR_NONE;

		file_stat = util_exist_file_path(path + 7);

		if (file_stat == MM_ERROR_NONE)
		{
			strncpy(data->uri, path, MM_MAX_URL_LEN-1);

			if ( util_is_sdp_file ( path ) )
			{
				LOGD("uri is actually a file but it's sdp file. giving it to rtspsrc\n");
				data->uri_type = MM_PLAYER_URI_TYPE_URL_RTSP;
			}
			else
			{
				data->uri_type = MM_PLAYER_URI_TYPE_FILE;
			}
			ret = MM_ERROR_NONE;
		}
		else if (file_stat == MM_ERROR_PLAYER_PERMISSION_DENIED)
		{
			data->uri_type = MM_PLAYER_URI_TYPE_NO_PERMISSION;
		}
		else
		{
			LOGW("could  access %s.\n", path);
		}
	}
	else if ((path = strstr(uri, "es_buff://")))
	{
		if (strlen(path))
		{
			strncpy(data->uri, uri, MM_MAX_URL_LEN-1);
			data->uri_type = MM_PLAYER_URI_TYPE_MS_BUFF;
			ret = MM_ERROR_NONE;
		}
	}
	else if ((path = strstr(uri, "buff://")))
	{
			data->uri_type = MM_PLAYER_URI_TYPE_BUFF;
			ret = MM_ERROR_NONE;
	}
	else if ((path = strstr(uri, "rtsp://")))
	{
		if (strlen(path)) {
			if((path = strstr(uri, "/wfd1.0/"))) {
				strcpy(data->uri, uri);
				data->uri_type = MM_PLAYER_URI_TYPE_URL_WFD;
				ret = MM_ERROR_NONE;
				LOGD("uri is actually a wfd client path. giving it to wfdrtspsrc\n");
			}
			else {
				strcpy(data->uri, uri);
				data->uri_type = MM_PLAYER_URI_TYPE_URL_RTSP;
				ret = MM_ERROR_NONE;
			}
		}
	}
	else if ((path = strstr(uri, "http://")))
	{
		if (strlen(path)) {
			strcpy(data->uri, uri);

			if (g_str_has_suffix (g_ascii_strdown(uri, strlen(uri)), ".ism/manifest") ||
				g_str_has_suffix (g_ascii_strdown(uri, strlen(uri)), ".isml/manifest"))
			{
				data->uri_type = MM_PLAYER_URI_TYPE_SS;
			}
			else
			        data->uri_type = MM_PLAYER_URI_TYPE_URL_HTTP;

			ret = MM_ERROR_NONE;
		}
	}
	else if ((path = strstr(uri, "https://")))
	{
		if (strlen(path)) {
			strcpy(data->uri, uri);

		if (g_str_has_suffix (g_ascii_strdown(uri, strlen(uri)), ".ism/manifest") ||
				g_str_has_suffix (g_ascii_strdown(uri, strlen(uri)), ".isml/manifest"))
			{
				data->uri_type = MM_PLAYER_URI_TYPE_SS;
			}

			data->uri_type = MM_PLAYER_URI_TYPE_URL_HTTP;

			ret = MM_ERROR_NONE;
		}
	}
	else if ((path = strstr(uri, "rtspu://")))
	{
		if (strlen(path)) {
			strcpy(data->uri, uri);
			data->uri_type = MM_PLAYER_URI_TYPE_URL_RTSP;
			ret = MM_ERROR_NONE;
		}
	}
	else if ((path = strstr(uri, "rtspr://")))
	{
		strcpy(data->uri, path);
		char *separater =strstr(path, "*");

		if (separater) {
			int urgent_len = 0;
			char *urgent = separater + strlen("*");

			if ((urgent_len = strlen(urgent))) {
				data->uri[strlen(path) - urgent_len - strlen("*")] = '\0';
				strcpy(data->urgent, urgent);
				data->uri_type = MM_PLAYER_URI_TYPE_URL_RTSP;
				ret = MM_ERROR_NONE;
			}
		}
	}
	else if ((path = strstr(uri, "mms://")))
	{
		if (strlen(path)) {
			strcpy(data->uri, uri);
			data->uri_type = MM_PLAYER_URI_TYPE_URL_MMS;
			ret = MM_ERROR_NONE;
		}
	}
	else if ((path = strstr(uri, "mem://")))
	{
		if (strlen(path)) {
			int mem_size = 0;
			char *buffer = NULL;
			char *seperator = strchr(path, ',');
			char ext[100] = {0,}, size[100] = {0,};

			if (seperator) {
				if ((buffer = strstr(path, "ext="))) {
					buffer += strlen("ext=");

					if (strlen(buffer)) {
						strcpy(ext, buffer);

						if ((seperator = strchr(ext, ','))
							|| (seperator = strchr(ext, ' '))
							|| (seperator = strchr(ext, '\0'))) {
							seperator[0] = '\0';
						}
					}
				}

				if ((buffer = strstr(path, "size="))) {
					buffer += strlen("size=");

					if (strlen(buffer) > 0) {
						strcpy(size, buffer);

						if ((seperator = strchr(size, ','))
							|| (seperator = strchr(size, ' '))
							|| (seperator = strchr(size, '\0'))) {
							seperator[0] = '\0';
						}

						mem_size = atoi(size);
					}
				}
			}

   			LOGD("ext: %s, mem_size: %d, mmap(param): %p\n", ext, mem_size, param);
			if ( mem_size && param)
			{
				data->mem = param;
				data->mem_size = mem_size;
				data->uri_type = MM_PLAYER_URI_TYPE_MEM;
				ret = MM_ERROR_NONE;
			}
		}
	}
	else
	{
		int file_stat = MM_ERROR_NONE;

		file_stat = util_exist_file_path(uri);

		/* if no protocol prefix exist. check file existence and then give file:// as it's prefix */
		if (file_stat == MM_ERROR_NONE)
		{
			g_snprintf(data->uri,  MM_MAX_URL_LEN, "file://%s", uri);

			if ( util_is_sdp_file( (char*)uri ) )
			{
				LOGD("uri is actually a file but it's sdp file. giving it to rtspsrc\n");
				data->uri_type = MM_PLAYER_URI_TYPE_URL_RTSP;
			}
			else
			{
				data->uri_type = MM_PLAYER_URI_TYPE_FILE;
			}
			ret = MM_ERROR_NONE;
		}
		else if (file_stat == MM_ERROR_PLAYER_PERMISSION_DENIED)
		{
			data->uri_type = MM_PLAYER_URI_TYPE_NO_PERMISSION;
		}
		else
		{
			LOGE ("invalid uri, could not play..\n");
			data->uri_type = MM_PLAYER_URI_TYPE_NONE;
		}
	}

	if (data->uri_type == MM_PLAYER_URI_TYPE_NONE) {
		ret = MM_ERROR_PLAYER_FILE_NOT_FOUND;
	} else if (data->uri_type == MM_PLAYER_URI_TYPE_NO_PERMISSION){
		ret = MM_ERROR_PLAYER_PERMISSION_DENIED;
	}

	/* dump parse result */
	SECURE_LOGW("incomming uri : %s\n", uri);
	LOGD("uri_type : %d, mem : %p, mem_size : %d, urgent : %s\n",
		data->uri_type, data->mem, data->mem_size, data->urgent);

	MMPLAYER_FLEAVE();

	return ret;
}

gboolean _asm_postmsg(gpointer *data)
{
	mm_player_t* player = (mm_player_t*)data;
	MMMessageParamType msg = {0, };

	MMPLAYER_FENTER();
	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );
	LOGW("get notified");

	if ((player->cmd == MMPLAYER_COMMAND_DESTROY) ||
		(player->cmd == MMPLAYER_COMMAND_UNREALIZE))
	{
		LOGW("dispatched");
		return FALSE;
	}


	msg.union_type = MM_MSG_UNION_CODE;
	msg.code = player->sound_focus.focus_changed_msg;

#if 0 // should remove
	if (player->sm.event_src == ASM_EVENT_SOURCE_RESUMABLE_CANCELED)
	{
		/* fill the message with state of player */
		msg.state.current = MMPLAYER_CURRENT_STATE(player);
		MMPLAYER_POST_MSG( player, MM_MESSAGE_STATE_INTERRUPTED, &msg);
		player->resumable_cancel_id = 0;
	}
	else
#endif
	{
		MMPLAYER_POST_MSG( player, MM_MESSAGE_READY_TO_RESUME, &msg);
		player->resume_event_id = 0;
	}

	LOGW("dispatched");
	return FALSE;
}

gboolean _asm_lazy_pause(gpointer *data)
{
	mm_player_t* player = (mm_player_t*)data;
	int ret = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );

	if (MMPLAYER_CURRENT_STATE(player) == MM_PLAYER_STATE_PLAYING)
	{
		LOGD ("Ready to proceed lazy pause\n");
		ret = _mmplayer_pause((MMHandleType)player);
		if(MM_ERROR_NONE != ret)
		{
			LOGE("MMPlayer pause failed in ASM callback lazy pause\n");
		}
	}
	else
	{
		LOGD ("Invalid state to proceed lazy pause\n");
	}

	/* unset mute */
	if (player->pipeline && player->pipeline->audiobin)
		g_object_set(G_OBJECT(player->pipeline->audiobin[MMPLAYER_A_SINK].gst), "mute", 0, NULL);

	player->sound_focus.by_asm_cb = FALSE; //should be reset here

	MMPLAYER_FLEAVE();

	return FALSE;
}

gboolean
__mmplayer_can_do_interrupt(mm_player_t *player)
{
	if (!player || !player->pipeline || !player->attrs)
	{
		LOGW("not initialized");
		goto FAILED;
	}

	if ((player->sound_focus.exit_cb) || (player->set_mode.pcm_extraction))
	{
		LOGW("leave from asm cb right now, %d, %d", player->sound_focus.exit_cb, player->set_mode.pcm_extraction);
		goto FAILED;
	}

	/* check if seeking */
	if (player->doing_seek)
	{
		MMMessageParamType msg_param;
		memset (&msg_param, 0, sizeof(MMMessageParamType));
		msg_param.code = MM_ERROR_PLAYER_SEEK;
		player->doing_seek = FALSE;
		MMPLAYER_POST_MSG(player, MM_MESSAGE_ERROR, &msg_param);
		goto FAILED;
	}

	/* check other thread */
	if (!g_mutex_trylock(&player->cmd_lock))
	{
		LOGW("locked already, cmd state : %d", player->cmd);

		/* check application command */
		if (player->cmd == MMPLAYER_COMMAND_START || player->cmd == MMPLAYER_COMMAND_RESUME)
		{
			LOGW("playing.. should wait cmd lock then, will be interrupted");
			g_mutex_lock(&player->cmd_lock);
			goto INTERRUPT;
		}
		LOGW("nothing to do");
		goto FAILED;
	}
	else
	{
		LOGW("can interrupt immediately");
		goto INTERRUPT;
	}

FAILED:    /* with CMD UNLOCKED */
	return FALSE;

INTERRUPT: /* with CMD LOCKED */
	return TRUE;
}

/* if you want to enable USE_ASM, please check the history get the ASM cb code. */
static int
__mmplayer_convert_sound_focus_state(gboolean acquire, const char *reason_for_change, MMPlayerFocusChangedMsg *msg)
{
	int ret = MM_ERROR_NONE;
	MMPlayerFocusChangedMsg focus_msg = MM_PLAYER_FOCUS_CHANGED_BY_UNKNOWN;

	if (strstr(reason_for_change, "alarm")) {
		focus_msg = MM_PLAYER_FOCUS_CHANGED_BY_ALARM;

	} else if (strstr(reason_for_change, "notification")) {
		focus_msg = MM_PLAYER_FOCUS_CHANGED_BY_NOTIFICATION;

	} else if (strstr(reason_for_change, "emergency")) {
		focus_msg = MM_PLAYER_FOCUS_CHANGED_BY_EMERGENCY;

	} else if (strstr(reason_for_change, "call-voice") ||
               strstr(reason_for_change, "call-video") ||
               strstr(reason_for_change, "voip") ||
               strstr(reason_for_change, "ringtone-voip") ||
               strstr(reason_for_change, "ringtone-call")) {
		focus_msg = MM_PLAYER_FOCUS_CHANGED_BY_CALL;

	} else if (strstr(reason_for_change, "media") ||
				strstr(reason_for_change, "radio") ||
				strstr(reason_for_change, "loopback") ||
				strstr(reason_for_change, "system") ||
				strstr(reason_for_change, "voice-information") ||
				strstr(reason_for_change, "voice-recognition")) {
		focus_msg = MM_PLAYER_FOCUS_CHANGED_BY_MEDIA;

	} else {
		ret = MM_ERROR_INVALID_ARGUMENT;
		LOGW("not supported reason(%s), err(0x%08x)", reason_for_change, ret);
		goto DONE;
	}

	if (acquire && (focus_msg != MM_PLAYER_FOCUS_CHANGED_BY_MEDIA))
	{
		/* can acqurie */
		focus_msg = MM_PLAYER_FOCUS_CHANGED_COMPLETED;
	}

	LOGD("converted from reason(%s) to msg(%d)", reason_for_change, focus_msg);
	*msg = focus_msg;

DONE:
	return ret;
}

/* FIXME: will be updated with new funct */
void __mmplayer_sound_focus_watch_callback(int id, mm_sound_focus_type_e focus_type, mm_sound_focus_state_e focus_state,
				       const char *reason_for_change, const char *additional_info, void *user_data)
{
	mm_player_t* player = (mm_player_t*) user_data;
	int result = MM_ERROR_NONE;
	MMPlayerFocusChangedMsg msg = MM_PLAYER_FOCUS_CHANGED_BY_UNKNOWN;

	LOGW("focus watch notified");

	if (!__mmplayer_can_do_interrupt(player))
	{
		LOGW("no need to interrupt, so leave");
		goto EXIT_WITHOUT_UNLOCK;
	}

	if (player->sound_focus.session_flags & MM_SESSION_OPTION_UNINTERRUPTIBLE)
	{
		LOGW("flags is UNINTERRUPTIBLE. do nothing.");
		goto EXIT;
	}

	LOGW("watch: state: %d, focus_type : %d, reason_for_change : %s",
		focus_state, focus_type, (reason_for_change?reason_for_change:"N/A"));

	player->sound_focus.cb_pending = TRUE;
	player->sound_focus.by_asm_cb = TRUE;

	if (focus_state == FOCUS_IS_ACQUIRED)
	{
		LOGW("watch: FOCUS_IS_ACQUIRED");
		if (MM_ERROR_NONE == __mmplayer_convert_sound_focus_state(FALSE, reason_for_change, &msg))
		{
			player->sound_focus.focus_changed_msg = (int)msg;
		}

		if (strstr(reason_for_change, "call") ||
			strstr(reason_for_change, "voip") ||	/* FIXME: to check */
			strstr(reason_for_change, "alarm") ||
			strstr(reason_for_change, "media"))
		{
			if (!MMPLAYER_IS_RTSP_STREAMING(player))
			{
				// hold 0.7 second to excute "fadedown mute" effect
				LOGW ("do fade down->pause->undo fade down");

				__mmplayer_do_sound_fadedown(player, MM_PLAYER_FADEOUT_TIME_DEFAULT);

				result = _mmplayer_pause((MMHandleType)player);
				if (result != MM_ERROR_NONE)
				{
					LOGW("fail to set Pause state by asm");
					goto EXIT;
				}
				__mmplayer_undo_sound_fadedown(player);
			}
			else
			{
				/* rtsp should connect again in specific network becasue tcp session can't be kept any more */
				_mmplayer_unrealize((MMHandleType)player);
			}
		}
		else
		{
			LOGW ("pause immediately");
			result = _mmplayer_pause((MMHandleType)player);
			if (result != MM_ERROR_NONE)
			{
				LOGW("fail to set Pause state by asm");
				goto EXIT;
			}
		}
	}
	else if (focus_state == FOCUS_IS_RELEASED)
	{
		LOGW("FOCUS_IS_RELEASED: Got msg from asm to resume");
		player->sound_focus.antishock = TRUE;
		player->sound_focus.by_asm_cb = FALSE;

		if (MM_ERROR_NONE == __mmplayer_convert_sound_focus_state(TRUE, reason_for_change, &msg))
		{
			player->sound_focus.focus_changed_msg = (int)msg;
		}

		//ASM server is single thread daemon. So use g_idle_add() to post resume msg
		player->resume_event_id = g_idle_add((GSourceFunc)_asm_postmsg, (gpointer)player);
		goto DONE;
	}
	else
	{
		LOGW("unknown focus state %d", focus_state);
	}

DONE:
	player->sound_focus.by_asm_cb = FALSE;
	player->sound_focus.cb_pending = FALSE;

EXIT:
	MMPLAYER_CMD_UNLOCK( player );
	LOGW("dispatched");
	return;

EXIT_WITHOUT_UNLOCK:
	LOGW("dispatched");
	return;
}

void
__mmplayer_sound_focus_callback(int id, mm_sound_focus_type_e focus_type, mm_sound_focus_state_e focus_state,
	const char *reason_for_change, const char *additional_info, void *user_data)
{
	mm_player_t* player = (mm_player_t*) user_data;
	int result = MM_ERROR_NONE;
	gboolean lazy_pause = FALSE;
	MMPlayerFocusChangedMsg msg = MM_PLAYER_FOCUS_CHANGED_BY_UNKNOWN;

	LOGW("get focus notified");

	if (!__mmplayer_can_do_interrupt(player))
	{
		LOGW("no need to interrupt, so leave");
		goto EXIT_WITHOUT_UNLOCK;
	}

	if (player->sound_focus.session_flags & MM_SESSION_OPTION_UNINTERRUPTIBLE)
	{
		LOGW("flags is UNINTERRUPTIBLE. do nothing.");
		goto EXIT;
	}

	LOGW("state: %d, focus_type : %d, reason_for_change : %s",
		focus_state, focus_type, (reason_for_change?reason_for_change:"N/A"));

	player->sound_focus.cb_pending = TRUE;
	player->sound_focus.by_asm_cb = TRUE;
//	player->sound_focus.event_src = event_src;

#if 0
	/* first, check event source */
	if(event_src == ASM_EVENT_SOURCE_EARJACK_UNPLUG)
	{
		int stop_by_asm = 0;
		mm_attrs_get_int_by_name(player->attrs, "sound_stop_when_unplugged", &stop_by_asm);
		if (!stop_by_asm)
			goto DONE;
	}
	else if (event_src == ASM_EVENT_SOURCE_RESOURCE_CONFLICT)
	{
		/* can use video overlay simultaneously */
		/* video resource conflict */
		if(player->pipeline->videobin)
		{
			LOGD("video conflict but, can support multiple video");
			result = _mmplayer_pause((MMHandleType)player);
			cb_res = ASM_CB_RES_PAUSE;
		}
		else if (player->pipeline->audiobin)
		{
			LOGD("audio resource conflict");
			result = _mmplayer_pause((MMHandleType)player);
			if (result != MM_ERROR_NONE)
			{
				LOGW("fail to set pause by asm");
			}
			cb_res = ASM_CB_RES_PAUSE;
		}
		goto DONE;
	}
#if 0 // should remove
	else if (event_src == ASM_EVENT_SOURCE_RESUMABLE_CANCELED)
	{
		LOGW("Got msg from asm for resumable canceled.\n");
		player->sound_focus.antishock = TRUE;
		player->sound_focus.by_asm_cb = FALSE;

		player->resumable_cancel_id = g_idle_add((GSourceFunc)_asm_postmsg, (gpointer)player);
		cb_res = ASM_CB_RES_IGNORE;
		goto DONE;
	}
#endif
#endif

	if (focus_state == FOCUS_IS_RELEASED)
	{
		LOGW("FOCUS_IS_RELEASED");

		if (MM_ERROR_NONE == __mmplayer_convert_sound_focus_state(FALSE, reason_for_change, &msg))
		{
			player->sound_focus.focus_changed_msg = (int)msg;
		}

		if (strstr(reason_for_change, "call") ||
			strstr(reason_for_change, "voip") ||	/* FIXME: to check */
			strstr(reason_for_change, "alarm") ||
			strstr(reason_for_change, "media"))
		{
			if (!MMPLAYER_IS_RTSP_STREAMING(player))
			{
				//hold 0.7 second to excute "fadedown mute" effect
				LOGW ("do fade down->pause->undo fade down");

				__mmplayer_do_sound_fadedown(player, MM_PLAYER_FADEOUT_TIME_DEFAULT);

				result = _mmplayer_pause((MMHandleType)player);
				if (result != MM_ERROR_NONE)
				{
					LOGW("fail to set Pause state by asm");
					goto EXIT;
				}
				__mmplayer_undo_sound_fadedown(player);
			}
			else
			{
				/* rtsp should connect again in specific network becasue tcp session can't be kept any more */
				_mmplayer_unrealize((MMHandleType)player);
			}
		}
		else
		{
			LOGW ("pause immediately");
			result = _mmplayer_pause((MMHandleType)player);
			if (result != MM_ERROR_NONE)
			{
				LOGW("fail to set Pause state by asm");
				goto EXIT;
			}
		}
	}
	else if (focus_state == FOCUS_IS_ACQUIRED)
	{
		LOGW("FOCUS_IS_ACQUIRED: Got msg from asm to resume");
		player->sound_focus.antishock = TRUE;
		player->sound_focus.by_asm_cb = FALSE;

		if (MM_ERROR_NONE == __mmplayer_convert_sound_focus_state(TRUE, reason_for_change, &msg))
		{
			player->sound_focus.focus_changed_msg = (int)msg;
		}

		//ASM server is single thread daemon. So use g_idle_add() to post resume msg
		player->resume_event_id = g_idle_add((GSourceFunc)_asm_postmsg, (gpointer)player);
		goto DONE;
	}
	else
	{
		LOGW("unknown focus state %d", focus_state);
	}

DONE:
	if ( !lazy_pause )
	{
		player->sound_focus.by_asm_cb = FALSE;
	}
	player->sound_focus.cb_pending = FALSE;

EXIT:
	MMPLAYER_CMD_UNLOCK( player );
	LOGW("dispatched");
	return;

EXIT_WITHOUT_UNLOCK:
	LOGW("dispatched");
	return;
}


int
_mmplayer_create_player(MMHandleType handle) // @
{
	mm_player_t* player = MM_PLAYER_CAST(handle);

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* initialize player state */
	MMPLAYER_CURRENT_STATE(player) = MM_PLAYER_STATE_NONE;
	MMPLAYER_PREV_STATE(player) = MM_PLAYER_STATE_NONE;
	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_NONE;
	MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_NONE;

	/* check current state */
	MMPLAYER_CHECK_STATE ( player, MMPLAYER_COMMAND_CREATE );

	/* construct attributes */
	player->attrs = _mmplayer_construct_attribute(handle);

	if ( !player->attrs )
	{
		LOGE("Failed to construct attributes\n");
		goto ERROR;
	}

	/* initialize gstreamer with configured parameter */
	if ( ! __mmplayer_init_gstreamer(player) )
	{
		LOGE("Initializing gstreamer failed\n");
		goto ERROR;
	}

	/* initialize factories if not using decodebin */
	if( player->factories == NULL )
		__mmplayer_init_factories(player);

	/* create lock. note that g_tread_init() has already called in gst_init() */
	g_mutex_init(&player->fsink_lock);

	/* create repeat mutex */
	g_mutex_init(&player->repeat_thread_mutex);

	/* create repeat cond */
	g_cond_init(&player->repeat_thread_cond);

	/* create repeat thread */
	player->repeat_thread =
		g_thread_try_new ("repeat_thread", __mmplayer_repeat_thread, (gpointer)player, NULL);


	/* create next play mutex */
	g_mutex_init(&player->next_play_thread_mutex);

	/* create next play cond */
	g_cond_init(&player->next_play_thread_cond);

	/* create next play thread */
	player->next_play_thread =
		g_thread_try_new ("next_play_thread", __mmplayer_next_play_thread, (gpointer)player, NULL);
	if ( ! player->next_play_thread )
	{
		LOGE("failed to create next play thread");
		goto ERROR;
	}

	if ( MM_ERROR_NONE != _mmplayer_initialize_video_capture(player))
	{
		LOGE("failed to initialize video capture\n");
		goto ERROR;
	}

	/* initialize resource manager */
	if ( MM_ERROR_NONE != _mmplayer_resource_manager_init(&player->resource_manager, player))
	{
		LOGE("failed to initialize resource manager\n");
		goto ERROR;
	}

#if 0 //need to change and test
	/* to add active device callback */
	if ( MM_ERROR_NONE != mm_sound_add_device_information_changed_callback(MM_SOUND_DEVICE_STATE_ACTIVATED_FLAG, __mmplayer_sound_device_info_changed_cb_func, (void*)player))
	{
		LOGE("failed mm_sound_add_device_information_changed_callback \n");
	}
#endif
	if (MMPLAYER_IS_HTTP_PD(player))
	{
		player->pd_downloader = NULL;
		player->pd_file_save_path = NULL;
	}

	player->streaming_type = STREAMING_SERVICE_NONE;

	/* give default value of audio effect setting */
	player->sound.volume = MM_VOLUME_FACTOR_DEFAULT;
	player->playback_rate = DEFAULT_PLAYBACK_RATE;

	player->play_subtitle = FALSE;
	player->use_textoverlay = FALSE;
	player->play_count = 0;
	player->use_decodebin = TRUE;
	player->ignore_asyncdone = FALSE;
	player->use_deinterleave = FALSE;
	player->max_audio_channels = 0;
	player->video_share_api_delta = 0;
	player->video_share_clock_delta = 0;
	player->has_closed_caption = FALSE;

	__mmplayer_post_proc_reset(player);

	if (player->ini.dump_element_keyword[0][0] == '\0')
	{
		player->ini.set_dump_element_flag= FALSE;
	}
	else
	{
		player->ini.set_dump_element_flag = TRUE;
	}

	/* set player state to null */
	MMPLAYER_STATE_CHANGE_TIMEOUT(player) = player->ini.localplayback_state_change_timeout;
	MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_NULL );

	return MM_ERROR_NONE;

ERROR:
	/* free lock */
	g_mutex_clear(&player->fsink_lock );

	/* free thread */
	if ( player->repeat_thread )
	{
		player->repeat_thread_exit = TRUE;
		g_cond_signal( &player->repeat_thread_cond );

		g_thread_join( player->repeat_thread );
		player->repeat_thread = NULL;

		g_mutex_clear(&player->repeat_thread_mutex );

		g_cond_clear (&player->repeat_thread_cond );
	}
	/* clear repeat thread mutex/cond if still alive
	 * this can happen if only thread creating has failed
	 */
	g_mutex_clear(&player->repeat_thread_mutex );
	g_cond_clear ( &player->repeat_thread_cond );

	/* free next play thread */
	if ( player->next_play_thread )
	{
		player->next_play_thread_exit = TRUE;
		g_cond_signal( &player->next_play_thread_cond );

		g_thread_join( player->next_play_thread );
		player->next_play_thread = NULL;

		g_mutex_clear(&player->next_play_thread_mutex );

		g_cond_clear ( &player->next_play_thread_cond );
	}
	/* clear next play thread mutex/cond if still alive
	 * this can happen if only thread creating has failed
	 */
	g_mutex_clear(&player->next_play_thread_mutex );

	g_cond_clear ( &player->next_play_thread_cond );

	/* release attributes */
	_mmplayer_deconstruct_attribute(handle);

	MMPLAYER_FLEAVE();

	return MM_ERROR_PLAYER_INTERNAL;
}

static gboolean
__mmplayer_init_gstreamer(mm_player_t* player) // @
{
	static gboolean initialized = FALSE;
	static const int max_argc = 50;
  	gint* argc = NULL;
	gchar** argv = NULL;
	gchar** argv2 = NULL;
	GError *err = NULL;
	int i = 0;
	int arg_count = 0;

	if ( initialized )
	{
		LOGD("gstreamer already initialized.\n");
		return TRUE;
	}

	/* alloc */
	argc = malloc( sizeof(int) );
	argv = malloc( sizeof(gchar*) * max_argc );
	argv2 = malloc( sizeof(gchar*) * max_argc );

	if ( !argc || !argv || !argv2)
		goto ERROR;

	memset( argv, 0, sizeof(gchar*) * max_argc );
	memset( argv2, 0, sizeof(gchar*) * max_argc );

	/* add initial */
	*argc = 1;
	argv[0] = g_strdup( "mmplayer" );

	/* add gst_param */
	for ( i = 0; i < 5; i++ ) /* FIXIT : num of param is now fixed to 5. make it dynamic */
	{
		if ( strlen( player->ini.gst_param[i] ) > 0 )
		{
			argv[*argc] = g_strdup( player->ini.gst_param[i] );
			(*argc)++;
		}
	}

	/* we would not do fork for scanning plugins */
	argv[*argc] = g_strdup("--gst-disable-registry-fork");
	(*argc)++;

	/* check disable registry scan */
	if ( player->ini.skip_rescan )
	{
		argv[*argc] = g_strdup("--gst-disable-registry-update");
		(*argc)++;
	}

	/* check disable segtrap */
	if ( player->ini.disable_segtrap )
	{
		argv[*argc] = g_strdup("--gst-disable-segtrap");
		(*argc)++;
	}

	LOGD("initializing gstreamer with following parameter\n");
	LOGD("argc : %d\n", *argc);
	arg_count = *argc;

	for ( i = 0; i < arg_count; i++ )
	{
		argv2[i] = argv[i];
		LOGD("argv[%d] : %s\n", i, argv2[i]);
	}


	/* initializing gstreamer */
	if ( ! gst_init_check (argc, &argv, &err))
	{
		LOGE("Could not initialize GStreamer: %s\n", err ? err->message : "unknown error occurred");
		if (err)
		{
			g_error_free (err);
		}

		goto ERROR;
	}
	/* release */
	for ( i = 0; i < arg_count; i++ )
	{
		//LOGD("release - argv[%d] : %s\n", i, argv2[i]);
		MMPLAYER_FREEIF( argv2[i] );
	}

	MMPLAYER_FREEIF( argv );
	MMPLAYER_FREEIF( argv2 );
	MMPLAYER_FREEIF( argc );

	/* done */
	initialized = TRUE;

	return TRUE;

ERROR:

	/* release */
	for ( i = 0; i < arg_count; i++ )
	{
		LOGD("free[%d] : %s\n", i, argv2[i]);
		MMPLAYER_FREEIF( argv2[i] );
	}

	MMPLAYER_FREEIF( argv );
	MMPLAYER_FREEIF( argv2 );
	MMPLAYER_FREEIF( argc );

	return FALSE;
}

int
__mmplayer_destroy_streaming_ext(mm_player_t* player)
{
	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	if (player->pd_downloader)
	{
		_mmplayer_unrealize_pd_downloader((MMHandleType)player);
		MMPLAYER_FREEIF(player->pd_downloader);
	}

	if (MMPLAYER_IS_HTTP_PD(player))
	{
		_mmplayer_destroy_pd_downloader((MMHandleType)player);
		MMPLAYER_FREEIF(player->pd_file_save_path);
	}

	return MM_ERROR_NONE;
}

int
_mmplayer_destroy(MMHandleType handle) // @
{
	mm_player_t* player = MM_PLAYER_CAST(handle);

	MMPLAYER_FENTER();

	/* check player handle */
	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* destroy can called at anytime */
	MMPLAYER_CHECK_STATE ( player, MMPLAYER_COMMAND_DESTROY );

	__mmplayer_destroy_streaming_ext(player);

	/* release repeat thread */
	if ( player->repeat_thread )
	{
		player->repeat_thread_exit = TRUE;
		g_cond_signal( &player->repeat_thread_cond );

		LOGD("waitting for repeat thread exit\n");
		g_thread_join ( player->repeat_thread );
		g_mutex_clear(&player->repeat_thread_mutex );
		g_cond_clear (&player->repeat_thread_cond );
		LOGD("repeat thread released\n");
	}

	/* release next play thread */
	if ( player->next_play_thread )
	{
		player->next_play_thread_exit = TRUE;
		g_cond_signal( &player->next_play_thread_cond );

		LOGD("waitting for next play thread exit\n");
		g_thread_join ( player->next_play_thread );
		g_mutex_clear(&player->next_play_thread_mutex );
		g_cond_clear(&player->next_play_thread_cond );
		LOGD("next play thread released\n");
	}

	_mmplayer_release_video_capture(player);

	/* flush any pending asm_cb */
	if (player->sound_focus.cb_pending)
	{
		/* set a flag for make sure asm_cb to be returned immediately */
		LOGW("asm cb has pending state");
		player->sound_focus.exit_cb = TRUE;

		/* make sure to release any pending asm_cb which locked by cmd_lock */
		MMPLAYER_CMD_UNLOCK(player);
		sched_yield();
		MMPLAYER_CMD_LOCK(player);
	}

	/* withdraw asm */
	if ( MM_ERROR_NONE != _mmplayer_sound_unregister(&player->sound_focus) )
	{
		LOGE("failed to deregister asm server\n");
	}

	/* de-initialize resource manager */
	if ( MM_ERROR_NONE != _mmplayer_resource_manager_deinit(&player->resource_manager))
	{
		LOGE("failed to deinitialize resource manager\n");
	}

#ifdef USE_LAZY_PAUSE
	if (player->lazy_pause_event_id)
	{
		__mmplayer_remove_g_source_from_context(player->context.global_default, player->lazy_pause_event_id);
		player->lazy_pause_event_id = 0;
	}
#endif

	if (player->resume_event_id)
	{
		g_source_remove (player->resume_event_id);
		player->resume_event_id = 0;
	}

	if (player->resumable_cancel_id)
	{
		g_source_remove (player->resumable_cancel_id);
		player->resumable_cancel_id = 0;
	}

	/* release pipeline */
	if ( MM_ERROR_NONE != __mmplayer_gst_destroy_pipeline( player ) )
	{
		LOGE("failed to destory pipeline\n");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	if (player->is_external_subtitle_present && player->subtitle_language_list)
	{
	  g_list_free (player->subtitle_language_list);
	  player->subtitle_language_list = NULL;
	}

	__mmplayer_release_dump_list (player->dump_list);

	/* release miscellaneous information.
	   these info needs to be released after pipeline is destroyed. */
	__mmplayer_release_misc_post( player );

	/* release attributes */
	_mmplayer_deconstruct_attribute( handle );

	/* release factories */
	__mmplayer_release_factories( player );

	/* release lock */
	g_mutex_clear(&player->fsink_lock );

	g_mutex_clear(&player->msg_cb_lock );

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

int
__mmplayer_realize_streaming_ext(mm_player_t* player)
{
	int ret = MM_ERROR_NONE;

	MMPLAYER_FENTER();
	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	if (MMPLAYER_IS_HTTP_PD(player))
	{
		gboolean bret = FALSE;

		player->pd_downloader = _mmplayer_create_pd_downloader();
		if ( !player->pd_downloader )
		{
			LOGE ("Unable to create PD Downloader...");
			ret = MM_ERROR_PLAYER_NO_FREE_SPACE;
		}

		bret = _mmplayer_realize_pd_downloader((MMHandleType)player, player->profile.uri, player->pd_file_save_path, player->pipeline->mainbin[MMPLAYER_M_SRC].gst);

		if (FALSE == bret)
		{
			LOGE ("Unable to create PD Downloader...");
			ret = MM_ERROR_PLAYER_NOT_INITIALIZED;
		}
	}

	MMPLAYER_FLEAVE();
	return ret;
}

int
_mmplayer_sound_register_with_pid(MMHandleType hplayer, int pid) // @
{
	mm_player_t* player =  (mm_player_t*)hplayer;
	MMHandleType attrs = 0;
	int ret = MM_ERROR_NONE;

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		LOGE("fail to get attributes.\n");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	player->sound_focus.pid = pid;

	/* register to asm */
	if ( MM_ERROR_NONE != _mmplayer_sound_register(&player->sound_focus,
						(mm_sound_focus_changed_cb)__mmplayer_sound_focus_callback,
						(mm_sound_focus_changed_watch_cb)__mmplayer_sound_focus_watch_callback,
						(void*)player) )

	{
		/* NOTE : we are dealing it as an error since we cannot expect it's behavior */
		LOGE("failed to register asm server\n");
		return MM_ERROR_POLICY_INTERNAL;
	}
	return ret;
}

int
_mmplayer_realize(MMHandleType hplayer) // @
{
	mm_player_t* player =  (mm_player_t*)hplayer;
	char *uri =NULL;
	void *param = NULL;
	gboolean update_registry = FALSE;
	MMHandleType attrs = 0;
	int ret = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	/* check player handle */
	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED )

	/* check current state */
	MMPLAYER_CHECK_STATE( player, MMPLAYER_COMMAND_REALIZE );

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		LOGE("fail to get attributes.\n");
		return MM_ERROR_PLAYER_INTERNAL;
	}
	mm_attrs_get_string_by_name(attrs, "profile_uri", &uri);
	mm_attrs_get_data_by_name(attrs, "profile_user_param", &param);

	if (player->profile.uri_type == MM_PLAYER_URI_TYPE_NONE)
	{
		ret = __mmfplayer_parse_profile((const char*)uri, param, &player->profile);

		if (ret != MM_ERROR_NONE)
		{
			LOGE("failed to parse profile\n");
			return ret;
		}
	}

	/* FIXIT : we can use thouse in player->profile directly */
	if (player->profile.uri_type == MM_PLAYER_URI_TYPE_MEM)
	{
		player->mem_buf.buf = (char *)player->profile.mem;
		player->mem_buf.len = player->profile.mem_size;
		player->mem_buf.offset = 0;
	}

	if (uri && (strstr(uri, "es_buff://")))
	{
		if (strstr(uri, "es_buff://push_mode"))
		{
			player->es_player_push_mode = TRUE;
		}
		else
		{
			player->es_player_push_mode = FALSE;
		}
	}

	if (player->profile.uri_type == MM_PLAYER_URI_TYPE_URL_MMS)
	{
		LOGW("mms protocol is not supported format.\n");
		return MM_ERROR_PLAYER_NOT_SUPPORTED_FORMAT;
	}

	if (MMPLAYER_IS_STREAMING(player))
		MMPLAYER_STATE_CHANGE_TIMEOUT(player) = player->ini.live_state_change_timeout;
	else
		MMPLAYER_STATE_CHANGE_TIMEOUT(player) = player->ini.localplayback_state_change_timeout;

	player->smooth_streaming = FALSE;
	player->videodec_linked  = 0;
	player->videosink_linked = 0;
	player->audiodec_linked  = 0;
	player->audiosink_linked = 0;
	player->textsink_linked = 0;
	player->is_external_subtitle_present = FALSE;
	/* set the subtitle ON default */
	player->is_subtitle_off = FALSE;

	/* registry should be updated for downloadable codec */
	mm_attrs_get_int_by_name(attrs, "profile_update_registry", &update_registry);

	if ( update_registry )
	{
		LOGD("updating registry...\n");
		gst_update_registry();

		/* then we have to rebuild factories */
		__mmplayer_release_factories( player );
		__mmplayer_init_factories(player);
	}

	/* realize pipeline */
	ret = __gst_realize( player );
	if ( ret != MM_ERROR_NONE )
	{
		LOGE("fail to realize the player.\n");
	}
	else
	{
		ret = __mmplayer_realize_streaming_ext(player);
	}

	MMPLAYER_FLEAVE();

	return ret;
}

int
__mmplayer_unrealize_streaming_ext(mm_player_t *player)
{
	MMPLAYER_FENTER();
	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* destroy can called at anytime */
	if (player->pd_downloader && MMPLAYER_IS_HTTP_PD(player))
	{
		_mmplayer_unrealize_pd_downloader ((MMHandleType)player);
		MMPLAYER_FREEIF(player->pd_downloader);
	}

	MMPLAYER_FLEAVE();
	return MM_ERROR_NONE;
}

int
_mmplayer_unrealize(MMHandleType hplayer)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int ret = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED )

	/* check current state */
	MMPLAYER_CHECK_STATE( player, MMPLAYER_COMMAND_UNREALIZE );

	__mmplayer_unrealize_streaming_ext(player);

	/* unrealize pipeline */
	ret = __gst_unrealize( player );

	/* set asm stop if success */
	if (MM_ERROR_NONE == ret)
	{
		ret = _mmplayer_sound_release_focus(&player->sound_focus);
		if ( ret != MM_ERROR_NONE )
		{
			LOGE("failed to release sound focus, ret(0x%x)\n", ret);
		}

		ret = _mmplayer_resource_manager_release(&player->resource_manager);
		if (ret == MM_ERROR_RESOURCE_INVALID_STATE)
		{
			LOGW("it could be in the middle of resource callback or there's no acquired resource\n");
			ret = MM_ERROR_NONE;
		}
		else if (ret != MM_ERROR_NONE)
		{
			LOGE("failed to release resource, ret(0x%x)\n", ret);
		}
		ret = _mmplayer_resource_manager_unprepare(&player->resource_manager);
		if (ret != MM_ERROR_NONE)
		{
			LOGE("failed to unprepare resource, ret(0x%x)\n", ret);
		}
	}
	else
	{
		LOGE("failed and don't change asm state to stop");
	}

	MMPLAYER_FLEAVE();

	return ret;
}

int
_mmplayer_set_message_callback(MMHandleType hplayer, MMMessageCallback callback, gpointer user_param) // @
{
	mm_player_t* player = (mm_player_t*)hplayer;

	MMPLAYER_RETURN_VAL_IF_FAIL(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	return __gst_set_message_callback(player, callback, user_param);
}

int
_mmplayer_get_state(MMHandleType hplayer, int* state) // @
{
	mm_player_t *player = (mm_player_t*)hplayer;

	MMPLAYER_RETURN_VAL_IF_FAIL(state, MM_ERROR_INVALID_ARGUMENT);

	*state = MMPLAYER_CURRENT_STATE(player);

	return MM_ERROR_NONE;
}


int
_mmplayer_set_volume(MMHandleType hplayer, MMPlayerVolumeType volume) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;
	GstElement* vol_element = NULL;
	int i = 0;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	LOGD("volume [L]=%f:[R]=%f\n",
		volume.level[MM_VOLUME_CHANNEL_LEFT], volume.level[MM_VOLUME_CHANNEL_RIGHT]);

	/* invalid factor range or not */
	for ( i = 0; i < MM_VOLUME_CHANNEL_NUM; i++ )
	{
		if (volume.level[i] < MM_VOLUME_FACTOR_MIN || volume.level[i] > MM_VOLUME_FACTOR_MAX) {
			LOGE("Invalid factor! (valid factor:0~1.0)\n");
			return MM_ERROR_INVALID_ARGUMENT;
		}
	}

	/* not support to set other value into each channel */
	if ((volume.level[MM_VOLUME_CHANNEL_LEFT] != volume.level[MM_VOLUME_CHANNEL_RIGHT]))
		return MM_ERROR_INVALID_ARGUMENT;

	/* Save volume to handle. Currently the first array element will be saved. */
	player->sound.volume = volume.level[MM_VOLUME_CHANNEL_LEFT];

	/* check pipeline handle */
	if ( ! player->pipeline || ! player->pipeline->audiobin )
	{
		LOGD("audiobin is not created yet\n");
		LOGD("but, current stored volume will be set when it's created.\n");

		/* NOTE : stored volume will be used in create_audiobin
		 * returning MM_ERROR_NONE here makes application to able to
		 * set volume at anytime.
		 */
		return MM_ERROR_NONE;
	}

	/* setting volume to volume element */
	vol_element = player->pipeline->audiobin[MMPLAYER_A_VOL].gst;

	if ( vol_element )
	{
		LOGD("volume is set [%f]\n", player->sound.volume);
		g_object_set(vol_element, "volume", player->sound.volume, NULL);
	}

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}


int
_mmplayer_get_volume(MMHandleType hplayer, MMPlayerVolumeType* volume)
{
	mm_player_t* player = (mm_player_t*) hplayer;
	int i = 0;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	MMPLAYER_RETURN_VAL_IF_FAIL( volume, MM_ERROR_INVALID_ARGUMENT );

	/* returning stored volume */
	for (i = 0; i < MM_VOLUME_CHANNEL_NUM; i++)
		volume->level[i] = player->sound.volume;

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}



int
_mmplayer_set_mute(MMHandleType hplayer, int mute) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;
	GstElement* vol_element = NULL;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* mute value shoud 0 or 1 */
	if ( mute != 0 && mute != 1 )
	{
		LOGE("bad mute value\n");

		/* FIXIT : definitly, we need _BAD_PARAM error code */
		return MM_ERROR_INVALID_ARGUMENT;
	}

	player->sound.mute = mute;

	/* just hold mute value if pipeline is not ready */
	if ( !player->pipeline || !player->pipeline->audiobin )
	{
		LOGD("pipeline is not ready. holding mute value\n");
		return MM_ERROR_NONE;
	}

	vol_element = player->pipeline->audiobin[MMPLAYER_A_SINK].gst;

	/* NOTE : volume will only created when the bt is enabled */
	if ( vol_element )
	{
		LOGD("mute : %d\n", mute);
		g_object_set(vol_element, "mute", mute, NULL);
	}
	else
	{
		LOGD("volume elemnet is not created. using volume in audiosink\n");
	}

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

int
_mmplayer_get_mute(MMHandleType hplayer, int* pmute) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	MMPLAYER_RETURN_VAL_IF_FAIL ( pmute, MM_ERROR_INVALID_ARGUMENT );

	/* just hold mute value if pipeline is not ready */
	if ( !player->pipeline || !player->pipeline->audiobin )
	{
		LOGD("pipeline is not ready. returning stored value\n");
		*pmute = player->sound.mute;
		return MM_ERROR_NONE;
	}

	*pmute = player->sound.mute;

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

int
_mmplayer_set_videostream_changed_cb(MMHandleType hplayer, mm_player_stream_changed_callback callback, void *user_param)
{
	mm_player_t* player = (mm_player_t*) hplayer;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	player->video_stream_changed_cb = callback;
	player->video_stream_changed_cb_user_param = user_param;
	LOGD("Handle value is %p : %p\n", player, player->video_stream_changed_cb);

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

int
_mmplayer_set_audiostream_changed_cb(MMHandleType hplayer, mm_player_stream_changed_callback callback, void *user_param)
{
	mm_player_t* player = (mm_player_t*) hplayer;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	player->audio_stream_changed_cb = callback;
	player->audio_stream_changed_cb_user_param = user_param;
	LOGD("Handle value is %p : %p\n", player, player->audio_stream_changed_cb);

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

int
_mmplayer_set_audiostream_cb_ex(MMHandleType hplayer, bool sync, mm_player_audio_stream_callback_ex callback, void *user_param) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	player->audio_stream_render_cb_ex = callback;
	player->audio_stream_cb_user_param = user_param;
	player->audio_stream_sink_sync = sync;
	LOGD("Audio Stream cb Handle value is %p : %p audio_stream_sink_sync : %d\n", player, player->audio_stream_render_cb_ex, player->audio_stream_sink_sync);

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

int
_mmplayer_set_videostream_cb(MMHandleType hplayer, mm_player_video_stream_callback callback, void *user_param) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	player->video_stream_cb = callback;
	player->video_stream_cb_user_param = user_param;
	player->use_video_stream = TRUE;
	LOGD("Stream cb Handle value is %p : %p\n", player, player->video_stream_cb);

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

int
_mmplayer_set_audiostream_cb(MMHandleType hplayer, mm_player_audio_stream_callback callback, void *user_param) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	player->audio_stream_cb = callback;
	player->audio_stream_cb_user_param = user_param;
	LOGD("Audio Stream cb Handle value is %p : %p\n", player, player->audio_stream_cb);

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

// set prepare size
int
_mmplayer_set_prepare_buffering_time(MMHandleType hplayer, int second)
{
	mm_player_t* player = (mm_player_t*) hplayer;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	if (MMPLAYER_CURRENT_STATE(player) !=  MM_PLAYER_STATE_NULL)
		return MM_ERROR_PLAYER_INVALID_STATE;

	LOGD("pre buffer size : %d sec\n", second);

	if ( second <= 0 )
	{
		LOGE("bad size value\n");
		return MM_ERROR_INVALID_ARGUMENT;
	}

	if (player->streamer == NULL)
	{
		player->streamer = __mm_player_streaming_create();
		__mm_player_streaming_initialize(player->streamer);
	}

	player->streamer->buffering_req.initial_second = second;

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

// set runtime mode
int
_mmplayer_set_runtime_buffering_mode(MMHandleType hplayer, MMPlayerBufferingMode mode, int second)
{
	mm_player_t* player = (mm_player_t*) hplayer;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL (player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	LOGD("mode %d\n", mode);

	if ((mode < 0) || (mode > MM_PLAYER_BUFFERING_MODE_MAX) ||
		((mode == MM_PLAYER_BUFFERING_MODE_FIXED) && (second <= 0)))
		return MM_ERROR_INVALID_ARGUMENT;

	if (player->streamer == NULL)
	{
		player->streamer = __mm_player_streaming_create();
		__mm_player_streaming_initialize(player->streamer);
	}

	player->streamer->buffering_req.mode = mode;

	if ((second > 0) &&
		((mode == MM_PLAYER_BUFFERING_MODE_FIXED) ||
		 (mode == MM_PLAYER_BUFFERING_MODE_ADAPTIVE)))
		player->streamer->buffering_req.runtime_second = second;

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

int
_mmplayer_set_videoframe_render_error_cb(MMHandleType hplayer, mm_player_video_frame_render_error_callback callback, void *user_param) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	MMPLAYER_RETURN_VAL_IF_FAIL ( callback, MM_ERROR_INVALID_ARGUMENT );

	player->video_frame_render_error_cb = callback;
	player->video_frame_render_error_cb_user_param = user_param;

	LOGD("Video frame render error cb Handle value is %p : %p\n", player, player->video_frame_render_error_cb);

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

int
__mmplayer_start_streaming_ext(mm_player_t *player)
{
	gint ret = MM_ERROR_NONE;

	MMPLAYER_FENTER();
	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	if (MMPLAYER_IS_HTTP_PD(player))
	{
		if ( !player->pd_downloader )
		{
			ret = __mmplayer_realize_streaming_ext(player);

			if ( ret != MM_ERROR_NONE)
			{
				LOGE ("failed to realize streaming ext\n");
				return ret;
			}
		}

		if (player->pd_downloader && player->pd_mode == MM_PLAYER_PD_MODE_URI)
		{
			ret = _mmplayer_start_pd_downloader ((MMHandleType)player);
			if ( !ret )
			{
				LOGE ("ERROR while starting PD...\n");
				return MM_ERROR_PLAYER_NOT_INITIALIZED;
			}
			ret = MM_ERROR_NONE;
		}
	}

	MMPLAYER_FLEAVE();
	return ret;
}

int
_mmplayer_start(MMHandleType hplayer) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;
	gint ret = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check current state */
	MMPLAYER_CHECK_STATE( player, MMPLAYER_COMMAND_START );

	ret = _mmplayer_sound_acquire_focus(&player->sound_focus);
	if ( ret != MM_ERROR_NONE )
	{
		LOGE("failed to acquire sound focus.\n");
		return ret;
	}

	/* NOTE : we should check and create pipeline again if not created as we destroy
	 * whole pipeline when stopping in streamming playback
	 */
	if ( ! player->pipeline )
	{
		ret = __gst_realize( player );
		if ( MM_ERROR_NONE != ret )
		{
			LOGE("failed to realize before starting. only in streamming\n");
			/* unlock */
			return ret;
		}
	}

	ret = __mmplayer_start_streaming_ext(player);
	if ( ret != MM_ERROR_NONE )
	{
		LOGE("failed to start streaming ext \n");
	}

	/* start pipeline */
	ret = __gst_start( player );
	if ( ret != MM_ERROR_NONE )
	{
		LOGE("failed to start player.\n");
	}

	MMPLAYER_FLEAVE();

	return ret;
}

/* NOTE: post "not supported codec message" to application
 * when one codec is not found during AUTOPLUGGING in MSL.
 * So, it's separated with error of __mmplayer_gst_callback().
 * And, if any codec is not found, don't send message here.
 * Because GST_ERROR_MESSAGE is posted by other plugin internally.
 */
int
__mmplayer_handle_missed_plugin(mm_player_t* player)
{
	MMMessageParamType msg_param;
	memset (&msg_param, 0, sizeof(MMMessageParamType));
	gboolean post_msg_direct = FALSE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	LOGD("not_supported_codec = 0x%02x, can_support_codec = 0x%02x\n",
			player->not_supported_codec, player->can_support_codec);

	if( player->not_found_demuxer )
	{
		msg_param.code = MM_ERROR_PLAYER_CODEC_NOT_FOUND;
		msg_param.data = g_strdup_printf("%s", player->unlinked_demuxer_mime);

		MMPLAYER_POST_MSG( player, MM_MESSAGE_ERROR, &msg_param );
		MMPLAYER_FREEIF(msg_param.data);

		return MM_ERROR_NONE;
	}

	if (player->not_supported_codec)
	{
		if ( player->can_support_codec ) // There is one codec to play
		{
			post_msg_direct = TRUE;
		}
		else
		{
			if ( player->pipeline->audiobin ) // Some content has only PCM data in container.
				post_msg_direct = TRUE;
		}

		if ( post_msg_direct )
		{
			MMMessageParamType msg_param;
			memset (&msg_param, 0, sizeof(MMMessageParamType));

			if ( player->not_supported_codec ==  MISSING_PLUGIN_AUDIO )
			{
				LOGW("not found AUDIO codec, posting error code to application.\n");

				msg_param.code = MM_ERROR_PLAYER_AUDIO_CODEC_NOT_FOUND;
				msg_param.data = g_strdup_printf("%s", player->unlinked_audio_mime);
			}
			else if ( player->not_supported_codec ==  MISSING_PLUGIN_VIDEO )
			{
				LOGW("not found VIDEO codec, posting error code to application.\n");

				msg_param.code = MM_ERROR_PLAYER_VIDEO_CODEC_NOT_FOUND;
				msg_param.data = g_strdup_printf("%s", player->unlinked_video_mime);
			}

			MMPLAYER_POST_MSG( player, MM_MESSAGE_ERROR, &msg_param );

			MMPLAYER_FREEIF(msg_param.data);

			return MM_ERROR_NONE;
		}
		else // no any supported codec case
		{
			LOGW("not found any codec, posting error code to application.\n");

			if ( player->not_supported_codec ==  MISSING_PLUGIN_AUDIO )
			{
				msg_param.code = MM_ERROR_PLAYER_AUDIO_CODEC_NOT_FOUND;
				msg_param.data = g_strdup_printf("%s", player->unlinked_audio_mime);
			}
			else
			{
				msg_param.code = MM_ERROR_PLAYER_CODEC_NOT_FOUND;
				msg_param.data = g_strdup_printf("%s, %s", player->unlinked_video_mime, player->unlinked_audio_mime);
			}

			MMPLAYER_POST_MSG( player, MM_MESSAGE_ERROR, &msg_param );

			MMPLAYER_FREEIF(msg_param.data);
		}
	}

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

static void __mmplayer_check_pipeline(mm_player_t* player)
{
	GstState element_state = GST_STATE_VOID_PENDING;
	GstState element_pending_state = GST_STATE_VOID_PENDING;
	gint timeout = 0;
	int ret = MM_ERROR_NONE;

	if (player->gapless.reconfigure)
	{
		LOGW("pipeline is under construction.\n");

		MMPLAYER_PLAYBACK_LOCK(player);
		MMPLAYER_PLAYBACK_UNLOCK(player);

		timeout = MMPLAYER_STATE_CHANGE_TIMEOUT ( player );

		/* wait for state transition */
		ret = gst_element_get_state( player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, &element_state, &element_pending_state, timeout * GST_SECOND );

		if ( ret == GST_STATE_CHANGE_FAILURE )
		{
			LOGE("failed to change pipeline state within %d sec\n", timeout );
		}
	}
}

/* NOTE : it should be able to call 'stop' anytime*/
int
_mmplayer_stop(MMHandleType hplayer) // @
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int ret = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check current state */
	MMPLAYER_CHECK_STATE( player, MMPLAYER_COMMAND_STOP );

	/* check pipline building state */
	__mmplayer_check_pipeline(player);
	player->gapless.start_time = 0;

	/* NOTE : application should not wait for EOS after calling STOP */
	__mmplayer_cancel_eos_timer( player );

	__mmplayer_unrealize_streaming_ext(player);

	/* reset */
	player->doing_seek = FALSE;

	/* stop pipeline */
	ret = __gst_stop( player );

	if ( ret != MM_ERROR_NONE )
	{
		LOGE("failed to stop player.\n");
	}

	MMPLAYER_FLEAVE();

	return ret;
}

int
_mmplayer_pause(MMHandleType hplayer) // @
{
	mm_player_t* player = (mm_player_t*)hplayer;
	gint64 pos_msec = 0;
	gboolean async = FALSE;
	gint ret = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check current state */
	MMPLAYER_CHECK_STATE( player, MMPLAYER_COMMAND_PAUSE );

	/* check pipline building state */
	__mmplayer_check_pipeline(player);

	switch (MMPLAYER_CURRENT_STATE(player))
	{
		case MM_PLAYER_STATE_READY:
		{
			/* check prepare async or not.
			 * In the case of streaming playback, it's recommned to avoid blocking wait.
			 */
			mm_attrs_get_int_by_name(player->attrs, "profile_prepare_async", &async);
			LOGD("prepare working mode : %s", (async ? "async" : "sync"));
		}
		break;

		case MM_PLAYER_STATE_PLAYING:
		{
			/* NOTE : store current point to overcome some bad operation
			* ( returning zero when getting current position in paused state) of some
			* elements
			*/
			if ( !gst_element_query_position(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_FORMAT_TIME, &pos_msec))
				LOGW("getting current position failed in paused\n");

			player->last_position = pos_msec;
		}
		break;
	}

	/* pause pipeline */
	ret = __gst_pause( player, async );

	if ( ret != MM_ERROR_NONE )
	{
		LOGE("failed to pause player. ret : 0x%x\n", ret);
	}

	MMPLAYER_FLEAVE();

	return ret;
}

int
_mmplayer_resume(MMHandleType hplayer)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int ret = MM_ERROR_NONE;
	gboolean async = FALSE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	ret = _mmplayer_sound_acquire_focus(&player->sound_focus);
	if ( ret != MM_ERROR_NONE )
	{
		LOGE("failed to acquire sound focus.\n");
		return ret;
	}

	/* check current state */
	MMPLAYER_CHECK_STATE( player, MMPLAYER_COMMAND_RESUME );

	ret = __gst_resume( player, async );

	if ( ret != MM_ERROR_NONE )
	{
		LOGE("failed to resume player.\n");
	}

	MMPLAYER_FLEAVE();

	return ret;
}

int
__mmplayer_set_play_count(mm_player_t* player, gint count)
{
	MMHandleType attrs = 0;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	attrs =  MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		LOGE("fail to get attributes.\n");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	mm_attrs_set_int_by_name(attrs, "profile_play_count", count);
	if ( mmf_attrs_commit ( attrs ) ) /* return -1 if error */
		LOGE("failed to commit\n");

	MMPLAYER_FLEAVE();

	return	MM_ERROR_NONE;
}

int
_mmplayer_activate_section_repeat(MMHandleType hplayer, unsigned long start, unsigned long end)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	gint64 start_pos = 0;
	gint64 end_pos = 0;
	gint infinity = -1;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	MMPLAYER_RETURN_VAL_IF_FAIL ( end <= GST_TIME_AS_MSECONDS(player->duration), MM_ERROR_INVALID_ARGUMENT );

	player->section_repeat = TRUE;
	player->section_repeat_start = start;
	player->section_repeat_end = end;

	start_pos = player->section_repeat_start * G_GINT64_CONSTANT(1000000);
	end_pos = player->section_repeat_end * G_GINT64_CONSTANT(1000000);

	__mmplayer_set_play_count( player, infinity );

	if ( (!__gst_seek( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst,
					player->playback_rate,
					GST_FORMAT_TIME,
					( GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE ),
					GST_SEEK_TYPE_SET, start_pos,
					GST_SEEK_TYPE_SET, end_pos)))
	{
		LOGE("failed to activate section repeat\n");

		return MM_ERROR_PLAYER_SEEK;
	}

	LOGD("succeeded to set section repeat from %d to %d\n",
		player->section_repeat_start, player->section_repeat_end);

	MMPLAYER_FLEAVE();

	return	MM_ERROR_NONE;
}

static int
__mmplayer_set_pcm_extraction(mm_player_t* player)
{
	gint64 start_nsec = 0;
	gint64 end_nsec = 0;
	gint64 dur_nsec = 0;
	gint64 dur_msec = 0;
	int required_start = 0;
	int required_end = 0;
	int ret = 0;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL( player, FALSE );

	mm_attrs_multiple_get(player->attrs,
		NULL,
		"pcm_extraction_start_msec", &required_start,
		"pcm_extraction_end_msec", &required_end,
		NULL);

	LOGD("pcm extraction required position is from [%d] to [%d] (msec)\n", required_start, required_end);

	if (required_start == 0 && required_end == 0)
	{
		LOGD("extracting entire stream");
		return MM_ERROR_NONE;
	}
	else if (required_start < 0 || required_start > required_end || required_end < 0 )
	{
		LOGD("invalid range for pcm extraction");
		return MM_ERROR_INVALID_ARGUMENT;
	}

	/* get duration */
	ret = gst_element_query_duration(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_FORMAT_TIME, &dur_nsec);
	if ( !ret )
	{
		LOGE("failed to get duration");
		return MM_ERROR_PLAYER_INTERNAL;
	}
	dur_msec = GST_TIME_AS_MSECONDS(dur_nsec);

	if (dur_msec < required_end) // FIXME
	{
		LOGD("invalid end pos for pcm extraction");
		return MM_ERROR_INVALID_ARGUMENT;
	}

	start_nsec = required_start * G_GINT64_CONSTANT(1000000);
	end_nsec = required_end * G_GINT64_CONSTANT(1000000);

	if ( (!__gst_seek( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst,
					1.0,
					GST_FORMAT_TIME,
					( GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE ),
					GST_SEEK_TYPE_SET, start_nsec,
					GST_SEEK_TYPE_SET, end_nsec)))
	{
		LOGE("failed to seek for pcm extraction\n");

		return MM_ERROR_PLAYER_SEEK;
	}

	LOGD("succeeded to set up segment extraction from [%llu] to [%llu] (nsec)\n", start_nsec, end_nsec);

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

int
_mmplayer_deactivate_section_repeat(MMHandleType hplayer)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	gint64 cur_pos = 0;
	gint onetime = 1;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	player->section_repeat = FALSE;

	__mmplayer_set_play_count( player, onetime );

	gst_element_query_position(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_FORMAT_TIME, &cur_pos);

	if ( (!__gst_seek( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst,
					1.0,
					GST_FORMAT_TIME,
					( GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE ),
					GST_SEEK_TYPE_SET, cur_pos,
					GST_SEEK_TYPE_SET, player->duration )))
	{
		LOGE("failed to deactivate section repeat\n");

		return MM_ERROR_PLAYER_SEEK;
	}

	MMPLAYER_FENTER();

	return MM_ERROR_NONE;
}

int
_mmplayer_set_playspeed(MMHandleType hplayer, float rate, bool streaming)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	gint64 pos_msec = 0;
	int ret = MM_ERROR_NONE;
	int mute = FALSE;
	signed long long start = 0, stop = 0;
	MMPlayerStateType current_state = MM_PLAYER_STATE_NONE;
	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );
	MMPLAYER_RETURN_VAL_IF_FAIL ( streaming || !MMPLAYER_IS_STREAMING(player), MM_ERROR_NOT_SUPPORT_API );

	/* The sound of video is not supported under 0.0 and over 2.0. */
	if(rate >= TRICK_PLAY_MUTE_THRESHOLD_MAX || rate < TRICK_PLAY_MUTE_THRESHOLD_MIN)
	{
		if (player->can_support_codec & FOUND_PLUGIN_VIDEO)
			mute = TRUE;
	}
	_mmplayer_set_mute(hplayer, mute);

	if (player->playback_rate == rate)
		return MM_ERROR_NONE;

	/* If the position is reached at start potion during fast backward, EOS is posted.
	 * So, This EOS have to be classified with it which is posted at reaching the end of stream.
	 * */
	player->playback_rate = rate;

	current_state = MMPLAYER_CURRENT_STATE(player);

	if ( current_state != MM_PLAYER_STATE_PAUSED )
		ret = gst_element_query_position(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_FORMAT_TIME, &pos_msec);

	LOGD ("pos_msec = %"GST_TIME_FORMAT" and ret = %d and state = %d", GST_TIME_ARGS (pos_msec), ret, current_state);

	if ( ( current_state == MM_PLAYER_STATE_PAUSED )
		|| ( ! ret ))
		//|| ( player->last_position != 0 && pos_msec == 0 ) )
	{
		LOGW("returning last point : %lld\n", player->last_position );
		pos_msec = player->last_position;
	}


	if(rate >= 0)
	{
		start = pos_msec;
		stop = GST_CLOCK_TIME_NONE;
	}
	else
	{
		start = GST_CLOCK_TIME_NONE;
		stop = pos_msec;
	}
	if ((!gst_element_seek (player->pipeline->mainbin[MMPLAYER_M_PIPE].gst,
				rate,
				GST_FORMAT_TIME,
				( GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE ),
				GST_SEEK_TYPE_SET, start,
                                GST_SEEK_TYPE_SET, stop)))
	{
    		LOGE("failed to set speed playback\n");
		return MM_ERROR_PLAYER_SEEK;
	}

	LOGD("succeeded to set speed playback as %0.1f\n", rate);

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;;
}

int
_mmplayer_set_position(MMHandleType hplayer, int format, int position) // @
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int ret = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	ret = __gst_set_position ( player, format, (unsigned long)position, FALSE );

	MMPLAYER_FLEAVE();

	return ret;
}

int
_mmplayer_get_position(MMHandleType hplayer, int format, unsigned long *position) // @
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int ret = MM_ERROR_NONE;

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	ret = __gst_get_position ( player, format, position );

	return ret;
}

int
_mmplayer_get_buffer_position(MMHandleType hplayer, int format, unsigned long* start_pos, unsigned long* stop_pos) // @
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int ret = MM_ERROR_NONE;

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	ret = __gst_get_buffer_position ( player, format, start_pos, stop_pos );

	return ret;
}

int
_mmplayer_adjust_subtitle_postion(MMHandleType hplayer, int format, int position) // @
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int ret = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	ret = __gst_adjust_subtitle_position(player, format, position);

	MMPLAYER_FLEAVE();

	return ret;
}
int
_mmplayer_adjust_video_postion(MMHandleType hplayer, int offset) // @
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int ret = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	ret = __gst_adjust_video_position(player, offset);

	MMPLAYER_FLEAVE();

	return ret;
}

static gboolean
__mmplayer_is_midi_type( gchar* str_caps)
{
	if ( ( g_strrstr(str_caps, "audio/midi") ) ||
		( g_strrstr(str_caps, "application/x-gst_ff-mmf") ) ||
		( g_strrstr(str_caps, "application/x-smaf") ) ||
		( g_strrstr(str_caps, "audio/x-imelody") ) ||
		( g_strrstr(str_caps, "audio/mobile-xmf") ) ||
		( g_strrstr(str_caps, "audio/xmf") ) ||
		( g_strrstr(str_caps, "audio/mxmf") ) )
	{
		LOGD("midi\n");

		return TRUE;
	}

	return FALSE;
}

static gboolean
__mmplayer_is_only_mp3_type (gchar *str_caps)
{
	if (g_strrstr(str_caps, "application/x-id3") ||
		(g_strrstr(str_caps, "audio/mpeg") && g_strrstr(str_caps, "mpegversion=(int)1")))
	{
		return TRUE;
	}
	return FALSE;
}

static void
__mmplayer_set_audio_attrs (mm_player_t* player, GstCaps* caps)
{
	GstStructure* caps_structure = NULL;
	gint samplerate = 0;
	gint channels = 0;

	MMPLAYER_FENTER();
	MMPLAYER_RETURN_IF_FAIL (player && caps);

	caps_structure = gst_caps_get_structure(caps, 0);

	/* set stream information */
	gst_structure_get_int (caps_structure, "rate", &samplerate);
	mm_attrs_set_int_by_name (player->attrs, "content_audio_samplerate", samplerate);

	gst_structure_get_int (caps_structure, "channels", &channels);
	mm_attrs_set_int_by_name (player->attrs, "content_audio_channels", channels);

	LOGD ("audio samplerate : %d	channels : %d\n", samplerate, channels);
}

static void
__mmplayer_update_content_type_info(mm_player_t* player)
{
	MMPLAYER_FENTER();
	MMPLAYER_RETURN_IF_FAIL( player && player->type);

	if (__mmplayer_is_midi_type(player->type))
	{
		player->bypass_audio_effect = TRUE;
	}
	else if (g_strrstr(player->type, "application/x-hls"))
	{
		/* If it can't know exact type when it parses uri because of redirection case,
		 * it will be fixed by typefinder or when doing autoplugging.
		 */
		player->profile.uri_type = MM_PLAYER_URI_TYPE_HLS;
		if (player->streamer)
		{
			player->streamer->is_adaptive_streaming = TRUE;
			player->streamer->buffering_req.mode = MM_PLAYER_BUFFERING_MODE_FIXED;
			player->streamer->buffering_req.runtime_second = 5;
		}
	}
	else if (g_strrstr(player->type, "application/dash+xml"))
	{
		player->profile.uri_type = MM_PLAYER_URI_TYPE_DASH;
	}

	MMPLAYER_FLEAVE();
}

static void
__mmplayer_typefind_have_type(  GstElement *tf, guint probability,  // @
GstCaps *caps, gpointer data)
{
	mm_player_t* player = (mm_player_t*)data;
	GstPad* pad = NULL;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL( player && tf && caps );

	/* store type string */
	MMPLAYER_FREEIF(player->type);
	player->type = gst_caps_to_string(caps);
	if (player->type)
		LOGD("meida type %s found, probability %d%% / %d\n", player->type, probability, gst_caps_get_size(caps));

	if ( (!MMPLAYER_IS_WFD_STREAMING( player )) &&
		 (!MMPLAYER_IS_RTSP_STREAMING( player )) &&
		 (g_strrstr(player->type, "audio/x-raw-int")))
	{
		LOGE("not support media format\n");

		if (player->msg_posted == FALSE)
		{
			MMMessageParamType msg_param;
			memset (&msg_param, 0, sizeof(MMMessageParamType));

			msg_param.code = MM_ERROR_PLAYER_NOT_SUPPORTED_FORMAT;
			MMPLAYER_POST_MSG( player, MM_MESSAGE_ERROR, &msg_param );

			/* don't post more if one was sent already */
			player->msg_posted = TRUE;
		}
		return;
	}

	__mmplayer_update_content_type_info(player);

	pad = gst_element_get_static_pad(tf, "src");
	if ( !pad )
	{
		LOGE("fail to get typefind src pad.\n");
		return;
	}

	if (player->use_decodebin)
	{
		if(!__mmplayer_try_to_plug_decodebin( player, pad, caps ))
		{
			gboolean async = FALSE;
			LOGE("failed to autoplug %s\n", player->type);

			mm_attrs_get_int_by_name(player->attrs, "profile_prepare_async", &async);

			if ( async && player->msg_posted == FALSE )
			{
				__mmplayer_handle_missed_plugin( player );
			}

			goto DONE;
		}
	}
	else
	{
		/* try to plug */
		if ( ! __mmplayer_try_to_plug( player, pad, caps ) )
		{
			gboolean async = FALSE;
			LOGE("failed to autoplug %s\n", player->type);

			mm_attrs_get_int_by_name(player->attrs, "profile_prepare_async", &async);

			if ( async && player->msg_posted == FALSE )
			{
				__mmplayer_handle_missed_plugin( player );
			}

			goto DONE;
		}

		/* finish autopluging if no dynamic pad waiting */
		if( ( ! player->have_dynamic_pad) && ( ! player->has_many_types) )
		{
			if ( ! MMPLAYER_IS_RTSP_STREAMING( player ) )
			{
				__mmplayer_pipeline_complete( NULL, (gpointer)player );
			}
		}
	}

DONE:
	gst_object_unref( GST_OBJECT(pad) );

	MMPLAYER_FLEAVE();

	return;
}

#ifdef _MM_PLAYER_ALP_PARSER
void check_name (void *data, void *user_data)
{
	mm_player_t* player = user_data;

	if (g_strrstr((gchar*)data, "mpegaudioparse"))
	{
		LOGD("mpegaudioparse - set alp-mp3dec\n");
		g_object_set(player->pipeline->mainbin[MMPLAYER_M_DEMUX].gst, "alp-mp3dec", TRUE, NULL);
	}
}
#endif

static GstElement *
__mmplayer_create_decodebin (mm_player_t* player)
{
	GstElement *decodebin = NULL;

	MMPLAYER_FENTER();

	/* create decodebin */
	decodebin = gst_element_factory_make("decodebin", NULL);

	if (!decodebin)
	{
		LOGE("fail to create decodebin\n");
		goto ERROR;
	}

	/* raw pad handling signal */
	MMPLAYER_SIGNAL_CONNECT( player, G_OBJECT(decodebin), MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "pad-added",
						G_CALLBACK(__mmplayer_gst_decode_pad_added), player);

	/* no-more-pad pad handling signal */
	MMPLAYER_SIGNAL_CONNECT( player, G_OBJECT(decodebin), MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "no-more-pads",
						G_CALLBACK(__mmplayer_gst_decode_no_more_pads), player);

	MMPLAYER_SIGNAL_CONNECT( player, G_OBJECT(decodebin), MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "pad-removed",
						G_CALLBACK(__mmplayer_gst_decode_pad_removed), player);

	/* This signal is emitted when a pad for which there is no further possible
	   decoding is added to the decodebin.*/
	MMPLAYER_SIGNAL_CONNECT( player, G_OBJECT(decodebin), MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "unknown-type",
						G_CALLBACK(__mmplayer_gst_decode_unknown_type), player );

	/* This signal is emitted whenever decodebin finds a new stream. It is emitted
	   before looking for any elements that can handle that stream.*/
	MMPLAYER_SIGNAL_CONNECT( player, G_OBJECT(decodebin), MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "autoplug-continue",
						G_CALLBACK(__mmplayer_gst_decode_autoplug_continue), player);

	/* This signal is emitted whenever decodebin finds a new stream. It is emitted
	   before looking for any elements that can handle that stream.*/
	MMPLAYER_SIGNAL_CONNECT( player, G_OBJECT(decodebin), MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "autoplug-select",
						G_CALLBACK(__mmplayer_gst_decode_autoplug_select), player);

	/* This signal is emitted once decodebin has finished decoding all the data.*/
	MMPLAYER_SIGNAL_CONNECT( player, G_OBJECT(decodebin), MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "drained",
						G_CALLBACK(__mmplayer_gst_decode_drained), player);

	/* This signal is emitted when a element is added to the bin.*/
	MMPLAYER_SIGNAL_CONNECT( player, G_OBJECT(decodebin), MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "element-added",
						G_CALLBACK(__mmplayer_gst_element_added), player);

ERROR:
	return decodebin;
}

static gboolean
__mmplayer_try_to_plug_decodebin(mm_player_t* player, GstPad *srcpad, const GstCaps *caps)
{
	MMPlayerGstElement* mainbin = NULL;
	GstElement* decodebin = NULL;
	GstElement* queue2 = NULL;
	GstPad* sinkpad = NULL;
	GstPad* qsrcpad= NULL;
	gchar *caps_str = NULL;
	gint64 dur_bytes = 0L;
	gchar* file_buffering_path = NULL;
	gboolean use_file_buffer = FALSE;

	guint max_buffer_size_bytes = 0;
	gdouble init_buffering_time = (gdouble)player->streamer->buffering_req.initial_second;

	MMPLAYER_FENTER();
	MMPLAYER_RETURN_VAL_IF_FAIL (player && player->pipeline && player->pipeline->mainbin, FALSE);

	mainbin = player->pipeline->mainbin;

	if ((!MMPLAYER_IS_HTTP_PD(player)) &&
		(MMPLAYER_IS_HTTP_STREAMING(player)))
	{
		LOGD ("creating http streaming buffering queue (queue2)\n");

		if (mainbin[MMPLAYER_M_MUXED_S_BUFFER].gst)
		{
			LOGE ("MMPLAYER_M_MUXED_S_BUFFER is not null\n");
		}
		else
	    {
			queue2 = gst_element_factory_make ("queue2", "queue2");
			if (!queue2)
			{
				LOGE ("failed to create buffering queue element\n");
				goto ERROR;
			}

			if (!gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), queue2))
			{
				LOGE("failed to add buffering queue\n");
				goto ERROR;
			}

			sinkpad = gst_element_get_static_pad(queue2, "sink");
			qsrcpad = gst_element_get_static_pad(queue2, "src");

			if (GST_PAD_LINK_OK != gst_pad_link(srcpad, sinkpad))
			{
				LOGE("failed to link buffering queue\n");
				goto ERROR;
			}

			// if ( !MMPLAYER_IS_HTTP_LIVE_STREAMING(player))
			{
				if ( !gst_element_query_duration(player->pipeline->mainbin[MMPLAYER_M_SRC].gst, GST_FORMAT_BYTES, &dur_bytes))
					LOGE("fail to get duration.\n");

				LOGD("dur_bytes = %lld\n", dur_bytes);

				if (dur_bytes > 0)
				{
					use_file_buffer = MMPLAYER_USE_FILE_FOR_BUFFERING(player);
					file_buffering_path = g_strdup(player->ini.http_file_buffer_path);
				}
				else
				{
					dur_bytes = 0;
				}
			}

			/* NOTE : we cannot get any duration info from ts container in case of streaming */
			// if(!g_strrstr(GST_ELEMENT_NAME(sinkelement), "mpegtsdemux"))
			if(!g_strrstr(player->type, "video/mpegts"))
			{
				max_buffer_size_bytes = (use_file_buffer)?(player->ini.http_max_size_bytes):(5*1024*1024);
				LOGD("max_buffer_size_bytes = %d\n", max_buffer_size_bytes);

				__mm_player_streaming_set_queue2(player->streamer,
												queue2,
												FALSE,
												max_buffer_size_bytes,
												player->ini.http_buffering_time,
												1.0,								// no meaning
												player->ini.http_buffering_limit,	// no meaning
												use_file_buffer,
												file_buffering_path,
												(guint64)dur_bytes);
			}

			MMPLAYER_FREEIF(file_buffering_path);
			if (GST_STATE_CHANGE_FAILURE == gst_element_sync_state_with_parent (queue2))
			{
				LOGE("failed to sync queue2 state with parent\n");
				goto ERROR;
			}

			srcpad = qsrcpad;

			gst_object_unref(GST_OBJECT(sinkpad));

			mainbin[MMPLAYER_M_MUXED_S_BUFFER].id = MMPLAYER_M_MUXED_S_BUFFER;
			mainbin[MMPLAYER_M_MUXED_S_BUFFER].gst = queue2;
		}
	}

	/* create decodebin */
	decodebin = __mmplayer_create_decodebin(player);

	if (!decodebin)
	{
		LOGE("can not create autoplug element\n");
		goto ERROR;
	}

	if (!gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), decodebin))
	{
		LOGE("failed to add decodebin\n");
		goto ERROR;
	}

	/* to force caps on the decodebin element and avoid reparsing stuff by
	* typefind. It also avoids a deadlock in the way typefind activates pads in
	* the state change */
	g_object_set (decodebin, "sink-caps", caps, NULL);

	sinkpad = gst_element_get_static_pad(decodebin, "sink");

	if (GST_PAD_LINK_OK != gst_pad_link(srcpad, sinkpad))
	{
		LOGE("failed to link decodebin\n");
		goto ERROR;
	}

	gst_object_unref(GST_OBJECT(sinkpad));

	mainbin[MMPLAYER_M_AUTOPLUG].id = MMPLAYER_M_AUTOPLUG;
	mainbin[MMPLAYER_M_AUTOPLUG].gst = decodebin;

	/* set decodebin property about buffer in streaming playback. *
	 * in case of hls, it does not need to have big buffer        *
	 * because it is kind of adaptive streaming.                  */
	if ( ((!MMPLAYER_IS_HTTP_PD(player)) &&
	    (MMPLAYER_IS_HTTP_STREAMING(player))) || MMPLAYER_IS_DASH_STREAMING (player))
	{
		guint max_size_bytes = MAX_DECODEBIN_BUFFER_BYTES;
		guint64 max_size_time = MAX_DECODEBIN_BUFFER_TIME;
		init_buffering_time = (init_buffering_time != 0)?(init_buffering_time):(player->ini.http_buffering_time);

		if (MMPLAYER_IS_HTTP_LIVE_STREAMING(player)) {
			max_size_bytes = MAX_DECODEBIN_ADAPTIVE_BUFFER_BYTES;
			max_size_time = MAX_DECODEBIN_ADAPTIVE_BUFFER_TIME;
		}

		g_object_set (G_OBJECT(decodebin), "use-buffering", TRUE,
											"high-percent", (gint)player->ini.http_buffering_limit,
											"low-percent", 1,   // 1%
											"max-size-bytes", max_size_bytes,
											"max-size-time", (guint64)(max_size_time * GST_SECOND),
											"max-size-buffers", 0, NULL);  // disable or automatic
	}

	if (GST_STATE_CHANGE_FAILURE == gst_element_sync_state_with_parent(decodebin))
	{
		LOGE("failed to sync decodebin state with parent\n");
		goto ERROR;
	}

	MMPLAYER_FLEAVE();

	return TRUE;

ERROR:

	MMPLAYER_FREEIF( caps_str );

	if (sinkpad)
		gst_object_unref(GST_OBJECT(sinkpad));

	if (queue2)
	{
		/* NOTE : Trying to dispose element queue0, but it is in READY instead of the NULL state.
		 * You need to explicitly set elements to the NULL state before
		 * dropping the final reference, to allow them to clean up.
		 */
		gst_element_set_state(queue2, GST_STATE_NULL);

		/* And, it still has a parent "player".
		 * You need to let the parent manage the object instead of unreffing the object directly.
		 */
		gst_bin_remove (GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), queue2);
		gst_object_unref (queue2);
		queue2 = NULL;
	}

	if (decodebin)
	{
		/* NOTE : Trying to dispose element queue0, but it is in READY instead of the NULL state.
		 * You need to explicitly set elements to the NULL state before
		 * dropping the final reference, to allow them to clean up.
		 */
		gst_element_set_state(decodebin, GST_STATE_NULL);

		/* And, it still has a parent "player".
		 * You need to let the parent manage the object instead of unreffing the object directly.
		 */

		gst_bin_remove (GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), decodebin);
		gst_object_unref (decodebin);
		decodebin = NULL;
	}

	return FALSE;
}

/* it will return first created element */
static gboolean
__mmplayer_try_to_plug(mm_player_t* player, GstPad *pad, const GstCaps *caps) // @
{
	MMPlayerGstElement* mainbin = NULL;
	const char* mime = NULL;
	const GList* item = NULL;
	const gchar* klass = NULL;
	GstCaps* res = NULL;
	gboolean skip = FALSE;
	GstPad* queue_pad = NULL;
	GstElement* queue = NULL;
	GstElement *element = NULL;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL( player && player->pipeline && player->pipeline->mainbin, FALSE );

	mainbin = player->pipeline->mainbin;

   	mime = gst_structure_get_name(gst_caps_get_structure(caps, 0));

	/* return if we got raw output */
	if(g_str_has_prefix(mime, "video/x-raw") || g_str_has_prefix(mime, "audio/x-raw")
		|| g_str_has_prefix(mime, "text/plain") ||g_str_has_prefix(mime, "text/x-pango-markup"))
	{

		element = (GstElement*)gst_pad_get_parent(pad);
/* NOTE : When no decoder has added during autoplugging. like a simple wave playback.
 * No queue will be added. I think it can caused breaking sound when playing raw audio
 * frames but there's no different. Decodebin also doesn't add with those wav fils.
 * Anyway, currentely raw-queue seems not necessary.
 */
#if 1
		/* NOTE : check if previously linked element is demuxer/depayloader/parse means no decoder
		 * has linked. if so, we need to add queue for quality of output. note that
		 * decodebin also has same problem.
		 */
		klass = gst_element_factory_get_metadata (gst_element_get_factory(element), GST_ELEMENT_METADATA_KLASS);

		/* add queue if needed */
		if( (g_strrstr(klass, "Demux") || g_strrstr(klass, "Depayloader")
			|| g_strrstr(klass, "Parse")) &&  !g_str_has_prefix(mime, "text"))
		{
			LOGD("adding raw queue\n");

			queue = gst_element_factory_make("queue", NULL);
			if ( ! queue )
			{
				LOGW("failed to create queue\n");
				goto ERROR;
			}

			/* warmup */
			if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state(queue, GST_STATE_READY) )
			{
				LOGW("failed to set state READY to queue\n");
				goto ERROR;
			}

			/* add to pipeline */
			if ( ! gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), queue) )
			{
				LOGW("failed to add queue\n");
				goto ERROR;
			}

			/* link queue */
			queue_pad = gst_element_get_static_pad(queue, "sink");

			if ( GST_PAD_LINK_OK != gst_pad_link(pad, queue_pad) )
			{
				LOGW("failed to link queue\n");
				goto ERROR;
			}
			gst_object_unref ( GST_OBJECT(queue_pad) );
			queue_pad = NULL;

			/* running */
			if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state(queue, GST_STATE_PAUSED) )
			{
				LOGW("failed to set state PAUSED to queue\n");
				goto ERROR;
			}

			/* replace given pad to queue:src */
			pad = gst_element_get_static_pad(queue, "src");
			if ( ! pad )
			{
				LOGW("failed to get pad from queue\n");
				goto ERROR;
			}
		}
#endif
		/* check if player can do start continually */
		MMPLAYER_CHECK_CMD_IF_EXIT(player);

		if(__mmplayer_link_sink(player,pad))
			__mmplayer_gst_decode_callback(element, pad, player);

		gst_object_unref( GST_OBJECT(element));
		element = NULL;

		return TRUE;
	}

	item = player->factories;
	for(; item != NULL ; item = item->next)
	{
		GstElementFactory *factory = GST_ELEMENT_FACTORY(item->data);
		const GList *pads;
		gint idx = 0;

		skip = FALSE;

		/* filtering exclude keyword */
		for ( idx = 0; player->ini.exclude_element_keyword[idx][0] != '\0'; idx++ )
		{
			if ( g_strrstr(GST_OBJECT_NAME (factory),
					player->ini.exclude_element_keyword[idx] ) )
			{
				LOGW("skipping [%s] by exculde keyword [%s]\n",
					GST_OBJECT_NAME (factory),
					player->ini.exclude_element_keyword[idx] );

				skip = TRUE;
				break;
			}
		}

		if ( MMPLAYER_IS_RTSP_STREAMING(player) && g_strrstr(GST_OBJECT_NAME (factory), "omx_mpeg4dec"))
		{
			// omx decoder can not support mpeg4video data partitioned
			// rtsp streaming didn't know mpeg4video data partitioned format
			// so, if rtsp playback, player will skip omx_mpeg4dec.
			LOGW("skipping [%s] when rtsp streaming \n",
					GST_OBJECT_NAME (factory));

			skip = TRUE;
		}

		if ( skip ) continue;

		/* check factory class for filtering */
		klass = gst_element_factory_get_metadata (GST_ELEMENT_FACTORY(factory), GST_ELEMENT_METADATA_KLASS);

		/*parsers are not required in case of external feeder*/
		if (g_strrstr(klass, "Codec/Parser") && MMPLAYER_IS_MS_BUFF_SRC(player))
			continue;

		/* NOTE : msl don't need to use image plugins.
		 * So, those plugins should be skipped for error handling.
		 */
		if ( g_strrstr(klass, "Codec/Decoder/Image") )
		{
			LOGD("skipping [%s] by not required\n", GST_OBJECT_NAME (factory));
			continue;
		}

		/* check pad compatability */
		for(pads = gst_element_factory_get_static_pad_templates(factory);
					pads != NULL; pads=pads->next)
		{
			GstStaticPadTemplate *temp1 = pads->data;
			GstCaps* static_caps = NULL;

			if( temp1->direction != GST_PAD_SINK
				|| temp1->presence != GST_PAD_ALWAYS)
				continue;

			if ( GST_IS_CAPS( &temp1->static_caps.caps) )
			{
				/* using existing caps */
				static_caps = gst_caps_ref(temp1->static_caps.caps );
			}
			else
			{
				/* create one */
				static_caps = gst_caps_from_string ( temp1->static_caps.string );
			}

			res = gst_caps_intersect((GstCaps*)caps, static_caps);
			gst_caps_unref( static_caps );
			static_caps = NULL;

			if( res && !gst_caps_is_empty(res) )
			{
				GstElement *new_element;
				GList *elements = player->parsers;
				char *name_template = g_strdup(temp1->name_template);
				gchar *name_to_plug = GST_OBJECT_NAME(factory);
				gst_caps_unref(res);

				/* check ALP Codec can be used or not */
				if ((g_strrstr(klass, "Codec/Decoder/Audio")))
				{
					/* consider mp3 audio only */
					if ( !MMPLAYER_IS_STREAMING(player) && __mmplayer_is_only_mp3_type(player->type) )
					{
						/* try to use ALP decoder first instead of selected decoder */
						GstElement *element = NULL;
						GstElementFactory * element_facory;
						gchar *path = NULL;
						guint64 data_size = 0;
						#define MIN_THRESHOLD_SIZE  320 * 1024 // 320K
						struct stat sb;

						mm_attrs_get_string_by_name(player->attrs, "profile_uri", &path);

						if (stat(path, &sb) == 0)
						{
							data_size = (guint64)sb.st_size;
						}
						LOGD("file size : %u", data_size);

						if (data_size > MIN_THRESHOLD_SIZE)
						{
							LOGD("checking if ALP can be used or not");
							element = gst_element_factory_make("omx_mp3dec", "omx mp3 decoder");
							if ( element )
							{
								/* check availability because multi-instance is not supported */
								GstStateChangeReturn ret = gst_element_set_state(element, GST_STATE_READY);

								if (ret != GST_STATE_CHANGE_SUCCESS) // use just selected decoder
								{
									gst_object_unref (element);
								}
								else if (ret == GST_STATE_CHANGE_SUCCESS) // replace facotry to use omx
								{
									/* clean  */
									gst_element_set_state(element, GST_STATE_NULL);
									gst_object_unref (element);

									element_facory = gst_element_factory_find("omx_mp3dec");
									/* replace, otherwise use selected thing instead */
									if (element_facory)
									{
										factory = element_facory;
										name_to_plug = GST_OBJECT_NAME(factory);
									}

									/* make parser alp mode */
									#ifdef _MM_PLAYER_ALP_PARSER
									g_list_foreach (player->parsers, check_name, player);
									#endif
								}
							}
						}
					}
				}
				else if ((g_strrstr(klass, "Codec/Decoder/Video")))
				{
					if ( g_strrstr(GST_OBJECT_NAME(factory), "omx_") )
					{
						char *env = getenv ("MM_PLAYER_HW_CODEC_DISABLE");
						if (env != NULL)
						{
							if (strncasecmp(env, "yes", 3) == 0)
							{
								LOGD("skipping [%s] by disabled\n", name_to_plug);
								MMPLAYER_FREEIF(name_template);
								continue;
							}
						}
					}
				}

				LOGD("found %s to plug\n", name_to_plug);

				new_element = gst_element_factory_create(GST_ELEMENT_FACTORY(factory), NULL);
				if ( ! new_element )
				{
					LOGE("failed to create element [%s]. continue with next.\n",
						GST_OBJECT_NAME (factory));

					MMPLAYER_FREEIF(name_template);

					continue;
				}

				/* check and skip it if it was already used. Otherwise, it can be an infinite loop
				 * because parser can accept its own output as input.
				 */
				if (g_strrstr(klass, "Parser"))
				{
					gchar *selected = NULL;

					for ( ; elements; elements = g_list_next(elements))
					{
						gchar *element_name = elements->data;

						if (g_strrstr(element_name, name_to_plug))
						{
							LOGD("but, %s already linked, so skipping it\n", name_to_plug);
							skip = TRUE;
						}
					}

					if (skip)
					{
						MMPLAYER_FREEIF(name_template);
						continue;
					}

					selected = g_strdup(name_to_plug);
					player->parsers = g_list_append(player->parsers, selected);
				}

				/* store specific handles for futher control */
				if(g_strrstr(klass, "Demux") || g_strrstr(klass, "Parse"))
				{
					/* FIXIT : first value will be overwritten if there's more
					 * than 1 demuxer/parser
					 */
					LOGD("plugged element is demuxer. take it\n");
					mainbin[MMPLAYER_M_DEMUX].id = MMPLAYER_M_DEMUX;
					mainbin[MMPLAYER_M_DEMUX].gst = new_element;

					/*Added for multi audio support */
					if(g_strrstr(klass, "Demux"))
					{
						mainbin[MMPLAYER_M_DEMUX_EX].id = MMPLAYER_M_DEMUX_EX;
						mainbin[MMPLAYER_M_DEMUX_EX].gst = new_element;

						/* NOTE : workaround for bug in mpegtsdemux since doesn't emit
						no-more-pad signal. this may cause wrong content attributes at PAUSED state
						this code should be removed after mpegtsdemux is fixed */
						if ( g_strrstr(GST_OBJECT_NAME(factory), "mpegtsdemux") )
						{
							LOGW("force no-more-pad to TRUE since mpegtsdemux os not giving no-more-pad signal. content attributes may wrong");
							player->no_more_pad = TRUE;
						}
					}
					if (g_strrstr(name_to_plug, "asfdemux")) // to support trust-zone only
					{
						g_object_set(mainbin[MMPLAYER_M_DEMUX_EX].gst, "file-location", player->profile.uri,NULL);
					}
				}
				else if(g_strrstr(klass, "Decoder") && __mmplayer_link_decoder(player,pad))
				{
					if(mainbin[MMPLAYER_M_DEC1].gst == NULL)
					{
						LOGD("plugged element is decoder. take it[MMPLAYER_M_DEC1]\n");
						mainbin[MMPLAYER_M_DEC1].id = MMPLAYER_M_DEC1;
						mainbin[MMPLAYER_M_DEC1].gst = new_element;
					}
					else if(mainbin[MMPLAYER_M_DEC2].gst == NULL)
					{
						LOGD("plugged element is decoder. take it[MMPLAYER_M_DEC2]\n");
						mainbin[MMPLAYER_M_DEC2].id = MMPLAYER_M_DEC2;
						mainbin[MMPLAYER_M_DEC2].gst = new_element;
					}
					/* NOTE : IF one codec is found, add it to supported_codec and remove from
					 * missing plugin. Both of them are used to check what's supported codec
					 * before returning result of play start. And, missing plugin should be
					 * updated here for multi track files.
					 */
					if(g_str_has_prefix(mime, "video"))
					{
						GstPad *src_pad = NULL;
						GstPadTemplate *pad_templ = NULL;
						GstCaps *caps = NULL;
						gchar *caps_str = NULL;

						LOGD("found VIDEO decoder\n");
						player->not_supported_codec &= MISSING_PLUGIN_AUDIO;
						player->can_support_codec |= FOUND_PLUGIN_VIDEO;

						src_pad = gst_element_get_static_pad (new_element, "src");
						pad_templ = gst_pad_get_pad_template (src_pad);
						caps = GST_PAD_TEMPLATE_CAPS(pad_templ);

						caps_str = gst_caps_to_string(caps);

						/* clean */
						MMPLAYER_FREEIF( caps_str );
						gst_object_unref (src_pad);
					}
					else if (g_str_has_prefix(mime, "audio"))
					{
						LOGD("found AUDIO decoder\n");
						player->not_supported_codec &= MISSING_PLUGIN_VIDEO;
						player->can_support_codec |= FOUND_PLUGIN_AUDIO;
					}
				}

				if ( ! __mmplayer_close_link(player, pad, new_element,
							name_template,gst_element_factory_get_static_pad_templates(factory)) )
				{
					MMPLAYER_FREEIF(name_template);
					if (player->keep_detecting_vcodec)
					continue;

					/* Link is failed even though a supportable codec is found. */
					__mmplayer_check_not_supported_codec(player, klass, mime);

					LOGE("failed to call _close_link\n");
					return FALSE;
				}

				MMPLAYER_FREEIF(name_template);
				return TRUE;
			}

			gst_caps_unref(res);
			break;
		}
	}

	/* There is no available codec. */
	__mmplayer_check_not_supported_codec(player, klass, mime);

	MMPLAYER_FLEAVE();
	return FALSE;

ERROR:
	/* release */
	if ( queue )
		gst_object_unref( queue );

	if ( queue_pad )
		gst_object_unref( queue_pad );

	if ( element )
		gst_object_unref ( element );

	return FALSE;
}


static int
__mmplayer_check_not_supported_codec(mm_player_t* player, const gchar* factory_class, const gchar* mime)
{
	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL(player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED);
	MMPLAYER_RETURN_VAL_IF_FAIL ( mime, MM_ERROR_INVALID_ARGUMENT );

	LOGD("class : %s, mime : %s \n", factory_class, mime );

	/* add missing plugin */
	/* NOTE : msl should check missing plugin for image mime type.
	 * Some motion jpeg clips can have playable audio track.
	 * So, msl have to play audio after displaying popup written video format not supported.
	 */
	if ( !( player->pipeline->mainbin[MMPLAYER_M_DEMUX].gst ) )
	{
		if ( !( player->can_support_codec | player->videodec_linked | player->audiodec_linked ) )
		{
			LOGD("not found demuxer\n");
			player->not_found_demuxer = TRUE;
			player->unlinked_demuxer_mime = g_strdup_printf ( "%s", mime );

			goto DONE;
		}
	}

	if( !g_strrstr(factory_class, "Demuxer"))
	{
		if( ( g_str_has_prefix(mime, "video") ) ||( g_str_has_prefix(mime, "image") ) )
		{
			LOGD("can support codec=%d, vdec_linked=%d, adec_linked=%d\n",
				player->can_support_codec, player->videodec_linked, player->audiodec_linked);

			/* check that clip have multi tracks or not */
			if ( ( player->can_support_codec & FOUND_PLUGIN_VIDEO ) && ( player->videodec_linked ) )
			{
				LOGD("video plugin is already linked\n");
			}
			else
			{
				LOGW("add VIDEO to missing plugin\n");
				player->not_supported_codec |= MISSING_PLUGIN_VIDEO;
			}
		}
		else if ( g_str_has_prefix(mime, "audio") )
		{
			if ( ( player->can_support_codec & FOUND_PLUGIN_AUDIO ) && ( player->audiodec_linked ) )
			{
				LOGD("audio plugin is already linked\n");
			}
			else
			{
				LOGW("add AUDIO to missing plugin\n");
				player->not_supported_codec |= MISSING_PLUGIN_AUDIO;
			}
		}
	}

DONE:
	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}


static void
__mmplayer_pipeline_complete(GstElement *decodebin,  gpointer data)
{
    mm_player_t* player = (mm_player_t*)data;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL( player );

	/* remove fakesink. */
	if ( !__mmplayer_gst_remove_fakesink( player,
				&player->pipeline->mainbin[MMPLAYER_M_SRC_FAKESINK]) )
	{
		/* NOTE : __mmplayer_pipeline_complete() can be called several time. because
		 * signaling mechanism ( pad-added, no-more-pad, new-decoded-pad ) from various
		 * source element are not same. To overcome this situation, this function will called
		 * several places and several times. Therefore, this is not an error case.
		 */
		return;
	}

	LOGD("pipeline has completely constructed\n");

	if (( player->ini.async_start ) &&
		( player->msg_posted == FALSE ) &&
		( player->cmd >= MMPLAYER_COMMAND_START ))
	{
		__mmplayer_handle_missed_plugin( player );
	}

	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-complete" );
}

static gboolean
__mmplayer_verify_next_play_path(mm_player_t *player)
{
	MMHandleType attrs = 0;
	MMPlayerParseProfile profile;
	gint uri_idx = 0, check_cnt = 0;
	char *uri = NULL;
	gint mode = MM_PLAYER_PD_MODE_NONE;
	gint count = 0;
	guint num_of_list = 0;

	MMPLAYER_FENTER();

	LOGD("checking for gapless play");

	if (player->pipeline->textbin)
	{
		LOGE("subtitle path is enabled. gapless play is not supported.\n");
		goto ERROR;
	}

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		LOGE("fail to get attributes.\n");
		goto ERROR;
	}

	if (mm_attrs_get_int_by_name (attrs, "pd_mode", &mode) == MM_ERROR_NONE)
	{
		if (mode == TRUE)
		{
			LOGW("pd mode\n");
			goto ERROR;
		}
	}

	if (mm_attrs_get_int_by_name(attrs, "profile_play_count", &count) != MM_ERROR_NONE)
	{
		LOGE("can not get play count\n");
	}

	num_of_list = g_list_length(player->uri_info.uri_list);

	LOGD("repeat count = %d, num_of_list = %d\n", count, num_of_list);

	if ( num_of_list == 0 )
	{
		if (mm_attrs_get_string_by_name(player->attrs, "profile_uri", &uri) != MM_ERROR_NONE)
		{
			LOGE("can not get profile_uri\n");
			goto ERROR;
		}

		if (!uri)
		{
			LOGE("uri list is empty.\n");
			goto ERROR;
		}

		player->uri_info.uri_list = g_list_append(player->uri_info.uri_list, g_strdup(uri));
		LOGD("add original path : %s ", uri);

		num_of_list = 1;
		uri= NULL;
	}

	uri_idx = player->uri_info.uri_idx;

	while(TRUE)
	{
		check_cnt++;

		if (check_cnt > num_of_list)
		{
			LOGE("there is no valid uri.");
			goto ERROR;
		}

		LOGD("uri idx : %d / %d\n", uri_idx, num_of_list);

		if ( uri_idx < num_of_list-1 )
		{
			uri_idx++;
		}
		else
		{
			if ((count <= 1) && (count != -1))
			{
				LOGD("no repeat.");
				goto ERROR;
			}
			else if ( count > 1 )	/* decrease play count */
			{
				/* we successeded to rewind. update play count and then wait for next EOS */
				count--;

				mm_attrs_set_int_by_name(attrs, "profile_play_count", count);

				/* commit attribute */
				if ( mmf_attrs_commit ( attrs ) )
				{
					LOGE("failed to commit attribute\n");
				}
			}

			/* count < 0 : repeat continually */
			uri_idx = 0;
		}

		uri = g_list_nth_data(player->uri_info.uri_list, uri_idx);
		LOGD("uri idx : %d, uri = %s\n", uri_idx, uri);

		if (uri == NULL)
		{
			LOGW("next uri does not exist\n");
			continue;
		}

		if (__mmfplayer_parse_profile((const char*)uri, NULL, &profile) != MM_ERROR_NONE)
		{
			LOGE("failed to parse profile\n");
			continue;
		}

		if ((profile.uri_type != MM_PLAYER_URI_TYPE_FILE) &&
			(profile.uri_type != MM_PLAYER_URI_TYPE_URL_HTTP))
		{
			LOGW("uri type is not supported (%d).", profile.uri_type);
			continue;
		}

		break;
	}

	player->uri_info.uri_idx = uri_idx;
	mm_attrs_set_string_by_name(player->attrs, "profile_uri", uri);


	if (mmf_attrs_commit(player->attrs))
	{
		LOGE("failed to commit.\n");
		goto ERROR;
	}

	LOGD("next uri %s (%d)\n", uri, uri_idx);

	return TRUE;

ERROR:

	LOGE("unable to play next path. EOS will be posted soon.\n");
	return FALSE;
}

static void
__mmplayer_initialize_next_play(mm_player_t *player)
{
	int i;

	MMPLAYER_FENTER();

	player->smooth_streaming = FALSE;
	player->videodec_linked = 0;
	player->audiodec_linked = 0;
	player->videosink_linked = 0;
	player->audiosink_linked = 0;
	player->textsink_linked = 0;
	player->is_external_subtitle_present = FALSE;
	player->not_supported_codec = MISSING_PLUGIN_NONE;
	player->can_support_codec = FOUND_PLUGIN_NONE;
	player->pending_seek.is_pending = FALSE;
	player->pending_seek.format = MM_PLAYER_POS_FORMAT_TIME;
	player->pending_seek.pos = 0;
	player->msg_posted = FALSE;
	player->has_many_types = FALSE;
	player->no_more_pad = FALSE;
	player->is_drm_file = FALSE;
	player->not_found_demuxer = 0;
	player->doing_seek = FALSE;
	player->max_audio_channels = 0;
	player->is_subtitle_force_drop = FALSE;
	player->play_subtitle = FALSE;
	player->use_textoverlay = FALSE;
	player->adjust_subtitle_pos = 0;

	player->updated_bitrate_count = 0;
	player->total_bitrate = 0;
	player->updated_maximum_bitrate_count = 0;
	player->total_maximum_bitrate = 0;

	_mmplayer_track_initialize(player);

	for (i = 0; i < MM_PLAYER_STREAM_COUNT_MAX; i++)
	{
		player->bitrate[i] = 0;
		player->maximum_bitrate[i] = 0;
	}

	if (player->v_stream_caps)
	{
		gst_caps_unref(player->v_stream_caps);
		player->v_stream_caps = NULL;
	}

	mm_attrs_set_int_by_name(player->attrs, "content_video_found", 0);
	mm_attrs_set_int_by_name(player->attrs, "content_audio_found", 0);

	/* clean found parsers */
	if (player->parsers)
	{
		GList *parsers = player->parsers;
		for ( ;parsers ; parsers = g_list_next(parsers))
		{
			gchar *name = parsers->data;
			MMPLAYER_FREEIF(name);
		}
		g_list_free(player->parsers);
		player->parsers = NULL;
	}

	/* clean found audio decoders */
	if (player->audio_decoders)
	{
		GList *a_dec = player->audio_decoders;
		for ( ;a_dec ; a_dec = g_list_next(a_dec))
		{
			gchar *name = a_dec->data;
			MMPLAYER_FREEIF(name);
		}
		g_list_free(player->audio_decoders);
		player->audio_decoders = NULL;
	}

	MMPLAYER_FLEAVE();
}

static void
__mmplayer_activate_next_source(mm_player_t *player, GstState target)
{
	MMPlayerGstElement *mainbin = NULL;
	MMMessageParamType msg_param = {0,};
	GstElement *element = NULL;
	MMHandleType attrs = 0;
	char *uri = NULL;
	enum MainElementID elemId = MMPLAYER_M_NUM;

	MMPLAYER_FENTER();

	if ((player == NULL) ||
		(player->pipeline == NULL) ||
		(player->pipeline->mainbin == NULL))
	{
		LOGE("player is null.\n");
		goto ERROR;
	}

	mainbin = player->pipeline->mainbin;
	msg_param.code = MM_ERROR_PLAYER_INTERNAL;

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		LOGE("fail to get attributes.\n");
		goto ERROR;
	}

	/* Initialize Player values */
	__mmplayer_initialize_next_play(player);

	mm_attrs_get_string_by_name(attrs, "profile_uri", &uri);

	if (__mmfplayer_parse_profile((const char*)uri, NULL, &player->profile) != MM_ERROR_NONE)
	{
		LOGE("failed to parse profile\n");
		msg_param.code = MM_ERROR_PLAYER_INVALID_URI;
		goto ERROR;
	}

	if ((MMPLAYER_URL_HAS_DASH_SUFFIX(player)) ||
		(MMPLAYER_URL_HAS_HLS_SUFFIX(player)))
	{
		LOGE("it's dash or hls. not support.");
		msg_param.code = MM_ERROR_PLAYER_INVALID_URI;
		goto ERROR;
	}

	/* setup source */
	switch ( player->profile.uri_type )
	{
		/* file source */
		case MM_PLAYER_URI_TYPE_FILE:
		{
			LOGD("using filesrc for 'file://' handler.\n");

			element = gst_element_factory_make("filesrc", "source");

			if ( !element )
			{
				LOGE("failed to create filesrc\n");
				break;
			}

			g_object_set(G_OBJECT(element), "location", (player->profile.uri)+7, NULL);	/* uri+7 -> remove "file:// */
			break;
		}
		case MM_PLAYER_URI_TYPE_URL_HTTP:
		{
			gchar *user_agent, *proxy, *cookies, **cookie_list;
			gint http_timeout = DEFAULT_HTTP_TIMEOUT;
			user_agent = proxy = cookies = NULL;
			cookie_list = NULL;

			element = gst_element_factory_make(player->ini.httpsrc_element, "http_streaming_source");
			if ( !element )
			{
				LOGE("failed to create http streaming source element[%s].\n", player->ini.httpsrc_element);
				break;
			}
			LOGD("using http streamming source [%s].\n", player->ini.httpsrc_element);

			/* get attribute */
			mm_attrs_get_string_by_name ( attrs, "streaming_cookie", &cookies );
			mm_attrs_get_string_by_name ( attrs, "streaming_user_agent", &user_agent );
			mm_attrs_get_string_by_name ( attrs, "streaming_proxy", &proxy );
			mm_attrs_get_int_by_name ( attrs, "streaming_timeout", &http_timeout );

			if ((http_timeout == DEFAULT_HTTP_TIMEOUT) &&
				(player->ini.http_timeout != DEFAULT_HTTP_TIMEOUT))
			{
				LOGD("get timeout from ini\n");
				http_timeout = player->ini.http_timeout;
			}

			/* get attribute */
			SECURE_LOGD("location : %s\n", player->profile.uri);
			SECURE_LOGD("cookies : %s\n", cookies);
			SECURE_LOGD("proxy : %s\n", proxy);
			SECURE_LOGD("user_agent :  %s\n", user_agent);
			LOGD("timeout : %d\n", http_timeout);

			/* setting property to streaming source */
			g_object_set(G_OBJECT(element), "location", player->profile.uri, NULL);
			g_object_set(G_OBJECT(element), "timeout", http_timeout, NULL);
			g_object_set(G_OBJECT(element), "blocksize", (unsigned long)(64*1024), NULL);

			/* check if prosy is vailid or not */
			if ( util_check_valid_url ( proxy ) )
				g_object_set(G_OBJECT(element), "proxy", proxy, NULL);
			/* parsing cookies */
			if ( ( cookie_list = util_get_cookie_list ((const char*)cookies) ) )
				g_object_set(G_OBJECT(element), "cookies", cookie_list, NULL);
			if ( user_agent )
				g_object_set(G_OBJECT(element), "user_agent", user_agent, NULL);
			break;
		}
		default:
			LOGE("not support uri type %d\n", player->profile.uri_type);
			break;
	}

	if ( !element )
	{
		LOGE("no source element was created.\n");
		goto ERROR;
	}

	if (gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), element) == FALSE)
	{
		LOGE("failed to add source element to pipeline\n");
		gst_object_unref(GST_OBJECT(element));
		element = NULL;
		goto ERROR;
	}

	/* take source element */
	mainbin[MMPLAYER_M_SRC].id = MMPLAYER_M_SRC;
	mainbin[MMPLAYER_M_SRC].gst = element;

	element = NULL;

	if (MMPLAYER_IS_HTTP_STREAMING(player))
	{
		if (player->streamer == NULL)
		{
			player->streamer = __mm_player_streaming_create();
			__mm_player_streaming_initialize(player->streamer);
		}

		elemId = MMPLAYER_M_TYPEFIND;
		element = gst_element_factory_make("typefind", "typefinder");
		MMPLAYER_SIGNAL_CONNECT( player, element, MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "have-type",
			G_CALLBACK(__mmplayer_typefind_have_type), (gpointer)player );
	}
	else
	{
		elemId = MMPLAYER_M_AUTOPLUG;
		element = __mmplayer_create_decodebin(player);
	}

	/* check autoplug element is OK */
	if ( ! element )
	{
		LOGE("can not create element (%d)\n", elemId);
		goto ERROR;
	}

	if (gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), element) == FALSE)
	{
		LOGE("failed to add sinkbin to pipeline\n");
		gst_object_unref(GST_OBJECT(element));
		element = NULL;
		goto ERROR;
	}

	mainbin[elemId].id = elemId;
	mainbin[elemId].gst = element;

	if ( gst_element_link (mainbin[MMPLAYER_M_SRC].gst, mainbin[elemId].gst) == FALSE )
	{
		LOGE("Failed to link src - autoplug (or typefind)\n");
		goto ERROR;
	}

	if (gst_element_set_state (mainbin[MMPLAYER_M_SRC].gst, target) == GST_STATE_CHANGE_FAILURE)
	{
		LOGE("Failed to change state of src element\n");
		goto ERROR;
	}

	if (!MMPLAYER_IS_HTTP_STREAMING(player))
	{
		if (gst_element_set_state (mainbin[MMPLAYER_M_AUTOPLUG].gst, target) == GST_STATE_CHANGE_FAILURE)
		{
			LOGE("Failed to change state of decodebin\n");
			goto ERROR;
		}
	}
	else
	{
		if (gst_element_set_state (mainbin[MMPLAYER_M_TYPEFIND].gst, target) == GST_STATE_CHANGE_FAILURE)
		{
			LOGE("Failed to change state of src element\n");
			goto ERROR;
		}
	}

	player->gapless.stream_changed = TRUE;
	player->gapless.running = TRUE;
	MMPLAYER_FLEAVE();
	return;

ERROR:
	MMPLAYER_PLAYBACK_UNLOCK(player);

	if (player && !player->msg_posted)
	{
		MMPLAYER_POST_MSG(player, MM_MESSAGE_ERROR, &msg_param);
		player->msg_posted = TRUE;
	}
	return;
}

static gboolean
__mmplayer_deactivate_selector(mm_player_t *player, MMPlayerTrackType type)
{
	mm_player_selector_t *selector = &player->selector[type];
	MMPlayerGstElement *sinkbin = NULL;
	enum MainElementID selectorId = MMPLAYER_M_NUM;
	enum MainElementID sinkId = MMPLAYER_M_NUM;
	GstPad *srcpad = NULL;
	GstPad *sinkpad = NULL;

	MMPLAYER_FENTER();
	MMPLAYER_RETURN_VAL_IF_FAIL (player, FALSE);

	LOGD("type %d", type);

	switch (type)
	{
		case MM_PLAYER_TRACK_TYPE_AUDIO:
			selectorId = MMPLAYER_M_A_INPUT_SELECTOR;
			sinkId = MMPLAYER_A_BIN;
			sinkbin = player->pipeline->audiobin;
		break;
		case MM_PLAYER_TRACK_TYPE_VIDEO:
			selectorId = MMPLAYER_M_V_INPUT_SELECTOR;
			sinkId = MMPLAYER_V_BIN;
			sinkbin = player->pipeline->videobin;
		break;
		case MM_PLAYER_TRACK_TYPE_TEXT:
			selectorId = MMPLAYER_M_T_INPUT_SELECTOR;
			sinkId = MMPLAYER_T_BIN;
			sinkbin = player->pipeline->textbin;
		break;
		default:
			LOGE("requested type is not supportable");
			return FALSE;
		break;
	}

	if (player->pipeline->mainbin[selectorId].gst)
	{
		gint n;

		srcpad = gst_element_get_static_pad(player->pipeline->mainbin[selectorId].gst, "src");

		if (selector->event_probe_id != 0)
			gst_pad_remove_probe (srcpad, selector->event_probe_id);
		selector->event_probe_id = 0;

		if ((sinkbin) && (sinkbin[sinkId].gst))
		{
			sinkpad = gst_element_get_static_pad(sinkbin[sinkId].gst, "sink");

			if (srcpad && sinkpad)
			{
				/* after getting drained signal there is no data flows, so no need to do pad_block */
				LOGD("unlink %s:%s, %s:%s", GST_DEBUG_PAD_NAME(srcpad), GST_DEBUG_PAD_NAME(sinkpad));
				gst_pad_unlink (srcpad, sinkpad);
			}

			gst_object_unref (sinkpad);
			sinkpad = NULL;
		}
		gst_object_unref (srcpad);
		srcpad = NULL;

		LOGD("selector release");

		/* release and unref requests pad from the selector */
		for (n = 0; n < selector->channels->len; n++)
		{
			GstPad *sinkpad = g_ptr_array_index (selector->channels, n);
			gst_element_release_request_pad ((player->pipeline->mainbin[selectorId].gst), sinkpad);
		}
		g_ptr_array_set_size (selector->channels, 0);

		gst_element_set_state (player->pipeline->mainbin[selectorId].gst, GST_STATE_NULL);
		gst_bin_remove (GST_BIN_CAST (player->pipeline->mainbin[MMPLAYER_M_PIPE].gst), player->pipeline->mainbin[selectorId].gst);

		player->pipeline->mainbin[selectorId].gst = NULL;
		selector = NULL;
	}

	return TRUE;
}

static void
__mmplayer_deactivate_old_path(mm_player_t *player)
{
	MMPLAYER_FENTER();
	MMPLAYER_RETURN_IF_FAIL ( player );

	if ((!__mmplayer_deactivate_selector(player, MM_PLAYER_TRACK_TYPE_AUDIO)) ||
		(!__mmplayer_deactivate_selector(player, MM_PLAYER_TRACK_TYPE_VIDEO)) ||
		(!__mmplayer_deactivate_selector(player, MM_PLAYER_TRACK_TYPE_TEXT)))
	{
		LOGE("deactivate selector error");
		goto ERROR;
	}

	_mmplayer_track_destroy(player);
	__mmplayer_release_signal_connection( player, MM_PLAYER_SIGNAL_TYPE_AUTOPLUG );

	if (player->streamer)
	{
		__mm_player_streaming_deinitialize (player->streamer);
		__mm_player_streaming_destroy(player->streamer);
		player->streamer = NULL;
	}

	MMPLAYER_PLAYBACK_LOCK(player);
	g_cond_signal( &player->next_play_thread_cond );

	MMPLAYER_FLEAVE();
	return;

ERROR:

	if (!player->msg_posted)
	{
		MMMessageParamType msg = {0,};

		/*post error*/
		msg.code = MM_ERROR_PLAYER_INTERNAL;
		LOGE("next_uri_play> deactivate error");

		MMPLAYER_POST_MSG(player, MM_MESSAGE_ERROR, &msg);
		player->msg_posted = TRUE;
	}
	return;
}

int _mmplayer_set_uri(MMHandleType hplayer, const char* uri)
{
	int result = MM_ERROR_NONE;
	mm_player_t* player = (mm_player_t*) hplayer;
	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL (player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	mm_attrs_set_string_by_name(player->attrs, "profile_uri", uri);
	if (mmf_attrs_commit(player->attrs))
	{
		LOGE("failed to commit the original uri.\n");
		result = MM_ERROR_PLAYER_INTERNAL;
	}
	else
	{
		if (_mmplayer_set_next_uri(hplayer, uri, TRUE) != MM_ERROR_NONE)
		{
			LOGE("failed to add the original uri in the uri list.\n");
		}
	}

	MMPLAYER_FLEAVE();
	return result;
}

int _mmplayer_set_next_uri(MMHandleType hplayer, const char* uri, bool is_first_path)
{
	mm_player_t* player = (mm_player_t*) hplayer;
	guint num_of_list = 0;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL (player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	MMPLAYER_RETURN_VAL_IF_FAIL (uri, MM_ERROR_INVALID_ARGUMENT);

	if (player->pipeline && player->pipeline->textbin)
	{
		LOGE("subtitle path is enabled.\n");
		return MM_ERROR_PLAYER_INVALID_STATE;
	}

	num_of_list = g_list_length(player->uri_info.uri_list);

	if (is_first_path == TRUE)
	{
		if (num_of_list == 0)
		{
			player->uri_info.uri_list = g_list_append(player->uri_info.uri_list, g_strdup(uri));
			LOGD("add original path : %s", uri);
		}
		else
		{
			player->uri_info.uri_list = g_list_delete_link(player->uri_info.uri_list, g_list_nth(player->uri_info.uri_list, 0));
			player->uri_info.uri_list = g_list_insert(player->uri_info.uri_list, g_strdup(uri), 0);

			LOGD("change original path : %s", uri);
		}
	}
	else
	{
		if (num_of_list == 0)
		{
			MMHandleType attrs = 0;
			char *original_uri = NULL;

			attrs = MMPLAYER_GET_ATTRS(player);
			if ( attrs )
			{
				mm_attrs_get_string_by_name(attrs, "profile_uri", &original_uri);

				if (!original_uri)
				{
					LOGE("there is no original uri.");
					return MM_ERROR_PLAYER_INVALID_STATE;
				}

				player->uri_info.uri_list = g_list_append(player->uri_info.uri_list, g_strdup(original_uri));
				player->uri_info.uri_idx = 0;

				LOGD("add original path at first : %s (%d)", original_uri);
			}
		}

		player->uri_info.uri_list = g_list_append(player->uri_info.uri_list, g_strdup(uri));
		LOGD("add new path : %s (total num of list = %d)", uri, g_list_length(player->uri_info.uri_list));
	}

	MMPLAYER_FLEAVE();
	return MM_ERROR_NONE;
}

int _mmplayer_get_next_uri(MMHandleType hplayer, char** uri)
{
	mm_player_t* player = (mm_player_t*) hplayer;
	char *next_uri = NULL;
	guint num_of_list = 0;

	MMPLAYER_FENTER();
	MMPLAYER_RETURN_VAL_IF_FAIL (player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	num_of_list = g_list_length(player->uri_info.uri_list);

	if (num_of_list > 0)
	{
		gint uri_idx = player->uri_info.uri_idx;

		if ( uri_idx < num_of_list-1 )
			uri_idx++;
		else
			uri_idx = 0;

		next_uri = g_list_nth_data(player->uri_info.uri_list, uri_idx);
		LOGE("next uri idx : %d, uri = %s\n", uri_idx, next_uri);

		*uri = g_strdup(next_uri);
	}

	MMPLAYER_FLEAVE();
	return MM_ERROR_NONE;
}

static void
__mmplayer_gst_decode_unknown_type(GstElement *elem,  GstPad* pad,
GstCaps *caps, gpointer data)
{
	mm_player_t* player = (mm_player_t*)data;
	const gchar* klass = NULL;
	const gchar* mime = NULL;
	gchar* caps_str = NULL;

	klass = gst_element_factory_get_metadata (gst_element_get_factory(elem), GST_ELEMENT_METADATA_KLASS);
	mime = gst_structure_get_name (gst_caps_get_structure(caps, 0));
	caps_str = gst_caps_to_string(caps);

	LOGW("unknown type of caps : %s from %s",
					caps_str, GST_ELEMENT_NAME (elem));

	MMPLAYER_FREEIF(caps_str);

	/* There is no available codec. */
	__mmplayer_check_not_supported_codec (player, klass, mime);
}

static gboolean
__mmplayer_gst_decode_autoplug_continue(GstElement *bin,  GstPad* pad,
GstCaps * caps,  gpointer data)
{
	mm_player_t* player = (mm_player_t*)data;
	const char* mime = NULL;
	gboolean ret = TRUE;

	MMPLAYER_LOG_GST_CAPS_TYPE(caps);
	mime = gst_structure_get_name (gst_caps_get_structure(caps, 0));

	if (g_str_has_prefix(mime, "audio")) {
		GstStructure* caps_structure = NULL;
		gint samplerate = 0;
		gint channels = 0;
		gchar *caps_str = NULL;

		caps_structure = gst_caps_get_structure(caps, 0);
		gst_structure_get_int (caps_structure, "rate", &samplerate);
		gst_structure_get_int (caps_structure, "channels", &channels);

		if ( (channels > 0 && samplerate == 0)) {
			LOGD("exclude audio...");
			ret = FALSE;
		}

		caps_str = gst_caps_to_string(caps);
		/* set it directly because not sent by TAG */
		if (g_strrstr(caps_str, "mobile-xmf")) {
			mm_attrs_set_string_by_name(player->attrs, "content_audio_codec", "mobile-xmf");
		}
		MMPLAYER_FREEIF (caps_str);
	} else if (g_str_has_prefix(mime, "video") && player->videodec_linked) {
		LOGD("already video linked");
		ret = FALSE;
	} else {
		LOGD("found new stream");
	}

	return ret;
}

static gint
__mmplayer_gst_decode_autoplug_select(GstElement *bin,  GstPad* pad,
GstCaps* caps, GstElementFactory* factory, gpointer data)
{
	/* NOTE : GstAutoplugSelectResult is defined in gstplay-enum.h but not exposed
	 We are defining our own and will be removed when it actually exposed */
	typedef enum {
		GST_AUTOPLUG_SELECT_TRY,
		GST_AUTOPLUG_SELECT_EXPOSE,
		GST_AUTOPLUG_SELECT_SKIP
	} GstAutoplugSelectResult;

	GstAutoplugSelectResult result = GST_AUTOPLUG_SELECT_TRY;
	mm_player_t* player = (mm_player_t*)data;

	gchar* factory_name = NULL;
	gchar* caps_str = NULL;
	const gchar* klass = NULL;
	gint idx = 0;
	int surface_type = 0;
	int pipeline_type = 0;

	factory_name = GST_OBJECT_NAME (factory);
	klass = gst_element_factory_get_metadata (factory, GST_ELEMENT_METADATA_KLASS);
	caps_str = gst_caps_to_string(caps);

	LOGD("found new element [%s] to link", factory_name);

	/* store type string */
	if (player->type == NULL)
	{
		player->type = gst_caps_to_string(caps);
		__mmplayer_update_content_type_info(player);
	}

	/* To support evasimagesink, omx is excluded temporarily*/
	mm_attrs_get_int_by_name(player->attrs, "pipeline_type", &pipeline_type);
	if (pipeline_type == MM_PLAYER_PIPELINE_LEGACY)
		mm_attrs_get_int_by_name(player->attrs,
				"display_surface_type", &surface_type);
	else
		mm_attrs_get_int_by_name(player->attrs,
				"display_surface_client_type", &surface_type);
	LOGD("check display surface type attribute: %d", surface_type);
	if (surface_type == MM_DISPLAY_SURFACE_EVAS && strstr(factory_name, "omx"))
	{
		LOGW("skipping [%s] for supporting evasimagesink temporarily.\n", factory_name);
		result = GST_AUTOPLUG_SELECT_SKIP;
		goto DONE;
	}

	/* filtering exclude keyword */
	for ( idx = 0; player->ini.exclude_element_keyword[idx][0] != '\0'; idx++ )
	{
		if ( strstr(factory_name, player->ini.exclude_element_keyword[idx] ) )
		{
			LOGW("skipping [%s] by exculde keyword [%s]\n",
			factory_name, player->ini.exclude_element_keyword[idx] );

			// NOTE : does we need to check n_value against the number of item selected?
			result = GST_AUTOPLUG_SELECT_SKIP;
			goto DONE;
		}
	}

	/* check factory class for filtering */
	/* NOTE : msl don't need to use image plugins.
	 * So, those plugins should be skipped for error handling.
	 */
	if (g_strrstr(klass, "Codec/Decoder/Image"))
	{
		LOGD("skipping [%s] by not required\n", factory_name);
		result = GST_AUTOPLUG_SELECT_SKIP;
		goto DONE;
	}

	if ((MMPLAYER_IS_MS_BUFF_SRC(player)) &&
		(g_strrstr(klass, "Codec/Demuxer") || (g_strrstr(klass, "Codec/Parser"))))
	{
		// TO CHECK : subtitle if needed, add subparse exception.
		LOGD("skipping parser/demuxer [%s] in es player by not required\n", factory_name);
		result = GST_AUTOPLUG_SELECT_SKIP;
		goto DONE;
	}

	if (g_strrstr(factory_name, "mpegpsdemux"))
	{
		LOGD("skipping PS container - not support\n");
		result = GST_AUTOPLUG_SELECT_SKIP;
		goto DONE;
	}

	if (g_strrstr(factory_name, "mssdemux"))
		player->smooth_streaming = TRUE;

	/* check ALP Codec can be used or not */
	if ((g_strrstr(klass, "Codec/Decoder/Audio")))
	{
		GstStructure* str = NULL;
		gint channels = 0;

		str = gst_caps_get_structure( caps, 0 );
		if ( str )
		{
			gst_structure_get_int (str, "channels", &channels);

			LOGD ("check audio ch : %d %d\n", player->max_audio_channels, channels);
			if (player->max_audio_channels < channels)
			{
				player->max_audio_channels = channels;
			}
		}

		if (!player->audiodec_linked)
		{
			/* set stream information */
			__mmplayer_set_audio_attrs (player, caps);
		}
	}
	else if ((g_strrstr(klass, "Codec/Decoder/Video")))
	{
		if (g_strrstr(factory_name, "omx"))
		{
			char *env = getenv ("MM_PLAYER_HW_CODEC_DISABLE");
			if (env != NULL)
			{
				if (strncasecmp(env, "yes", 3) == 0)
				{
					LOGD ("skipping [%s] by disabled\n", factory_name);
					result = GST_AUTOPLUG_SELECT_SKIP;
					goto DONE;
				}
			}

			/* prepare resource manager for video decoder */
			if((_mmplayer_resource_manager_prepare(&player->resource_manager, RESOURCE_TYPE_VIDEO_DECODER)))
			{
				LOGW ("could not prepare for video_decoder resource, skip it.");
				result = GST_AUTOPLUG_SELECT_SKIP;
				goto DONE;
			}
		}
	}

	if ((g_strrstr(klass, "Codec/Parser/Converter/Video")) ||
		(g_strrstr(klass, "Codec/Decoder/Video")))
	{
		gint stype = 0;
		gint width = 0;
		GstStructure *str = NULL;
		mm_attrs_get_int_by_name (player->attrs, "display_surface_type", &stype);

		/* don't make video because of not required */
		if (stype == MM_DISPLAY_SURFACE_NULL)
		{
			if (player->set_mode.media_packet_video_stream == FALSE
				|| !(player->profile.uri_type == MM_PLAYER_URI_TYPE_MS_BUFF))
			{
				LOGD ("no video because it's not required. -> return expose");
				result = GST_AUTOPLUG_SELECT_EXPOSE;
				goto DONE;
			}
		}

		/* get w/h for omx state-tune */
		str = gst_caps_get_structure (caps, 0);
		gst_structure_get_int (str, "width", &width);

		if (width != 0) {
			if (player->v_stream_caps) {
				gst_caps_unref(player->v_stream_caps);
				player->v_stream_caps = NULL;
			}

			player->v_stream_caps = gst_caps_copy(caps);
			LOGD ("take caps for video state tune");
			MMPLAYER_LOG_GST_CAPS_TYPE(player->v_stream_caps);
		}
	}

	if (g_strrstr(klass, "Decoder"))
	{
		const char* mime = NULL;
		mime = gst_structure_get_name (gst_caps_get_structure(caps, 0));

		if (g_str_has_prefix(mime, "video"))
		{
			// __mmplayer_check_video_zero_cpoy(player, factory);

			player->not_supported_codec &= MISSING_PLUGIN_AUDIO;
			player->can_support_codec |= FOUND_PLUGIN_VIDEO;

			player->videodec_linked = 1;
		}
		else if(g_str_has_prefix(mime, "audio"))
		{
			player->not_supported_codec &= MISSING_PLUGIN_VIDEO;
			player->can_support_codec |= FOUND_PLUGIN_AUDIO;

			player->audiodec_linked = 1;
		}
	}

DONE:
	MMPLAYER_FREEIF(caps_str);

	return result;
}


#if 0
static GValueArray*
__mmplayer_gst_decode_autoplug_factories(GstElement *bin,  GstPad* pad,
GstCaps * caps,  gpointer data)
{
   	//mm_player_t* player = (mm_player_t*)data;

	LOGD("decodebin is requesting factories for caps [%s] from element[%s]",
		gst_caps_to_string(caps),
		GST_ELEMENT_NAME(GST_PAD_PARENT(pad)));

	return NULL;
}
#endif

static void
__mmplayer_gst_decode_pad_removed(GstElement *elem,  GstPad* new_pad,
gpointer data) // @
{
   	//mm_player_t* player = (mm_player_t*)data;
	GstCaps* caps = NULL;

	LOGD("[Decodebin2] pad-removed signal\n");

	caps = gst_pad_query_caps(new_pad, NULL);
	if (caps)
	{
		gchar* caps_str = NULL;
		caps_str = gst_caps_to_string(caps);

		LOGD("pad removed caps : %s from %s", caps_str, GST_ELEMENT_NAME(elem) );

		MMPLAYER_FREEIF(caps_str);
	}
}

static void
__mmplayer_gst_decode_drained(GstElement *bin, gpointer data)
{
	mm_player_t* player = (mm_player_t*)data;

	MMPLAYER_FENTER();
	MMPLAYER_RETURN_IF_FAIL ( player );

	LOGD("__mmplayer_gst_decode_drained");

	if (player->use_deinterleave == TRUE)
	{
		LOGD("group playing mode.");
		return;
	}

	if (!g_mutex_trylock(&player->cmd_lock))
	{
		LOGW("Fail to get cmd lock");
		return;
	}

	if (!__mmplayer_verify_next_play_path(player))
	{
		LOGD("decoding is finished.");
		player->gapless.running = FALSE;
		player->gapless.start_time = 0;
		g_mutex_unlock(&player->cmd_lock);
		return;
	}

	player->gapless.reconfigure = TRUE;

	/* deactivate pipeline except sinkbins to set up the new pipeline of next uri*/
	__mmplayer_deactivate_old_path(player);
	g_mutex_unlock(&player->cmd_lock);

	MMPLAYER_FLEAVE();
}

static void
__mmplayer_gst_element_added (GstElement *bin, GstElement *element, gpointer data)
{
	mm_player_t* player = (mm_player_t*)data;
	const gchar* klass = NULL;
	gchar* factory_name = NULL;

	klass = gst_element_factory_get_metadata (gst_element_get_factory(element), GST_ELEMENT_METADATA_KLASS);
	factory_name = GST_OBJECT_NAME (gst_element_get_factory(element));

	LOGD("new elem klass: %s, factory_name: %s, new elem name : %s\n", klass, factory_name, GST_ELEMENT_NAME(element));

	if (__mmplayer_add_dump_buffer_probe(player, element))
		LOGD("add buffer probe");

	//<-
	if (g_strrstr(klass, "Codec/Decoder/Audio"))
	{
		gchar* selected = NULL;
		selected = g_strdup( GST_ELEMENT_NAME(element));
		player->audio_decoders = g_list_append (player->audio_decoders, selected);
	}
	//-> temp code

	if (g_strrstr(klass, "Parser"))
	{
		gchar* selected = NULL;

		selected = g_strdup (factory_name);
		player->parsers = g_list_append (player->parsers, selected);
	}

	if ((g_strrstr(klass, "Demux") || g_strrstr(klass, "Parse")) && !(g_strrstr(klass, "Adaptive")))
	{
		/* FIXIT : first value will be overwritten if there's more
		 * than 1 demuxer/parser
		 */

		//LOGD ("plugged element is demuxer. take it\n");
		player->pipeline->mainbin[MMPLAYER_M_DEMUX].id = MMPLAYER_M_DEMUX;
		player->pipeline->mainbin[MMPLAYER_M_DEMUX].gst = element;

		/*Added for multi audio support */ // Q. del?
		if (g_strrstr(klass, "Demux"))
		{
			player->pipeline->mainbin[MMPLAYER_M_DEMUX_EX].id = MMPLAYER_M_DEMUX_EX;
			player->pipeline->mainbin[MMPLAYER_M_DEMUX_EX].gst = element;
		}
	}

	if (g_strrstr(factory_name, "asfdemux") || g_strrstr(factory_name, "qtdemux") || g_strrstr(factory_name, "avidemux"))
	{
		int surface_type = 0;

		mm_attrs_get_int_by_name (player->attrs, "display_surface_type", &surface_type);

#if 0	// this is for 0.10 plugin with downstream modification
		/* playback protection if drm file */
		if (player->use_video_stream || surface_type == MM_DISPLAY_SURFACE_EVAS || surface_type == MM_DISPLAY_SURFACE_X_EXT)
		{
			LOGD("playback can be protected if playready drm");
			g_object_set (G_OBJECT(element), "playback-protection", TRUE, NULL);
		}
#endif
	}

	// to support trust-zone only
	if (g_strrstr(factory_name, "asfdemux"))
	{
		LOGD ("set file-location %s\n", player->profile.uri);
		g_object_set (G_OBJECT(element), "file-location", player->profile.uri, NULL);

		if (player->video_hub_download_mode == TRUE)
		{
			g_object_set (G_OBJECT(element), "downloading-mode", player->video_hub_download_mode, NULL);
		}
	}
	else if (g_strrstr(factory_name, "legacyh264parse"))
	{
		LOGD ("[%s] output-format to legacyh264parse\n", "mssdemux");
		g_object_set (G_OBJECT(element), "output-format", 1, NULL); /* NALU/Byte Stream format */
	}
	else if (g_strrstr(factory_name, "mpegaudioparse"))
	{
		if ((MMPLAYER_IS_HTTP_STREAMING(player)) &&
			(__mmplayer_is_only_mp3_type(player->type)))
		{
			LOGD ("[mpegaudioparse] set streaming pull mode.");
			g_object_set(G_OBJECT(element), "http-pull-mp3dec", TRUE, NULL);
		}
	}
	else if (g_strrstr(factory_name, "omx"))
	{
		if (g_strrstr(klass, "Codec/Decoder/Video"))
		{
			gboolean ret = FALSE;

			if (player->v_stream_caps != NULL)
			{
				GstPad *pad = gst_element_get_static_pad(element, "sink");

				if (pad)
				{
					ret = gst_pad_set_caps(pad, player->v_stream_caps);
					LOGD("found omx decoder, setting gst_pad_set_caps for omx (ret:%d)", ret);
					MMPLAYER_LOG_GST_CAPS_TYPE(player->v_stream_caps);
					gst_object_unref (pad);
				}
			}
			g_object_set (G_OBJECT(element), "state-tuning", TRUE, NULL);
		}
#ifdef _MM_PLAYER_ALP_PARSER
		if (g_strrstr(factory_name, "omx_mp3dec"))
		{
			g_list_foreach (player->parsers, check_name, player);
		}
#endif
		player->pipeline->mainbin[MMPLAYER_M_DEC1].gst = element;
	}

	if ((player->pipeline->mainbin[MMPLAYER_M_DEMUX].gst) &&
		(g_strrstr(GST_ELEMENT_NAME(element), "multiqueue")))
	{
		LOGD ("plugged element is multiqueue. take it\n");

		player->pipeline->mainbin[MMPLAYER_M_DEMUXED_S_BUFFER].id = MMPLAYER_M_DEMUXED_S_BUFFER;
		player->pipeline->mainbin[MMPLAYER_M_DEMUXED_S_BUFFER].gst = element;

		if ((MMPLAYER_IS_HTTP_STREAMING(player)) ||
			(MMPLAYER_IS_HTTP_LIVE_STREAMING(player)))
		{
			/* in case of multiqueue, max bytes size is defined with fixed value in mm_player_streaming.h*/
			__mm_player_streaming_set_multiqueue(player->streamer,
				element,
				TRUE,
				player->ini.http_buffering_time,
				1.0,
				player->ini.http_buffering_limit);

			__mm_player_streaming_sync_property(player->streamer, player->pipeline->mainbin[MMPLAYER_M_AUTOPLUG].gst);
		}
	}

	return;
}

static gboolean __mmplayer_configure_audio_callback(mm_player_t* player)
{
	MMPLAYER_FENTER();
	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );

	if ( MMPLAYER_IS_STREAMING(player) )
		return FALSE;

	/* This callback can be set to music player only. */
	if((player->can_support_codec & 0x02) == FOUND_PLUGIN_VIDEO)
	{
		LOGW("audio callback is not supported for video");
		return FALSE;
	}

	if (player->audio_stream_cb)
	{
		{
			GstPad *pad = NULL;

			pad = gst_element_get_static_pad (player->pipeline->audiobin[MMPLAYER_A_SINK].gst, "sink");

			if ( !pad )
			{
				LOGE("failed to get sink pad from audiosink to probe data\n");
				return FALSE;
			}
			player->audio_cb_probe_id = gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
				__mmplayer_audio_stream_probe, player, NULL);

			gst_object_unref (pad);

			pad = NULL;
    	       }
	}
	else
	{
		LOGE("There is no audio callback to configure.\n");
		return FALSE;
	}

	MMPLAYER_FLEAVE();

	return TRUE;
}

static void
__mmplayer_init_factories(mm_player_t* player) // @
{
	MMPLAYER_RETURN_IF_FAIL ( player );

	player->factories = gst_registry_feature_filter(gst_registry_get(),
                                        (GstPluginFeatureFilter)__mmplayer_feature_filter, FALSE, NULL);
	player->factories = g_list_sort(player->factories, (GCompareFunc)util_factory_rank_compare);
}

static void
__mmplayer_release_factories(mm_player_t* player) // @
{
	MMPLAYER_FENTER();
	MMPLAYER_RETURN_IF_FAIL ( player );

	if (player->factories)
	{
		gst_plugin_feature_list_free (player->factories);
		player->factories = NULL;
	}

	MMPLAYER_FLEAVE();
}

static void
__mmplayer_release_misc(mm_player_t* player)
{
	int i;
	gboolean cur_mode = player->set_mode.rich_audio;
	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL ( player );

	player->use_video_stream = FALSE;
	player->video_stream_cb = NULL;
	player->video_stream_cb_user_param = NULL;

	player->audio_stream_cb = NULL;
	player->audio_stream_render_cb_ex = NULL;
	player->audio_stream_cb_user_param = NULL;
	player->audio_stream_sink_sync = false;

	player->video_stream_changed_cb = NULL;
	player->video_stream_changed_cb_user_param = NULL;

	player->audio_stream_changed_cb = NULL;
	player->audio_stream_changed_cb_user_param = NULL;

	player->sent_bos = FALSE;
	player->playback_rate = DEFAULT_PLAYBACK_RATE;

	player->doing_seek = FALSE;

	player->updated_bitrate_count = 0;
	player->total_bitrate = 0;
	player->updated_maximum_bitrate_count = 0;
	player->total_maximum_bitrate = 0;

	player->not_found_demuxer = 0;

	player->last_position = 0;
	player->duration = 0;
	player->http_content_size = 0;
	player->not_supported_codec = MISSING_PLUGIN_NONE;
	player->can_support_codec = FOUND_PLUGIN_NONE;
	player->pending_seek.is_pending = FALSE;
	player->pending_seek.format = MM_PLAYER_POS_FORMAT_TIME;
	player->pending_seek.pos = 0;
	player->msg_posted = FALSE;
	player->has_many_types = FALSE;
	player->is_drm_file = FALSE;
	player->max_audio_channels = 0;
	player->video_share_api_delta = 0;
	player->video_share_clock_delta = 0;
	player->sound_focus.keep_last_pos = FALSE;
	player->sound_focus.acquired = FALSE;
	player->is_subtitle_force_drop = FALSE;
	player->play_subtitle = FALSE;
	player->use_textoverlay = FALSE;
	player->adjust_subtitle_pos = 0;
	player->last_multiwin_status = FALSE;
	player->has_closed_caption = FALSE;
	player->set_mode.media_packet_video_stream = FALSE;
	player->profile.uri_type = MM_PLAYER_URI_TYPE_NONE;
	memset(&player->set_mode, 0, sizeof(MMPlayerSetMode));
	/* recover mode */
	player->set_mode.rich_audio = cur_mode;

	for (i = 0; i < MM_PLAYER_STREAM_COUNT_MAX; i++)
	{
		player->bitrate[i] = 0;
		player->maximum_bitrate[i] = 0;
	}

	/* remove media stream cb (appsrc cb) */
	for (i = 0; i < MM_PLAYER_STREAM_TYPE_MAX; i++)
	{
		player->media_stream_buffer_status_cb[i] = NULL;
		player->media_stream_seek_data_cb[i] = NULL;
	}

	/* free memory related to audio effect */
	MMPLAYER_FREEIF(player->audio_effect_info.custom_ext_level_for_plugin);

	if (player->state_tune_caps)
	{
		gst_caps_unref(player->state_tune_caps);
		player->state_tune_caps = NULL;
	}

	if (player->video_cb_probe_id)
	{
		GstPad *pad = NULL;

		pad = gst_element_get_static_pad (player->video_fakesink, "sink");

		if (pad) {
			LOGD("release video probe\n");

			/* release audio callback */
			gst_pad_remove_probe (pad, player->video_cb_probe_id);
			player->video_cb_probe_id = 0;
			player->video_stream_cb = NULL;
			player->video_stream_cb_user_param = NULL;
		}
	}

	MMPLAYER_FLEAVE();
}

static void
__mmplayer_release_misc_post(mm_player_t* player)
{
	char *original_uri = NULL;
	MMPLAYER_FENTER();

	/* player->pipeline is already released before. */

	MMPLAYER_RETURN_IF_FAIL ( player );

	mm_attrs_set_int_by_name(player->attrs, "content_video_found", 0);
	mm_attrs_set_int_by_name(player->attrs, "content_audio_found", 0);

	/* clean found parsers */
	if (player->parsers)
	{
		GList *parsers = player->parsers;
		for ( ;parsers ; parsers = g_list_next(parsers))
		{
			gchar *name = parsers->data;
			MMPLAYER_FREEIF(name);
		}
		g_list_free(player->parsers);
		player->parsers = NULL;
	}

	/* clean found audio decoders */
	if (player->audio_decoders)
	{
		GList *a_dec = player->audio_decoders;
		for ( ;a_dec ; a_dec = g_list_next(a_dec))
		{
			gchar *name = a_dec->data;
			MMPLAYER_FREEIF(name);
		}
		g_list_free(player->audio_decoders);
		player->audio_decoders = NULL;
	}

	/* clean the uri list except original uri */
	if (player->uri_info.uri_list)
	{
		original_uri = g_list_nth_data(player->uri_info.uri_list, 0);

		if (player->attrs)
		{
			mm_attrs_set_string_by_name(player->attrs, "profile_uri", original_uri);
			LOGD("restore original uri = %s\n", original_uri);

			if (mmf_attrs_commit(player->attrs))
			{
				LOGE("failed to commit the original uri.\n");
			}
		}

		GList *uri_list = player->uri_info.uri_list;
		for ( ;uri_list ; uri_list = g_list_next(uri_list))
		{
			gchar *uri = uri_list->data;
			MMPLAYER_FREEIF(uri);
		}
		g_list_free(player->uri_info.uri_list);
		player->uri_info.uri_list = NULL;
	}

	player->uri_info.uri_idx = 0;
	MMPLAYER_FLEAVE();
}

static GstElement *__mmplayer_element_create_and_link(mm_player_t *player, GstPad* pad, const char* name)
{
	GstElement *element = NULL;
	GstPad *sinkpad;

	LOGD("creating %s to plug\n", name);

	element = gst_element_factory_make(name, NULL);
	if ( ! element )
	{
		LOGE("failed to create queue\n");
		return NULL;
	}

	if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state(element, GST_STATE_READY) )
	{
		LOGE("failed to set state READY to %s\n", name);
		gst_object_unref (element);
		return NULL;
	}

	if ( ! gst_bin_add(GST_BIN(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst), element))
	{
		LOGE("failed to add %s\n", name);
		gst_object_unref (element);
		return NULL;
	}

	sinkpad = gst_element_get_static_pad(element, "sink");

	if ( GST_PAD_LINK_OK != gst_pad_link(pad, sinkpad) )
	{
		LOGE("failed to link %s\n", name);
		gst_object_unref (sinkpad);
		gst_object_unref (element);
		return NULL;
	}

	LOGD("linked %s to pipeline successfully\n", name);

	gst_object_unref (sinkpad);

	return element;
}

static gboolean
__mmplayer_close_link(mm_player_t* player, GstPad *srcpad, GstElement *sinkelement,
const char *padname, const GList *templlist)
{
	GstPad *pad = NULL;
	gboolean has_dynamic_pads = FALSE;
	gboolean has_many_types = FALSE;
	const char *klass = NULL;
	GstStaticPadTemplate *padtemplate = NULL;
	GstElementFactory *factory = NULL;
	GstElement* queue = NULL;
	GstElement* parser = NULL;
	GstPad *pssrcpad = NULL;
	GstPad *qsrcpad = NULL, *qsinkpad = NULL;
	MMPlayerGstElement *mainbin = NULL;
	GstStructure* str = NULL;
	GstCaps* srccaps = NULL;
	GstState target_state = GST_STATE_READY;
	gboolean isvideo_decoder = FALSE;
	guint q_max_size_time = 0;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player &&
		player->pipeline &&
		player->pipeline->mainbin,
		FALSE );

	mainbin = player->pipeline->mainbin;

	LOGD("plugging pad %s:%s to newly create %s:%s\n",
			GST_ELEMENT_NAME( GST_PAD_PARENT ( srcpad ) ),
	                GST_PAD_NAME( srcpad ),
	                GST_ELEMENT_NAME( sinkelement ),
	                padname);

	factory = gst_element_get_factory(sinkelement);
	klass = gst_element_factory_get_metadata (factory, GST_ELEMENT_METADATA_KLASS);

	/* check if player can do start continually */
	MMPLAYER_CHECK_CMD_IF_EXIT(player);

	/* need it to warm up omx before linking to pipeline */
	if (g_strrstr(GST_ELEMENT_NAME( GST_PAD_PARENT ( srcpad ) ), "demux"))
	{
		LOGD("get demux caps.\n");
		if (player->state_tune_caps)
		{
			gst_caps_unref(player->state_tune_caps);
			player->state_tune_caps = NULL;
		}
		player->state_tune_caps = gst_caps_copy(gst_pad_get_current_caps(srcpad));
	}

	/* NOTE : OMX Codec can check if resource is available or not at this state. */
	if (g_strrstr(GST_ELEMENT_NAME(sinkelement), "omx"))
	{
		if (player->state_tune_caps != NULL)
		{
			LOGD("set demux's caps to omx codec if resource is available");
			if (gst_pad_set_caps(gst_element_get_static_pad(sinkelement, "sink"), player->state_tune_caps))
			{
				target_state = GST_STATE_PAUSED;
				isvideo_decoder = TRUE;
				g_object_set(G_OBJECT(sinkelement), "state-tuning", TRUE, NULL);
			}
			else
			{
				LOGW("failed to set caps for state tuning");
			}
		}
		gst_caps_unref(player->state_tune_caps);
		player->state_tune_caps = NULL;
	}

	if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state(sinkelement, target_state) )
	{
		LOGE("failed to set %d state to %s\n", target_state, GST_ELEMENT_NAME( sinkelement ));
		if (isvideo_decoder)
		{
			gst_element_set_state(sinkelement, GST_STATE_NULL);
			gst_object_unref(G_OBJECT(sinkelement));
			player->keep_detecting_vcodec = TRUE;
		}
		goto ERROR;
	}

	/* add to pipeline */
	if ( ! gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), sinkelement) )
	{
		LOGE("failed to add %s to mainbin\n", GST_ELEMENT_NAME( sinkelement ));
		goto ERROR;
	}

	LOGD("element klass : %s\n", klass);

	/* added to support multi track files */
	/* only decoder case and any of the video/audio still need to link*/
	if(g_strrstr(klass, "Decoder") && __mmplayer_link_decoder(player,srcpad))
	{
		gchar *name = g_strdup(GST_ELEMENT_NAME( GST_PAD_PARENT ( srcpad )));

		if (g_strrstr(name, "mpegtsdemux")|| g_strrstr(name, "mssdemux"))
		{
			gchar *src_demux_caps_str = NULL;
			gchar *needed_parser = NULL;
			GstCaps *src_demux_caps = NULL;
			gboolean smooth_streaming = FALSE;

			src_demux_caps = gst_pad_query_caps(srcpad, NULL);
			src_demux_caps_str = gst_caps_to_string(src_demux_caps);

			gst_caps_unref(src_demux_caps);

			if (g_strrstr(src_demux_caps_str, "video/x-h264"))
			{
				if (g_strrstr(name, "mssdemux"))
				{
					needed_parser = g_strdup("legacyh264parse");
					smooth_streaming = TRUE;
				}
				else
				{
					needed_parser = g_strdup("h264parse");
				}
			}
			else if (g_strrstr(src_demux_caps_str, "video/mpeg"))
			{
				needed_parser = g_strdup("mpeg4videoparse");
			}
			MMPLAYER_FREEIF(src_demux_caps_str);

			if (needed_parser)
			{
				parser = __mmplayer_element_create_and_link(player, srcpad, needed_parser);
				MMPLAYER_FREEIF(needed_parser);

				if ( !parser )
				{
					LOGE("failed to create parser\n");
				}
				else
				{
					if (smooth_streaming)
					{
						g_object_set (parser, "output-format", 1, NULL); /* NALU/Byte Stream format */
					}

					/* update srcpad if parser is created */
					pssrcpad = gst_element_get_static_pad(parser, "src");
					srcpad = pssrcpad;
				}
			}
		}
		MMPLAYER_FREEIF(name);

		queue = __mmplayer_element_create_and_link(player, srcpad, "queue"); // parser - queue or demuxer - queue
		if ( ! queue )
		{
			LOGE("failed to create queue\n");
			goto ERROR;
		}

		/* update srcpad to link with decoder */
		qsrcpad = gst_element_get_static_pad(queue, "src");
		srcpad = qsrcpad;

		q_max_size_time = GST_QUEUE_DEFAULT_TIME;

		/* assigning queue handle for futher manipulation purpose */
		/* FIXIT : make it some kind of list so that msl can support more then two stream (text, data, etc...) */
		if(mainbin[MMPLAYER_M_Q1].gst == NULL)
		{
			mainbin[MMPLAYER_M_Q1].id = MMPLAYER_M_Q1;
			mainbin[MMPLAYER_M_Q1].gst = queue;

			if (player->profile.uri_type == MM_PLAYER_URI_TYPE_SS)
			{
				g_object_set (G_OBJECT (mainbin[MMPLAYER_M_Q1].gst), "max-size-time", 0 , NULL);
				g_object_set (G_OBJECT (mainbin[MMPLAYER_M_Q1].gst), "max-size-buffers", 2, NULL);
				g_object_set (G_OBJECT (mainbin[MMPLAYER_M_Q1].gst), "max-size-bytes", 0, NULL);
			}
			else
			{
				if (!MMPLAYER_IS_RTSP_STREAMING(player))
					g_object_set (G_OBJECT (mainbin[MMPLAYER_M_Q1].gst), "max-size-time", q_max_size_time * GST_SECOND, NULL);
			}
		}
		else if(mainbin[MMPLAYER_M_Q2].gst == NULL)
		{
			mainbin[MMPLAYER_M_Q2].id = MMPLAYER_M_Q2;
			mainbin[MMPLAYER_M_Q2].gst = queue;

			if (player->profile.uri_type == MM_PLAYER_URI_TYPE_SS)
			{
				g_object_set (G_OBJECT (mainbin[MMPLAYER_M_Q2].gst), "max-size-time", 0 , NULL);
				g_object_set (G_OBJECT (mainbin[MMPLAYER_M_Q2].gst), "max-size-buffers", 2, NULL);
				g_object_set (G_OBJECT (mainbin[MMPLAYER_M_Q2].gst), "max-size-bytes", 0, NULL);
			}
			else
			{
				if (!MMPLAYER_IS_RTSP_STREAMING(player))
					g_object_set (G_OBJECT (mainbin[MMPLAYER_M_Q2].gst), "max-size-time", q_max_size_time * GST_SECOND, NULL);
			}
		}
		else
		{
			LOGE("Not supporting more then two elementary stream\n");
			g_assert(1);
		}

		pad = gst_element_get_static_pad(sinkelement, padname);

		if ( ! pad )
		{
			LOGW("failed to get pad(%s) from %s. retrying with [sink]\n",
				padname, GST_ELEMENT_NAME(sinkelement) );

			pad = gst_element_get_static_pad(sinkelement, "sink");
 			if ( ! pad )
			{
				LOGE("failed to get pad(sink) from %s. \n",
				GST_ELEMENT_NAME(sinkelement) );
				goto ERROR;
			}
		}

		/*  to check the video/audio type set the proper flag*/
		const gchar *mime_type = NULL;
		{
			srccaps = gst_pad_query_caps(srcpad, NULL);
			if ( !srccaps )
				goto ERROR;

			str = gst_caps_get_structure( srccaps, 0 );
			if ( ! str )
				goto ERROR;

			mime_type = gst_structure_get_name(str);
			if ( ! mime_type )
				goto ERROR;
		}

		/* link queue and decoder. so, it will be queue - decoder. */
		if ( GST_PAD_LINK_OK != gst_pad_link(srcpad, pad) )
		{
			gst_object_unref(GST_OBJECT(pad));
			LOGE("failed to link (%s) to pad(%s)\n", GST_ELEMENT_NAME( sinkelement ), padname );

			/* reconstitute supportable codec */
			if (strstr(mime_type, "video"))
			{
				player->can_support_codec ^= FOUND_PLUGIN_VIDEO;
			}
			else if (strstr(mime_type, "audio"))
			{
				player->can_support_codec ^= FOUND_PLUGIN_AUDIO;
			}
			goto ERROR;
		}

		if (strstr(mime_type, "video"))
		{
			player->videodec_linked = 1;
			LOGI("player->videodec_linked set to 1\n");

		}
		else if (strstr(mime_type, "audio"))
		{
			player->audiodec_linked = 1;
			LOGI("player->auddiodec_linked set to 1\n");
		}

		gst_object_unref(GST_OBJECT(pad));
		gst_caps_unref(GST_CAPS(srccaps));
		srccaps = NULL;
	}

	if ( !MMPLAYER_IS_HTTP_PD(player) )
	{
		if( (g_strrstr(klass, "Demux") && !g_strrstr(klass, "Metadata")) || (g_strrstr(klass, "Parser") ) )
		{
			if (MMPLAYER_IS_HTTP_STREAMING(player))
			{
				gint64 dur_bytes = 0L;
				gchar *file_buffering_path = NULL;
				gboolean use_file_buffer = FALSE;

				if ( !mainbin[MMPLAYER_M_MUXED_S_BUFFER].gst)
				{
					LOGD("creating http streaming buffering queue\n");

					queue = gst_element_factory_make("queue2", "queue2");
					if ( ! queue )
					{
						LOGE ( "failed to create buffering queue element\n" );
						goto ERROR;
					}

					if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state(queue, GST_STATE_READY) )
					{
						LOGE("failed to set state READY to buffering queue\n");
						goto ERROR;
					}

					if ( !gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), queue) )
					{
						LOGE("failed to add buffering queue\n");
						goto ERROR;
					}

					qsinkpad = gst_element_get_static_pad(queue, "sink");
					qsrcpad = gst_element_get_static_pad(queue, "src");

					if ( GST_PAD_LINK_OK != gst_pad_link(srcpad, qsinkpad) )
					{
						LOGE("failed to link buffering queue\n");
						goto ERROR;
					}
					srcpad = qsrcpad;


					mainbin[MMPLAYER_M_MUXED_S_BUFFER].id = MMPLAYER_M_MUXED_S_BUFFER;
					mainbin[MMPLAYER_M_MUXED_S_BUFFER].gst = queue;

					if ( !MMPLAYER_IS_HTTP_LIVE_STREAMING(player))
					{
						if ( !gst_element_query_duration(player->pipeline->mainbin[MMPLAYER_M_SRC].gst, GST_FORMAT_BYTES, &dur_bytes))
							LOGE("fail to get duration.\n");

						if (dur_bytes > 0)
						{
							use_file_buffer = MMPLAYER_USE_FILE_FOR_BUFFERING(player);
							file_buffering_path = g_strdup(player->ini.http_file_buffer_path);
						}
						else
						{
							dur_bytes = 0;
						}
					}

					/* NOTE : we cannot get any duration info from ts container in case of streaming */
					if(!g_strrstr(GST_ELEMENT_NAME(sinkelement), "mpegtsdemux"))
					{
						__mm_player_streaming_set_queue2(player->streamer,
							queue,
							TRUE,
							player->ini.http_max_size_bytes,
							player->ini.http_buffering_time,
							1.0,
							player->ini.http_buffering_limit,
							use_file_buffer,
							file_buffering_path,
							(guint64)dur_bytes);
					}

					MMPLAYER_FREEIF(file_buffering_path);
				}
			}
		}
	}
	/* if it is not decoder or */
	/* in decoder case any of the video/audio still need to link*/
	if(!g_strrstr(klass, "Decoder"))
	{

		pad = gst_element_get_static_pad(sinkelement, padname);
		if ( ! pad )
		{
			LOGW("failed to get pad(%s) from %s. retrying with [sink]\n",
					padname, GST_ELEMENT_NAME(sinkelement) );

			pad = gst_element_get_static_pad(sinkelement, "sink");

			if ( ! pad )
			{
				LOGE("failed to get pad(sink) from %s. \n",
					GST_ELEMENT_NAME(sinkelement) );
				goto ERROR;
			}
		}

		if ( GST_PAD_LINK_OK != gst_pad_link(srcpad, pad) )
		{
			gst_object_unref(GST_OBJECT(pad));
			LOGE("failed to link (%s) to pad(%s)\n", GST_ELEMENT_NAME( sinkelement ), padname );
			goto ERROR;
		}

		gst_object_unref(GST_OBJECT(pad));
	}

	for(;templlist != NULL; templlist = templlist->next)
	{
		padtemplate = templlist->data;

		LOGD ("director = [%d], presence = [%d]\n", padtemplate->direction, padtemplate->presence);

		if(	padtemplate->direction != GST_PAD_SRC ||
			padtemplate->presence == GST_PAD_REQUEST	)
			continue;

		switch(padtemplate->presence)
		{
			case GST_PAD_ALWAYS:
			{
				GstPad *srcpad = gst_element_get_static_pad(sinkelement, "src");
				GstCaps *caps = gst_pad_query_caps(srcpad, NULL);

				/* Check whether caps has many types */
				if ( !gst_caps_is_fixed(caps))
				{
					LOGD ("always pad but, caps has many types");
					MMPLAYER_LOG_GST_CAPS_TYPE(caps);
					has_many_types = TRUE;
					break;
				}

				if ( ! __mmplayer_try_to_plug(player, srcpad, caps) )
				{
					gst_object_unref(GST_OBJECT(srcpad));
					gst_caps_unref(GST_CAPS(caps));

					LOGE("failed to plug something after %s\n", GST_ELEMENT_NAME( sinkelement ));
					goto ERROR;
				}

				gst_caps_unref(GST_CAPS(caps));
				gst_object_unref(GST_OBJECT(srcpad));

			}
			break;


			case GST_PAD_SOMETIMES:
				has_dynamic_pads = TRUE;
			break;

			default:
				break;
		}
	}

	/* check if player can do start continually */
	MMPLAYER_CHECK_CMD_IF_EXIT(player);

	if( has_dynamic_pads )
	{
		player->have_dynamic_pad = TRUE;
		MMPLAYER_SIGNAL_CONNECT ( player, sinkelement, MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "pad-added",
			G_CALLBACK(__mmplayer_add_new_pad), player);

		/* for streaming, more then one typefind will used for each elementary stream
		 * so this doesn't mean the whole pipeline completion
		 */
		if ( ! MMPLAYER_IS_RTSP_STREAMING( player ) )
		{
			MMPLAYER_SIGNAL_CONNECT( player, sinkelement, MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "no-more-pads",
				G_CALLBACK(__mmplayer_pipeline_complete), player);
		}
	}

	if (has_many_types)
	{
		GstPad *pad = NULL;

		player->has_many_types = has_many_types;

		pad = gst_element_get_static_pad(sinkelement, "src");
		MMPLAYER_SIGNAL_CONNECT (player, pad, MM_PLAYER_SIGNAL_TYPE_AUTOPLUG, "notify::caps", G_CALLBACK(__mmplayer_add_new_caps), player);
		gst_object_unref (GST_OBJECT(pad));
	}


	/* check if player can do start continually */
	MMPLAYER_CHECK_CMD_IF_EXIT(player);

	if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state(sinkelement, GST_STATE_PAUSED) )
	{
		LOGE("failed to set state PAUSED to %s\n", GST_ELEMENT_NAME( sinkelement ));
		goto ERROR;
	}

	if ( queue )
	{
		if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state (queue, GST_STATE_PAUSED) )
		{
			LOGE("failed to set state PAUSED to queue\n");
			goto ERROR;
		}

		queue = NULL;

		gst_object_unref (GST_OBJECT(qsrcpad));
		qsrcpad = NULL;
	}

	if ( parser )
	{
		if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state (parser, GST_STATE_PAUSED) )
		{
			LOGE("failed to set state PAUSED to queue\n");
			goto ERROR;
		}

		parser = NULL;

		gst_object_unref (GST_OBJECT(pssrcpad));
		pssrcpad = NULL;
	}

	MMPLAYER_FLEAVE();

	return TRUE;

ERROR:

	if ( queue )
	{
		gst_object_unref(GST_OBJECT(qsrcpad));

		/* NOTE : Trying to dispose element queue0, but it is in READY instead of the NULL state.
		 * You need to explicitly set elements to the NULL state before
		 * dropping the final reference, to allow them to clean up.
		 */
		gst_element_set_state(queue, GST_STATE_NULL);
		/* And, it still has a parent "player".
	         * You need to let the parent manage the object instead of unreffing the object directly.
	         */

		gst_bin_remove (GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), queue);
		//gst_object_unref( queue );
	}

	if ( srccaps )
		gst_caps_unref(GST_CAPS(srccaps));

    return FALSE;
}

static gboolean __mmplayer_feature_filter(GstPluginFeature *feature, gpointer data) // @
{
		const gchar *klass;

	/* we only care about element factories */
	if (!GST_IS_ELEMENT_FACTORY(feature))
		return FALSE;

	/* only parsers, demuxers and decoders */
		klass = gst_element_factory_get_metadata (GST_ELEMENT_FACTORY(feature), GST_ELEMENT_METADATA_KLASS);

	if( g_strrstr(klass, "Demux") == NULL &&
			g_strrstr(klass, "Codec/Decoder") == NULL &&
			g_strrstr(klass, "Depayloader") == NULL &&
			g_strrstr(klass, "Parse") == NULL)
	{
		return FALSE;
	}
    return TRUE;
}


static void	__mmplayer_add_new_caps(GstPad* pad, GParamSpec* unused, gpointer data)
{
	mm_player_t* player = (mm_player_t*) data;
	GstCaps *caps = NULL;
	GstStructure *str = NULL;
	const char *name;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL ( pad )
	MMPLAYER_RETURN_IF_FAIL ( unused )
	MMPLAYER_RETURN_IF_FAIL ( data )

	caps = gst_pad_query_caps(pad, NULL);
	if ( !caps )
		return;

	str = gst_caps_get_structure(caps, 0);
	if ( !str )
		return;

	name = gst_structure_get_name(str);
	if ( !name )
		return;
	LOGD("name=%s\n", name);

	if ( ! __mmplayer_try_to_plug(player, pad, caps) )
	{
		LOGE("failed to autoplug for type (%s)\n", name);
		gst_caps_unref(caps);
		return;
	}

	gst_caps_unref(caps);

	__mmplayer_pipeline_complete( NULL, (gpointer)player );

	MMPLAYER_FLEAVE();

	return;
}

static void __mmplayer_set_unlinked_mime_type(mm_player_t* player, GstCaps *caps)
{
	GstStructure *str;
	gint version = 0;
	const char *stream_type;
	gchar *version_field = NULL;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL ( player );
	MMPLAYER_RETURN_IF_FAIL ( caps );

	str = gst_caps_get_structure(caps, 0);
	if ( !str )
		return;

	stream_type = gst_structure_get_name(str);
	if ( !stream_type )
		return;


	/* set unlinked mime type for downloadable codec */
	if (g_str_has_prefix(stream_type, "video/"))
	{
		if (g_str_has_prefix(stream_type, "video/mpeg"))
		{
			gst_structure_get_int (str, MM_PLAYER_MPEG_VNAME, &version);
			version_field = MM_PLAYER_MPEG_VNAME;
		}
		else if (g_str_has_prefix(stream_type, "video/x-wmv"))
		{
			gst_structure_get_int (str, MM_PLAYER_WMV_VNAME, &version);
			version_field = MM_PLAYER_WMV_VNAME;

		}
		else if (g_str_has_prefix(stream_type, "video/x-divx"))
		{
			gst_structure_get_int (str, MM_PLAYER_DIVX_VNAME, &version);
			version_field = MM_PLAYER_DIVX_VNAME;
		}

		if (version)
		{
			player->unlinked_video_mime = g_strdup_printf("%s, %s=%d", stream_type, version_field, version);
		}
		else
		{
			player->unlinked_video_mime = g_strdup_printf("%s", stream_type);
		}
	}
	else if (g_str_has_prefix(stream_type, "audio/"))
	{
		if (g_str_has_prefix(stream_type, "audio/mpeg")) // mp3 or aac
		{
			gst_structure_get_int (str, MM_PLAYER_MPEG_VNAME, &version);
			version_field = MM_PLAYER_MPEG_VNAME;
		}
		else if (g_str_has_prefix(stream_type, "audio/x-wma"))
		{
			gst_structure_get_int (str, MM_PLAYER_WMA_VNAME, &version);
			version_field = MM_PLAYER_WMA_VNAME;
		}

		if (version)
		{
			player->unlinked_audio_mime = g_strdup_printf("%s, %s=%d", stream_type, version_field, version);
		}
		else
		{
			player->unlinked_audio_mime = g_strdup_printf("%s", stream_type);
		}
	}

	MMPLAYER_FLEAVE();
}

static void __mmplayer_add_new_pad(GstElement *element, GstPad *pad, gpointer data)
{
	mm_player_t* player = (mm_player_t*) data;
	GstCaps *caps = NULL;
	GstStructure *str = NULL;
	const char *name;

	MMPLAYER_FENTER();
	MMPLAYER_RETURN_IF_FAIL ( player );
	MMPLAYER_RETURN_IF_FAIL ( pad );

   	GST_OBJECT_LOCK (pad);
	if ((caps = gst_pad_get_current_caps(pad)))
		gst_caps_ref(caps);
	GST_OBJECT_UNLOCK (pad);

	if ( NULL == caps )
	{
		caps = gst_pad_query_caps(pad, NULL);
		if ( !caps ) return;
	}

	MMPLAYER_LOG_GST_CAPS_TYPE(caps);

	str = gst_caps_get_structure(caps, 0);
	if ( !str )
		return;

	name = gst_structure_get_name(str);
	if ( !name )
		return;

	player->num_dynamic_pad++;
	LOGD("stream count inc : %d\n", player->num_dynamic_pad);

	/* Note : If the stream is the subtitle, we try not to play it. Just close the demuxer subtitle pad.
	  *	If want to play it, remove this code.
	  */
	if (g_strrstr(name, "application"))
	{
		if (g_strrstr(name, "x-id3") || g_strrstr(name, "x-apetag"))
		{
			/* If id3/ape tag comes, keep going */
			LOGD("application mime exception : id3/ape tag");
		}
		else
		{
			/* Otherwise, we assume that this stream is subtile. */
			LOGD(" application mime type pad is closed.");
			return;
		}
	}
	else if (g_strrstr(name, "audio"))
	{
		gint samplerate = 0, channels = 0;

		if (player->audiodec_linked)
		{
			gst_caps_unref(caps);
			LOGD("multi tracks. skip to plug");
			return;
		}

		/* set stream information */
		/* if possible, set it here because the caps is not distrubed by resampler. */
		gst_structure_get_int (str, "rate", &samplerate);
		mm_attrs_set_int_by_name(player->attrs, "content_audio_samplerate", samplerate);

		gst_structure_get_int (str, "channels", &channels);
		mm_attrs_set_int_by_name(player->attrs, "content_audio_channels", channels);

		LOGD("audio samplerate : %d	channels : %d", samplerate, channels);
	}
	else if (g_strrstr(name, "video"))
	{
		gint stype;
		mm_attrs_get_int_by_name (player->attrs, "display_surface_type", &stype);

		/* don't make video because of not required */
		if (stype == MM_DISPLAY_SURFACE_NULL)
		{
			LOGD("no video because it's not required");
			return;
		}

		player->v_stream_caps = gst_caps_copy(caps); //if needed, video caps is required when videobin is created
	}

	if ( ! __mmplayer_try_to_plug(player, pad, caps) )
	{
		LOGE("failed to autoplug for type (%s)", name);

		__mmplayer_set_unlinked_mime_type(player, caps);
	}

	gst_caps_unref(caps);

	MMPLAYER_FLEAVE();
	return;
}

gboolean
__mmplayer_check_subtitle( mm_player_t* player )
{
	MMHandleType attrs = 0;
	char *subtitle_uri = NULL;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL( player, FALSE );

	/* get subtitle attribute */
	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
		return FALSE;

	mm_attrs_get_string_by_name(attrs, "subtitle_uri", &subtitle_uri);
	if ( !subtitle_uri || !strlen(subtitle_uri))
		return FALSE;

	LOGD ("subtite uri is %s[%d]\n", subtitle_uri, strlen(subtitle_uri));
	player->is_external_subtitle_present = TRUE;

	MMPLAYER_FLEAVE();

	return TRUE;
}

static gboolean
__mmplayer_can_extract_pcm( mm_player_t* player )
{
	MMHandleType attrs = 0;
	gboolean is_drm = FALSE;
	gboolean sound_extraction = FALSE;

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		LOGE("fail to get attributes.");
		return FALSE;
	}

	/* check file is drm or not */
	if (g_object_class_find_property(G_OBJECT_GET_CLASS(player->pipeline->mainbin[MMPLAYER_M_SRC].gst), "is-drm"))
		g_object_get(G_OBJECT(player->pipeline->mainbin[MMPLAYER_M_SRC].gst), "is-drm", &is_drm, NULL);

	/* get sound_extraction property */
	mm_attrs_get_int_by_name(attrs, "pcm_extraction", &sound_extraction);

	if ( ! sound_extraction || is_drm )
	{
		LOGD("checking pcm extraction mode : %d, drm : %d", sound_extraction, is_drm);
		return FALSE;
	}

	return TRUE;
}

static gboolean
__mmplayer_handle_streaming_error  ( mm_player_t* player, GstMessage * message )
{
	LOGD("\n");
	MMMessageParamType msg_param;
	gchar *msg_src_element = NULL;
	GstStructure *s = NULL;
	guint error_id = 0;
	gchar *error_string = NULL;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );
	MMPLAYER_RETURN_VAL_IF_FAIL ( message, FALSE );

	s = malloc( sizeof(GstStructure) );
	memcpy ( s, gst_message_get_structure ( message ), sizeof(GstStructure));

	if ( !gst_structure_get_uint (s, "error_id", &error_id) )
		error_id = MMPLAYER_STREAMING_ERROR_NONE;

	switch ( error_id )
	{
		case MMPLAYER_STREAMING_ERROR_UNSUPPORTED_AUDIO:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_UNSUPPORTED_AUDIO;
			break;
		case MMPLAYER_STREAMING_ERROR_UNSUPPORTED_VIDEO:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_UNSUPPORTED_VIDEO;
			break;
		case MMPLAYER_STREAMING_ERROR_CONNECTION_FAIL:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_CONNECTION_FAIL;
			break;
		case MMPLAYER_STREAMING_ERROR_DNS_FAIL:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_DNS_FAIL;
			break;
		case MMPLAYER_STREAMING_ERROR_SERVER_DISCONNECTED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_SERVER_DISCONNECTED;
			break;
		case MMPLAYER_STREAMING_ERROR_BAD_SERVER:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_BAD_SERVER;
			break;
		case MMPLAYER_STREAMING_ERROR_INVALID_PROTOCOL:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_INVALID_PROTOCOL;
			break;
		case MMPLAYER_STREAMING_ERROR_INVALID_URL:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_INVALID_URL;
			break;
		case MMPLAYER_STREAMING_ERROR_UNEXPECTED_MSG:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_UNEXPECTED_MSG;
			break;
		case MMPLAYER_STREAMING_ERROR_OUT_OF_MEMORIES:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_OUT_OF_MEMORIES;
			break;
		case MMPLAYER_STREAMING_ERROR_RTSP_TIMEOUT:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_RTSP_TIMEOUT;
			break;
		case MMPLAYER_STREAMING_ERROR_BAD_REQUEST:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_BAD_REQUEST;
			break;
		case MMPLAYER_STREAMING_ERROR_NOT_AUTHORIZED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_NOT_AUTHORIZED;
			break;
		case MMPLAYER_STREAMING_ERROR_PAYMENT_REQUIRED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_PAYMENT_REQUIRED;
			break;
		case MMPLAYER_STREAMING_ERROR_FORBIDDEN:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_FORBIDDEN;
			break;
		case MMPLAYER_STREAMING_ERROR_CONTENT_NOT_FOUND:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_CONTENT_NOT_FOUND;
			break;
		case MMPLAYER_STREAMING_ERROR_METHOD_NOT_ALLOWED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_METHOD_NOT_ALLOWED;
			break;
		case MMPLAYER_STREAMING_ERROR_NOT_ACCEPTABLE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_NOT_ACCEPTABLE;
			break;
		case MMPLAYER_STREAMING_ERROR_PROXY_AUTHENTICATION_REQUIRED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_PROXY_AUTHENTICATION_REQUIRED;
			break;
		case MMPLAYER_STREAMING_ERROR_SERVER_TIMEOUT:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_SERVER_TIMEOUT;
			break;
		case MMPLAYER_STREAMING_ERROR_GONE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_GONE;
			break;
		case MMPLAYER_STREAMING_ERROR_LENGTH_REQUIRED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_LENGTH_REQUIRED;
			break;
		case MMPLAYER_STREAMING_ERROR_PRECONDITION_FAILED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_PRECONDITION_FAILED;
			break;
		case MMPLAYER_STREAMING_ERROR_REQUEST_ENTITY_TOO_LARGE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_REQUEST_ENTITY_TOO_LARGE;
			break;
		case MMPLAYER_STREAMING_ERROR_REQUEST_URI_TOO_LARGE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_REQUEST_URI_TOO_LARGE;
			break;
		case MMPLAYER_STREAMING_ERROR_UNSUPPORTED_MEDIA_TYPE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_UNSUPPORTED_MEDIA_TYPE;
			break;
		case MMPLAYER_STREAMING_ERROR_PARAMETER_NOT_UNDERSTOOD:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_PARAMETER_NOT_UNDERSTOOD;
			break;
		case MMPLAYER_STREAMING_ERROR_CONFERENCE_NOT_FOUND:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_CONFERENCE_NOT_FOUND;
			break;
		case MMPLAYER_STREAMING_ERROR_NOT_ENOUGH_BANDWIDTH:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_NOT_ENOUGH_BANDWIDTH;
			break;
		case MMPLAYER_STREAMING_ERROR_NO_SESSION_ID:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_NO_SESSION_ID;
			break;
		case MMPLAYER_STREAMING_ERROR_METHOD_NOT_VALID_IN_THIS_STATE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_METHOD_NOT_VALID_IN_THIS_STATE;
			break;
		case MMPLAYER_STREAMING_ERROR_HEADER_FIELD_NOT_VALID_FOR_SOURCE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_HEADER_FIELD_NOT_VALID_FOR_SOURCE;
			break;
		case MMPLAYER_STREAMING_ERROR_INVALID_RANGE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_INVALID_RANGE;
			break;
		case MMPLAYER_STREAMING_ERROR_PARAMETER_IS_READONLY:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_PARAMETER_IS_READONLY;
			break;
		case MMPLAYER_STREAMING_ERROR_AGGREGATE_OP_NOT_ALLOWED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_AGGREGATE_OP_NOT_ALLOWED;
			break;
		case MMPLAYER_STREAMING_ERROR_ONLY_AGGREGATE_OP_ALLOWED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_ONLY_AGGREGATE_OP_ALLOWED;
			break;
		case MMPLAYER_STREAMING_ERROR_BAD_TRANSPORT:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_BAD_TRANSPORT;
			break;
		case MMPLAYER_STREAMING_ERROR_DESTINATION_UNREACHABLE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_DESTINATION_UNREACHABLE;
			break;
		case MMPLAYER_STREAMING_ERROR_INTERNAL_SERVER_ERROR:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_INTERNAL_SERVER_ERROR;
			break;
		case MMPLAYER_STREAMING_ERROR_NOT_IMPLEMENTED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_NOT_IMPLEMENTED;
			break;
		case MMPLAYER_STREAMING_ERROR_BAD_GATEWAY:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_BAD_GATEWAY;
			break;
		case MMPLAYER_STREAMING_ERROR_SERVICE_UNAVAILABLE:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_SERVICE_UNAVAILABLE;
			break;
		case MMPLAYER_STREAMING_ERROR_GATEWAY_TIME_OUT:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_GATEWAY_TIME_OUT;
			break;
		case MMPLAYER_STREAMING_ERROR_RTSP_VERSION_NOT_SUPPORTED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_RTSP_VERSION_NOT_SUPPORTED;
			break;
		case MMPLAYER_STREAMING_ERROR_OPTION_NOT_SUPPORTED:
			msg_param.code = MM_ERROR_PLAYER_STREAMING_OPTION_NOT_SUPPORTED;
			break;
		default:
			{
				MMPLAYER_FREEIF(s);
				return MM_ERROR_PLAYER_STREAMING_FAIL;
			}
	}

	error_string = g_strdup(gst_structure_get_string (s, "error_string"));
	if ( error_string )
		msg_param.data = (void *) error_string;

	if ( message->src )
	{
		msg_src_element = GST_ELEMENT_NAME( GST_ELEMENT_CAST( message->src ) );

		LOGE("-Msg src : [%s] Code : [%x] Error : [%s]  \n",
			msg_src_element, msg_param.code, (char*)msg_param.data );
	}

	/* post error to application */
	if ( ! player->msg_posted )
	{
		MMPLAYER_POST_MSG( player, MM_MESSAGE_ERROR, &msg_param );

		/* don't post more if one was sent already */
		player->msg_posted = TRUE;
	}
	else
	{
		LOGD("skip error post because it's sent already.\n");
	}

	MMPLAYER_FREEIF(s);
	MMPLAYER_FLEAVE();
	g_free(error_string);

	return TRUE;

}

static void
__mmplayer_handle_eos_delay( mm_player_t* player, int delay_in_ms )
{
	MMPLAYER_RETURN_IF_FAIL( player );


	/* post now if delay is zero */
	if ( delay_in_ms == 0 || player->set_mode.pcm_extraction)
	{
		LOGD("eos delay is zero. posting EOS now\n");
		MMPLAYER_POST_MSG( player, MM_MESSAGE_END_OF_STREAM, NULL );

		if ( player->set_mode.pcm_extraction )
			__mmplayer_cancel_eos_timer(player);

		return;
	}

	/* cancel if existing */
	__mmplayer_cancel_eos_timer( player );

	/* init new timeout */
	/* NOTE : consider give high priority to this timer */
	LOGD("posting EOS message after [%d] msec\n", delay_in_ms);

	player->eos_timer = g_timeout_add( delay_in_ms,
		__mmplayer_eos_timer_cb, player );

	player->context.global_default = g_main_context_default ();
	LOGD("global default context = %p, eos timer id = %d", player->context.global_default, player->eos_timer);

	/* check timer is valid. if not, send EOS now */
	if ( player->eos_timer == 0 )
	{
		LOGW("creating timer for delayed EOS has failed. sending EOS now\n");
		MMPLAYER_POST_MSG( player, MM_MESSAGE_END_OF_STREAM, NULL );
	}
}

static void
__mmplayer_cancel_eos_timer( mm_player_t* player )
{
	MMPLAYER_RETURN_IF_FAIL( player );

	if ( player->eos_timer )
	{
		LOGD("cancel eos timer");
		__mmplayer_remove_g_source_from_context(player->context.global_default, player->eos_timer);
		player->eos_timer = 0;
	}

	return;
}

static gboolean
__mmplayer_eos_timer_cb(gpointer u_data)
{
	mm_player_t* player = NULL;
	player = (mm_player_t*) u_data;

	MMPLAYER_RETURN_VAL_IF_FAIL( player, FALSE );

	if ( player->play_count > 1 )
	{
		gint ret_value = 0;
		ret_value = __gst_set_position( player, MM_PLAYER_POS_FORMAT_TIME, 0, TRUE);
		if (ret_value == MM_ERROR_NONE)
		{
			MMHandleType attrs = 0;
			attrs = MMPLAYER_GET_ATTRS(player);

			/* we successeded to rewind. update play count and then wait for next EOS */
			player->play_count--;

			mm_attrs_set_int_by_name(attrs, "profile_play_count", player->play_count);
			mmf_attrs_commit ( attrs );
		}
		else
		{
			LOGE("seeking to 0 failed in repeat play");
		}
	}
	else
	{
		/* posting eos */
		MMPLAYER_POST_MSG( player, MM_MESSAGE_END_OF_STREAM, NULL );
	}

	/* we are returning FALSE as we need only one posting */
	return FALSE;
}

static gboolean
__mmplayer_link_decoder( mm_player_t* player, GstPad *srcpad)
{
	const gchar* name = NULL;
	GstStructure* str = NULL;
	GstCaps* srccaps = NULL;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL( player, FALSE );
	MMPLAYER_RETURN_VAL_IF_FAIL ( srcpad, FALSE );

	/* to check any of the decoder (video/audio) need to be linked  to parser*/
	srccaps = gst_pad_query_caps( srcpad, NULL);
	if ( !srccaps )
		goto ERROR;

	str = gst_caps_get_structure( srccaps, 0 );
	if ( ! str )
		goto ERROR;

	name = gst_structure_get_name(str);
	if ( ! name )
		goto ERROR;

	if (strstr(name, "video"))
	{
		if(player->videodec_linked)
		{
		    LOGI("Video decoder already linked\n");
			return FALSE;
		}
	}
	if (strstr(name, "audio"))
	{
		if(player->audiodec_linked)
		{
		    LOGI("Audio decoder already linked\n");
			return FALSE;
		}
	}

	gst_caps_unref( srccaps );

	MMPLAYER_FLEAVE();

	return TRUE;

ERROR:
	if ( srccaps )
		gst_caps_unref( srccaps );

	return FALSE;
}

static gboolean
__mmplayer_link_sink( mm_player_t* player , GstPad *srcpad)
{
	const gchar* name = NULL;
	GstStructure* str = NULL;
	GstCaps* srccaps = NULL;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );
	MMPLAYER_RETURN_VAL_IF_FAIL ( srcpad, FALSE );

	/* to check any of the decoder (video/audio) need to be linked	to parser*/
	srccaps = gst_pad_query_caps( srcpad, NULL );
	if ( !srccaps )
		goto ERROR;

	str = gst_caps_get_structure( srccaps, 0 );
	if ( ! str )
		goto ERROR;

	name = gst_structure_get_name(str);
	if ( ! name )
		goto ERROR;

	if (strstr(name, "video"))
	{
		if(player->videosink_linked)
		{
			LOGI("Video Sink already linked\n");
			return FALSE;
		}
	}
	if (strstr(name, "audio"))
	{
		if(player->audiosink_linked)
		{
			LOGI("Audio Sink already linked\n");
			return FALSE;
		}
	}
	if (strstr(name, "text"))
	{
		if(player->textsink_linked)
		{
			LOGI("Text Sink already linked\n");
			return FALSE;
		}
	}

	gst_caps_unref( srccaps );

	MMPLAYER_FLEAVE();

	return TRUE;
	//return (!player->videosink_linked || !player->audiosink_linked);

ERROR:
	if ( srccaps )
		gst_caps_unref( srccaps );

	return FALSE;
}


/* sending event to one of sinkelements */
static gboolean
__gst_send_event_to_sink( mm_player_t* player, GstEvent* event )
{
	GstEvent * event2 = NULL;
	GList *sinks = NULL;
	gboolean res = FALSE;
	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL( player, FALSE );
	MMPLAYER_RETURN_VAL_IF_FAIL ( event, FALSE );

	if ( player->play_subtitle && !player->use_textoverlay)
		event2 = gst_event_copy((const GstEvent *)event);

	sinks = player->sink_elements;
	while (sinks)
	{
		GstElement *sink = GST_ELEMENT_CAST (sinks->data);

		if (GST_IS_ELEMENT(sink))
		{
			/* keep ref to the event */
			gst_event_ref (event);

			if ( (res = gst_element_send_event (sink, event)) )
			{
				LOGD("sending event[%s] to sink element [%s] success!\n",
					GST_EVENT_TYPE_NAME(event), GST_ELEMENT_NAME(sink) );

				/* rtsp case, asyn_done is not called after seek during pause state */
				if (MMPLAYER_IS_RTSP_STREAMING(player))
				{
					if (strstr(GST_EVENT_TYPE_NAME(event), "seek"))
					{
						if (MMPLAYER_TARGET_STATE(player) == MM_PLAYER_STATE_PAUSED)
						{
							LOGD("RTSP seek completed, after pause state..\n");
							player->doing_seek = FALSE;
							MMPLAYER_POST_MSG ( player, MM_MESSAGE_SEEK_COMPLETED, NULL );
						}

					}
				}

				if( MMPLAYER_IS_MS_BUFF_SRC(player))
				{
					sinks = g_list_next (sinks);
					continue;
				}
				else
					break;
			}

			LOGD("sending event[%s] to sink element [%s] failed. try with next one.\n",
				GST_EVENT_TYPE_NAME(event), GST_ELEMENT_NAME(sink) );
		}

		sinks = g_list_next (sinks);
	}

#if 0
	if (internal_sub)
	  request pad name = sink0;
	else
	  request pad name = sink1; // external
#endif

	/* Note : Textbin is not linked to the video or audio bin.
	 * It needs to send the event to the text sink seperatelly.
	 */
	 if ( player->play_subtitle && !player->use_textoverlay)
	 {
		GstElement *text_sink = GST_ELEMENT_CAST (player->pipeline->textbin[MMPLAYER_T_FAKE_SINK].gst);

		if (GST_IS_ELEMENT(text_sink))
		{
			/* keep ref to the event */
			gst_event_ref (event2);

			if ((res = gst_element_send_event (text_sink, event2)))
			{
				LOGD("sending event[%s] to subtitle sink element [%s] success!\n",
					GST_EVENT_TYPE_NAME(event2), GST_ELEMENT_NAME(text_sink) );
			}
			else
			{
				LOGE("sending event[%s] to subtitle sink element [%s] failed!\n",
					GST_EVENT_TYPE_NAME(event2), GST_ELEMENT_NAME(text_sink) );
			}

			gst_event_unref (event2);
		}
	 }

	gst_event_unref (event);

	MMPLAYER_FLEAVE();

	return res;
}

static void
__mmplayer_add_sink( mm_player_t* player, GstElement* sink )
{
	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL ( player );
	MMPLAYER_RETURN_IF_FAIL ( sink );

	player->sink_elements =
		g_list_append(player->sink_elements, sink);

	MMPLAYER_FLEAVE();
}

static void
__mmplayer_del_sink( mm_player_t* player, GstElement* sink )
{
	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL ( player );
	MMPLAYER_RETURN_IF_FAIL ( sink );

	player->sink_elements =
			g_list_remove(player->sink_elements, sink);

	MMPLAYER_FLEAVE();
}

static gboolean
__gst_seek(mm_player_t* player, GstElement * element, gdouble rate,
			GstFormat format, GstSeekFlags flags, GstSeekType cur_type,
			gint64 cur, GstSeekType stop_type, gint64 stop )
{
	GstEvent* event = NULL;
	gboolean result = FALSE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL( player, FALSE );

	__mmplayer_drop_subtitle(player, FALSE);

	event = gst_event_new_seek (rate, format, flags, cur_type,
		cur, stop_type, stop);

	result = __gst_send_event_to_sink( player, event );

	MMPLAYER_FLEAVE();

	return result;
}

/* NOTE : be careful with calling this api. please refer to below glib comment
 * glib comment : Note that there is a bug in GObject that makes this function much
 * less useful than it might seem otherwise. Once gobject is disposed, the callback
 * will no longer be called, but, the signal handler is not currently disconnected.
 * If the instance is itself being freed at the same time than this doesn't matter,
 * since the signal will automatically be removed, but if instance persists,
 * then the signal handler will leak. You should not remove the signal yourself
 * because in a future versions of GObject, the handler will automatically be
 * disconnected.
 *
 * It's possible to work around this problem in a way that will continue to work
 * with future versions of GObject by checking that the signal handler is still
 * connected before disconnected it:
 *
 *  if (g_signal_handler_is_connected (instance, id))
 *    g_signal_handler_disconnect (instance, id);
 */
static void
__mmplayer_release_signal_connection(mm_player_t* player, MMPlayerSignalType type)
{
	GList* sig_list = NULL;
	MMPlayerSignalItem* item = NULL;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_IF_FAIL( player );

	LOGD("release signals type : %d", type);

	if ((type < MM_PLAYER_SIGNAL_TYPE_AUTOPLUG) || (type >= MM_PLAYER_SIGNAL_TYPE_ALL))
	{
		__mmplayer_release_signal_connection (player, MM_PLAYER_SIGNAL_TYPE_AUTOPLUG);
		__mmplayer_release_signal_connection (player, MM_PLAYER_SIGNAL_TYPE_VIDEOBIN);
		__mmplayer_release_signal_connection (player, MM_PLAYER_SIGNAL_TYPE_AUDIOBIN);
		__mmplayer_release_signal_connection (player, MM_PLAYER_SIGNAL_TYPE_TEXTBIN);
		__mmplayer_release_signal_connection (player, MM_PLAYER_SIGNAL_TYPE_OTHERS);
		return;
	}

	sig_list = player->signals[type];

	for ( ; sig_list; sig_list = sig_list->next )
	{
		item = sig_list->data;

		if ( item && item->obj && GST_IS_ELEMENT(item->obj) )
		{
			if ( g_signal_handler_is_connected ( item->obj, item->sig ) )
			{
				g_signal_handler_disconnect ( item->obj, item->sig );
			}
		}

		MMPLAYER_FREEIF( item );
	}

	g_list_free ( player->signals[type] );
	player->signals[type] = NULL;

	MMPLAYER_FLEAVE();

	return;
}

int _mmplayer_change_videosink(MMHandleType handle, MMDisplaySurfaceType surface_type, void *display_overlay)
{
	mm_player_t* player = 0;
	int prev_display_surface_type = 0;
	void *prev_display_overlay = NULL;
	const gchar *klass = NULL;
	gchar *cur_videosink_name = NULL;
	int ret = 0;
	int i = 0;
	int num_of_dec = 2; /* DEC1, DEC2 */

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL(handle, MM_ERROR_COMMON_INVALID_ARGUMENT);
	MMPLAYER_RETURN_VAL_IF_FAIL(display_overlay, MM_ERROR_COMMON_INVALID_ARGUMENT);

	player = MM_PLAYER_CAST(handle);

	if (surface_type < MM_DISPLAY_SURFACE_X && surface_type >= MM_DISPLAY_SURFACE_NUM)
	{
		LOGE("Not support this surface type(%d) for changing vidoesink", surface_type);
		MMPLAYER_FLEAVE();
		return MM_ERROR_INVALID_ARGUMENT;
	}

	/* load previous attributes */
	if (player->attrs)
	{
		mm_attrs_get_int_by_name (player->attrs, "display_surface_type", &prev_display_surface_type);
		mm_attrs_get_data_by_name (player->attrs, "display_overlay", &prev_display_overlay);
		LOGD("[0: X surface, 1: EVAS surface] previous surface type(%d), new surface type(%d)", prev_display_surface_type, surface_type);
		if (prev_display_surface_type == surface_type)
		{
			LOGD("incoming display surface type is same as previous one, do nothing..");
			MMPLAYER_FLEAVE();
			return MM_ERROR_NONE;
		}
	}
	else
	{
		LOGE("failed to load attributes");
		MMPLAYER_FLEAVE();
		return MM_ERROR_PLAYER_INTERNAL;
	}

	/* check videosink element is created */
	if (!player->pipeline || !player->pipeline->videobin ||
		!player->pipeline->videobin[MMPLAYER_V_SINK].gst )
	{
		LOGD("videosink element is not yet ready");

		/* videobin is not created yet, so we just set attributes related to display surface */
		LOGD("store display attribute for given surface type(%d)", surface_type);
		mm_attrs_set_int_by_name (player->attrs, "display_surface_type", surface_type);
		mm_attrs_set_data_by_name (player->attrs, "display_overlay", display_overlay, sizeof(display_overlay));
		if ( mmf_attrs_commit ( player->attrs ) )
		{
			LOGE("failed to commit attribute");
			MMPLAYER_FLEAVE();
			return MM_ERROR_PLAYER_INTERNAL;
		}
		MMPLAYER_FLEAVE();
		return MM_ERROR_NONE;
	}
	else
	{
		/* get player command status */
		if ( !(player->cmd == MMPLAYER_COMMAND_START || player->cmd == MMPLAYER_COMMAND_RESUME || player->cmd == MMPLAYER_COMMAND_PAUSE) )
		{
			LOGE("invalid player command status(%d), __mmplayer_do_change_videosink() is only available with START/RESUME/PAUSE command",player->cmd);
			MMPLAYER_FLEAVE();
			return MM_ERROR_PLAYER_INVALID_STATE;
		}

		/* get a current videosink name */
		cur_videosink_name = GST_ELEMENT_NAME(player->pipeline->videobin[MMPLAYER_V_SINK].gst);

		/* surface change */
		for ( i = 0 ; i < num_of_dec ; i++)
		{
			if ( player->pipeline->mainbin &&
				player->pipeline->mainbin[MMPLAYER_M_DEC1+i].gst )
			{
				GstElementFactory *decfactory;
				decfactory = gst_element_get_factory (player->pipeline->mainbin[MMPLAYER_M_DEC1+i].gst);

				klass = gst_element_factory_get_metadata (decfactory, GST_ELEMENT_METADATA_KLASS);
				if ((g_strrstr(klass, "Codec/Decoder/Video")))
				{
					if ( !strncmp(cur_videosink_name, "x", 1) && (surface_type == MM_DISPLAY_SURFACE_EVAS) )
					{
						ret = __mmplayer_do_change_videosink(player, MMPLAYER_M_DEC1+i, player->ini.videosink_element_evas, surface_type, display_overlay);
						if (ret)
						{
							goto ERROR_CASE;
						}
						else
						{
							LOGW("success to changing display surface(%d)",surface_type);
							MMPLAYER_FLEAVE();
							return MM_ERROR_NONE;
						}
					}
					else if (!strncmp(cur_videosink_name, "evas", 4) && (surface_type == MM_DISPLAY_SURFACE_X) )
					{
						ret = __mmplayer_do_change_videosink(player, MMPLAYER_M_DEC1+i, player->ini.videosink_element_x, surface_type, display_overlay);
						if (ret)
						{
							goto ERROR_CASE;
						}
						else
						{
							LOGW("success to changing display surface(%d)",surface_type);
							MMPLAYER_FLEAVE();
							return MM_ERROR_NONE;
						}
					}
					else
					{
						LOGE("invalid incoming surface type(%d) and current videosink_name(%s) for changing display surface",surface_type, cur_videosink_name);
						ret = MM_ERROR_PLAYER_INTERNAL;
						goto ERROR_CASE;
					}
				}
			}
		}
	}

ERROR_CASE:
	/* rollback to previous attributes */
	mm_attrs_set_int_by_name (player->attrs, "display_surface_type", prev_display_surface_type);
	mm_attrs_set_data_by_name(player->attrs, "display_overlay", prev_display_overlay, sizeof(void*));
	if ( mmf_attrs_commit ( player->attrs ) )
	{
		LOGE("failed to commit attributes to rollback");
		MMPLAYER_FLEAVE();
		return MM_ERROR_PLAYER_INTERNAL;
	}
	MMPLAYER_FLEAVE();
	return ret;
}

/* NOTE : It does not support some use cases, eg using colorspace converter */
int
__mmplayer_do_change_videosink(mm_player_t* player, const int dec_index, const char *videosink_element, MMDisplaySurfaceType surface_type, void *display_overlay)
{
	GstPad *src_pad_dec = NULL;
	GstPad *sink_pad_videosink = NULL;
	GstPad *sink_pad_videobin = NULL;
	GstClock *clock = NULL;
	MMPlayerStateType previous_state = MM_PLAYER_STATE_NUM;
	int ret = MM_ERROR_NONE;
	gboolean is_audiobin_created = TRUE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL(player, MM_ERROR_COMMON_INVALID_ARGUMENT);
	MMPLAYER_RETURN_VAL_IF_FAIL(videosink_element, MM_ERROR_COMMON_INVALID_ARGUMENT);
	MMPLAYER_RETURN_VAL_IF_FAIL(display_overlay, MM_ERROR_COMMON_INVALID_ARGUMENT);

	LOGD("video dec is found(idx:%d), we are going to change videosink to %s", dec_index, videosink_element);
	LOGD("surface type(%d), display overlay(%x)", surface_type, display_overlay);

	/* get information whether if audiobin is created */
	if ( !player->pipeline->audiobin ||
		     !player->pipeline->audiobin[MMPLAYER_A_SINK].gst )
	{
		LOGW("audiobin is null, this video content may not have audio data");
		is_audiobin_created = FALSE;
	}

	/* get current state of player */
	previous_state = MMPLAYER_CURRENT_STATE(player);
	LOGD("previous state(%d)", previous_state);


	/* get src pad of decoder and block it */
	src_pad_dec = gst_element_get_static_pad (GST_ELEMENT(player->pipeline->mainbin[dec_index].gst), "src");
	if (!src_pad_dec)
	{
		LOGE("failed to get src pad from decode in mainbin");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	if (!player->doing_seek && previous_state == MM_PLAYER_STATE_PLAYING)
	{
		LOGW("trying to block pad(video)");
//		if (!gst_pad_set_blocked (src_pad_dec, TRUE))
		gst_pad_add_probe(src_pad_dec, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
			NULL, NULL, NULL);

		{
			LOGE("failed to set block pad(video)");
			return MM_ERROR_PLAYER_INTERNAL;
		}
		LOGW("pad is blocked(video)");
	}
	else
	{
		/* no data flows, so no need to do pad_block */
		if (player->doing_seek) {
			LOGW("not completed seek(%d), do nothing", player->doing_seek);
		}
		LOGD("MM_PLAYER_STATE is not PLAYING now, skip pad-block(TRUE)");
	}

	/* remove pad */
	if (!gst_element_remove_pad(player->pipeline->videobin[MMPLAYER_V_BIN].gst,
		GST_PAD_CAST(GST_GHOST_PAD(player->ghost_pad_for_videobin))))
	{
		LOGE("failed to remove previous ghost_pad for videobin");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	/* change state of videobin to NULL */
	LOGD("setting [%s] state to : %d", GST_ELEMENT_NAME(player->pipeline->videobin[MMPLAYER_V_BIN].gst), GST_STATE_NULL);
	ret = gst_element_set_state(player->pipeline->videobin[MMPLAYER_V_BIN].gst, GST_STATE_NULL);
	if (ret != GST_STATE_CHANGE_SUCCESS)
	{
		LOGE("failed to change state of videobin to NULL");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	/* unlink between decoder and videobin and remove previous videosink from videobin */
	GST_ELEMENT_UNLINK(GST_ELEMENT(player->pipeline->mainbin[dec_index].gst),GST_ELEMENT(player->pipeline->videobin[MMPLAYER_V_BIN].gst));
	if ( !gst_bin_remove (GST_BIN(player->pipeline->videobin[MMPLAYER_V_BIN].gst), GST_ELEMENT(player->pipeline->videobin[MMPLAYER_V_SINK].gst)) )
	{
		LOGE("failed to remove former videosink from videobin");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	__mmplayer_del_sink( player, player->pipeline->videobin[MMPLAYER_V_SINK].gst );

	/* create a new videosink and add it to videobin */
	player->pipeline->videobin[MMPLAYER_V_SINK].gst = gst_element_factory_make(videosink_element, videosink_element);
	gst_bin_add (GST_BIN(player->pipeline->videobin[MMPLAYER_V_BIN].gst), GST_ELEMENT(player->pipeline->videobin[MMPLAYER_V_SINK].gst));
	__mmplayer_add_sink( player, player->pipeline->videobin[MMPLAYER_V_SINK].gst );
	g_object_set (G_OBJECT (player->pipeline->videobin[MMPLAYER_V_SINK].gst), "qos", TRUE, NULL);

	/* save attributes */
	if (player->attrs)
	{
		/* set a new display surface type */
		mm_attrs_set_int_by_name (player->attrs, "display_surface_type", surface_type);
		/* set a new diplay overlay */
		switch (surface_type)
		{
			case MM_DISPLAY_SURFACE_X:
				LOGD("save attributes related to display surface to X : xid = %d", *(int*)display_overlay);
				mm_attrs_set_data_by_name (player->attrs, "display_overlay", display_overlay, sizeof(display_overlay));
				break;
			case MM_DISPLAY_SURFACE_EVAS:
				LOGD("save attributes related to display surface to EVAS : evas image object = %x", display_overlay);
				mm_attrs_set_data_by_name (player->attrs, "display_overlay", display_overlay, sizeof(void*));
				break;
			default:
				LOGE("invalid type(%d) for changing display surface",surface_type);
				MMPLAYER_FLEAVE();
				return MM_ERROR_INVALID_ARGUMENT;
		}
		if ( mmf_attrs_commit ( player->attrs ) )
		{
			LOGE("failed to commit");
			MMPLAYER_FLEAVE();
			return MM_ERROR_PLAYER_INTERNAL;
		}
	}
	else
	{
		LOGE("player->attrs is null, failed to save attributes");
		MMPLAYER_FLEAVE();
		return MM_ERROR_PLAYER_INTERNAL;
	}

	/* update video param */
	if ( MM_ERROR_NONE != _mmplayer_update_video_param( player ) )
	{
		LOGE("failed to update video param");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	/* change state of videobin to READY */
	LOGD("setting [%s] state to : %d", GST_ELEMENT_NAME(player->pipeline->videobin[MMPLAYER_V_BIN].gst), GST_STATE_READY);
	ret = gst_element_set_state(player->pipeline->videobin[MMPLAYER_V_BIN].gst, GST_STATE_READY);
	if (ret != GST_STATE_CHANGE_SUCCESS)
	{
		LOGE("failed to change state of videobin to READY");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	/* change ghostpad */
	sink_pad_videosink = gst_element_get_static_pad(GST_ELEMENT(player->pipeline->videobin[MMPLAYER_V_SINK].gst), "sink");
	if ( !sink_pad_videosink )
	{
		LOGE("failed to get sink pad from videosink element");
		return MM_ERROR_PLAYER_INTERNAL;
	}
	player->ghost_pad_for_videobin = gst_ghost_pad_new("sink", sink_pad_videosink);
	if (!gst_pad_set_active(player->ghost_pad_for_videobin, TRUE))
	{
		LOGE("failed to set active to ghost_pad");
		return MM_ERROR_PLAYER_INTERNAL;
	}
	if ( FALSE == gst_element_add_pad(player->pipeline->videobin[MMPLAYER_V_BIN].gst, player->ghost_pad_for_videobin) )
	{
		LOGE("failed to change ghostpad for videobin");
		return MM_ERROR_PLAYER_INTERNAL;
	}
	gst_object_unref(sink_pad_videosink);

	/* link decoder with videobin */
	sink_pad_videobin = gst_element_get_static_pad( GST_ELEMENT(player->pipeline->videobin[MMPLAYER_V_BIN].gst), "sink");
	if ( !sink_pad_videobin )
	{
		LOGE("failed to get sink pad from videobin");
		return MM_ERROR_PLAYER_INTERNAL;
	}
	if ( GST_PAD_LINK_OK != GST_PAD_LINK(src_pad_dec, sink_pad_videobin) )
	{
		LOGE("failed to link");
		return MM_ERROR_PLAYER_INTERNAL;
	}
	gst_object_unref(sink_pad_videobin);

	/* clock setting for a new videosink plugin */
	/* NOTE : Below operation is needed, because a new videosink plugin doesn't have clock for basesink,
			so we set it from audiosink plugin or pipeline(system clock) */
	if (!is_audiobin_created)
	{
		LOGW("audiobin is not created, get clock from pipeline..");
		clock = GST_ELEMENT_CLOCK (player->pipeline->mainbin[MMPLAYER_M_PIPE].gst);
	}
	else
	{
		clock = GST_ELEMENT_CLOCK (player->pipeline->audiobin[MMPLAYER_A_SINK].gst);
	}
	if (clock)
	{
		GstClockTime now;
		GstClockTime base_time;
		LOGD("set the clock to videosink");
		gst_element_set_clock (GST_ELEMENT_CAST(player->pipeline->videobin[MMPLAYER_V_SINK].gst), clock);
		clock = GST_ELEMENT_CLOCK (player->pipeline->videobin[MMPLAYER_V_SINK].gst);
		if (clock)
		{
			LOGD("got clock of videosink");
			now = gst_clock_get_time ( clock );
			base_time = GST_ELEMENT_CAST (player->pipeline->videobin[MMPLAYER_V_SINK].gst)->base_time;
			LOGD ("at time %" GST_TIME_FORMAT ", base %"
					GST_TIME_FORMAT, GST_TIME_ARGS (now), GST_TIME_ARGS (base_time));
		}
		else
		{
			LOGE("failed to get clock of videosink after setting clock");
			return MM_ERROR_PLAYER_INTERNAL;
		}
	}
	else
	{
		LOGW("failed to get clock, maybe it is the time before first playing");
	}

	if (!player->doing_seek && previous_state == MM_PLAYER_STATE_PLAYING)
	{
		/* change state of videobin to PAUSED */
		LOGD("setting [%s] state to : %d", GST_ELEMENT_NAME(player->pipeline->videobin[MMPLAYER_V_BIN].gst), GST_STATE_PLAYING);
		ret = gst_element_set_state(player->pipeline->videobin[MMPLAYER_V_BIN].gst, GST_STATE_PLAYING);
		if (ret != GST_STATE_CHANGE_FAILURE)
		{
			LOGW("change state of videobin to PLAYING, ret(%d)", ret);
		}
		else
		{
			LOGE("failed to change state of videobin to PLAYING");
			return MM_ERROR_PLAYER_INTERNAL;
		}

		/* release blocked and unref src pad of video decoder */
		#if 0
		if (!gst_pad_set_blocked (src_pad_dec, FALSE))
		{
			LOGE("failed to set pad blocked FALSE(video)");
			return MM_ERROR_PLAYER_INTERNAL;
		}
		#endif
		LOGW("pad is unblocked(video)");
	}
	else
	{
		if (player->doing_seek) {
			LOGW("not completed seek(%d)", player->doing_seek);
		}
		/* change state of videobin to PAUSED */
		LOGD("setting [%s] state to : %d", GST_ELEMENT_NAME(player->pipeline->videobin[MMPLAYER_V_BIN].gst), GST_STATE_PAUSED);
		ret = gst_element_set_state(player->pipeline->videobin[MMPLAYER_V_BIN].gst, GST_STATE_PAUSED);
		if (ret != GST_STATE_CHANGE_FAILURE)
		{
			LOGW("change state of videobin to PAUSED, ret(%d)", ret);
		}
		else
		{
			LOGE("failed to change state of videobin to PLAYING");
			return MM_ERROR_PLAYER_INTERNAL;
		}

		/* already skipped pad block */
		LOGD("previous MM_PLAYER_STATE is not PLAYING, skip pad-block(FALSE)");
	}

	/* do get/set position for new videosink plugin */
	{
		unsigned long position = 0;
		gint64 pos_msec = 0;

		LOGD("do get/set position for new videosink plugin");
		if (__gst_get_position(player, MM_PLAYER_POS_FORMAT_TIME, &position ))
		{
			LOGE("failed to get position");
			return MM_ERROR_PLAYER_INTERNAL;
		}
#ifdef SINKCHANGE_WITH_ACCURATE_SEEK
		/* accurate seek */
		if (__gst_set_position(player, MM_PLAYER_POS_FORMAT_TIME, position, TRUE ))
		{
			LOGE("failed to set position");
			return MM_ERROR_PLAYER_INTERNAL;
		}
#else
		/* key unit seek */
		pos_msec = position * G_GINT64_CONSTANT(1000000);
		ret = __gst_seek ( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, 1.0,
				GST_FORMAT_TIME, ( GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT ),
							GST_SEEK_TYPE_SET, pos_msec,
							GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE );
		if ( !ret  )
		{
			LOGE("failed to set position");
			return MM_ERROR_PLAYER_INTERNAL;
		}
#endif
	}

	if (src_pad_dec)
	{
		gst_object_unref (src_pad_dec);
	}
	LOGD("success to change sink");

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}


/* Note : if silent is true, then subtitle would not be displayed. :*/
int _mmplayer_set_subtitle_silent (MMHandleType hplayer, int silent)
{
	mm_player_t* player = (mm_player_t*) hplayer;

	MMPLAYER_FENTER();

	/* check player handle */
	MMPLAYER_RETURN_VAL_IF_FAIL(player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	player->set_mode.subtitle_off = silent;

	LOGD("subtitle is %s.\n", player->set_mode.subtitle_off ? "ON" : "OFF");

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

int _mmplayer_remove_audio_parser_decoder(mm_player_t* player,GstPad *inpad)
{
	int result = MM_ERROR_NONE;
	GstPad *peer = NULL,*pad = NULL;
	GstElement *Element = NULL;
	MMPlayerGstElement* mainbin = NULL;
	mainbin = player->pipeline->mainbin;

	#if 0
	if(!gst_pad_set_blocked(inpad,TRUE))
	{
		result = MM_ERROR_PLAYER_INTERNAL;
		goto EXIT;
	}
	#endif
	gst_pad_add_probe(inpad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
			NULL, NULL, NULL);

	/*Getting pad connected to demuxer audio pad */
	peer = gst_pad_get_peer(inpad);
	/* Disconnecting Demuxer and its peer plugin [audio] */
	if(peer)
	{
		if(!gst_pad_unlink(inpad,peer))
		{
			result = MM_ERROR_PLAYER_INTERNAL;
			goto EXIT;
		}
	}
	else
	{
		result = MM_ERROR_PLAYER_INTERNAL;
		goto EXIT;
	}
	/*Removing elements between Demuxer and audiobin*/
	while(peer != NULL)
	{
		gchar *Element_name = NULL;
		gchar *factory_name = NULL;
		GList *elements = NULL;
		GstElementFactory *factory = NULL;
		/*Getting peer element*/
		Element = gst_pad_get_parent_element(peer);
		if(Element == NULL)
		{
			gst_object_unref(peer);
			result = MM_ERROR_PLAYER_INTERNAL;
			break;
		}

		Element_name = gst_element_get_name(Element);
		factory = gst_element_get_factory(Element);
		/*checking the element is audio bin*/
		if(!strcmp(Element_name,"audiobin"))
		{
			gst_object_unref(peer);
			result = MM_ERROR_NONE;
            g_free(Element_name);
			break;
		}
		factory_name = GST_OBJECT_NAME(factory);
		pad = gst_element_get_static_pad(Element,"src");
		if(pad == NULL)
		{
			result = MM_ERROR_PLAYER_INTERNAL;
            g_free(Element_name);
			break;
		}
		gst_object_unref(peer);
		peer = gst_pad_get_peer(pad);
		if(peer)
		{
			if(!gst_pad_unlink(pad,peer))
			{
				gst_object_unref(peer);
				gst_object_unref(pad);
				result = MM_ERROR_PLAYER_INTERNAL;
                g_free(Element_name);
				break;
			}
		}
		elements = player->parsers;
		/* Removing the element form the list*/
		for ( ; elements; elements = g_list_next(elements))
		{
			Element_name = elements->data;
			if(g_strrstr(Element_name,factory_name))
			{
				player->parsers = g_list_remove(player->parsers,elements->data);
			}
		}
		gst_element_set_state(Element,GST_STATE_NULL);
		gst_bin_remove(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst),Element);
		gst_object_unref(pad);
		if(Element == mainbin[MMPLAYER_M_Q1].gst)
		{
			mainbin[MMPLAYER_M_Q1].gst = NULL;
		}
		else if(Element == mainbin[MMPLAYER_M_Q2].gst)
		{
			mainbin[MMPLAYER_M_Q2].gst = NULL;
		}
		else if(Element == mainbin[MMPLAYER_M_DEC1].gst)
		{
			mainbin[MMPLAYER_M_DEC1].gst = NULL;
		}
		else if(Element == mainbin[MMPLAYER_M_DEC2].gst)
		{
			mainbin[MMPLAYER_M_DEC2].gst = NULL;
		}
		gst_object_unref(Element);
	}
EXIT:
	return result;
}

int _mmplayer_sync_subtitle_pipeline(mm_player_t* player)
{
	MMPlayerGstElement* mainbin = NULL;
	MMPlayerGstElement* textbin = NULL;
	GstStateChangeReturn ret = GST_STATE_CHANGE_FAILURE;
	GstState current_state = GST_STATE_VOID_PENDING;
	GstState element_state = GST_STATE_VOID_PENDING;
	GstState element_pending_state = GST_STATE_VOID_PENDING;
	gint64 time = 0;
	GstEvent *event = NULL;
	int result = MM_ERROR_NONE;

	GstClock *curr_clock = NULL;
	GstClockTime base_time, start_time, curr_time;


	MMPLAYER_FENTER();

	/* check player handle */
	MMPLAYER_RETURN_VAL_IF_FAIL ( player && player->pipeline , MM_ERROR_PLAYER_NOT_INITIALIZED);

	if (!(player->pipeline->mainbin) || !(player->pipeline->textbin))
	{
		LOGE("Pipeline is not in proper state\n");
		result = MM_ERROR_PLAYER_NOT_INITIALIZED;
		goto EXIT;
	}

	mainbin = player->pipeline->mainbin;
	textbin = player->pipeline->textbin;

	current_state = GST_STATE (mainbin[MMPLAYER_M_PIPE].gst);

	// sync clock with current pipeline
	curr_clock = GST_ELEMENT_CLOCK (player->pipeline->mainbin[MMPLAYER_M_PIPE].gst);
	curr_time = gst_clock_get_time (curr_clock);

	base_time = gst_element_get_base_time (GST_ELEMENT_CAST (player->pipeline->mainbin[MMPLAYER_M_PIPE].gst));
	start_time = gst_element_get_start_time (GST_ELEMENT_CAST(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst));

	LOGD ("base_time=%" GST_TIME_FORMAT " start_time=%" GST_TIME_FORMAT " curr_time=%" GST_TIME_FORMAT,
		GST_TIME_ARGS (base_time), GST_TIME_ARGS (start_time), GST_TIME_ARGS (curr_time));

	if (current_state > GST_STATE_READY)
	{
		// sync state with current pipeline
		gst_element_set_state(textbin[MMPLAYER_T_BIN].gst, GST_STATE_PAUSED);
		gst_element_set_state(mainbin[MMPLAYER_M_SUBPARSE].gst, GST_STATE_PAUSED);
		gst_element_set_state(mainbin[MMPLAYER_M_SUBSRC].gst, GST_STATE_PAUSED);

		ret = gst_element_get_state (mainbin[MMPLAYER_M_SUBSRC].gst, &element_state, &element_pending_state, 5 * GST_SECOND);
		if ( GST_STATE_CHANGE_FAILURE == ret )
		{
			LOGE("fail to state change.\n");
		}
	}

	gst_element_set_base_time (textbin[MMPLAYER_T_BIN].gst, base_time);
	gst_element_set_start_time(textbin[MMPLAYER_T_BIN].gst, start_time);

	if (curr_clock)
	{
		gst_element_set_clock (textbin[MMPLAYER_T_BIN].gst, curr_clock);
		gst_object_unref (curr_clock);
	}

	// seek to current position
	if (!gst_element_query_position (mainbin[MMPLAYER_M_PIPE].gst, GST_FORMAT_TIME, &time))
	{
		result = MM_ERROR_PLAYER_INVALID_STATE;
		LOGE("gst_element_query_position failed, invalid state\n");
		goto EXIT;
	}

	LOGD("seek time = %lld\n", time);
	event = gst_event_new_seek (1.0, GST_FORMAT_TIME, (GstSeekFlags)(GST_SEEK_FLAG_FLUSH), GST_SEEK_TYPE_SET, time, GST_SEEK_TYPE_NONE, -1);
	if (event)
	{
		__gst_send_event_to_sink(player, event);
	}
	else
	{
		result = MM_ERROR_PLAYER_INTERNAL;
		LOGE("gst_event_new_seek failed\n");
		goto EXIT;
	}

	// sync state with current pipeline
	gst_element_sync_state_with_parent(textbin[MMPLAYER_T_BIN].gst);
	gst_element_sync_state_with_parent(mainbin[MMPLAYER_M_SUBPARSE].gst);
	gst_element_sync_state_with_parent(mainbin[MMPLAYER_M_SUBSRC].gst);

EXIT:
	return result;
}

static int
__mmplayer_change_external_subtitle_language(mm_player_t* player, const char* filepath)
{
	GstStateChangeReturn ret = GST_STATE_CHANGE_FAILURE;
	GstState current_state = GST_STATE_VOID_PENDING;

	MMHandleType attrs = 0;
	MMPlayerGstElement* mainbin = NULL;
	MMPlayerGstElement* textbin = NULL;

	gchar* subtitle_uri = NULL;
	int result = MM_ERROR_NONE;
	const gchar *charset = NULL;

	MMPLAYER_FENTER();

	/* check player handle */
	MMPLAYER_RETURN_VAL_IF_FAIL( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	MMPLAYER_RETURN_VAL_IF_FAIL( filepath, MM_ERROR_COMMON_INVALID_ARGUMENT );

	if (!(player->pipeline) || !(player->pipeline->mainbin))
	{
		result = MM_ERROR_PLAYER_INVALID_STATE;
		LOGE("Pipeline is not in proper state\n");
		goto EXIT;
	}

	mainbin = player->pipeline->mainbin;
	textbin = player->pipeline->textbin;

	current_state = GST_STATE (mainbin[MMPLAYER_M_PIPE].gst);
	if (current_state < GST_STATE_READY)
	{
		result = MM_ERROR_PLAYER_INVALID_STATE;
		LOGE("Pipeline is not in proper state\n");
		goto EXIT;
	}

	attrs = MMPLAYER_GET_ATTRS(player);
	if (!attrs)
	{
		LOGE("cannot get content attribute\n");
		result = MM_ERROR_PLAYER_INTERNAL;
		goto EXIT;
	}

	mm_attrs_get_string_by_name (attrs, "subtitle_uri", &subtitle_uri);
	if (!subtitle_uri || strlen(subtitle_uri) < 1)
	{
		LOGE("subtitle uri is not proper filepath\n");
		result = MM_ERROR_PLAYER_INVALID_URI;
		goto EXIT;
	}

	LOGD("old subtitle file path is [%s]\n", subtitle_uri);
	LOGD("new subtitle file path is [%s]\n", filepath);

	if (!strcmp (filepath, subtitle_uri))
	{
		LOGD("No need to swtich subtitle, as input filepath is same as current filepath\n");
		goto EXIT;
	}
	else
	{
		mm_attrs_set_string_by_name(player->attrs, "subtitle_uri", filepath);
		if (mmf_attrs_commit(player->attrs))
		{
			LOGE("failed to commit.\n");
			goto EXIT;
		}
	}

	//gst_pad_set_blocked_async(src-srcpad, TRUE)

	ret = gst_element_set_state(textbin[MMPLAYER_T_BIN].gst, GST_STATE_READY);
	if (ret != GST_STATE_CHANGE_SUCCESS)
	{
		LOGE("failed to change state of textbin to READY");
		result = MM_ERROR_PLAYER_INTERNAL;
		goto EXIT;
	}

	ret = gst_element_set_state(mainbin[MMPLAYER_M_SUBPARSE].gst, GST_STATE_READY);
	if (ret != GST_STATE_CHANGE_SUCCESS)
	{
		LOGE("failed to change state of subparse to READY");
		result = MM_ERROR_PLAYER_INTERNAL;
		goto EXIT;
	}

	ret = gst_element_set_state(mainbin[MMPLAYER_M_SUBSRC].gst, GST_STATE_READY);
	if (ret != GST_STATE_CHANGE_SUCCESS)
	{
		LOGE("failed to change state of filesrc to READY");
		result = MM_ERROR_PLAYER_INTERNAL;
		goto EXIT;
	}

	g_object_set(G_OBJECT(mainbin[MMPLAYER_M_SUBSRC].gst), "location", filepath, NULL);

	charset = util_get_charset(filepath);
	if (charset)
	{
		LOGD ("detected charset is %s\n", charset );
		g_object_set (G_OBJECT (mainbin[MMPLAYER_M_SUBPARSE].gst), "subtitle-encoding", charset, NULL);
	}

	result = _mmplayer_sync_subtitle_pipeline(player);

EXIT:
	MMPLAYER_FLEAVE();
	return result;
}

/* API to switch between external subtitles */
int _mmplayer_set_external_subtitle_path(MMHandleType hplayer, const char* filepath)
{
	int result = MM_ERROR_NONE;
	mm_player_t* player = (mm_player_t*)hplayer;

	MMPLAYER_FENTER();

	/* check player handle */
	MMPLAYER_RETURN_VAL_IF_FAIL( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	if (!player->pipeline)	// IDLE state
	{
		mm_attrs_set_string_by_name(player->attrs, "subtitle_uri", filepath);
		if (mmf_attrs_commit(player->attrs))
		{
			LOGE("failed to commit.\n");
			result= MM_ERROR_PLAYER_INTERNAL;
		}
	}
	else	// curr state <> IDLE (READY, PAUSE, PLAYING..)
	{
		if ( filepath == NULL )
			return MM_ERROR_COMMON_INVALID_ARGUMENT;

		if (!__mmplayer_check_subtitle(player))
		{
			mm_attrs_set_string_by_name(player->attrs, "subtitle_uri", filepath);
			if (mmf_attrs_commit(player->attrs))
			{
				LOGE("failed to commit.\n");
				result = MM_ERROR_PLAYER_INTERNAL;
			}

			if ( MM_ERROR_NONE != __mmplayer_gst_create_subtitle_src(player) )
				LOGE("fail to create subtitle src\n");

			result = _mmplayer_sync_subtitle_pipeline(player);
		}
		else
		{
			result = __mmplayer_change_external_subtitle_language(player, filepath);
		}
	}

	MMPLAYER_FLEAVE();
	return result;
}

static int
__mmplayer_change_selector_pad (mm_player_t* player, MMPlayerTrackType type, int index)
{
	int result = MM_ERROR_NONE;
	gchar* change_pad_name = NULL;
	GstPad* sinkpad = NULL;
	MMPlayerGstElement* mainbin = NULL;
	enum MainElementID elemId = MMPLAYER_M_NUM;
	GstCaps* caps = NULL;
	gint total_track_num = 0;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL (player && player->pipeline && player->pipeline->mainbin,
													MM_ERROR_PLAYER_NOT_INITIALIZED);

	LOGD ("Change Track(%d) to %d\n", type, index);

	mainbin = player->pipeline->mainbin;

	if (type == MM_PLAYER_TRACK_TYPE_AUDIO)
	{
		elemId = MMPLAYER_M_A_INPUT_SELECTOR;
	}
	else if (type == MM_PLAYER_TRACK_TYPE_TEXT)
	{
		elemId = MMPLAYER_M_T_INPUT_SELECTOR;
	}
	else
	{
		/* Changing Video Track is not supported. */
		LOGE ("Track Type Error\n");
		goto EXIT;
	}

	if (mainbin[elemId].gst == NULL)
	{
		result = MM_ERROR_PLAYER_NO_OP;
		LOGD ("Req track doesn't exist\n");
		goto EXIT;
	}

	total_track_num = player->selector[type].total_track_num;
	if (total_track_num <= 0)
	{
		result = MM_ERROR_PLAYER_NO_OP;
		LOGD ("Language list is not available \n");
		goto EXIT;
	}

	if ((index < 0) || (index >= total_track_num))
	{
		result = MM_ERROR_INVALID_ARGUMENT;
		LOGD ("Not a proper index : %d \n", index);
		goto EXIT;
	}

	/*To get the new pad from the selector*/
	change_pad_name = g_strdup_printf ("sink_%u", index);
	if (change_pad_name == NULL)
	{
		result = MM_ERROR_PLAYER_INTERNAL;
		LOGD ("Pad does not exists\n");
		goto EXIT;
	}

	LOGD ("new active pad name: %s\n", change_pad_name);

	sinkpad = gst_element_get_static_pad (mainbin[elemId].gst, change_pad_name);
	if (sinkpad == NULL)
	{
		LOGD ("sinkpad is NULL");
		result = MM_ERROR_PLAYER_INTERNAL;
		goto EXIT;
	}

	LOGD ("Set Active Pad - %s:%s\n", GST_DEBUG_PAD_NAME(sinkpad));
	g_object_set (mainbin[elemId].gst, "active-pad", sinkpad, NULL);

	caps = gst_pad_get_current_caps(sinkpad);
	MMPLAYER_LOG_GST_CAPS_TYPE(caps);

	if (sinkpad)
		gst_object_unref (sinkpad);

	if (type == MM_PLAYER_TRACK_TYPE_AUDIO)
	{
		__mmplayer_set_audio_attrs (player, caps);
	}

EXIT:

	MMPLAYER_FREEIF(change_pad_name);
	return result;
}

int _mmplayer_change_track_language (MMHandleType hplayer, MMPlayerTrackType type, int index)
{
	int result = MM_ERROR_NONE;
	mm_player_t* player = NULL;
	MMPlayerGstElement* mainbin = NULL;

	gint current_active_index = 0;

	GstState current_state = GST_STATE_VOID_PENDING;
	GstEvent* event = NULL;
	gint64 time = 0;

	MMPLAYER_FENTER();

	player = (mm_player_t*)hplayer;
	MMPLAYER_RETURN_VAL_IF_FAIL (player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if (!player->pipeline)
	{
		LOGE ("Track %d pre setting -> %d\n", type, index);

		player->selector[type].active_pad_index = index;
		goto EXIT;
	}

	mainbin = player->pipeline->mainbin;

	current_active_index = player->selector[type].active_pad_index;

	/*If index is same as running index no need to change the pad*/
	if (current_active_index == index)
	{
		goto EXIT;
	}

	if (!gst_element_query_position(mainbin[MMPLAYER_M_PIPE].gst, GST_FORMAT_TIME, &time))
	{
		result = MM_ERROR_PLAYER_INVALID_STATE;
		goto EXIT;
	}

	current_state = GST_STATE(mainbin[MMPLAYER_M_PIPE].gst);
	if (current_state < GST_STATE_PAUSED)
	{
		result = MM_ERROR_PLAYER_INVALID_STATE;
		LOGW ("Pipeline not in porper state\n");
		goto EXIT;
	}

	result = __mmplayer_change_selector_pad(player, type, index);
	if (result != MM_ERROR_NONE)
	{
		LOGE ("change selector pad error\n");
		goto EXIT;
	}

	player->selector[type].active_pad_index = index;

	if (current_state == GST_STATE_PLAYING)
	{
		event = gst_event_new_seek (1.0, GST_FORMAT_TIME,(GstSeekFlags) (GST_SEEK_FLAG_SEGMENT | GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SKIP),GST_SEEK_TYPE_SET, time, GST_SEEK_TYPE_NONE, -1);
		if (event)
		{
			__gst_send_event_to_sink (player, event);
		}
		else
		{
			result = MM_ERROR_PLAYER_INTERNAL;
			goto EXIT;
		}
	}

EXIT:
	return result;
}

int _mmplayer_get_subtitle_silent (MMHandleType hplayer, int* silent)
{
	mm_player_t* player = (mm_player_t*) hplayer;

	MMPLAYER_FENTER();

	/* check player handle */
	MMPLAYER_RETURN_VAL_IF_FAIL(player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	*silent = player->set_mode.subtitle_off;

	LOGD("subtitle is %s.\n", silent ? "ON" : "OFF");

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

gboolean
__is_ms_buff_src( mm_player_t* player )
{
	MMPLAYER_RETURN_VAL_IF_FAIL ( player, FALSE );

	return ( player->profile.uri_type == MM_PLAYER_URI_TYPE_MS_BUFF) ? TRUE : FALSE;
}

gboolean
__has_suffix(mm_player_t* player, const gchar* suffix)
{
	MMPLAYER_RETURN_VAL_IF_FAIL( player, FALSE );
	MMPLAYER_RETURN_VAL_IF_FAIL( suffix, FALSE );

	gboolean ret = FALSE;
	gchar* t_url = g_ascii_strdown(player->profile.uri, -1);
	gchar* t_suffix = g_ascii_strdown(suffix, -1);

	if ( g_str_has_suffix(player->profile.uri, suffix) )
	{
		ret = TRUE;
	}

	MMPLAYER_FREEIF(t_url);
	MMPLAYER_FREEIF(t_suffix);

	return ret;
}

int
_mmplayer_set_display_zoom(MMHandleType hplayer, float level, int x, int y)
{
	mm_player_t* player = (mm_player_t*) hplayer;

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	MMPLAYER_VIDEO_SINK_CHECK(player);

	LOGD("setting display zoom level = %f, offset = %d, %d", level, x, y);

	g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst, "zoom", level, "zoom-pos-x", x, "zoom-pos-y", y, NULL);

	return MM_ERROR_NONE;
}
int
_mmplayer_get_display_zoom(MMHandleType hplayer, float *level, int *x, int *y)
{

	mm_player_t* player = (mm_player_t*) hplayer;
	float _level = 0.0;
	int _x = 0;
	int _y = 0;

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	MMPLAYER_VIDEO_SINK_CHECK(player);

	g_object_get(player->pipeline->videobin[MMPLAYER_V_SINK].gst, "zoom", &_level, "zoom-pos-x", &_x, "zoom-pos-y", &_y, NULL);

	LOGD("display zoom level = %f, start off x = %d, y = %d", _level, _x, _y);

	*level = _level;
	*x = _x;
	*y = _y;

	return MM_ERROR_NONE;
}

int
_mmplayer_set_video_hub_download_mode(MMHandleType hplayer, bool mode)
{
	mm_player_t* player = (mm_player_t*) hplayer;

	MMPLAYER_RETURN_VAL_IF_FAIL (player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if (MMPLAYER_CURRENT_STATE(player) != MM_PLAYER_STATE_NULL)
	{
		MMPLAYER_PRINT_STATE(player);
		LOGE("wrong-state : can't set the download mode to parse");
		return MM_ERROR_PLAYER_INVALID_STATE;
	}

	LOGD("set video hub download mode to %s", (mode)?"ON":"OFF");
	player->video_hub_download_mode = mode;

	return MM_ERROR_NONE;
}

int
_mmplayer_enable_sync_handler(MMHandleType hplayer, bool enable)
{
	mm_player_t* player = (mm_player_t*) hplayer;

	MMPLAYER_RETURN_VAL_IF_FAIL (player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	LOGD("enable sync handler : %s", (enable)?"ON":"OFF");
	player->sync_handler = enable;

	return MM_ERROR_NONE;
}

int
_mmplayer_set_video_share_master_clock(	MMHandleType hplayer,
					long long clock,
					long long clock_delta,
					long long video_time,
					long long media_clock,
					long long audio_time)
{
	mm_player_t* player = (mm_player_t*) hplayer;
	MMPlayerGstElement* mainbin = NULL;
	GstClockTime start_time_audio = 0, start_time_video = 0;
	GstClockTimeDiff base_time = 0, new_base_time = 0;
	MMPlayerStateType current_state = MM_PLAYER_STATE_NONE;
	gint64 api_delta = 0;
	gint64 position = 0, position_delta = 0;
	gint64 adj_base_time = 0;
	GstClock *curr_clock = NULL;
	GstClockTime curr_time = 0;
	gboolean query_ret = TRUE;
	int result = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED);
	MMPLAYER_RETURN_VAL_IF_FAIL ( player->pipeline->mainbin, MM_ERROR_PLAYER_NOT_INITIALIZED);
	MMPLAYER_RETURN_VAL_IF_FAIL ( player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, MM_ERROR_PLAYER_NOT_INITIALIZED);

	// LOGD("in(us) : %lld, %lld, %lld, %lld, %lld", clock, clock_delta, video_time, media_clock, audio_time);

	if ((video_time < 0) || (player->doing_seek))
	{
		LOGD("skip setting master clock.  %lld", video_time);
		goto EXIT;
	}

	mainbin = player->pipeline->mainbin;

	curr_clock = gst_pipeline_get_clock (GST_PIPELINE_CAST(mainbin[MMPLAYER_M_PIPE].gst));
	curr_time = gst_clock_get_time (curr_clock);

	current_state = MMPLAYER_CURRENT_STATE(player);

	if ( current_state == MM_PLAYER_STATE_PLAYING )
		query_ret = gst_element_query_position(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_FORMAT_TIME, &position);

	if ( ( current_state != MM_PLAYER_STATE_PLAYING ) ||
		 ( !query_ret ))
	{
		position = player->last_position;
		LOGD ("query fail. %lld", position);
	}

	clock*= GST_USECOND;
	clock_delta *= GST_USECOND;

	api_delta = clock - curr_time;
	if ((player->video_share_api_delta == 0 ) || (player->video_share_api_delta > api_delta))
	{
		player->video_share_api_delta = api_delta;
	}
	else
	{
		clock_delta += (api_delta - player->video_share_api_delta);
	}

	if ((player->video_share_clock_delta == 0 ) || (player->video_share_clock_delta > clock_delta))
	{
		player->video_share_clock_delta = (gint64)clock_delta;

		position_delta = (position/GST_USECOND) - video_time;
		position_delta *= GST_USECOND;

		adj_base_time = position_delta;
		LOGD ("video_share_clock_delta = %lld, adj = %lld", player->video_share_clock_delta, adj_base_time);

	}
	else
	{
		gint64 new_play_time = 0;
		gint64 network_delay =0;

		video_time *= GST_USECOND;

		network_delay = clock_delta - player->video_share_clock_delta;
		new_play_time = video_time + network_delay;

		adj_base_time = position - new_play_time;

		LOGD ("%lld(delay) = %lld - %lld / %lld(adj) = %lld(slave_pos) - %lld(master_pos) - %lld(delay)",
			network_delay, clock_delta, player->video_share_clock_delta, adj_base_time, position, video_time, network_delay);
	}

	/* Adjust Current Stream Time with base_time of sink
	 * 1. Set Start time to CLOCK NONE, to control the base time by MSL
	 * 2. Set new base time
	 *    if adj_base_time is positive value, the stream time will be decreased.
	 * 3. If seek event is occurred, the start time will be reset. */
	if ((player->pipeline->audiobin) &&
		(player->pipeline->audiobin[MMPLAYER_A_SINK].gst))
	{
		start_time_audio = gst_element_get_start_time (player->pipeline->audiobin[MMPLAYER_A_SINK].gst);

		if (start_time_audio != GST_CLOCK_TIME_NONE)
		{
			LOGD ("audio sink : gst_element_set_start_time -> NONE");
			gst_element_set_start_time(player->pipeline->audiobin[MMPLAYER_A_SINK].gst, GST_CLOCK_TIME_NONE);
		}

		base_time = gst_element_get_base_time (player->pipeline->audiobin[MMPLAYER_A_SINK].gst);
	}

	if ((player->pipeline->videobin) &&
		(player->pipeline->videobin[MMPLAYER_V_SINK].gst))
	{
		start_time_video = gst_element_get_start_time (player->pipeline->videobin[MMPLAYER_V_SINK].gst);

		if (start_time_video != GST_CLOCK_TIME_NONE)
		{
			LOGD ("video sink : gst_element_set_start_time -> NONE");
			gst_element_set_start_time(player->pipeline->videobin[MMPLAYER_V_SINK].gst, GST_CLOCK_TIME_NONE);
		}

		// if videobin exist, get base_time from videobin.
		base_time = gst_element_get_base_time (player->pipeline->videobin[MMPLAYER_V_SINK].gst);
	}

	new_base_time = base_time + adj_base_time;

	if ((player->pipeline->audiobin) &&
		(player->pipeline->audiobin[MMPLAYER_A_SINK].gst))
		gst_element_set_base_time(GST_ELEMENT_CAST(player->pipeline->audiobin[MMPLAYER_A_SINK].gst), (GstClockTime)new_base_time);

	if ((player->pipeline->videobin) &&
		(player->pipeline->videobin[MMPLAYER_V_SINK].gst))
		gst_element_set_base_time(GST_ELEMENT_CAST(player->pipeline->videobin[MMPLAYER_V_SINK].gst), (GstClockTime)new_base_time);

EXIT:
	MMPLAYER_FLEAVE();

	return result;
}

int
_mmplayer_get_video_share_master_clock(	MMHandleType hplayer,
					long long *video_time,
					long long *media_clock,
					long long *audio_time)
{
	mm_player_t* player = (mm_player_t*) hplayer;
	MMPlayerGstElement* mainbin = NULL;
	GstClock *curr_clock = NULL;
	MMPlayerStateType current_state = MM_PLAYER_STATE_NONE;
	gint64 position = 0;
	gboolean query_ret = TRUE;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED);
	MMPLAYER_RETURN_VAL_IF_FAIL ( player->pipeline->mainbin, MM_ERROR_PLAYER_NOT_INITIALIZED);
	MMPLAYER_RETURN_VAL_IF_FAIL ( player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_RETURN_VAL_IF_FAIL ( video_time, MM_ERROR_COMMON_INVALID_ARGUMENT );
	MMPLAYER_RETURN_VAL_IF_FAIL ( media_clock, MM_ERROR_COMMON_INVALID_ARGUMENT );
	MMPLAYER_RETURN_VAL_IF_FAIL ( audio_time, MM_ERROR_COMMON_INVALID_ARGUMENT );

	mainbin = player->pipeline->mainbin;

	curr_clock = gst_pipeline_get_clock (GST_PIPELINE_CAST(mainbin[MMPLAYER_M_PIPE].gst));

	current_state = MMPLAYER_CURRENT_STATE(player);

	if ( current_state != MM_PLAYER_STATE_PAUSED )
		query_ret = gst_element_query_position(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_FORMAT_TIME, &position);

	if ( ( current_state == MM_PLAYER_STATE_PAUSED ) ||
		 ( !query_ret ))
	{
		position = player->last_position;
	}

	*media_clock = *video_time = *audio_time = (position/GST_USECOND);

	LOGD("media_clock: %lld, video_time: %lld (us)", *media_clock, *video_time);

	if (curr_clock)
		gst_object_unref (curr_clock);

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

int
_mmplayer_get_video_rotate_angle(MMHandleType hplayer, int *angle)
{
	mm_player_t* player = (mm_player_t*) hplayer;
	int org_angle = 0;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	MMPLAYER_RETURN_VAL_IF_FAIL ( angle, MM_ERROR_COMMON_INVALID_ARGUMENT );

	if (player->v_stream_caps)
	{
		GstStructure *str = NULL;

		str = gst_caps_get_structure (player->v_stream_caps, 0);
		if ( !gst_structure_get_int (str, "orientation", &org_angle))
		{
			LOGD ("missing 'orientation' field in video caps");
		}
	}

	LOGD("orientation: %d", org_angle);
	*angle = org_angle;

	MMPLAYER_FLEAVE();
	return MM_ERROR_NONE;
}

static gboolean
__mmplayer_add_dump_buffer_probe(mm_player_t *player, GstElement *element)
{
	MMPLAYER_RETURN_VAL_IF_FAIL (player, FALSE);
	MMPLAYER_RETURN_VAL_IF_FAIL (element, FALSE);

	gchar *factory_name = GST_OBJECT_NAME (gst_element_get_factory(element));
	gchar dump_file_name[PLAYER_INI_MAX_STRLEN*2];

	int idx = 0;

	for ( idx = 0; player->ini.dump_element_keyword[idx][0] != '\0'; idx++ )
	{
		if (g_strrstr(factory_name, player->ini.dump_element_keyword[idx]))
		{
			LOGD("dump [%s] sink pad", player->ini.dump_element_keyword[idx]);
			mm_player_dump_t *dump_s;
			dump_s = g_malloc (sizeof(mm_player_dump_t));

			if (dump_s == NULL)
			{
				LOGE ("malloc fail");
				return FALSE;
			}

			dump_s->dump_element_file = NULL;
			dump_s->dump_pad = NULL;
			dump_s->dump_pad = gst_element_get_static_pad (element, "sink");

			if (dump_s->dump_pad)
			{
				memset (dump_file_name, 0x00, PLAYER_INI_MAX_STRLEN*2);
				sprintf (dump_file_name, "%s/%s_sink_pad.dump", player->ini.dump_element_path, player->ini.dump_element_keyword[idx]);
				dump_s->dump_element_file = fopen(dump_file_name,"w+");
				dump_s->probe_handle_id = gst_pad_add_probe (dump_s->dump_pad, GST_PAD_PROBE_TYPE_BUFFER, __mmplayer_dump_buffer_probe_cb, dump_s->dump_element_file, NULL);
				/* add list for removed buffer probe and close FILE */
				player->dump_list = g_list_append (player->dump_list, dump_s);
				LOGD ("%s sink pad added buffer probe for dump", factory_name);
				return TRUE;
			}
			else
			{
				g_free(dump_s);
				dump_s = NULL;
				LOGE ("failed to get %s sink pad added", factory_name);
			}


		}
	}
	return FALSE;
}

static GstPadProbeReturn
__mmplayer_dump_buffer_probe_cb(GstPad *pad,  GstPadProbeInfo *info, gpointer u_data)
{
	FILE *dump_data = (FILE *) u_data;
//	int written = 0;
	GstBuffer *buffer = gst_pad_probe_info_get_buffer(info);
	GstMapInfo probe_info = GST_MAP_INFO_INIT;

	MMPLAYER_RETURN_VAL_IF_FAIL ( dump_data, FALSE );

	gst_buffer_map(buffer, &probe_info, GST_MAP_READ);

//	LOGD ("buffer timestamp = %" GST_TIME_FORMAT, GST_TIME_ARGS( GST_BUFFER_TIMESTAMP(buffer)));

	fwrite ( probe_info.data, 1, probe_info.size , dump_data);

	return GST_PAD_PROBE_OK;
}

static void
__mmplayer_release_dump_list (GList *dump_list)
{
	if (dump_list)
	{
		GList *d_list = dump_list;
		for ( ;d_list ; d_list = g_list_next(d_list))
		{
			mm_player_dump_t *dump_s = d_list->data;
			if (dump_s->dump_pad)
			{
				if (dump_s->probe_handle_id)
				{
					gst_pad_remove_probe (dump_s->dump_pad, dump_s->probe_handle_id);
				}

			}
			if (dump_s->dump_element_file)
			{
				fclose(dump_s->dump_element_file);
				dump_s->dump_element_file = NULL;
			}
			MMPLAYER_FREEIF(dump_s);
		}
		g_list_free(dump_list);
		dump_list = NULL;
	}
}

int
_mmplayer_has_closed_caption(MMHandleType hplayer, bool* exist)
{
	mm_player_t* player = (mm_player_t*) hplayer;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	MMPLAYER_RETURN_VAL_IF_FAIL ( exist, MM_ERROR_INVALID_ARGUMENT );

	*exist = player->has_closed_caption;

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

int
_mmplayer_enable_media_packet_video_stream(MMHandleType hplayer, bool enable)
{
	mm_player_t* player = (mm_player_t*) hplayer;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL (player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	MMPLAYER_RETURN_VAL_IF_FAIL (enable == TRUE || enable == FALSE, MM_ERROR_INVALID_ARGUMENT);
	if(enable)
		player->bufmgr = tbm_bufmgr_init(-1);
	else {
		tbm_bufmgr_deinit(player->bufmgr);
		player->bufmgr = NULL;
	}

	player->set_mode.media_packet_video_stream = enable;

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

void * _mm_player_media_packet_video_stream_internal_buffer_ref(void *buffer)
{
	void * ret = NULL
	MMPLAYER_FENTER();
	/* increase ref count of gst buffer */
	if (buffer)
		ret = gst_buffer_ref((GstBuffer *)buffer);

	MMPLAYER_FLEAVE();
	return ret;
}

void _mm_player_media_packet_video_stream_internal_buffer_unref(void *buffer)
{
	MMPLAYER_FENTER();
	if (buffer) {
		gst_buffer_unref((GstBuffer *)buffer);
		buffer = NULL;
	}
	MMPLAYER_FLEAVE();
}

void
__gst_appsrc_feed_audio_data(GstElement *element, guint size, gpointer user_data)
{
	mm_player_t *player  = (mm_player_t*)user_data;
	MMPlayerStreamType type = MM_PLAYER_STREAM_TYPE_AUDIO;

	MMPLAYER_RETURN_IF_FAIL ( player );

	LOGI("app-src: feed audio\n");

	if (player->media_stream_buffer_status_cb[type])
	{
		player->media_stream_buffer_status_cb[type](type, MM_PLAYER_MEDIA_STREAM_BUFFER_UNDERRUN, player->buffer_cb_user_param);
	}
}

void
__gst_appsrc_feed_video_data(GstElement *element, guint size, gpointer user_data)
{
	mm_player_t *player  = (mm_player_t*)user_data;
	MMPlayerStreamType type = MM_PLAYER_STREAM_TYPE_VIDEO;

	MMPLAYER_RETURN_IF_FAIL ( player );

	LOGI("app-src: feed video\n");

	if (player->media_stream_buffer_status_cb[type])
	{
		player->media_stream_buffer_status_cb[type](type, MM_PLAYER_MEDIA_STREAM_BUFFER_UNDERRUN, player->buffer_cb_user_param);
	}
}

void
__gst_appsrc_feed_subtitle_data(GstElement *element, guint size, gpointer user_data)
{
	mm_player_t *player  = (mm_player_t*)user_data;
	MMPlayerStreamType type = MM_PLAYER_STREAM_TYPE_TEXT;

	MMPLAYER_RETURN_IF_FAIL ( player );

	LOGI("app-src: feed subtitle\n");

	if (player->media_stream_buffer_status_cb[type])
	{
		player->media_stream_buffer_status_cb[type](type, MM_PLAYER_MEDIA_STREAM_BUFFER_UNDERRUN, player->buffer_cb_user_param);
	}
}

void
__gst_appsrc_enough_audio_data(GstElement *element, gpointer user_data)
{
	mm_player_t *player  = (mm_player_t*)user_data;
	MMPlayerStreamType type = MM_PLAYER_STREAM_TYPE_AUDIO;

	MMPLAYER_RETURN_IF_FAIL ( player );

	LOGI("app-src: audio buffer is full.\n");

	if (player->media_stream_buffer_status_cb[type])
	{
		player->media_stream_buffer_status_cb[type](type, MM_PLAYER_MEDIA_STREAM_BUFFER_OVERFLOW, player->buffer_cb_user_param);
	}
}

void
__gst_appsrc_enough_video_data(GstElement *element, gpointer user_data)
{
	mm_player_t *player  = (mm_player_t*)user_data;
	MMPlayerStreamType type = MM_PLAYER_STREAM_TYPE_VIDEO;

	MMPLAYER_RETURN_IF_FAIL ( player );

	LOGI("app-src: video buffer is full.\n");

	if (player->media_stream_buffer_status_cb[type])
	{
		player->media_stream_buffer_status_cb[type](type, MM_PLAYER_MEDIA_STREAM_BUFFER_OVERFLOW, player->buffer_cb_user_param);
	}
}

gboolean
__gst_seek_audio_data (GstElement * appsrc, guint64 position, gpointer user_data)
{
	mm_player_t *player  = (mm_player_t*)user_data;
	MMPlayerStreamType type = MM_PLAYER_STREAM_TYPE_AUDIO;

	MMPLAYER_RETURN_VAL_IF_FAIL( player, FALSE );

	LOGD("app-src: seek audio data\n");

	if (player->media_stream_seek_data_cb[type])
	{
		player->media_stream_seek_data_cb[type](type, position, player->buffer_cb_user_param);
	}

	return TRUE;
}

gboolean
__gst_seek_video_data (GstElement * appsrc, guint64 position, gpointer user_data)
{
	mm_player_t *player  = (mm_player_t*)user_data;
	MMPlayerStreamType type = MM_PLAYER_STREAM_TYPE_VIDEO;

	MMPLAYER_RETURN_VAL_IF_FAIL( player, FALSE );

	LOGD("app-src: seek video data\n");

	if (player->media_stream_seek_data_cb[type])
	{
		player->media_stream_seek_data_cb[type](type, position, player->buffer_cb_user_param);
	}

	return TRUE;
}

gboolean
__gst_seek_subtitle_data (GstElement * appsrc, guint64 position, gpointer user_data)
{
	mm_player_t *player  = (mm_player_t*)user_data;
	MMPlayerStreamType type = MM_PLAYER_STREAM_TYPE_TEXT;

	MMPLAYER_RETURN_VAL_IF_FAIL( player, FALSE );

	LOGD("app-src: seek subtitle data\n");

	if (player->media_stream_seek_data_cb[type])
	{
		player->media_stream_seek_data_cb[type](type, position, player->buffer_cb_user_param);
	}

	return TRUE;
}

int
_mmplayer_set_pcm_spec(MMHandleType hplayer, int samplerate, int channel)
{
	mm_player_t* player = (mm_player_t*) hplayer;

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	player->pcm_samplerate = samplerate;
	player->pcm_channel = channel;

	MMPLAYER_FLEAVE();
	return MM_ERROR_NONE;
}

int _mmplayer_get_raw_video_caps(mm_player_t *player, char **caps)
{
	GstCaps *v_caps = NULL;
	GstPad *pad = NULL;
	GstElement *gst;
	gint stype = 0;

	if(!player->videosink_linked) {
		LOGD("No video sink");
		return MM_ERROR_NONE;
	}
	mm_attrs_get_int_by_name (player->attrs, "display_surface_type", &stype);

	if (stype == MM_DISPLAY_SURFACE_NULL) {
		LOGD("Display type is NULL");
		if(!player->video_fakesink) {
			LOGE("No fakesink");
			return MM_ERROR_PLAYER_INVALID_STATE;
		}
		gst = player->video_fakesink;
	}
	else {
		if ( !player->pipeline || !player->pipeline->videobin ||
				!player->pipeline->videobin[MMPLAYER_V_SINK].gst ) {
			LOGE("No video pipeline");
			return MM_ERROR_PLAYER_INVALID_STATE;
		}
		gst = player->pipeline->videobin[MMPLAYER_V_SINK].gst;
	}
	pad = gst_element_get_static_pad(gst, "sink");
	if(!pad) {
		LOGE("static pad is NULL");
		return MM_ERROR_PLAYER_INVALID_STATE;
	}
	v_caps = gst_pad_get_current_caps(pad);
	gst_object_unref( pad );

	if(!v_caps) {
		LOGE("fail to get caps");
		return MM_ERROR_PLAYER_INVALID_STATE;
	}

	*caps = gst_caps_to_string(v_caps);

	gst_caps_unref(v_caps);

	return MM_ERROR_NONE;
}
