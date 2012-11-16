/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, YeJin Cho <cho.yejin@samsung.com>, YoungHwan An <younghwan_.an@samsung.com>
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
|  																							|
========================================================================================== */
#include <glib.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/interfaces/xoverlay.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>

#include <mm_error.h>
#include <mm_attrs.h>
#include <mm_attrs_private.h>
#include <mm_debug.h>

#include "mm_player_priv.h"
#include "mm_player_ini.h"
#include "mm_player_attrs.h"
#include "mm_player_capture.h"

/*===========================================================================================
|																							|
|  LOCAL DEFINITIONS AND DECLARATIONS FOR MODULE											|
|  																							|
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

#define MM_VOLUME_FACTOR_DEFAULT 		1.0
#define MM_VOLUME_FACTOR_MIN				0
#define MM_VOLUME_FACTOR_MAX				1.0

#define MM_PLAYER_FADEOUT_TIME_DEFAULT	700000 // 700 msec

#define MM_PLAYER_MPEG_VNAME				"mpegversion"
#define MM_PLAYER_DIVX_VNAME				"divxversion"
#define MM_PLAYER_WMV_VNAME				"wmvversion"
#define MM_PLAYER_WMA_VNAME				"wmaversion"

#define DEFAULT_PLAYBACK_RATE			1.0

#define GST_QUEUE_DEFAULT_TIME			2
#define GST_QUEUE_HLS_TIME				8

/* video capture callback*/
gulong ahs_appsrc_cb_probe_id = 0;

#define MMPLAYER_USE_FILE_FOR_BUFFERING(player) (((player)->profile.uri_type != MM_PLAYER_URI_TYPE_HLS) && (PLAYER_INI()->http_file_buffer_path) && (strlen(PLAYER_INI()->http_file_buffer_path) > 0) )
#define MMPLAYER_PLAY_SUBTITLE(player) ((player)->play_subtitle)

#define	LAZY_PAUSE_TIMEOUT_MSEC	700	

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
static gboolean __mmplayer_set_state(mm_player_t* player, int state);
static int 		__mmplayer_get_state(mm_player_t* player);
static int 		__mmplayer_gst_create_video_pipeline(mm_player_t* player, GstCaps *caps, MMDisplaySurfaceType surface_type);
static int 		__mmplayer_gst_create_audio_pipeline(mm_player_t* player);
static int 		__mmplayer_gst_create_text_pipeline(mm_player_t* player);
static int 		__mmplayer_gst_create_subtitle_pipeline(mm_player_t* player);
static int 		__mmplayer_gst_create_pipeline(mm_player_t* player);
static int 		__mmplayer_gst_destroy_pipeline(mm_player_t* player);
static int		__mmplayer_gst_element_link_bucket(GList* element_bucket);

static gboolean __mmplayer_gst_callback(GstBus *bus, GstMessage *msg, gpointer data);
static void 	__mmplayer_gst_decode_callback(GstElement *decodebin, GstPad *pad, gboolean last, gpointer data);

static void 	__mmplayer_typefind_have_type(  GstElement *tf, guint probability, GstCaps *caps, gpointer data);
static gboolean __mmplayer_try_to_plug(mm_player_t* player, GstPad *pad, const GstCaps *caps);
static void 	__mmplayer_pipeline_complete(GstElement *decodebin,  gpointer data);
static gboolean __mmplayer_is_midi_type(gchar* str_caps);
static gboolean __mmplayer_is_amr_type (gchar *str_caps);
static gboolean __mmplayer_is_only_mp3_type (gchar *str_caps);

static gboolean	__mmplayer_close_link(mm_player_t* player, GstPad *srcpad, GstElement *sinkelement, const char *padname, const GList *templlist);
static gboolean __mmplayer_feature_filter(GstPluginFeature *feature, gpointer data);
static void 	__mmplayer_add_new_pad(GstElement *element, GstPad *pad, gpointer data);

static void		__mmplayer_gst_rtp_no_more_pads (GstElement *element,  gpointer data);
static void		__mmplayer_gst_rtp_dynamic_pad (GstElement *element, GstPad *pad, gpointer data);
static gboolean	__mmplayer_update_stream_service_type( mm_player_t* player );
static gboolean	__mmplayer_update_subtitle( GstElement* object, GstBuffer *buffer, GstPad *pad, gpointer data);


static void 	__mmplayer_init_factories(mm_player_t* player);
static void 	__mmplayer_release_factories(mm_player_t* player);
static void	__mmplayer_release_misc(mm_player_t* player);
static gboolean	__mmplayer_gstreamer_init(void);

static int		__mmplayer_gst_set_state (mm_player_t* player, GstElement * pipeline,  GstState state, gboolean async, gint timeout );
gboolean __mmplayer_post_message(mm_player_t* player, enum MMMessageType msgtype, MMMessageParamType* param);
static gboolean	__mmplayer_gst_extract_tag_from_msg(mm_player_t* player, GstMessage *msg);
int		__mmplayer_switch_audio_sink (mm_player_t* player);
static gboolean __mmplayer_gst_remove_fakesink(mm_player_t* player, MMPlayerGstElement* fakesink);
static int		__mmplayer_check_state(mm_player_t* player, enum PlayerCommandState command);
static gboolean __mmplayer_audio_stream_probe (GstPad *pad, GstBuffer *buffer, gpointer u_data);

static gboolean __mmplayer_dump_pipeline_state( mm_player_t* player );
static gboolean __mmplayer_check_subtitle( mm_player_t* player );
static gboolean __mmplayer_handle_gst_error ( mm_player_t* player, GstMessage * message, GError* error );
static gboolean __mmplayer_handle_streaming_error  ( mm_player_t* player, GstMessage * message );
static void		__mmplayer_post_delayed_eos( mm_player_t* player, int delay_in_ms );
static void 	__mmplayer_cancel_delayed_eos( mm_player_t* player );
static gboolean	__mmplayer_eos_timer_cb(gpointer u_data);
static gboolean __mmplayer_link_decoder( mm_player_t* player,GstPad *srcpad);
static gboolean __mmplayer_link_sink( mm_player_t* player,GstPad *srcpad);
static int 	__mmplayer_post_missed_plugin(mm_player_t* player);
static int		__mmplayer_check_not_supported_codec(mm_player_t* player, gchar* mime);
static gboolean __mmplayer_configure_audio_callback(mm_player_t* player);
static void 	__mmplayer_add_sink( mm_player_t* player, GstElement* sink);
static void 	__mmplayer_del_sink( mm_player_t* player, GstElement* sink);
static void		__mmplayer_release_signal_connection(mm_player_t* player);
static void __mmplayer_set_antishock( mm_player_t* player, gboolean disable_by_force);
static gpointer __mmplayer_repeat_thread(gpointer data);
int _mmplayer_get_track_count(MMHandleType hplayer,  MMPlayerTrackType track_type, int *count);

static int 		__gst_realize(mm_player_t* player);
static int 		__gst_unrealize(mm_player_t* player);
static int 		__gst_start(mm_player_t* player);
static int 		__gst_stop(mm_player_t* player);
static int 		__gst_pause(mm_player_t* player, gboolean async);
static int 		__gst_resume(mm_player_t* player, gboolean async);
static gboolean	__gst_seek(mm_player_t* player, GstElement * element, gdouble rate,
					GstFormat format, GstSeekFlags flags, GstSeekType cur_type,
					gint64 cur, GstSeekType stop_type, gint64 stop );
static int __gst_pending_seek ( mm_player_t* player );

static int 		__gst_set_position(mm_player_t* player, int format, unsigned long position, gboolean internal_called);
static int 		__gst_get_position(mm_player_t* player, int format, unsigned long *position);
static int 		__gst_get_buffer_position(mm_player_t* player, int format, unsigned long* start_pos, unsigned long* stop_pos);
static int 		__gst_adjust_subtitle_position(mm_player_t* player, int format, int position);
static int 		__gst_set_message_callback(mm_player_t* player, MMMessageCallback callback, gpointer user_param);
static void 	__gst_set_async_state_change(mm_player_t* player, gboolean async);

static gint 	__gst_handle_core_error( mm_player_t* player, int code );
static gint 	__gst_handle_library_error( mm_player_t* player, int code );
static gint 	__gst_handle_resource_error( mm_player_t* player, int code );
static gint 	__gst_handle_stream_error( mm_player_t* player, GError* error, GstMessage * message );
static gint		__gst_transform_gsterror( mm_player_t* player, GstMessage * message, GError* error);
static gboolean __gst_send_event_to_sink( mm_player_t* player, GstEvent* event );

static int __mmplayer_set_pcm_extraction(mm_player_t* player);
static gboolean __mmplayer_can_extract_pcm( mm_player_t* player );

/*fadeout */
static void __mmplayer_do_sound_fadedown(mm_player_t* player, unsigned int time);
static void __mmplayer_undo_sound_fadedown(mm_player_t* player);

static void 	__mmplayer_add_new_caps(GstPad* pad, GParamSpec* unused, gpointer data);
static void __mmplayer_set_unlinked_mime_type(mm_player_t* player, GstCaps *caps);

/* util */
const gchar * __get_state_name ( int state );
static gboolean __is_streaming( mm_player_t* player );
static gboolean __is_rtsp_streaming( mm_player_t* player );
static gboolean __is_live_streaming ( mm_player_t* player );
static gboolean __is_http_streaming( mm_player_t* player );
static gboolean __is_http_live_streaming( mm_player_t* player );
static gboolean __is_http_progressive_down(mm_player_t* player);

static gboolean __mmplayer_warm_up_video_codec( mm_player_t* player,  GstElementFactory *factory);
static GstBusSyncReply __mmplayer_bus_sync_callback (GstBus * bus, GstMessage * message, gpointer data);

/*===========================================================================================
|																							|
|  FUNCTION DEFINITIONS																		|
|  																							|
========================================================================================== */

/* implementing player FSM */
/* FIXIT : We need to handle state transition also at here since start api is no more sync */
static int
__mmplayer_check_state(mm_player_t* player, enum PlayerCommandState command)
{
	MMPlayerStateType current_state = MM_PLAYER_STATE_NUM;
	MMPlayerStateType pending_state = MM_PLAYER_STATE_NUM;
	MMPlayerStateType target_state = MM_PLAYER_STATE_NUM;
	MMPlayerStateType prev_state = MM_PLAYER_STATE_NUM;

	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	//debug_log("incomming command : %d \n", command );

	current_state = MMPLAYER_CURRENT_STATE(player);
	pending_state = MMPLAYER_PENDING_STATE(player);
	target_state = MMPLAYER_TARGET_STATE(player);
	prev_state = MMPLAYER_PREV_STATE(player);

	MMPLAYER_PRINT_STATE(player);

	switch( command )
	{
		case MMPLAYER_COMMAND_CREATE:
		{
			MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_NULL;
			
			if ( current_state == MM_PLAYER_STATE_NULL ||
				current_state == MM_PLAYER_STATE_READY ||
				current_state == MM_PLAYER_STATE_PAUSED ||
				current_state == MM_PLAYER_STATE_PLAYING )
				goto NO_OP;
		}
		break;

		case MMPLAYER_COMMAND_DESTROY:
		{
			/* destroy can called anytime */

			MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_NONE;
		}
		break;

		case MMPLAYER_COMMAND_REALIZE:
		{
			MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_READY;

			if ( pending_state != MM_PLAYER_STATE_NONE )
			{
				goto INVALID_STATE;
			}
			else
			{
				/* need ready state to realize */
				if ( current_state == MM_PLAYER_STATE_READY )
					goto NO_OP;

				if ( current_state != MM_PLAYER_STATE_NULL )
					goto INVALID_STATE;
			}
		}
		break;

		case MMPLAYER_COMMAND_UNREALIZE:
		{
			MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_NULL;

			if ( current_state == MM_PLAYER_STATE_NULL )
				goto NO_OP;
		}
		break;

		case MMPLAYER_COMMAND_START:
		{
			MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_PLAYING;

			if ( pending_state == MM_PLAYER_STATE_NONE )
			{
				if ( current_state == MM_PLAYER_STATE_PLAYING )
					goto NO_OP;
				else if ( current_state  != MM_PLAYER_STATE_READY &&
					current_state != MM_PLAYER_STATE_PAUSED )
					goto INVALID_STATE;
			}
			else if ( pending_state == MM_PLAYER_STATE_PLAYING )
			{
				goto ALREADY_GOING;
			}
			else if ( pending_state == MM_PLAYER_STATE_PAUSED )
			{
				debug_log("player is going to paused state, just change the pending state as playing");
			}
			else
			{
				goto INVALID_STATE;
			}
		}
		break;

		case MMPLAYER_COMMAND_STOP:
		{
			MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_READY;

			if ( current_state == MM_PLAYER_STATE_READY )
				goto NO_OP;
			
			/* need playing/paused state to stop */
			if ( current_state != MM_PLAYER_STATE_PLAYING &&
				 current_state != MM_PLAYER_STATE_PAUSED )
				goto INVALID_STATE;
		}
		break;

		case MMPLAYER_COMMAND_PAUSE:
		{
			if ( MMPLAYER_IS_LIVE_STREAMING( player ) )
				goto NO_OP;

			if (player->doing_seek)
				goto NOT_COMPLETED_SEEK;

			MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_PAUSED;

			if ( pending_state == MM_PLAYER_STATE_NONE )
			{
				if ( current_state == MM_PLAYER_STATE_PAUSED )
					goto NO_OP;
				else if ( current_state != MM_PLAYER_STATE_PLAYING && current_state != MM_PLAYER_STATE_READY ) // support loading state of broswer
					goto INVALID_STATE;
			}
			else if ( pending_state == MM_PLAYER_STATE_PAUSED )
			{
				goto ALREADY_GOING;
			}
			else if ( pending_state == MM_PLAYER_STATE_PLAYING )
			{
				if ( current_state == MM_PLAYER_STATE_PAUSED ) {
					debug_log("player is PAUSED going to PLAYING, just change the pending state as PAUSED");
				} else {
					goto INVALID_STATE;
				}
			}
		}
		break;

		case MMPLAYER_COMMAND_RESUME:
		{
			if ( MMPLAYER_IS_LIVE_STREAMING(player) )
				goto NO_OP;

			if (player->doing_seek)
				goto NOT_COMPLETED_SEEK;

			MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_PLAYING;

			if ( pending_state == MM_PLAYER_STATE_NONE )
			{
				if ( current_state == MM_PLAYER_STATE_PLAYING )
					goto NO_OP;
				else if (  current_state != MM_PLAYER_STATE_PAUSED )
					goto INVALID_STATE;
			}
			else if ( pending_state == MM_PLAYER_STATE_PLAYING )
			{
				goto ALREADY_GOING;
			}
			else if ( pending_state == MM_PLAYER_STATE_PAUSED )
			{
				debug_log("player is going to paused state, just change the pending state as playing");
			}
			else
			{
				goto INVALID_STATE;
			}
		}
		break;

		default:
		break;
	}
	player->cmd = command;

	debug_fleave();
	return MM_ERROR_NONE;

INVALID_STATE:
	debug_warning("since player is in wrong state(%s). it's not able to apply the command(%d)",
		MMPLAYER_STATE_GET_NAME(current_state), command);
	return MM_ERROR_PLAYER_INVALID_STATE;

NOT_COMPLETED_SEEK:
	debug_warning("not completed seek");
	return MM_ERROR_PLAYER_DOING_SEEK;

NO_OP:
	debug_warning("player is in the desired state(%s). doing noting", MMPLAYER_STATE_GET_NAME(current_state));
	return MM_ERROR_PLAYER_NO_OP;

ALREADY_GOING:
	debug_warning("player is already going to %s, doing nothing", MMPLAYER_STATE_GET_NAME(pending_state));
	return MM_ERROR_PLAYER_NO_OP;
}

int
__mmplayer_gst_set_state (mm_player_t* player, GstElement * element,  GstState state, gboolean async, gint timeout) // @
{
	GstState element_state = GST_STATE_VOID_PENDING;
	GstState element_pending_state = GST_STATE_VOID_PENDING;
	GstStateChangeReturn ret = GST_STATE_CHANGE_FAILURE;

	debug_fenter();
	
	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( element, MM_ERROR_INVALID_ARGUMENT );

	debug_log("setting [%s] element state to : %d\n", GST_ELEMENT_NAME(element),  state);

	/* set state */
	ret = gst_element_set_state(element, state);

	if ( ret == GST_STATE_CHANGE_FAILURE )
	{
		debug_error("failed to set  [%s] state to [%d]\n", GST_ELEMENT_NAME(element), state);
		return MM_ERROR_PLAYER_INTERNAL;
	}
	
	/* return here so state transition to be done in async mode */
	if ( async )
	{
		debug_log("async state transition. not waiting for state complete.\n");
		return MM_ERROR_NONE;
	}

	/* wait for state transition */
	ret = gst_element_get_state( element, &element_state, &element_pending_state, timeout * GST_SECOND );

	if ( ret == GST_STATE_CHANGE_FAILURE || ( state != element_state ) )
	{
		debug_error("failed to change [%s] element state to [%s] within %d sec\n",
			GST_ELEMENT_NAME(element), 
			gst_element_state_get_name(state), timeout );
		
		debug_error(" [%s] state : %s   pending : %s \n", 
			GST_ELEMENT_NAME(element), 
			gst_element_state_get_name(element_state), 
			gst_element_state_get_name(element_pending_state) );

		return MM_ERROR_PLAYER_INTERNAL;
	}

	debug_log("[%s] element state has changed to %s \n",
		GST_ELEMENT_NAME(element), 
		gst_element_state_get_name(element_state));

	debug_fleave();
	
	return MM_ERROR_NONE;
}

static void
__mmplayer_videostream_cb(GstElement *element, void *stream,
int width, int height, gpointer data) // @
{
 	mm_player_t* player = (mm_player_t*)data;
   	int length = 0;

	return_if_fail ( player );

	debug_fenter();

   	if (player->video_stream_cb )
    	{
        	length = width * height * 4; // for rgb 32bit
        	player->video_stream_cb(stream, length, player->video_stream_cb_user_param, width, height);
    	}

	debug_fleave();
}

gboolean
_mmplayer_update_content_attrs(mm_player_t* player) // @
{
	GstFormat fmt  = GST_FORMAT_TIME;
	gint64 dur_nsec = 0;
	GstStructure* p = NULL;
	MMHandleType attrs = 0;
	gint retry_count = 0;
	gint retry_count_max = 10;
	gchar *path = NULL;
	struct stat sb;

	return_val_if_fail ( player, FALSE );

	if ( ! player->need_update_content_attrs )
	{
		debug_log("content attributes are already updated");
		return TRUE;
	}

	/* get content attribute first */
	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("cannot get content attribute");
		return FALSE;
	}

	/* update duration
	 * NOTE : we need to wait for a while until is possible to get duration from pipeline
	 * as getting duration timing is depends on behavier of demuxers ( or etc ).
	 * we set timeout 100ms * 10 as initial value. fix it if needed.
	 */
	if ( player->need_update_content_dur  )
	{
		while ( retry_count <  retry_count_max)
		{
			if ( FALSE == gst_element_query_duration( player->pipeline->mainbin[MMPLAYER_M_PIPE].gst,
				&fmt, &dur_nsec ) )
			{
				/* retry if failed */
				debug_warning("failed to get duraton. waiting 100ms and then retrying...");
				usleep(100000);
				retry_count++;
				continue;
			}

			if ( dur_nsec == 0 && ( !MMPLAYER_IS_LIVE_STREAMING( player ) ) )
			{
				/* retry if duration is zero in case of not live stream */
				debug_warning("returned duration is zero. but it's not an live stream. retrying...");
				usleep(100000);
				retry_count++;
				continue;
			}

			break;
		}

		player->duration = dur_nsec;
		debug_log("duration : %lld msec", GST_TIME_AS_MSECONDS(dur_nsec));

		/* try to get streaming service type */
		__mmplayer_update_stream_service_type( player );

		/* check duration is OK */
		if ( dur_nsec == 0 && !MMPLAYER_IS_LIVE_STREAMING( player ) )
		{
			/* FIXIT : find another way to get duration here. */
			debug_error("finally it's failed to get duration from pipeline. progressbar will not work correctely!");
		}
		else
		{
			player->need_update_content_dur = FALSE;
		}

		/*update duration */
		mm_attrs_set_int_by_name(attrs, "content_duration", GST_TIME_AS_MSECONDS(dur_nsec));
	}
	else
	{
		debug_log("not ready to get duration or already updated");
	}

	/* update rate, channels */
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
			caps_a = gst_pad_get_negotiated_caps( pad );

			if ( caps_a )
			{
				p = gst_caps_get_structure (caps_a, 0);

				mm_attrs_get_int_by_name(attrs, "content_audio_samplerate", &samplerate);
				if ( ! samplerate ) // check if update already or not
				{
					gst_structure_get_int (p, "rate", &samplerate);
					mm_attrs_set_int_by_name(attrs, "content_audio_samplerate", samplerate);

					gst_structure_get_int (p, "channels", &channels);
					mm_attrs_set_int_by_name(attrs, "content_audio_channels", channels);

					debug_log("samplerate : %d	channels : %d", samplerate, channels);
				}
				gst_caps_unref( caps_a );
				caps_a = NULL;
			}
			else
			{
				debug_warning("not ready to get audio caps");
			}

			gst_object_unref( pad );
		}
		else
		{
			debug_warning("failed to get pad from audiosink");
		}
	}

	/* update width, height, framerate */
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
			caps_v = gst_pad_get_negotiated_caps( pad );
			if (caps_v)
			{
				p = gst_caps_get_structure (caps_v, 0);
				gst_structure_get_int (p, "width", &width);
				mm_attrs_set_int_by_name(attrs, "content_video_width", width);

				gst_structure_get_int (p, "height", &height);
				mm_attrs_set_int_by_name(attrs, "content_video_height", height);

				gst_structure_get_fraction (p, "framerate", &tmpNu, &tmpDe);

				debug_log("width : %d     height : %d", width, height );

				gst_caps_unref( caps_v );
				caps_v = NULL;

				if (tmpDe > 0)
				{
					mm_attrs_set_int_by_name(attrs, "content_video_fps", tmpNu / tmpDe);
					debug_log("fps : %d", tmpNu / tmpDe);
				}
			}
			else
			{
				debug_warning("failed to get negitiated caps from videosink");
			}
			gst_object_unref( pad );
			pad = NULL;
		}
		else
		{
			debug_warning("failed to get pad from videosink");
		}
	}

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

		if (data_size)
		{
			guint64 bitrate = 0;
			guint64 msec_dur = 0;

			msec_dur = GST_TIME_AS_MSECONDS(player->duration);
			bitrate = data_size * 8 * 1000 / msec_dur;
			debug_log("file size : %u, video bitrate = %llu", data_size, bitrate);
			mm_attrs_set_int_by_name(attrs, "content_video_bitrate", bitrate);
		}
	}


	/* validate all */
	if (  mmf_attrs_commit ( attrs ) )
	{
		debug_error("failed to update attributes\n");
		return FALSE;
	}

	player->need_update_content_attrs = FALSE;

	return TRUE;
}

gboolean __mmplayer_update_stream_service_type( mm_player_t* player )
{
	MMHandleType attrs = 0;
	gint streaming_type = STREAMING_SERVICE_NONE;

	debug_fenter();

	return_val_if_fail ( player &&
					player->pipeline &&
					player->pipeline->mainbin &&
					player->pipeline->mainbin[MMPLAYER_M_SRC].gst,
					FALSE );

	/* streaming service type if streaming */
	if ( ! MMPLAYER_IS_STREAMING(player) );
		return FALSE;

	if (MMPLAYER_IS_RTSP_STREAMING(player))
	{
		/* get property from rtspsrc element */
		g_object_get(G_OBJECT(player->pipeline->mainbin[MMPLAYER_M_SRC].gst), "service_type", &streaming_type, NULL);
	}
	else if (MMPLAYER_IS_HTTP_STREAMING(player))
	{
		if ( player->duration <= 0)
			streaming_type = STREAMING_SERVICE_LIVE;
		else
			streaming_type = STREAMING_SERVICE_VOD;			
	}
		
	player->streaming_type = streaming_type;

	if ( player->streaming_type == STREAMING_SERVICE_LIVE)
	{
		debug_log("It's live streaming. pause/resume/seek are not working.\n");
	}
	else if (player->streaming_type == STREAMING_SERVICE_LIVE)
	{
		debug_log("It's vod streaming. pause/resume/seek are working.\n");
	}
	else
	{
		debug_warning("fail to determine streaming type. pause/resume/seek may not working properly if stream is live stream\n");
	}

	/* get profile attribute */
	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("cannot get content attribute\n");
		return FALSE;
	}

	mm_attrs_set_int_by_name ( attrs, "streaming_type", streaming_type );
	/* validate all */
	if (  mmf_attrs_commit ( attrs ) )
	{
		debug_warning("updating streaming service type failed. pause/resume/seek may not working properly if stream is live stream\n");
		return FALSE;
	}

	debug_fleave();

	return TRUE;
}


/* this function sets the player state and also report
 * it to applicaton by calling callback function
 */
static gboolean
__mmplayer_set_state(mm_player_t* player, int state) // @
{
	MMMessageParamType msg = {0, };
	int asm_result = MM_ERROR_NONE;

	debug_fenter();
	return_val_if_fail ( player, FALSE );

	if ( MMPLAYER_CURRENT_STATE(player) == state )
	{
		debug_warning("already same state(%s)\n", MMPLAYER_STATE_GET_NAME(state));
		MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_NONE;
		return TRUE;
	}

	/* post message to application */
	if (MMPLAYER_TARGET_STATE(player) == state)
	{
		/* fill the message with state of player */
		msg.state.previous = MMPLAYER_CURRENT_STATE(player);
		msg.state.current = state;

		/* state changed by asm callback */
		if ( player->sm.by_asm_cb )
		{
		    	msg.union_type = MM_MSG_UNION_CODE;
		    	msg.code = player->sm.event_src;
			MMPLAYER_POST_MSG( player, MM_MESSAGE_STATE_INTERRUPTED, &msg );
		}
		/* state changed by usecase */
		else
		{
			MMPLAYER_POST_MSG( player, MM_MESSAGE_STATE_CHANGED, &msg );
		}

		debug_log ("player reach the target state, then do something in each state(%s).\n", 
			MMPLAYER_STATE_GET_NAME(MMPLAYER_TARGET_STATE(player)));
	}
	else
	{
		debug_log ("intermediate state, do nothing.\n");
		MMPLAYER_PRINT_STATE(player);

		return TRUE;
	}

	/* update player states */
	MMPLAYER_PREV_STATE(player) = MMPLAYER_CURRENT_STATE(player);
	MMPLAYER_CURRENT_STATE(player) = state;
	if ( MMPLAYER_CURRENT_STATE(player) == MMPLAYER_PENDING_STATE(player) )
		MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_NONE;

	/* print state */
	MMPLAYER_PRINT_STATE(player);
	
	switch ( MMPLAYER_TARGET_STATE(player) )
	{
		case MM_PLAYER_STATE_NULL:	
		case MM_PLAYER_STATE_READY:
		{
			if (player->cmd == MMPLAYER_COMMAND_STOP)
			{
				asm_result = _mmplayer_asm_set_state((MMHandleType)player, ASM_STATE_STOP);
				if ( asm_result != MM_ERROR_NONE )
				{
					debug_error("failed to set asm state to stop\n");
					return FALSE;
				}
			}
		}
		break;
			
		case MM_PLAYER_STATE_PAUSED:
		{
			/* special care for local playback. normaly we can get some content attribute
			 * when the demuxer is changed to PAUSED. so we are trying it. it will be tried again
			 * when PLAYING state has signalled if failed.
			 * note that this is only happening pause command has come before the state of pipeline
			 * reach to the PLAYING.
			 */
			 if ( ! player->sent_bos ) // managed prepare sync case
			 {
				player->need_update_content_dur = TRUE;
				_mmplayer_update_content_attrs( player );
			 }

			/* add audio callback probe if condition is satisfied */
			if ( ! player->audio_cb_probe_id && player->is_sound_extraction )
				__mmplayer_configure_audio_callback(player);

			asm_result = _mmplayer_asm_set_state((MMHandleType)player, ASM_STATE_PAUSE);
			if ( asm_result )
			{
				debug_error("failed to set asm state to PAUSE\n");
				return FALSE;
			}
		}
		break;

		case MM_PLAYER_STATE_PLAYING:
		{
			/* non-managed prepare case, should be updated */
			if ( ! player->need_update_content_dur)
			{
				player->need_update_content_dur = TRUE;
				_mmplayer_update_content_attrs ( player );
			}

			if ( player->cmd == MMPLAYER_COMMAND_START  && !player->sent_bos )
			{
				__mmplayer_post_missed_plugin ( player );

				/* update video resource status */
				if ( ( player->can_support_codec & 0x02) == FOUND_PLUGIN_VIDEO )
				{
					asm_result = _mmplayer_asm_set_state((MMHandleType)player, ASM_STATE_PLAYING);
					if ( asm_result )
					{
						MMMessageParamType msg = {0, };
								
						debug_error("failed to go ahead because of video conflict\n");
													
						msg.union_type = MM_MSG_UNION_CODE;
						msg.code = MM_ERROR_POLICY_INTERRUPTED;
						MMPLAYER_POST_MSG( player, MM_MESSAGE_STATE_INTERRUPTED, &msg);

						_mmplayer_unrealize((MMHandleType)player);
														
						return FALSE;								
					}
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
					debug_log("set audio codec type with caps\n");
				}

				MMTA_ACUM_ITEM_END("[KPI] start media player service", FALSE);
				MMTA_ACUM_ITEM_END("[KPI] media player service create->playing", FALSE);

				MMPLAYER_POST_MSG ( player, MM_MESSAGE_BEGIN_OF_STREAM, NULL );
				player->sent_bos = TRUE;
			}
		}
		break;

		case MM_PLAYER_STATE_NONE:
		default:
			debug_warning("invalid target state, there is nothing to do.\n");
			break;
	}

	debug_fleave();

	return TRUE;
}


gboolean
__mmplayer_post_message(mm_player_t* player, enum MMMessageType msgtype, MMMessageParamType* param) // @
{
	return_val_if_fail( player, FALSE );

	debug_fenter();

	if ( !player->msg_cb )
	{
		debug_warning("no msg callback. can't post\n");
		return FALSE;
	}

	//debug_log("Message (type : %d)  will be posted using msg-cb(%p). \n", msgtype, player->msg_cb);

	player->msg_cb(msgtype, param, player->msg_cb_param);

	debug_fleave();

	return TRUE;
}


static int
__mmplayer_get_state(mm_player_t* player) // @
{
	int state = MM_PLAYER_STATE_NONE;

	debug_fenter();
	
	return_val_if_fail ( player, MM_PLAYER_STATE_NONE );

	state = MMPLAYER_CURRENT_STATE(player);

	debug_log("player state is %s.\n", MMPLAYER_STATE_GET_NAME(state));

	debug_fleave();
	
	return state;
}

static void
__gst_set_async_state_change(mm_player_t* player, gboolean async)
{
	//debug_fenter();
	return_if_fail( player && player->pipeline && player->pipeline->mainbin );

	/* need only when we are using decodebin */
	if ( ! PLAYER_INI()->use_decodebin )
		return;

	/* audio sink */
	if ( player->pipeline->audiobin &&
		 player->pipeline->audiobin[MMPLAYER_A_SINK].gst )
	{
		debug_log("audiosink async : %d\n", async);
		g_object_set (G_OBJECT (player->pipeline->audiobin[MMPLAYER_A_SINK].gst), "async", async, NULL);
	}

	/* video sink */
	if ( player->pipeline->videobin &&
		 player->pipeline->videobin[MMPLAYER_V_SINK].gst )
	{
		debug_log("videosink async : %d\n", async);
		g_object_set (G_OBJECT (player->pipeline->videobin[MMPLAYER_V_SINK].gst), "async", async, NULL);
	}

	/* decodebin if enabled */
	if ( PLAYER_INI()->use_decodebin )
	{
		debug_log("decodebin async : %d\n", async);
		g_object_set (G_OBJECT (player->pipeline->mainbin[MMPLAYER_M_AUTOPLUG].gst), "async-handling", async, NULL);
	}

	//debug_fleave();
}

static gpointer __mmplayer_repeat_thread(gpointer data)
{
	mm_player_t* player = (mm_player_t*) data;
	gboolean ret_value = FALSE;
	MMHandleType attrs = 0;
	gint count = 0;

	return_val_if_fail ( player, NULL );

	while ( ! player->repeat_thread_exit )
	{
		debug_log("repeat thread started. waiting for signal.\n");
		g_cond_wait( player->repeat_thread_cond, player->repeat_thread_mutex );

		if ( player->repeat_thread_exit )
		{
			debug_log("exiting repeat thread\n");
			break;
		}

		if ( !player->cmd_lock )
		{
			debug_log("can't get cmd lock\n");
			return NULL;
		}

		/* lock */
		g_mutex_lock(player->cmd_lock);	

		attrs = MMPLAYER_GET_ATTRS(player);

		if (mm_attrs_get_int_by_name(attrs, "profile_play_count", &count) != MM_ERROR_NONE)
		{
			debug_error("can not get play count\n");
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

			ret_value = __gst_seek( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, 1.0,
				GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET,
				0, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);

			/* initialize */
			player->sent_bos = FALSE;
		}

		if ( ! ret_value )
		{
			debug_error("failed to set position to zero for rewind\n");
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
				debug_error("failed to commit attribute\n");
			}
		}

		/* unlock */
		g_mutex_unlock(player->cmd_lock);
	}

	return NULL;
}

static void
__mmplayer_handle_buffering_message ( mm_player_t* player )
{
	MMPlayerStateType prev_state = MM_PLAYER_STATE_NONE;
	MMPlayerStateType current_state = MM_PLAYER_STATE_NONE;
	MMPlayerStateType target_state = MM_PLAYER_STATE_NONE;
	MMPlayerStateType pending_state = MM_PLAYER_STATE_NONE;

	return_if_fail ( player );

	prev_state = MMPLAYER_PREV_STATE(player),
	current_state = MMPLAYER_CURRENT_STATE(player);
	target_state = MMPLAYER_TARGET_STATE(player);
	pending_state = MMPLAYER_PENDING_STATE(player);

	if ( MMPLAYER_IS_RTSP_STREAMING(player) )
		return;

	if ( !player->streamer->is_buffering )
	{
		debug_log( "player state : prev %s, current %s, pending %s, target %s \n",
			MMPLAYER_STATE_GET_NAME(prev_state),
			MMPLAYER_STATE_GET_NAME(current_state),
			MMPLAYER_STATE_GET_NAME(pending_state),
			MMPLAYER_STATE_GET_NAME(target_state));

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
						 debug_log("player is already going to paused state, there is nothing to do.\n");
					}
					break;

					case MM_PLAYER_STATE_NONE:
					case MM_PLAYER_STATE_NULL:
					case MM_PLAYER_STATE_READY:
					default :
					{
						debug_warning("invalid pending state [%s].\n", MMPLAYER_STATE_GET_NAME(pending_state) );
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
						 debug_log("player is already going to playing state, there is nothing to do.\n");
					}
					break;

					case MM_PLAYER_STATE_NULL:
					case MM_PLAYER_STATE_READY:
					default :
					{
						debug_warning("invalid pending state [%s].\n", MMPLAYER_STATE_GET_NAME(pending_state) );
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
				debug_warning("invalid target state [%s].\n", MMPLAYER_STATE_GET_NAME(target_state) );
			}
				break;
		}
	}
	else
	{
		/* NOTE : during the buffering, pause the player for stopping pipeline clock.
		 * 	it's for stopping the pipeline clock to prevent dropping the data in sink element.
		 */
		switch ( pending_state )
		{
			case MM_PLAYER_STATE_NONE:
			{
				if (current_state != MM_PLAYER_STATE_PAUSED)
					__gst_pause ( player, TRUE );
			}
			break;

			case MM_PLAYER_STATE_PLAYING:
			{
				__gst_pause ( player, TRUE );
			}
			break;

			case MM_PLAYER_STATE_PAUSED:
			{
				 debug_log("player is already going to paused state, there is nothing to do.\n");
			}
			break;

			case MM_PLAYER_STATE_NULL:
			case MM_PLAYER_STATE_READY:
			default :
			{
				debug_warning("invalid pending state [%s].\n", MMPLAYER_STATE_GET_NAME(pending_state) );
			}
				break;
		}
	}
}

static gboolean
__mmplayer_gst_callback(GstBus *bus, GstMessage *msg, gpointer data) // @
{
	mm_player_t* player = (mm_player_t*) data;
	gboolean ret = TRUE;
	static gboolean async_done = FALSE;

	return_val_if_fail ( player, FALSE );
	return_val_if_fail ( msg && GST_IS_MESSAGE(msg), FALSE );

	switch ( GST_MESSAGE_TYPE( msg ) )
	{
		case GST_MESSAGE_UNKNOWN:
			debug_warning("unknown message received\n");
		break;

		case GST_MESSAGE_EOS:
		{
			MMHandleType attrs = 0;
			gint count = 0;

			debug_log("GST_MESSAGE_EOS received\n");

			/* NOTE : EOS event is comming multiple time. watch out it */
			/* check state. we only process EOS when pipeline state goes to PLAYING */
			if ( ! (player->cmd == MMPLAYER_COMMAND_START || player->cmd == MMPLAYER_COMMAND_RESUME) )
			{
				debug_warning("EOS received on non-playing state. ignoring it\n");
				break;
			}

			if ( (player->audio_stream_cb) && (player->is_sound_extraction) )
			{
				GstPad *pad = NULL;
	
				pad = gst_element_get_static_pad (player->pipeline->audiobin[MMPLAYER_A_SINK].gst, "sink");

				debug_error("release audio callback\n");
				
				/* release audio callback */
				gst_pad_remove_buffer_probe (pad, player->audio_cb_probe_id);
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

				debug_log("remaining play count: %d, playback rate: %f\n", count, player->playback_rate);

				if ( count > 1 || count == -1 || player->playback_rate < 0.0 ) /* default value is 1 */
				{
					if ( smooth_repeat )
					{
						debug_log("smooth repeat enabled. seeking operation will be excuted in new thread\n");

						g_cond_signal( player->repeat_thread_cond );

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

							ret_value = __gst_set_position( player, MM_PLAYER_POS_FORMAT_TIME, 0, TRUE);

							/* initialize */
							player->sent_bos = FALSE;
						}

						if ( MM_ERROR_NONE != ret_value )
						{
							debug_error("failed to set position to zero for rewind\n");
						}
						else
						{
							if ( count > 1 )
							{
								/* we successeded to rewind. update play count and then wait for next EOS */
								count--;

								mm_attrs_set_int_by_name(attrs, "profile_play_count", count);

								if ( mmf_attrs_commit ( attrs ) )
									debug_error("failed to commit attrs\n");
							}
						}

						break;
					}
				}
			}

			MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-eos" );

			/* post eos message to application */
			__mmplayer_post_delayed_eos( player, PLAYER_INI()->eos_delay );

			/* reset last position */
			player->last_position = 0;
		}
		break;

		case GST_MESSAGE_ERROR:
		{
			GError *error = NULL;
			gchar* debug = NULL;
  	      		gchar *msg_src_element = NULL;

			/* generating debug info before returning error */
			MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-error" );

			/* get error code */
			gst_message_parse_error( msg, &error, &debug );

			msg_src_element = GST_ELEMENT_NAME( GST_ELEMENT_CAST( msg->src ) );
			if ( gst_structure_has_name ( msg->structure, "streaming_error" ) )
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

				/* dump state of all element */
				__mmplayer_dump_pipeline_state( player );

			}

			if (MMPLAYER_IS_HTTP_PD(player))
			{	
				_mmplayer_pd_stop ((MMHandleType)player);
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

			debug_warning("warning : %s\n", error->message);
			debug_warning("debug : %s\n", debug);

			MMPLAYER_POST_MSG( player, MM_MESSAGE_WARNING, NULL );

			MMPLAYER_FREEIF( debug );
			g_error_free( error );
		}
		break;

		case GST_MESSAGE_INFO:				debug_log("GST_MESSAGE_STATE_DIRTY\n"); break;

		case GST_MESSAGE_TAG:
		{
			debug_log("GST_MESSAGE_TAG\n");
			if ( ! __mmplayer_gst_extract_tag_from_msg( player, msg ) )
			{
				debug_warning("failed to extract tags from gstmessage\n");
			}
		}
		break;

		case GST_MESSAGE_BUFFERING:
		{
			MMMessageParamType msg_param = {0, };
			gboolean update_buffering_percent = TRUE;

			if ( !MMPLAYER_IS_STREAMING(player) || (player->profile.uri_type == MM_PLAYER_URI_TYPE_HLS) ) // pure hlsdemux case, don't consider buffering of msl currently
				break;

			__mm_player_streaming_buffering (player->streamer, msg);

			__mmplayer_handle_buffering_message ( player );

			update_buffering_percent = (player->pipeline_is_constructed || MMPLAYER_IS_RTSP_STREAMING(player) );
			if (update_buffering_percent)
			{
				msg_param.connection.buffering = player->streamer->buffering_percent;
				MMPLAYER_POST_MSG ( player, MM_MESSAGE_BUFFERING, &msg_param );
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
				debug_error("player pipeline handle is null");

				break;
			}

			mainbin = player->pipeline->mainbin;

			/* get state info from msg */
			voldstate = gst_structure_get_value (msg->structure, "old-state");
			vnewstate = gst_structure_get_value (msg->structure, "new-state");
			vpending = gst_structure_get_value (msg->structure, "pending-state");

			oldstate = (GstState)voldstate->data[0].v_int;
			newstate = (GstState)vnewstate->data[0].v_int;
			pending = (GstState)vpending->data[0].v_int;

			if (oldstate == newstate)
				break;

			debug_log("state changed [%s] : %s ---> %s     final : %s\n",
				GST_OBJECT_NAME(GST_MESSAGE_SRC(msg)),
				gst_element_state_get_name( (GstState)oldstate ),
				gst_element_state_get_name( (GstState)newstate ),
				gst_element_state_get_name( (GstState)pending ) );

			/* we only handle messages from pipeline */
			if( msg->src != (GstObject *)mainbin[MMPLAYER_M_PIPE].gst )
				break;

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

					player->need_update_content_dur = TRUE;

					if ( ! player->audio_cb_probe_id && player->is_sound_extraction )
						__mmplayer_configure_audio_callback(player);

					if ( ! player->sent_bos && oldstate == GST_STATE_READY) // managed prepare async case
					{
						mm_attrs_get_int_by_name(player->attrs, "profile_prepare_async", &prepare_async);
						debug_log("checking prepare mode for async transition - %d", prepare_async);
					}

					if ( MMPLAYER_IS_STREAMING(player) || prepare_async )
					{
						MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_PAUSED );

						if (player->streamer)
							__mm_player_streaming_set_content_bitrate(player->streamer, player->total_maximum_bitrate, player->total_bitrate);
					}
				}					
				break;

				case GST_STATE_PLAYING:
				{
					if (player->doing_seek && async_done)
					{
						player->doing_seek = FALSE;
						async_done = FALSE;
						MMPLAYER_POST_MSG ( player, MM_MESSAGE_SEEK_COMPLETED, NULL );
					}

					if ( MMPLAYER_IS_STREAMING(player) ) // managed prepare async case when buffering is completed
					{
						// pending state should be reset oyherwise, it's still playing even though it's resumed after bufferging.
						MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_PLAYING);
					}
				}
				break;

				default:
				break;
			}
		}
		break;

		case GST_MESSAGE_STATE_DIRTY:		debug_log("GST_MESSAGE_STATE_DIRTY\n"); break;
		case GST_MESSAGE_STEP_DONE:			debug_log("GST_MESSAGE_STEP_DONE\n"); break;
		case GST_MESSAGE_CLOCK_PROVIDE:		debug_log("GST_MESSAGE_CLOCK_PROVIDE\n"); break;
		
		case GST_MESSAGE_CLOCK_LOST:
			{
				GstClock *clock = NULL;
				gst_message_parse_clock_lost (msg, &clock);
				debug_log("GST_MESSAGE_CLOCK_LOST : %s\n", (clock ? GST_OBJECT_NAME (clock) : "NULL"));
				g_print ("GST_MESSAGE_CLOCK_LOST : %s\n", (clock ? GST_OBJECT_NAME (clock) : "NULL"));

				if (PLAYER_INI()->provide_clock)
				{
					debug_log ("Provide clock is TRUE, do pause->resume\n");
					__gst_pause(player, FALSE);
					__gst_resume(player, FALSE);
				}
			}
			break;
			
		case GST_MESSAGE_NEW_CLOCK:
			{
				GstClock *clock = NULL;
				gst_message_parse_new_clock (msg, &clock);
				debug_log("GST_MESSAGE_NEW_CLOCK : %s\n", (clock ? GST_OBJECT_NAME (clock) : "NULL"));
			}
			break;

		case GST_MESSAGE_STRUCTURE_CHANGE:	debug_log("GST_MESSAGE_STRUCTURE_CHANGE\n"); break;
		case GST_MESSAGE_STREAM_STATUS:		debug_log("GST_MESSAGE_STREAM_STATUS\n"); break;
		case GST_MESSAGE_APPLICATION:		debug_log("GST_MESSAGE_APPLICATION\n"); break;

		case GST_MESSAGE_ELEMENT:
		{
			debug_log("GST_MESSAGE_ELEMENT\n");
		}
		break;
		
		case GST_MESSAGE_SEGMENT_START:		debug_log("GST_MESSAGE_SEGMENT_START\n"); break;
		case GST_MESSAGE_SEGMENT_DONE:		debug_log("GST_MESSAGE_SEGMENT_DONE\n"); break;
		
		case GST_MESSAGE_DURATION:
		{
			debug_log("GST_MESSAGE_DURATION\n");

			if (MMPLAYER_IS_STREAMING(player))
			{
				GstFormat format;
				gint64 bytes = 0;

				gst_message_parse_duration (msg, &format, &bytes);
				if (format == GST_FORMAT_BYTES)
				{
					debug_log("data total size of http content: %lld", bytes);
					player->http_content_size = bytes;
				}
			}

			player->need_update_content_attrs = TRUE;
			player->need_update_content_dur = TRUE;
			_mmplayer_update_content_attrs(player);
		}
		break;

		case GST_MESSAGE_LATENCY:				debug_log("GST_MESSAGE_LATENCY\n"); break;
		case GST_MESSAGE_ASYNC_START:		debug_log("GST_MESSAGE_ASYNC_DONE : %s\n", gst_element_get_name(GST_MESSAGE_SRC(msg))); break;
		
		case GST_MESSAGE_ASYNC_DONE:	
		{
			debug_log("GST_MESSAGE_ASYNC_DONE : %s\n", gst_element_get_name(GST_MESSAGE_SRC(msg)));

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
		break;
		
		case GST_MESSAGE_REQUEST_STATE:		debug_log("GST_MESSAGE_REQUEST_STATE\n"); break;
		case GST_MESSAGE_STEP_START:		debug_log("GST_MESSAGE_STEP_START\n"); break;
		case GST_MESSAGE_QOS:					debug_log("GST_MESSAGE_QOS\n"); break;
		case GST_MESSAGE_PROGRESS:			debug_log("GST_MESSAGE_PROGRESS\n"); break;
		case GST_MESSAGE_ANY:				debug_log("GST_MESSAGE_ANY\n"); break;

		default:
			debug_warning("unhandled message\n");
		break;
	}

	/* FIXIT : this cause so many warnings/errors from glib/gstreamer. we should not call it since
	 * gst_element_post_message api takes ownership of the message.
	 */
	//gst_message_unref( msg );

	return ret;
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
		debug_log ( "update tag string : %s\n", string); \
		mm_attrs_set_string_by_name(attribute, playertag, string); \
		g_free(string);\
		string = NULL;\
	}\
}

#define MMPLAYER_UPDATE_TAG_IMAGE(gsttag, attribute, playertag) \
value = gst_tag_list_get_value_index(tag_list, gsttag, index); \
if (value) \
{\
	buffer = gst_value_get_buffer (value); \
	debug_log ( "update album cover data : %p, size : %d\n", GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer)); \
	player->album_art = (gchar *)g_malloc(GST_BUFFER_SIZE(buffer)); \
	if (player->album_art); \
	{ \
		memcpy(player->album_art, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer)); \
		mm_attrs_set_data_by_name(attribute, playertag, (void *)player->album_art, GST_BUFFER_SIZE(buffer)); \
	} \
}

#define MMPLAYER_UPDATE_TAG_UINT(gsttag, attribute, playertag) \
if (gst_tag_list_get_uint(tag_list, gsttag, &v_uint))\
{\
	if(v_uint)\
	{\
		if(gsttag==GST_TAG_BITRATE)\
		{\
			if (player->updated_bitrate_count == 0) \
				mm_attrs_set_int_by_name(attribute, "content_audio_bitrate", v_uint); \
			if (player->updated_bitrate_count<MM_PLAYER_STREAM_COUNT_MAX) \
			{\
				player->bitrate[player->updated_bitrate_count] = v_uint;\
				player->total_bitrate += player->bitrate[player->updated_maximum_bitrate_count]; \
				player->updated_bitrate_count++; \
				mm_attrs_set_int_by_name(attribute, playertag, player->total_bitrate);\
				debug_log ( "update bitrate %d[bps] of stream #%d.\n", v_uint, player->updated_bitrate_count);\
			}\
		}\
		else if (gsttag==GST_TAG_MAXIMUM_BITRATE)\
		{\
			if (player->updated_maximum_bitrate_count<MM_PLAYER_STREAM_COUNT_MAX) \
			{\
				player->maximum_bitrate[player->updated_maximum_bitrate_count] = v_uint;\
				player->total_maximum_bitrate += player->maximum_bitrate[player->updated_maximum_bitrate_count]; \
				player->updated_maximum_bitrate_count++; \
				mm_attrs_set_int_by_name(attribute, playertag, player->total_maximum_bitrate); \
				debug_log ( "update maximum bitrate %d[bps] of stream #%d\n", v_uint, player->updated_maximum_bitrate_count);\
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
		debug_log ( "metainfo year : %s\n", string);\
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
	const GValue *value;

	/* currently not used. but those are needed for above macro */
	//guint64 v_uint64 = 0;
	//gdouble v_double = 0;

	return_val_if_fail( player && msg, FALSE );

	attrs = MMPLAYER_GET_ATTRS(player);

	return_val_if_fail( attrs, FALSE );

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

	if ( mmf_attrs_commit ( attrs ) )
		debug_error("failed to commit.\n");

	gst_tag_list_free(tag_list);

	return TRUE;
}

static void
__mmplayer_gst_rtp_no_more_pads (GstElement *element,  gpointer data)  // @
{
	mm_player_t* player = (mm_player_t*) data;

	debug_fenter();

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
		debug_log("it seems pad caps is directely used for autoplugging. removing fakesink now\n");

		if ( ! __mmplayer_gst_remove_fakesink( player,
			&player->pipeline->mainbin[MMPLAYER_M_SRC_FAKESINK]) );
	}

	/* create dot before error-return. for debugging */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-no-more-pad" );

	/* NOTE : if rtspsrc goes to PLAYING before adding it's src pads, a/v sink elements will
	 * not goes to PLAYING. they will just remain in PAUSED state. simply we are giving
	 * PLAYING state again.
	 */
	__mmplayer_gst_set_state(player,
		player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_PLAYING, TRUE, 5000 );

	player->no_more_pad = TRUE;

	debug_fleave();
}

static gboolean
__mmplayer_gst_remove_fakesink(mm_player_t* player, MMPlayerGstElement* fakesink) // @
{
	GstElement* parent = NULL;

	return_val_if_fail(player && player->pipeline && fakesink, FALSE);

	/* lock */
	g_mutex_lock( player->fsink_lock );

	if ( ! fakesink->gst )
	{
		goto ERROR;
	}

	/* get parent of fakesink */
	parent = (GstElement*)gst_object_get_parent( (GstObject*)fakesink->gst );
	if ( ! parent )
	{
		debug_log("fakesink already removed\n");
		goto ERROR;
	}

	gst_element_set_locked_state( fakesink->gst, TRUE );

	/* setting the state to NULL never returns async
	 * so no need to wait for completion of state transiton
	 */
	if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state (fakesink->gst, GST_STATE_NULL) )
	{
		debug_error("fakesink state change failure!\n");

		/* FIXIT : should I return here? or try to proceed to next? */
		/* return FALSE; */
	}

	/* remove fakesink from it's parent */
	if ( ! gst_bin_remove( GST_BIN( parent ), fakesink->gst ) )
	{
		debug_error("failed to remove fakesink\n");

		gst_object_unref( parent );

		goto ERROR;
	}

	gst_object_unref( parent );

	/* FIXIT : releasing fakesink takes too much time (around 700ms)
	 * we need to figure out the reason why. just for now, fakesink will be released
	 * in __mmplayer_gst_destroy_pipeline()
	 */
	//	gst_object_unref ( fakesink->gst );
	//	fakesink->gst = NULL;

	debug_log("state-holder removed\n");

	gst_element_set_locked_state( fakesink->gst, FALSE );

	g_mutex_unlock( player->fsink_lock );
	return TRUE;

ERROR:
	if ( fakesink->gst )
	{
		gst_element_set_locked_state( fakesink->gst, FALSE );
	}

	g_mutex_unlock( player->fsink_lock );
	return FALSE;
}


static void
__mmplayer_gst_rtp_dynamic_pad (GstElement *element, GstPad *pad, gpointer data) // @
{
	GstPad *sinkpad = NULL;
	GstCaps* caps = NULL;
	GstElement* new_element = NULL;
	enum MainElementID element_id = MMPLAYER_M_NUM;

	mm_player_t* player = (mm_player_t*) data;

	debug_fenter();

	return_if_fail( element && pad );
	return_if_fail(	player &&
					player->pipeline &&
					player->pipeline->mainbin );


	/* payload type is recognizable. increase num_dynamic and wait for sinkbin creation.
	 * num_dynamic_pad will decreased after creating a sinkbin.
	 */
	player->num_dynamic_pad++;
	debug_log("stream count inc : %d\n", player->num_dynamic_pad);

	/* perform autoplugging if dump is disabled */
	if ( PLAYER_INI()->rtsp_do_typefinding )
	{
		/* create typefind */
		new_element = gst_element_factory_make( "typefind", NULL );
		if ( ! new_element )
		{
			debug_error("failed to create typefind\n");
			goto ERROR;
		}

		MMPLAYER_SIGNAL_CONNECT( 	player,
									G_OBJECT(new_element),
									"have-type",
									G_CALLBACK(__mmplayer_typefind_have_type),
									(gpointer)player);

		/* FIXIT : try to remove it */
		player->have_dynamic_pad = FALSE;
	}
	else  /* NOTE : use pad's caps directely. if enabled. what I am assuming is there's no elemnt has dynamic pad */
	{
		debug_log("using pad caps to autopluging instead of doing typefind\n");

		caps = gst_pad_get_caps( pad );

		MMPLAYER_CHECK_NULL( caps );

		/* clear  previous result*/
		player->have_dynamic_pad = FALSE;

		if ( ! __mmplayer_try_to_plug( player, pad, caps ) )
		{
			debug_error("failed to autoplug for caps : %s\n", gst_caps_to_string( caps ) );
			goto ERROR;
		}

		/* check if there's dynamic pad*/
		if( player->have_dynamic_pad )
		{
			debug_error("using pad caps assums there's no dynamic pad !\n");
			debug_error("try with enalbing rtsp_do_typefinding\n");
			goto ERROR;
		}

		gst_caps_unref( caps );
		caps = NULL;
	}

	/* excute new_element if created*/
	if ( new_element )
	{
		debug_log("adding new element to pipeline\n");

		/* set state to READY before add to bin */
		MMPLAYER_ELEMENT_SET_STATE( new_element, GST_STATE_READY );

		/* add new element to the pipeline */
		if ( FALSE == gst_bin_add( GST_BIN(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst), new_element)  )
		{
			debug_error("failed to add autoplug element to bin\n");
			goto ERROR;
		}

		/* get pad from element */
		sinkpad = gst_element_get_static_pad ( GST_ELEMENT(new_element), "sink" );
		if ( !sinkpad )
		{
			debug_error("failed to get sinkpad from autoplug element\n");
			goto ERROR;
		}

		/* link it */
		if ( GST_PAD_LINK_OK != GST_PAD_LINK(pad, sinkpad) )
		{
			debug_error("failed to link autoplug element\n");
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

	debug_fleave();

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


static void
__mmplayer_gst_decode_callback(GstElement *decodebin, GstPad *pad, gboolean last, gpointer data) // @
{
	mm_player_t* player = NULL;
	MMHandleType attrs = 0;
	GstElement* pipeline = NULL;
	GstCaps* caps = NULL;
	GstStructure* str = NULL;
	const gchar* name = NULL;
	GstPad* sinkpad = NULL;
	GstElement* sinkbin = NULL;

	/* check handles */
	player = (mm_player_t*) data;

	return_if_fail( decodebin && pad );
	return_if_fail(player && player->pipeline && player->pipeline->mainbin);

	pipeline = player->pipeline->mainbin[MMPLAYER_M_PIPE].gst;

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("cannot get content attribute\n");
		goto ERROR;
	}

	/* get mimetype from caps */
	caps = gst_pad_get_caps( pad );
	if ( !caps )
	{
		debug_error("cannot get caps from pad.\n");
		goto ERROR;
	}

	str = gst_caps_get_structure( caps, 0 );
	if ( ! str )
	{
		debug_error("cannot get structure from capse.\n");
		goto ERROR;
	}

	name = gst_structure_get_name(str);
	if ( ! name )
	{
		debug_error("cannot get mimetype from structure.\n");
		goto ERROR;
	}

	debug_log("detected mimetype : %s\n", name);

	if (strstr(name, "audio"))
	{
		if (player->pipeline->audiobin == NULL)
		{
			__ta__("__mmplayer_gst_create_audio_pipeline",
				if (MM_ERROR_NONE !=  __mmplayer_gst_create_audio_pipeline(player))
				{
					debug_error("failed to create audiobin. continuing without audio\n");
					goto ERROR;
				}
			)

			sinkbin = player->pipeline->audiobin[MMPLAYER_A_BIN].gst;
			debug_log("creating audiosink bin success\n");
		}
		else
		{
			sinkbin = player->pipeline->audiobin[MMPLAYER_A_BIN].gst;
			debug_log("re-using audiobin\n");
		}

		/* FIXIT : track number shouldn't be hardcoded */
		mm_attrs_set_int_by_name(attrs, "content_audio_track_num", 1);

		player->audiosink_linked  = 1;
		debug_msg("player->audsink_linked set to 1\n");

	}
	else if (strstr(name, "video"))
	{
		if (player->pipeline->videobin == NULL)
		{
			/*	NOTE : not make videobin because application dose not want to play it even though file has video stream.
			*/

			/* get video surface type */
			int surface_type = 0;
			mm_attrs_get_int_by_name (player->attrs, "display_surface_type", &surface_type);
			debug_log("check display surface type attribute: %d", surface_type);
			if (surface_type == MM_DISPLAY_SURFACE_NULL)
			{
				debug_log("not make videobin because it dose not want\n");
				goto ERROR;
			}

			__ta__("__mmplayer_gst_create_video_pipeline",
			if (MM_ERROR_NONE !=  __mmplayer_gst_create_video_pipeline(player, caps, surface_type) )
			{
				debug_error("failed to create videobin. continuing without video\n");
				goto ERROR;
			}
			)

			sinkbin = player->pipeline->videobin[MMPLAYER_V_BIN].gst;
			debug_log("creating videosink bin success\n");
		}
		else
		{
			sinkbin = player->pipeline->videobin[MMPLAYER_V_BIN].gst;
			debug_log("re-using videobin\n");
		}

		/* FIXIT : track number shouldn't be hardcoded */
		mm_attrs_set_int_by_name(attrs, "content_video_track_num", 1);

		player->videosink_linked  = 1;
		debug_msg("player->videosink_linked set to 1\n");

	}
	else if (strstr(name, "text"))
	{
		if (player->pipeline->textbin == NULL)
		{
			__ta__("__mmplayer_gst_create_text_pipeline",
				if (MM_ERROR_NONE !=  __mmplayer_gst_create_text_pipeline(player))
				{
					debug_error("failed to create textbin. continuing without text\n");
					goto ERROR;
				}
			)

			sinkbin = player->pipeline->textbin[MMPLAYER_T_BIN].gst;
			debug_log("creating textink bin success\n");
		}
		else
		{
			sinkbin = player->pipeline->textbin[MMPLAYER_T_BIN].gst;
			debug_log("re-using textbin\n");
		}

		/* FIXIT : track number shouldn't be hardcoded */
		mm_attrs_set_int_by_name(attrs, "content_text_track_num", 1);

		player->textsink_linked  = 1;
		debug_msg("player->textsink_linked set to 1\n");
	}
	else
	{
		debug_warning("unknown type of elementary stream! ignoring it...\n");
		goto ERROR;
	}

	if ( sinkbin )
	{
		/* warm up */
		if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state( sinkbin, GST_STATE_READY ) )
		{
			debug_error("failed to set state(READY) to sinkbin\n");
			goto ERROR;
		}

		/* add */
		if ( FALSE == gst_bin_add( GST_BIN(pipeline), sinkbin ) )
		{
			debug_error("failed to add sinkbin to pipeline\n");
			goto ERROR;
		}

		sinkpad = gst_element_get_static_pad( GST_ELEMENT(sinkbin), "sink" );

		if ( !sinkpad )
		{
			debug_error("failed to get pad from sinkbin\n");
			goto ERROR;
		}

		/* link */
		if ( GST_PAD_LINK_OK != GST_PAD_LINK(pad, sinkpad) )
		{
			debug_error("failed to get pad from sinkbin\n");
			goto ERROR;
		}

		/* run */
		if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state( sinkbin, GST_STATE_PAUSED ) )
		{
			debug_error("failed to set state(PLAYING) to sinkbin\n");
			goto ERROR;
		}

		gst_object_unref( sinkpad );
		sinkpad = NULL;
	}

	/* update track number attributes */
	if ( mmf_attrs_commit ( attrs ) )
		debug_error("failed to commit attrs\n");

	debug_log("linking sink bin success\n");


	/* FIXIT : we cannot hold callback for 'no-more-pad' signal because signal was emitted in
 	 * streaming task. if the task blocked, then buffer will not flow to the next element
 	 * ( autoplugging element ). so this is special hack for streaming. please try to remove it
 	 */
	/* dec stream count. we can remove fakesink if it's zero */
	player->num_dynamic_pad--;

	debug_log("stream count dec : %d (num of dynamic pad)\n", player->num_dynamic_pad);

	if ( ( player->no_more_pad ) && ( player->num_dynamic_pad == 0 ) )
	{
		__mmplayer_pipeline_complete( NULL, player );
	}

ERROR:
	if ( caps )
		gst_caps_unref( caps );

	if ( sinkpad )
		gst_object_unref(GST_OBJECT(sinkpad));

	return;
}

int
_mmplayer_update_video_param(mm_player_t* player) // @
{
	MMHandleType attrs = 0;
	int surface_type = 0;

	debug_fenter();

	/* check video sinkbin is created */
	return_val_if_fail ( player && 
		player->pipeline &&
		player->pipeline->videobin &&
		player->pipeline->videobin[MMPLAYER_V_BIN].gst &&
		player->pipeline->videobin[MMPLAYER_V_SINK].gst, 
		MM_ERROR_PLAYER_NOT_INITIALIZED );

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("cannot get content attribute");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	/* update display surface */
	mm_attrs_get_int_by_name (player->attrs, "display_surface_type", &surface_type);
	debug_log("check display surface type attribute: %d", surface_type);

	/* check video stream callback is used */
	if( player->use_video_stream )
	{
		int rotate, width, height, orientation;

		rotate = width = height = orientation = 0;

		debug_log("using video stream callback with memsink. player handle : [%p]", player);

		mm_attrs_get_int_by_name(attrs, "display_width", &width);
		mm_attrs_get_int_by_name(attrs, "display_height", &height);
		mm_attrs_get_int_by_name(attrs, "display_rotation", &rotate);
		mm_attrs_get_int_by_name(attrs, "display_orientation", &orientation);

		if (rotate < MM_DISPLAY_ROTATION_NONE || rotate > MM_DISPLAY_ROTATION_270)
			rotate = 0;
		else
			rotate *= 90;

		if(orientation == 1)        rotate = 90;
		else if(orientation == 2)   rotate = 180;
		else if(orientation == 3)   rotate = 270;

		if (width)
			g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst, "width", width, NULL);

		if (height)
			g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst, "height", height, NULL);

		g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst, "rotate", rotate,NULL);
		
		return MM_ERROR_NONE;
	}

	/* configuring display */
	switch ( surface_type )
	{
		case MM_DISPLAY_SURFACE_X:
		{
		/* ximagesink or xvimagesink */
			void *xid = NULL;
			int zoom = 0;
			int degree = 0;
			int display_method = 0;
			int roi_x = 0;
			int roi_y = 0;
			int roi_w = 0;
			int roi_h = 0;
			int force_aspect_ratio = 0;

			gboolean visible = TRUE;

			/* common case if using x surface */
			mm_attrs_get_data_by_name(attrs, "display_overlay", &xid);
			if ( xid )
			{
				debug_log("set video param : xid %d", *(int*)xid);
				gst_x_overlay_set_xwindow_id( GST_X_OVERLAY( player->pipeline->videobin[MMPLAYER_V_SINK].gst ), *(int*)xid );
			}
			else
			{
				/* FIXIT : is it error case? */
				debug_warning("still we don't have xid on player attribute. create it's own surface.");
			}

			/* if xvimagesink */
			if (!strcmp(PLAYER_INI()->videosink_element_x,"xvimagesink"))
			{
				mm_attrs_get_int_by_name(attrs, "display_force_aspect_ration", &force_aspect_ratio);
				mm_attrs_get_int_by_name(attrs, "display_zoom", &zoom);
				mm_attrs_get_int_by_name(attrs, "display_rotation", &degree);
				mm_attrs_get_int_by_name(attrs, "display_method", &display_method);
				mm_attrs_get_int_by_name(attrs, "display_roi_x", &roi_x);
				mm_attrs_get_int_by_name(attrs, "display_roi_y", &roi_y);
				mm_attrs_get_int_by_name(attrs, "display_roi_width", &roi_w);
				mm_attrs_get_int_by_name(attrs, "display_roi_height", &roi_h);
				mm_attrs_get_int_by_name(attrs, "display_visible", &visible);

				g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
					"force-aspect-ratio", force_aspect_ratio,
					"zoom", zoom,
					"rotate", degree,
					"handle-events", TRUE,
					"display-geometry-method", display_method,
					"draw-borders", FALSE,
					"dst-roi-x", roi_x,
					"dst-roi-y", roi_y,
					"dst-roi-w", roi_w,
					"dst-roi-h", roi_h,
					"visible", visible,
					NULL );

				debug_log("set video param : zoom %d", zoom);
				debug_log("set video param : rotate %d", degree);
				debug_log("set video param : method %d", display_method);
				debug_log("set video param : dst-roi-x: %d, dst-roi-y: %d, dst-roi-w: %d, dst-roi-h: %d",
								roi_x, roi_y, roi_w, roi_h );
				debug_log("set video param : visible %d", visible);
				debug_log("set video param : force aspect ratio %d", force_aspect_ratio);
			}
		}
		break;
		case MM_DISPLAY_SURFACE_EVAS:
		{
			void *object = NULL;
			int scaling = 0;
			gboolean visible = TRUE;

			/* common case if using evas surface */
			mm_attrs_get_data_by_name(attrs, "display_overlay", &object);
			mm_attrs_get_int_by_name(attrs, "display_visible", &visible);
			mm_attrs_get_int_by_name(attrs, "display_evas_do_scaling", &scaling);
			if (object)
			{
				g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
						"evas-object", object,
						"visible", visible,
						NULL);
				debug_log("set video param : evas-object %x", object);
				debug_log("set video param : visible %d", visible);
			}
			else
			{
				debug_error("no evas object");
				return MM_ERROR_PLAYER_INTERNAL;
			}

			/* if evaspixmapsink */
			if (!strcmp(PLAYER_INI()->videosink_element_evas,"evaspixmapsink"))
			{
				int display_method = 0;
				int roi_x = 0;
				int roi_y = 0;
				int roi_w = 0;
				int roi_h = 0;
				int force_aspect_ratio = 0;
				int origin_size = !scaling;

				mm_attrs_get_int_by_name(attrs, "display_force_aspect_ration", &force_aspect_ratio);
				mm_attrs_get_int_by_name(attrs, "display_method", &display_method);
				mm_attrs_get_int_by_name(attrs, "display_roi_x", &roi_x);
				mm_attrs_get_int_by_name(attrs, "display_roi_y", &roi_y);
				mm_attrs_get_int_by_name(attrs, "display_roi_width", &roi_w);
				mm_attrs_get_int_by_name(attrs, "display_roi_height", &roi_h);

				g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
					"force-aspect-ratio", force_aspect_ratio,
					"origin-size", origin_size,
					"dst-roi-x", roi_x,
					"dst-roi-y", roi_y,
					"dst-roi-w", roi_w,
					"dst-roi-h", roi_h,
					"display-geometry-method", display_method,
					NULL );

				debug_log("set video param : method %d", display_method);
				debug_log("set video param : dst-roi-x: %d, dst-roi-y: %d, dst-roi-w: %d, dst-roi-h: %d",
								roi_x, roi_y, roi_w, roi_h );
				debug_log("set video param : force aspect ratio %d", force_aspect_ratio);
				debug_log("set video param : display_evas_do_scaling %d (origin-size %d)", scaling, origin_size);
			}
		}
		break;
		case MM_DISPLAY_SURFACE_X_EXT:	/* NOTE : this surface type is for the video texture(canvas texture) */
		{
			void *pixmap_id_cb = NULL;
			void *pixmap_id_cb_user_data = NULL;
			int zoom = 0;
			int degree = 0;
			int display_method = 0;
			int roi_x = 0;
			int roi_y = 0;
			int roi_w = 0;
			int roi_h = 0;
			int force_aspect_ratio = 0;
			gboolean visible = TRUE;

			/* if xvimagesink */
			if (strcmp(PLAYER_INI()->videosink_element_x,"xvimagesink"))
			{
				debug_error("videosink is not xvimagesink");
				return MM_ERROR_PLAYER_INTERNAL;
			}

			/* get information from attributes */
			mm_attrs_get_data_by_name(attrs, "display_overlay", &pixmap_id_cb);
			mm_attrs_get_data_by_name(attrs, "display_overlay_user_data", &pixmap_id_cb_user_data);
			mm_attrs_get_int_by_name(attrs, "display_method", &display_method);

			if ( pixmap_id_cb )
			{
				debug_log("set video param : display_overlay(0x%x)", pixmap_id_cb);
				if (pixmap_id_cb_user_data)
				{
					debug_log("set video param : display_overlay_user_data(0x%x)", pixmap_id_cb_user_data);
				}
			}
			else
			{
				debug_error("failed to set pixmap-id-callback");
				return MM_ERROR_PLAYER_INTERNAL;
			}
			debug_log("set video param : method %d", display_method);
			debug_log("set video param : visible %d", visible);

			/* set properties of videosink plugin */
			g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
				"display-geometry-method", display_method,
				"draw-borders", FALSE,
				"visible", visible,
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
	}

	debug_fleave();

	return MM_ERROR_NONE;
}

static int
__mmplayer_gst_element_link_bucket(GList* element_bucket) // @
{
	GList* bucket = element_bucket;
	MMPlayerGstElement* element = NULL;
	MMPlayerGstElement* prv_element = NULL;
	gint successful_link_count = 0;

	debug_fenter();

	return_val_if_fail(element_bucket, -1);

	prv_element = (MMPlayerGstElement*)bucket->data;
	bucket = bucket->next;

	for ( ; bucket; bucket = bucket->next )
	{
		element = (MMPlayerGstElement*)bucket->data;

		if ( element && element->gst )
		{
			if ( GST_ELEMENT_LINK(GST_ELEMENT(prv_element->gst), GST_ELEMENT(element->gst)) )
			{
				debug_log("linking [%s] to [%s] success\n",
					GST_ELEMENT_NAME(GST_ELEMENT(prv_element->gst)),
					GST_ELEMENT_NAME(GST_ELEMENT(element->gst)) );
				successful_link_count ++;
			}
			else
			{
				debug_log("linking [%s] to [%s] failed\n",
					GST_ELEMENT_NAME(GST_ELEMENT(prv_element->gst)),
					GST_ELEMENT_NAME(GST_ELEMENT(element->gst)) );
				return -1;
			}
		}

		prv_element = element;
	}

	debug_fleave();

	return successful_link_count;
}

static int
__mmplayer_gst_element_add_bucket_to_bin(GstBin* bin, GList* element_bucket) // @
{
	GList* bucket = element_bucket;
	MMPlayerGstElement* element = NULL;
	int successful_add_count = 0;

	debug_fenter();

	return_val_if_fail(element_bucket, 0);
	return_val_if_fail(bin, 0);

	for ( ; bucket; bucket = bucket->next )
	{
		element = (MMPlayerGstElement*)bucket->data;

		if ( element && element->gst )
		{
			if( !gst_bin_add(bin, GST_ELEMENT(element->gst)) )
			{
				debug_log("__mmplayer_gst_element_link_bucket : Adding element [%s]  to bin [%s] failed\n",
					GST_ELEMENT_NAME(GST_ELEMENT(element->gst)),
					GST_ELEMENT_NAME(GST_ELEMENT(bin) ) );
				return 0;
			}
			successful_add_count ++;
		}
	}

	debug_fleave();

	return successful_add_count;
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
	debug_critical("failed to create %s \n", x_factory);\
	goto ERROR;\
}\

/* macro for code readability. just for sinkbin-creation functions */
#define MMPLAYER_CREATE_ELEMENT(x_bin, x_id, x_factory, x_name, x_add_bucket) \
do \
{ \
	x_bin[x_id].id = x_id;\
	x_bin[x_id].gst = gst_element_factory_make(x_factory, x_name);\
	if ( ! x_bin[x_id].gst )\
	{\
		debug_critical("failed to create %s \n", x_factory);\
		goto ERROR;\
	}\
	if ( x_add_bucket )\
		element_bucket = g_list_append(element_bucket, &x_bin[x_id]);\
} while(0);


/**
  * AUDIO PIPELINE 
  * - Local playback 	: audioconvert !volume ! capsfilter ! dnse ! audiosink
  * - Streaming 		: audioconvert !volume ! audiosink
  * - PCM extraction 	: audioconvert ! audioresample ! capsfilter ! fakesink 
  */
static int
__mmplayer_gst_create_audio_pipeline(mm_player_t* player)
{
	MMPlayerGstElement* first_element = NULL;
	MMPlayerGstElement* audiobin = NULL;
	MMHandleType attrs = 0;
	GstPad *pad = NULL;
	GstPad *ghostpad = NULL;
	GList* element_bucket = NULL;
	char *device_name = NULL;
	gboolean link_audio_sink_now = TRUE;
	int i =0;

	debug_fenter();

	return_val_if_fail( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* alloc handles */
	audiobin = (MMPlayerGstElement*)g_malloc0(sizeof(MMPlayerGstElement) * MMPLAYER_A_NUM);
	audiobin = (MMPlayerGstElement*)g_malloc0(sizeof(MMPlayerGstElement) * MMPLAYER_A_NUM);
	if ( ! audiobin )
	{
		debug_error("failed to allocate memory for audiobin\n");
		return MM_ERROR_PLAYER_NO_FREE_SPACE;
	}

	attrs = MMPLAYER_GET_ATTRS(player);

	/* create bin */
	audiobin[MMPLAYER_A_BIN].id = MMPLAYER_A_BIN;
	audiobin[MMPLAYER_A_BIN].gst = gst_bin_new("audiobin");
	if ( !audiobin[MMPLAYER_A_BIN].gst )
	{
		debug_critical("failed to create audiobin\n");
		goto ERROR;
	}

	/* take it */
	player->pipeline->audiobin = audiobin;

	player->is_sound_extraction = __mmplayer_can_extract_pcm(player);

	/* Adding audiotp plugin for reverse trickplay feature */
	MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_TP, "audiotp", "audiotrickplay", TRUE);

	/* converter */
	MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_CONV, "audioconvert", "audioconverter", TRUE);

	if ( ! player->is_sound_extraction )
	{
		GstCaps* caps = NULL;

		/* for logical volume control */
		MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_VOL, "volume", "volume", TRUE);
		g_object_set(G_OBJECT (audiobin[MMPLAYER_A_VOL].gst), "volume", player->sound.volume, NULL);

		if (player->sound.mute)
		{
			debug_log("mute enabled\n");
			g_object_set(G_OBJECT (audiobin[MMPLAYER_A_VOL].gst), "mute", player->sound.mute, NULL);
		}

		/*capsfilter */
		MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_CAPS_DEFAULT, "capsfilter", "audiocapsfilter", TRUE);

		caps = gst_caps_from_string(		"audio/x-raw-int, "
										"endianness = (int) LITTLE_ENDIAN, "
										"signed = (boolean) true, "
										"width = (int) 16, "
										"depth = (int) 16" 	);

		g_object_set (GST_ELEMENT(audiobin[MMPLAYER_A_CAPS_DEFAULT].gst), "caps", caps, NULL );

		gst_caps_unref( caps );

		/* audio filter. if enabled */
		if ( PLAYER_INI()->use_audio_filter_preset || PLAYER_INI()->use_audio_filter_custom )
		{
			MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_FILTER, "soundalive", "audiofilter", TRUE);
		}

		/* create audio sink */
		MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_SINK, PLAYER_INI()->name_of_audiosink,
			"audiosink", link_audio_sink_now);

		/* sync on */
		if (MMPLAYER_IS_RTSP_STREAMING(player))
			g_object_set (G_OBJECT (audiobin[MMPLAYER_A_SINK].gst), "sync", FALSE, NULL); 	/* sync off */
		else
			g_object_set (G_OBJECT (audiobin[MMPLAYER_A_SINK].gst), "sync", TRUE, NULL); 	/* sync on */

		/* qos on */
		g_object_set (G_OBJECT (audiobin[MMPLAYER_A_SINK].gst), "qos", TRUE, NULL); 	/* qos on */

		/* FIXIT : using system clock. isn't there another way? */
		g_object_set (G_OBJECT (audiobin[MMPLAYER_A_SINK].gst), "provide-clock", PLAYER_INI()->provide_clock,  NULL);

		__mmplayer_add_sink( player, audiobin[MMPLAYER_A_SINK].gst );

	        if(player->audio_buffer_cb)
	        {
	            g_object_set(audiobin[MMPLAYER_A_SINK].gst, "audio-handle", player->audio_buffer_cb_user_param, NULL);
	            g_object_set(audiobin[MMPLAYER_A_SINK].gst, "audio-callback", player->audio_buffer_cb, NULL);
	        }

		if ( g_strrstr(PLAYER_INI()->name_of_audiosink, "avsysaudiosink") )
		{
			gint volume_type = 0;
			gint audio_route = 0;
			gint sound_priority = FALSE;
			gint is_spk_out_only = 0;

			/* set volume table
			 * It should be set after player creation through attribute.
			 * But, it can not be changed during playing.
			 */
			mm_attrs_get_int_by_name(attrs, "sound_volume_type", &volume_type);
			mm_attrs_get_int_by_name(attrs, "sound_route", &audio_route);
			mm_attrs_get_int_by_name(attrs, "sound_priority", &sound_priority);
			mm_attrs_get_int_by_name(attrs, "sound_spk_out_only", &is_spk_out_only);

			/* hook sound_type if emergency case */
			if ( player->sm.event == ASM_EVENT_EMERGENCY)
			{
				debug_log ("This is emergency session, hook sound_type from [%d] to [%d]\n", volume_type, MM_SOUND_VOLUME_TYPE_EMERGENCY);
				volume_type = MM_SOUND_VOLUME_TYPE_EMERGENCY;
			}

			g_object_set(audiobin[MMPLAYER_A_SINK].gst,
								"volumetype", volume_type,
								"audio-route", audio_route,
								"priority", sound_priority,
								"user-route", is_spk_out_only,
								NULL);

			debug_log("audiosink property status...volume type:%d, route:%d, priority=%d, user-route=%d\n",
				volume_type, audio_route, sound_priority, is_spk_out_only);
		}

		/* Antishock can be enabled when player is resumed by soundCM.
		 * But, it's not used in MMS, setting and etc.
		 * Because, player start seems like late.
		 */
		__mmplayer_set_antishock( player , FALSE );
	}
	else // pcm extraction only and no sound output 
	{
		int dst_samplerate = 0;
		int dst_channels = 0;
		int dst_depth = 0;
		char *caps_type = NULL;
		GstCaps* caps = NULL;

		/* resampler */
		MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_RESAMPLER, "audioresample", "resampler", TRUE);

		/* get conf. values */
		mm_attrs_multiple_get(player->attrs, 
						NULL,
						"pcm_extraction_samplerate", &dst_samplerate,
						"pcm_extraction_channels", &dst_channels,
						"pcm_extraction_depth", &dst_depth,
						NULL);
		/* capsfilter */
		MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_CAPS_DEFAULT, "capsfilter", "audiocapsfilter", TRUE);

		caps = gst_caps_new_simple ("audio/x-raw-int",
					       "rate", G_TYPE_INT, dst_samplerate,
					       "channels", G_TYPE_INT, dst_channels,
					       "depth", G_TYPE_INT, dst_depth,
						NULL);

		caps_type = gst_caps_to_string(caps);
		debug_log("resampler new caps : %s\n", caps_type);

			g_object_set (GST_ELEMENT(audiobin[MMPLAYER_A_CAPS_DEFAULT].gst), "caps", caps, NULL );

			/* clean */
			gst_caps_unref( caps );
			MMPLAYER_FREEIF( caps_type );

		/* fake sink */
		MMPLAYER_CREATE_ELEMENT(audiobin, MMPLAYER_A_SINK, "fakesink", "fakesink", TRUE);

		/* set sync */
		g_object_set (G_OBJECT (audiobin[MMPLAYER_A_SINK].gst), "sync", FALSE, NULL);

		__mmplayer_add_sink( player, audiobin[MMPLAYER_A_SINK].gst );
	}

	/* adding created elements to bin */
	debug_log("adding created elements to bin\n");
	if( !__mmplayer_gst_element_add_bucket_to_bin( GST_BIN(audiobin[MMPLAYER_A_BIN].gst), element_bucket ))
	{
		debug_error("failed to add elements\n");
		goto ERROR;
	}

	/* linking elements in the bucket by added order. */
	debug_log("Linking elements in the bucket by added order.\n");
	if ( __mmplayer_gst_element_link_bucket(element_bucket) == -1 )
	{
		debug_error("failed to link elements\n");
		goto ERROR;
	}

    	/* get first element's sinkpad for creating ghostpad */
    	first_element = (MMPlayerGstElement *)element_bucket->data;

    	pad = gst_element_get_static_pad(GST_ELEMENT(first_element->gst), "sink");
	if ( ! pad )
	{
		debug_error("failed to get pad from first element of audiobin\n");
		goto ERROR;
	}

	ghostpad = gst_ghost_pad_new("sink", pad);
	if ( ! ghostpad )
	{
		debug_error("failed to create ghostpad\n");
		goto ERROR;
	}

	if ( FALSE == gst_element_add_pad(audiobin[MMPLAYER_A_BIN].gst, ghostpad) )
	{
		debug_error("failed to add ghostpad to audiobin\n");
		goto ERROR;
	}

	gst_object_unref(pad);

	if ( !player->bypass_sound_effect && (PLAYER_INI()->use_audio_filter_preset || PLAYER_INI()->use_audio_filter_custom) )
	{
		if ( player->audio_filter_info.filter_type == MM_AUDIO_FILTER_TYPE_PRESET )
		{
			if (!_mmplayer_sound_filter_preset_apply(player, player->audio_filter_info.preset))
			{
				debug_msg("apply sound effect(preset:%d) setting success\n",player->audio_filter_info.preset);
			}
		}
		else if ( player->audio_filter_info.filter_type == MM_AUDIO_FILTER_TYPE_CUSTOM )
		{
			if (!_mmplayer_sound_filter_custom_apply(player))
			{
				debug_msg("apply sound effect(custom) setting success\n");
			}
		}
	}

	/* done. free allocated variables */
	MMPLAYER_FREEIF( device_name );
	g_list_free(element_bucket);

	mm_attrs_set_int_by_name(attrs, "content_audio_found", TRUE);
	if ( mmf_attrs_commit ( attrs ) ) /* return -1 if error */
		debug_error("failed to commit attribute ""content_audio_found"".\n");

	debug_fleave();

	return MM_ERROR_NONE;

ERROR:

	debug_log("ERROR : releasing audiobin\n");

	MMPLAYER_FREEIF( device_name );

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

static gboolean
__mmplayer_audio_stream_probe (GstPad *pad, GstBuffer *buffer, gpointer u_data)
{
	mm_player_t* player = (mm_player_t*) u_data;
	gint size;
	guint8 *data;

	data = GST_BUFFER_DATA(buffer);
	size = GST_BUFFER_SIZE(buffer);

	if (player->audio_stream_cb && size && data)
		player->audio_stream_cb((void *)data, size, player->audio_stream_cb_user_param);

	return TRUE;
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
  * - evas surface  (arm) : ffmpegcolorspace ! evasimagesink
  * - evas surface  (x86) : videoconvertor ! evasimagesink
  */
static int
__mmplayer_gst_create_video_pipeline(mm_player_t* player, GstCaps* caps, MMDisplaySurfaceType surface_type)
{
	GstPad *pad = NULL;
	MMHandleType attrs;
	GList*element_bucket = NULL;
	MMPlayerGstElement* first_element = NULL;
	MMPlayerGstElement* videobin = NULL;
	gchar* vconv_factory = NULL;
	gchar *videosink_element = NULL;

	debug_fenter();

	return_val_if_fail(player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED);

	/* alloc handles */
    	videobin = (MMPlayerGstElement*)g_malloc0(sizeof(MMPlayerGstElement) * MMPLAYER_V_NUM);
    	if ( !videobin )
    		return MM_ERROR_PLAYER_NO_FREE_SPACE;

	player->pipeline->videobin = videobin;

   	attrs = MMPLAYER_GET_ATTRS(player);

	/* create bin */
	videobin[MMPLAYER_V_BIN].id = MMPLAYER_V_BIN;
	videobin[MMPLAYER_V_BIN].gst = gst_bin_new("videobin");
	if ( !videobin[MMPLAYER_V_BIN].gst )
	{
		debug_critical("failed to create videobin");
		goto ERROR;
	}

    	if( player->use_video_stream ) // video stream callack, so send raw video data to application
    	{
		GstStructure *str = NULL;
		guint32 fourcc = 0;
		gint ret = 0;

		debug_log("using memsink\n");

		/* first, create colorspace convert */
		if (strlen(PLAYER_INI()->name_of_video_converter) > 0)
		{
				vconv_factory = PLAYER_INI()->name_of_video_converter;
		}

		if (vconv_factory)
		{
			MMPLAYER_CREATE_ELEMENT(videobin, MMPLAYER_V_CONV, vconv_factory, "video converter", TRUE);
		}
		
		/* then, create video scale to resize if needed */
		str = gst_caps_get_structure (caps, 0);

		if ( ! str )
		{
			debug_error("cannot get structure\n");
			goto ERROR;
		}

		MMPLAYER_LOG_GST_CAPS_TYPE(caps);

		ret = gst_structure_get_fourcc (str, "format", &fourcc);

		if ( !ret )
			debug_log("not fixed format at this point, and not consider this case\n")

		/* NOTE :  if the width of I420 format is not multiple of 8, it should be resize before colorspace conversion.
		  * so, video scale is required for this case only.
		  */
		if ( GST_MAKE_FOURCC ('I', '4', '2', '0') == fourcc )
		{
			gint width = 0; 			//width of video
			gint height = 0;			//height of video
			gint framerate_n = 0;		//numerator of frame rate
			gint framerate_d = 0;		//denominator of frame rate
			GstCaps* video_caps = NULL;
			const GValue *fps = NULL;

			/* video scale  */
			MMPLAYER_CREATE_ELEMENT(videobin, MMPLAYER_V_SCALE, "videoscale", "videoscale", TRUE);

			/*to limit width as multiple of 8 */
			MMPLAYER_CREATE_ELEMENT(videobin, MMPLAYER_V_CAPS, "capsfilter", "videocapsfilter", TRUE);

			/* get video stream caps parsed by demuxer */
			str = gst_caps_get_structure (player->v_stream_caps, 0);
			if ( ! str )
			{
				debug_error("cannot get structure\n");
				goto ERROR;
			}

			/* check the width if it's a multiple of 8 or not */
			ret = gst_structure_get_int (str, "width", &width);
			if ( ! ret )
			{
				debug_error("cannot get width\n");
				goto ERROR;
			}
			width = GST_ROUND_UP_8(width);

			ret = gst_structure_get_int(str, "height", &height);
			if ( ! ret )
			{
				debug_error("cannot get height\n");
				goto ERROR;
			}

			fps = gst_structure_get_value (str, "framerate");
			if ( ! fps )
			{
				debug_error("cannot get fps\n");
				goto ERROR;
			}
			framerate_n = gst_value_get_fraction_numerator (fps);
			framerate_d = gst_value_get_fraction_denominator (fps);

			video_caps = gst_caps_new_simple( "video/x-raw-yuv",
											"format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('I', '4', '2', '0'),
											"width", G_TYPE_INT, width,
											"height", G_TYPE_INT, height,
											"framerate", GST_TYPE_FRACTION, framerate_n, framerate_d,
											NULL);

			g_object_set (GST_ELEMENT(videobin[MMPLAYER_V_CAPS].gst), "caps", video_caps, NULL );

			gst_caps_unref( video_caps );
		}

		/* finally, create video sink. its oupput should be BGRX8888 for application like cario surface. */
		MMPLAYER_CREATE_ELEMENT(videobin, MMPLAYER_V_SINK, "avsysmemsink", "videosink", TRUE);

		MMPLAYER_SIGNAL_CONNECT( player,
									 videobin[MMPLAYER_V_SINK].gst,
									 "video-stream",
									 G_CALLBACK(__mmplayer_videostream_cb),
									 player );
	}
    	else // render video data using sink pugin like xvimagesink
	{
		debug_log("using videosink");
		
		/*set video converter */
		if (strlen(PLAYER_INI()->name_of_video_converter) > 0)
		{
			vconv_factory = PLAYER_INI()->name_of_video_converter;
			if (vconv_factory)
			{
				MMPLAYER_CREATE_ELEMENT(videobin, MMPLAYER_V_CONV, vconv_factory, "video converter", TRUE);
				debug_log("using video converter: %s", vconv_factory);
			}
		}

		/* videoscaler */ /* NOTE : ini parsing method seems to be more suitable rather than define method */
		#if !defined(__arm__)
		MMPLAYER_CREATE_ELEMENT(videobin, MMPLAYER_V_SCALE, "videoscale", "videoscaler", TRUE);
		#endif

		/* set video sink */
		switch (surface_type)
		{
		case MM_DISPLAY_SURFACE_X:
			if (strlen(PLAYER_INI()->videosink_element_x) > 0)
				videosink_element = PLAYER_INI()->videosink_element_x;
			else
				goto ERROR;
			break;
		case MM_DISPLAY_SURFACE_EVAS:
			if (strlen(PLAYER_INI()->videosink_element_evas) > 0)
				videosink_element = PLAYER_INI()->videosink_element_evas;
			else
				goto ERROR;
			break;
		case MM_DISPLAY_SURFACE_X_EXT:
		{
			void *pixmap_id_cb = NULL;
			mm_attrs_get_data_by_name(attrs, "display_overlay", &pixmap_id_cb);
			if (pixmap_id_cb) /* this is for the video textue(canvas texture) */
			{
				videosink_element = PLAYER_INI()->videosink_element_x;
				debug_warning("video texture usage");
			}
			else
			{
				debug_error("something wrong.. callback function for getting pixmap id is null");
				goto ERROR;
			}
			break;
		}
		case MM_DISPLAY_SURFACE_NULL:
			if (strlen(PLAYER_INI()->videosink_element_fake) > 0)
				videosink_element = PLAYER_INI()->videosink_element_fake;
			else
				goto ERROR;
			break;
		default:
			debug_error("unidentified surface type");
			goto ERROR;
		}

		MMPLAYER_CREATE_ELEMENT(videobin, MMPLAYER_V_SINK, videosink_element, videosink_element, TRUE);
		debug_log("selected videosink name: %s", videosink_element);
	}

	if ( _mmplayer_update_video_param(player) != MM_ERROR_NONE)
		goto ERROR;

	/* qos on */
	g_object_set (G_OBJECT (videobin[MMPLAYER_V_SINK].gst), "qos", TRUE, NULL);

	/* store it as it's sink element */
	__mmplayer_add_sink( player, videobin[MMPLAYER_V_SINK].gst );

	/* adding created elements to bin */
	if( ! __mmplayer_gst_element_add_bucket_to_bin(GST_BIN(videobin[MMPLAYER_V_BIN].gst), element_bucket) )
	{
		debug_error("failed to add elements\n");
		goto ERROR;
	}

	/* Linking elements in the bucket by added order */
	if ( __mmplayer_gst_element_link_bucket(element_bucket) == -1 )
	{
		debug_error("failed to link elements\n");
		goto ERROR;
	}

	/* get first element's sinkpad for creating ghostpad */
	first_element = (MMPlayerGstElement *)element_bucket->data;
	if ( !first_element )
	{
		debug_error("failed to get first element from bucket\n");
		goto ERROR;
	}

	pad = gst_element_get_static_pad(GST_ELEMENT(first_element->gst), "sink");
	if ( !pad )
	{
		debug_error("failed to get pad from first element\n");
		goto ERROR;
	}

	/* create ghostpad */
	if (FALSE == gst_element_add_pad(videobin[MMPLAYER_V_BIN].gst, gst_ghost_pad_new("sink", pad)))
	{
		debug_error("failed to add ghostpad to videobin\n");
		goto ERROR;
	}
	gst_object_unref(pad);

	/* done. free allocated variables */
	g_list_free(element_bucket);

	mm_attrs_set_int_by_name(attrs, "content_video_found", TRUE);
	if ( mmf_attrs_commit ( attrs ) ) /* return -1 if error */
		debug_error("failed to commit attribute ""content_video_found"".\n");

	debug_fleave();

	return MM_ERROR_NONE;

ERROR:
	debug_error("ERROR : releasing videobin\n");

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

static int 		__mmplayer_gst_create_text_pipeline(mm_player_t* player)
{
	MMPlayerGstElement* first_element = NULL;
	MMPlayerGstElement* textbin = NULL;
	GList* element_bucket = NULL;
	GstPad *pad = NULL;
	GstPad *ghostpad = NULL;
	gint i = 0;

	debug_fenter();

	return_val_if_fail( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* alloc handles */
	textbin = (MMPlayerGstElement*)g_malloc0(sizeof(MMPlayerGstElement) * MMPLAYER_T_NUM);
	if ( ! textbin )
	{
		debug_error("failed to allocate memory for textbin\n");
		return MM_ERROR_PLAYER_NO_FREE_SPACE;
	}

	/* create bin */
	textbin[MMPLAYER_T_BIN].id = MMPLAYER_T_BIN;
	textbin[MMPLAYER_T_BIN].gst = gst_bin_new("textbin");
	if ( !textbin[MMPLAYER_T_BIN].gst )
	{
		debug_critical("failed to create textbin\n");
		goto ERROR;
	}

	/* take it */
	player->pipeline->textbin = textbin;

	/* fakesink */
	MMPLAYER_CREATE_ELEMENT(textbin, MMPLAYER_T_SINK, "fakesink", "text_sink", TRUE);

	g_object_set (G_OBJECT (textbin[MMPLAYER_T_SINK].gst), "sync", TRUE, NULL);
	g_object_set (G_OBJECT (textbin[MMPLAYER_T_SINK].gst), "async", FALSE, NULL);
	g_object_set (G_OBJECT (textbin[MMPLAYER_T_SINK].gst), "signal-handoffs", TRUE, NULL);

	MMPLAYER_SIGNAL_CONNECT( player,
							G_OBJECT(textbin[MMPLAYER_T_SINK].gst),
							"handoff",
							G_CALLBACK(__mmplayer_update_subtitle),
							(gpointer)player );

	__mmplayer_add_sink (player, GST_ELEMENT(textbin[MMPLAYER_T_SINK].gst));

	/* adding created elements to bin */
	debug_log("adding created elements to bin\n");
	if( !__mmplayer_gst_element_add_bucket_to_bin( GST_BIN(textbin[MMPLAYER_T_BIN].gst), element_bucket ))
	{
		debug_error("failed to add elements\n");
		goto ERROR;
	}

	/* linking elements in the bucket by added order. */
	debug_log("Linking elements in the bucket by added order.\n");
	if ( __mmplayer_gst_element_link_bucket(element_bucket) == -1 )
	{
		debug_error("failed to link elements\n");
		goto ERROR;
	}

    	/* get first element's sinkpad for creating ghostpad */
    	first_element = (MMPlayerGstElement *)element_bucket->data;

    	pad = gst_element_get_static_pad(GST_ELEMENT(first_element->gst), "sink");
	if ( ! pad )
	{
		debug_error("failed to get pad from first element of textbin\n");
		goto ERROR;
	}

	ghostpad = gst_ghost_pad_new("sink", pad);
	if ( ! ghostpad )
	{
		debug_error("failed to create ghostpad\n");
		goto ERROR;
	}

	if ( FALSE == gst_element_add_pad(textbin[MMPLAYER_T_BIN].gst, ghostpad) )
	{
		debug_error("failed to add ghostpad to textbin\n");
		goto ERROR;
	}

	gst_object_unref(pad);


	/* done. free allocated variables */
	g_list_free(element_bucket);

	debug_fleave();

	return MM_ERROR_NONE;

ERROR:

	debug_log("ERROR : releasing textbin\n");

	if ( pad )
		gst_object_unref(GST_OBJECT(pad));

	if ( ghostpad )
		gst_object_unref(GST_OBJECT(ghostpad));

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
__mmplayer_gst_create_subtitle_pipeline(mm_player_t* player)
{
	MMPlayerGstElement* subtitlebin = NULL;
	MMHandleType attrs = 0;
	gchar *subtitle_uri =NULL;
	GList*element_bucket = NULL;

	#define USE_MESSAGE_FOR_PLAYING_SUBTITLE 
#ifndef USE_MESSAGE_FOR_PLAYING_SUBTITLE
	void *xid = NULL;
	gint width =0, height = 0;
	gboolean silent=FALSE;
#endif

	debug_fenter();

	/* get mainbin */
	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("cannot get content attribute\n");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	mm_attrs_get_string_by_name ( attrs, "subtitle_uri", &subtitle_uri );
	if ( !subtitle_uri || strlen(subtitle_uri) < 1)
	{
		debug_error("subtitle uri is not proper filepath.\n");
		return MM_ERROR_PLAYER_INVALID_URI;	
	}
	debug_log("subtitle file path is [%s].\n", subtitle_uri);


	/* alloc handles */
    	subtitlebin = (MMPlayerGstElement*)g_malloc0(sizeof(MMPlayerGstElement) * MMPLAYER_SUB_NUM);
    	if ( !subtitlebin )
    	{
		debug_error("failed to allocate memory\n");
    		return MM_ERROR_PLAYER_NO_FREE_SPACE;
	}

	/* create bin */
    	subtitlebin[MMPLAYER_SUB_PIPE].id = MMPLAYER_SUB_PIPE;
    	subtitlebin[MMPLAYER_SUB_PIPE].gst = gst_pipeline_new("subtitlebin");
	if ( !subtitlebin[MMPLAYER_SUB_PIPE].gst )
	{
		debug_error("failed to create text pipeline\n");
		goto ERROR;
	}
	player->pipeline->subtitlebin = subtitlebin;

	/* create the text file source */
	MMPLAYER_CREATE_ELEMENT(subtitlebin, MMPLAYER_SUB_SRC, "filesrc", "subtitle_source", TRUE);
	g_object_set(G_OBJECT (subtitlebin[MMPLAYER_SUB_SRC].gst), "location", subtitle_uri, NULL);

	/* queue */
	MMPLAYER_CREATE_ELEMENT(subtitlebin, MMPLAYER_SUB_QUEUE, "queue", NULL, TRUE);

	/* subparse */
	MMPLAYER_CREATE_ELEMENT(subtitlebin, MMPLAYER_SUB_SUBPARSE, "subparse", "subtitle_parser", TRUE);

#ifndef USE_MESSAGE_FOR_PLAYING_SUBTITLE
	/* textrender */
	MMPLAYER_CREATE_ELEMENT(subtitlebin, MMPLAYER_SUB_TEXTRENDER, "textrender", "subtitle_render", TRUE);

	mm_attrs_get_int_by_name(attrs,"width", &width);
	mm_attrs_get_int_by_name(attrs,"height", &height);
	mm_attrs_get_int_by_name(attrs,"silent", &silent);
	g_object_set ( G_OBJECT (subtitlebin[MMPLAYER_SUB_TEXTRENDER].gst),"width", width, NULL);
	g_object_set ( G_OBJECT (subtitlebin[MMPLAYER_SUB_TEXTRENDER].gst),"height", height, NULL);
	g_object_set ( G_OBJECT (subtitlebin[MMPLAYER_SUB_TEXTRENDER].gst),"silent", silent, NULL);

	debug_log ( "subtitle winow size is [%dX%d].\n", width, height );
	debug_log ( "subtitle silent is [%d].\n", silent );

	/* converter1 */
	MMPLAYER_CREATE_ELEMENT(subtitlebin, MMPLAYER_SUB_CONV1, "ffmpegcolorspace", "subtitle_converter1", TRUE);

	/* videofliper */
	MMPLAYER_CREATE_ELEMENT(subtitlebin, MMPLAYER_SUB_FLIP, "videoflip", "subtitle_fliper", TRUE);

	/* converter2 */
	MMPLAYER_CREATE_ELEMENT(subtitlebin, MMPLAYER_SUB_CONV2, "ffmpegcolorspace", "subtitle_converter2", TRUE);

	/* text sink */
	MMPLAYER_CREATE_ELEMENT(subtitlebin, MMPLAYER_SUB_SINK, "ximagesink", "subtitle_sink", TRUE);

	mm_attrs_get_data_by_name(attrs, "xid", &xid);
	if ( xid )
	{
		debug_log("setting subtitle xid = %d\n", *(int*)xid);
		gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(subtitlebin[MMPLAYER_SUB_SINK].gst), *(int*)xid);
	}
	else
	{
	      	/* FIXIT : is it error case? */
	        debug_warning("still we don't have xid on player attribute. create it's own surface.\n");
	}
#else
	/* text sink */
	MMPLAYER_CREATE_ELEMENT(subtitlebin, MMPLAYER_SUB_SINK, "fakesink", "subtitle_sink", TRUE);

	g_object_set (G_OBJECT (subtitlebin[MMPLAYER_SUB_SINK].gst), "sync", TRUE, NULL);
	g_object_set (G_OBJECT (subtitlebin[MMPLAYER_SUB_SINK].gst), "async", FALSE, NULL);
	g_object_set (G_OBJECT (subtitlebin[MMPLAYER_SUB_SINK].gst), "signal-handoffs", TRUE, NULL);

	MMPLAYER_SIGNAL_CONNECT( player,
							G_OBJECT(subtitlebin[MMPLAYER_SUB_SINK].gst),
							"handoff",
							G_CALLBACK(__mmplayer_update_subtitle),
							(gpointer)player );
#endif

    	/* adding created elements to bin */
    	if( ! __mmplayer_gst_element_add_bucket_to_bin(GST_BIN(subtitlebin[MMPLAYER_SUB_PIPE].gst), element_bucket) )
    	{
		debug_error("failed to add elements\n");
		goto ERROR;
    	}

    	/* Linking elements in the bucket by added order */
    	if ( __mmplayer_gst_element_link_bucket(element_bucket) == -1 )
    	{
		debug_error("failed to link elements\n");
		goto ERROR;
    	}

	/* done. free allocated variables */
    	g_list_free(element_bucket);

	player->play_subtitle = TRUE;

	debug_fleave();
	
    	return MM_ERROR_NONE;


ERROR:
	debug_error("ERROR : releasing text pipeline\n");

	g_list_free( element_bucket );

	/* release subtitlebin with it's childs */
	if ( subtitlebin[MMPLAYER_SUB_PIPE].gst )
	{
		gst_object_unref(GST_OBJECT(subtitlebin[MMPLAYER_SUB_PIPE].gst));
	}

	MMPLAYER_FREEIF( subtitlebin );

	player->pipeline->subtitlebin = NULL;

	return MM_ERROR_PLAYER_INTERNAL;
}

gboolean
__mmplayer_update_subtitle( GstElement* object, GstBuffer *buffer, GstPad *pad, gpointer data)
{
	mm_player_t* player = (mm_player_t*) data;
	MMMessageParamType msg = {0, };
  	GstClockTime duration = 0;
	guint8 *text = NULL;
	gboolean ret = TRUE;

	debug_fenter();

	return_val_if_fail ( player, FALSE );
	return_val_if_fail ( buffer, FALSE );

	text = GST_BUFFER_DATA(buffer);
  	duration = GST_BUFFER_DURATION(buffer);

	if ( player->is_subtitle_off )
	{
		debug_log("subtitle is OFF.\n" );
		return TRUE;
	}

	if ( !text )
	{
		debug_log("There is no subtitle to be displayed.\n" );
		return TRUE;
	}

	msg.data = (void *) text;
	msg.subtitle.duration = GST_TIME_AS_MSECONDS(duration);

	debug_warning("update subtitle : [%ld msec] %s\n'", msg.subtitle.duration, (char*)msg.data );

	MMPLAYER_POST_MSG( player, MM_MESSAGE_UPDATE_SUBTITLE, &msg );

	debug_fleave();

	return ret;
}


static int 	__gst_adjust_subtitle_position(mm_player_t* player, int format, int position)
{
	GstEvent* event = NULL;
	gint64 current_pos = 0;
	gint64 adusted_pos = 0;
	gboolean ret = TRUE;

	debug_fenter();

	/* check player and subtitlebin are created */
	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( MMPLAYER_PLAY_SUBTITLE(player),	MM_ERROR_PLAYER_NOT_INITIALIZED );

	if (position == 0)
	{
		debug_log("adjusted values is 0, no need to adjust subtitle position.\n");
		return MM_ERROR_NONE;
	}

	switch (format)
	{
		case MM_PLAYER_POS_FORMAT_TIME:
		{
			GstFormat fmt = GST_FORMAT_TIME;
			/* check current postion */
			ret = gst_element_query_position( GST_ELEMENT(player->pipeline->subtitlebin[MMPLAYER_SUB_PIPE].gst), &fmt, &current_pos );
			if ( !ret )
			{
				debug_warning("fail to query current postion.\n");
				return MM_ERROR_PLAYER_SEEK;
			}
			else
			{
				adusted_pos = current_pos + ((gint64)position * G_GINT64_CONSTANT(1000000));
				if (adusted_pos < 0)
					adusted_pos = G_GINT64_CONSTANT(0);
				debug_log("adjust subtitle postion : %lu -> %lu [msec]\n", GST_TIME_AS_MSECONDS(current_pos), GST_TIME_AS_MSECONDS(adusted_pos));
			}

			event = gst_event_new_seek (1.0, 	GST_FORMAT_TIME,
					( GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE ),
					GST_SEEK_TYPE_SET, adusted_pos,
					GST_SEEK_TYPE_SET, -1);
		}
		break;

		case MM_PLAYER_POS_FORMAT_PERCENT:
		{
			debug_warning("percent format is not supported yet.\n");
			return MM_ERROR_INVALID_ARGUMENT;
		}
		break;

		default:
		{
			debug_warning("invalid format.\n");
			return MM_ERROR_INVALID_ARGUMENT;
		}
	}

	/* keep ref to the event */
	gst_event_ref (event);

	debug_log("sending event[%s] to sink element [%s]\n",
			GST_EVENT_TYPE_NAME(event), GST_ELEMENT_NAME(player->pipeline->subtitlebin[MMPLAYER_SUB_SINK].gst) );

	if ( ret = gst_element_send_event (player->pipeline->subtitlebin[MMPLAYER_SUB_SINK].gst, event) )
	{
		debug_log("sending event[%s] to sink element [%s] success!\n",
			GST_EVENT_TYPE_NAME(event), GST_ELEMENT_NAME(player->pipeline->subtitlebin[MMPLAYER_SUB_SINK].gst) );
	}

	/* unref to the event */
	gst_event_unref (event);


	debug_fleave();

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

	return_if_fail ( element );
	return_if_fail ( buf );

    	buffer = gst_buffer_new ();

	if (buf->offset >= buf->len)
	{
		debug_log("call eos appsrc\n");
	       g_signal_emit_by_name (appsrc, "end-of-stream", &ret);
	       return;
	}

	if ( buf->len - buf->offset < size)
    	{
        	len = buf->len - buf->offset + buf->offset;
    	}

    	GST_BUFFER_DATA(buffer) = (guint8*)(buf->buf + buf->offset);
    	GST_BUFFER_SIZE(buffer) = len;
    	GST_BUFFER_OFFSET(buffer) = buf->offset;
    	GST_BUFFER_OFFSET_END(buffer) = buf->offset + len;

    	debug_log("feed buffer %p, offset %u-%u length %u\n", buffer, buf->offset, buf->len,len);
    	g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);

    	buf->offset += len;
}

static gboolean
__gst_appsrc_seek_data_mem(GstElement *element, guint64 size, gpointer user_data) // @
{
	tBuffer *buf = (tBuffer *)user_data;

	return_val_if_fail ( buf, FALSE );

	buf->offset  = (int)size;

    	return TRUE;
}

static void
__gst_appsrc_feed_data(GstElement *element, guint size, gpointer user_data) // @
{
       mm_player_t *player  = (mm_player_t*)user_data;

	return_if_fail ( player );

	debug_msg("app-src: feed data\n");
	     
	if(player->need_data_cb)
    		player->need_data_cb(size, player->buffer_cb_user_param);
}

static gboolean
__gst_appsrc_seek_data(GstElement *element, guint64 offset, gpointer user_data) // @
{
	mm_player_t *player  = (mm_player_t*)user_data;

	return_val_if_fail ( player, FALSE );

	debug_msg("app-src: seek data\n");

	if(player->seek_data_cb)
		player->seek_data_cb(offset, player->buffer_cb_user_param);

	return TRUE;
}


static gboolean
__gst_appsrc_enough_data(GstElement *element, gpointer user_data) // @
{
	mm_player_t *player  = (mm_player_t*)user_data;

	return_val_if_fail ( player, FALSE );

	debug_msg("app-src: enough data:%p\n", player->enough_data_cb);
	
	if(player->enough_data_cb)
		player->enough_data_cb(player->buffer_cb_user_param);

	return TRUE;
}

int
_mmplayer_push_buffer(MMHandleType hplayer, unsigned char *buf, int size) // @
{
	mm_player_t* player = (mm_player_t*)hplayer;
    	GstBuffer *buffer = NULL;
    	GstFlowReturn gst_ret = GST_FLOW_OK;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check current state */
//	MMPLAYER_CHECK_STATE_RETURN_IF_FAIL( player, MMPLAYER_COMMAND_START );


	/* NOTE : we should check and create pipeline again if not created as we destroy
	 * whole pipeline when stopping in streamming playback
	 */
	if ( ! player->pipeline )
	{
		if ( MM_ERROR_NONE != __gst_realize( player ) )
		{
			debug_error("failed to realize before starting. only in streamming\n");
			return MM_ERROR_PLAYER_INTERNAL;
		}
	}

     	debug_msg("app-src: pushing data\n");

    	if ( buf == NULL )
    	{
        	debug_error("buf is null\n");
        	return MM_ERROR_NONE;
    	}

    	buffer = gst_buffer_new ();

    	if (size <= 0)
    	{
        	debug_log("call eos appsrc\n");
        	g_signal_emit_by_name (player->pipeline->mainbin[MMPLAYER_M_SRC].gst, "end-of-stream", &gst_ret);
        	return MM_ERROR_NONE;
    	}

    	GST_BUFFER_DATA(buffer) = (guint8*)(buf);
    	GST_BUFFER_SIZE(buffer) = size;

    	debug_log("feed buffer %p, length %u\n", buf, size);
    	g_signal_emit_by_name (player->pipeline->mainbin[MMPLAYER_M_SRC].gst, "push-buffer", buffer, &gst_ret);

	debug_fleave();

	return ret;
}

static GstBusSyncReply
__mmplayer_bus_sync_callback (GstBus * bus, GstMessage * message, gpointer data)
{
	mm_player_t *player = (mm_player_t *)data;
	GstElement *sender = (GstElement *) GST_MESSAGE_SRC (message);
	const gchar *name = gst_element_get_name (sender);

	switch (GST_MESSAGE_TYPE (message))
	{
		case GST_MESSAGE_TAG:
			__mmplayer_gst_extract_tag_from_msg(player, message);
			break;

		default:
			return GST_BUS_PASS;
	}
	gst_message_unref (message);

	return GST_BUS_DROP;
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
	GList* element_bucket = NULL;
	gboolean need_state_holder = TRUE;
	gint i = 0;

	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	/* get profile attribute */
	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("cannot get content attribute\n");
		goto INIT_ERROR;
	}

	/* create pipeline handles */
	if ( player->pipeline )
	{
		debug_warning("pipeline should be released before create new one\n");
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
		debug_error("failed to create pipeline\n");
		goto INIT_ERROR;
	}


	/* create source element */
	switch ( player->profile.uri_type )
	{
		/* rtsp streamming */
		case MM_PLAYER_URI_TYPE_URL_RTSP:
		{
			gint network_bandwidth;
			gchar *user_agent, *wap_profile;

			element = gst_element_factory_make(PLAYER_INI()->name_of_rtspsrc, "streaming_source");

			if ( !element )
			{
				debug_critical("failed to create streaming source element\n");
				break;
			}

			debug_log("using streamming source [%s].\n", PLAYER_INI()->name_of_rtspsrc);

			/* make it zero */
			network_bandwidth = 0;
			user_agent = wap_profile = NULL;

			/* get attribute */
			mm_attrs_get_string_by_name ( attrs, "streaming_user_agent", &user_agent );
			mm_attrs_get_string_by_name ( attrs,"streaming_wap_profile", &wap_profile );
			mm_attrs_get_int_by_name ( attrs, "streaming_network_bandwidth", &network_bandwidth );

			debug_log("setting streaming source ----------------\n");
			debug_log("user_agent : %s\n", user_agent);
			debug_log("wap_profile : %s\n", wap_profile);
			debug_log("network_bandwidth : %d\n", network_bandwidth);
			debug_log("buffering time : %d\n", PLAYER_INI()->rtsp_buffering_time);
			debug_log("rebuffering time : %d\n", PLAYER_INI()->rtsp_rebuffering_time);
			debug_log("-----------------------------------------\n");

			/* setting property to streaming source */
			g_object_set(G_OBJECT(element), "location", player->profile.uri, NULL);
			g_object_set(G_OBJECT(element), "bandwidth", network_bandwidth, NULL);
			g_object_set(G_OBJECT(element), "buffering_time", PLAYER_INI()->rtsp_buffering_time, NULL);
			g_object_set(G_OBJECT(element), "rebuffering_time", PLAYER_INI()->rtsp_rebuffering_time, NULL);
			if ( user_agent )
				g_object_set(G_OBJECT(element), "user_agent", user_agent, NULL);
			if ( wap_profile )
				g_object_set(G_OBJECT(element), "wap_profile", wap_profile, NULL);

			MMPLAYER_SIGNAL_CONNECT ( player, G_OBJECT(element), "pad-added",
				G_CALLBACK (__mmplayer_gst_rtp_dynamic_pad), player );
			MMPLAYER_SIGNAL_CONNECT ( player, G_OBJECT(element), "no-more-pads",
				G_CALLBACK (__mmplayer_gst_rtp_no_more_pads), player );

			player->no_more_pad = FALSE;
			player->num_dynamic_pad = 0;

			/* NOTE : we cannot determine it yet. this filed will be filled by
			 * _mmplayer_update_content_attrs() after START.
			 */
			player->streaming_type = STREAMING_SERVICE_NONE;
		}
		break;

		/* http streaming*/
		case MM_PLAYER_URI_TYPE_URL_HTTP:
		{
			gchar *user_agent, *proxy, *cookies, **cookie_list;
			user_agent = proxy = cookies = NULL;
			cookie_list = NULL;
			gint mode = MM_PLAYER_PD_MODE_NONE;

			mm_attrs_get_int_by_name ( attrs, "pd_mode", &mode );

			player->pd_mode = mode;

			debug_log("http playback, PD mode : %d\n", player->pd_mode);

			if ( ! MMPLAYER_IS_HTTP_PD(player) )
			{
				element = gst_element_factory_make(PLAYER_INI()->name_of_httpsrc, "http_streaming_source");
				if ( !element )
				{
					debug_critical("failed to create http streaming source element[%s].\n", PLAYER_INI()->name_of_httpsrc);
					break;
				}
				debug_log("using http streamming source [%s].\n", PLAYER_INI()->name_of_httpsrc);

				/* get attribute */
				mm_attrs_get_string_by_name ( attrs, "streaming_cookie", &cookies );
				mm_attrs_get_string_by_name ( attrs, "streaming_user_agent", &user_agent );
				mm_attrs_get_string_by_name ( attrs, "streaming_proxy", &proxy );
				
				/* get attribute */
				debug_log("setting http streaming source ----------------\n");
				debug_log("location : %s\n", player->profile.uri);
				debug_log("cookies : %s\n", cookies);
				debug_log("proxy : %s\n", proxy);
				debug_log("user_agent :  %s\n",  user_agent);
				debug_log("timeout : %d\n",  PLAYER_INI()->http_timeout);
				debug_log("-----------------------------------------\n");

				/* setting property to streaming source */
				g_object_set(G_OBJECT(element), "location", player->profile.uri, NULL);
				g_object_set(G_OBJECT(element), "timeout", PLAYER_INI()->http_timeout, NULL);
				/* check if prosy is vailid or not */
				if ( util_check_valid_url ( proxy ) )
					g_object_set(G_OBJECT(element), "proxy", proxy, NULL);
				/* parsing cookies */
				if ( ( cookie_list = util_get_cookie_list ((const char*)cookies) ) )
					g_object_set(G_OBJECT(element), "cookies", cookie_list, NULL);
				if ( user_agent )
					g_object_set(G_OBJECT(element), "user_agent", user_agent, NULL);
			}
			else // progressive download 
			{
				if (player->pd_mode == MM_PLAYER_PD_MODE_URI)
				{
					gchar *path = NULL;
					
					mm_attrs_get_string_by_name ( attrs, "pd_location", &path );
					
					MMPLAYER_FREEIF(player->pd_file_location);

					debug_log("PD Location : %s\n", path);

					if ( path )
					{
						player->pd_file_location = g_strdup(path);
					}
					else
					{
						debug_error("can't find pd location so, it should be set \n");
						return MM_ERROR_PLAYER_FILE_NOT_FOUND;	
					}
				}

				element = gst_element_factory_make("pdpushsrc", "PD pushsrc");
				if ( !element )
				{
					debug_critical("failed to create PD push source element[%s].\n", "pdpushsrc");
					break;
				}

				g_object_set(G_OBJECT(element), "location", player->pd_file_location, NULL);
			}
			
			player->streaming_type = STREAMING_SERVICE_NONE;
		}
		break;

		/* file source */
		case MM_PLAYER_URI_TYPE_FILE:
		{
			char* drmsrc = PLAYER_INI()->name_of_drmsrc;

			debug_log("using [%s] for 'file://' handler.\n", drmsrc);

			element = gst_element_factory_make(drmsrc, "source");
			if ( !element )
			{
				debug_critical("failed to create %s\n", drmsrc);
				break;
			}

			g_object_set(G_OBJECT(element), "location", (player->profile.uri)+7, NULL);	/* uri+7 -> remove "file:// */
			//g_object_set(G_OBJECT(element), "use-mmap", TRUE, NULL);
		}
		break;

		/* appsrc */
		case MM_PLAYER_URI_TYPE_BUFF:
		{
			guint64 stream_type = GST_APP_STREAM_TYPE_STREAM;

			debug_log("mem src is selected\n");

			element = gst_element_factory_make("appsrc", "buff-source");
			if ( !element )
			{
				debug_critical("failed to create appsrc element\n");
				break;
			}

			g_object_set( element, "stream-type", stream_type, NULL );
			//g_object_set( element, "size", player->mem_buf.len, NULL );
			//g_object_set( element, "blocksize", (guint64)20480, NULL );

			MMPLAYER_SIGNAL_CONNECT( player, element, "seek-data",
				G_CALLBACK(__gst_appsrc_seek_data), player);
			MMPLAYER_SIGNAL_CONNECT( player, element, "need-data",
				G_CALLBACK(__gst_appsrc_feed_data), player);
			MMPLAYER_SIGNAL_CONNECT( player, element, "enough-data",
				G_CALLBACK(__gst_appsrc_enough_data), player);
		}
		break;

		/* appsrc */
		case MM_PLAYER_URI_TYPE_MEM:
		{
			guint64 stream_type = GST_APP_STREAM_TYPE_RANDOM_ACCESS;

			debug_log("mem src is selected\n");

			element = gst_element_factory_make("appsrc", "mem-source");
			if ( !element )
			{
				debug_critical("failed to create appsrc element\n");
				break;
			}

			g_object_set( element, "stream-type", stream_type, NULL );
			g_object_set( element, "size", player->mem_buf.len, NULL );
			g_object_set( element, "blocksize", (guint64)20480, NULL );

			MMPLAYER_SIGNAL_CONNECT( player, element, "seek-data",
				G_CALLBACK(__gst_appsrc_seek_data_mem), &player->mem_buf );
			MMPLAYER_SIGNAL_CONNECT( player, element, "need-data",
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
		debug_critical("no source element was created.\n");
		goto INIT_ERROR;
	}

	/* take source element */
	mainbin[MMPLAYER_M_SRC].id = MMPLAYER_M_SRC;
	mainbin[MMPLAYER_M_SRC].gst = element;
	element_bucket = g_list_append(element_bucket, &mainbin[MMPLAYER_M_SRC]);

	if (MMPLAYER_IS_STREAMING(player))
	{
		player->streamer = __mm_player_streaming_create();
		__mm_player_streaming_initialize(player->streamer);
	}

	if ( MMPLAYER_IS_HTTP_PD(player) )
	{	
	       debug_log ("Picked queue2 element....\n");
		element = gst_element_factory_make("queue2", "hls_stream_buffer");
		if ( !element )
		{
			debug_critical ( "failed to create http streaming buffer element\n" );
			goto INIT_ERROR;
		}
			
		/* take it */
		mainbin[MMPLAYER_M_S_BUFFER].id = MMPLAYER_M_S_BUFFER;
		mainbin[MMPLAYER_M_S_BUFFER].gst = element;
		element_bucket = g_list_append(element_bucket, &mainbin[MMPLAYER_M_S_BUFFER]);

		__mm_player_streaming_set_buffer(player->streamer,
				element,
				TRUE,
				PLAYER_INI()->http_max_size_bytes,
				1.0,
				PLAYER_INI()->http_buffering_limit,
				PLAYER_INI()->http_buffering_time,
				FALSE,
				NULL,
				0);
	}

	/* create autoplugging element if src element is not a streamming src */
	if ( player->profile.uri_type != MM_PLAYER_URI_TYPE_URL_RTSP )
	{
		element = NULL;

		if( PLAYER_INI()->use_decodebin )
		{
			/* create decodebin */
			element = gst_element_factory_make("decodebin", "decodebin");

			g_object_set(G_OBJECT(element), "async-handling", TRUE, NULL);

			/* set signal handler */
			MMPLAYER_SIGNAL_CONNECT( player, G_OBJECT(element), "new-decoded-pad",
					G_CALLBACK(__mmplayer_gst_decode_callback), player);

			/* we don't need state holder, bcz decodebin is doing well by itself */
			need_state_holder = FALSE;
		}
		else
		{
			element = gst_element_factory_make("typefind", "typefinder");
			MMPLAYER_SIGNAL_CONNECT( player, element, "have-type",
				G_CALLBACK(__mmplayer_typefind_have_type), (gpointer)player );
		}

		/* check autoplug element is OK */
		if ( ! element )
		{
			debug_critical("can not create autoplug element\n");
			goto INIT_ERROR;
		}

		mainbin[MMPLAYER_M_AUTOPLUG].id = MMPLAYER_M_AUTOPLUG;
		mainbin[MMPLAYER_M_AUTOPLUG].gst = element;

		element_bucket = g_list_append(element_bucket, &mainbin[MMPLAYER_M_AUTOPLUG]);
	}


	/* add elements to pipeline */
	if( !__mmplayer_gst_element_add_bucket_to_bin(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), element_bucket))
	{
		debug_error("Failed to add elements to pipeline\n");
		goto INIT_ERROR;
	}


	/* linking elements in the bucket by added order. */
	if ( __mmplayer_gst_element_link_bucket(element_bucket) == -1 )
	{
		debug_error("Failed to link some elements\n");
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
			debug_error ("fakesink element could not be created\n");
			goto INIT_ERROR;
		}
		GST_OBJECT_FLAG_UNSET (mainbin[MMPLAYER_M_SRC_FAKESINK].gst, GST_ELEMENT_IS_SINK);

		/* take ownership of fakesink. we are reusing it */
		gst_object_ref( mainbin[MMPLAYER_M_SRC_FAKESINK].gst );

		/* add */
		if ( FALSE == gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst),
			mainbin[MMPLAYER_M_SRC_FAKESINK].gst) )
		{
			debug_error("failed to add fakesink to bin\n");
			goto INIT_ERROR;
		}
	}

	/* now we have completed mainbin. take it */
	player->pipeline->mainbin = mainbin;

	/* connect bus callback */
	bus = gst_pipeline_get_bus(GST_PIPELINE(mainbin[MMPLAYER_M_PIPE].gst));
	if ( !bus )
	{
		debug_error ("cannot get bus from pipeline.\n");
		goto INIT_ERROR;
	}
	player->bus_watcher = gst_bus_add_watch(bus, (GstBusFunc)__mmplayer_gst_callback, player);

	/* Note : check whether subtitle atrribute uri is set. If uri is set, then create the text pipeline */
	if ( __mmplayer_check_subtitle ( player ) )
	{
		debug_log("try to create subtitle pipeline \n");

		if ( MM_ERROR_NONE != __mmplayer_gst_create_subtitle_pipeline(player) )
			debug_error("fail to create subtitle pipeline")
		else
			debug_log("subtitle pipeline is created successfully\n");
	}

	/* set sync handler to get tag synchronously */
	gst_bus_set_sync_handler(bus, __mmplayer_bus_sync_callback, player);


	/* finished */
	gst_object_unref(GST_OBJECT(bus));
	g_list_free(element_bucket);

	debug_fleave();

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


static int
__mmplayer_gst_destroy_pipeline(mm_player_t* player) // @
{
	gint timeout = 0;
	int ret = MM_ERROR_NONE;

	debug_fenter();
	
	return_val_if_fail ( player, MM_ERROR_INVALID_HANDLE );

	/* cleanup stuffs */
	MMPLAYER_FREEIF(player->type);
	player->have_dynamic_pad = FALSE;
	player->no_more_pad = FALSE;
	player->num_dynamic_pad = 0;

	if (player->v_stream_caps)
	{
		gst_caps_unref(player->v_stream_caps);
		player->v_stream_caps = NULL;
	}

	if (ahs_appsrc_cb_probe_id )
	{
		GstPad *pad = NULL;
		pad = gst_element_get_static_pad(player->pipeline->mainbin[MMPLAYER_M_SRC].gst, "src" );

		gst_pad_remove_buffer_probe (pad, ahs_appsrc_cb_probe_id);
		gst_object_unref(pad);
		pad = NULL;
		ahs_appsrc_cb_probe_id = 0;
	}

	if ( player->sink_elements )
		g_list_free ( player->sink_elements );
	player->sink_elements = NULL;

	/* cleanup unlinked mime type */
	MMPLAYER_FREEIF(player->unlinked_audio_mime);
	MMPLAYER_FREEIF(player->unlinked_video_mime);
	MMPLAYER_FREEIF(player->unlinked_demuxer_mime);	

	/* cleanup running stuffs */
	__mmplayer_cancel_delayed_eos( player );

	/* cleanup gst stuffs */
	if ( player->pipeline )
	{
		MMPlayerGstElement* mainbin = player->pipeline->mainbin;
		GstTagList* tag_list = player->pipeline->tag_list;
		GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (mainbin[MMPLAYER_M_PIPE].gst));

		/* first we need to disconnect all signal hander */
		__mmplayer_release_signal_connection( player );

		/* disconnecting bus watch */
		if ( player->bus_watcher )
			g_source_remove( player->bus_watcher );
		player->bus_watcher = 0;

		gst_bus_set_sync_handler (bus, NULL, NULL);

		if ( mainbin )
		{
			MMPlayerGstElement* audiobin = player->pipeline->audiobin;
			MMPlayerGstElement* videobin = player->pipeline->videobin;
			MMPlayerGstElement* textbin = player->pipeline->textbin;
			MMPlayerGstElement* subtitlebin = player->pipeline->subtitlebin;

			debug_log("pipeline status before set state to NULL\n");
			__mmplayer_dump_pipeline_state( player );

			timeout = MMPLAYER_STATE_CHANGE_TIMEOUT(player);
			ret = __mmplayer_gst_set_state ( player, mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_NULL, FALSE, timeout );
			if ( ret != MM_ERROR_NONE )
			{
				debug_error("fail to change state to NULL\n");
				return MM_ERROR_PLAYER_INTERNAL;
			}

			debug_log("pipeline status before unrefering pipeline\n");
			__mmplayer_dump_pipeline_state( player );

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
			MMPLAYER_FREEIF( subtitlebin);
			MMPLAYER_FREEIF( mainbin );
		}

		if ( tag_list )
			gst_tag_list_free(tag_list);

		MMPLAYER_FREEIF( player->pipeline );
	}

	player->pipeline_is_constructed = FALSE;
	
	debug_fleave();

	return ret;
}

static int __gst_realize(mm_player_t* player) // @
{
	gint timeout = 0;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_READY;

	__ta__("__mmplayer_gst_create_pipeline",
		ret = __mmplayer_gst_create_pipeline(player);
		if ( ret )
		{
			debug_critical("failed to create pipeline\n");
			return ret;
		}
	)

	/* set pipeline state to READY */
	/* NOTE : state change to READY must be performed sync. */
	timeout = MMPLAYER_STATE_CHANGE_TIMEOUT(player);
	ret = __mmplayer_gst_set_state(player,
				player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_READY, FALSE, timeout);

	if (MMPLAYER_PLAY_SUBTITLE(player))
		ret = __mmplayer_gst_set_state(player,
					player->pipeline->subtitlebin[MMPLAYER_SUB_PIPE].gst, GST_STATE_READY, FALSE, timeout);

	if ( ret != MM_ERROR_NONE )
	{
		/* return error if failed to set state */
		debug_error("failed to set state PAUSED (live : READY).\n");

		/* dump state of all element */
		__mmplayer_dump_pipeline_state( player );

		return ret;
	}
	else 
	{
		MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_READY );
	}

	/* create dot before error-return. for debugging */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-realize" );

	debug_fleave();

	return ret;
}

static int __gst_unrealize(mm_player_t* player) // @
{
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_NULL;
	MMPLAYER_PRINT_STATE(player);	

	/* release miscellaneous information */
	__mmplayer_release_misc( player );

	/* destroy pipeline */
	ret = __mmplayer_gst_destroy_pipeline( player );
	if ( ret != MM_ERROR_NONE )
	{
		debug_error("failed to destory pipeline\n");
		return ret;
	}

	MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_NULL );

	debug_fleave();

	return ret;
}

static int __gst_pending_seek ( mm_player_t* player )
{
	MMPlayerStateType current_state = MM_PLAYER_STATE_NONE;
	MMPlayerStateType pending_state = MM_PLAYER_STATE_NONE;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );

	if ( !player->pending_seek.is_pending )
	{
		debug_log("pending seek is not reserved. nothing to do.\n" );
		return ret;
	}

	/* check player state if player could pending seek or not. */
	current_state = MMPLAYER_CURRENT_STATE(player);
	pending_state = MMPLAYER_PENDING_STATE(player);

	if ( current_state != MM_PLAYER_STATE_PAUSED && current_state != MM_PLAYER_STATE_PLAYING  )
	{
		debug_warning("try to pending seek in %s state, try next time. \n",
			MMPLAYER_STATE_GET_NAME(current_state));
		return ret;
	}
	
	debug_log("trying to play from (%lu) pending position\n", player->pending_seek.pos);
	
	ret = __gst_set_position ( player, player->pending_seek.format, player->pending_seek.pos, FALSE );
	
	if ( MM_ERROR_NONE != ret )
		debug_error("failed to seek pending postion. just keep staying current position.\n");

	player->pending_seek.is_pending = FALSE;

	debug_fleave();

	return ret;
}

static int __gst_start(mm_player_t* player) // @
{
	gboolean sound_extraction = 0;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );

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
			debug_error("failed to set state to PAUSED for pending seek\n");
			return ret;
		}

		MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_PLAYING;

		if ( sound_extraction )
		{
			debug_log("setting pcm extraction\n");

			ret = __mmplayer_set_pcm_extraction(player);
			if ( MM_ERROR_NONE != ret )
			{
				debug_warning("failed to set pcm extraction\n");
				return ret;
			}
		}
		else
		{ 			
			if ( MM_ERROR_NONE != __gst_pending_seek(player) )
			{
				debug_warning("failed to seek pending postion. starting from the begin of content.\n");
			}
		}
	}

	debug_log("current state before doing transition");
	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_PLAYING;
	MMPLAYER_PRINT_STATE(player);

	/* set pipeline state to PLAYING  */
	ret = __mmplayer_gst_set_state(player,
		player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_PLAYING, FALSE, MMPLAYER_STATE_CHANGE_TIMEOUT(player) );

	if (MMPLAYER_PLAY_SUBTITLE(player))
		ret = __mmplayer_gst_set_state(player,
			player->pipeline->subtitlebin[MMPLAYER_SUB_PIPE].gst, GST_STATE_PLAYING, FALSE, MMPLAYER_STATE_CHANGE_TIMEOUT(player) );

	if (ret == MM_ERROR_NONE)
	{
		MMPLAYER_SET_STATE(player, MM_PLAYER_STATE_PLAYING);
	}
	else
	{
		debug_error("failed to set state to PLAYING");

		/* dump state of all element */
		__mmplayer_dump_pipeline_state( player );

		return ret;
	}

	/* FIXIT : analyze so called "async problem" */
	/* set async off */
	__gst_set_async_state_change( player, FALSE );

	/* generating debug info before returning error */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-start" );

	debug_fleave();

	return ret;
}

static void __mmplayer_do_sound_fadedown(mm_player_t* player, unsigned int time)
{
	debug_fenter();
	
	return_if_fail(player 
		&& player->pipeline
		&& player->pipeline->audiobin
		&& player->pipeline->audiobin[MMPLAYER_A_SINK].gst);

	g_object_set(G_OBJECT(player->pipeline->audiobin[MMPLAYER_A_SINK].gst), "mute", 2, NULL);
	
	usleep(time);

	debug_fleave();
}

static void __mmplayer_undo_sound_fadedown(mm_player_t* player)
{
	debug_fenter();
	
	return_if_fail(player 
		&& player->pipeline
		&& player->pipeline->audiobin
		&& player->pipeline->audiobin[MMPLAYER_A_SINK].gst);
	
	g_object_set(G_OBJECT(player->pipeline->audiobin[MMPLAYER_A_SINK].gst), "mute", 0, NULL);	

	debug_fleave();
}

static int __gst_stop(mm_player_t* player) // @
{
	GstStateChangeReturn change_ret = GST_STATE_CHANGE_SUCCESS;
	MMHandleType attrs = 0;
	gboolean fadewown = FALSE;
	gboolean rewind = FALSE;
	gint timeout = 0;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED);

	debug_log("current state before doing transition");
	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_READY;
	MMPLAYER_PRINT_STATE(player);	

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("cannot get content attribute\n");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	mm_attrs_get_int_by_name(attrs,"sound_fadedown", &fadewown);

	/* enable fadedown */
	if (fadewown)
		__mmplayer_do_sound_fadedown(player, MM_PLAYER_FADEOUT_TIME_DEFAULT);

	/* Just set state to PAUESED and the rewind. it's usual player behavior. */
	timeout = MMPLAYER_STATE_CHANGE_TIMEOUT ( player );
	if  ( player->profile.uri_type == MM_PLAYER_URI_TYPE_BUFF || player->profile.uri_type == MM_PLAYER_URI_TYPE_HLS)
	{
		ret = __mmplayer_gst_set_state(player, 
			player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_READY, FALSE, timeout );
	}
	else
	{
		ret = __mmplayer_gst_set_state( player,
			player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_PAUSED, FALSE, timeout );

		if (MMPLAYER_PLAY_SUBTITLE(player))
			ret = __mmplayer_gst_set_state( player,
				player->pipeline->subtitlebin[MMPLAYER_SUB_PIPE].gst, GST_STATE_PAUSED, FALSE, timeout );

		if ( !MMPLAYER_IS_STREAMING(player))
			rewind = TRUE;
	}

	/* disable fadeout */
	if (fadewown)
		__mmplayer_undo_sound_fadedown(player);


	/* return if set_state has failed */
	if ( ret != MM_ERROR_NONE )
	{
		debug_error("failed to set state.\n");

		/* dump state of all element. don't care it success or not */
		__mmplayer_dump_pipeline_state( player );

		return ret;
	}

	/* rewind */
	if ( rewind )
	{
		if ( ! __gst_seek( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, 1.0,
				GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, 0,
				GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE) )
		{
			debug_warning("failed to rewind\n");
			ret = MM_ERROR_PLAYER_SEEK;
		}
	}

	/* initialize */
	player->sent_bos = FALSE;

	/* wait for seek to complete */
	change_ret = gst_element_get_state (player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, NULL, NULL, timeout * GST_SECOND);
	if (MMPLAYER_PLAY_SUBTITLE(player))
		change_ret = gst_element_get_state (player->pipeline->subtitlebin[MMPLAYER_SUB_PIPE].gst, NULL, NULL, timeout * GST_SECOND);

	if ( change_ret == GST_STATE_CHANGE_SUCCESS || change_ret == GST_STATE_CHANGE_NO_PREROLL )
	{
		MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_READY );
	}
	else
	{
		debug_error("fail to stop player.\n");
		ret = MM_ERROR_PLAYER_INTERNAL;
	}

	/* generate dot file if enabled */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-stop" );

	debug_fleave();	

	return ret;
}

int __gst_pause(mm_player_t* player, gboolean async) // @
{
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail(player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED);

	debug_log("current state before doing transition");
	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_PAUSED;
	MMPLAYER_PRINT_STATE(player);	

	/* set pipeline status to PAUSED */
	ret = __mmplayer_gst_set_state(player,
		player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_PAUSED, async, MMPLAYER_STATE_CHANGE_TIMEOUT(player));

	if (MMPLAYER_PLAY_SUBTITLE(player))
		ret = __mmplayer_gst_set_state(player,
			player->pipeline->subtitlebin[MMPLAYER_SUB_PIPE].gst, GST_STATE_PAUSED, async, MMPLAYER_STATE_CHANGE_TIMEOUT(player));

	if ( ret != MM_ERROR_NONE )
	{
		debug_error("failed to set state to PAUSED\n");

		/* dump state of all element */
		__mmplayer_dump_pipeline_state( player );

		return ret;
	}
	else
	{	
		if ( async == FALSE ) 
		{
			MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_PAUSED );
		}
	} 

	/* FIXIT : analyze so called "async problem" */
	/* set async off */
	__gst_set_async_state_change( player, TRUE);

	/* generate dot file before returning error */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-pause" );

	debug_fleave();

	return ret;
}

int __gst_resume(mm_player_t* player, gboolean async) // @
{
	int ret = MM_ERROR_NONE;
	gint timeout = 0;

	debug_fenter();

	return_val_if_fail(player && player->pipeline,
		MM_ERROR_PLAYER_NOT_INITIALIZED);

	debug_log("current state before doing transition");
	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_PLAYING;
	MMPLAYER_PRINT_STATE(player);	

	/* generate dot file before returning error */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-resume" );

	__mmplayer_set_antishock( player , FALSE );

	if ( async )
		debug_log("do async state transition to PLAYING.\n");

	/* set pipeline state to PLAYING */
	timeout = MMPLAYER_STATE_CHANGE_TIMEOUT(player);
	ret = __mmplayer_gst_set_state(player,
		player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_PLAYING, async, timeout );

	if (MMPLAYER_PLAY_SUBTITLE(player))
		ret = __mmplayer_gst_set_state(player,
			player->pipeline->subtitlebin[MMPLAYER_SUB_PIPE].gst, GST_STATE_PLAYING, async, timeout );

	if (ret != MM_ERROR_NONE)
	{
		debug_error("failed to set state to PLAYING\n");

		/* dump state of all element */
		__mmplayer_dump_pipeline_state( player );

		return ret;
	}
	else
	{
		if (async == FALSE)
		{
			MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_PLAYING );
		}
	}
	
	/* FIXIT : analyze so called "async problem" */
	/* set async off */
	__gst_set_async_state_change( player, FALSE );

	/* generate dot file before returning error */
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-resume" );

	debug_fleave();

	return ret;
}

static int
__gst_set_position(mm_player_t* player, int format, unsigned long position, gboolean internal_called) // @
{
	GstFormat fmt  = GST_FORMAT_TIME;
	unsigned long dur_msec = 0;
	gint64 dur_nsec = 0;
	gint64 pos_nsec = 0;
	gboolean ret = TRUE;

	debug_fenter();
	return_val_if_fail ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( !MMPLAYER_IS_LIVE_STREAMING(player), MM_ERROR_PLAYER_NO_OP );

	if ( MMPLAYER_CURRENT_STATE(player) != MM_PLAYER_STATE_PLAYING
		&& MMPLAYER_CURRENT_STATE(player) != MM_PLAYER_STATE_PAUSED )
		goto PENDING;

	/* check duration */
	/* NOTE : duration cannot be zero except live streaming.
	 * 		Since some element could have some timing problemn with quering duration, try again.
	 */
	if ( !player->duration )
	{
		if ( !gst_element_query_duration( player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, &fmt, &dur_nsec ))
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
		debug_error("could not get the duration. fail to seek.\n");
		goto SEEK_ERROR;
	}

	debug_log("playback rate: %f\n", player->playback_rate);

	/* do seek */
	switch ( format )
	{
		case MM_PLAYER_POS_FORMAT_TIME:
		{
			/* check position is valid or not */
			if ( position > dur_msec )
				goto INVALID_ARGS;

			debug_log("seeking to (%lu) msec, duration is %d msec\n", position, dur_msec);

			if (player->doing_seek)
			{
				debug_log("not completed seek");
				return MM_ERROR_PLAYER_DOING_SEEK;
			}

			if ( !internal_called)
				player->doing_seek = TRUE;

			pos_nsec = position * G_GINT64_CONSTANT(1000000);
			ret = __gst_seek ( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, 1.0,
							GST_FORMAT_TIME, ( GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE ),
							GST_SEEK_TYPE_SET, pos_nsec, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE );
			if ( !ret  )
			{
				debug_error("failed to set position. dur[%lu]  pos[%lu]  pos_msec[%llu]\n", dur_msec, position, pos_nsec);
				goto SEEK_ERROR;
			}
		}
		break;

		case MM_PLAYER_POS_FORMAT_PERCENT:
		{
			if ( position < 0 && position > 100 )
				goto INVALID_ARGS;

			debug_log("seeking to (%lu)%% \n", position);

			if (player->doing_seek)
			{
				debug_log("not completed seek");
				return MM_ERROR_PLAYER_DOING_SEEK;
			}

			if ( !internal_called)
				player->doing_seek = TRUE;

			/* FIXIT : why don't we use 'GST_FORMAT_PERCENT' */
			pos_nsec = (gint64) ( ( position * player->duration ) / 100 );
			ret = __gst_seek ( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, 1.0,
							GST_FORMAT_TIME, ( GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE ),
							GST_SEEK_TYPE_SET, pos_nsec, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE );
			if ( !ret  )
			{
				debug_error("failed to set position. dur[%lud]  pos[%lud]  pos_msec[%llud]\n", dur_msec, position, pos_nsec);
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
		_mmplayer_set_playspeed ( (MMHandleType)player, player->playback_rate );

	debug_fleave();
	return MM_ERROR_NONE;

PENDING:
	player->pending_seek.is_pending = TRUE;
	player->pending_seek.format = format;
	player->pending_seek.pos = position;
	
	debug_warning("player current-state : %s, pending-state : %s, just preserve pending position(%lu).\n", 
		MMPLAYER_STATE_GET_NAME(MMPLAYER_CURRENT_STATE(player)), MMPLAYER_STATE_GET_NAME(MMPLAYER_PENDING_STATE(player)), player->pending_seek.pos);
	
	return MM_ERROR_NONE;
	
INVALID_ARGS:	
	debug_error("invalid arguments, position : %ld  dur : %ld format : %d \n", position, dur_msec, format);
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
	GstFormat fmt = GST_FORMAT_TIME;
	signed long long pos_msec = 0;
	gboolean ret = TRUE;

	return_val_if_fail( player && position && player->pipeline && player->pipeline->mainbin,
		MM_ERROR_PLAYER_NOT_INITIALIZED );

	current_state = MMPLAYER_CURRENT_STATE(player);

	/* NOTE : query position except paused state to overcome some bad operation
	 * please refer to below comments in details
	 */
	if ( current_state != MM_PLAYER_STATE_PAUSED )
	{
		ret = gst_element_query_position(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, &fmt, &pos_msec);
	}

	/* NOTE : get last point to overcome some bad operation of some elements
	 * ( returning zero when getting current position in paused state
	 * and when failed to get postion during seeking
	 */
	if ( ( current_state == MM_PLAYER_STATE_PAUSED )
		|| ( ! ret ))
		//|| ( player->last_position != 0 && pos_msec == 0 ) )
	{
		debug_warning ("pos_msec = %"GST_TIME_FORMAT" and ret = %d and state = %d", GST_TIME_ARGS (pos_msec), ret, current_state);

		if(player->playback_rate < 0.0)
			pos_msec = player->last_position - TRICKPLAY_OFFSET;
		else
			pos_msec = player->last_position;

		if (!ret)
			pos_msec = player->last_position;
		else
			player->last_position = pos_msec;

		debug_warning("returning last point : %"GST_TIME_FORMAT, GST_TIME_ARGS(pos_msec));

	}
	else
	{
		player->last_position = pos_msec;
	}

	switch (format) {
		case MM_PLAYER_POS_FORMAT_TIME:
			*position = GST_TIME_AS_MSECONDS(pos_msec);
			break;

		case MM_PLAYER_POS_FORMAT_PERCENT:
		{
			int dur = 0;
			int pos = 0;

			dur = player->duration / GST_SECOND;
			if (dur <= 0)
			{
				debug_log ("duration is [%d], so returning position 0\n",dur);
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

	debug_log("current position : %lu\n", *position);

	
	return MM_ERROR_NONE;
}


static int 	__gst_get_buffer_position(mm_player_t* player, int format, unsigned long* start_pos, unsigned long* stop_pos)
{
      	GstElement *element = NULL;
      	GstQuery *query = NULL;

	return_val_if_fail( player && 
		player->pipeline && 
		player->pipeline->mainbin,
		MM_ERROR_PLAYER_NOT_INITIALIZED );

	return_val_if_fail( start_pos && stop_pos, MM_ERROR_INVALID_ARGUMENT );

	if ( MMPLAYER_IS_HTTP_STREAMING ( player ))
	{
		/* Note : In case of http streaming or HLS, the buffering queue [ queue2 ] could handle buffering query. */
		element = GST_ELEMENT ( player->pipeline->mainbin[MMPLAYER_M_S_BUFFER].gst );
	}
	else if ( MMPLAYER_IS_RTSP_STREAMING ( player ) )
	{
		debug_warning ( "it's not supported yet.\n" );
		return MM_ERROR_NONE;
	}
	else
	{
		debug_warning ( "it's only used for streaming case.\n" );
		return MM_ERROR_NONE;	
	}

	*start_pos = 0;
	*stop_pos = 0;
	
	switch ( format )
	{
		case MM_PLAYER_POS_FORMAT_PERCENT :
		{
			      	query = gst_query_new_buffering ( GST_FORMAT_PERCENT );
			      	if ( gst_element_query ( element, query ) ) 
				{
			        	gint64 start, stop;
			        	GstFormat format;
			        	gboolean busy;
			        	gint percent;

			        	gst_query_parse_buffering_percent ( query, &busy, &percent);
			        	gst_query_parse_buffering_range ( query, &format, &start, &stop, NULL );

			        	debug_log ( "buffering start %" G_GINT64_FORMAT ", stop %" G_GINT64_FORMAT "\n",  start, stop);

			        	if ( start != -1)
			          		*start_pos = 100 * start / GST_FORMAT_PERCENT_MAX;
			        	else
			          		*start_pos = 0;

			        	if ( stop != -1)
			          		*stop_pos = 100 * stop / GST_FORMAT_PERCENT_MAX;
			        	else
			          		*stop_pos = 0;
			      	}
			      	gst_query_unref (query);
		}
		break;

		case MM_PLAYER_POS_FORMAT_TIME :
			debug_warning ( "Time format is not supported yet.\n" );
			break;
			
		default :
			break;
	}

  	debug_log("current buffer position : %lu~%lu \n", *start_pos, *stop_pos );

	return MM_ERROR_NONE;
}

static int
__gst_set_message_callback(mm_player_t* player, MMMessageCallback callback, gpointer user_param) // @
{
	debug_fenter();

	if ( !player )
	{
		debug_warning("set_message_callback is called with invalid player handle\n");
		return MM_ERROR_PLAYER_NOT_INITIALIZED;
	}

	player->msg_cb = callback;
	player->msg_cb_param = user_param;

	debug_log("msg_cb : 0x%x     msg_cb_param : 0x%x\n", (guint)callback, (guint)user_param);

	debug_fleave();

	return MM_ERROR_NONE;
}

static gboolean __mmfplayer_parse_profile(const char *uri, void *param, MMPlayerParseProfile* data) // @
{
	gboolean ret = FALSE;
	char *path = NULL;

	debug_fenter();

	return_val_if_fail ( uri , FALSE);
	return_val_if_fail ( data , FALSE);
	return_val_if_fail ( ( strlen(uri) <= MM_MAX_URL_LEN ), FALSE );

	memset(data, 0, sizeof(MMPlayerParseProfile));

	if ((path = strstr(uri, "file://")))
	{
		if (util_exist_file_path(path + 7)) {
			strncpy(data->uri, path, MM_MAX_URL_LEN-1);			

			if ( util_is_sdp_file ( path ) )
			{
				debug_log("uri is actually a file but it's sdp file. giving it to rtspsrc\n");
				data->uri_type = MM_PLAYER_URI_TYPE_URL_RTSP;
			}
			else
			{
			data->uri_type = MM_PLAYER_URI_TYPE_FILE;
			}
			ret = TRUE;
		}
		else
		{
			debug_warning("could  access %s.\n", path);
		}
	}
	else if ((path = strstr(uri, "buff://")))
	{
			data->uri_type = MM_PLAYER_URI_TYPE_BUFF;
			ret = TRUE;
	}
	else if ((path = strstr(uri, "rtsp://")))
	{
		if (strlen(path)) {
			strcpy(data->uri, uri);
			data->uri_type = MM_PLAYER_URI_TYPE_URL_RTSP;
			ret = TRUE;
		}
	}
	else if ((path = strstr(uri, "http://")))
	{
		if (strlen(path)) {
			strcpy(data->uri, uri);
			        data->uri_type = MM_PLAYER_URI_TYPE_URL_HTTP;

			ret = TRUE;
		}
	}
	else if ((path = strstr(uri, "https://")))
	{
		if (strlen(path)) {
			strcpy(data->uri, uri);
				data->uri_type = MM_PLAYER_URI_TYPE_URL_HTTP;
			
			ret = TRUE;
		}
	}
	else if ((path = strstr(uri, "rtspu://")))
	{
		if (strlen(path)) {
			strcpy(data->uri, uri);
			data->uri_type = MM_PLAYER_URI_TYPE_URL_RTSP;
			ret = TRUE;
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
				ret = TRUE;
			}
		}
	}
	else if ((path = strstr(uri, "mms://")))
	{
		if (strlen(path)) {
			strcpy(data->uri, uri);
			data->uri_type = MM_PLAYER_URI_TYPE_URL_MMS;
			ret = TRUE;
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

   			debug_log("ext: %s, mem_size: %d, mmap(param): %p\n", ext, mem_size, param);
			if ( mem_size && param) {
					data->mem = param;
					data->mem_size = mem_size;
				data->uri_type = MM_PLAYER_URI_TYPE_MEM;
				ret = TRUE;
			}
		}
	}
	else
	{
		/* if no protocol prefix exist. check file existence and then give file:// as it's prefix */
		if (util_exist_file_path(uri))
		{
			debug_warning("uri has no protocol-prefix. giving 'file://' by default.\n");
			g_snprintf(data->uri,  MM_MAX_URL_LEN, "file://%s", uri);

			if ( util_is_sdp_file( (char*)uri ) )
			{
				debug_log("uri is actually a file but it's sdp file. giving it to rtspsrc\n");
				data->uri_type = MM_PLAYER_URI_TYPE_URL_RTSP;
			}
			else
			{
				data->uri_type = MM_PLAYER_URI_TYPE_FILE;
			}
			ret = TRUE;
		}
		else
		{
			debug_error ("invalid uri, could not play..\n");
		 	data->uri_type = MM_PLAYER_URI_TYPE_NONE;
		}
	}
	
	if (data->uri_type == MM_PLAYER_URI_TYPE_NONE) {
		ret = FALSE;
	}

	/* dump parse result */
	debug_log("profile parsing result ---\n");
	debug_log("incomming uri : %s\n", uri);
	debug_log("uri : %s\n", data->uri);
	debug_log("uri_type : %d\n", data->uri_type);
	debug_log("play_mode : %d\n", data->play_mode);
	debug_log("mem : 0x%x\n", (guint)data->mem);
	debug_log("mem_size : %d\n", data->mem_size);
	debug_log("urgent : %s\n", data->urgent);
	debug_log("--------------------------\n");

	debug_fleave();

	return ret;
}

gboolean _asm_postmsg(gpointer *data)
{
	mm_player_t* player = (mm_player_t*)data;
	MMMessageParamType msg = {0, };

	debug_fenter();

	return_val_if_fail ( player, FALSE );

	msg.union_type = MM_MSG_UNION_CODE;
	msg.code = player->sm.event_src;
	
	MMPLAYER_POST_MSG( player, MM_MESSAGE_READY_TO_RESUME, &msg);
	
	return FALSE;
}
gboolean _asm_lazy_pause(gpointer *data)
{
	mm_player_t* player = (mm_player_t*)data;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail ( player, FALSE );

	if (MMPLAYER_CURRENT_STATE(player) == MM_PLAYER_STATE_PLAYING)
	{
		debug_log ("Ready to proceed lazy pause\n");
		ret = _mmplayer_pause((MMHandleType)player);
		if(MM_ERROR_NONE != ret)
		{
			debug_error("MMPlayer pause failed in ASM callback lazy pause\n");
		}	
	}
	else
	{
		debug_log ("Invalid state to proceed lazy pause\n");
	}

	/* unset mute */
	if (player->pipeline && player->pipeline->audiobin)
		g_object_set(G_OBJECT(player->pipeline->audiobin[MMPLAYER_A_SINK].gst), "mute", 0, NULL);

	player->sm.by_asm_cb = 0; //should be reset here

	debug_fleave();

	return FALSE;
}
ASM_cb_result_t
__mmplayer_asm_callback(int handle, ASM_event_sources_t event_src, ASM_sound_commands_t command, unsigned int sound_status, void* cb_data)
{
	mm_player_t* player = (mm_player_t*) cb_data;
	ASM_cb_result_t cb_res = ASM_CB_RES_IGNORE;
	int result = MM_ERROR_NONE;
	gboolean lazy_pause = FALSE;

	debug_fenter();

	return_val_if_fail ( player && player->pipeline, ASM_CB_RES_IGNORE );
	return_val_if_fail ( player->attrs, MM_ERROR_PLAYER_INTERNAL );

	if (player->is_sound_extraction)
	{
		debug_log("sound extraction is working...so, asm command is ignored.\n");
		return result;
	}
	
	player->sm.by_asm_cb = 1; // it should be enabled for player state transition with called application command
	player->sm.event_src = event_src;

	if(event_src == ASM_EVENT_SOURCE_OTHER_PLAYER_APP)
	{
		player->sm.event_src = ASM_EVENT_SOURCE_OTHER_APP;
	}
	else if(event_src == ASM_EVENT_SOURCE_EARJACK_UNPLUG )
	{
		int stop_by_asm = 0;

		mm_attrs_get_int_by_name(player->attrs, "sound_stop_when_unplugged", &stop_by_asm);
		if (!stop_by_asm)
			return cb_res;
	}
	else if (event_src == ASM_EVENT_SOURCE_RESOURCE_CONFLICT)
	{
		/* can use video overlay simultaneously */
		/* video resource conflict */
		if(player->pipeline->videobin) 
		{
			if (PLAYER_INI()->multiple_codec_supported)
			{
				debug_log("video conflict but, can support to use video overlay simultaneously");
				result = _mmplayer_pause((MMHandleType)player);
				cb_res = ASM_CB_RES_PAUSE;
			}
			else
			{
				debug_log("video conflict, can't support for multiple codec instance");
				result = _mmplayer_unrealize((MMHandleType)player);
				cb_res = ASM_CB_RES_STOP;
			}
		}
		return cb_res;
	}

	switch(command)
	{
		case ASM_COMMAND_PLAY:
		case ASM_COMMAND_STOP:
			debug_warning ("Got unexpected asm command (%d)", command);
		break;
			
		case ASM_COMMAND_PAUSE:
		{
			debug_log("Got msg from asm to Pause");
			
			if(event_src == ASM_EVENT_SOURCE_CALL_START
				|| event_src == ASM_EVENT_SOURCE_ALARM_START
				|| event_src == ASM_EVENT_SOURCE_OTHER_APP)
			{
				//hold 0.7 second to excute "fadedown mute" effect
				debug_log ("do fade down->pause->undo fade down");
					
				__mmplayer_do_sound_fadedown(player, MM_PLAYER_FADEOUT_TIME_DEFAULT);
					
				result = _mmplayer_pause((MMHandleType)player);
				if (result != MM_ERROR_NONE)
				{
					debug_warning("fail to set Pause state by asm");
					cb_res = ASM_CB_RES_IGNORE;
					break;
				}
				__mmplayer_undo_sound_fadedown(player);
			}
			else if(event_src == ASM_EVENT_SOURCE_OTHER_PLAYER_APP)
			{
				lazy_pause = TRUE; // return as soon as possible, for fast start of other app

				if ( player->pipeline->audiobin && player->pipeline->audiobin[MMPLAYER_A_SINK].gst )
					g_object_set( player->pipeline->audiobin[MMPLAYER_A_SINK].gst, "mute", 2, NULL);

				player->lazy_pause_event_id = g_timeout_add(LAZY_PAUSE_TIMEOUT_MSEC, (GSourceFunc)_asm_lazy_pause, (gpointer)player);
				debug_log ("set lazy pause timer (id=[%d], timeout=[%d ms])", player->lazy_pause_event_id, LAZY_PAUSE_TIMEOUT_MSEC);
			}
			else
			{
				//immediate pause
				debug_log ("immediate pause");
				result = _mmplayer_pause((MMHandleType)player);
			}
			cb_res = ASM_CB_RES_PAUSE;
		}
		break;
			
		case ASM_COMMAND_RESUME:
		{
			debug_log("Got msg from asm to Resume. So, application can resume. code (%d) \n", event_src);
			player->sm.by_asm_cb = 0;
			//ASM server is single thread daemon. So use g_idle_add() to post resume msg
			g_idle_add((GSourceFunc)_asm_postmsg, (gpointer)player);
			cb_res = ASM_CB_RES_IGNORE;
		}
		break;

		default:
		break;
	}

	if (!lazy_pause)
		player->sm.by_asm_cb = 0;

	debug_fleave();

	return cb_res;
}

int
_mmplayer_create_player(MMHandleType handle) // @
{
	mm_player_t* player = MM_PLAYER_CAST(handle);
	gint i;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	MMTA_ACUM_ITEM_BEGIN("[KPI] media player service create->playing", FALSE);

	/* initialize player state */
	MMPLAYER_CURRENT_STATE(player) = MM_PLAYER_STATE_NONE;
	MMPLAYER_PREV_STATE(player) = MM_PLAYER_STATE_NONE;
	MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_NONE;
	MMPLAYER_TARGET_STATE(player) = MM_PLAYER_STATE_NONE;

	/* check current state */
	MMPLAYER_CHECK_STATE_RETURN_IF_FAIL ( player, MMPLAYER_COMMAND_CREATE );

	/* construct attributes */
	player->attrs = _mmplayer_construct_attribute(handle);

	if ( !player->attrs )
	{
		debug_critical("Failed to construct attributes\n");
		goto ERROR;
	}

	/* initialize gstreamer with configured parameter */
	if ( ! __mmplayer_gstreamer_init() )
	{
		debug_critical("Initializing gstreamer failed\n");
		goto ERROR;
	}

	/* initialize factories if not using decodebin */
	if ( FALSE == PLAYER_INI()->use_decodebin )
	{
		if( player->factories == NULL )
		    __mmplayer_init_factories(player);
	}

	/* create lock. note that g_tread_init() has already called in gst_init() */
	player->fsink_lock = g_mutex_new();
	if ( ! player->fsink_lock )
	{
		debug_critical("Cannot create mutex for command lock\n");
		goto ERROR;
	}

	/* create repeat mutex */
	player->repeat_thread_mutex = g_mutex_new();
	if ( ! player->repeat_thread_mutex )
	{
		debug_critical("Cannot create repeat mutex\n");
		goto ERROR;
	}

	/* create repeat cond */
	player->repeat_thread_cond = g_cond_new();
	if ( ! player->repeat_thread_cond )
	{
		debug_critical("Cannot create repeat cond\n");
		goto ERROR;
	}

	/* create repeat thread */
	player->repeat_thread =
		g_thread_create (__mmplayer_repeat_thread, (gpointer)player, TRUE, NULL);
	if ( ! player->repeat_thread )
	{
		goto ERROR;
	}

	if ( MM_ERROR_NONE != _mmplayer_initialize_video_capture(player))
	{
		debug_error("failed to initialize video capture\n");
		goto ERROR;
	}

	/* register to asm */
	if ( MM_ERROR_NONE != _mmplayer_asm_register(&player->sm, (ASM_sound_cb_t)__mmplayer_asm_callback, (void*)player) )
	{
		/* NOTE : we are dealing it as an error since we cannot expect it's behavior */
		debug_error("failed to register asm server\n");
		return MM_ERROR_POLICY_INTERNAL;
	}

	if (MMPLAYER_IS_HTTP_PD(player))
	{
		player->pd_downloader = NULL;
		player->pd_file_location = NULL;
	}

	/* give default value of sound effect setting */
	player->bypass_sound_effect = TRUE;
	player->sound.volume = MM_VOLUME_FACTOR_DEFAULT;
	player->playback_rate = DEFAULT_PLAYBACK_RATE;
	player->no_more_pad = TRUE;

	/* set player state to null */
	MMPLAYER_STATE_CHANGE_TIMEOUT(player) = PLAYER_INI()->localplayback_state_change_timeout;
	MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_NULL );

	debug_fleave();
	return MM_ERROR_NONE;

ERROR:
	/* free lock */
	if ( player->fsink_lock )
		g_mutex_free( player->fsink_lock );
	player->fsink_lock = NULL;

	/* free thread */
	if ( player->repeat_thread_cond &&
		 player->repeat_thread_mutex &&
		 player->repeat_thread )
	{
		player->repeat_thread_exit = TRUE;
		g_cond_signal( player->repeat_thread_cond );

		g_thread_join( player->repeat_thread );
		player->repeat_thread = NULL;

		g_mutex_free ( player->repeat_thread_mutex );
		player->repeat_thread_mutex = NULL;

		g_cond_free ( player->repeat_thread_cond );
		player->repeat_thread_cond = NULL;
	}
	/* clear repeat thread mutex/cond if still alive
	 * this can happen if only thread creating has failed
	 */
	if ( player->repeat_thread_mutex )
		g_mutex_free ( player->repeat_thread_mutex );

	if ( player->repeat_thread_cond )
		g_cond_free ( player->repeat_thread_cond );

	/* release attributes */
	_mmplayer_deconstruct_attribute(handle);

	return MM_ERROR_PLAYER_INTERNAL;
}

static gboolean
__mmplayer_gstreamer_init(void) // @
{
	static gboolean initialized = FALSE;
	static const int max_argc = 50;
  	gint* argc = NULL;
	gchar** argv = NULL;
	GError *err = NULL;
	int i = 0;

	debug_fenter();

	if ( initialized )
	{
		debug_log("gstreamer already initialized.\n");
		return TRUE;
	}

	/* alloc */
	argc = malloc( sizeof(int) );
	argv = malloc( sizeof(gchar*) * max_argc );

	if ( !argc || !argv )
		goto ERROR;

	memset( argv, 0, sizeof(gchar*) * max_argc );

	/* add initial */
	*argc = 1;
	argv[0] = g_strdup( "mmplayer" );

	/* add gst_param */
	for ( i = 0; i < 5; i++ ) /* FIXIT : num of param is now fixed to 5. make it dynamic */
	{
		if ( strlen( PLAYER_INI()->gst_param[i] ) > 0 )
		{
			argv[*argc] = g_strdup( PLAYER_INI()->gst_param[i] );
			(*argc)++;
		}
	}

	/* we would not do fork for scanning plugins */
	argv[*argc] = g_strdup("--gst-disable-registry-fork");
	(*argc)++;

	/* check disable registry scan */
	if ( PLAYER_INI()->skip_rescan )
	{
		argv[*argc] = g_strdup("--gst-disable-registry-update");
		(*argc)++;
	}

	/* check disable segtrap */
	if ( PLAYER_INI()->disable_segtrap )
	{
		argv[*argc] = g_strdup("--gst-disable-segtrap");
		(*argc)++;
	}

	debug_log("initializing gstreamer with following parameter\n");
	debug_log("argc : %d\n", *argc);

	for ( i = 0; i < *argc; i++ )
	{
		debug_log("argv[%d] : %s\n", i, argv[i]);
	}


	/* initializing gstreamer */
	__ta__("gst_init time",

		if ( ! gst_init_check (argc, &argv, &err))
		{
			debug_error("Could not initialize GStreamer: %s\n", err ? err->message : "unknown error occurred");
			if (err)
			{
				g_error_free (err);
			}

			goto ERROR;
		}
	);

	/* release */
	for ( i = 0; i < *argc; i++ )
	{
		MMPLAYER_FREEIF( argv[i] );
	}

	MMPLAYER_FREEIF( argv );
	MMPLAYER_FREEIF( argc );

	/* done */
	initialized = TRUE;

	debug_fleave();

	return TRUE;

ERROR:

	MMPLAYER_FREEIF( argv );
	MMPLAYER_FREEIF( argc );

	return FALSE;
}

int 
__mmplayer_release_extended_streaming(mm_player_t* player)
{
	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	if (player->pd_downloader && MMPLAYER_IS_HTTP_PD(player))
	{
		_mmplayer_pd_stop ((MMHandleType)player);
		_mmplayer_pd_deinitialize ((MMHandleType)player);
		_mmplayer_pd_destroy((MMHandleType)player);
	}

       if (MMPLAYER_IS_STREAMING(player))
       {
		if (player->streamer)
		{
			__mm_player_streaming_deinitialize (player->streamer);
			__mm_player_streaming_destroy(player->streamer);
			player->streamer = NULL;
		}
       }
}

int
_mmplayer_destroy(MMHandleType handle) // @
{
	mm_player_t* player = MM_PLAYER_CAST(handle);

	debug_fenter();

	/* check player handle */
	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* destroy can called at anytime */
	MMPLAYER_CHECK_STATE_RETURN_IF_FAIL ( player, MMPLAYER_COMMAND_DESTROY );

	__mmplayer_release_extended_streaming(player);

	/* release repeat thread */
	if ( player->repeat_thread_cond &&
		 player->repeat_thread_mutex &&
		 player->repeat_thread )
	{
		player->repeat_thread_exit = TRUE;
		g_cond_signal( player->repeat_thread_cond );

		debug_log("waitting for repeat thread exit\n");
		g_thread_join ( player->repeat_thread );
		g_mutex_free ( player->repeat_thread_mutex );
		g_cond_free ( player->repeat_thread_cond );
		debug_log("repeat thread released\n");
	}

	if (MM_ERROR_NONE != _mmplayer_release_video_capture(player))
	{
		debug_error("failed to release video capture\n");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	/* withdraw asm */
	if ( MM_ERROR_NONE != _mmplayer_asm_deregister(&player->sm) )
	{
		debug_error("failed to deregister asm server\n");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	/* release pipeline */
	if ( MM_ERROR_NONE != __mmplayer_gst_destroy_pipeline( player ) )
	{
		debug_error("failed to destory pipeline\n");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	/* release attributes */
	_mmplayer_deconstruct_attribute( handle );

	/* release factories */
	__mmplayer_release_factories( player );

	/* release lock */
	if ( player->fsink_lock )
		g_mutex_free( player->fsink_lock );

	if ( player->msg_cb_lock )
		g_mutex_free( player->msg_cb_lock );

	if (player->lazy_pause_event_id) 
	{
		g_source_remove (player->lazy_pause_event_id);
		player->lazy_pause_event_id = 0;
	}

	debug_fleave();

	return MM_ERROR_NONE;
}


int 
__mmplayer_init_extended_streaming(mm_player_t* player)
{
	int ret = MM_ERROR_NONE;
	
	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	if (MMPLAYER_IS_HTTP_PD(player))
	{
		gboolean bret = FALSE;
		gchar *src_uri = NULL;

		player->pd_downloader = _mmplayer_pd_create ();

		if ( !player->pd_downloader )
		{
			debug_error ("Unable to create PD Downloader...");
			ret = MM_ERROR_PLAYER_NO_FREE_SPACE;
		}
		
		if (player->pd_mode == MM_PLAYER_PD_MODE_URI)
			src_uri = player->profile.uri;
		
		bret = _mmplayer_pd_initialize ((MMHandleType)player, src_uri, player->pd_file_location, player->pipeline->mainbin[MMPLAYER_M_SRC].gst);
		
		if (FALSE == bret)
		{
			debug_error ("Unable to create PD Downloader...");
			ret = MM_ERROR_PLAYER_NOT_INITIALIZED;
		}
	}

	return ret;
}

int
_mmplayer_realize(MMHandleType hplayer) // @
{
	mm_player_t* player =  (mm_player_t*)hplayer;
	char *uri =NULL;
	void *param = NULL;
	int application_pid = -1;
	gboolean update_registry = FALSE;
	MMHandleType attrs = 0;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	/* check player handle */
	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED )

	/* check current state */
	MMPLAYER_CHECK_STATE_RETURN_IF_FAIL( player, MMPLAYER_COMMAND_REALIZE );

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("fail to get attributes.\n");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	mm_attrs_get_int_by_name(attrs, "sound_application_pid", &application_pid );
	player->sm.pid = application_pid;

	mm_attrs_get_string_by_name(attrs, "profile_uri", &uri);
	mm_attrs_get_data_by_name(attrs, "profile_user_param", &param);

	if (! __mmfplayer_parse_profile((const char*)uri, param, &player->profile) )
	{
		debug_error("failed to parse profile\n");
		return MM_ERROR_PLAYER_INVALID_URI;
	}

	/* FIXIT : we can use thouse in player->profile directly */
	if (player->profile.uri_type == MM_PLAYER_URI_TYPE_MEM)
	{
		player->mem_buf.buf = (char *)player->profile.mem;
		player->mem_buf.len = player->profile.mem_size;
		player->mem_buf.offset = 0;
	}

	if (player->profile.uri_type == MM_PLAYER_URI_TYPE_URL_MMS)
	{
		debug_warning("mms protocol is not supported format.\n");
		return MM_ERROR_PLAYER_NOT_SUPPORTED_FORMAT;
	}

	if (MMPLAYER_IS_STREAMING(player))
		MMPLAYER_STATE_CHANGE_TIMEOUT(player) = PLAYER_INI()->live_state_change_timeout;
	else
		MMPLAYER_STATE_CHANGE_TIMEOUT(player) = PLAYER_INI()->localplayback_state_change_timeout;

	player->videodec_linked  = 0;
	player->videosink_linked = 0;
	player->audiodec_linked  = 0;
	player->audiosink_linked = 0;
	player->textsink_linked = 0;

	/* set the subtitle ON default */
	player->is_subtitle_off = FALSE;

	/* we need to update content attrs only the content has changed */
	player->need_update_content_attrs = TRUE;
	player->need_update_content_dur = FALSE;

	/* registry should be updated for downloadable codec */
	mm_attrs_get_int_by_name(attrs, "profile_update_registry", &update_registry);

	if ( update_registry )
	{
		debug_log("updating registry...\n");
		gst_update_registry();

		/* then we have to rebuild factories */
		__mmplayer_release_factories( player );
		__mmplayer_init_factories(player);
	}

	/* realize pipeline */
	ret = __gst_realize( player );
	if ( ret != MM_ERROR_NONE )
	{
		debug_error("fail to realize the player.\n");
	}
	else
	{
		__mmplayer_init_extended_streaming(player);
	}

	debug_fleave();

	return ret;
}

int
__mmplayer_deinit_extended_streaming(mm_player_t *player)
{
	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	
	/* destroy can called at anytime */
	if (player->pd_downloader && MMPLAYER_IS_HTTP_PD(player))
	{
		_mmplayer_pd_stop ((MMHandleType)player);
	}

	return MM_ERROR_NONE;

}

int
_mmplayer_unrealize(MMHandleType hplayer) // @
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED )

	/* check current state */
	MMPLAYER_CHECK_STATE_RETURN_IF_FAIL( player, MMPLAYER_COMMAND_UNREALIZE );

	__mmplayer_deinit_extended_streaming(player);

	/* unrealize pipeline */
	ret = __gst_unrealize( player );

	/* set player state if success */
	if ( MM_ERROR_NONE == ret )
	{
		ret = _mmplayer_asm_set_state(hplayer, ASM_STATE_STOP);
		if ( ret )
		{
			debug_error("failed to set asm state to STOP\n");
			return ret;
		}
	}

	debug_fleave();

	return ret;
}

int
_mmplayer_set_message_callback(MMHandleType hplayer, MMMessageCallback callback, gpointer user_param) // @
{
	mm_player_t* player = (mm_player_t*)hplayer;

    	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	return __gst_set_message_callback(player, callback, user_param);
}

int
_mmplayer_get_state(MMHandleType hplayer, int* state) // @
{
	mm_player_t *player = (mm_player_t*)hplayer;

	return_val_if_fail(state, MM_ERROR_INVALID_ARGUMENT);

	*state = MMPLAYER_CURRENT_STATE(player);

	return MM_ERROR_NONE;
}


int
_mmplayer_set_volume(MMHandleType hplayer, MMPlayerVolumeType volume) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;
	GstElement* vol_element = NULL;
	int i = 0;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	debug_log("volume [L]=%f:[R]=%f\n", 
		volume.level[MM_VOLUME_CHANNEL_LEFT], volume.level[MM_VOLUME_CHANNEL_RIGHT]);

	/* invalid factor range or not */
	for ( i = 0; i < MM_VOLUME_CHANNEL_NUM; i++ )
	{
		if (volume.level[i] < MM_VOLUME_FACTOR_MIN || volume.level[i] > MM_VOLUME_FACTOR_MAX) {
			debug_error("Invalid factor! (valid factor:0~1.0)\n");
			return MM_ERROR_INVALID_ARGUMENT;
		}
	}

	/* Save volume to handle. Currently the first array element will be saved. */
	player->sound.volume = volume.level[0];

	/* check pipeline handle */
	if ( ! player->pipeline || ! player->pipeline->audiobin )
	{
		debug_log("audiobin is not created yet\n");
		debug_log("but, current stored volume will be set when it's created.\n");

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
		debug_log("volume is set [%f]\n", player->sound.volume);
		g_object_set(vol_element, "volume", player->sound.volume, NULL);
	}

	debug_fleave();

	return MM_ERROR_NONE;
}


int
_mmplayer_get_volume(MMHandleType hplayer, MMPlayerVolumeType* volume)
{
	mm_player_t* player = (mm_player_t*) hplayer;
	int i = 0;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail( volume, MM_ERROR_INVALID_ARGUMENT );

	/* returning stored volume */
	for (i = 0; i < MM_VOLUME_CHANNEL_NUM; i++)
		volume->level[i] = player->sound.volume;

	debug_fleave();

	return MM_ERROR_NONE;
}



int
_mmplayer_set_mute(MMHandleType hplayer, int mute) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;
	GstElement* vol_element = NULL;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	debug_log("mute : %d\n", mute);

	/* mute value shoud 0 or 1 */
	if ( mute != 0 && mute != 1 )
	{
		debug_error("bad mute value\n");

		/* FIXIT : definitly, we need _BAD_PARAM error code */
		return MM_ERROR_INVALID_ARGUMENT;
	}


	/* just hold mute value if pipeline is not ready */
	if ( !player->pipeline || !player->pipeline->audiobin )
	{
		debug_log("pipeline is not ready. holding mute value\n");
		player->sound.mute = mute;
		return MM_ERROR_NONE;
	}


	vol_element = player->pipeline->audiobin[MMPLAYER_A_VOL].gst;

	/* NOTE : volume will only created when the bt is enabled */
	if ( vol_element )
	{
		g_object_set(vol_element, "mute", mute, NULL);
	}
	else
	{
		debug_log("volume elemnet is not created. using volume in audiosink\n");
	}

	player->sound.mute = mute;

	debug_fleave();

	return MM_ERROR_NONE;
}

int
_mmplayer_get_mute(MMHandleType hplayer, int* pmute) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;
	GstElement* vol_element = NULL;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( pmute, MM_ERROR_INVALID_ARGUMENT );

	/* just hold mute value if pipeline is not ready */
	if ( !player->pipeline || !player->pipeline->audiobin )
	{
		debug_log("pipeline is not ready. returning stored value\n");
		*pmute = player->sound.mute;
		return MM_ERROR_NONE;
	}


	vol_element = player->pipeline->audiobin[MMPLAYER_A_VOL].gst;

	if ( vol_element )
	{
		g_object_get(vol_element, "mute", pmute, NULL);
		debug_log("mute=%d\n\n", *pmute);
	}
	else
	{
		*pmute = player->sound.mute;
	}

	debug_fleave();

	return MM_ERROR_NONE;
}

int
_mmplayer_set_videostream_cb(MMHandleType hplayer, mm_player_video_stream_callback callback, void *user_param) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( callback, MM_ERROR_INVALID_ARGUMENT );

    	player->video_stream_cb = callback;
    	player->video_stream_cb_user_param = user_param;
    	player->use_video_stream = TRUE;
    	debug_log("Stream cb Handle value is %p : %p\n", player, player->video_stream_cb);

	debug_fleave();

	return MM_ERROR_NONE;
}

int
_mmplayer_set_audiostream_cb(MMHandleType hplayer, mm_player_audio_stream_callback callback, void *user_param) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail(callback, MM_ERROR_INVALID_ARGUMENT);

    	player->audio_stream_cb = callback;
    	player->audio_stream_cb_user_param = user_param;
    	debug_log("Audio Stream cb Handle value is %p : %p\n", player, player->audio_stream_cb);

	debug_fleave();

	return MM_ERROR_NONE;
}

int
_mmplayer_set_audiobuffer_cb(MMHandleType hplayer, mm_player_audio_stream_callback callback, void *user_param) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail(callback, MM_ERROR_INVALID_ARGUMENT);

    	player->audio_buffer_cb = callback;
    	player->audio_buffer_cb_user_param = user_param;
    	debug_log("Audio Stream cb Handle value is %p : %p\n", player, player->audio_buffer_cb);

	debug_fleave();

    	return MM_ERROR_NONE;
}

int
_mmplayer_set_buffer_need_data_cb(MMHandleType hplayer, mm_player_buffer_need_data_callback callback, void *user_param) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;

	debug_fenter();

	return_val_if_fail ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail(callback, MM_ERROR_INVALID_ARGUMENT);

	player->need_data_cb = callback;
    	player->buffer_cb_user_param = user_param;

    	debug_log("buffer need dataHandle value is %p : %p\n", player, player->need_data_cb);

	debug_fleave();

	return MM_ERROR_NONE;
}

int
_mmplayer_set_buffer_enough_data_cb(MMHandleType hplayer, mm_player_buffer_enough_data_callback callback, void *user_param) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;

	debug_fenter();

	return_val_if_fail ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail(callback, MM_ERROR_INVALID_ARGUMENT);

    	player->enough_data_cb = callback;
    	player->buffer_cb_user_param = user_param;

    	debug_log("buffer enough data cb Handle value is %p : %p\n", player, player->enough_data_cb);

	debug_fleave();
		
    	return MM_ERROR_NONE;
}

int
_mmplayer_set_buffer_seek_data_cb(MMHandleType hplayer, mm_player_buffer_seek_data_callback callback, void *user_param) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;

	debug_fenter();

	return_val_if_fail ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail(callback, MM_ERROR_INVALID_ARGUMENT);

	player->seek_data_cb = callback;
    	player->buffer_cb_user_param = user_param;

    	debug_log("buffer seek data cb Handle value is %p : %p\n", player, player->seek_data_cb);

	debug_fleave();

    	return MM_ERROR_NONE;
}

int __mmplayer_start_extended_streaming(mm_player_t *player)
{	
	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	if (MMPLAYER_IS_HTTP_PD(player) && player->pd_downloader)
	{
		if (player->pd_mode == MM_PLAYER_PD_MODE_URI)
		{
			gboolean bret = FALSE;
			
			bret = _mmplayer_pd_start ((MMHandleType)player);
			if (FALSE == bret)
			{
				debug_error ("ERROR while starting PD...\n");
				return MM_ERROR_PLAYER_NOT_INITIALIZED;
			}
		}
	}

	return MM_ERROR_NONE;
}

int
_mmplayer_start(MMHandleType hplayer) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;
	gint ret = MM_ERROR_NONE;
	
	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check current state */
	MMPLAYER_CHECK_STATE_RETURN_IF_FAIL( player, MMPLAYER_COMMAND_START );

	ret = _mmplayer_asm_set_state(hplayer, ASM_STATE_PLAYING);
	if ( ret != MM_ERROR_NONE )
	{
		debug_error("failed to set asm state to PLAYING\n");
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
			debug_error("failed to realize before starting. only in streamming\n");
			return ret;
		}
	}

	if (__mmplayer_start_extended_streaming(player) != MM_ERROR_NONE)
		return MM_ERROR_PLAYER_INTERNAL;
	
	/* start pipeline */
	ret = __gst_start( player );
	if ( ret != MM_ERROR_NONE )
	{
		debug_error("failed to start player.\n");
	}

	debug_fleave();
	
	return ret;
}

/* NOTE: post "not supported codec message" to application
 * when one codec is not found during AUTOPLUGGING in MSL.
 * So, it's separated with error of __mmplayer_gst_callback().
 * And, if any codec is not found, don't send message here.
 * Because GST_ERROR_MESSAGE is posted by other plugin internally.
 */
int
__mmplayer_post_missed_plugin(mm_player_t* player)
{
	MMMessageParamType msg_param;
	memset (&msg_param, 0, sizeof(MMMessageParamType));
	gboolean post_msg_direct = FALSE;

	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	debug_log("not_supported_codec = 0x%02x, can_support_codec = 0x%02x\n",
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
				debug_warning("not found AUDIO codec, posting error code to application.\n");

				msg_param.code = MM_ERROR_PLAYER_AUDIO_CODEC_NOT_FOUND;
				msg_param.data = g_strdup_printf("%s", player->unlinked_audio_mime);
			}
			else if ( player->not_supported_codec ==  MISSING_PLUGIN_VIDEO )
			{
				debug_warning("not found VIDEO codec, posting error code to application.\n");

				msg_param.code = MM_ERROR_PLAYER_VIDEO_CODEC_NOT_FOUND;
				msg_param.data = g_strdup_printf("%s", player->unlinked_video_mime);
			}

			MMPLAYER_POST_MSG( player, MM_MESSAGE_ERROR, &msg_param );

			MMPLAYER_FREEIF(msg_param.data);

			return MM_ERROR_NONE;
		}
		else // no any supported codec case 
		{
			debug_warning("not found any codec, posting error code to application.\n");
			
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
	
	debug_fleave();

	return MM_ERROR_NONE;
}

/* NOTE : it should be able to call 'stop' anytime*/
int
_mmplayer_stop(MMHandleType hplayer) // @
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check current state */
	MMPLAYER_CHECK_STATE_RETURN_IF_FAIL( player, MMPLAYER_COMMAND_STOP );

	/* NOTE : application should not wait for EOS after calling STOP */
	__mmplayer_cancel_delayed_eos( player );

	/* stop pipeline */
	ret = __gst_stop( player );

	if ( ret != MM_ERROR_NONE )
	{
		debug_error("failed to stop player.\n");
	}

	debug_fleave();

	return ret;
}

int
_mmplayer_pause(MMHandleType hplayer) // @
{
	mm_player_t* player = (mm_player_t*)hplayer;
	GstFormat fmt = GST_FORMAT_TIME;
	gint64 pos_msec = 0;
	gboolean async = FALSE;
	gint ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check current state */
	MMPLAYER_CHECK_STATE_RETURN_IF_FAIL( player, MMPLAYER_COMMAND_PAUSE );

	switch (MMPLAYER_CURRENT_STATE(player))
	{
		case MM_PLAYER_STATE_READY:
		{
			/* check prepare async or not.
			 * In the case of streaming playback, it's recommned to avoid blocking wait.
			 */
			mm_attrs_get_int_by_name(player->attrs, "profile_prepare_async", &async);
			debug_log("prepare mode : %s", (async ? "async" : "sync"));
		}
		break;

		case MM_PLAYER_STATE_PLAYING:
		{
			/* NOTE : store current point to overcome some bad operation
			* ( returning zero when getting current position in paused state) of some
			* elements
			*/
			ret = gst_element_query_position(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst,	&fmt, &pos_msec);
			if ( ! ret )
			debug_warning("getting current position failed in paused\n");

			player->last_position = pos_msec;
		}
		break;
	}

	/* pause pipeline */
	ret = __gst_pause( player, async );

	if ( ret != MM_ERROR_NONE )
	{
		debug_error("failed to pause player.\n");
	}

	debug_fleave();

	return ret;
}

int
_mmplayer_resume(MMHandleType hplayer)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	ret = _mmplayer_asm_set_state(hplayer, ASM_STATE_PLAYING);
	if ( ret )
	{
		debug_error("failed to set asm state to PLAYING\n");
		return ret;
	}

	/* check current state */
	MMPLAYER_CHECK_STATE_RETURN_IF_FAIL( player, MMPLAYER_COMMAND_RESUME );

	/* resume pipeline */
	ret = __gst_resume( player, FALSE );

	if ( ret != MM_ERROR_NONE )
	{
		debug_error("failed to resume player.\n");
	}


	debug_fleave();

	return ret;
}

int
__mmplayer_set_play_count(mm_player_t* player, gint count)
{
	MMHandleType attrs = 0;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	attrs =  MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("fail to get attributes.\n");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	mm_attrs_set_int_by_name(attrs, "profile_play_count", count);
	if ( mmf_attrs_commit ( attrs ) ) /* return -1 if error */
		debug_error("failed to commit\n");

	debug_fleave();

	return	MM_ERROR_NONE;
}

int
_mmplayer_activate_section_repeat(MMHandleType hplayer, unsigned long start, unsigned long end)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	gint64 start_pos = 0;
	gint64 end_pos = 0;
	gint infinity = -1;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( end <= GST_TIME_AS_MSECONDS(player->duration), MM_ERROR_INVALID_ARGUMENT );

	player->section_repeat = TRUE;
	player->section_repeat_start = start;
	player->section_repeat_end = end;

	start_pos = player->section_repeat_start * G_GINT64_CONSTANT(1000000);
	end_pos = player->section_repeat_end * G_GINT64_CONSTANT(1000000);

	__mmplayer_set_play_count( player, infinity );

	if ( (!__gst_seek( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst,
					1.0,
					GST_FORMAT_TIME,
					( GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE ),
					GST_SEEK_TYPE_SET, start_pos,
					GST_SEEK_TYPE_SET, end_pos)))
	{
		debug_error("failed to activate section repeat\n");

		return MM_ERROR_PLAYER_SEEK;
	}

	debug_log("succeeded to set section repeat from %d to %d\n",
		player->section_repeat_start, player->section_repeat_end);

	debug_fleave();

	return	MM_ERROR_NONE;
}

static int 
__mmplayer_set_pcm_extraction(mm_player_t* player)
{
	guint64 start_nsec = 0;
	guint64 end_nsec = 0;
	guint64 dur_nsec = 0;
	guint64 dur_msec = 0;
	GstFormat fmt = GST_FORMAT_TIME;
	int required_start = 0;
	int required_end = 0;
	int ret = 0;

	debug_fenter();

	return_val_if_fail( player, FALSE );

	mm_attrs_multiple_get(player->attrs,
		NULL,
		"pcm_extraction_start_msec", &required_start,
		"pcm_extraction_end_msec", &required_end,
		NULL);

	debug_log("pcm extraction required position is from [%d] to [%d] (msec)\n", required_start, required_end);

	if (required_start == 0 && required_end == 0)
	{
		debug_log("extracting entire stream");
		return MM_ERROR_NONE;
	}
	else if (required_start < 0 || required_start > required_end || required_end < 0 )
	{
		debug_log("invalid range for pcm extraction");
		return MM_ERROR_INVALID_ARGUMENT;
	}

	/* get duration */
	ret = gst_element_query_duration(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, &fmt, &dur_nsec);
	if ( !ret )
	{
		debug_error("failed to get duration");
		return MM_ERROR_PLAYER_INTERNAL;
	}
	dur_msec = GST_TIME_AS_MSECONDS(dur_nsec);

	if (dur_msec < required_end) // FIXME
	{
		debug_log("invalid end pos for pcm extraction");
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
		debug_error("failed to seek for pcm extraction\n");

		return MM_ERROR_PLAYER_SEEK;
	}

	debug_log("succeeded to set up segment extraction from [%llu] to [%llu] (nsec)\n", start_nsec, end_nsec);

	debug_fleave();	

	return MM_ERROR_NONE;
}

int
_mmplayer_deactivate_section_repeat(MMHandleType hplayer)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	gint64 cur_pos = 0;
	GstFormat fmt  = GST_FORMAT_TIME;
	gint onetime = 1;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	player->section_repeat = FALSE;

	__mmplayer_set_play_count( player, onetime );

	gst_element_query_position(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, &fmt, &cur_pos);

	if ( (!__gst_seek( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst,
					1.0,
					GST_FORMAT_TIME,
					( GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE ),
					GST_SEEK_TYPE_SET, cur_pos,
					GST_SEEK_TYPE_SET, player->duration )))
	{
		debug_error("failed to deactivate section repeat\n");

		return MM_ERROR_PLAYER_SEEK;
	}

	debug_fenter();

	return MM_ERROR_NONE;
}

int
_mmplayer_set_playspeed(MMHandleType hplayer, gdouble rate)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	signed long long pos_msec = 0;
	int ret = MM_ERROR_NONE;
	int mute = FALSE;
	GstFormat format =GST_FORMAT_TIME;
	MMPlayerStateType current_state = MM_PLAYER_STATE_NONE;
	debug_fenter();

	return_val_if_fail ( player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( !MMPLAYER_IS_STREAMING(player), MM_ERROR_NOT_SUPPORT_API );

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
		ret = gst_element_query_position(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, &format, &pos_msec);

	debug_log ("pos_msec = %"GST_TIME_FORMAT" and ret = %d and state = %d", GST_TIME_ARGS (pos_msec), ret, current_state);

	if ( ( current_state == MM_PLAYER_STATE_PAUSED )
		|| ( ! ret ))
		//|| ( player->last_position != 0 && pos_msec == 0 ) )
	{
		debug_warning("returning last point : %lld\n", player->last_position );
		pos_msec = player->last_position;
	}

	if ((!gst_element_seek (player->pipeline->mainbin[MMPLAYER_M_PIPE].gst,
				rate,
				GST_FORMAT_TIME,
				( GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE ),
				//( GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_KEY_UNIT),
				GST_SEEK_TYPE_SET, pos_msec,
				//GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE,
                GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)))
	{
    		debug_error("failed to set speed playback\n");
		return MM_ERROR_PLAYER_SEEK;
	}

	debug_log("succeeded to set speed playback as %fl\n", rate);

	debug_fleave();

	return MM_ERROR_NONE;;
}

int
_mmplayer_set_position(MMHandleType hplayer, int format, int position) // @
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	ret = __gst_set_position ( player, format, (unsigned long)position, FALSE );

	debug_fleave();	

	return ret;
}

int
_mmplayer_get_position(MMHandleType hplayer, int format, unsigned long *position) // @
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int ret = MM_ERROR_NONE;

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	ret = __gst_get_position ( player, format, position );
	
	return ret;
}

int
_mmplayer_get_buffer_position(MMHandleType hplayer, int format, unsigned long* start_pos, unsigned long* stop_pos) // @
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int ret = MM_ERROR_NONE;

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	ret = __gst_get_buffer_position ( player, format, start_pos, stop_pos );

	return ret;
}

int
_mmplayer_adjust_subtitle_postion(MMHandleType hplayer, int format, int position) // @
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	ret = __gst_adjust_subtitle_position(player, format, position);

	debug_fleave();

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
		debug_log("midi\n");

		return TRUE;
	}

	debug_log("not midi.\n");

	return FALSE;
}

static gboolean
__mmplayer_is_amr_type (gchar *str_caps)
{
	if ((g_strrstr(str_caps, "AMR")) ||
		(g_strrstr(str_caps, "amr")))
	{
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
__mmplayer_typefind_have_type(  GstElement *tf, guint probability,  // @
GstCaps *caps, gpointer data)
{
	mm_player_t* player = (mm_player_t*)data;
	GstPad* pad = NULL;

	debug_fenter();

	return_if_fail( player && tf && caps );

	/* store type string */
	MMPLAYER_FREEIF(player->type);
	player->type = gst_caps_to_string(caps);
	if (player->type)
		debug_log("meida type %s found, probability %d%% / %d\n", player->type, probability, gst_caps_get_size(caps));

	/* midi type should be stored because it will be used to set audio gain in avsysauiosink */
	if ( __mmplayer_is_midi_type(player->type))
	{
		player->profile.play_mode = MM_PLAYER_MODE_MIDI;
	}
	else if (__mmplayer_is_amr_type(player->type))
	{
		player->bypass_sound_effect = FALSE;
		if ( (PLAYER_INI()->use_audio_filter_preset || PLAYER_INI()->use_audio_filter_custom) )
		{
			if ( player->audio_filter_info.filter_type == MM_AUDIO_FILTER_TYPE_PRESET )
			{
				if (!_mmplayer_sound_filter_preset_apply(player, player->audio_filter_info.preset))
				{
					debug_msg("apply sound effect(preset:%d) setting success\n",player->audio_filter_info.preset);
				}
			}
			else if ( player->audio_filter_info.filter_type == MM_AUDIO_FILTER_TYPE_CUSTOM )
			{
				if (!_mmplayer_sound_filter_custom_apply(player))
				{
					debug_msg("apply sound effect(custom) setting success\n");
				}
			}
		}
	}
	else if ( g_strrstr(player->type, "application/x-hls"))
	{
		/* If it can't know exact type when it parses uri because of redirection case,
		  * it will be fixed by typefinder here.
		  */
		player->profile.uri_type = MM_PLAYER_URI_TYPE_HLS;
	}

	pad = gst_element_get_static_pad(tf, "src");
	if ( !pad )
	{
		debug_error("fail to get typefind src pad.\n");
		return;
	}


	/* try to plug */
	if ( ! __mmplayer_try_to_plug( player, pad, caps ) )
	{
		debug_error("failed to autoplug for type : %s\n", player->type);

		if ( ( PLAYER_INI()->async_start ) &&
		( player->posted_msg == FALSE ) )
		{
			__mmplayer_post_missed_plugin( player );
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

DONE:
	gst_object_unref( GST_OBJECT(pad) );

	debug_fleave();	

	return;
}

static gboolean
__mmplayer_warm_up_video_codec( mm_player_t* player,  GstElementFactory *factory)
{
	GstElement *element;
	GstStateChangeReturn  ret;
	gboolean usable = TRUE;

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( factory, MM_ERROR_COMMON_INVALID_ARGUMENT );

	element = gst_element_factory_create (factory, NULL);

	ret = gst_element_set_state (element, GST_STATE_READY);

	if (ret != GST_STATE_CHANGE_SUCCESS)
	{
		debug_error ("resource conflict so,  %s unusable\n", GST_PLUGIN_FEATURE_NAME (factory));
		usable = FALSE;
	}

	gst_element_set_state (element, GST_STATE_NULL);
	gst_object_unref (element);

	return usable;
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

	debug_fenter();

	return_val_if_fail( player &&
						player->pipeline &&
						player->pipeline->mainbin,
						FALSE );


	mainbin = player->pipeline->mainbin;

    	mime = gst_structure_get_name(gst_caps_get_structure(caps, 0));

	/* return if we got raw output */
    	if(g_str_has_prefix(mime, "video/x-raw") || g_str_has_prefix(mime, "audio/x-raw") ||g_str_has_prefix(mime, "text/plain") )
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

		klass = gst_element_factory_get_klass( gst_element_get_factory(element) );

		/* add queue if needed */
	    	if( g_strrstr(klass, "Demux") ||
	       	g_strrstr(klass, "Depayloader") ||
	        	g_strrstr(klass, "Parse") )
	    	{
			debug_log("adding raw queue\n");

			queue = gst_element_factory_make("queue", NULL);
			if ( ! queue )
			{
				debug_warning("failed to create queue\n");
				goto ERROR;
			}

			/* warmup */
			if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state(queue, GST_STATE_READY) )
			{
				debug_warning("failed to set state READY to queue\n");
				goto ERROR;
			}

			/* add to pipeline */
			if ( ! gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), queue) )
			{
				debug_warning("failed to add queue\n");
				goto ERROR;
			}

			/* link queue */
			queue_pad = gst_element_get_static_pad(queue, "sink");

			if ( GST_PAD_LINK_OK != gst_pad_link(pad, queue_pad) )
			{
				debug_warning("failed to link queue\n");
				goto ERROR;
			}
			gst_object_unref ( GST_OBJECT(queue_pad) );
			queue_pad = NULL;

			/* running */
			if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state(queue, GST_STATE_PAUSED) )
			{
				debug_warning("failed to set state READY to queue\n");
				goto ERROR;
			}

			/* replace given pad to queue:src */
			pad = gst_element_get_static_pad(queue, "src");
			if ( ! pad )
			{
				debug_warning("failed to get pad from queue\n");
				goto ERROR;
			}
		}
#endif
		/* check if player can do start continually */
		MMPLAYER_CHECK_CMD_IF_EXIT(player);

		if(__mmplayer_link_sink(player,pad))
       		 __mmplayer_gst_decode_callback(element, pad, FALSE, player);

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
		for ( idx = 0; PLAYER_INI()->exclude_element_keyword[idx][0] != '\0'; idx++ )
		{
			if ( g_strrstr(GST_PLUGIN_FEATURE_NAME (factory),
					PLAYER_INI()->exclude_element_keyword[idx] ) )
			{
				debug_warning("skipping [%s] by exculde keyword [%s]\n",
					GST_PLUGIN_FEATURE_NAME (factory),
					PLAYER_INI()->exclude_element_keyword[idx] );

				skip = TRUE;
				break;
			}
		}

		if ( skip ) continue;


		/* check factory class for filtering */
		klass = gst_element_factory_get_klass(GST_ELEMENT_FACTORY(factory));

		/* NOTE : msl don't need to use image plugins.
		 * So, those plugins should be skipped for error handling.
		 */
		if ( g_strrstr(klass, "Codec/Decoder/Image") )
		{
			debug_log("player doesn't need [%s] so, skipping it\n",
				GST_PLUGIN_FEATURE_NAME (factory) );

			continue;
		}


		/* check pad compatability */
	        for(pads = gst_element_factory_get_static_pad_templates(factory);
    	             pads != NULL; pads=pads->next)
 	       {
     		       GstStaticPadTemplate *temp1 = pads->data;
			GstCaps* static_caps = NULL;

	            	if( temp1->direction != GST_PAD_SINK ||
	                	temp1->presence != GST_PAD_ALWAYS)
	                	continue;


			if ( GST_IS_CAPS( &temp1->static_caps.caps) )
			{
				/* using existing caps */
				static_caps = gst_caps_ref( &temp1->static_caps.caps );
			}
			else
			{
				/* create one */
				static_caps = gst_caps_from_string ( temp1->static_caps.string );
			}

            		res = gst_caps_intersect(caps, static_caps);

			gst_caps_unref( static_caps );
			static_caps = NULL;

            		if( res && !gst_caps_is_empty(res) )
            		{
                		GstElement *new_element;
				GList *elements = player->parsers;
                		char *name_template = g_strdup(temp1->name_template);
				gchar *name_to_plug = GST_PLUGIN_FEATURE_NAME(factory);

                		gst_caps_unref(res);

				debug_log("found %s to plug\n", name_to_plug);

				new_element = gst_element_factory_create(GST_ELEMENT_FACTORY(factory), NULL);
				if ( ! new_element )
				{
					debug_error("failed to create element [%s]. continue with next.\n",
						GST_PLUGIN_FEATURE_NAME (factory));

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
							debug_log("but, %s already linked, so skipping it\n", name_to_plug);
							skip = TRUE;
						}
					}

					if (skip) continue;

					selected = g_strdup(name_to_plug);

					player->parsers = g_list_append(player->parsers, selected);
				}

				/* store specific handles for futher control */
	                	if(g_strrstr(klass, "Demux") || g_strrstr(klass, "Parse"))
	               	{
		                	/* FIXIT : first value will be overwritten if there's more
		                	 * than 1 demuxer/parser
		                	 */
	                		debug_log("plugged element is demuxer. take it\n");
                    			mainbin[MMPLAYER_M_DEMUX].id = MMPLAYER_M_DEMUX;
                    			mainbin[MMPLAYER_M_DEMUX].gst = new_element;
	                	}
                		else if(g_strrstr(klass, "Decoder") && __mmplayer_link_decoder(player,pad))
                		{									
		                    if(mainbin[MMPLAYER_M_DEC1].gst == NULL)
		                    {
			                    	debug_log("plugged element is decoder. take it[MMPLAYER_M_DEC1]\n");
		                        	mainbin[MMPLAYER_M_DEC1].id = MMPLAYER_M_DEC1;
		                        	mainbin[MMPLAYER_M_DEC1].gst = new_element;
		                    }
		                    else if(mainbin[MMPLAYER_M_DEC2].gst == NULL)
		                    {
			                    	debug_log("plugged element is decoder. take it[MMPLAYER_M_DEC2]\n");
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
						gchar *caps_type = NULL;
							
						debug_log("found VIDEO decoder\n");
						player->not_supported_codec &= MISSING_PLUGIN_AUDIO;
						player->can_support_codec |= FOUND_PLUGIN_VIDEO;

						src_pad = gst_element_get_static_pad (new_element, "src");
						pad_templ = gst_pad_get_pad_template (src_pad);
						caps = GST_PAD_TEMPLATE_CAPS(pad_templ);

						caps_type = gst_caps_to_string(caps);

						if ( g_strrstr( caps_type, "ST12") )
							player->is_nv12_tiled = TRUE;

						/* clean */
						MMPLAYER_FREEIF( caps_type );
						gst_object_unref (src_pad);
					}
					else if (g_str_has_prefix(mime, "audio"))
					{
						debug_log("found AUDIO decoder\n");
						player->not_supported_codec &= MISSING_PLUGIN_VIDEO;
						player->can_support_codec |= FOUND_PLUGIN_AUDIO;
					}
                		}
	                	if ( ! __mmplayer_close_link(player, pad, new_element,
	                                    name_template,gst_element_factory_get_static_pad_templates(factory)) )
	            		{
					if (player->keep_detecting_vcodec)
						continue;

	            			/* Link is failed even though a supportable codec is found. */
					__mmplayer_check_not_supported_codec(player, (gchar *)mime);

	            			MMPLAYER_FREEIF(name_template);
					debug_error("failed to call _close_link\n");
					return FALSE;
	            		}

	                	MMPLAYER_FREEIF(name_template);
	                	return TRUE;
            		}

			gst_caps_unref(res);

			break;
        	}
    	}

	/* There is no any found codec. */
	__mmplayer_check_not_supported_codec(player,(gchar *)mime);

	debug_error("failed to autoplug\n");

	debug_fleave();
	
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


static
int __mmplayer_check_not_supported_codec(mm_player_t* player, gchar* mime)
{
	debug_fenter();

	return_val_if_fail(player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail ( mime, MM_ERROR_INVALID_ARGUMENT );

	debug_log("mimetype to check: %s\n", mime );

	/* add missing plugin */
	/* NOTE : msl should check missing plugin for image mime type.
	 * Some motion jpeg clips can have playable audio track.
	 * So, msl have to play audio after displaying popup written video format not supported.
	 */
	if ( !( player->pipeline->mainbin[MMPLAYER_M_DEMUX].gst ) )
	{
		if ( !( player->can_support_codec | player->videodec_linked | player->audiodec_linked ) )
		{
			debug_log("not found demuxer\n");
			player->not_found_demuxer = TRUE;
			player->unlinked_demuxer_mime = g_strdup_printf ( "%s", mime );

			goto DONE;
		}
	}

	if( ( g_str_has_prefix(mime, "video") ) ||( g_str_has_prefix(mime, "image") ) )
	{
		debug_log("can support codec=%d, vdec_linked=%d, adec_linked=%d\n",
			player->can_support_codec, player->videodec_linked, player->audiodec_linked);

		/* check that clip have multi tracks or not */
		if ( ( player->can_support_codec & FOUND_PLUGIN_VIDEO ) && ( player->videodec_linked ) )
		{
			debug_log("video plugin is already linked\n");
		}
		else
		{
			debug_warning("add VIDEO to missing plugin\n");
			player->not_supported_codec |= MISSING_PLUGIN_VIDEO;
		}
	}
	else if ( g_str_has_prefix(mime, "audio") )
	{
		if ( ( player->can_support_codec & FOUND_PLUGIN_AUDIO ) && ( player->audiodec_linked ) )
		{
			debug_log("audio plugin is already linked\n");
		}
		else
		{
			debug_warning("add AUDIO to missing plugin\n");
			player->not_supported_codec |= MISSING_PLUGIN_AUDIO;
		}
	}

DONE:
    	debug_fleave();	

	return MM_ERROR_NONE;
}


static void __mmplayer_pipeline_complete(GstElement *decodebin,  gpointer data) // @
{
    mm_player_t* player = (mm_player_t*)data;

	debug_fenter();

	return_if_fail( player );

	/* remove fakesink */
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
	debug_log("pipeline has completely constructed\n");

	player->pipeline_is_constructed = TRUE;
	
	if ( ( PLAYER_INI()->async_start ) &&
		( player->posted_msg == FALSE ) )
	{
		__mmplayer_post_missed_plugin( player );
	}
	
	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-complate" );
}

static gboolean __mmplayer_configure_audio_callback(mm_player_t* player)
{
	debug_fenter();

	return_val_if_fail ( player, FALSE );


	if ( MMPLAYER_IS_STREAMING(player) )
		return FALSE;

	/* This callback can be set to music player only. */
	if((player->can_support_codec & 0x02) == FOUND_PLUGIN_VIDEO)
	{
		debug_warning("audio callback is not supported for video");
		return FALSE;
	}

	if (player->audio_stream_cb)
	{
		{
			GstPad *pad = NULL;

			pad = gst_element_get_static_pad (player->pipeline->audiobin[MMPLAYER_A_SINK].gst, "sink");

			if ( !pad )
			{
				debug_error("failed to get sink pad from audiosink to probe data\n");
				return FALSE;
			}

			player->audio_cb_probe_id = gst_pad_add_buffer_probe (pad,
				G_CALLBACK (__mmplayer_audio_stream_probe), player);

			gst_object_unref (pad);

			pad = NULL;
    	       }
	}
	else
	{
		debug_error("There is no audio callback to configure.\n");
		return FALSE;
	}

    	debug_fleave();	

	return TRUE;
}

static void
__mmplayer_init_factories(mm_player_t* player) // @
{
	debug_fenter();

	return_if_fail ( player );

	player->factories = gst_registry_feature_filter(gst_registry_get_default(),
                                        (GstPluginFeatureFilter)__mmplayer_feature_filter, FALSE, NULL);

    	player->factories = g_list_sort(player->factories, (GCompareFunc)util_factory_rank_compare);

    	debug_fleave();	
}

static void
__mmplayer_release_factories(mm_player_t* player) // @
{
	debug_fenter();

	return_if_fail ( player );

	if (player->factories)
	{
		gst_plugin_feature_list_free (player->factories);
		player->factories = NULL;
	}

	debug_fleave();
}

static void
__mmplayer_release_misc(mm_player_t* player)
{
	int i;
	debug_fenter();

	return_if_fail ( player );

	player->use_video_stream = FALSE;
	player->video_stream_cb = NULL;
	player->video_stream_cb_user_param = NULL;

	player->audio_stream_cb = NULL;
	player->audio_stream_cb_user_param = NULL;

	player->audio_buffer_cb = NULL;
	player->audio_buffer_cb_user_param = NULL;

	player->sent_bos = FALSE;
	player->playback_rate = DEFAULT_PLAYBACK_RATE;

	player->doing_seek = FALSE;

	player->streamer = NULL;
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
	player->need_update_content_dur = FALSE;
	player->pending_seek.is_pending = FALSE;
	player->pending_seek.format = MM_PLAYER_POS_FORMAT_TIME;
	player->pending_seek.pos = 0;
	player->posted_msg == FALSE;
	player->has_many_types = FALSE;

	for (i = 0; i < MM_PLAYER_STREAM_COUNT_MAX; i++)
	{
		player->bitrate[i] = 0;
		player->maximum_bitrate[i] = 0;
	}

	/* clean found parsers */
	if (player->parsers)
	{
		g_list_free(player->parsers);
		player->parsers = NULL;
	}

	MMPLAYER_FREEIF(player->album_art);

	/* free memory related to sound effect */
	if(player->audio_filter_info.custom_ext_level_for_plugin)
	{
		free(player->audio_filter_info.custom_ext_level_for_plugin);
	}

	debug_fleave();
}

static GstElement *__mmplayer_element_create_and_link(mm_player_t *player, GstPad* pad, const char* name)
{
	GstElement *element = NULL;
	GstPad *sinkpad;

	debug_log("creating %s to plug\n", name);
	
	element = gst_element_factory_make(name, NULL);
	if ( ! element )
	{
		debug_error("failed to create queue\n");
		return NULL;
	}

	if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state(element, GST_STATE_READY) )
	{
		debug_error("failed to set state READY to %s\n", name);
		return NULL;
	}

	if ( ! gst_bin_add(GST_BIN(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst), element))
	{
		debug_error("failed to add %s\n", name);
		return NULL;
	}

	sinkpad = gst_element_get_static_pad(element, "sink");

	if ( GST_PAD_LINK_OK != gst_pad_link(pad, sinkpad) )
	{
		debug_error("failed to link %s\n", name);
		gst_object_unref (sinkpad);
		
		return NULL;
	}

	debug_log("linked %s to pipeline successfully\n", name);

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
	GstState warmup = GST_STATE_READY;
	gboolean isvideo_decoder = FALSE;
	guint q_max_size_time = 0;

	debug_fenter();

	return_val_if_fail ( player && 
		player->pipeline && 
		player->pipeline->mainbin, 
		FALSE );

	mainbin = player->pipeline->mainbin;

	debug_log("plugging pad %s:%s to newly create %s:%s\n",
					GST_ELEMENT_NAME( GST_PAD_PARENT ( srcpad ) ),
	                GST_PAD_NAME( srcpad ),
	                GST_ELEMENT_NAME( sinkelement ),
	                padname);

	factory = gst_element_get_factory(sinkelement);
	klass = gst_element_factory_get_klass(factory);

	/* check if player can do start continually */
	MMPLAYER_CHECK_CMD_IF_EXIT(player);

	if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state(sinkelement, warmup) )
	{
		if (isvideo_decoder)
			player->keep_detecting_vcodec = TRUE;

		debug_error("failed to set %d state to %s\n", warmup, GST_ELEMENT_NAME( sinkelement ));
		goto ERROR;
	}

	/* add to pipeline */
	if ( ! gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), sinkelement) )
	{
		debug_error("failed to add %s to mainbin\n", GST_ELEMENT_NAME( sinkelement ));
		goto ERROR;
	}

	debug_log("element klass : %s\n", klass);

	/* added to support multi track files */
	/* only decoder case and any of the video/audio still need to link*/
	if(g_strrstr(klass, "Decoder") && __mmplayer_link_decoder(player,srcpad))
	{
		gchar *name = NULL;

		name = g_strdup(GST_ELEMENT_NAME( GST_PAD_PARENT ( srcpad )));
		
		if (g_strrstr(name, "mpegtsdemux"))
		{
			gchar *demux_caps = NULL;
			gchar *parser_name = NULL;
			GstCaps *dcaps = NULL;

			dcaps = gst_pad_get_caps(srcpad);
			demux_caps = gst_caps_to_string(dcaps);
			
			if (g_strrstr(demux_caps, "video/x-h264"))
			{
				parser_name = g_strdup("h264parse");
			}
			else if (g_strrstr(demux_caps, "video/mpeg"))
			{
				parser_name = g_strdup("mpeg4videoparse");
			}
			
			gst_caps_unref(dcaps);
			MMPLAYER_FREEIF( demux_caps );

			if (parser_name)
			{
				parser = __mmplayer_element_create_and_link(player, srcpad, parser_name);

				MMPLAYER_FREEIF(parser_name);
				
				if ( ! parser )
				{
					debug_error("failed to create parser\n");
				}
				else
				{
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
			debug_error("failed to create queue\n");
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

			g_object_set (G_OBJECT (mainbin[MMPLAYER_M_Q1].gst), "max-size-time", q_max_size_time * GST_SECOND, NULL);
		}
		else if(mainbin[MMPLAYER_M_Q2].gst == NULL)
		{
			mainbin[MMPLAYER_M_Q2].id = MMPLAYER_M_Q2;
			mainbin[MMPLAYER_M_Q2].gst = queue;

			g_object_set (G_OBJECT (mainbin[MMPLAYER_M_Q2].gst), "max-size-time", q_max_size_time * GST_SECOND, NULL);
		}
		else
		{
			debug_critical("Not supporting more then two elementary stream\n");
			g_assert(1);
		}

		pad = gst_element_get_static_pad(sinkelement, padname);

		if ( ! pad )
		{
			debug_warning("failed to get pad(%s) from %s. retrying with [sink]\n",
				padname, GST_ELEMENT_NAME(sinkelement) );

			pad = gst_element_get_static_pad(sinkelement, "sink");
 			if ( ! pad )
			{
				debug_error("failed to get pad(sink) from %s. \n",
				GST_ELEMENT_NAME(sinkelement) );
				goto ERROR;
			}
		}

		/*  to check the video/audio type set the proper flag*/
		{
			srccaps = gst_pad_get_caps( srcpad );
			if ( !srccaps )
				goto ERROR;

			str = gst_caps_get_structure( srccaps, 0 );
			if ( ! str )
				goto ERROR;

			name = gst_structure_get_name(str);
			if ( ! name )
				goto ERROR;
		}

		/* link queue and decoder. so, it will be queue - decoder. */
		if ( GST_PAD_LINK_OK != gst_pad_link(srcpad, pad) )
		{
			gst_object_unref(GST_OBJECT(pad));
			debug_error("failed to link (%s) to pad(%s)\n", GST_ELEMENT_NAME( sinkelement ), padname );

			/* reconstitute supportable codec */
			if (strstr(name, "video"))
			{
				player->can_support_codec ^= FOUND_PLUGIN_VIDEO;
			}
			else if (strstr(name, "audio"))
			{
				player->can_support_codec ^= FOUND_PLUGIN_AUDIO;
			}
			goto ERROR;
		}

		if (strstr(name, "video"))
		{
			player->videodec_linked = 1;
			debug_msg("player->videodec_linked set to 1\n");

		}
		else if (strstr(name, "audio"))
		{
			player->audiodec_linked = 1;
			debug_msg("player->auddiodec_linked set to 1\n");
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
				GstFormat fmt  = GST_FORMAT_BYTES;
				gint64 dur_bytes = 0L;
				gchar *file_buffering_path = NULL;
				gboolean use_file_buffer = FALSE;

				if ( !mainbin[MMPLAYER_M_S_BUFFER].gst)
				{
					debug_log("creating http streaming buffering queue\n");

					queue = gst_element_factory_make("queue2", "http_streaming_buffer");
					if ( ! queue )
					{
						debug_critical ( "failed to create buffering queue element\n" );
						goto ERROR;
					}

					if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state(queue, GST_STATE_READY) )
					{
						debug_error("failed to set state READY to buffering queue\n");
						goto ERROR;
					}

					if ( !gst_bin_add(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst), queue) )
					{
						debug_error("failed to add buffering queue\n");
						goto ERROR;
					}

					qsinkpad = gst_element_get_static_pad(queue, "sink");
					qsrcpad = gst_element_get_static_pad(queue, "src");

					if ( GST_PAD_LINK_OK != gst_pad_link(srcpad, qsinkpad) )
					{
						debug_error("failed to link buffering queue\n");
						goto ERROR;
					}
					srcpad = qsrcpad;


					mainbin[MMPLAYER_M_S_BUFFER].id = MMPLAYER_M_S_BUFFER;
					mainbin[MMPLAYER_M_S_BUFFER].gst = queue;

					if ( !MMPLAYER_IS_HTTP_LIVE_STREAMING(player))
	             			 {
						if ( !gst_element_query_duration(player->pipeline->mainbin[MMPLAYER_M_SRC].gst, &fmt, &dur_bytes))
							debug_error("fail to get duration.\n");

						if (dur_bytes>0)
						{
							use_file_buffer = MMPLAYER_USE_FILE_FOR_BUFFERING(player);
							file_buffering_path = g_strdup(PLAYER_INI()->http_file_buffer_path);
						}
					}

					__mm_player_streaming_set_buffer(player->streamer,
						queue,
						TRUE,
						PLAYER_INI()->http_max_size_bytes,
						1.0,
						PLAYER_INI()->http_buffering_limit,
						PLAYER_INI()->http_buffering_time,
						use_file_buffer,
						file_buffering_path,
						dur_bytes);

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
			debug_warning("failed to get pad(%s) from %s. retrying with [sink]\n",
					padname, GST_ELEMENT_NAME(sinkelement) );

			pad = gst_element_get_static_pad(sinkelement, "sink");

			if ( ! pad )
			{
				debug_error("failed to get pad(sink) from %s. \n",
					GST_ELEMENT_NAME(sinkelement) );
				goto ERROR;
			}
		}

		if ( GST_PAD_LINK_OK != gst_pad_link(srcpad, pad) )
		{
			gst_object_unref(GST_OBJECT(pad));
			debug_error("failed to link (%s) to pad(%s)\n", GST_ELEMENT_NAME( sinkelement ), padname );
			goto ERROR;
		}

		gst_object_unref(GST_OBJECT(pad));
	}

	for(;templlist != NULL; templlist = templlist->next)
	{
		padtemplate = templlist->data;

		debug_log ("director = [%d], presence = [%d]\n", padtemplate->direction, padtemplate->presence);

		if(	padtemplate->direction != GST_PAD_SRC ||
			padtemplate->presence == GST_PAD_REQUEST	)
			continue;

		switch(padtemplate->presence)
		{
			case GST_PAD_ALWAYS:
			{
				GstPad *srcpad = gst_element_get_static_pad(sinkelement, "src");
				GstCaps *caps = gst_pad_get_caps(srcpad);

				/* Check whether caps has many types */
				if ( gst_caps_get_size (caps) > 1 && g_strrstr(klass, "Parser")) {
					debug_log ("has_many_types for this caps [%s]\n", gst_caps_to_string(caps));
					has_many_types = TRUE;
					break;
				}

				if ( ! __mmplayer_try_to_plug(player, srcpad, caps) )
				{
					gst_object_unref(GST_OBJECT(srcpad));
					gst_caps_unref(GST_CAPS(caps));

					debug_error("failed to plug something after %s\n", GST_ELEMENT_NAME( sinkelement ));
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
		MMPLAYER_SIGNAL_CONNECT ( player, sinkelement, "pad-added",
			G_CALLBACK(__mmplayer_add_new_pad), player);

		/* for streaming, more then one typefind will used for each elementary stream
		 * so this doesn't mean the whole pipeline completion
		 */
		if ( ! MMPLAYER_IS_RTSP_STREAMING( player ) )
		{
			MMPLAYER_SIGNAL_CONNECT( player, sinkelement, "no-more-pads",
				G_CALLBACK(__mmplayer_pipeline_complete), player);
		}
	}

	if (has_many_types)
	{
		GstPad *pad = NULL;

		player->has_many_types = has_many_types;
		
		pad = gst_element_get_static_pad(sinkelement, "src");
		MMPLAYER_SIGNAL_CONNECT (player, pad, "notify::caps", G_CALLBACK(__mmplayer_add_new_caps), player);
		gst_object_unref (GST_OBJECT(pad));
	}


	/* check if player can do start continually */
	MMPLAYER_CHECK_CMD_IF_EXIT(player);

	if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state(sinkelement, GST_STATE_PAUSED) )
	{
		debug_error("failed to set state PAUSED to %s\n", GST_ELEMENT_NAME( sinkelement ));
		goto ERROR;
	}

	if ( queue )
	{
		if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state (queue, GST_STATE_PAUSED) )
		{
			debug_error("failed to set state PAUSED to queue\n");
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
			debug_error("failed to set state PAUSED to queue\n");
			goto ERROR;
		}

		parser = NULL;

		gst_object_unref (GST_OBJECT(pssrcpad));
		pssrcpad = NULL;
	}

	debug_fleave();

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
    	//const gchar *name;

	/* we only care about element factories */
	if (!GST_IS_ELEMENT_FACTORY(feature))
		return FALSE;

	/* only parsers, demuxers and decoders */
    	klass = gst_element_factory_get_klass(GST_ELEMENT_FACTORY(feature));
    	//name = gst_element_factory_get_longname(GST_ELEMENT_FACTORY(feature));

    	if( g_strrstr(klass, "Demux") == NULL &&
        	g_strrstr(klass, "Codec/Decoder") == NULL &&
        	g_strrstr(klass, "Depayloader") == NULL &&
        	g_strrstr(klass, "Parse") == NULL)
    	{
        	return FALSE;
    	}
    return TRUE;
}


static void 	__mmplayer_add_new_caps(GstPad* pad, GParamSpec* unused, gpointer data)
{
	mm_player_t* player = (mm_player_t*) data;
	GstCaps *caps = NULL;
	GstStructure *str = NULL;
	const char *name;

	debug_fenter();

	return_if_fail ( pad )
	return_if_fail ( unused )
	return_if_fail ( data )

	caps = gst_pad_get_caps(pad);
	if ( !caps )
		return;
	
	str = gst_caps_get_structure(caps, 0);
	if ( !str )
		return;

	name = gst_structure_get_name(str);
	if ( !name )
		return;
	debug_log("name=%s\n", name);

	if ( ! __mmplayer_try_to_plug(player, pad, caps) )
	{
		debug_error("failed to autoplug for type (%s)\n", name);
		gst_caps_unref(caps);
		return;
	}

	gst_caps_unref(caps);

	__mmplayer_pipeline_complete( NULL, (gpointer)player );

	debug_fleave();	

	return;
}

static void __mmplayer_set_unlinked_mime_type(mm_player_t* player, GstCaps *caps)
{
	GstStructure *str;
	gint version = 0;
	const char *stream_type;
	gchar *version_field = NULL;

	debug_fenter();

	return_if_fail ( player );
	return_if_fail ( caps );
	
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

	debug_fleave();
}

static void __mmplayer_add_new_pad(GstElement *element, GstPad *pad, gpointer data)
{
	mm_player_t* player = (mm_player_t*) data;
	GstCaps *caps = NULL;
	GstStructure *str = NULL;
	const char *name;

	debug_fenter();
	return_if_fail ( player );
	return_if_fail ( pad );

   	GST_OBJECT_LOCK (pad);
	if ((caps = GST_PAD_CAPS(pad)))
		gst_caps_ref(caps);
	GST_OBJECT_UNLOCK (pad);

	if ( NULL == caps )
	{
		caps = gst_pad_get_caps(pad);
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
	debug_log("stream count inc : %d\n", player->num_dynamic_pad);

	/* Note : If the stream is the subtitle, we try not to play it. Just close the demuxer subtitle pad.
	  *	If want to play it, remove this code.
	  */
	if (g_strrstr(name, "application"))
	{
		if (g_strrstr(name, "x-id3") || g_strrstr(name, "x-apetag"))
		{
			/* If id3/ape tag comes, keep going */
			debug_log("application mime exception : id3/ape tag");
		}
		else
		{
			/* Otherwise, we assume that this stream is subtile. */
			debug_log(" application mime type pad is closed.");
			return;
		}
	}
	else if (g_strrstr(name, "audio"))
	{
		gint samplerate = 0, channels = 0;

		/* set stream information */
		/* if possible, set it here because the caps is not distrubed by resampler. */
		gst_structure_get_int (str, "rate", &samplerate);
		mm_attrs_set_int_by_name(player->attrs, "content_audio_samplerate", samplerate);

		gst_structure_get_int (str, "channels", &channels);
		mm_attrs_set_int_by_name(player->attrs, "content_audio_channels", channels);

		debug_log("audio samplerate : %d	channels : %d", samplerate, channels);

		/* validate all */
		if (  mmf_attrs_commit ( player->attrs ) )
		{
			debug_error("failed to update attributes");
			return;
		}
	}
	else if (g_strrstr(name, "video"))
	{
		gint stype;
		mm_attrs_get_int_by_name (player->attrs, "display_surface_type", &stype);

		/* don't make video because of not required */
		if (stype == MM_DISPLAY_SURFACE_NULL)
		{
			debug_log("no video because it's not required");
			return;
		}

		player->v_stream_caps = gst_caps_copy(caps); //if needed, video caps is required when videobin is created
	}

	if ( ! __mmplayer_try_to_plug(player, pad, caps) )
	{
		debug_error("failed to autoplug for type (%s)", name);

		__mmplayer_set_unlinked_mime_type(player, caps);
	}

	gst_caps_unref(caps);

	debug_fleave();
	return;
}

/* test API for tuning audio gain. this API should be
 * deprecated before the day of final release
 */
int
_mmplayer_set_volume_tune(MMHandleType hplayer, MMPlayerVolumeType volume)
{
	mm_player_t* player = (mm_player_t*) hplayer;
	gint error = MM_ERROR_NONE;
	gint vol_max = 0;
	gboolean isMidi = FALSE;
	gint i = 0;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail( player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED )

	debug_log("clip type=%d(1-midi, 0-others), volume [L]=%d:[R]=%d\n",
		player->profile.play_mode, volume.level[0], volume.level[1]);

	isMidi = ( player->profile.play_mode == MM_PLAYER_MODE_MIDI ) ? TRUE : FALSE;

	if ( isMidi )
		vol_max = 1000;
	else
		vol_max = 100;

	/* is it proper volume level? */
	for (i = 0; i < MM_VOLUME_CHANNEL_NUM; ++i)
	{
		if (volume.level[i] < 0 || volume.level[i] > vol_max) {
			debug_log("Invalid Volume level!!!! \n");
			return MM_ERROR_INVALID_ARGUMENT;
		}
	}

	if ( isMidi )
	{
		if ( player->pipeline->mainbin )
		{
			GstElement *midi_element = player->pipeline->mainbin[MMPLAYER_M_DEMUX].gst;

			if ( midi_element && ( strstr(GST_ELEMENT_NAME(midi_element), "midiparse")) )
			{
				debug_log("setting volume (%d) level to midi plugin\n", volume.level[0]);

				g_object_set(midi_element, "volume", volume.level[0], NULL);
			}
		}
	}
	else
	{
		if ( player->pipeline->audiobin )
		{
			GstElement *sink_element = player->pipeline->audiobin[MMPLAYER_A_SINK].gst;

			/* Set to Avsysaudiosink element */
			if ( sink_element )
			{
				gint vol_value = 0;
				gboolean mute = FALSE;
				vol_value = volume.level[0];

				g_object_set(G_OBJECT(sink_element), "tuningvolume", vol_value, NULL);

				mute = (vol_value == 0)? TRUE:FALSE;

				g_object_set(G_OBJECT(sink_element), "mute", mute, NULL);
			}

		}
	}

	debug_fleave();

	return error;
}

gboolean
__mmplayer_dump_pipeline_state( mm_player_t* player )
{
	GstIterator*iter = NULL;
	gboolean done = FALSE;

	GstElement *item = NULL;
	GstElementFactory *factory = NULL;

	GstState state = GST_STATE_VOID_PENDING;
	GstState pending = GST_STATE_VOID_PENDING;
	GstClockTime time = 200*GST_MSECOND;

	debug_fenter();

	return_val_if_fail ( player &&
		player->pipeline &&
		player->pipeline->mainbin,
		FALSE );


	iter = gst_bin_iterate_recurse(GST_BIN(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst) );

	if ( iter != NULL )
	{
		while (!done) {
			 switch ( gst_iterator_next (iter, (gpointer)&item) )
			 {
			   case GST_ITERATOR_OK:
			   	gst_element_get_state(GST_ELEMENT (item),&state, &pending,time);

			   	factory = gst_element_get_factory (item) ;
				 debug_log("%s:%s : From:%s To:%s   refcount : %d\n", GST_OBJECT_NAME(factory) , GST_ELEMENT_NAME(item) ,
				 	gst_element_state_get_name(state), gst_element_state_get_name(pending) , GST_OBJECT_REFCOUNT_VALUE(item));


				 gst_object_unref (item);
				 break;
			   case GST_ITERATOR_RESYNC:
				 gst_iterator_resync (iter);
				 break;
			   case GST_ITERATOR_ERROR:
				 done = TRUE;
				 break;
			   case GST_ITERATOR_DONE:
				 done = TRUE;
				 break;
			 }
		}
	}

	item = GST_ELEMENT(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst);

	gst_element_get_state(GST_ELEMENT (item),&state, &pending,time);

	factory = gst_element_get_factory (item) ;

	debug_log("%s:%s : From:%s To:%s  refcount : %d\n",
		GST_OBJECT_NAME(factory),
		GST_ELEMENT_NAME(item),
		gst_element_state_get_name(state),
		gst_element_state_get_name(pending),
		GST_OBJECT_REFCOUNT_VALUE(item) );

	if ( iter )
		gst_iterator_free (iter);

	debug_fleave();

	return FALSE;
}


gboolean
__mmplayer_check_subtitle( mm_player_t* player )
{
	MMHandleType attrs = 0;
	char *subtitle_uri = NULL;

	debug_fenter();

	return_val_if_fail( player, FALSE );

	/* get subtitle attribute */
	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
		return FALSE;

	mm_attrs_get_string_by_name(attrs, "subtitle_uri", &subtitle_uri);
	if ( !subtitle_uri || !strlen(subtitle_uri))
		return FALSE;

	debug_log ("subtite uri is %s[%d]\n", subtitle_uri, strlen(subtitle_uri));

	debug_fleave();

	return TRUE;
}

static gboolean
__mmplayer_can_extract_pcm( mm_player_t* player )
{
	MMHandleType attrs = 0;
	gboolean is_drm = FALSE;
	gboolean sound_extraction = FALSE;

	debug_fenter();

	return_val_if_fail ( player, FALSE );

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("fail to get attributes.");
		return FALSE;
	}
	
	/* check file is drm or not */
	g_object_get(G_OBJECT(player->pipeline->mainbin[MMPLAYER_M_SRC].gst), "is-drm", &is_drm, NULL);

	/* get sound_extraction property */
	mm_attrs_get_int_by_name(attrs, "pcm_extraction", &sound_extraction);

	if ( ! sound_extraction || is_drm )
	{
		debug_log("pcm extraction param.. is drm = %d, extraction mode = %d", is_drm, sound_extraction);
		return FALSE;
	}

	debug_fleave();

	return TRUE;
}

static gboolean
__mmplayer_handle_gst_error ( mm_player_t* player, GstMessage * message, GError* error )
{
	MMMessageParamType msg_param;
       gchar *msg_src_element;

	debug_fenter();

	return_val_if_fail( player, FALSE );
	return_val_if_fail( error, FALSE );

	/* NOTE : do somthing necessary inside of __gst_handle_XXX_error. not here */

	memset (&msg_param, 0, sizeof(MMMessageParamType));

	if ( error->domain == GST_CORE_ERROR )
	{
		msg_param.code = __gst_handle_core_error( player, error->code );
	}
	else if ( error->domain == GST_LIBRARY_ERROR )
	{
		msg_param.code = __gst_handle_library_error( player, error->code );
	}
	else if ( error->domain == GST_RESOURCE_ERROR )
	{
		msg_param.code = __gst_handle_resource_error( player, error->code );
	}
	else if ( error->domain == GST_STREAM_ERROR )
	{
		msg_param.code = __gst_handle_stream_error( player, error, message );
	}
	else
	{
		debug_warning("This error domain is not defined.\n");

		/* we treat system error as an internal error */
		msg_param.code = MM_ERROR_PLAYER_INVALID_STREAM;
	}

	if ( message->src )
	{
		msg_src_element = GST_ELEMENT_NAME( GST_ELEMENT_CAST( message->src ) );

		msg_param.data = (void *) error->message;

		debug_error("-Msg src : [%s]	Domain : [%s]   Error : [%s]  Code : [%d] is tranlated to error code : [0x%x]\n",
			msg_src_element, g_quark_to_string (error->domain), error->message, error->code, msg_param.code);
	}

	/* post error to application */
	if ( ! player->posted_msg )
	{
		if (msg_param.code == MM_MESSAGE_DRM_NOT_AUTHORIZED)
		{
			MMPLAYER_POST_MSG( player, MM_MESSAGE_DRM_NOT_AUTHORIZED, NULL );
		}
		else
		{
			MMPLAYER_POST_MSG( player, MM_MESSAGE_ERROR, &msg_param );
		}

		/* don't post more if one was sent already */
		player->posted_msg = TRUE;
	}
	else
	{
		debug_log("skip error post because it's sent already.\n");
	}

	debug_fleave();

	return TRUE;
}

static gboolean
__mmplayer_handle_streaming_error  ( mm_player_t* player, GstMessage * message )
{
	debug_log("\n");
	MMMessageParamType msg_param;
	gchar *msg_src_element = NULL;
	GstStructure *s = NULL;
	guint error_id = 0;
	gchar *error_string = NULL;

	debug_fenter();

	return_val_if_fail ( player, FALSE );
	return_val_if_fail ( message, FALSE );

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
			return MM_ERROR_PLAYER_STREAMING_FAIL;
	}

	error_string = g_strdup(gst_structure_get_string (s, "error_string"));
	if ( error_string )
		msg_param.data = (void *) error_string;

	if ( message->src )
	{
		msg_src_element = GST_ELEMENT_NAME( GST_ELEMENT_CAST( message->src ) );

		debug_error("-Msg src : [%s] Code : [%x] Error : [%s]  \n",
			msg_src_element, msg_param.code, (char*)msg_param.data );
	}

	/* post error to application */
	if ( ! player->posted_msg )
	{
		MMPLAYER_POST_MSG( player, MM_MESSAGE_ERROR, &msg_param );

		/* don't post more if one was sent already */
		player->posted_msg = TRUE;
	}
	else
	{
		debug_log("skip error post because it's sent already.\n");
	}

	debug_fleave();

	return TRUE;

}

static gint
__gst_handle_core_error( mm_player_t* player, int code )
{
	gint trans_err = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	switch ( code )
	{
		case GST_CORE_ERROR_STATE_CHANGE:
		case GST_CORE_ERROR_MISSING_PLUGIN:
		case GST_CORE_ERROR_SEEK:
        	case GST_CORE_ERROR_NOT_IMPLEMENTED:
		case GST_CORE_ERROR_FAILED:
		case GST_CORE_ERROR_TOO_LAZY:
		case GST_CORE_ERROR_PAD:
		case GST_CORE_ERROR_THREAD:
		case GST_CORE_ERROR_NEGOTIATION:
		case GST_CORE_ERROR_EVENT:
		case GST_CORE_ERROR_CAPS:
		case GST_CORE_ERROR_TAG:
		case GST_CORE_ERROR_CLOCK:
		case GST_CORE_ERROR_DISABLED:
		default:
			trans_err = MM_ERROR_PLAYER_INVALID_STREAM;
		break;
	}

	debug_fleave();

	return trans_err;
}

static gint
__gst_handle_library_error( mm_player_t* player, int code )
{
	gint trans_err = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	switch ( code )
	{
		case GST_LIBRARY_ERROR_FAILED:
		case GST_LIBRARY_ERROR_TOO_LAZY:
		case GST_LIBRARY_ERROR_INIT:
		case GST_LIBRARY_ERROR_SHUTDOWN:
		case GST_LIBRARY_ERROR_SETTINGS:
		case GST_LIBRARY_ERROR_ENCODE:
		default:
			trans_err =  MM_ERROR_PLAYER_INVALID_STREAM;
		break;
	}
	
	debug_fleave();

	return trans_err;
}


static gint
__gst_handle_resource_error( mm_player_t* player, int code )
{
	gint trans_err = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	switch ( code )
	{
		case GST_RESOURCE_ERROR_NO_SPACE_LEFT:
			trans_err = MM_ERROR_PLAYER_NO_FREE_SPACE;
			break;
		case GST_RESOURCE_ERROR_NOT_FOUND:
		case GST_RESOURCE_ERROR_OPEN_READ:
			if ( MMPLAYER_IS_HTTP_STREAMING(player) || MMPLAYER_IS_HTTP_LIVE_STREAMING ( player ) )
			{
				trans_err = MM_ERROR_PLAYER_STREAMING_CONNECTION_FAIL;
				break;
			}
		case GST_RESOURCE_ERROR_READ:
			if ( MMPLAYER_IS_HTTP_STREAMING(player) ||  MMPLAYER_IS_HTTP_LIVE_STREAMING ( player ))
			{
				trans_err = MM_ERROR_PLAYER_STREAMING_FAIL;
				break;
			}
		case GST_RESOURCE_ERROR_SEEK:
		case GST_RESOURCE_ERROR_FAILED:
		case GST_RESOURCE_ERROR_TOO_LAZY:
		case GST_RESOURCE_ERROR_BUSY:
		case GST_RESOURCE_ERROR_OPEN_WRITE:
		case GST_RESOURCE_ERROR_OPEN_READ_WRITE:
		case GST_RESOURCE_ERROR_CLOSE:
		case GST_RESOURCE_ERROR_WRITE:
		case GST_RESOURCE_ERROR_SYNC:
		case GST_RESOURCE_ERROR_SETTINGS:
		default:
			trans_err = MM_ERROR_PLAYER_FILE_NOT_FOUND;
		break;
	}

	debug_fleave();

	return trans_err;
}


static gint
__gst_handle_stream_error( mm_player_t* player, GError* error, GstMessage * message )
{
	gint trans_err = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail( error, MM_ERROR_INVALID_ARGUMENT );
	return_val_if_fail ( message, MM_ERROR_INVALID_ARGUMENT );

	switch ( error->code )
	{
		case GST_STREAM_ERROR_FAILED:
		case GST_STREAM_ERROR_TYPE_NOT_FOUND:
		case GST_STREAM_ERROR_DECODE:
		case GST_STREAM_ERROR_WRONG_TYPE:
		case GST_STREAM_ERROR_DECRYPT:
			 trans_err = __gst_transform_gsterror( player, message, error );
		break;

		case GST_STREAM_ERROR_CODEC_NOT_FOUND:
		case GST_STREAM_ERROR_NOT_IMPLEMENTED:
		case GST_STREAM_ERROR_TOO_LAZY:
		case GST_STREAM_ERROR_ENCODE:
		case GST_STREAM_ERROR_DEMUX:
		case GST_STREAM_ERROR_MUX:
		case GST_STREAM_ERROR_FORMAT:
		case GST_STREAM_ERROR_DECRYPT_NOKEY:
		default:
			trans_err = MM_ERROR_PLAYER_INVALID_STREAM;
		break;
	}

	debug_fleave();

	return trans_err;
}


/* NOTE : decide gstreamer state whether there is some playable track or not. */
static gint
__gst_transform_gsterror( mm_player_t* player, GstMessage * message, GError* error )
{
	gchar *src_element_name = NULL;
	GstElement *src_element = NULL;
	GstElementFactory *factory = NULL;
	const gchar* klass = NULL;
	
	debug_fenter();

	/* FIXIT */
	return_val_if_fail ( message, MM_ERROR_INVALID_ARGUMENT );
	return_val_if_fail ( message->src, MM_ERROR_INVALID_ARGUMENT );
	return_val_if_fail ( error, MM_ERROR_INVALID_ARGUMENT );

	src_element = GST_ELEMENT_CAST(message->src);
	if ( !src_element )
		goto INTERNAL_ERROR;
	
	src_element_name = GST_ELEMENT_NAME(src_element);
	if ( !src_element_name )
		goto INTERNAL_ERROR;

	factory = gst_element_get_factory(src_element);
	if ( !factory )
		goto INTERNAL_ERROR;
	
	klass = gst_element_factory_get_klass(factory);
	if ( !klass )
		goto INTERNAL_ERROR;

	debug_log("error code=%d, msg=%s, src element=%s, class=%s\n", 
			error->code, error->message, src_element_name, klass);


	switch ( error->code )
	{
		case GST_STREAM_ERROR_DECODE:
		{
			/* NOTE : Delay is needed because gst callback is sometime sent
			 * before completing autoplugging.
			 * Timer is more better than usleep.
			 * But, transformed msg value should be stored in player handle
			 * for function to call by timer.
			 */
			if ( PLAYER_INI()->async_start )
				usleep(500000);

			/* Demuxer can't parse one track because it's corrupted.
			 * So, the decoder for it is not linked.
			 * But, it has one playable track.
			 */
			if ( g_strrstr(klass, "Demux") )
			{
				if ( player->can_support_codec == FOUND_PLUGIN_VIDEO )
				{
					return MM_ERROR_PLAYER_AUDIO_CODEC_NOT_FOUND;
				}
				else if ( player->can_support_codec == FOUND_PLUGIN_AUDIO )
				{
					return MM_ERROR_PLAYER_VIDEO_CODEC_NOT_FOUND;
				}
				else
				{
					if ( player->pipeline->audiobin ) // PCM
					{
						return MM_ERROR_PLAYER_VIDEO_CODEC_NOT_FOUND;
					}
					else
					{
						goto CODEC_NOT_FOUND;
					}
				}
			}
			return MM_ERROR_PLAYER_INVALID_STREAM;
		}
		break;

		case GST_STREAM_ERROR_WRONG_TYPE:
		{
			return MM_ERROR_PLAYER_NOT_SUPPORTED_FORMAT;
		}
		break;

		case GST_STREAM_ERROR_FAILED:
		{
			/* Decoder Custom Message */
			if ( strstr(error->message, "ongoing") )
			{
				if ( strcasestr(klass, "audio") )
				{
					if ( ( player->can_support_codec & FOUND_PLUGIN_VIDEO ) )
					{
						debug_log("Video can keep playing.\n");
						return MM_ERROR_PLAYER_AUDIO_CODEC_NOT_FOUND;
					}
					else
					{
						goto CODEC_NOT_FOUND;
					}

				}
				else if ( strcasestr(klass, "video") )
				{
					if ( ( player->can_support_codec & FOUND_PLUGIN_AUDIO ) )
					{
						debug_log("Audio can keep playing.\n");
						return MM_ERROR_PLAYER_VIDEO_CODEC_NOT_FOUND;
					}
					else
					{
						goto CODEC_NOT_FOUND;
					}
				}
			}
	return MM_ERROR_PLAYER_INVALID_STREAM;
		}
		break;

		case GST_STREAM_ERROR_TYPE_NOT_FOUND:
		{				
			goto CODEC_NOT_FOUND;
		}
		break;

		case GST_STREAM_ERROR_DECRYPT:
		{				
			debug_log("%s failed reason : %s\n", src_element_name, error->message);
			return MM_MESSAGE_DRM_NOT_AUTHORIZED;
		}
		break;

		default:
		break;
	}

	debug_fleave();

	return MM_ERROR_PLAYER_INVALID_STREAM;

INTERNAL_ERROR:
	return MM_ERROR_PLAYER_INTERNAL;

CODEC_NOT_FOUND:
	debug_log("not found any available codec. Player should be destroyed.\n");
	return MM_ERROR_PLAYER_CODEC_NOT_FOUND;
}

static void
__mmplayer_post_delayed_eos( mm_player_t* player, int delay_in_ms )
{
	debug_fenter();

	return_if_fail( player );

	/* cancel if existing */
	__mmplayer_cancel_delayed_eos( player );


	/* post now if delay is zero */
	if ( delay_in_ms == 0 || player->is_sound_extraction)
	{
		debug_log("eos delay is zero. posting EOS now\n");
		MMPLAYER_POST_MSG( player, MM_MESSAGE_END_OF_STREAM, NULL );

		if ( player->is_sound_extraction )
			__mmplayer_cancel_delayed_eos(player);

		return;
	}

	/* init new timeout */
	/* NOTE : consider give high priority to this timer */

	debug_log("posting EOS message after [%d] msec\n", delay_in_ms);
	player->eos_timer = g_timeout_add( delay_in_ms,
		__mmplayer_eos_timer_cb, player );


	/* check timer is valid. if not, send EOS now */
	if ( player->eos_timer == 0 )
	{
		debug_warning("creating timer for delayed EOS has failed. sending EOS now\n");
		MMPLAYER_POST_MSG( player, MM_MESSAGE_END_OF_STREAM, NULL );
	}

	debug_fleave();
}

static void
__mmplayer_cancel_delayed_eos( mm_player_t* player )
{
	debug_fenter();

	return_if_fail( player );

	if ( player->eos_timer )
	{
		g_source_remove( player->eos_timer );
	}

	player->eos_timer = 0;

	debug_fleave();

	return;
}

static gboolean
__mmplayer_eos_timer_cb(gpointer u_data)
{
	mm_player_t* player = NULL;
	player = (mm_player_t*) u_data;

	debug_fenter();

	return_val_if_fail( player, FALSE );

	/* posting eos */
	MMPLAYER_POST_MSG( player, MM_MESSAGE_END_OF_STREAM, NULL );

	/* cleare timer id */
	player->eos_timer = 0;

	debug_fleave();

	/* we are returning FALSE as we need only one posting */
	return FALSE;
}

static void __mmplayer_set_antishock( mm_player_t* player, gboolean disable_by_force)
{
	gint antishock = FALSE;
	MMHandleType attrs = 0;

	debug_fenter();

	return_if_fail ( player && player->pipeline );

	/* It should be passed for video only clip */
	if ( ! player->pipeline->audiobin )
		return;

	if ( ( g_strrstr(PLAYER_INI()->name_of_audiosink, "avsysaudiosink")) )
	{
		attrs = MMPLAYER_GET_ATTRS(player);
		if ( ! attrs )
		{
			debug_error("fail to get attributes.\n");
			return;
		}

		mm_attrs_get_int_by_name(attrs, "sound_fadeup", &antishock);

		debug_log("setting antishock as (%d)\n", antishock);

		if ( disable_by_force )
		{
			debug_log("but, antishock is disabled by force when is seeked\n");

			antishock = FALSE;
		}

		g_object_set(G_OBJECT(player->pipeline->audiobin[MMPLAYER_A_SINK].gst), "fadeup", antishock, NULL);
	}

	debug_fleave();

	return;
}


static gboolean
__mmplayer_link_decoder( mm_player_t* player, GstPad *srcpad)
{
	const gchar* name = NULL;
	GstStructure* str = NULL;
	GstCaps* srccaps = NULL;

	debug_fenter();

	return_val_if_fail( player, FALSE );
	return_val_if_fail ( srcpad, FALSE );

	/* to check any of the decoder (video/audio) need to be linked  to parser*/
	srccaps = gst_pad_get_caps( srcpad );
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
		    debug_msg("Video decoder already linked\n");
			return FALSE;
		}
	}
	if (strstr(name, "audio"))
	{
		if(player->audiodec_linked)
		{
		    debug_msg("Audio decoder already linked\n");
			return FALSE;
		}
	}

	gst_caps_unref( srccaps );

	debug_fleave();

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

	debug_fenter();

	return_val_if_fail ( player, FALSE );
	return_val_if_fail ( srcpad, FALSE );

	/* to check any of the decoder (video/audio) need to be linked	to parser*/
	srccaps = gst_pad_get_caps( srcpad );
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
			debug_msg("Video Sink already linked\n");
			return FALSE;
		}
	}
	if (strstr(name, "audio"))
	{
		if(player->audiosink_linked)
		{
			debug_msg("Audio Sink already linked\n");
			return FALSE;
		}
	}
	if (strstr(name, "text"))
	{
		if(player->textsink_linked)
		{
			debug_msg("Text Sink already linked\n");
			return FALSE;
		}
	}

	gst_caps_unref( srccaps );

	debug_fleave();

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
	GList *sinks = NULL;
	gboolean res = FALSE;

	debug_fenter();
	
	return_val_if_fail( player, FALSE );
	return_val_if_fail ( event, FALSE );

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
				debug_log("sending event[%s] to sink element [%s] success!\n",
					GST_EVENT_TYPE_NAME(event), GST_ELEMENT_NAME(sink) );
				break;
			}

			debug_log("sending event[%s] to sink element [%s] failed. try with next one.\n",
				GST_EVENT_TYPE_NAME(event), GST_ELEMENT_NAME(sink) );
		}

		sinks = g_list_next (sinks);
	}

	/* Note : Textbin is not linked to the video or audio bin.
	 * 		It needs to send the event to the text sink seperatelly.
	 */
	 if ( MMPLAYER_PLAY_SUBTITLE(player) )
	 {
	 	GstElement *subtitle_sink = GST_ELEMENT_CAST (player->pipeline->subtitlebin[MMPLAYER_SUB_SINK].gst);

		if ( (res != gst_element_send_event (subtitle_sink, event)) )
		{
			debug_error("sending event[%s] to subtitle sink element [%s] failed!\n",
				GST_EVENT_TYPE_NAME(event), GST_ELEMENT_NAME(subtitle_sink) );
		}
		else
		{
			debug_log("sending event[%s] to subtitle sink element [%s] success!\n",
				GST_EVENT_TYPE_NAME(event), GST_ELEMENT_NAME(subtitle_sink) );
		}
	 }

	gst_event_unref (event);

	debug_fleave();

	return res;
}

static void
__mmplayer_add_sink( mm_player_t* player, GstElement* sink )
{
	debug_fenter();

	return_if_fail ( player );
	return_if_fail ( sink );

	player->sink_elements =
		g_list_append(player->sink_elements, sink);

	debug_fleave();
}

static void
__mmplayer_del_sink( mm_player_t* player, GstElement* sink )
{
	debug_fenter();

	return_if_fail ( player );
	return_if_fail ( sink );

	player->sink_elements =
			g_list_remove(player->sink_elements, sink);

	debug_fleave();
}

static gboolean
__gst_seek(mm_player_t* player, GstElement * element, gdouble rate,
			GstFormat format, GstSeekFlags flags, GstSeekType cur_type,
			gint64 cur, GstSeekType stop_type, gint64 stop )
{
	GstEvent* event = NULL;
	gboolean result = FALSE;

	debug_fenter();
	
	return_val_if_fail( player, FALSE );

	event = gst_event_new_seek (rate, format, flags, cur_type,
		cur, stop_type, stop);

	result = __gst_send_event_to_sink( player, event );

	debug_fleave();

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
__mmplayer_release_signal_connection(mm_player_t* player)
{
	GList* sig_list = player->signals;
	MMPlayerSignalItem* item = NULL;

	debug_fenter();
	
	return_if_fail( player );

	for ( ; sig_list; sig_list = sig_list->next )
	{
		item = sig_list->data;

		if ( item && item->obj && GST_IS_ELEMENT(item->obj) )
		{
			debug_log("checking signal connection : [%lud] from [%s]\n", item->sig, GST_OBJECT_NAME( item->obj ));

			if ( g_signal_handler_is_connected ( item->obj, item->sig ) )
			{
				debug_log("signal disconnecting : [%lud] from [%s]\n", item->sig, GST_OBJECT_NAME( item->obj ));
				g_signal_handler_disconnect ( item->obj, item->sig );
			}
		}

		MMPLAYER_FREEIF( item );

	}
	g_list_free ( player->signals );
	player->signals = NULL;

	debug_fleave();

	return;
}


/* Note : if silent is true, then subtitle would not be displayed. :*/
int _mmplayer_set_subtitle_silent (MMHandleType hplayer, int silent)
{
	mm_player_t* player = (mm_player_t*) hplayer;

	debug_fenter();

	/* check player handle */
	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( MMPLAYER_PLAY_SUBTITLE(player),	MM_ERROR_PLAYER_NOT_INITIALIZED );

	player->is_subtitle_off = silent;

	debug_log("subtitle is %s.\n", player->is_subtitle_off ? "ON" : "OFF");

	debug_fleave();

	return MM_ERROR_NONE;
}


int _mmplayer_get_subtitle_silent (MMHandleType hplayer, int* silent)
{
	mm_player_t* player = (mm_player_t*) hplayer;

	debug_fenter();

	/* check player handle */
	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( MMPLAYER_PLAY_SUBTITLE(player),	MM_ERROR_PLAYER_NOT_INITIALIZED );

	*silent = player->is_subtitle_off;

	debug_log("subtitle is %s.\n", silent ? "ON" : "OFF");

	debug_fleave();

	return MM_ERROR_NONE;
}

int _mmplayer_get_track_count(MMHandleType hplayer,  MMPlayerTrackType track_type, int *count)
{
	mm_player_t* player = (mm_player_t*) hplayer;
	MMHandleType attrs = 0;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	/* check player handle */
	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(count, MM_ERROR_COMMON_INVALID_ARGUMENT);
	return_val_if_fail((MMPLAYER_CURRENT_STATE(player) != MM_PLAYER_STATE_PAUSED)
		 ||(MMPLAYER_CURRENT_STATE(player) != MM_PLAYER_STATE_PLAYING),
		MM_ERROR_PLAYER_INVALID_STATE);

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("cannot get content attribute");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	switch (track_type)
	{
		case MM_PLAYER_TRACK_TYPE_AUDIO:
			ret = mm_attrs_get_int_by_name(attrs, "content_audio_track_num", count);
			break;
		case MM_PLAYER_TRACK_TYPE_VIDEO:
			ret = mm_attrs_get_int_by_name(attrs, "content_video_track_num", count);
			break;
		case MM_PLAYER_TRACK_TYPE_TEXT:
			ret = mm_attrs_get_int_by_name(attrs, "content_text_track_num", count);
			break;
		default:
			ret = MM_ERROR_COMMON_INVALID_ARGUMENT;
			break;
	}

	debug_log ("%d track num is %d\n", track_type, *count);

	debug_fleave();

	return ret;
}



const gchar * 
__get_state_name ( int state )
{
	switch ( state )
	{
		case MM_PLAYER_STATE_NULL:
			return "NULL";
		case MM_PLAYER_STATE_READY:
			return "READY";
		case MM_PLAYER_STATE_PAUSED:
			return "PAUSED";
		case MM_PLAYER_STATE_PLAYING:
			return "PLAYING";
		case MM_PLAYER_STATE_NONE:
			return "NONE";
		default:
			return "INVAID";
	}
}
gboolean
__is_rtsp_streaming ( mm_player_t* player )
{
	return_val_if_fail ( player, FALSE );

	return ( player->profile.uri_type == MM_PLAYER_URI_TYPE_URL_RTSP ) ? TRUE : FALSE;
}

static gboolean
__is_http_streaming ( mm_player_t* player )
{
	return_val_if_fail ( player, FALSE );

	return ( player->profile.uri_type == MM_PLAYER_URI_TYPE_URL_HTTP ) ? TRUE : FALSE;
}

static gboolean
__is_streaming ( mm_player_t* player )
{
	return_val_if_fail ( player, FALSE );

	return ( __is_rtsp_streaming ( player ) || __is_http_streaming ( player ) || __is_http_live_streaming ( player )) ? TRUE : FALSE;
}

gboolean
__is_live_streaming ( mm_player_t* player )
{
	return_val_if_fail ( player, FALSE );

	return ( __is_rtsp_streaming ( player ) && player->streaming_type == STREAMING_SERVICE_LIVE ) ? TRUE : FALSE;
}

static gboolean
__is_http_live_streaming( mm_player_t* player )
{
	return_val_if_fail( player, FALSE );

	return ( player->profile.uri_type == MM_PLAYER_URI_TYPE_HLS ) ? TRUE : FALSE;
}

static gboolean
__is_http_progressive_down(mm_player_t* player)
{
	return_val_if_fail( player, FALSE );

	return ((player->pd_mode) ? TRUE:FALSE);
}
