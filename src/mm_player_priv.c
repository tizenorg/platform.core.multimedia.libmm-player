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
#include <gst/video/videooverlay.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdlib.h>

#include <mm_error.h>
#include <mm_attrs.h>
#include <mm_attrs_private.h>
#include <mm_debug.h>

#include "mm_player_priv.h"
#include "mm_player_ini.h"
#include "mm_player_attrs.h"
#include "mm_player_capture.h"
#include "mm_player_priv_internal.h"
#include "mm_player_priv_locl_func.h"

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

#define MM_PLAYER_MPEG_VNAME				"mpegversion"
#define MM_PLAYER_DIVX_VNAME				"divxversion"
#define MM_PLAYER_WMV_VNAME				"wmvversion"
#define MM_PLAYER_WMA_VNAME				"wmaversion"

#define DEFAULT_PLAYBACK_RATE			1.0

#define GST_QUEUE_DEFAULT_TIME			8
#define GST_QUEUE_HLS_TIME				8

#define MMPLAYER_USE_FILE_FOR_BUFFERING(player) (((player)->profile.uri_type != MM_PLAYER_URI_TYPE_HLS) && (PLAYER_INI()->http_file_buffer_path) && (strlen(PLAYER_INI()->http_file_buffer_path) > 0) )

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
static int 		__mmplayer_get_state(mm_player_t* player);
static gboolean __mmplayer_is_midi_type(gchar* str_caps);
static gboolean __mmplayer_is_amr_type (gchar *str_caps);
static gboolean __mmplayer_is_only_mp3_type (gchar *str_caps);

static gboolean	__mmplayer_close_link(mm_player_t* player, GstPad *srcpad, GstElement *sinkelement, const char *padname, const GList *templlist);
static gboolean __mmplayer_feature_filter(GstPluginFeature *feature, gpointer data);
static void 	__mmplayer_add_new_pad(GstElement *element, GstPad *pad, gpointer data);

static gboolean	__mmplayer_get_stream_service_type( mm_player_t* player );
static void 	__mmplayer_init_factories(mm_player_t* player);
static void 	__mmplayer_release_factories(mm_player_t* player);
static gboolean	__mmplayer_gstreamer_init(void);

gboolean __mmplayer_post_message(mm_player_t* player, enum MMMessageType msgtype, MMMessageParamType* param);

int		__mmplayer_switch_audio_sink (mm_player_t* player);
static int		__mmplayer_check_state(mm_player_t* player, enum PlayerCommandState command);
static GstPadProbeReturn __mmplayer_audio_stream_probe (GstPad *pad, GstPadProbeInfo *info, gpointer u_data);
static gboolean	__mmplayer_eos_timer_cb(gpointer u_data);
static int		__mmplayer_check_not_supported_codec(mm_player_t* player, gchar* mime);
static gpointer __mmplayer_repeat_thread(gpointer data);
static void 	__mmplayer_add_new_caps(GstPad* pad, GParamSpec* unused, gpointer data);
static void __mmplayer_set_unlinked_mime_type(mm_player_t* player, GstCaps *caps);

/* util */
const gchar * __get_state_name ( int state );

static gboolean __mmplayer_warm_up_video_codec( mm_player_t* player,  GstElementFactory *factory);

static int  __mmplayer_realize_streaming_ext(mm_player_t* player);
static int __mmplayer_unrealize_streaming_ext(mm_player_t *player);
static int __mmplayer_start_streaming_ext(mm_player_t *player);
static int __mmplayer_destroy_streaming_ext(mm_player_t* player);



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

void
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

void
__mmplayer_videoframe_render_error_cb(GstElement *element, void *error_id, gpointer data)
{
	mm_player_t* player = (mm_player_t*)data;

	return_if_fail ( player );

	debug_fenter();

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
				debug_log("display surface type(X_EXT) : render error callback(%p) is finished", player->video_frame_render_error_cb);
				break;
			default:
				debug_error("video_frame_render_error_cb was set, but this surface type(%d) is not supported", surface_type);
				break;
			}
		}
		else
		{
			debug_error("could not get surface type");
		}
	}
	else
	{
		debug_warning("video_frame_render_error_cb was not set");
	}

	debug_fleave();
}

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

	debug_fenter();

	return_val_if_fail ( player, FALSE );

	/* check player state here */
	if ( MMPLAYER_CURRENT_STATE(player) != MM_PLAYER_STATE_PAUSED &&
		MMPLAYER_CURRENT_STATE(player) != MM_PLAYER_STATE_PLAYING )
	{
		/* give warning now only */
		debug_warning("be careful. content attributes may not available in this state ");
	}

	/* get content attribute first */
	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("cannot get content attribute");
		return FALSE;
	}

	/* get update flag */

	if ( flag & ATTR_MISSING_ONLY )
	{
		missing_only = TRUE;
		debug_log("updating missed attr only");
	}

	if ( flag & ATTR_ALL )
	{
		all = TRUE;
		has_duration = FALSE;
		has_video_attrs = FALSE;
		has_audio_attrs = FALSE;
		has_bitrate = FALSE;

		debug_log("updating all attrs");
	}

	if ( missing_only && all )
	{
		debug_warning("cannot use ATTR_MISSING_ONLY and ATTR_ALL. ignoring ATTR_MISSING_ONLY flag!");
		missing_only = FALSE;
	}

	if (  (flag & ATTR_DURATION) ||	(!has_duration && missing_only) || all )
	{
		debug_log("try to update duration");
		has_duration = FALSE;

		if (gst_element_query_duration(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, GST_FORMAT_TIME, &dur_nsec ))
		{
			player->duration = dur_nsec;
			debug_log("duration : %lld msec", GST_TIME_AS_MSECONDS(dur_nsec));
		}

		/* try to get streaming service type */
		stream_service_type = __mmplayer_get_stream_service_type( player );
		mm_attrs_set_int_by_name ( attrs, "streaming_type", stream_service_type );

		/* check duration is OK */
		if ( dur_nsec == 0 && !MMPLAYER_IS_LIVE_STREAMING( player ) )
		{
			debug_error("not ready to get duration");
		}
		else
		{
			/*update duration */
			mm_attrs_set_int_by_name(attrs, "content_duration", GST_TIME_AS_MSECONDS(dur_nsec));
			has_duration = TRUE;
			debug_log("duration updated");
		}
	}

	if (  (flag & ATTR_AUDIO) || (!has_audio_attrs && missing_only) || all )
	{
		/* update audio params
		NOTE : We need original audio params and it can be only obtained from src pad of audio
		decoder. Below code only valid when we are not using 'resampler' just before
		'audioconverter'. */

		debug_log("try to update audio attrs");
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

					debug_log("samplerate : %d	channels : %d", samplerate, channels);

					gst_caps_unref( caps_a );
					caps_a = NULL;

					has_audio_attrs = TRUE;
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
	}

	if ( (flag & ATTR_VIDEO) || (!has_video_attrs && missing_only) || all )
	{
		debug_log("try to update video attrs");
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

					has_video_attrs = TRUE;
				}
				else
				{
					debug_log("no negitiated caps from videosink");
				}
				gst_object_unref( pad );
				pad = NULL;
			}
			else
			{
				debug_log("no videosink sink pad");
			}
		}
	}


	if ( (flag & ATTR_BITRATE) || (!has_bitrate && missing_only) || all )
	{
		debug_log("try to update bitrate");
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

			if (data_size)
			{
				guint64 bitrate = 0;
				guint64 msec_dur = 0;

				msec_dur = GST_TIME_AS_MSECONDS(player->duration);
				bitrate = data_size * 8 * 1000 / msec_dur;
				debug_log("file size : %u, video bitrate = %llu", data_size, bitrate);
				mm_attrs_set_int_by_name(attrs, "content_video_bitrate", bitrate);

				has_bitrate = TRUE;
			}
		}
	}

	/* validate all */
	if (  mmf_attrs_commit ( attrs ) )
	{
		debug_error("failed to update attributes\n");
		return FALSE;
	}

	debug_fleave();
	return TRUE;
}

static gint __mmplayer_get_stream_service_type( mm_player_t* player )
{
	gint streaming_type = STREAMING_SERVICE_NONE;

	debug_fenter();

	return_val_if_fail ( player &&
			player->pipeline &&
			player->pipeline->mainbin &&
			player->pipeline->mainbin[MMPLAYER_M_SRC].gst,
			FALSE );

	/* streaming service type if streaming */
	if ( ! MMPLAYER_IS_STREAMING(player) )
	{
		debug_log("not a streamming");
		return STREAMING_SERVICE_NONE;
	}

	if (MMPLAYER_IS_RTSP_STREAMING(player))
	{
		/* get property from rtspsrc element */
		g_object_get(G_OBJECT(player->pipeline->mainbin[MMPLAYER_M_SRC].gst),
			"service_type", &streaming_type, NULL);
	}
	else if (MMPLAYER_IS_HTTP_STREAMING(player))
	{
		streaming_type = player->duration == 0 ?
			STREAMING_SERVICE_LIVE : STREAMING_SERVICE_VOD;
	}

	switch ( streaming_type )
	{
		case STREAMING_SERVICE_LIVE:
			debug_log("it's live streaming");
		break;
		case STREAMING_SERVICE_VOD:
			debug_log("it's vod streaming");
		break;
		case STREAMING_SERVICE_NONE:
			debug_error("should not get here");
		break;
		default:
			debug_error("should not get here");
	}

	player->streaming_type = streaming_type;
	debug_fleave();

	return streaming_type;
}


/* this function sets the player state and also report
 * it to applicaton by calling callback function
 */
gboolean
__mmplayer_set_state(mm_player_t* player, int state) // @
{
	MMMessageParamType msg = {0, };
	int asm_result = MM_ERROR_NONE;
	gboolean post_bos = FALSE;
	gboolean interrupted_by_asm = FALSE;

	debug_fenter();
	return_val_if_fail ( player, FALSE );

	if ( MMPLAYER_CURRENT_STATE(player) == state )
	{
		debug_warning("already same state(%s)\n", MMPLAYER_STATE_GET_NAME(state));
		MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_NONE;
		return TRUE;
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
	interrupted_by_asm = player->sm.by_asm_cb;

	switch ( MMPLAYER_CURRENT_STATE(player) )
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
			 if ( ! player->sent_bos )
			 {
			 	/* it's first time to update all content attrs. */
				_mmplayer_update_content_attrs( player, ATTR_ALL );
			 }

			/* add audio callback probe if condition is satisfied */
			if ( ! player->audio_cb_probe_id && player->is_sound_extraction )
			{
				__mmplayer_configure_audio_callback(player);
			}

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
			/* try to get content metadata */
			if ( ! player->sent_bos )
			{
				/* NOTE : giving ATTR_MISSING_ONLY may have dependency with
				 * c-api since c-api doesn't use _start() anymore. It may not work propery with
				 * legacy mmfw-player api */
				_mmplayer_update_content_attrs( player, ATTR_MISSING_ONLY);
			}

			if ( player->cmd == MMPLAYER_COMMAND_START  && !player->sent_bos )
			{
				__mmplayer_handle_missed_plugin ( player );

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

				post_bos = TRUE;
			}
		}
		break;

		case MM_PLAYER_STATE_NONE:
		default:
			debug_warning("invalid target state, there is nothing to do.\n");
			break;
	}


	/* post message to application */
	if (MMPLAYER_TARGET_STATE(player) == state)
	{
		/* fill the message with state of player */
		msg.state.previous = MMPLAYER_PREV_STATE(player);
		msg.state.current = MMPLAYER_CURRENT_STATE(player);

		/* state changed by asm callback */
		if ( interrupted_by_asm )
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

	if ( post_bos )
	{
		MMTA_ACUM_ITEM_END("[KPI] start media player service", FALSE);
		MMTA_ACUM_ITEM_END("[KPI] media player service create->playing", FALSE);

		MMPLAYER_POST_MSG ( player, MM_MESSAGE_BEGIN_OF_STREAM, NULL );
		player->sent_bos = TRUE;
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
		debug_warning("no msg callback. can't post msg now\n");
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

void
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

			ret_value = __gst_seek( player, player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, player->playback_rate,
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

void
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
__mmplayer_get_property_value_for_rotation(mm_player_t* player, int rotation_angle, int *value)
{
	int pro_value = 0; // in the case of expection, default will be returned.
	int dest_angle = rotation_angle;
	int rotation_using_type = -1;
	#define ROTATION_USING_X	0
	#define ROTATION_USING_FIMC	1
	#define ROTATION_USING_FLIP	2

	return_val_if_fail(player, FALSE);
	return_val_if_fail(value, FALSE);
	return_val_if_fail(rotation_angle >= 0, FALSE);

	if (rotation_angle >= 360)
	{
		dest_angle = rotation_angle - 360;
	}

	/* chech if supported or not */
	if ( dest_angle % 90 )
	{
		debug_log("not supported rotation angle = %d", rotation_angle);
		return FALSE;
	}

	if (player->use_video_stream)
	{
		if (player->is_nv12_tiled)
		{
			rotation_using_type = ROTATION_USING_FIMC;
		}
		else
		{
			rotation_using_type = ROTATION_USING_FLIP;
		}
	}
	else
	{
		int surface_type = 0;
		mm_attrs_get_int_by_name(player->attrs, "display_surface_type", &surface_type);
		debug_log("check display surface type for rotation: %d", surface_type);

		switch (surface_type)
		{
			case MM_DISPLAY_SURFACE_X:
				rotation_using_type = ROTATION_USING_X;
				break;
			case MM_DISPLAY_SURFACE_EVAS:
				if (player->is_nv12_tiled && !strcmp(PLAYER_INI()->videosink_element_evas,"evasimagesink"))
				{
					rotation_using_type = ROTATION_USING_FIMC;
				}
				else if (!player->is_nv12_tiled)
				{
					rotation_using_type = ROTATION_USING_FLIP;
				}
				else if (!strcmp(PLAYER_INI()->videosink_element_evas,"evaspixmapsink"))
				{
					rotation_using_type = ROTATION_USING_X;
				}
				else
				{
					debug_error("it should not be here..");
					return FALSE;
				}
				break;
			default:
				rotation_using_type = ROTATION_USING_FLIP;
				break;
		}
	}

	debug_log("using %d type for rotation", rotation_using_type);

	/* get property value for setting */
	switch(rotation_using_type)
	{
		case ROTATION_USING_X: // xvimagesink
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
		case ROTATION_USING_FIMC: // fimcconvert
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

	debug_log("setting rotation property value : %d", pro_value);

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
	int user_angle_type= 0;
	int rotation_value = 0;

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

	/* update user roation */
	mm_attrs_get_int_by_name(attrs, "display_rotation", &user_angle_type);

	/* get angle with user type */
	switch(user_angle_type)
	{
		case MM_DISPLAY_ROTATION_NONE:
			user_angle = 0;
			break;
		case MM_DISPLAY_ROTATION_90: // counter-clockwise 90
			user_angle = 270;
			break;
		case MM_DISPLAY_ROTATION_180:
			user_angle = 180;
			break;
		case MM_DISPLAY_ROTATION_270: // clockwise 90
			user_angle = 90;
			break;
	}

	/* get original orientation */
	if (player->v_stream_caps)
	{
		GstStructure *str = NULL;

		str = gst_caps_get_structure (player->v_stream_caps, 0);
		if ( !gst_structure_get_int (str, "orientation", &org_angle))
		{
			debug_log ("missing 'orientation' field in video caps");
		}
		else
		{
			debug_log("origianl video orientation = %d", org_angle);
		}
	}

	debug_log("check user angle: %d, orientation: %d", user_angle, org_angle);

	/* check video stream callback is used */
	if( player->use_video_stream )
	{
		if (player->is_nv12_tiled)
		{
			gchar *ename = NULL;
			int width = 0;
			int height = 0;

			mm_attrs_get_int_by_name(attrs, "display_width", &width);
			mm_attrs_get_int_by_name(attrs, "display_height", &height);

			/* resize video frame with requested values for fimcconvert */
            ename = GST_OBJECT_NAME(gst_element_get_factory(player->pipeline->videobin[MMPLAYER_V_CONV].gst));

			if (g_strrstr(ename, "fimcconvert"))
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
				debug_log("updating fimcconvert - r[%d], w[%d], h[%d]", rotation_value, width, height);
			}
		}
		else
		{
			debug_log("using video stream callback with memsink. player handle : [%p]", player);

			/* get rotation value to set */
			__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);

			g_object_set(player->pipeline->videobin[MMPLAYER_V_FLIP].gst, "method", rotation_value, NULL);
		}

		return MM_ERROR_NONE;
	}

	/* update display surface */
	mm_attrs_get_int_by_name(attrs, "display_surface_type", &surface_type);
	debug_log("check display surface type attribute: %d", surface_type);

	/* configuring display */
	switch ( surface_type )
	{
		case MM_DISPLAY_SURFACE_X:
		{
			/* ximagesink or xvimagesink */
			void *xid = NULL;
			double zoom = 0;
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
#define GST_VAAPI_DISPLAY_TYPE_X11 1
                if (!strncmp(PLAYER_INI()->videosink_element_x,"vaapisink", strlen("vaapisink"))){
                    debug_log("set video param: vaapisink display %d", GST_VAAPI_DISPLAY_TYPE_X11);
                    g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
                            "display", GST_VAAPI_DISPLAY_TYPE_X11,
                            NULL);
                }

				debug_log("set video param : xid %d", *(int*)xid);
                gst_video_overlay_set_window_handle( GST_VIDEO_OVERLAY( player->pipeline->videobin[MMPLAYER_V_SINK].gst ), *(int*)xid );
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
				mm_attrs_get_double_by_name(attrs, "display_zoom", &zoom);
				mm_attrs_get_int_by_name(attrs, "display_method", &display_method);
				mm_attrs_get_int_by_name(attrs, "display_roi_x", &roi_x);
				mm_attrs_get_int_by_name(attrs, "display_roi_y", &roi_y);
				mm_attrs_get_int_by_name(attrs, "display_roi_width", &roi_w);
				mm_attrs_get_int_by_name(attrs, "display_roi_height", &roi_h);
				mm_attrs_get_int_by_name(attrs, "display_visible", &visible);
				#define DEFAULT_DISPLAY_MODE	2	// TV only, PRI_VIDEO_OFF_AND_SEC_VIDEO_FULL_SCREEN

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
					"zoom", (float)zoom,
					"orientation", org_angle/90, // setting for orientation of media, it is used for ROI/ZOOM feature in xvimagesink
					"rotate", rotation_value,
					"handle-events", TRUE,
					"display-geometry-method", display_method,
					"draw-borders", FALSE,
					"visible", visible,
					"display-mode", DEFAULT_DISPLAY_MODE,
					NULL );

				debug_log("set video param : zoom %lf, rotate %d, method %d visible %d", zoom, rotation_value, display_method, visible);
				debug_log("set video param : dst-roi-x: %d, dst-roi-y: %d, dst-roi-w: %d, dst-roi-h: %d", roi_x, roi_y, roi_w, roi_h );
				debug_log("set video param : force aspect ratio %d, display mode %d", force_aspect_ratio, DEFAULT_DISPLAY_MODE);
			}

            /* if vaapisink */
            if (!strncmp(PLAYER_INI()->videosink_element_x, "vaapisink", strlen("vaapisink")))
            {
                g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst,
                        "rotation", rotation_value,
                        NULL);
                debug_log("set video param: vaapisink rotation %d", rotation_value);
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
				debug_log("set video param : evas-object %x, visible %d", object, visible);
			}
			else
			{
				debug_error("no evas object");
				return MM_ERROR_PLAYER_INTERNAL;
			}

			/* if evasimagesink */
			if (!strcmp(PLAYER_INI()->videosink_element_evas,"evasimagesink") && player->is_nv12_tiled)
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
					debug_log("set video param : video frame scaling down to width(%d) height(%d)", width, height);
				}
				debug_log("set video param : display_evas_do_scaling %d", scaling);
			}

			/* if evaspixmapsink */
			if (!strcmp(PLAYER_INI()->videosink_element_evas,"evaspixmapsink"))
			{
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

				debug_log("set video param : method %d", display_method);
				debug_log("set video param : dst-roi-x: %d, dst-roi-y: %d, dst-roi-w: %d, dst-roi-h: %d",
								roi_x, roi_y, roi_w, roi_h );
				debug_log("set video param : display_evas_do_scaling %d (origin-size %d)", scaling, origin_size);
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
			if (strcmp(PLAYER_INI()->videosink_element_x,"xvimagesink"))
			{
				debug_error("videosink is not xvimagesink");
				return MM_ERROR_PLAYER_INTERNAL;
			}

			/* get information from attributes */
			mm_attrs_get_data_by_name(attrs, "display_overlay", &pixmap_id_cb);
			mm_attrs_get_data_by_name(attrs, "display_overlay_user_data", &pixmap_id_cb_user_data);
			mm_attrs_get_int_by_name(attrs, "display_method", &display_method);
			mm_attrs_get_int_by_name(attrs, "display_visible", &visible);

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
	}

	debug_fleave();

	return MM_ERROR_NONE;
}

static GstPadProbeReturn
__mmplayer_audio_stream_probe (GstPad *pad, GstPadProbeInfo *info, gpointer u_data)
{
	mm_player_t* player = (mm_player_t*) u_data;
	GstBuffer *pad_buffer = gst_pad_probe_info_get_buffer(info);
	GstMapInfo probe_info = GST_MAP_INFO_INIT;

	gst_buffer_map(pad_buffer, &probe_info, GST_MAP_READ);
	if (player->audio_stream_cb && probe_info.size && probe_info.data)
		player->audio_stream_cb(probe_info.data, probe_info.size, player->audio_stream_cb_user_param);
	gst_buffer_unmap(pad_buffer, &probe_info);

    return GST_PAD_PROBE_OK;
}


gboolean
__mmplayer_update_subtitle( GstElement* object, GstBuffer *buffer, GstPad *pad, gpointer data)
{
	mm_player_t* player = (mm_player_t*) data;
	MMMessageParamType msg = {0, };
	GstClockTime duration = 0;
	gpointer text = NULL;
	gboolean ret = TRUE;
	GstMapInfo info;

	debug_fenter();

	return_val_if_fail ( player, FALSE );
	return_val_if_fail ( buffer, FALSE );

	gst_buffer_map (buffer, &info, GST_MAP_READ);
	text = g_memdup(info.data, info.size);
	gst_buffer_unmap (buffer, &info);

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

	gst_buffer_insert_memory(buffer, -1, gst_memory_new_wrapped(0, (guint8*)buf, size, 0, size, (guint8*)buf, g_free));

	debug_log("feed buffer %p, length %u\n", buf, size);
	g_signal_emit_by_name (player->pipeline->mainbin[MMPLAYER_M_SRC].gst, "push-buffer", buffer, &gst_ret);

	debug_fleave();

	return ret;
}

GstBusSyncReply
__mmplayer_bus_sync_callback (GstBus * bus, GstMessage * message, gpointer data)
{
	mm_player_t *player = (mm_player_t *)data;

	switch (GST_MESSAGE_TYPE (message))
	{
		case GST_MESSAGE_TAG:
			__mmplayer_gst_extract_tag_from_msg(player, message);
			break;
		case GST_MESSAGE_DURATION:
			__mmplayer_gst_handle_duration(player, message);
			break;

		default:
			return GST_BUS_PASS;
	}
	gst_message_unref (message);

	return GST_BUS_DROP;
}

void __mmplayer_remove_g_source_from_context(guint source_id)
{
	GMainContext *context = g_main_context_get_thread_default ();
	GSource *source = NULL;

	debug_fenter();

	source = g_main_context_find_source_by_id (context, source_id);

	if (source != NULL)
	{
		debug_log("context : %x, source : %x", context, source);
		g_source_destroy(source);
	}

	debug_fleave();
}
void __mmplayer_do_sound_fadedown(mm_player_t* player, unsigned int time)
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

void __mmplayer_undo_sound_fadedown(mm_player_t* player)
{
	debug_fenter();

	return_if_fail(player
		&& player->pipeline
		&& player->pipeline->audiobin
		&& player->pipeline->audiobin[MMPLAYER_A_SINK].gst);

	g_object_set(G_OBJECT(player->pipeline->audiobin[MMPLAYER_A_SINK].gst), "mute", 0, NULL);

	debug_fleave();
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
	debug_warning("incomming uri : %s\n", uri);
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

	if(event_src == ASM_EVENT_SOURCE_EARJACK_UNPLUG )
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
			debug_warning ("Got unexpected asm command (%d)", command);
		break;

		case ASM_COMMAND_STOP: // notification case
		{
			debug_warning("Got msg from asm to stop");

			result = _mmplayer_stop((MMHandleType)player);
			if (result != MM_ERROR_NONE)
			{
				debug_warning("fail to set stop state by asm");
				cb_res = ASM_CB_RES_IGNORE;
			}
			else
			{
				cb_res = ASM_CB_RES_STOP;
			}
			player->sm.by_asm_cb = 0; // reset because no message any more from asm
		}
		break;

		case ASM_COMMAND_PAUSE:
		{
			debug_warning("Got msg from asm to Pause");

			if(event_src == ASM_EVENT_SOURCE_CALL_START
				|| event_src == ASM_EVENT_SOURCE_ALARM_START
				|| event_src == ASM_EVENT_SOURCE_MEDIA)
			{
				//hold 0.7 second to excute "fadedown mute" effect
				debug_warning ("do fade down->pause->undo fade down");

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
				debug_warning ("set lazy pause timer (id=[%d], timeout=[%d ms])", player->lazy_pause_event_id, LAZY_PAUSE_TIMEOUT_MSEC);
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
			debug_warning("Got msg from asm to Resume. So, application can resume. code (%d) \n", event_src);
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
		player->pd_file_save_path = NULL;
	}

	/* give default value of audio effect setting */
	player->bypass_audio_effect = TRUE;
	player->sound.volume = MM_VOLUME_FACTOR_DEFAULT;
	player->playback_rate = DEFAULT_PLAYBACK_RATE;

	player->play_subtitle = FALSE;
	player->use_textoverlay = FALSE;

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
__mmplayer_destroy_streaming_ext(mm_player_t* player)
{
	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	if (player->pd_downloader)
		_mmplayer_unrealize_pd_downloader((MMHandleType)player);

	if (MMPLAYER_IS_HTTP_PD(player))
		_mmplayer_destroy_pd_downloader((MMHandleType)player);

	if (MMPLAYER_IS_STREAMING(player))
	{
		if (player->streamer)
		{
			__mm_player_streaming_deinitialize (player->streamer);
			__mm_player_streaming_destroy(player->streamer);
			player->streamer = NULL;
		}
	}
	return MM_ERROR_NONE;
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

	__mmplayer_destroy_streaming_ext(player);

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
	if ( MM_ERROR_NONE != _mmplayer_asm_unregister(&player->sm) )
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
		__mmplayer_remove_g_source_from_context(player->lazy_pause_event_id);
		player->lazy_pause_event_id = 0;
	}

	debug_fleave();

	return MM_ERROR_NONE;
}

int
__mmplayer_realize_streaming_ext(mm_player_t* player)
{
	int ret = MM_ERROR_NONE;

	debug_fenter();
	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	if (MMPLAYER_IS_HTTP_PD(player))
	{
		gboolean bret = FALSE;

		player->pd_downloader = _mmplayer_create_pd_downloader();
		if ( !player->pd_downloader )
		{
			debug_error ("Unable to create PD Downloader...");
			ret = MM_ERROR_PLAYER_NO_FREE_SPACE;
		}

		bret = _mmplayer_realize_pd_downloader((MMHandleType)player, player->profile.uri, player->pd_file_save_path, player->pipeline->mainbin[MMPLAYER_M_SRC].gst);

		if (FALSE == bret)
		{
			debug_error ("Unable to create PD Downloader...");
			ret = MM_ERROR_PLAYER_NOT_INITIALIZED;
		}
	}

	debug_fleave();
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
		ret = __mmplayer_realize_streaming_ext(player);
	}

	debug_fleave();

	return ret;
}

int
__mmplayer_unrealize_streaming_ext(mm_player_t *player)
{
	debug_fenter();
	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* destroy can called at anytime */
	if (player->pd_downloader && MMPLAYER_IS_HTTP_PD(player))
	{
		_mmplayer_unrealize_pd_downloader ((MMHandleType)player);
		player->pd_downloader = NULL;
	}

	debug_fleave();
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

	__mmplayer_unrealize_streaming_ext(player);

	/* unrealize pipeline */
	ret = __gst_unrealize( player );

	/* set player state if success */
	if ( MM_ERROR_NONE == ret )
	{
		if (player->sm.state != ASM_STATE_STOP) {
			ret = _mmplayer_asm_set_state(hplayer, ASM_STATE_STOP);
			if ( ret )
			{
				debug_error("failed to set asm state to STOP\n");
				return ret;
			}
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

int
_mmplayer_set_videoframe_render_error_cb(MMHandleType hplayer, mm_player_video_frame_render_error_callback callback, void *user_param) // @
{
	mm_player_t* player = (mm_player_t*) hplayer;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( callback, MM_ERROR_INVALID_ARGUMENT );

	player->video_frame_render_error_cb = callback;
	player->video_frame_render_error_cb_user_param = user_param;

	debug_log("Video frame render error cb Handle value is %p : %p\n", player, player->video_frame_render_error_cb);

	debug_fleave();

	return MM_ERROR_NONE;
}

int
__mmplayer_start_streaming_ext(mm_player_t *player)
{
	gint ret = MM_ERROR_NONE;

	debug_fenter();
	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	if (MMPLAYER_IS_HTTP_PD(player))
	{
		if ( !player->pd_downloader )
		{
			ret = __mmplayer_realize_streaming_ext(player);

			if ( ret != MM_ERROR_NONE)
			{
				debug_error ("failed to realize streaming ext\n");
				return ret;
			}
		}

		if (player->pd_downloader && player->pd_mode == MM_PLAYER_PD_MODE_URI)
		{
			ret = _mmplayer_start_pd_downloader ((MMHandleType)player);
			if ( !ret )
			{
				debug_error ("ERROR while starting PD...\n");
				return MM_ERROR_PLAYER_NOT_INITIALIZED;
			}
			ret = MM_ERROR_NONE;
		}
	}

	debug_fleave();
	return ret;
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

	ret = __mmplayer_start_streaming_ext(player);
	if ( ret != MM_ERROR_NONE )
	{
		debug_error("failed to start streaming ext \n");
	}

	/* start pipeline */
	ret = __gst_start( player );
	if ( ret != MM_ERROR_NONE )
	{
		debug_error("failed to start player.\n");
	}

	debug_fleave();

	return ret;
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

	__mmplayer_unrealize_streaming_ext(player);

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
			if ( !gst_element_query_position(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, fmt, &pos_msec))
			debug_warning("getting current position failed in paused\n");

			player->last_position = pos_msec;
		}
		break;
	}

	/* pause pipeline */
	ret = __gst_pause( player, async );

	if ( ret != MM_ERROR_NONE )
	{
		debug_error("failed to pause player. ret : 0x%x\n", ret);
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
					player->playback_rate,
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

int
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
	ret = gst_element_query_duration(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, fmt, &dur_nsec);
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
	gst_element_query_position(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, fmt, &cur_pos);

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
		ret = gst_element_query_position(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, format, &pos_msec);

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
_mmplayer_adjust_subtitle_postion(MMHandleType hplayer, int format, unsigned long position) // @
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

void
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

	/* midi type should be stored because it will be used to set audio gain in avsysaudiosink */
	if ( __mmplayer_is_midi_type(player->type))
	{
		player->profile.play_mode = MM_PLAYER_MODE_MIDI;
	}
	else if (__mmplayer_is_amr_type(player->type))
	{
		player->bypass_audio_effect = FALSE;
		if ( (PLAYER_INI()->use_audio_effect_preset || PLAYER_INI()->use_audio_effect_custom) )
		{
			if ( player->audio_effect_info.effect_type == MM_AUDIO_EFFECT_TYPE_PRESET )
			{
				if (!_mmplayer_audio_effect_preset_apply(player, player->audio_effect_info.preset))
				{
					debug_msg("apply audio effect(preset:%d) setting success\n",player->audio_effect_info.preset);
				}
			}
			else if ( player->audio_effect_info.effect_type == MM_AUDIO_EFFECT_TYPE_CUSTOM )
			{
				if (!_mmplayer_audio_effect_custom_apply(player))
				{
					debug_msg("apply audio effect(custom) setting success\n");
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
		gboolean async = FALSE;

		debug_error("failed to autoplug %s\n", player->type);
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
gboolean
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
	GstStructure *structure = NULL;

	debug_fenter();

	return_val_if_fail( player && player->pipeline && player->pipeline->mainbin, FALSE );

	mainbin = player->pipeline->mainbin;

	mime = gst_structure_get_name(gst_caps_get_structure(caps, 0));

	/* return if we got raw output */
	if(g_str_has_prefix(mime, "video/x-raw") || g_str_has_prefix(mime, "audio/x-raw")
		|| g_str_has_prefix(mime, "video/x-surface")
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
		klass = gst_element_factory_get_klass( gst_element_get_factory(element) );

		/* add queue if needed */
		if( (g_strrstr(klass, "Demux") || g_strrstr(klass, "Depayloader")
			|| g_strrstr(klass, "Parse")) &&  !g_str_has_prefix(mime, "text"))
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
            if ( g_strrstr(GST_OBJECT_NAME (factory),
					PLAYER_INI()->exclude_element_keyword[idx] ) )
			{
				debug_warning("skipping [%s] by exculde keyword [%s]\n",
                    GST_OBJECT_NAME (factory),
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
            debug_log("skipping [%s] by not required\n", GST_OBJECT_NAME (factory));
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

			if ( GST_IS_CAPS( temp1->static_caps.caps) )
			{
				/* using existing caps */
				static_caps = gst_caps_ref( temp1->static_caps.caps );
			}
			else
			{
				/* create one */
				static_caps = gst_caps_from_string ( temp1->static_caps.string );
			}

			if ( strcmp (GST_OBJECT_NAME(factory),"rtpamrdepay") ==0 )
			{
				/* store encoding-name */
				structure = gst_caps_get_structure (caps, 0);

				/* figure out the mode first and set the clock rates */
				player->temp_encode_name = gst_structure_get_string (structure, "encoding-name");

			}
			if (player->temp_encode_name != NULL)
			{
				if ((strcmp (player->temp_encode_name, "AMR") == 0) && (strcmp (GST_OBJECT_NAME(factory), "amrwbdec" ) == 0))
				{
					debug_log("skip AMR-WB dec\n");
					continue;
				}
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

				debug_log("found %s to plug\n", name_to_plug);

				new_element = gst_element_factory_create(GST_ELEMENT_FACTORY(factory), NULL);
				if ( ! new_element )
				{
					debug_error("failed to create element [%s]. continue with next.\n",
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
                        gst_caps_unref (caps);
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

	/* There is no available codec. */
	__mmplayer_check_not_supported_codec(player,(gchar *)mime);

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


void __mmplayer_pipeline_complete(GstElement *decodebin,  gpointer data) // @
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
		( player->msg_posted == FALSE ) &&
		( player->cmd >= MMPLAYER_COMMAND_START ))
	{
		__mmplayer_handle_missed_plugin( player );
	}

	MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-complete" );
}

gboolean __mmplayer_configure_audio_callback(mm_player_t* player)
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

			player->audio_cb_probe_id = gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
				__mmplayer_audio_stream_probe, player, NULL);

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

	player->factories = gst_registry_feature_filter(gst_registry_get(),
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

void
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
	player->pending_seek.is_pending = FALSE;
	player->pending_seek.format = MM_PLAYER_POS_FORMAT_TIME;
	player->pending_seek.pos = 0;
	player->msg_posted = FALSE;
	player->has_many_types = FALSE;
	player->temp_encode_name = NULL;

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

	/* free memory related to audio effect */
	if(player->audio_effect_info.custom_ext_level_for_plugin)
	{
		free(player->audio_effect_info.custom_ext_level_for_plugin);
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
		const gchar *name = NULL;

		name = g_strdup(GST_ELEMENT_NAME( GST_PAD_PARENT ( srcpad )));

		if (g_strrstr(name, "mpegtsdemux"))
		{
			gchar *demux_caps = NULL;
			gchar *parser_name = NULL;
			GstCaps *dcaps = NULL;

			dcaps = gst_pad_query_caps(srcpad, NULL);
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
			if (!MMPLAYER_IS_RTSP_STREAMING(player))
				g_object_set (G_OBJECT (mainbin[MMPLAYER_M_Q1].gst), "max-size-time", q_max_size_time * GST_SECOND, NULL);
		}
		else if(mainbin[MMPLAYER_M_Q2].gst == NULL)
		{
			mainbin[MMPLAYER_M_Q2].id = MMPLAYER_M_Q2;
			mainbin[MMPLAYER_M_Q2].gst = queue;
			if (!MMPLAYER_IS_RTSP_STREAMING(player))
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
			srccaps = gst_pad_query_caps( srcpad, NULL );
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
						if ( !gst_element_query_duration(player->pipeline->mainbin[MMPLAYER_M_SRC].gst, fmt, &dur_bytes))
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
				GstCaps *caps = gst_pad_query_caps(srcpad, NULL);

				/* Check whether caps has many types */
				if ( !gst_caps_is_fixed (caps)) {
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

	caps = gst_pad_query_caps(pad, NULL);
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

	caps = gst_pad_get_current_caps(pad);

	if ( NULL == caps )
	{
		caps = gst_pad_query_caps(pad, NULL);
		if ( !caps ) return;
	}

	//MMPLAYER_LOG_GST_CAPS_TYPE(caps);

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
__mmplayer_can_extract_pcm( mm_player_t* player )
{
	MMHandleType attrs = 0;
	gboolean is_drm = FALSE;
	gboolean sound_extraction = FALSE;

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
		debug_log("checking pcm extraction mode : %d, drm : %d", sound_extraction, is_drm);
		return FALSE;
	}

	return TRUE;
}

void
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

void __mmplayer_set_antishock( mm_player_t* player, gboolean disable_by_force)
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

/* Note : if silent is true, then subtitle would not be displayed. :*/
int _mmplayer_set_subtitle_silent (MMHandleType hplayer, int silent)
{
	mm_player_t* player = (mm_player_t*) hplayer;

	debug_fenter();

	/* check player handle */
	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED );

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

int
_mmplayer_set_display_zoom(MMHandleType hplayer, float level)
{
	mm_player_t* player = (mm_player_t*) hplayer;

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	MMPLAYER_VIDEO_SINK_CHECK(player);

	debug_log("setting display zoom level = %f", level);

	g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst, "zoom", level, NULL);

	return MM_ERROR_NONE;
}

int
_mmplayer_get_display_zoom(MMHandleType hplayer, float *level)
{
	mm_player_t* player = (mm_player_t*) hplayer;

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	MMPLAYER_VIDEO_SINK_CHECK(player);

	g_object_get(player->pipeline->videobin[MMPLAYER_V_SINK].gst, "zoom", level, NULL);

	debug_log("display zoom level = %f", *level);

	return MM_ERROR_NONE;
}

int
_mmplayer_set_display_zoom_start_pos(MMHandleType hplayer, int x, int y)
{
	mm_player_t* player = (mm_player_t*) hplayer;

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	MMPLAYER_VIDEO_SINK_CHECK(player);

	debug_log("setting display zoom offset = %d, %d", x, y);

	g_object_set(player->pipeline->videobin[MMPLAYER_V_SINK].gst, "zoom-pos-x", x, "zoom-pos-y", y, NULL);

	return MM_ERROR_NONE;
}

int
_mmplayer_get_display_zoom_start_pos(MMHandleType hplayer, int *x, int *y)
{
	int _x = 0;
	int _y = 0;
	mm_player_t* player = (mm_player_t*) hplayer;

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	MMPLAYER_VIDEO_SINK_CHECK(player);

	g_object_get(player->pipeline->videobin[MMPLAYER_V_SINK].gst, "zoom-pos-x", &_x, "zoom-pos-y", &_y, NULL);

	debug_log("display zoom start off x = %d, y = %d", _x, _y);

	*x = _x;
	*y = _y;

	return MM_ERROR_NONE;
}
