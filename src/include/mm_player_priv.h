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

#ifndef __MM_PLAYER_PRIV_H__
#define	__MM_PLAYER_PRIV_H__

/*===========================================================================================
|																							|
|  INCLUDE FILES																			|
|  																							|
========================================================================================== */
#include <glib.h>
#include <gst/gst.h>
#include <mm_types.h>
#include <mm_attrs.h>
#include <mm_ta.h>
#include <mm_debug.h>

#include "mm_player.h"
#include "mm_player_internal.h"
#include "mm_player_audioeffect.h"
#include "mm_message.h"
#include "mm_player_utils.h"
#include "mm_player_asm.h"
#include "mm_player_pd.h"
#include "mm_player_streaming.h"

/*===========================================================================================
|																							|
|  GLOBAL DEFINITIONS AND DECLARATIONS FOR MODULE											|
|  																							|
========================================================================================== */

/*---------------------------------------------------------------------------
|    GLOBAL #defines:														|
---------------------------------------------------------------------------*/

#define MM_PLAYER_IMGB_MPLANE_MAX	4
#define MM_PLAYER_STREAM_COUNT_MAX	3

#define MM_PLAYER_CAST(x_player) 		((mm_player_t *)(x_player))
/**
 * @x_player: MMHandleType of player
 *
 * Get the PD downloader of this player.
 */
#define MM_PLAYER_GET_PD(x_player)	(MM_PLAYER_CAST(x_player)->pd_downloader)
/**
 * @x_player: MMHandleType of player
 *
 * Get the attributes handle of this player.
 */
#define MM_PLAYER_GET_ATTRS(x_player)	(MM_PLAYER_CAST(x_player)->attrs)

/*---------------------------------------------------------------------------
|    GLOBAL CONSTANT DEFINITIONS:											|
---------------------------------------------------------------------------*/
enum tag_info
{
	TAG_AUDIO_CODEC	= 0x0001,
	TAG_VIDEO_CODEC	= 0x0002,
	TAG_ARTIST		= 0x0004,
	TAG_TITLE		= 0x0008,
	TAG_ALBUM		= 0x0010,
	TAG_GENRE		= 0x0020,
	TAG_COPYRIGHT	= 0x0040,
	TAG_DATE		= 0x0080,
	TAG_DESCRIPTION	= 0x0100,
	TAG_TRACK_NUMBER = 0x0200
};

/* async mode makes trouble. alsasink sometimes fails to pause. */
enum alassink_sync
{
	ALSASINK_SYNC,
	ALSASINK_ASYNC
};


/**
 * Enumerations of Player Mode
 */
enum MMPlayerMode {
	MM_PLAYER_MODE_NONE,			/**< Player mode None */
	MM_PLAYER_MODE_MIDI,			/**< Player mode Midi */
	MM_PLAYER_MODE_GST,			/**< Player mode Gstreamer */
};


/**
 * Enumerations of Player Uri type
 */
enum MMPlayerUriType {
	MM_PLAYER_URI_TYPE_NONE, 		/**< Player URI type None */
	MM_PLAYER_URI_TYPE_URL_RTSP,	/**< Player URI type RTSP */
	MM_PLAYER_URI_TYPE_URL_HTTP,/**< Player URI type HTTP */
	MM_PLAYER_URI_TYPE_URL_MMS,/**< Player URI type MMS */
	MM_PLAYER_URI_TYPE_MEM,		/**< Player URI type Mem */
	MM_PLAYER_URI_TYPE_FILE, 		/**< Player URI type File */
	MM_PLAYER_URI_TYPE_URL, 		/**< Player URI type URL */
	MM_PLAYER_URI_TYPE_BUFF, 		/**< Player URI type Buffer */
	MM_PLAYER_URI_TYPE_HLS,			/**< Player URI type http live streaming */	
	MM_PLAYER_URI_TYPE_TEMP,			/**< Player URI type Temp */
};

typedef enum _MissingCodec
{
	MISSING_PLUGIN_NONE 			= 0x00,
	MISSING_PLUGIN_AUDIO 			= 0x01,
	MISSING_PLUGIN_VIDEO 			= 0x02
}MissingCodec;


typedef enum _FoundCodec
{
	FOUND_PLUGIN_NONE 			= 0x00,
	FOUND_PLUGIN_AUDIO 			= 0x01,
	FOUND_PLUGIN_VIDEO 			= 0x02
}FoundCodec;

/* main pipeline's element id */
enum MainElementID
{
	MMPLAYER_M_PIPE = 0, /* NOTE : MMPLAYER_M_PIPE should be zero */
	MMPLAYER_M_SRC,

	/* it could be a decodebin or could be a typefind. depends on player ini */
	MMPLAYER_M_AUTOPLUG,

	/* NOTE : we need two fakesink to autoplug without decodebin.
	 * first one will hold whole pipeline state. and second one will hold state of
	 * a sink-decodebin for an elementary stream. no metter if there's more then one
	 * elementary streams because MSL reuse it.
	 */
	MMPLAYER_M_SRC_FAKESINK,
	MMPLAYER_M_SRC_2ND_FAKESINK,

	/* streaming plugin */
	MMPLAYER_M_S_BUFFER, 
	MMPLAYER_M_S_ADEC, 
	MMPLAYER_M_S_VDEC, 
	
	/* FIXIT : if there's really no usage for following IDs. remove it */
	MMPLAYER_M_DEC1,
	MMPLAYER_M_DEC2,
	MMPLAYER_M_Q1,
	MMPLAYER_M_Q2,
	MMPLAYER_M_DEMUX,
	MMPLAYER_M_NUM
};

/* audio pipeline's element id */
enum AudioElementID
{
	MMPLAYER_A_BIN = 0, /* NOTE : MMPLAYER_A_BIN should be zero */
	MMPLAYER_A_TP,
	MMPLAYER_A_CONV,
	MMPLAYER_A_VOL,
	MMPLAYER_A_FILTER,
	MMPLAYER_A_CAPS_DEFAULT,
	MMPLAYER_A_SINK,
	MMPLAYER_A_RESAMPLER,
	MMPLAYER_A_NUM
};

/* video pipeline's element id */
enum VideoElementID
{
	MMPLAYER_V_BIN = 0, /* NOTE : MMPLAYER_V_BIN should be zero */
	MMPLAYER_V_FLIP,
	MMPLAYER_V_CONV,
	MMPLAYER_V_SCALE,
	MMPLAYER_V_CAPS,
	MMPLAYER_V_SINK,
	MMPLAYER_V_NUM
};

/* text pipeline's element id */
enum TextElementID
{
	MMPLAYER_T_BIN = 0, /* NOTE : MMPLAYER_V_BIN should be zero */
	MMPLAYER_T_QUEUE,
	MMPLAYER_T_OVERLAY,
	MMPLAYER_T_SINK,
	MMPLAYER_T_NUM
};

/* subtitle pipeline's element id */
enum SubtitleElementID
{
	MMPLAYER_SUB_PIPE = 0, /* NOTE : MMPLAYER_SUB_PIPE should be zero */
	MMPLAYER_SUB_SRC,
	MMPLAYER_SUB_QUEUE,
	MMPLAYER_SUB_SUBPARSE,
	MMPLAYER_SUB_TEXTRENDER,
	MMPLAYER_SUB_FLIP,
	MMPLAYER_SUB_CONV1,
	MMPLAYER_SUB_CONV2,
	MMPLAYER_SUB_SCALE,
	MMPLAYER_SUB_SINK,
	MMPLAYER_SUB_NUM
};

/* midi main pipeline's element id */
enum MidiElementID
{
	MMPLAYER_MIDI_PIPE,
	MMPLAYER_MIDI_PLAYER,
	MMPLAYER_MIDI_NUM
};

enum PlayerCommandState
{
	MMPLAYER_COMMAND_NONE,
	MMPLAYER_COMMAND_CREATE,
	MMPLAYER_COMMAND_DESTROY,
	MMPLAYER_COMMAND_REALIZE,
	MMPLAYER_COMMAND_UNREALIZE,
	MMPLAYER_COMMAND_START,
	MMPLAYER_COMMAND_STOP,
	MMPLAYER_COMMAND_PAUSE,
	MMPLAYER_COMMAND_RESUME,
	MMPLAYER_COMMAND_NUM
};

/* Note : StreamingSrcError is error enum for streaming source which post error message
 *	using custom message made by itself. The enum value must start with zero,
 *	because the streaming source(secrtspsrc) also does.
 */
enum StreamingSrcError
{
	MMPLAYER_STREAMING_ERROR_NONE = 0,
	MMPLAYER_STREAMING_ERROR_UNSUPPORTED_AUDIO,
	MMPLAYER_STREAMING_ERROR_UNSUPPORTED_VIDEO,
	MMPLAYER_STREAMING_ERROR_CONNECTION_FAIL,
	MMPLAYER_STREAMING_ERROR_DNS_FAIL,
	MMPLAYER_STREAMING_ERROR_SERVER_DISCONNECTED,
	MMPLAYER_STREAMING_ERROR_BAD_SERVER,
	MMPLAYER_STREAMING_ERROR_INVALID_PROTOCOL,
	MMPLAYER_STREAMING_ERROR_INVALID_URL,
	MMPLAYER_STREAMING_ERROR_UNEXPECTED_MSG,
	MMPLAYER_STREAMING_ERROR_OUT_OF_MEMORIES,
	MMPLAYER_STREAMING_ERROR_RTSP_TIMEOUT,
	MMPLAYER_STREAMING_ERROR_BAD_REQUEST,
	MMPLAYER_STREAMING_ERROR_NOT_AUTHORIZED,
	MMPLAYER_STREAMING_ERROR_PAYMENT_REQUIRED,
	MMPLAYER_STREAMING_ERROR_FORBIDDEN,
	MMPLAYER_STREAMING_ERROR_CONTENT_NOT_FOUND,
	MMPLAYER_STREAMING_ERROR_METHOD_NOT_ALLOWED,
	MMPLAYER_STREAMING_ERROR_NOT_ACCEPTABLE,
	MMPLAYER_STREAMING_ERROR_PROXY_AUTHENTICATION_REQUIRED,
	MMPLAYER_STREAMING_ERROR_SERVER_TIMEOUT,
	MMPLAYER_STREAMING_ERROR_GONE,
	MMPLAYER_STREAMING_ERROR_LENGTH_REQUIRED,
	MMPLAYER_STREAMING_ERROR_PRECONDITION_FAILED,
	MMPLAYER_STREAMING_ERROR_REQUEST_ENTITY_TOO_LARGE,
	MMPLAYER_STREAMING_ERROR_REQUEST_URI_TOO_LARGE,
	MMPLAYER_STREAMING_ERROR_UNSUPPORTED_MEDIA_TYPE,
	MMPLAYER_STREAMING_ERROR_PARAMETER_NOT_UNDERSTOOD,
	MMPLAYER_STREAMING_ERROR_CONFERENCE_NOT_FOUND,
	MMPLAYER_STREAMING_ERROR_NOT_ENOUGH_BANDWIDTH,
	MMPLAYER_STREAMING_ERROR_NO_SESSION_ID,
	MMPLAYER_STREAMING_ERROR_METHOD_NOT_VALID_IN_THIS_STATE,
	MMPLAYER_STREAMING_ERROR_HEADER_FIELD_NOT_VALID_FOR_SOURCE,
	MMPLAYER_STREAMING_ERROR_INVALID_RANGE,
	MMPLAYER_STREAMING_ERROR_PARAMETER_IS_READONLY,
	MMPLAYER_STREAMING_ERROR_AGGREGATE_OP_NOT_ALLOWED,
	MMPLAYER_STREAMING_ERROR_ONLY_AGGREGATE_OP_ALLOWED,
	MMPLAYER_STREAMING_ERROR_BAD_TRANSPORT,
	MMPLAYER_STREAMING_ERROR_DESTINATION_UNREACHABLE,
	MMPLAYER_STREAMING_ERROR_INTERNAL_SERVER_ERROR,
	MMPLAYER_STREAMING_ERROR_NOT_IMPLEMENTED,
	MMPLAYER_STREAMING_ERROR_BAD_GATEWAY,
	MMPLAYER_STREAMING_ERROR_SERVICE_UNAVAILABLE,
	MMPLAYER_STREAMING_ERROR_GATEWAY_TIME_OUT	,
	MMPLAYER_STREAMING_ERROR_RTSP_VERSION_NOT_SUPPORTED,
	MMPLAYER_STREAMING_ERROR_OPTION_NOT_SUPPORTED,
};


/*---------------------------------------------------------------------------
|    GLOBAL DATA TYPE DEFINITIONS:											|
---------------------------------------------------------------------------*/

typedef struct
{
	int id;
	GstElement *gst;
} MMPlayerGstElement;

typedef struct
{
	GstTagList			*tag_list;
	MMPlayerGstElement 	*mainbin;
	MMPlayerGstElement 	*subtitlebin;
	MMPlayerGstElement 	*audiobin;
	MMPlayerGstElement 	*videobin;
	MMPlayerGstElement 	*textbin;
} MMPlayerGstPipelineInfo;

typedef struct
{
	char	device[MAX_SOUND_DEVICE_LEN];
	float volume;
	int mute;
	int bluetooth;	/* enable/disable */
} MMPlayerSoundInfo;

typedef struct {
	char *buf;
	int len;
	int offset;

} tBuffer; /* FIXIT : choose better name */

typedef struct {
	int uri_type;
	int	play_mode;
	void *mem;
	int	mem_size;
	char uri[MM_MAX_URL_LEN];
	char urgent[MM_MAX_FILENAME_LEN];
} MMPlayerParseProfile;

typedef struct {
	bool is_pending;
	MMPlayerPosFormatType format;
	unsigned long pos;
}MMPlayerPendingSeek;

typedef struct {
	GObject* obj;
	gulong sig;
} MMPlayerSignalItem;

/* image buffer definition ***************************************************

    +------------------------------------------+ ---
    |                                          |  ^
    |     a[], p[]                             |  |
    |     +---------------------------+ ---    |  |
    |     |                           |  ^     |  |
    |     |<---------- w[] ---------->|  |     |  |
    |     |                           |  |     |  |
    |     |                           |        |
    |     |                           |  h[]   |  e[]
    |     |                           |        |
    |     |                           |  |     |  |
    |     |                           |  |     |  |
    |     |                           |  v     |  |
    |     +---------------------------+ ---    |  |
    |                                          |  v
    +------------------------------------------+ ---

    |<----------------- s[] ------------------>|
*/
typedef struct
{
	/* width of each image plane */
	int	w[MM_PLAYER_IMGB_MPLANE_MAX];
	/* height of each image plane */
	int	h[MM_PLAYER_IMGB_MPLANE_MAX];
	/* stride of each image plane */
	int	s[MM_PLAYER_IMGB_MPLANE_MAX];
	/* elevation of each image plane */
	int	e[MM_PLAYER_IMGB_MPLANE_MAX];
	/* user space address of each image plane */
	void	*a[MM_PLAYER_IMGB_MPLANE_MAX];
	/* physical address of each image plane, if needs */
	void	*p[MM_PLAYER_IMGB_MPLANE_MAX];
	/* color space type of image */
	int	cs;
	/* left postion, if needs */
	int	x;
	/* top position, if needs */
	int	y;
	/* to align memory */
	int	__dummy2;
	/* arbitrary data */
	int	data[16];
} MMPlayerMPlaneImage;

typedef struct {
	/* STATE */
	int state;					// player current state
	int prev_state;				// player previous state
	int pending_state;			// player state which is going to now
	int target_state;				// player state which user want to go to
	guint state_change_timeout;	

	gboolean section_repeat;
	gint section_repeat_start;
	gint section_repeat_end;

	gchar *album_art;

	int cmd;

	/* command lock */
	GMutex* cmd_lock;

	/* repeat thread lock */
	GCond* repeat_thread_cond;
	GMutex* repeat_thread_mutex;
	GThread* repeat_thread;
	gboolean repeat_thread_exit;

	/* capture thread */
	GThread* capture_thread;
	gboolean capture_thread_exit;
	GCond* capture_thread_cond;
	GMutex* capture_thread_mutex;
	MMPlayerVideoCapture capture;
	MMPlayerVideoColorspace video_cs;	
	MMPlayerMPlaneImage captured;
	
	/* fakesink handling lock */
	GMutex* fsink_lock;

	/* player attributes */
	MMHandleType attrs;

	/* message callback */
	MMMessageCallback msg_cb;
	void* msg_cb_param;
	GMutex* msg_cb_lock;

	/* progressive download */
	mm_player_pd_t *pd_downloader;
	gchar *pd_file_save_path;
	MMPlayerPDMode pd_mode;

	/* streaming player */
	mm_player_streaming_t *streamer;

	/* gstreamer pipeline */
	MMPlayerGstPipelineInfo	*pipeline;
	gboolean pipeline_is_constructed;

	/* Buffering support cbs*/
	mm_player_buffer_need_data_callback need_data_cb;
	mm_player_buffer_enough_data_callback enough_data_cb;
	mm_player_buffer_seek_data_callback seek_data_cb;

	void* buffer_cb_user_param;

	/* for video stream callback */
	mm_player_video_stream_callback video_stream_cb;
	void* video_stream_cb_user_param;
	int use_video_stream;

	/* audio stram callback */
	mm_player_audio_stream_callback audio_stream_cb;
	void* audio_stream_cb_user_param;

	/* audio buffer callback */
	mm_player_audio_stream_callback audio_buffer_cb;
	void* audio_buffer_cb_user_param;

	/* video capture callback*/
	gulong video_capture_cb_probe_id;

	/* sound info */
	MMPlayerSoundInfo	sound;

	/* type string */
	gchar *type;

	/* video stream caps parsed by demuxer */
	GstCaps* v_stream_caps;

	/* audio effect infomation */
	MMAudioEffectInfo audio_effect_info;
	gboolean bypass_audio_effect;

	gulong audio_cb_probe_id;

	/* for appsrc */
	tBuffer mem_buf;

	/* content profile */
	MMPlayerParseProfile profile;

	/* streaming service type */
	MMStreamingType streaming_type;

	/* autoplugging */
	GList* factories;
	gboolean have_dynamic_pad;
	GList* parsers; // list of linked parser name
	gboolean no_more_pad;
	gint num_dynamic_pad;
	gboolean has_many_types;

	/* progress callback timer */
	/* FIXIT : since duplicated functionality with get_position
	 * this function will be deprecated after fixing all application
	 * which are using it.
	 */
	guint progress_timer;

	/* timer for sending delayed EOS */
	guint eos_timer;

	/* last point (msec) that player is paused or seeking */
	gint64 last_position;

	/* duration */
	gint64 duration;

	/* data size of http streaming  */
	guint64 http_content_size;

	/* last error */
	gchar last_error_msg[1024]; /* FIXIT : should it be dynamic ? */

	gint videodec_linked;
	gint audiodec_linked;
	gint videosink_linked;
	gint audiosink_linked;
	gint textsink_linked;

	/* missing plugin during autoplugging */
	MissingCodec not_supported_codec;

	/*unlinked audio/video mime type */
	gchar *unlinked_video_mime;
	gchar *unlinked_audio_mime;
	gchar *unlinked_demuxer_mime;

	/* found codec during autoplugging */
	FoundCodec can_support_codec;

	gboolean not_found_demuxer;

	/* support seek even though player is not start */
	MMPlayerPendingSeek pending_seek;

	gboolean doing_seek;

	/* prevent to post msg over and over */
	gboolean posted_msg;

	/* list of sink elements */
	GList* sink_elements;

	/* signal notifiers */
	GList* signals;
	guint bus_watcher;

	/* NOTE : if sink elements receive flush start event then it's state will be lost.
	 * this can happen when doing buffering in streaming pipeline since all control operation
	 * (play/pause/resume/seek) is requiring server interaction. during 'state lost' situation
	 * _set_state will not work correctely and state transition message will not posted to our
	 * gst_callback.
	 * So. we need to do some special care on the situation.
	 */
	gboolean state_lost;

	gboolean need_update_content_attrs;
	gboolean need_update_content_dur;

	gboolean is_sound_extraction;

	gdouble playback_rate;
       /* player state resumed by fast rewind */
	gboolean resumed_by_rewind;

	gboolean is_nv12_tiled;

	MMPlayerASM	sm;

	gboolean is_subtitle_off;

	/* contents bitrate for buffering management */
	guint bitrate[MM_PLAYER_STREAM_COUNT_MAX];
	guint total_bitrate;
	guint updated_bitrate_count;
	guint maximum_bitrate[MM_PLAYER_STREAM_COUNT_MAX];
	guint total_maximum_bitrate;
	guint updated_maximum_bitrate_count;

	/* prevent it from posting duplicatly*/
	gboolean sent_bos;

	/* timeout source for lazy pause */
	guint lazy_pause_event_id;

	gboolean keep_detecting_vcodec;

	gboolean play_subtitle;

	/* PD downloader message callback and param */
	MMMessageCallback pd_msg_cb;
	void* pd_msg_cb_param;
} mm_player_t;

/*===========================================================================================
|																							|
|  GLOBAL FUNCTION PROTOTYPES																|
|  																							|
========================================================================================== */
#ifdef __cplusplus
	extern "C" {
#endif

int _mmplayer_create_player(MMHandleType hplayer);
int _mmplayer_destroy(MMHandleType hplayer);
int _mmplayer_realize(MMHandleType hplayer);
int _mmplayer_unrealize(MMHandleType hplayer);
int _mmplayer_get_state(MMHandleType hplayer, int* pstate);
int _mmplayer_set_volume(MMHandleType hplayer, MMPlayerVolumeType volume);
int _mmplayer_get_volume(MMHandleType hplayer, MMPlayerVolumeType *volume);
int _mmplayer_set_mute(MMHandleType hplayer, int mute);
int _mmplayer_get_mute(MMHandleType hplayer, int* pmute);
int _mmplayer_start(MMHandleType hplayer);
int _mmplayer_stop(MMHandleType hplayer);
int _mmplayer_pause(MMHandleType hplayer);
int _mmplayer_resume(MMHandleType hplayer);
int _mmplayer_set_position(MMHandleType hplayer, int format, int pos);
int _mmplayer_get_position(MMHandleType hplayer, int format, unsigned long *pos);
int _mmplayer_adjust_subtitle_postion(MMHandleType hplayer, int format,  int pos);
int _mmplayer_activate_section_repeat(MMHandleType hplayer, unsigned long start, unsigned long end);
int _mmplayer_deactivate_section_repeat(MMHandleType hplayer);
int _mmplayer_push_buffer(MMHandleType hplayer, unsigned char *buf, int size);
int _mmplayer_set_buffer_need_data_cb(MMHandleType hplayer,mm_player_buffer_need_data_callback callback, void *user_param);
int _mmplayer_set_buffer_enough_data_cb(MMHandleType hplayer,mm_player_buffer_enough_data_callback callback, void *user_param);
int _mmplayer_set_buffer_seek_data_cb(MMHandleType hplayer,mm_player_buffer_seek_data_callback callback, void *user_param);
int _mmplayer_set_playspeed(MMHandleType hplayer, gdouble rate);
int _mmplayer_set_message_callback(MMHandleType hplayer, MMMessageCallback callback, void *user_param);
int _mmplayer_set_videostream_cb(MMHandleType hplayer,mm_player_video_stream_callback callback, void *user_param);
int _mmplayer_set_audiostream_cb(MMHandleType hplayer,mm_player_audio_stream_callback callback, void *user_param);
int _mmplayer_set_subtitle_silent (MMHandleType hplayer, int silent);
int _mmplayer_get_subtitle_silent (MMHandleType hplayer, int* silent);
int _mmplayer_get_buffer_position(MMHandleType hplayer, int format, unsigned long* start_pos, unsigned long* stop_pos);
gboolean	_mmplayer_update_content_attrs(mm_player_t* player);
/* test API for tuning audio gain. this API should be
 * deprecated before the day of final release
 */
int _mmplayer_set_volume_tune(MMHandleType hplayer, MMPlayerVolumeType volume);
int _mmplayer_update_video_param(mm_player_t* player);
int _mmplayer_set_audiobuffer_cb(MMHandleType hplayer, mm_player_audio_stream_callback callback, void *user_param);

#ifdef __cplusplus
	}
#endif

#endif	/* __MM_PLAYER_PRIV_H__ */
