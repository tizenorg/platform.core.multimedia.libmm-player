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

#ifndef __MM_PLAYER_PRIV_H__
#define	__MM_PLAYER_PRIV_H__

/*===========================================================================================
|																							|
|  INCLUDE FILES																			|
|																						|
========================================================================================== */
#include <glib.h>
#include <gst/gst.h>
#include <mm_attrs.h>
#include <math.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <tbm_bufmgr.h>
#include <Evas.h>
#include "mm_player.h"
#include "mm_player_internal.h"
#include "mm_player_audioeffect.h"
#include "mm_message.h"
#include "mm_player_ini.h"
#include "mm_player_resource.h"
#include "mm_player_sound_focus.h"
#include "mm_player_pd.h"
#include "mm_player_streaming.h"

/*===========================================================================================
|																							|
|  GLOBAL DEFINITIONS AND DECLARATIONS FOR MODULE											|
|																							|
========================================================================================== */

/*---------------------------------------------------------------------------
|    GLOBAL #defines:														|
---------------------------------------------------------------------------*/

#define MM_PLAYER_IMGB_MPLANE_MAX	4
#define MM_PLAYER_STREAM_COUNT_MAX	3

#define MM_PLAYER_CAST(x_player)		((mm_player_t *)(x_player))
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

#define ROTATION_USING_SINK	0
#define ROTATION_USING_CUSTOM	1
#define ROTATION_USING_FLIP	2

/*---------------------------------------------------------------------------
|    GLOBAL CONSTANT DEFINITIONS:											|
---------------------------------------------------------------------------*/
enum latency_mode
{
    AUDIO_LATENCY_MODE_LOW = 0,     /**< Low audio latency mode */
    AUDIO_LATENCY_MODE_MID,         /**< Middle audio latency mode */
    AUDIO_LATENCY_MODE_HIGH,        /**< High audio latency mode */
};

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

enum content_attr_flag
{
	ATTR_MISSING_ONLY = 0x0001,
	ATTR_DURATION = 0x0002,
	ATTR_AUDIO  = 0x0004,
	ATTR_VIDEO = 0x0008,
	ATTR_BITRATE = 0x0010,
	ATTR_ALL = 0x0020,
};

/* async mode makes trouble. alsasink sometimes fails to pause. */
enum alassink_sync
{
	ALSASINK_SYNC,
	ALSASINK_ASYNC
};

/**
 * Enumerations of Player Uri type
 */
enum MMPlayerUriType {
	MM_PLAYER_URI_TYPE_NONE, 		/**< Player URI type None */
	MM_PLAYER_URI_TYPE_URL_RTSP,	/**< Player URI type RTSP */
	MM_PLAYER_URI_TYPE_URL_WFD,  /**< Player URI type WFD */
	MM_PLAYER_URI_TYPE_URL_HTTP,/**< Player URI type HTTP */
	MM_PLAYER_URI_TYPE_URL_MMS,/**< Player URI type MMS */
	MM_PLAYER_URI_TYPE_MEM,		/**< Player URI type Mem */
	MM_PLAYER_URI_TYPE_FILE, 		/**< Player URI type File */
	MM_PLAYER_URI_TYPE_URL, 		/**< Player URI type URL */
	MM_PLAYER_URI_TYPE_BUFF, 		/**< Player URI type Buffer */
	MM_PLAYER_URI_TYPE_MS_BUFF,		/**< Player URI type Media Stream Buffer */
	MM_PLAYER_URI_TYPE_HLS,			/**< Player URI type http live streaming */
	MM_PLAYER_URI_TYPE_SS,			/**< Player URI type Smooth streaming */
	MM_PLAYER_URI_TYPE_DASH,			/**< Player URI type Mpeg Dash */
	MM_PLAYER_URI_TYPE_NO_PERMISSION,/**< Player URI type No Permission  */
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

/**
 * Enumeration of signal type
 */
typedef enum {
	MM_PLAYER_SIGNAL_TYPE_AUTOPLUG = 0,
	MM_PLAYER_SIGNAL_TYPE_VIDEOBIN,
	MM_PLAYER_SIGNAL_TYPE_AUDIOBIN,
	MM_PLAYER_SIGNAL_TYPE_TEXTBIN,
	MM_PLAYER_SIGNAL_TYPE_OTHERS,
	MM_PLAYER_SIGNAL_TYPE_ALL,
	MM_PLAYER_SIGNAL_TYPE_MAX = MM_PLAYER_SIGNAL_TYPE_ALL,
}MMPlayerSignalType;

/* main pipeline's element id */
enum MainElementID
{
	MMPLAYER_M_PIPE = 0, /* NOTE : MMPLAYER_M_PIPE should be zero */
	MMPLAYER_M_SRC,
	MMPLAYER_M_2ND_SRC,	/* 2nd Source Element for es buff src */
	MMPLAYER_M_SUBSRC,

	/* it could be a decodebin or could be a typefind. depends on player ini */
	MMPLAYER_M_TYPEFIND,
	MMPLAYER_M_AUTOPLUG,

	MMPLAYER_M_AUTOPLUG_V_DEC,
	MMPLAYER_M_AUTOPLUG_A_DEC,

	/* NOTE : we need two fakesink to autoplug without decodebin.
	 * first one will hold whole pipeline state. and second one will hold state of
	 * a sink-decodebin for an elementary stream. no metter if there's more then one
	 * elementary streams because MSL reuse it.
	 */
	MMPLAYER_M_SRC_FAKESINK,
	MMPLAYER_M_SRC_2ND_FAKESINK,

	/* streaming plugin */
	MMPLAYER_M_MUXED_S_BUFFER,
	MMPLAYER_M_DEMUXED_S_BUFFER,
	MMPLAYER_M_ID3DEMUX,

	/* es buff src queue */
	MMPLAYER_M_V_BUFFER,
	MMPLAYER_M_A_BUFFER,
	MMPLAYER_M_S_BUFFER,

	/* FIXIT : if there's really no usage for following IDs. remove it */
	MMPLAYER_M_DEC1,
	MMPLAYER_M_DEC2,
	MMPLAYER_M_Q1,
	MMPLAYER_M_Q2,
	MMPLAYER_M_DEMUX,
	MMPLAYER_M_SUBPARSE,
	MMPLAYER_M_DEMUX_EX,
	MMPLAYER_M_V_INPUT_SELECTOR,	// video input_select
	MMPLAYER_M_A_INPUT_SELECTOR,	// audio input_select
	MMPLAYER_M_T_INPUT_SELECTOR,	// text input_select
	MMPLAYER_M_A_TEE,
	MMPLAYER_M_A_Q1,
	MMPLAYER_M_A_Q2,
	MMPLAYER_M_A_CONV,
	MMPLAYER_M_A_FILTER,
	MMPLAYER_M_A_DEINTERLEAVE,
	MMPLAYER_M_A_SELECTOR,
	MMPLAYER_M_V_SINK,
	MMPLAYER_M_V_CONV,
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
	MMPLAYER_A_FILTER_SEC,
	MMPLAYER_A_VSP,
	MMPLAYER_A_CAPS_DEFAULT,
	MMPLAYER_A_SINK,
	MMPLAYER_A_RESAMPLER,
	MMPLAYER_A_DEINTERLEAVE,
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
	MMPLAYER_T_VIDEO_QUEUE,
	MMPLAYER_T_VIDEO_CONVERTER,
	MMPLAYER_T_OVERLAY,
	MMPLAYER_T_FAKE_SINK,
	MMPLAYER_T_IDENTITY,
	MMPLAYER_T_NUM
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
	MMPLAYER_COMMAND_UNREALIZE,
	MMPLAYER_COMMAND_START,
	MMPLAYER_COMMAND_REALIZE,
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
	MMPlayerGstElement 	*audiobin;
	MMPlayerGstElement 	*videobin;
	MMPlayerGstElement 	*textbin;
} MMPlayerGstPipelineInfo;

typedef struct
{
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

typedef struct {
	bool rich_audio;
	bool safety_volume;
	bool pcm_extraction;
	bool video_zc; // video zero-copy
	bool subtitle_off;
	bool media_packet_video_stream;
}MMPlayerSetMode;

typedef struct {
	GMainContext *global_default;
	GMainContext *thread_default;
}MMPlayerGMainContext;

typedef struct {
	gint uri_idx;
	GList *uri_list;
}MMPlayerUriList;

typedef struct {
	gint active_pad_index;
	gint total_track_num;
	GPtrArray *channels;
	gulong block_id;
	gulong event_probe_id;
} mm_player_selector_t;

typedef struct {
	gboolean running;

	gboolean stream_changed;
	gboolean reconfigure;

	GstClockTime next_pts;		/* latest decoded buffer's pts+duration */
	GstClockTime start_time;	/* updated once get SEGMENT event */

	gulong audio_data_probe_id;
	gulong video_data_probe_id;
} mm_player_gapless_t;

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
	guint play_count;

	gchar *album_art;

	int cmd;

	/* command lock */
	GMutex cmd_lock;
	GMutex playback_lock;

	/* repeat thread lock */
	GCond repeat_thread_cond;
	GMutex repeat_thread_mutex;
	GThread* repeat_thread;
	gboolean repeat_thread_exit;

	/* next play thread */
	GThread* next_play_thread;
	gboolean next_play_thread_exit;
	GCond next_play_thread_cond;
	GMutex next_play_thread_mutex;
	mm_player_gapless_t gapless;

	/* capture thread */
	GThread* capture_thread;
	gboolean capture_thread_exit;
	GCond capture_thread_cond;
	GMutex capture_thread_mutex;
	MMPlayerVideoCapture capture;
	MMPlayerVideoColorspace video_cs;
	MMVideoBuffer captured;

	/* fakesink handling lock */
	GMutex fsink_lock;

	/* player attributes */
	MMHandleType attrs;

	/* message callback */
	MMMessageCallback msg_cb;
	void* msg_cb_param;
	GMutex msg_cb_lock;

	/* progressive download */
	mm_player_pd_t *pd_downloader;
	gchar *pd_file_save_path;
	MMPlayerPDMode pd_mode;

	/* streaming player */
	mm_player_streaming_t *streamer;

	/* gstreamer pipeline */
	MMPlayerGstPipelineInfo	*pipeline;

	/* pad */
	GstPad *ghost_pad_for_videobin;

	guint64 media_stream_buffer_max_size[MM_PLAYER_STREAM_TYPE_MAX];
	guint media_stream_buffer_min_percent[MM_PLAYER_STREAM_TYPE_MAX];
	mm_player_media_stream_buffer_status_callback media_stream_buffer_status_cb[MM_PLAYER_STREAM_TYPE_MAX];
	mm_player_media_stream_seek_data_callback media_stream_seek_data_cb[MM_PLAYER_STREAM_TYPE_MAX];

	void* buffer_cb_user_param;

	/* video stream changed callback */
	mm_player_stream_changed_callback video_stream_changed_cb;
	void* video_stream_changed_cb_user_param;

	/* audio stream changed callback */
	mm_player_stream_changed_callback audio_stream_changed_cb;
	void* audio_stream_changed_cb_user_param;

	/* video stream callback */
	mm_player_video_stream_callback video_stream_cb;
	void* video_stream_cb_user_param;
	int use_video_stream;

	/* audio stram callback */
	mm_player_audio_stream_callback audio_stream_cb;
	void* audio_stream_cb_user_param;
	bool audio_stream_sink_sync;

	/* audio buffer callback */
	mm_player_audio_stream_callback_ex audio_stream_render_cb_ex;

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
	gulong video_cb_probe_id;

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
	GList* audio_decoders; // list of linked audio name
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

	gboolean smooth_streaming;

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
	gboolean msg_posted;

	/* list of sink elements */
	GList* sink_elements;

	/* signal notifiers */
	GList* signals[MM_PLAYER_SIGNAL_TYPE_MAX];
	guint bus_watcher;
	MMPlayerGMainContext context;
	MMPlayerUriList uri_info;

	gboolean is_sound_extraction;

	gfloat playback_rate;

	/* player state resumed by fast rewind */
	gboolean resumed_by_rewind;

	gboolean is_nv12_tiled;
	gboolean is_drm_file;

	/* resource manager for H/W resources */
	MMPlayerResourceManager resource_manager;

	/* sound focus for being compatible with legacy session policy internally */
	MMPlayerSoundFocus sound_focus;

	gboolean is_subtitle_off;
	gboolean is_external_subtitle_present;

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
	guint resume_event_id;
	guint resumable_cancel_id;

	gboolean keep_detecting_vcodec;

	gboolean play_subtitle;
	gboolean use_textoverlay;
	gboolean is_subtitle_force_drop;	// set TRUE after bus_cb get EOS

	/* PD downloader message callback and param */
	MMMessageCallback pd_msg_cb;
	void* pd_msg_cb_param;

	/* adjust subtitle position store */
	gint64 adjust_subtitle_pos;
	GList *subtitle_language_list;

	/* To store the current multiwindow status */
	gboolean last_multiwin_status;

	/* To store the current running audio pad index of demuxer */
	gint demux_pad_index;

	mm_player_selector_t selector[MM_PLAYER_TRACK_TYPE_MAX];
	mm_player_selector_t audio_mode;
	gboolean use_deinterleave;
	guint max_audio_channels;

	guint internal_text_idx;
	guint external_text_idx;

	MMPlayerSetMode set_mode;

	/* decodbin usage */
	gboolean use_decodebin;

	/* initialize values */
	mm_player_ini_t ini;

	/* check to use h/w codec */
	GstCaps* state_tune_caps;
	gboolean ignore_asyncdone;

	/* video share sync */
	gint64 video_share_api_delta;
	gint64 video_share_clock_delta;

	/* just for native app (video hub) */
	gboolean video_hub_download_mode;
	gboolean sync_handler;

	/* store dump pad list */
	GList* dump_list;

	/* whether a video has closed caption or not */
	gboolean has_closed_caption;

	GstElement *video_fakesink;

	/* audio stream caps parsed by demuxer or set by external demuxer */
	GstCaps* a_stream_caps;

	/* subtitle stream caps parsed by demuxer or set by external demuxer */
	GstCaps* s_stream_caps;

	/*es player using feed-data callback or calling app_src_push_buffer directly*/
	gboolean es_player_push_mode;

	/* tmb buffer manager for s/w codec tmb_bo */
	tbm_bufmgr bufmgr;

	int pcm_samplerate;
	int pcm_channel;
} mm_player_t;

typedef struct
{
	gchar *language_code;
	gchar *language_key;
	gboolean active;
}MMPlayerLangStruct;

typedef struct
{
	GstPad *dump_pad;
	gulong probe_handle_id;
	FILE *dump_element_file;
} mm_player_dump_t;

typedef struct{
	char *name;
	int value_type;
	int flags;				// r, w
	void *default_value;
	int valid_type;			// validity type
	int value_min;			//<- set validity value range
	int value_max;		//->
}MMPlayerAttrsSpec;

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
int _mmplayer_adjust_video_postion(MMHandleType hplayer,int offset);
int _mmplayer_activate_section_repeat(MMHandleType hplayer, unsigned long start, unsigned long end);
int _mmplayer_deactivate_section_repeat(MMHandleType hplayer);
int _mmplayer_push_buffer(MMHandleType hplayer, unsigned char *buf, int size);
int _mmplayer_set_playspeed(MMHandleType hplayer, float rate, bool streaming);
int _mmplayer_set_message_callback(MMHandleType hplayer, MMMessageCallback callback, void *user_param);
int _mmplayer_set_videostream_changed_cb(MMHandleType hplayer, mm_player_stream_changed_callback callback, void *user_param);
int _mmplayer_set_audiostream_changed_cb(MMHandleType hplayer, mm_player_stream_changed_callback callback, void *user_param);
int _mmplayer_set_videostream_cb(MMHandleType hplayer,mm_player_video_stream_callback callback, void *user_param);
int _mmplayer_set_audiostream_cb(MMHandleType hplayer,mm_player_audio_stream_callback callback, void *user_param);
int _mmplayer_set_subtitle_silent (MMHandleType hplayer, int silent);
int _mmplayer_get_subtitle_silent (MMHandleType hplayer, int* silent);
int _mmplayer_set_external_subtitle_path(MMHandleType hplayer, const char* filepath);
int _mmplayer_get_buffer_position(MMHandleType hplayer, int format, unsigned long* start_pos, unsigned long* stop_pos);

/* test API for tuning audio gain. this API should be
 * deprecated before the day of final release
 */
int _mmplayer_set_volume_tune(MMHandleType hplayer, MMPlayerVolumeType volume);
int _mmplayer_update_video_param(mm_player_t* player);
int _mmplayer_set_audiobuffer_cb(MMHandleType hplayer, mm_player_audio_stream_callback callback, void *user_param);
int _mmplayer_change_videosink(MMHandleType handle, MMDisplaySurfaceType surface_type, void *display_overlay);
int _mmplayer_audio_effect_custom_apply(mm_player_t *player);

int _mmplayer_set_audiostream_cb_ex(MMHandleType hplayer, bool sync, mm_player_audio_stream_callback_ex callback, void *user_param);
gboolean __mmplayer_post_message(mm_player_t* player, enum MMMessageType msgtype, MMMessageParamType* param);

int _mmplayer_gst_set_audio_channel(MMHandleType hplayer, MMPlayerAudioChannel ch_idx);
int _mmplayer_change_track_language (MMHandleType hplayer, MMPlayerTrackType type, int index);
int _mmplayer_sync_subtitle_pipeline(mm_player_t* player);
int _mmplayer_set_prepare_buffering_time(MMHandleType hplayer, int second);
int _mmplayer_set_runtime_buffering_mode(MMHandleType hplayer, MMPlayerBufferingMode mode, int second);
int _mmplayer_set_display_zoom(MMHandleType hplayer, float level, int x, int y);
int _mmplayer_get_display_zoom(MMHandleType hplayer, float *level, int *x, int *y);
int _mmplayer_set_video_hub_download_mode(MMHandleType hplayer, bool mode);
int _mmplayer_use_system_clock (MMHandleType hplayer);
int _mmplayer_set_video_share_master_clock(MMHandleType hplayer, long long clock, long long clock_delta, long long video_time, long long media_clock, long long audio_time);
int _mmplayer_get_video_share_master_clock(MMHandleType hplayer, long long *video_time, long long *media_clock, long long *audio_time);
int _mmplayer_get_video_rotate_angle(MMHandleType hplayer, int *angle);
int _mmplayer_enable_sync_handler(MMHandleType hplayer, bool enable);
int _mmplayer_set_uri(MMHandleType hplayer, const char* uri);
int _mmplayer_set_next_uri(MMHandleType hplayer, const char* uri, bool is_first_path);
int _mmplayer_get_next_uri(MMHandleType hplayer, char** uri);
int _mmplayer_has_closed_caption(MMHandleType hplayer, bool* exist);
int _mmplayer_enable_media_packet_video_stream(MMHandleType hplayer, bool enable);
void * _mm_player_media_packet_video_stream_internal_buffer_ref(void *buffer);
void _mm_player_media_packet_video_stream_internal_buffer_unref(void *buffer);
int _mmplayer_set_pcm_spec(MMHandleType hplayer, int samplerate, int channel);
int _mmplayer_get_timeout(MMHandleType hplayer, int *timeout);
int __mmplayer_gst_set_state (mm_player_t* player, GstElement * pipeline,  GstState state, gboolean async, gint timeout );
int __mmplayer_set_state(mm_player_t* player, int state);
int __mmplayer_check_state(mm_player_t* player, enum PlayerCommandState command);
gboolean __mmplayer_dump_pipeline_state( mm_player_t* player );
void __mmplayer_remove_g_source_from_context(GMainContext *context, guint source_id);
/* util */
const gchar * __get_state_name ( int state );
gboolean __mmplayer_can_do_interrupt(mm_player_t *player);
gboolean __is_streaming( mm_player_t* player );
gboolean __is_rtsp_streaming( mm_player_t* player );
gboolean __is_wfd_streaming( mm_player_t* player );
gboolean __is_live_streaming ( mm_player_t* player );
gboolean __is_http_streaming( mm_player_t* player );
gboolean __is_http_live_streaming( mm_player_t* player );
gboolean __is_dash_streaming( mm_player_t* player );
gboolean __is_smooth_streaming( mm_player_t* player );
gboolean __is_http_progressive_down(mm_player_t* player);

gboolean __mmplayer_check_useful_message(mm_player_t *player, GstMessage * message);
gboolean __mmplayer_handle_gst_error ( mm_player_t* player, GstMessage * message, GError* error );
gint __gst_handle_core_error( mm_player_t* player, int code );
gint __gst_handle_library_error( mm_player_t* player, int code );
gint __gst_handle_resource_error( mm_player_t* player, int code );
gint __gst_handle_stream_error( mm_player_t* player, GError* error, GstMessage * message );
int _mmplayer_sound_register_with_pid(MMHandleType hplayer, int pid);
int __mmplayer_get_video_angle(mm_player_t* player, int *user_angle, int *org_angle);
#ifdef __cplusplus
	}
#endif

#endif	/* __MM_PLAYER_PRIV_H__ */
