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

#ifndef __MM_PLAYER_H__
#define	__MM_PLAYER_H__


/*===========================================================================================
|                                                                                           |
|  INCLUDE FILES                                        |
|                                                                                           |
========================================================================================== */

#include <glib.h>

#include <mm_types.h>
#include <mm_message.h>
#include <media_packet.h>

#ifdef __cplusplus
	extern "C" {
#endif

/**
	@addtogroup PLAYER
	@{

	@par
	This part describes APIs used for playback of multimedia contents.
	All multimedia contents are created by a media player through handle of playback.
	In creating a player, it displays the player's status or information
	by registering callback function.

	@par
	In case of streaming playback, network has to be opend by using datanetwork API.
	If proxy, cookies and the other attributes for streaming playback are needed,
	set those attributes using mm_player_set_attribute() before create player.

	@par
	The subtitle for local video playback is supported. Set "subtitle_uri" attribute
	using mm_player_set_attribute() before the application creates the player.
	Then the application could receive MMMessageParamType which includes subtitle string and duration.

	@par
	Player can have 5 states, and each state can be changed by calling
	described functions on "Figure1. State of Player".

	@par
	@image html		player_state.jpg	"Figure1. State of Player"	width=12cm
	@image latex	player_state.jpg	"Figure1. State of Player"	width=12cm

	@par
	Most of functions which change player state work as synchronous. But, mm_player_start() should be used
	asynchronously. Both mm_player_pause() and mm_player_resume() should also be used asynchronously
	in the case of streaming data.
	So, application have to confirm the result of those APIs through message callback function.

	@par
	Note that "None" and Null" state could be reached from any state
	by calling mm_player_destroy() and mm_player_unrealize().

	@par
	<div><table>
	<tr>
	<td><B>FUNCTION</B></td>
	<td><B>PRE-STATE</B></td>
	<td><B>POST-STATE</B></td>
	<td><B>SYNC TYPE</B></td>
	</tr>
	<tr>
	<td>mm_player_create()</td>
	<td>NONE</td>
	<td>NULL</td>
	<td>SYNC</td>
	</tr>
	<tr>
	<td>mm_player_destroy()</td>
	<td>NULL</td>
	<td>NONE</td>
	<td>SYNC</td>
	</tr>
	<tr>
	<td>mm_player_realize()</td>
	<td>NULL</td>
	<td>READY</td>
	<td>SYNC</td>
	</tr>
	<tr>
	<td>mm_player_unrealize()</td>
	<td>READY</td>
	<td>NULL</td>
	<td>SYNC</td>
	</tr>
	<tr>
	<td>mm_player_start()</td>
	<td>READY</td>
	<td>PLAYING</td>
	<td>ASYNC</td>
	</tr>
	<tr>
	<td>mm_player_stop()</td>
	<td>PLAYING</td>
	<td>READY</td>
	<td>SYNC</td>
	</tr>
	<tr>
	<td>mm_player_pause()</td>
	<td>PLAYING</td>
	<td>PAUSED</td>
	<td>ASYNC</td>
	</tr>
	<tr>
	<td>mm_player_resume()</td>
	<td>PAUSED</td>
	<td>PLAYING</td>
	<td>ASYNC</td>
	</tr>
	<tr>
	<td>mm_player_set_message_callback()</td>
	<td>N/A</td>
	<td>N/A</td>
	<td>SYNC</td>
	</tr>
	<tr>
	<td>mm_player_get_state()</td>
	<td>N/A</td>
	<td>N/A</td>
	<td>SYNC</td>
	</tr>
	<tr>
	<td>mm_player_set_volume()</td>
	<td>N/A</td>
	<td>N/A</td>
	<td>SYNC</td>
	</tr>
	<tr>
	<td>mm_player_get_volume()</td>
	<td>N/A</td>
	<td>N/A</td>
	<td>SYNC</td>
	</tr>
	<tr>
	<td>mm_player_set_position()</td>
	<td>N/A</td>
	<td>N/A</td>
	<td>SYNC</td>
	</tr>
	<tr>
	<td>mm_player_get_position()</td>
	<td>N/A</td>
	<td>N/A</td>
	<td>SYNC</td>
	</tr>
	<tr>
	<td>mm_player_get_attribute()</td>
	<td>N/A</td>
	<td>N/A</td>
	<td>SYNC</td>
	</tr>
	<tr>
	<td>mm_player_set_attribute()</td>
	<td>N/A</td>
	<td>N/A</td>
	<td>SYNC</td>
	</tr>
	</table></div>

	@par
	Following are the attributes supported in player which may be set after initialization. \n
	Those are handled as a string.

	@par
	<div><table>
	<tr>
	<td>PROPERTY</td>
	<td>TYPE</td>
	<td>VALID TYPE</td>
	</tr>
	<tr>
	<td>"profile_uri"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"content_duration"</td>
	<td>int</td>
	<td>range</td>
	</tr>
	<tr>
	<td>"content_video_width"</td>
	<td>int</td>
	<td>range</td>
	</tr>
	<tr>
	<td>"content_video_height"</td>
	<td>int</td>
	<td>range</td>
	</tr>
	<tr>
	<td>"display_evas_do_scaling"</td>
	<td>int</td>
	<td>range</td>
	</tr>
	<tr>
	<td>"display_evas_surface_sink"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"profile_user_param"</td>
	<td>data</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"profile_play_count"</td>
	<td>int</td>
	<td>range</td>
	</tr>
	<tr>
	<td>"streaming_type"</td>
	<td>int</td>
	<td>range</td>
	</tr>
	<tr>
	<td>"streaming_udp_timeout"</td>
	<td>int</td>
	<td>range</td>
	</tr>
	<tr>
    	<td>"streaming_user_agent"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"streaming_wap_profile"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"streaming_network_bandwidth"</td>
	<td>int</td>
	<td>range</td>
	</tr>
	<tr>
	<td>"streaming_cookie"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"streaming_proxy_ip"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"streaming_proxy_port"</td>
	<td>int</td>
	<td>range</td>
	</tr>
	<tr>
	<td>"streaming_timeout"</td>
	<td>int</td>
	<td>range</td>
	</tr>
	<tr>
	<td>"display_overlay"</td>
	<td>data</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"display_rotation"</td>
	<td>int</td>
	<td>range</td>
	</tr>
	<tr>
	<td>"subtitle_uri"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	</table></div>

	@par
	Following attributes are supported for playing stream data. Those value can be readable only and valid after starting playback.\n
	Please use mm_fileinfo for local playback.

	@par
	<div><table>
	<tr>
	<td>PROPERTY</td>
	<td>TYPE</td>
	<td>VALID TYPE</td>
	</tr>
	<tr>
	<td>"content_video_found"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"content_video_codec"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"content_video_track_num"</td>
	<td>int</td>
	<td>range</td>
	</tr>
	<tr>
	<td>"content_audio_found"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"content_audio_codec"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"content_audio_bitrate"</td>
	<td>int</td>
	<td>array</td>
	</tr>
	<tr>
	<td>"content_audio_channels"</td>
	<td>int</td>
	<td>range</td>
	</tr>
	<tr>
	<td>"content_audio_samplerate"</td>
	<td>int</td>
	<td>array</td>
	</tr>
	<tr>
	<td>"content_audio_track_num"</td>
	<td>int</td>
	<td>range</td>
	</tr>
	<tr>
	<td>"content_text_track_num"</td>
	<td>int</td>
	<td>range</td>
	</tr>
	<tr>
	<td>"tag_artist"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"tag_title"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"tag_album"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"tag_genre"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"tag_author"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"tag_copyright"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"tag_date"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"tag_description"</td>
	<td>string</td>
	<td>N/A</td>
	</tr>
	<tr>
	<td>"tag_track_num"</td>
	<td>int</td>
	<td>range</td>
	</tr>
	</table></div>

 */


/*===========================================================================================
|                                                                                           |
|  GLOBAL DEFINITIONS AND DECLARATIONS                                        |
|                                                                                           |
========================================================================================== */

/**
 * MM_PLAYER_URI:
 *
 * uri to play (string)
 *
 */
#define MM_PLAYER_CONTENT_URI					"profile_uri"
/**
 * MM_PLAYER_CONTENT_DURATION:
 *
 * get the duration (int) as millisecond, It's guaranteed after calling mm_player_start() or
 * receiving MM_MESSAGE_BEGIN_OF_STREAM.
 *
 */
#define MM_PLAYER_CONTENT_DURATION			"content_duration"
/**
 * MM_PLAYER_VIDEO_ROTATION
 *
 * can change video angle (int)
 * @see MMDisplayRotationType
 */
#define MM_PLAYER_VIDEO_ROTATION				"display_rotation"
/**
 * MM_PLAYER_VIDEO_WIDTH:
 *
 * get the video width (int), It's guaranteed after calling mm_player_start() or
 * receiving MM_MESSAGE_BEGIN_OF_STREAM.
 *
 */
#define MM_PLAYER_VIDEO_WIDTH				"content_video_width"
/**
 * MM_PLAYER_VIDEO_HEIGHT:
 *
 * get the video height (int), It's guaranteed after calling mm_player_start() or
 * receiving MM_MESSAGE_BEGIN_OF_STREAM.
 *
 */
#define MM_PLAYER_VIDEO_HEIGHT				"content_video_height"
/**
 * MM_PLAYER_VIDEO_EVAS_SURFACE_DO_SCALING:
 *
 * set whether or not to scale frames size for evas surface.
 * if TRUE, it scales down width, height size of frames with given size.
 * if FALSE, it does not scale down any frames.
 *
 */
#define MM_PLAYER_VIDEO_EVAS_SURFACE_DO_SCALING		"display_evas_do_scaling"
/**
 * MM_PLAYER_VIDEO_EVAS_SURFACE_SINK:
 *
 * get the video evas surface sink plugin name (string), It's guaranteed after calling mm_player_create()
 *
 */
#define MM_PLAYER_VIDEO_EVAS_SURFACE_SINK		"display_evas_surface_sink"
/**
 * MM_PLAYER_MEM_SRC:
 *
 * set memory pointer to play (data)
 *
 */
#define MM_PLAYER_MEMORY_SRC					"profile_user_param"
/**
 * MM_PLAYER_PLAYBACK_COUNT
 *
 * can set playback count (int), Default value is 1 and -1 is for infinity playing until releasing it.
 *
 */
#define MM_PLAYER_PLAYBACK_COUNT				"profile_play_count"
/**
 * MM_PLAYER_SUBTITLE_URI
 *
 * set the subtitle path (string)
 */
#define MM_PLAYER_SUBTITLE_URI					"subtitle_uri"
/**
 * MM_PLAYER_STREAMING_TYPE
 *
 * set the streaming type (int)
 * @see MMStreamingType
 */
#define MM_PLAYER_STREAMING_TYPE				"streaming_type"
/**
 * MM_PLAYER_STREAMING_UDP_TIMEOUT
 *
 * set the streaming udp timeout(int)
 */
#define MM_PLAYER_STREAMING_UDP_TIMEOUT		"streaming_udp_timeout"
/**
 * MM_PLAYER_STREAMING_USER_AGENT
 *
 * set the streaming user agent (string)
 */
#define MM_PLAYER_STREAMING_USER_AGENT		"streaming_user_agent"
/**
 * MM_PLAYER_STREAMING_WAP_PROFILE
 *
 * set the streaming wap profile (int)
 */
#define MM_PLAYER_STREAMING_WAP_PROFILE		"streaming_wap_profile"
/**
 * MM_PLAYER_STREAMING_NET_BANDWIDTH
 *
 * set the streaming network bandwidth (int)
 */
#define MM_PLAYER_STREAMING_NET_BANDWIDTH	"streaming_network_bandwidth"
/**
 * MM_PLAYER_STREAMING_COOKIE
 *
 * set the streaming cookie (int)
 */
#define MM_PLAYER_STREAMING_COOKIE			"streaming_cookie"
/**
 * MM_PLAYER_STREAMING_PROXY_IP
 *
 * set the streaming proxy ip (string)
 */
#define MM_PLAYER_STREAMING_PROXY_IP			"streaming_proxy_ip"
/**
 * MM_PLAYER_STREAMING_PROXY_PORT
 *
 * set the streaming proxy port (int)
 */
#define MM_PLAYER_STREAMING_PROXY_PORT		"streaming_proxy_port"
/**
 * MM_PLAYER_STREAMING_TIMEOUT
 *
 * set the streaming timeout (int)
 */
#define MM_PLAYER_STREAMING_TIMEOUT			"streaming_timeout"
/**
 * MM_PLAYER_VIDEO_CODEC
 *
 * codec the video data is stored in (string)
 */
#define MM_PLAYER_VIDEO_CODEC				"content_video_codec"
/**
 * MM_PLAYER_VIDEO_TRACK_NUM
 *
 * track number inside a collection  (int)
 */
#define MM_PLAYER_VIDEO_TRACK_NUM			"content_video_track_num"
/**
 * MM_PLAYER_AUDIO_CODEC
 *
 * codec the audio data is stored in (string)
 */
#define MM_PLAYER_AUDIO_CODEC				"content_audio_codec"
/**
 * MM_PLAYER_AUDIO_BITRATE
 *
 * set the streaming proxy port (int)
 */
#define MM_PLAYER_AUDIO_BITRATE				"content_audio_bitrate"
/**
 * MM_PLAYER_AUDIO_CHANNEL
 *
 * the number of audio channel (int)
 */
#define MM_PLAYER_AUDIO_CHANNEL				"content_audio_channels"
/**
 * MM_PLAYER_AUDIO_SAMPLERATE
 *
 * audio samplerate  (int)
 */
#define MM_PLAYER_AUDIO_SAMPLERATE			"content_audio_samplerate"
/**
 * MM_PLAYER_AUDIO_TRACK_NUM
 *
 * track number inside a collection (int)
 */
#define MM_PLAYER_AUDIO_TRACK_NUM			"content_audio_track_num"
/**
 * MM_PLAYER_TEXT_TRACK_NUM
 *
 * track number inside a collection (int)
 */
#define MM_PLAYER_TEXT_TRACK_NUM			"content_text_track_num"
/**
 * MM_PLAYER_TAG_ARTIST
 *
 * person(s) responsible for the recording (string)
 */
#define MM_PLAYER_TAG_ARTIST					"tag_artist"
/**
 * MM_PLAYER_TAG_ARTIST
 *
 * title (string)
 */
#define MM_PLAYER_TAG_TITLE					"tag_title"
/**
 * MM_PLAYER_TAG_ARTIST
 *
 * album containing this data (string)
 */
#define MM_PLAYER_TAG_ALBUM					"tag_album"
/**
 * MM_PLAYER_TAG_ARTIST
 *
 * genre this data belongs to (string)
 */
#define MM_PLAYER_TAG_GENRE					"tag_genre"
/**
 * MM_PLAYER_TAG_ARTIST
 *
 * author (string)
 */
#define MM_PLAYER_TAG_AUTHOUR				"tag_author"
/**
 * MM_PLAYER_TAG_ARTIST
 *
 * copyright notice of the data (string)
 */
#define MM_PLAYER_TAG_COPYRIGHT				"tag_copyright"
/**
 * MM_PLAYER_TAG_ARTIST
 *
 * date the data was created (string)
 */
#define MM_PLAYER_TAG_DATE					"tag_date"
/**
 * MM_PLAYER_TAG_ARTIST
 *
 * short text describing the content of the data (string)
 */
#define MM_PLAYER_TAG_DESCRIPRION				"tag_description"
/**
 * MM_PLAYER_TAG_ARTIST
 *
 * track number inside a collection (int)
 */
#define MM_PLAYER_TAG_TRACK_NUM				"tag_track_num"
/**
 * MM_PLAYER_PD_MODE
 *
 * progressive download mode (int)
 */
#define MM_PLAYER_PD_MODE						"pd_mode"

#define BUFFER_MAX_PLANE_NUM (4)

typedef struct {
	MMPixelFormatType format;       		/**< image format */
	int width;                      			/**< width of video buffer */
	int height;                     			/**< height of video buffer */
	unsigned int timestamp;         		/**< timestamp of stream buffer (msec)*/
	unsigned int length_total; 			/**< total length of stream buffer (in byte)*/
	void *data[BUFFER_MAX_PLANE_NUM];
	void *bo[BUFFER_MAX_PLANE_NUM];  /**< TBM buffer object */
	void *internal_buffer;          		/**< Internal buffer pointer */
	int stride[BUFFER_MAX_PLANE_NUM];		/**< stride of plane */
	int elevation[BUFFER_MAX_PLANE_NUM];  	/**< elevation of plane */
}MMPlayerVideoStreamDataType;

/**
 * Enumerations of player state.
 */
typedef enum {
	MM_PLAYER_STATE_NULL,				/**< Player is created, but not realized yet */
	MM_PLAYER_STATE_READY,				/**< Player is ready to play media */
	MM_PLAYER_STATE_PLAYING,			/**< Player is now playing media */
	MM_PLAYER_STATE_PAUSED,				/**< Player is paused while playing media */
	MM_PLAYER_STATE_NONE,				/**< Player is not created yet */
	MM_PLAYER_STATE_NUM,				/**< Number of player states */
} MMPlayerStateType;

/**
 * Enumerations of position formats.
 * Used while invoking mm_player_get_position/mm_player_set_position APIs
 */
typedef enum {
	MM_PLAYER_POS_FORMAT_TIME,			/**< Format for time based */
	MM_PLAYER_POS_FORMAT_PERCENT,			/**< Format for percentage */
	MM_PLAYER_POS_FORMAT_NUM,			/**< Number of position formats */
} MMPlayerPosFormatType;

/**
 * Enumeration for attribute values types.
 */
typedef enum {
 MM_PLAYER_ATTRS_TYPE_INVALID = -1,        /**< Type is invalid */
 MM_PLAYER_ATTRS_TYPE_INT,                 /**< Integer type */
 MM_PLAYER_ATTRS_TYPE_DOUBLE,              /**< Double type */
 MM_PLAYER_ATTRS_TYPE_STRING,              /**< UTF-8 String type */
 MM_PLAYER_ATTRS_TYPE_DATA,                /**< Pointer type */
 MM_PLAYER_ATTRS_TYPE_ARRAY,               /**< Array type */
 MM_PLAYER_ATTRS_TYPE_RANGE,               /**< Range type */
 MM_PLAYER_ATTRS_TYPE_NUM,                 /**< Number of attribute type */
} MMPlayerAttrsType;

/**
 * Enumeration for attribute validation type.
 */
typedef enum {
 MM_PLAYER_ATTRS_VALID_TYPE_INVALID = -1,		/**< Invalid validation type */
 MM_PLAYER_ATTRS_VALID_TYPE_NONE,				/**< Do not check validity */
 MM_PLAYER_ATTRS_VALID_TYPE_INT_ARRAY,          /**< validity checking type of integer array */
 MM_PLAYER_ATTRS_VALID_TYPE_INT_RANGE,          /**< validity checking type of integer range */
 MM_PLAYER_ATTRS_VALID_TYPE_DOUBLE_ARRAY,		/**< validity checking type of double array */
 MM_PLAYER_ATTRS_VALID_TYPE_DOUBLE_RANGE,       /**< validity checking type of double range */
} MMPlayerAttrsValidType;

/**
 * Enumeration for attribute access flag.
 */
typedef enum {
 MM_PLAYER_ATTRS_FLAG_NONE = 0,					/**< None flag is set */
 MM_PLAYER_ATTRS_FLAG_READABLE = 1 << 0,			/**< Readable */
 MM_PLAYER_ATTRS_FLAG_WRITABLE = 1 << 1,			/**< Writable */
 MM_PLAYER_ATTRS_FLAG_MODIFIED = 1 << 2,			/**< Modified */

 MM_PLAYER_ATTRS_FLAG_RW = MM_PLAYER_ATTRS_FLAG_READABLE | MM_PLAYER_ATTRS_FLAG_WRITABLE, /**< Readable and Writable */
} MMPlayerAttrsFlag;

/**
 * Enumeration for progressive download
 */
typedef enum {
        MM_PLAYER_PD_MODE_NONE,
        MM_PLAYER_PD_MODE_URI,
        MM_PLAYER_PD_MODE_FILE	// not tested yet, because of no fixed scenario
}MMPlayerPDMode;

/**
 * Enumeration of track types
 */
typedef enum {
	MM_PLAYER_TRACK_TYPE_AUDIO = 0,
	MM_PLAYER_TRACK_TYPE_VIDEO,
	MM_PLAYER_TRACK_TYPE_TEXT,
	MM_PLAYER_TRACK_TYPE_MAX
}MMPlayerTrackType;

/**
 * Enumeration of runtime buffering mode
 */
typedef enum {
	MM_PLAYER_BUFFERING_MODE_ADAPTIVE = 0,	/**< default, If buffering is occurred, player will consider the bandwidth to adjust buffer setting. */
	MM_PLAYER_BUFFERING_MODE_FIXED,			/**< player will set buffer size with this fixed size value. */
	MM_PLAYER_BUFFERING_MODE_SLINK,			/**< If buffering is occurred, player will adjust buffer setting and no more buffering will be occurred again. */
	MM_PLAYER_BUFFERING_MODE_MAX = MM_PLAYER_BUFFERING_MODE_SLINK,
}MMPlayerBufferingMode;

/**
 * Enumeration of audio channel for video share
 */
typedef enum
{
	MM_PLAYER_AUDIO_CH_MONO_LEFT = 0,
	MM_PLAYER_AUDIO_CH_MONO_RIGHT,
	MM_PLAYER_AUDIO_CH_STEREO,
} MMPlayerAudioChannel;

typedef enum
{
	MM_PLAYER_SOUND_RESOURCE_PRELISTENING_RINGTONE = 0,
	MM_PLAYER_SOUND_RESOURCE_PRELISTENING_NOTIFICATION,
	MM_PLAYER_SOUND_RESOURCE_PRELISTENING_ALARM,
	MM_PLAYER_SOUND_RESOURCE_PRELISTENING_MEDIA,
} MMPlayerSoundResource;


/**
 * Edge Properties of the text.
 */
typedef enum {
	MM_PLAYER_EDGE_NO,
	MM_PLAYER_EDGE_RAISED,
	MM_PLAYER_EDGE_DEPRESSED,
	MM_PLAYER_EDGE_UNIFORM,
	MM_PLAYER_EDGE_DROPSHADOW
} MMPlayerSubtitleEdge;

/**
 * Enumeration of media stream buffer status
 */
typedef enum
{
	MM_PLAYER_MEDIA_STREAM_BUFFER_UNDERRUN,
	MM_PLAYER_MEDIA_STREAM_BUFFER_OVERFLOW,
} MMPlayerMediaStreamBufferStatus;

/**
 * Enumeration for stream type.
 */
typedef enum
{
	MM_PLAYER_STREAM_TYPE_DEFAULT,	/**< Container type */
	MM_PLAYER_STREAM_TYPE_AUDIO,	/**< Audio element stream type */
	MM_PLAYER_STREAM_TYPE_VIDEO,	/**< Video element stream type */
	MM_PLAYER_STREAM_TYPE_TEXT,     /**< Text type */
	MM_PLAYER_STREAM_TYPE_MAX,
} MMPlayerStreamType;

/**
 * Attribute validity structure
 */
typedef struct {
	 MMPlayerAttrsType type;
	 MMPlayerAttrsValidType validity_type;
	 MMPlayerAttrsFlag flag;
	/**
	  * a union that describes validity of the attribute.
	  * Only when type is 'MM_ATTRS_TYPE_INT' or 'MM_ATTRS_TYPE_DOUBLE',
	  * the attribute can have validity.
	 */
	 union {
		/**
		   * Validity structure for integer array.
		 */
		struct {
			int *array;  /**< a pointer of array */
			int count;   /**< size of array */
			int d_val;
		} int_array;
		/**
		   * Validity structure for integer range.
		 */
		struct {
			int min;   /**< minimum range */
			int max;   /**< maximum range */
			int d_val;
		} int_range;
		/**
		* Validity structure for double array.
		*/
		struct {
			double   * array;  /**< a pointer of array */
			int    count;   /**< size of array */
			double d_val;
		} double_array;
		/**
		* Validity structure for double range.
		*/
		struct {
			double   min;   /**< minimum range */
			double   max;   /**< maximum range */
			double d_val;
		} double_range;
	};
} MMPlayerAttrsInfo;

/**
 * Volume type.
 *
 * @see		mm_player_set_volume, mm_player_get_volume
 */
typedef struct {
	float	level[MM_VOLUME_CHANNEL_NUM];	/**< Relative volume factor for each channels */
} MMPlayerVolumeType;

#ifdef TEST_ES
/**
 * Video stream info in external demux case
 *
**/
typedef struct _VideoStreamInfo
{
	const char *mime;
	unsigned int framerate_num;
	unsigned int framerate_den;
	unsigned int width;
	unsigned int height;
	unsigned char *codec_extradata;
	unsigned int extradata_size;
	unsigned int version;
}MMPlayerVideoStreamInfo;

/**
 * Audio stream info in external demux case
 *
**/
typedef struct _AudioStreamInfo
{
	const char *mime;
	unsigned int channels;
	unsigned int sample_rate;
	unsigned char *codec_extradata;
	unsigned int extradata_size;
	unsigned int version;
	unsigned int user_info;

	/* for pcm */
//	unsigned int width;
//	unsigned int depth;
//	unsigned int endianness;
//	bool signedness;
}MMPlayerAudioStreamInfo;

/**
 * Subtitle stream info in external demux case
 *
**/
typedef struct _SubtitleStreamInfo
{
	const char *mime;
	unsigned int codec_tag;
	void *context;  //for smpte text
}MMPlayerSubtitleStreamInfo;

#endif

/**
 * Audio stream callback function type.
 *
 * @param	stream		[in]	Reference pointer to audio frame data
 * @param	stream_size	[in]	Size of audio frame data
 * @param	user_param	[in]	User defined parameter which is passed when set
 *								audio stream callback
 *
 * @return	This callback function have to return MM_ERROR_NONE.
 */
typedef bool	(*mm_player_audio_stream_callback) (void *stream, int stream_size, void *user_param);


/**
 * selected subtitle track number callback function type.
 *
 * @param	track_num	[in]	Track number of subtitle
 * @param	user_param	[in]	User defined parameter
 *
 *
 * @return	This callback function have to return MM_ERROR_NONE.
 */
typedef bool		(*mm_player_track_selected_subtitle_language_callback)(int track_num, void *user_param);

/**
 * Buffer underrun / overflow data callback function type.
 *
 * @param	status     [in] buffer status
 * @param	user_param [in] User defined parameter which is passed when set
 *       	                to enough data callback or need data callback
 *
 * @return	This callback function have to return MM_ERROR_NONE.
 */
typedef bool	(*mm_player_media_stream_buffer_status_callback) (MMPlayerStreamType type, MMPlayerMediaStreamBufferStatus status, void *user_param);

/**
 * Buffer seek data callback function type.
 *
 * @param	offset     [in] offset for the buffer playback
 * @param	user_param [in] User defined parameter which is passed when set
 *       	                to seek data callback
 *
 * @return	This callback function have to return MM_ERROR_NONE.
 */
typedef bool	(*mm_player_media_stream_seek_data_callback) (MMPlayerStreamType type, unsigned long long offset, void *user_param);

/**
 * Called to notify the stream changed.
 *
 * @param user_data [in] The user data passed from the callback registration function
 *
 * @return	This callback function have to return MM_ERROR_NONE.
 */
typedef bool	(*mm_player_stream_changed_callback) (void *user_param);


/*===========================================================================================
|                                                                                           |
|  GLOBAL FUNCTION PROTOTYPES                                        |
|                                                                                           |
========================================================================================== */

/**
 * This function creates a player object for playing multimedia contents. \n
 * The attributes of player are created to get/set some values with application. \n
 * And, mutex, gstreamer and other resources are initialized at this time. \n
 * If player is created, the state will become MM_PLAYER_STATE_NULL.
 *
 * @param	player		[out]	Handle of player
 *
 * @return	This function returns zero on success, or negative value with error code. \n
 *			Please refer 'mm_error.h' to know it in detail.
 * @pre		None
 * @post 	MM_PLAYER_STATE_NULL
 * @see		mm_player_destroy
 * @remark	You can create multiple handles on a context at the same time. \n
 *			However, player cannot guarantee proper operation because of limitation of resources, \n
 * 			such as audio device or display device.
 *
 * @par Example
 * @code
char *g_err_attr_name = NULL;

if (mm_player_create(&g_player) != MM_ERROR_NONE)
{
	debug_error("failed to create player\n");
}

if (mm_player_set_attribute(g_player,
						&g_err_attr_name,
						"profile_uri", filename, strlen(filename),
						"display_overlay", (void*)&g_win.xid, sizeof(g_win.xid),
						NULL) != MM_ERROR_NONE)
{
	debug_error("failed to set %s attribute\n", g_err_attr_name);
	free(g_err_attr_name);
}

mm_player_set_message_callback(g_player, msg_callback, (void*)g_player);
 * @endcode
 */
int mm_player_create(MMHandleType *player);

/**
 * This function releases player object and all resources which were created by mm_player_create(). \n
 * And, player handle will also be destroyed.
 *
 * @param	player		[in]	Handle of player
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @pre		Player state may be MM_PLAYER_STATE_NULL. \n
 * 			But, it can be called in any state.
 * @post		Because handle is released, there is no any state.
 * @see		mm_player_create
 * @remark	This method can be called with a valid player handle from any state to \n
 *			completely shutdown the player operation.
 *
 * @par Example
 * @code
if (mm_player_destroy(g_player) != MM_ERROR_NONE)
{
	debug_error("failed to destroy player\n");
}
 * @endcode
 */
int mm_player_destroy(MMHandleType player);

/**
 * This function parses uri and makes gstreamer pipeline by uri scheme. \n
 * So, uri should be set before realizing with mm_player_set_attribute(). \n
 *
 * @param	player		[in]	Handle of player
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @pre		Player state should be MM_PLAYER_STATE_NULL.
 * @post		Player state will be MM_PLAYER_STATE_READY.
 * @see		mm_player_unrealize
 * @remark 	None
 * @par Example
 * @code
if (mm_player_realize(g_player) != MM_ERROR_NONE)
{
	debug_error("failed to realize player\n");
}
 * @endcode
 */
int mm_player_realize(MMHandleType player) ;

/**
 * This function uninitializes player object. So, resources and allocated memory \n
 * will be freed. And, gstreamer pipeline is also destroyed. So, if you want to play \n
 * other contents, player should be created again after destruction or realized with new uri.
 *
 * @param	player		[in]	Handle of player
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @pre		Player state may be MM_PLAYER_STATE_READY to unrealize. \n
 * 			But, it can be called in any state.
 * @post		Player state will be MM_PLAYER_STATE_NULL.
 * @see		mm_player_realize
 * @remark	This method can be called with a valid player handle from any state.
 *
 * @par Example
 * @code
if (mm_player_unrealize(g_player) != MM_ERROR_NONE)
{
	debug_error("failed to unrealize player\n");
}
 * @endcode
 */
int mm_player_unrealize(MMHandleType player);

/**
 * This function is to get current state of player. \n
 * Application have to check current state before doing some action.
 *
 * @param	player		[in]	Handle of player
 * @param	state       [out] current state of player on success
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @see		MMPlayerStateType
 * @remark 	None
 * @par Example
 * @code
if (mm_player_get_state(g_player, &state) != MM_ERROR_NONE)
{
	debug_error("failed to get state\n");
}
 * @endcode
 */
int mm_player_get_state(MMHandleType player, MMPlayerStateType *state);

/**
 * This function is to set relative volume of player. \n
 * So, It controls logical volume value. \n
 * But, if developer want to change system volume, mm sound api should be used.
 *
 * @param	player		[in]	Handle of player
 * @param	volume		[in]	Volume factor of each channel
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @see		MMPlayerVolumeType, mm_player_get_volume
 * @remark	The range of factor range is from 0 to 1.0. (1.0 = 100%) And, default value is 1.0.
 * @par Example
 * @code
MMPlayerVolumeType volume;
int i = 0;

for (i = 0; i < MM_VOLUME_CHANNEL_NUM; i++)
	volume.level[i] = MM_VOLUME_LEVEL_MAX;

if (mm_player_set_volume(g_player, &volume) != MM_ERROR_NONE)
{
    debug_error("failed to set volume\n");
}
 * @endcode
 */
int mm_player_set_volume(MMHandleType player, MMPlayerVolumeType *volume);

/**
 * This function is to get current volume factor of player.
 *
 * @param	player		[in]	Handle of player.
 * @param	volume		[out]	Volume factor of each channel.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @see		MMPlayerVolumeType, mm_player_set_volume
 * @remark 	None
 * @par Example
 * @code
MMPlayerVolumeType volume;
int i;

if (mm_player_get_volume(g_player, &volume) != MM_ERROR_NONE)
{
        debug_warning("failed to get volume\n");
}

for (i = 0; i < MM_VOLUME_CHANNEL_NUM; i++)
	debug_log("channel[%d] = %d \n", i, volume.level[i]);
 * @endcode
 */
int mm_player_get_volume(MMHandleType player, MMPlayerVolumeType *volume);

/**
 * This function is to start playing media contents. Demux(parser), codec and related plugins are decided \n
 * at this time. And, MM_MESSAGE_BEGIN_OF_STREAM will be posted through callback function registered \n
 * by mm_player_set_message_callback().
 *
 * @param	player		[in]	Handle of player
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @remark
 *
 * @pre		Player state may be MM_PLAYER_STATE_READY.
 * @post		Player state will be MM_PLAYER_STATE_PLAYING.
 * @see		mm_player_stop
 * @remark 	None
 * @par Example
 * @code
if (mm_player_start(g_player) != MM_ERROR_NONE)
{
	debug_error("failed to start player\n");
}
 * @endcode
 */
int mm_player_start(MMHandleType player);

/**
 * This function is to stop playing media contents and it's different with pause. \n
 * If mm_player_start() is called after this, content will be started again from the beginning. \n
 * So, it can be used to close current playback.
 *
 * @param	player		[in]	Handle of player
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @pre		Player state may be MM_PLAYER_STATE_PLAYING.
 * @post		Player state will be MM_PLAYER_STATE_READY.
 * @see		mm_player_start
 * @remark 	None
 * @par Example
 * @code
if (mm_player_stop(g_player) != MM_ERROR_NONE)
{
	debug_error("failed to stop player\n");
}
 * @endcode
 */
int mm_player_stop(MMHandleType player);

/**
 * This function is to pause playing media contents.
 *
 * @param	player		[in]	Handle of player.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @pre		Player state may be MM_PLAYER_STATE_PLAYING.
 * @post		Player state will be MM_PLAYER_STATE_PAUSED.
 * @see		mm_player_resume
 * @remark 	None
 * @par Example
 * @code
if (mm_player_pause(g_player) != MM_ERROR_NONE)
{
	debug_error("failed to pause player\n");
}
 * @endcode
 */
int mm_player_pause(MMHandleType player);

/**
 * This function is to resume paused media contents.
 *
 * @param	player		[in]	Handle of player.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @pre		Player state may be MM_PLAYER_STATE_PAUSED.
 * @post		Player state will be MM_PLAYER_STATE_PLAYING.
 * @see		mm_player_pause
 * @remark 	None
 * @par Example
 * @code
if (mm_player_resume(g_player) != MM_ERROR_NONE)
{
	debug_error("failed to resume player\n");
}
 * @endcode
 */
int mm_player_resume(MMHandleType player);

/**
 * This function is to set the position for playback. \n
 * So, it can be seeked to requested position. \n
 *
 * @param	player		[in]	Handle of player
 * @param	format		[in]	Format of position.
 * @param	pos			[in]	Position for playback
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @see		MMPlayerPosFormatType, mm_player_get_position
 * @remark  the unit of time-based format is millisecond and other case is percent.
 * @par Example
 * @code
int position = 1000; //1sec

if (mm_player_set_position(g_player, MM_PLAYER_POS_FORMAT_TIME, position) != MM_ERROR_NONE)
{
	debug_error("failed to set position\n");
}
 * @endcode
 */
int mm_player_set_position(MMHandleType player, MMPlayerPosFormatType format, int pos);

/**
 * This function is to get current position of playback content.
 *
 * @param	player		[in]	Handle of player.
 * @param	format		[in]	Format of position.
 * @param    pos        [out] contains current position on success or zero in case of failure.
 *
 * @return	This function returns zero on success, or negative value with errors
 * @see		MMPlayerPosFormatType, mm_player_set_position
 * @remark	the unit of time-based format is millisecond and other case is percent.
 * @par Example
 * @code
int position = 0;
int duration = 0;

mm_player_get_position(g_player, MM_PLAYER_POS_FORMAT_TIME, &position);

mm_player_get_attribute(g_player, &g_err_name, "content_duration", &duration, NULL);

debug_log("pos: [%d/%d] msec\n", position, duration);
 * @endcode
 */
int mm_player_get_position(MMHandleType player, MMPlayerPosFormatType format, int *pos);

/**
 * This function is to get current buffer position of playback content.
 *
 * @param	player		[in]	Handle of player.
 * @param	format		[in]	Format of position.
 * @param    	start_pos       	[out] contains buffer start  position on success or zero in case of failure.
 * @param   	stop_pos        [out] contains buffer current  position on success or zero in case of failure.
 *
 * @return	This function returns zero on success, or negative value with errors
 * @see		MMPlayerPosFormatType, mm_player_set_position
 * @remark	the unit of time-based format is millisecond and other case is percent.
 * @par Example
 * @code
int start_pos = 0, stop_pos = 0;

mm_player_get_buffer_position(g_player, MM_PLAYER_POS_FORMAT_PERCENT, &start_pos, &stop_pos );

debug_log("buffer position: [%d] ~ [%d] \%\n", start_pos, stop_pos );
 * @endcode
 */
int mm_player_get_buffer_position(MMHandleType player, MMPlayerPosFormatType format, int *start_pos, int *stop_pos);

/**
 * This function is to activate the section repeat. If it's set, selected section will be played \n
 * continually before deactivating it by mm_player_deactivate_section_repeat(). \n
 * The unit for setting is millisecond.
 *
 * @param	player		[in]	Handle of player.
 * @param	start_pos		[in]	start position.
 * @param	end_pos			[in]	end position.
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @see		mm_player_deactivate_section_repeat
 * @remark	None
 * @par Example
 * @code
int position;
int endtime = 4000; //msec

mm_player_get_position(g_player, MM_PLAYER_POS_FORMAT_TIME, &position);

mm_player_activate_section_repeat(g_player, position, position+endtime);
 * @endcode
 */
int mm_player_activate_section_repeat(MMHandleType player, int start_pos, int end_pos);

/**
 * This function is to deactivate the section repeat.
 *
 * @param	player		[in]	Handle of player.
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @see		mm_player_activate_section_repeat
 * @remark	None
 * @par Example
 * @code
if ( mm_player_deactivate_section_repeat(g_player) != MM_ERROR_NONE)
{
	debug_warning("failed to deactivate section repeat\n");
}
 * @endcode
 */
int mm_player_deactivate_section_repeat(MMHandleType player);

/**
 * This function sets callback function for receiving messages from player.
 * So, player can notify warning, error and normal cases to application.
 *
 * @param	player		[in]	Handle of player.
 * @param	callback	[in]	Message callback function.
 * @param	user_param	[in]	User parameter which is passed to callback function.
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @see		MMMessageCallback
 * @remark	None
 * @par Example
 * @code
int msg_callback(int message, MMMessageParamType *param, void *user_param)
{
	switch (message)
	{
		case MM_MESSAGE_ERROR:
			//do something
			break;

	 	case MM_MESSAGE_END_OF_STREAM:
	 		//do something
	    	  	break;

		case MM_MESSAGE_STATE_CHANGED:
			//do something
	    		break;

		case MM_MESSAGE_BEGIN_OF_STREAM:
			//do something
	    		break;

		default:
			break;
	}
	return TRUE;
}

mm_player_set_message_callback(g_player, msg_callback, (void*)g_player);
 * @endcode
 */
int mm_player_set_message_callback(MMHandleType player, MMMessageCallback callback, void *user_param);

/**
 * This function set callback function for receiving audio stream from player. \n
 * So, application can get raw audio data and modify it. \n
 * But, if callback don't return or holds it for long time, performance can be deteriorated. \n
 * It's only supported when audio stream is included in file. \n
 * So, if there is video stream or DRM content, it can't be used.
 *
 * @param	player		[in]	Handle of player.
 * @param	callback		[in]	Audio stream callback function.
 * @param	user_param	[in]	User parameter.
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 * @see		mm_player_audio_stream_callback
 * @remark	It can be used for audio playback only.
 * @par Example
 * @code
bool audio_callback(void *stream, int stream_size, void *user_param)
{
	debug_log("audio stream callback\n");
	return TRUE;
}
mm_player_set_audio_stream_callback(g_player, audio_callback, NULL);
 * @endcode
 */
 int mm_player_set_audio_stream_callback(MMHandleType player, mm_player_audio_stream_callback callback, void *user_param);

/**
 * This function is to mute volume of player
 *
 * @param	player	[in]	Handle of player
 * @param	mute	[in]	Mute(1) or not mute(0)
 *
 * @return	This function returns zero on success, or negative value with error code
 * @see		mm_player_get_mute
 * @remark	None
 * @par Example
 * @code
if (mm_player_set_mute(g_player, TRUE) != MM_ERROR_NONE)
{
	debug_warning("failed to set mute\n");
}
 * @endcode
 */
int mm_player_set_mute(MMHandleType player, int mute);

/**
 * This function is to get mute value of player
 *
 * @param	player	[in]	Handle of player
 * @param	mute	[out]	Sound is muted
 *
 * @return	This function returns zero on success, or negative value with error code
 * @see		mm_player_set_mute
 * @remark	None
 * @par Example
 * @code
int mute;

if (mm_player_get_mute(g_player, &mute) != MM_ERROR_NONE)
{
	debug_warning("failed to get mute\n");
}

debug_log("mute status:%d\n", mute);
 * @endcode
 */
int mm_player_get_mute(MMHandleType player, int *mute);

/**
 * This function is to adjust subtitle postion. So, subtitle can show at the adjusted position. \n
 * If pos is negative, subtitle will be displayed previous time, the other hand forward time. \n
 *
 * @param	player	[in]	Handle of player
 * @param	pos		[in]	postion to be adjusted
 *
 * @return	This function returns zero on success, or negative value with error
 *			code
 * @see		mm_player_adjust_subtitle_position
 * @remark	None
 * @par Example
 * @code
int pos;

pos = 5000;
if (mm_player_adjust_subtitle_position(g_player, MM_PLAYER_POS_FORMAT_TIME, pos) != MM_ERROR_NONE)
{
	debug_warning("failed to adjust subtitle postion.\n");
}
 * @endcode
 */

int mm_player_adjust_subtitle_position(MMHandleType player, MMPlayerPosFormatType format, int pos);

/**
 * This function is to set the offset in timestamps of video so as to bring the a/v sync
 * @param      player          Handle of player
 * @param      offset          offset to be set in milliseconds(can be positive or negative both)
 * postive offset to make video lag
 * negative offset to make video lead
 */
int mm_player_adjust_video_position(MMHandleType player,int offset);
/**
 * This function is to set subtitle silent status. So, subtitle can show or hide during playback \n
 * by this value. But, one subtitle file should be set with "subtitle_uri" attribute before calling mm_player_realize(); \n
 * Player FW parses subtitle file and send text data including timestamp to application \n
 * through message callback with MM_MESSAGE_UPDATE_SUBTITLE will be. \n
 * So, application have to render it. And, subtitle can be supported only in a seprate file. \n
 * So, it's not supported for embedded case.
 *
 * @param	player	[in]	Handle of player
 * @param	silent	[in]	silent(integer value except 0) or not silent(0)
 *
 * @return	This function returns zero on success, or negative value with error
 *			code
 * @see		mm_player_get_subtitle_silent, MM_MESSAGE_UPDATE_SUBTITLE
 * @remark	None
 * @par Example
 * @code
mm_player_set_attribute(g_player,
					&g_err_name,
					"subtitle_uri", g_subtitle_uri, strlen(g_subtitle_uri),
					NULL
					);

if (mm_player_set_subtitle_silent(g_player, TRUE) != MM_ERROR_NONE)
{
	debug_warning("failed to set subtitle silent\n");
}
 * @endcode
 */
int mm_player_set_subtitle_silent(MMHandleType player, int silent);

/**
 * This function is to get silent status of subtitle.
 *
 * @param	player	[in]	Handle of player
 * @param	silent	[out]	subtitle silent property
 *
 * @return	This function returns zero on success, or negative value with error
 *			code
 * @see		mm_player_set_subtitle_silent, MM_MESSAGE_UPDATE_SUBTITLE
 * @remark	None
 * @par Example
 * @code
int silent = FALSE;

if (mm_player_get_subtitle_silent(g_player, &silent) != MM_ERROR_NONE)
{
	debug_warning("failed to set subtitle silent\n");
}
 * @endcode
 */
int mm_player_get_subtitle_silent(MMHandleType player, int *silent);

/**
 * This function is to set attributes into player. Multiple attributes can be set simultaneously. \n
 * If one of attribute fails, this function will stop at the point and let you know the name which is failed. \n
 *
 * @param	player				[in]	Handle of player.
 * @param   	err_attr_name			[out]  Name of attribute which is failed to set
 * @param   	first_attribute_name 	[in] 	Name of the first attribute to set
 * @param   ...					[in] 	Value for the first attribute, followed optionally by more name/value pairs, terminated by NULL.
 *									 But, in the case of data or string type, it should be name/value/size.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @see		mm_player_get_attribute
 * @remark  	This function must be terminated by NULL argument.
 * 			And, if this function is failed, err_attr_name param must be free.
 * @par Example
 * @code
char *g_err_attr_name = NULL;

if (mm_player_set_attribute(g_player,
						&g_err_attr_name,
						"profile_uri", filename, strlen(filename),
						"profile_play_count", count,
						NULL) != MM_ERROR_NONE)
{
	debug_warning("failed to set %s attribute\n", g_err_attr_name);
	free(g_err_attr_name);
}

 * @endcode
 */
int mm_player_set_attribute(MMHandleType player,  char **err_attr_name, const char *first_attribute_name, ...)G_GNUC_NULL_TERMINATED;

/**
 * This function is to get attributes from player. Multiple attributes can be got simultaneously.
 *
 * @param	player				[in]	Handle of player.
 * @param	err_attr_name	     		[out]  Name of attribute which is failed to get
 * @param   	first_attribute_name 	[in] 	Name of the first attribute to get
 * @param   	...					[out] Value for the first attribute, followed optionally by more name/value pairs, terminated by NULL.
 *									 But, in the case of data or string type, it should be name/value/size.
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 * @see		mm_player_set_attribute
 * @remark	This function must be terminated by NULL argument.
 *			And, if this function is failed, err_attr_name param must be free.
 * @par Example
 * @code
char *g_err_attr_name = NULL;

if (mm_player_get_attribute(g_player, &g_err_attr_name, "content_duration", &duration, NULL) != MM_ERROR_NONE)
{
	debug_warning("failed to set %s attribute\n", g_err_attr_name);
	free(g_err_attr_name);
}
 * @endcode
 */
int mm_player_get_attribute(MMHandleType player,  char **err_attr_name, const char *first_attribute_name, ...)G_GNUC_NULL_TERMINATED;

/**
 * This function is to get detail information of attribute.
 *
 * @param	player				 [in]	Handle of player.
 * @param   attribute_name		 [in] 	Name of the attribute to get
 * @param   info				 [out] 	Attribute infomation
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 *
 * @see		mm_player_set_attribute, mm_player_get_attribute
 * @remark	None
 * @par Example
 * @code
if (mm_player_get_attribute_info (g_player, "display_method", &method_info) != MM_ERROR_NONE)
{
	debug_warning("failed to get info\n");
}

debug_log("type:%d \n", method_info.type); //int, double..
debug_log("flag:%d \n", method_info.flag); //readable, writable..
debug_log("validity type:%d \n", method_info.validity_type); //range, array..

if (method_info. validity_type == MM_PLAYER_ATTRS_VALID_TYPE_INT_RANGE)
{
	debug_log("range min:%d\n", method_info.int_range.min);
	debug_log("range max:%d\n", method_info.int_range.max);
}
 * @endcode
 */
int mm_player_get_attribute_info(MMHandleType player,  const char *attribute_name, MMPlayerAttrsInfo *info);

/**
 * This function is to get download position and total size of progressive download
 *
 * @param	player		[in]	Handle of player.
 * @param	current_pos	[in]	Download position currently (bytes)
 * @param   	total_size 	[in] 	Total size of file (bytes)
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @see
 * @remark
 * @par Example
 * @code
guint64 current_pos = 0LLU;
guint64 total_size = 0LLU;

if (mm_player_get_pd_status(g_player, &current_pos, &total_size, NULL) != MM_ERROR_NONE)
{
	debug_log("current download pos = %llu, total size = %llu\n", current_pos, total_size);
}
 * @endcode
 */
int mm_player_get_pd_status(MMHandleType player, guint64 *current_pos, guint64 *total_size);

/**
 * This function sets callback function for receiving messages of PD downloader.
 *
 * @param	player		[in]	Handle of player.
 * @param	callback		[in]	Message callback function.
 * @param	user_param	[in]	User parameter which is passed to callback function.
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @see
 * @remark	None
 * @par Example
 * @code
int msg_callback(int message, MMMessageParamType *param, void *user_param)
{
	switch (message)
	{
		case MM_MESSAGE_PD_DOWNLOADER_START:
			debug_log("Progressive download is started...\n");
			break;
	 	case MM_MESSAGE_PD_DOWNLOADER_END:
	 		debug_log("Progressive download is ended...\n");
	    	  	break;
		default:
			break;
	}
	return TRUE;
}

mm_player_set_pd_message_callback(g_player, msg_callback, NULL);
 * @endcode
 */
int mm_player_set_pd_message_callback(MMHandleType player, MMMessageCallback callback, void *user_param);

/**
 * This function is to get the track count
 *
 * @param	player		[in]	handle of player.
 * @param   	track			[in] 	type of the track type
 * @param   	info			[out]	the count of the track
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 *
 * @see
 * @remark	None
 * @par Example
 * @code
gint audio_count = 0;

if (mm_player_get_track_count (g_player, MM_PLAYER_TRACK_TYPE_AUDIO, &audio_count) != MM_ERROR_NONE)
{
	debug_warning("failed to get audio track count\n");
}

debug_log("audio track count : %d \n", audio_count);
 * @endcode
 */
int mm_player_get_track_count(MMHandleType player,  MMPlayerTrackType type, int *count);

/**
 * This function is to select the track
 *
 * @param	player		[in]	handle of player.
 * @param   	type			[in] 	type of the track type
 * @param   	index		[in]	the index of the track
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 *
 * @see
 * @remark	None
 */
int mm_player_select_track(MMHandleType player, MMPlayerTrackType type, int index);
#ifdef _MULTI_TRACK
/**
 * This function is to add the track when user want multi subtitle
 *
 * @param	player		[in]	handle of player.
 * @param   	index		[in]	the index of the track
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 *
 * @see
 * @remark	None
 */
int mm_player_track_add_subtitle_language(MMHandleType player, int index);

/**
 * This function is to remove the track when user want multi subtitle
 *
 * @param	player		[in]	handle of player.
 * @param   	index		[in]	the index of the track
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 *
 * @see
 * @remark	None
 */
int mm_player_track_remove_subtitle_language(MMHandleType player, int index);

/**
 * This function is to notify which sutitle track is in use
 *
 * @param	player		[in]	handle of player.
 * @param   	callback			[in] 	callback function to register
 * @param   	user_data	[in]	user data to be passed to the callback function
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 *
 * @see
 * @remark	None
 */
int mm_player_track_foreach_selected_subtitle_language(MMHandleType player, mm_player_track_selected_subtitle_language_callback callback, void *user_param);
#endif
/**
 * This function is to get the track language
 *
 * @param	player		[in]	handle of player.
 * @param   	type			[in] 	type of the track type
 * @param   	index		[in]	the index of the track
 * @param   	code			[out] language code in ISO 639-1(string)
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 *
 * @see
 * @remark	None
 */
int mm_player_get_track_language_code(MMHandleType player,  MMPlayerTrackType type, int index, char **code);

/**
 * This function is to get the current running track
 *
 * @param       player          [in]    handle of player.
 * @param       type                    [in]    type of the track type
 * @param       index           [out]    the index of the track
 *
 * @return      This function returns zero on success, or negative value with error
 *                      code.
 *
 * @see
 * @remark      None
 */

int mm_player_get_current_track(MMHandleType hplayer, MMPlayerTrackType type, int *index);

/**
 * This function is to set the buffer size for streaming playback. \n
 *
 * @param	player		[in]	Handle of player
 * @param	second		[in]	Size of initial buffer
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @remark  None
 * @par Example
 * @code
gint second = 10; //10sec

if (mm_player_set_prepare_buffering_time(g_player, second) != MM_ERROR_NONE)
{
	debug_error("failed to set buffer size\n");
}
 * @endcode
 */

int mm_player_set_prepare_buffering_time(MMHandleType player, int second);

/**
 * This function is to set the runtime buffering mode for streaming playback. \n
 *
 * @param	player		[in]	Handle of player
 * @param	mode		[in]	mode of runtime buffering
 * @param	second		[in]	max size of buffering
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @remark  None
 * @par Example
 * @code

if (mm_player_set_runtime_buffering_mode(g_player, MM_PLAYER_BUFFERING_MODE_ADAPTIVE, 10) != MM_ERROR_NONE)
{
	debug_error("failed to set buffering mode\n");
}
 * @endcode
 */

int mm_player_set_runtime_buffering_mode(MMHandleType player, MMPlayerBufferingMode mode, int second);

/**
 * This function is to set the start position of zoom
 *
 * @param       player          [in]    handle of player
 * @param       level           [in]    level of zoom
 * @param       x             	[in]    start x position
 * @param       y           	[in]  	start y position
 *
 * @return      This function returns zero on success, or negative value with error
 *                      code.
 *
 * @see
 * @remark      None
 */
int mm_player_set_display_zoom(MMHandleType player, float level, int x, int y);

/**
 * This function is to get the start position of zoom
 *
 * @param       player           [in]    handle of player
 * @param       type            [out]    current level of zoom
 * @param       x             	[out]    start x position
 * @param       y           	[out]    start y position
 *
 * @return      This function returns zero on success, or negative value with error
 *                      code.
 *
 * @see
 * @remark      None
 */
int mm_player_get_display_zoom(MMHandleType player, float *level, int *x, int *y);

/**
 * This function is to set the subtitle path
 *
 * @param       player  [in]    handle of player
 * @param       path    [in]    subtitle path
 *
 * @return      This function returns zero on success, or negative value with error code.
 *
 * @see
 * @remark      None
 */
int mm_player_set_external_subtitle_path(MMHandleType player, const char* path);

/**
 * This function is to change clock provider to system clock
 *
 * @param       player  [in]    handle of player
 * @return      This function returns zero on success, or negative value with error code.
 *
 * @see
 * @remark      None
 */
int mm_player_use_system_clock(MMHandleType player);

/**
 * This function is to set the clock which is from master player
 *
 * @param       player  [in]    handle of player
 * @param       clock	[in]	clock of master player
 * @param       clock_delta [in]	clock difference between master and slave
 * @param       video_time	[in]	current playing position
 * @param       media_clock	[in]	media clock information
 * @param       audio_time	[in]	audio timestamp information
 * @return      This function returns zero on success, or negative value with error code.
 *
 * @see
 * @remark      None
 */
int mm_player_set_video_share_master_clock(MMHandleType player, long long clock, long long clock_delta, long long video_time, long long media_clock, long long audio_time);
/**
 * This function is to get the master clock
 *
 * @param       player		[in]    handle of player
 * @param       video_time	[out]	current playing position
 * @param       media_clock	[out]	media clock information
 * @param       audio_time	[out]	audio timestamp information
 * @return      This function returns zero on success, or negative value with error code.
 *
 * @see
 * @remark      None
 */
int mm_player_get_video_share_master_clock(MMHandleType player, long long *video_time, long long *media_clock, long long *audio_time);
/**
 * This function is to set audio channel
 *
 * @param       player		[in]    handle of player
 * @param       ch			[in]	audio channel
 * @return      This function returns zero on success, or negative value with error code.
 *
 * @see
 * @remark      None
 */
int mm_player_gst_set_audio_channel(MMHandleType player, MMPlayerAudioChannel ch);

/**
 * This function is to get the content angle
 *
 * @param       player		[in]    handle of player
 * @param       angle		[out]	orignal angle from content
 * @return      This function returns zero on success, or negative value with error code.
 *
 * @see
 * @remark      None
 */
int mm_player_get_video_rotate_angle(MMHandleType player, int *angle);

/**
 * This function is to set download mode of video hub
 *
 * @param       player		[in]    handle of player
 * @param       mode		[in]	download mode
 * @return      This function returns zero on success, or negative value with error code.
 *
 * @see
 * @remark      None
 */
int mm_player_set_video_hub_download_mode(MMHandleType player, bool mode);

/**
 * This function is to set using sync handler.
 *
 * @param       player		[in]    handle of player
 * @param       enable		[in]	enable/disable
 * @return      This function returns zero on success, or negative value with error code.
 *
 * @see
 * @remark      None
 */
int mm_player_enable_sync_handler(MMHandleType player, bool enable);

/**
 * This function is to set uri.
 *
 * @param       player		[in]    handle of player
 * @param       uri 		[in]    uri
 * @return      This function returns zero on success, or negative value with error code.
 *
 * @see
 * @remark      None
 */
int mm_player_set_uri(MMHandleType player, const char *uri);

/**
 * This function is to set next uri.
 *
 * @param       player		[in]    handle of player
 * @param       uri 		[in]    uri
 * @return      This function returns zero on success, or negative value with error code.
 *
 * @see
 * @remark      None
 */
int mm_player_set_next_uri(MMHandleType player, const char *uri);

/**
 * This function is to get next uri.
 *
 * @param       player		[in]    handle of player
 * @param       uri 		[out]   uri
 * @return      This function returns zero on success, or negative value with error code.
 *
 * @see
 * @remark      None
 */
int mm_player_get_next_uri(MMHandleType player, char **uri);

int mm_player_enable_media_packet_video_stream(MMHandleType player, bool enable);

/**
 * This function is to increase reference count of internal buffer.
 *
 * @param       buffer 		[in]   video callback internal buffer
 * @return      This function returns buffer point;
 *
 * @see
 * @remark      None
 */
void * mm_player_media_packet_video_stream_internal_buffer_ref(void *buffer);

/**
 * This function is to decrease reference count of internal buffer.
 *
 * @param       buffer 		[in]   video callback internal buffer
 * @return      None;
 *
 * @see
 * @remark      None
 */
void mm_player_media_packet_video_stream_internal_buffer_unref(void *buffer);

#ifdef TEST_ES

/**mm_player_submit_packet
 * This function is to submit buffer to appsrc.  \n
 * @param	player			[in]    Handle of player.
 * @param	buf             [in]    buffer to be submit in appsrc in external feeder case.
 * @param	len				[in]	length of buffer.
 * @param	pts				[in]	timestamp of buffer.
 * @param	streamtype		[in]	stream type of buffer.
 * @return      This function returns zero on success, or negative value with error code.
 * @par Example
 *
 * @endcode
 */
int mm_player_submit_packet(MMHandleType player, media_packet_h packet);

/**mm_player_set_video_info
 * This function is to set caps of src pad of video appsrc in external feeder case.  \n
 * @param       player                          [in]    Handle of player.
 * @param       media_format_h               	[in]    Video stream info.
 * @return      This function returns zero on success, or negative value with error code.
 * @par Example
 *
 * @endcode
 */

int mm_player_set_video_info (MMHandleType player, media_format_h format);

/**mm_player_set_audio_info
 * This function is to set caps of src pad of Audio appsrc in external feeder case.  \n
 * @param       player                       [in]    Handle of player.
 * @param       media_format_h               [in]    Audio stream info.
 * @return      This function returns zero on success, or negative value with error code.
 * @par Example
 *
 * @endcode
 */

int mm_player_set_audio_info (MMHandleType player, media_format_h format);

/**mm_player_set_subtitle_info
 * This function is to set caps of src pad of subtitle appsrc in external feeder case.  \n
 * @param       player                          [in]    Handle of player.
 * @param       subtitle_stream_info               [in]    Subtitle stream info.
 * @return      This function returns zero on success, or negative value with error code.
 * @par Example
 *
 * @endcode
 */

int mm_player_set_subtitle_info (MMHandleType player, MMPlayerSubtitleStreamInfo *info);

/**
 * This function set callback function for receiving need or enough data message from player.
 *
 * @param       player          [in]    Handle of player.
 * @param       type            [in]    stream type
 * @param       callback        [in]    data callback function for stream type.
 * @param       user_param      [in]    User parameter.
 *
 * @return      This function returns zero on success, or negative value with error
 *                      code.
 * @remark
 * @see
 * @since
 */
int mm_player_set_media_stream_buffer_status_callback(MMHandleType player, MMPlayerStreamType type, mm_player_media_stream_buffer_status_callback callback, void * user_param);

/**
 * This function set callback function for receiving seek data message from player.
 *
 * @param       player          [in]    Handle of player.
 * @param       type            [in]    stream type
 * @param       callback        [in]    Seek data callback function for stream type.
 * @param       user_param      [in]    User parameter.
 *
 * @return      This function returns zero on success, or negative value with error
 *                      code.
 * @remark
 * @see
 * @since
 */
int mm_player_set_media_stream_seek_data_callback(MMHandleType player, MMPlayerStreamType type, mm_player_media_stream_seek_data_callback callback, void * user_param);

/**
 * This function is to set max size of buffer(appsrc).
 *
 * @param       player          [in]    Handle of player.
 * @param       type            [in]    stream type
 * @param       max_size        [in]    max bytes of buffer.
 *
 * @return      This function returns zero on success, or negative value with error
 *                      code.
 * @remark
 * @see
 * @since
 */
int mm_player_set_media_stream_buffer_max_size(MMHandleType player, MMPlayerStreamType type, unsigned long long max_size);

/**
 * This function is to get max size of buffer(appsrc).
 *
 * @param       player          [in]    Handle of player.
 * @param       type            [in]    stream type
 * @param       max_size        [out]   max bytes of buffer.
 *
 * @return      This function returns zero on success, or negative value with error
 *                      code.
 * @remark
 * @see
 * @since
 */
int mm_player_get_media_stream_buffer_max_size(MMHandleType player, MMPlayerStreamType type, unsigned long long *max_size);

/**
 * This function is to set min percent of buffer(appsrc).
 *
 * @param       player          [in]    Handle of player.
 * @param       type            [in]    stream type
 * @param       min_percent     [in]    min percent of buffer.
 *
 * @return      This function returns zero on success, or negative value with error
 *                      code.
 * @remark
 * @see
 * @since
 */
int mm_player_set_media_stream_buffer_min_percent(MMHandleType player, MMPlayerStreamType type, unsigned min_percent);

/**
 * This function is to get min percent of buffer(appsrc).
 *
 * @param       player          [in]    Handle of player.
 * @param       type            [in]    stream type
 * @param       min_percent     [out]   min percent of buffer.
 *
 * @return      This function returns zero on success, or negative value with error
 *                      code.
 * @remark
 * @see
 * @since
 */
int mm_player_get_media_stream_buffer_min_percent(MMHandleType player, MMPlayerStreamType type, unsigned int *min_percent);

/**
 * This function set callback function for changing audio stream from player. \n
 * It's only supported when audio stream is included in file. \n
 *
 * @param	player   [in] Handle of player.
 * @param	callback [in] Audio stream changed callback function.
 * @param	user_param [in] User parameter.
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 * @see		mm_player_stream_changed_callback
 * @since
 */
int mm_player_set_audio_stream_changed_callback(MMHandleType player, mm_player_stream_changed_callback callback, void *user_param);

/**
 * This function set callback function for changing video stream from player. \n
 * It's only supported when video stream is included in file. \n
 *
 * @param	player   [in] Handle of player.
 * @param	callback [in] Video stream changed callback function.
 * @param	user_param [in] User parameter.
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 * @see		mm_player_stream_changed_callback
 * @since
 */
int mm_player_set_video_stream_changed_callback(MMHandleType player, mm_player_stream_changed_callback callback, void *user_param);

#endif

/**
	@}
 */

#ifdef __cplusplus
	}
#endif

#endif	/* __MM_PLAYER_H__ */
