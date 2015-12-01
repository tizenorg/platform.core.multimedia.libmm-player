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

#ifndef __MM_PLAYER_UTILS_H__
#define __MM_PLAYER_UTILS_H__

#include <glib.h>
#include <gst/gst.h>
#include <dlog.h>
#include <mm_player_ini.h>
#include <mm_types.h>
#include <mm_error.h>
#include <mm_message.h>

#ifdef __cplusplus
	extern "C" {
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "MM_PLAYER"

/* general */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)		(sizeof(arr) / sizeof((arr)[0]))
#endif

#define MMPLAYER_MAX_INT	(2147483647)

#define MMPLAYER_FREEIF(x) \
if ( x ) \
	g_free( x ); \
x = NULL;

#define MMPLAYER_CMD_LOCK(x_player) g_mutex_lock(&((mm_player_t *)x_player)->cmd_lock)
#define MMPLAYER_CMD_UNLOCK(x_player)	g_mutex_unlock( &((mm_player_t*)x_player)->cmd_lock )
#define MMPLAYER_MSG_POST_LOCK(x_player)	g_mutex_lock( &((mm_player_t*)x_player)->msg_cb_lock )
#define MMPLAYER_MSG_POST_UNLOCK(x_player)	g_mutex_unlock( &((mm_player_t*)x_player)->msg_cb_lock )
#define MMPLAYER_PLAYBACK_LOCK(x_player) g_mutex_lock(&((mm_player_t *)x_player)->playback_lock)
#define MMPLAYER_PLAYBACK_UNLOCK(x_player) g_mutex_unlock( &((mm_player_t*)x_player)->playback_lock )
#define MMPLAYER_GET_ATTRS(x_player)		((mm_player_t*)x_player)->attrs

#if 0
#define MMPLAYER_FENTER();					LOGD("<ENTER>");
#define MMPLAYER_FLEAVE();					LOGD("<LEAVE>");
#else
#define MMPLAYER_FENTER();
#define MMPLAYER_FLEAVE();
#endif

#define MAX_SOUND_DEVICE_LEN	18

/* element linking */
#ifdef GST_EXT_PAD_LINK_UNCHECKED
#define GST_ELEMENT_LINK_FILTERED 	gst_element_link_filtered_unchecked
#define GST_ELEMENT_LINK_MANY 		gst_element_link_many_unchecked
#define GST_ELEMENT_LINK 			gst_element_link_unchecked
#define GST_ELEMENT_LINK_PADS 		gst_element_link_pads_unchecked
#define GST_PAD_LINK 				gst_pad_link_unchecked
#else
#define GST_ELEMENT_LINK_FILTERED 	gst_element_link_filtered
#define GST_ELEMENT_LINK_MANY 		gst_element_link_many
#define GST_ELEMENT_LINK 			gst_element_link
#define GST_ELEMENT_UNLINK 			gst_element_unlink
#define GST_ELEMENT_LINK_PADS 		gst_element_link_pads
#define GST_PAD_LINK 				gst_pad_link
#endif

#define MMPLAYER_RETURN_IF_FAIL(expr) \
		if(!(expr)) { \
			LOGW("faild [%s]", #expr); \
			return; \
		}

#define MMPLAYER_RETURN_VAL_IF_FAIL(expr, var) \
		if(!(expr)) { \
			LOGW("faild [%s]", #expr); \
			return (var); \
		}

/* debug caps string */
#define MMPLAYER_LOG_GST_CAPS_TYPE(x_caps) \
do \
{ \
	gchar* caps_type = NULL; \
	caps_type = gst_caps_to_string(x_caps); \
	LOGD ("caps: %s\n", caps_type ); \
	MMPLAYER_FREEIF (caps_type) \
} while (0)

/* message posting */
#define MMPLAYER_POST_MSG( x_player, x_msgtype, x_msg_param ) \
LOGD("posting %s to application\n", #x_msgtype); \
__mmplayer_post_message(x_player, x_msgtype, x_msg_param);

/* setting player state */
#define MMPLAYER_SET_STATE( x_player, x_state ) \
LOGD("update state machine to %d\n", x_state); \
__mmplayer_set_state(x_player, x_state);

#define MMPLAYER_CHECK_STATE( x_player, x_command ) \
LOGD("checking player state before doing %s\n", #x_command); \
switch ( __mmplayer_check_state(x_player, x_command) ) \
{ \
	case MM_ERROR_PLAYER_INVALID_STATE: \
		return MM_ERROR_PLAYER_INVALID_STATE; \
	break; \
	/* NOTE : for robustness of player. we won't treat it as an error */ \
	case MM_ERROR_PLAYER_NO_OP: \
		return MM_ERROR_NONE; \
	break; \
	case MM_ERROR_PLAYER_DOING_SEEK: \
		return MM_ERROR_PLAYER_DOING_SEEK; \
	default: \
	break; \
}

/* setting element state */
#define MMPLAYER_ELEMENT_SET_STATE( x_element, x_state ) \
LOGD("setting state [%s:%d] to [%s]\n", #x_state, x_state, GST_ELEMENT_NAME( x_element ) ); \
if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state ( x_element, x_state) ) \
{ \
	LOGE("failed to set state %s to %s\n", #x_state, GST_ELEMENT_NAME( x_element )); \
	goto STATE_CHANGE_FAILED; \
}

#define MMPLAYER_CHECK_NULL( x_var ) \
if ( ! x_var ) \
{ \
	LOGE("[%s] is NULL\n", #x_var ); \
	goto ERROR; \
}

#define MMPLAYER_CHECK_CMD_IF_EXIT( x_player ) \
if ( x_player->cmd == MMPLAYER_COMMAND_UNREALIZE || x_player->cmd == MMPLAYER_COMMAND_DESTROY ) \
{ \
	LOGD("it's exit state...\n");\
	goto ERROR;  \
}

/* generating dot */
#define MMPLAYER_GENERATE_DOT_IF_ENABLED( x_player, x_name ) \
if ( x_player->ini.generate_dot ) \
{ \
	GST_DEBUG_BIN_TO_DOT_FILE (GST_BIN (player->pipeline->mainbin[MMPLAYER_M_PIPE].gst), \
	GST_DEBUG_GRAPH_SHOW_ALL, x_name); \
}

/* signal manipulation */
#define MMPLAYER_SIGNAL_CONNECT( x_player, x_object, x_type, x_signal, x_callback, x_arg ) \
do \
{ \
	MMPlayerSignalItem* item = NULL; \
	item = (MMPlayerSignalItem*) g_malloc( sizeof (MMPlayerSignalItem) ); \
	if ( ! item ) \
	{ \
		LOGE("cannot connect signal [%s]\n", x_signal ); \
	} \
	else \
	{ \
		item->obj = G_OBJECT( x_object ); \
		item->sig = g_signal_connect( G_OBJECT(x_object), x_signal, \
					x_callback, x_arg ); \
		if ((x_type >= MM_PLAYER_SIGNAL_TYPE_AUTOPLUG) && (x_type < MM_PLAYER_SIGNAL_TYPE_MAX)) \
			x_player->signals[x_type] = g_list_append(x_player->signals[x_type], item); \
		else \
			LOGE("wrong signal type [%d]\n", x_type ); \
	} \
} while ( 0 );

/* release element resource */
#define MMPLAYER_RELEASE_ELEMENT( x_player, x_bin, x_id ) \
do \
{ \
	if (x_bin[x_id].gst) \
	{ \
		gst_element_set_state(x_bin[x_id].gst, GST_STATE_NULL); \
		gst_bin_remove(GST_BIN(x_player->pipeline->mainbin[MMPLAYER_M_PIPE].gst), x_bin[x_id].gst); \
		x_bin[x_id].gst = NULL; \
		LOGD("release done [element %d]", x_id); \
	} \
} while ( 0 )

/* state */
#define	MMPLAYER_PREV_STATE(x_player)		((mm_player_t*)x_player)->prev_state
#define	MMPLAYER_CURRENT_STATE(x_player)		((mm_player_t*)x_player)->state
#define 	MMPLAYER_PENDING_STATE(x_player)		((mm_player_t*)x_player)->pending_state
#define 	MMPLAYER_TARGET_STATE(x_player)		((mm_player_t*)x_player)->target_state
#define 	MMPLAYER_STATE_GET_NAME(state) __get_state_name(state)

#define 	MMPLAYER_PRINT_STATE(x_player) \
LOGD("-- prev %s, current %s, pending %s, target %s --\n", \
	MMPLAYER_STATE_GET_NAME(MMPLAYER_PREV_STATE(x_player)), \
 	MMPLAYER_STATE_GET_NAME(MMPLAYER_CURRENT_STATE(x_player)), \
	MMPLAYER_STATE_GET_NAME(MMPLAYER_PENDING_STATE(x_player)), \
	MMPLAYER_STATE_GET_NAME(MMPLAYER_TARGET_STATE(x_player)));

#define 	MMPLAYER_STATE_CHANGE_TIMEOUT(x_player )	 ((mm_player_t*)x_player)->state_change_timeout

/* streaming */
#define MMPLAYER_IS_STREAMING(x_player)  			__is_streaming(x_player)
#define MMPLAYER_IS_RTSP_STREAMING(x_player) 	__is_rtsp_streaming(x_player)
#define MMPLAYER_IS_WFD_STREAMING(x_player) 	__is_wfd_streaming(x_player)
#define MMPLAYER_IS_HTTP_STREAMING(x_player)  	__is_http_streaming(x_player)
#define MMPLAYER_IS_HTTP_PD(x_player)			__is_http_progressive_down(x_player)
#define MMPLAYER_IS_HTTP_LIVE_STREAMING(x_player)  __is_http_live_streaming(x_player)
#define MMPLAYER_IS_LIVE_STREAMING(x_player)  	__is_live_streaming(x_player)
#define MMPLAYER_IS_DASH_STREAMING(x_player)  	__is_dash_streaming(x_player)
#define MMPLAYER_IS_SMOOTH_STREAMING(x_player)	__is_smooth_streaming(x_player)
#define MMPLAYER_IS_MS_BUFF_SRC(x_player)		__is_ms_buff_src(x_player)

#define MMPLAYER_URL_HAS_DASH_SUFFIX(x_player) __has_suffix(x_player, "mpd")
#define MMPLAYER_URL_HAS_HLS_SUFFIX(x_player) __has_suffix(x_player, "m3u8")

/* etc */
#define	MMF_PLAYER_FILE_BACKUP_PATH		"/tmp/media_temp."
#define 	MMPLAYER_PT_IS_AUDIO( x_pt )		( strstr(x_pt, "_97") || strstr(x_pt, "audio") )
#define 	MMPLAYER_PT_IS_VIDEO( x_pt )		( strstr(x_pt, "_96") || strstr(x_pt, "video") )

#define MMPLAYER_VIDEO_SINK_CHECK(x_player) \
do \
{ \
	MMPLAYER_RETURN_VAL_IF_FAIL ( x_player && \
		x_player->pipeline && \
		x_player->pipeline->videobin && \
		x_player->pipeline->videobin[MMPLAYER_V_SINK].gst, \
		MM_ERROR_PLAYER_NOT_INITIALIZED ); \
} while(0)

enum
{
	MMPLAYER_DISPLAY_NULL = 0,
	MMPLAYER_DISPLAY_HDMI_ACTIVE,
	MMPLAYER_DISPLAY_MIRRORING_ACTIVE,
};

bool util_is_sdp_file ( const char *path );
int util_get_rank_increase ( const char *factory_class );
int util_factory_rank_compare(GstPluginFeature *f1, GstPluginFeature *f2); // @
int util_exist_file_path(const char *file_path);
bool util_write_file_backup(const char *backup_path, char *data_ptr, int data_size);
bool util_remove_file_backup(const char *backup_path); /* For Midi Player */
int util_is_midi_type_by_mem(void *mem, int size);
int util_is_midi_type_by_file(const char *file_path);
char** util_get_cookie_list ( const char *cookies );
bool util_check_valid_url ( const char *proxy );
const char* util_get_charset(const char *file_path);
int util_get_pixtype(unsigned int fourcc);

#ifdef __cplusplus
	}
#endif

#endif /* __MM_PLAYER_UTILS_H__ */

