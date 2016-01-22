/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, heechul jeon <heechul.jeon@samsung.co>,
 * YoungHwan An <younghwan_.an@samsung.com>, Eunhae Choi <eunhae1.choi@samsung.com>
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
#include <dlog.h>
#include "mm_player_es.h"
#include "mm_player_utils.h"
#include "mm_player_internal.h"

#include <gst/app/gstappsrc.h>

/*---------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS for internal								|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:												|
---------------------------------------------------------------------------*/
static int _parse_media_format (MMPlayerVideoStreamInfo * video, MMPlayerAudioStreamInfo * audio, media_format_h format);
static int _convert_media_format_video_mime_to_str (MMPlayerVideoStreamInfo * video, media_format_mimetype_e mime);
static int _convert_media_format_audio_mime_to_str (MMPlayerAudioStreamInfo * audio, media_format_mimetype_e mime);

/*===========================================================================================
|																							|
|  FUNCTION DEFINITIONS																		|
|  																							|
========================================================================================== */

static int
_convert_media_format_video_mime_to_str (MMPlayerVideoStreamInfo * video,
    media_format_mimetype_e mime)
{
  MMPLAYER_RETURN_VAL_IF_FAIL (video, MM_ERROR_INVALID_ARGUMENT);

  switch (mime) {
    case MEDIA_FORMAT_MPEG4_SP:
      video->mime = g_strdup ("video/mpeg");
      video->version = 4;
      break;
    case MEDIA_FORMAT_H264_SP:
	case MEDIA_FORMAT_H264_MP:
	case MEDIA_FORMAT_H264_HP:
      video->mime = g_strdup ("video/x-h264");
      break;
    default:
      video->mime = g_strdup ("unknown");
      break;
  }

  return MM_ERROR_NONE;
}

static int
_convert_media_format_audio_mime_to_str (MMPlayerAudioStreamInfo * audio,
    media_format_mimetype_e mime)
{
  MMPLAYER_RETURN_VAL_IF_FAIL (audio, MM_ERROR_INVALID_ARGUMENT);

  switch (mime) {
    case MEDIA_FORMAT_AAC:
      audio->mime = g_strdup ("audio/mpeg");
      audio->version = 2;
      break;
    default:
      audio->mime = g_strdup ("unknown");
      break;
  }

  return MM_ERROR_NONE;
}

static int
_parse_media_format (MMPlayerVideoStreamInfo * video,
    MMPlayerAudioStreamInfo * audio, media_format_h format)
{
  if (audio) {
    media_format_mimetype_e mime;
    int channel;
    int samplerate;
    int avg_bps;

    if (media_format_get_audio_info (format, &mime, &channel, &samplerate, NULL,
            &avg_bps) != MEDIA_FORMAT_ERROR_NONE) {
      LOGE ("media_format_get_audio_info failed");
	  return MM_ERROR_PLAYER_INTERNAL;
    }

    _convert_media_format_audio_mime_to_str (audio, mime);
    audio->sample_rate = samplerate;
    audio->channels = channel;
//video->user_info = ;
  }

  if (video) {
    media_format_mimetype_e mime;
    int width;
    int height;
    int avg_bps;

    if (media_format_get_video_info (format, &mime, &width, &height, &avg_bps,
            NULL) != MEDIA_FORMAT_ERROR_NONE) {
      LOGE ("media_format_get_video_info failed");
	  return MM_ERROR_PLAYER_INTERNAL;
    }

    _convert_media_format_video_mime_to_str (video, mime);
    video->width = width;
    video->height = height;
  }

  return MM_ERROR_NONE;
}

static gboolean
_mmplayer_update_video_info(MMHandleType hplayer, media_format_h fmt)
{
  mm_player_t *player = (mm_player_t *) hplayer;
  gboolean ret = FALSE;
  GstStructure *str = NULL;
  media_format_mimetype_e mimetype = 0;
  gint cur_width = 0, width = 0;
  gint cur_height = 0, height = 0;

  MMPLAYER_FENTER ();

  MMPLAYER_RETURN_VAL_IF_FAIL (player, FALSE);
  MMPLAYER_RETURN_VAL_IF_FAIL (fmt, FALSE);

  if (player->v_stream_caps)
  {
    str = gst_caps_get_structure (player->v_stream_caps, 0);
    if ( !gst_structure_get_int (str, "width", &cur_width))
    {
      LOGD ("missing 'width' field in video caps");
    }

    if ( !gst_structure_get_int (str, "height", &cur_height))
    {
      LOGD ("missing 'height' field in video caps");
    }

    media_format_get_video_info(fmt, &mimetype, &width, &height, NULL, NULL);
    if ((cur_width != width) || (cur_height != height))
    {
      LOGW ("resolution is changed %dx%d -> %dx%d",
                          cur_width, cur_height, width, height);
      _mmplayer_set_video_info(hplayer, fmt);
      ret = TRUE;
    }
  }

  MMPLAYER_FLEAVE ();
  return ret;
}


int
_mmplayer_set_media_stream_buffer_status_cb(MMHandleType hplayer,
                                            MMPlayerStreamType type,
                                            mm_player_media_stream_buffer_status_callback callback,
                                            void *user_param)
{
	mm_player_t *player = (mm_player_t *) hplayer;

	MMPLAYER_FENTER ();

	MMPLAYER_RETURN_VAL_IF_FAIL (player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if ((type < MM_PLAYER_STREAM_TYPE_DEFAULT) || (type > MM_PLAYER_STREAM_TYPE_TEXT))
		return MM_ERROR_INVALID_ARGUMENT;

	if (player->media_stream_buffer_status_cb[type])
	{
		if (!callback)
		{
			LOGD ("[type:%d] will be clear.\n", type);
		}
		else
		{
			LOGD ("[type:%d] will be overwritten.\n", type);
		}
	}

	player->media_stream_buffer_status_cb[type] = callback;
	player->buffer_cb_user_param = user_param;

	LOGD ("player handle %p, type %d, callback %p\n", player, type,
		player->media_stream_buffer_status_cb[type]);

	MMPLAYER_FLEAVE ();

	return MM_ERROR_NONE;
}

int
_mmplayer_set_media_stream_seek_data_cb(MMHandleType hplayer,
                                        MMPlayerStreamType type,
                                        mm_player_media_stream_seek_data_callback callback,
                                        void *user_param)
{
	mm_player_t *player = (mm_player_t *) hplayer;

	MMPLAYER_FENTER ();

	MMPLAYER_RETURN_VAL_IF_FAIL (player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if ((type < MM_PLAYER_STREAM_TYPE_DEFAULT) || (type > MM_PLAYER_STREAM_TYPE_TEXT))
		return MM_ERROR_INVALID_ARGUMENT;

	if (player->media_stream_seek_data_cb[type])
	{
		if (!callback)
		{
			LOGD ("[type:%d] will be clear.\n", type);
		}
		else
		{
			LOGD ("[type:%d] will be overwritten.\n", type);
		}
	}

	player->media_stream_seek_data_cb[type] = callback;
	player->buffer_cb_user_param = user_param;

	LOGD ("player handle %p, type %d, callback %p\n", player, type,
		player->media_stream_seek_data_cb[type]);

	MMPLAYER_FLEAVE ();

	return MM_ERROR_NONE;
}

int
_mmplayer_set_media_stream_max_size(MMHandleType hplayer, MMPlayerStreamType type, guint64 max_size)
{
	mm_player_t *player = (mm_player_t *) hplayer;

	MMPLAYER_FENTER ();

	MMPLAYER_RETURN_VAL_IF_FAIL (player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if ((type < MM_PLAYER_STREAM_TYPE_DEFAULT) || (type > MM_PLAYER_STREAM_TYPE_TEXT) ||
		(max_size == 0))
		return MM_ERROR_INVALID_ARGUMENT;

	player->media_stream_buffer_max_size[type] = max_size;

	LOGD ("type %d, max_size %llu\n",
					type, player->media_stream_buffer_max_size[type]);

	MMPLAYER_FLEAVE ();

	return MM_ERROR_NONE;
}

int
_mmplayer_get_media_stream_max_size(MMHandleType hplayer, MMPlayerStreamType type, guint64 *max_size)
{
	mm_player_t *player = (mm_player_t *) hplayer;

	MMPLAYER_FENTER ();

	MMPLAYER_RETURN_VAL_IF_FAIL (player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	MMPLAYER_RETURN_VAL_IF_FAIL (max_size, MM_ERROR_INVALID_ARGUMENT);

	if ((type < MM_PLAYER_STREAM_TYPE_DEFAULT) || (type > MM_PLAYER_STREAM_TYPE_TEXT))
		return MM_ERROR_INVALID_ARGUMENT;

	*max_size = player->media_stream_buffer_max_size[type];

	MMPLAYER_FLEAVE ();

	return MM_ERROR_NONE;
}

int
_mmplayer_set_media_stream_min_percent(MMHandleType hplayer, MMPlayerStreamType type, guint min_percent)
{
	mm_player_t *player = (mm_player_t *) hplayer;

	MMPLAYER_FENTER ();

	MMPLAYER_RETURN_VAL_IF_FAIL (player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if ((type < MM_PLAYER_STREAM_TYPE_DEFAULT) || (type > MM_PLAYER_STREAM_TYPE_TEXT))
		return MM_ERROR_INVALID_ARGUMENT;

	player->media_stream_buffer_min_percent[type] = min_percent;

	LOGD ("type %d, min_per %u\n",
					type, player->media_stream_buffer_min_percent[type]);

	MMPLAYER_FLEAVE ();

	return MM_ERROR_NONE;
}

int
_mmplayer_get_media_stream_min_percent(MMHandleType hplayer, MMPlayerStreamType type, guint *min_percent)
{
	mm_player_t *player = (mm_player_t *) hplayer;

	MMPLAYER_FENTER ();

	MMPLAYER_RETURN_VAL_IF_FAIL (player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	MMPLAYER_RETURN_VAL_IF_FAIL (min_percent, MM_ERROR_INVALID_ARGUMENT);

	if ((type < MM_PLAYER_STREAM_TYPE_DEFAULT) || (type > MM_PLAYER_STREAM_TYPE_TEXT))
		return MM_ERROR_INVALID_ARGUMENT;

	*min_percent = player->media_stream_buffer_min_percent[type];

	MMPLAYER_FLEAVE ();

	return MM_ERROR_NONE;
}

static int
__mmplayer_check_buffer_level(mm_player_t *player, GstElement* element, MMPlayerStreamType type)
{
	guint64 current_level_bytes = 0;
	guint64 max_bytes = 0;
	guint current_level_per = 0;

	MMPLAYER_FENTER ();
	MMPLAYER_RETURN_VAL_IF_FAIL ( player && element, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if (player->media_stream_buffer_max_size[type] > 0) {
		max_bytes = player->media_stream_buffer_max_size[type];
	} else {
		g_object_get(G_OBJECT(element), "max-bytes", &max_bytes, NULL);
	}

	if (max_bytes == 0) {
		LOGW ("buffer max size is zero.");
		return MM_ERROR_NONE;
	}

	g_object_get(G_OBJECT(element), "current-level-bytes", &current_level_bytes, NULL);

	if (max_bytes <= current_level_bytes) {
		LOGE ("no available buffer space. type %d, max %lld, curr %lld", type, max_bytes, current_level_bytes);
	}

	if (MMPLAYER_CURRENT_STATE(player) != MM_PLAYER_STATE_PLAYING) {
		if (!player->media_stream_buffer_status_cb[type]) {
			return MM_ERROR_NONE;
		}

		current_level_per = (guint)(gst_util_guint64_to_gdouble(current_level_bytes)/gst_util_guint64_to_gdouble(max_bytes)*100);

		LOGD ("type %d, min_per %u, curr_per %u max %lld cur %lld\n",
					type, player->media_stream_buffer_min_percent[type],
					current_level_per,
					player->media_stream_buffer_max_size[type],
					current_level_bytes);

		if (current_level_per < player->media_stream_buffer_min_percent[type])
			player->media_stream_buffer_status_cb[type](type, MM_PLAYER_MEDIA_STREAM_BUFFER_UNDERRUN, current_level_bytes, player->buffer_cb_user_param);
	}

	MMPLAYER_FLEAVE ();
	return MM_ERROR_NONE;
}

int
_mmplayer_submit_packet (MMHandleType hplayer, media_packet_h packet)
{
  int ret = MM_ERROR_NONE;
  GstBuffer *_buffer = NULL;
  mm_player_t *player = (mm_player_t *) hplayer;
  guint8 *buf = NULL;
  uint64_t size = 0;
  enum MainElementID elemId = MMPLAYER_M_NUM;
  MMPlayerStreamType streamtype = MM_PLAYER_STREAM_TYPE_AUDIO;
  media_format_h fmt = NULL;
  bool flag = FALSE;
  bool is_eos = FALSE;

  MMPLAYER_RETURN_VAL_IF_FAIL (packet, MM_ERROR_INVALID_ARGUMENT);
  MMPLAYER_RETURN_VAL_IF_FAIL ( player &&
    player->pipeline &&
    player->pipeline->mainbin &&
    player->pipeline->mainbin[MMPLAYER_M_SRC].gst,
    MM_ERROR_PLAYER_INTERNAL );

  /* get stream type if audio or video */
  media_packet_is_audio (packet, &flag);
  if (flag) {
    streamtype = MM_PLAYER_STREAM_TYPE_AUDIO;
    if (player->pipeline->mainbin[MMPLAYER_M_2ND_SRC].gst) {
      elemId = MMPLAYER_M_2ND_SRC;
    } else if (g_strrstr (GST_ELEMENT_NAME (player->pipeline->mainbin[MMPLAYER_M_SRC].gst), "audio_appsrc")) {
      elemId = MMPLAYER_M_SRC;
    } else {
      LOGE ("there is no audio appsrc");
      ret = MM_ERROR_PLAYER_INTERNAL;
      goto ERROR;
    }
  } else {
    media_packet_is_video (packet, &flag);
    if (flag) {
      streamtype = MM_PLAYER_STREAM_TYPE_VIDEO;
      elemId = MMPLAYER_M_SRC;
    } else {
      streamtype = MM_PLAYER_STREAM_TYPE_TEXT;
      elemId = MMPLAYER_M_SUBSRC;
    }
  }

  /* get data */
  if (media_packet_get_buffer_data_ptr (packet, (void **) &buf) != MEDIA_PACKET_ERROR_NONE) {
    LOGE("failed to get buffer data ptr");
    ret = MM_ERROR_PLAYER_INTERNAL;
    goto ERROR;
  }

  if (media_packet_get_buffer_size (packet, &size) != MEDIA_PACKET_ERROR_NONE) {
    LOGE("failed to get buffer size");
    ret = MM_ERROR_PLAYER_INTERNAL;
    goto ERROR;
  }

  if (buf != NULL && size > 0) {
    GstMapInfo buff_info = GST_MAP_INFO_INIT;
    uint64_t pts = 0;

    /* get size */
    _buffer = gst_buffer_new_and_alloc (size);

    if (!_buffer) {
        LOGE("failed to allocate memory for push buffer\n");
        ret = MM_ERROR_PLAYER_NO_FREE_SPACE;
        goto ERROR;
    }

    if (gst_buffer_map (_buffer, &buff_info, GST_MAP_READWRITE)) {

      memcpy (buff_info.data, buf, size);
      buff_info.size = size;

      gst_buffer_unmap (_buffer, &buff_info);
    }

    if (streamtype == MM_PLAYER_STREAM_TYPE_VIDEO) {
      /* get format to check video format */
      media_packet_get_format (packet, &fmt);
      if (fmt) {
        if (_mmplayer_update_video_info(hplayer, fmt)) {
          LOGD("update video caps");
          g_object_set(G_OBJECT(player->pipeline->mainbin[MMPLAYER_M_SRC].gst),
                                    "caps", player->v_stream_caps, NULL);
        }
      }
    }

    /* get pts */
    if (media_packet_get_pts (packet, &pts) != MEDIA_PACKET_ERROR_NONE) {
      LOGE("failed to get pts info");
      ret = MM_ERROR_PLAYER_INTERNAL;
      goto ERROR;
    }
    GST_BUFFER_PTS (_buffer) = (GstClockTime)pts;

    if ((elemId < MMPLAYER_M_NUM) && (player->pipeline->mainbin[elemId].gst)) {
      gst_app_src_push_buffer (GST_APP_SRC (player->pipeline->mainbin[elemId].gst), _buffer);
    } else {
      LOGE ("elem(%d) does not exist.", elemId);
      ret = MM_ERROR_PLAYER_INTERNAL;
      goto ERROR;
    }
  }

  ret = __mmplayer_check_buffer_level (player, player->pipeline->mainbin[elemId].gst, streamtype);
  if (ret != MM_ERROR_NONE)
    return ret;

  /* check eos */
  if (media_packet_is_end_of_stream(packet, &is_eos) != MEDIA_PACKET_ERROR_NONE) {
    LOGE("failed to get eos info");
    ret = MM_ERROR_PLAYER_INTERNAL;
    goto ERROR;
  }

  if (is_eos) {
    LOGW ("we got eos of stream type(%d)", streamtype);
    if ((elemId < MMPLAYER_M_NUM) && (player->pipeline->mainbin[elemId].gst)) {
      g_signal_emit_by_name (player->pipeline->
          mainbin[elemId].gst, "end-of-stream", &ret);
    } else {
      LOGE ("elem(%d) does not exist.", elemId);
      ret = MM_ERROR_PLAYER_INTERNAL;
    }
  }

ERROR:
  return ret;
}

int
_mmplayer_video_caps_new (MMHandleType hplayer, MMPlayerVideoStreamInfo * video,
    const char *fieldname, ...)
{
  int cap_size;
  GstCaps *caps = NULL;
  GstStructure *structure = NULL;
  va_list var_args;
  mm_player_t *player = MM_PLAYER_CAST (hplayer);

  MMPLAYER_FENTER ();
  MMPLAYER_RETURN_VAL_IF_FAIL (player, MM_ERROR_PLAYER_NOT_INITIALIZED);
  MMPLAYER_RETURN_VAL_IF_FAIL (video, MM_ERROR_PLAYER_NOT_INITIALIZED);

  LOGD ("width=%d height=%d framerate num=%d, den=%d",
    video->width, video->height, video->framerate_num, video->framerate_den);

  caps = gst_caps_new_simple (video->mime,
      "width", G_TYPE_INT, video->width,
      "height", G_TYPE_INT, video->height,
      "framerate", GST_TYPE_FRACTION, video->framerate_num, video->framerate_den, NULL);

  for (cap_size = 0; cap_size < gst_caps_get_size (caps); cap_size++) {
    va_start (var_args, fieldname);
    structure = gst_caps_get_structure (caps, cap_size);
    gst_structure_set_valist (structure, fieldname, var_args);
    va_end (var_args);
  }

  if (video->extradata_size) {
    GstBuffer *buf = NULL;
    GstMapInfo buff_info = GST_MAP_INFO_INIT;

    buf = gst_buffer_new_and_alloc (video->extradata_size);

    if (gst_buffer_map (buf, &buff_info, GST_MAP_READ)) {
      memcpy (buff_info.data, video->codec_extradata, video->extradata_size);
      buff_info.size = video->extradata_size;
      gst_buffer_unmap (buf, &buff_info);
    }

    gst_caps_set_simple (caps, "codec_data", GST_TYPE_BUFFER, buf, NULL);
    gst_buffer_unref (buf);
  }

  if (player->v_stream_caps)
  {
    LOGW ("caps will be updated ");

    gst_caps_unref(player->v_stream_caps);
    player->v_stream_caps = NULL;
  }

  player->v_stream_caps = gst_caps_copy (caps);
  MMPLAYER_LOG_GST_CAPS_TYPE (player->v_stream_caps);
  gst_caps_unref (caps);

  MMPLAYER_FLEAVE ();

  return MM_ERROR_NONE;
}

static void
_mmplayer_set_uri_type(mm_player_t *player)
{
	MMPLAYER_FENTER ();

	player->profile.uri_type = MM_PLAYER_URI_TYPE_MS_BUFF;
	player->es_player_push_mode = TRUE;

	MMPLAYER_FLEAVE ();
	return;
}

int
_mmplayer_set_video_info (MMHandleType hplayer, media_format_h format)
{
  mm_player_t *player = MM_PLAYER_CAST (hplayer);
  MMPlayerVideoStreamInfo video = { 0, };
  int ret = MM_ERROR_NONE;

  MMPLAYER_FENTER ();

  MMPLAYER_RETURN_VAL_IF_FAIL (player, MM_ERROR_PLAYER_NOT_INITIALIZED);

  _mmplayer_set_uri_type(player);

  ret = _parse_media_format (&video, NULL, format);
  if(ret != MM_ERROR_NONE)
    return ret;

  if (strstr (video.mime, "video/mpeg")) {
    _mmplayer_video_caps_new (hplayer, &video,
        "mpegversion", G_TYPE_INT, video.version,
        "systemstream", G_TYPE_BOOLEAN, FALSE, NULL);
  } else if (strstr (video.mime, "video/x-h264")) {
    //if (info.colordepth)
    {
      //      _mmplayer_video_caps_new(hplayer, &info,
      //              "colordepth", G_TYPE_INT, info.colordepth, NULL);
    }
    //else
    {
      _mmplayer_video_caps_new (hplayer, &video,
          "stream-format", G_TYPE_STRING, "byte-stream",
          "alignment", G_TYPE_STRING, "au", NULL);
    }
  }
#if 0
  else if (strstr (info->mime, "video/x-wmv")) {
    _mmplayer_video_caps_new (hplayer, &info,
        "wmvversion", G_TYPE_INT, info.version, NULL);
  } else if (strstr (info.mime, "video/x-pn-realvideo")) {
    _mmplayer_video_caps_new (hplayer, &info,
        "rmversion", G_TYPE_INT, info.version, NULL);
  } else if (strstr (info.mime, "video/x-msmpeg")) {
    _mmplayer_video_caps_new (hplayer, &info,
        "msmpegversion", G_TYPE_INT, info.version, NULL);
  } else if (strstr (info.mime, "video/x-h265")) {
    if (info.colordepth) {
      _mmplayer_video_caps_new (hplayer, &info,
          "colordepth", G_TYPE_INT, info.colordepth, NULL);
    } else {
      _mmplayer_video_caps_new (hplayer, &info, NULL);
    }
  } else {
    _mmplayer_video_caps_new (hplayer, &info, NULL);
  }
#endif
  g_free ((char *) video.mime);

  MMPLAYER_FLEAVE ();

  return MM_ERROR_NONE;
}

int
_mmplayer_set_audio_info (MMHandleType hplayer, media_format_h format)
{
  mm_player_t *player = MM_PLAYER_CAST (hplayer);
  GstCaps *caps = NULL;
  MMPlayerAudioStreamInfo audio = { 0, };
  int ret = MM_ERROR_NONE;

  MMPLAYER_FENTER ();

  MMPLAYER_RETURN_VAL_IF_FAIL (hplayer, MM_ERROR_PLAYER_NOT_INITIALIZED);

  _mmplayer_set_uri_type(player);

  ret = _parse_media_format (NULL, &audio, format);
  if(ret != MM_ERROR_NONE)
    return ret;

  audio.user_info = 0;           //test

  LOGD ("set audio player[%p] info [%p] version=%d rate=%d channel=%d",
      player, audio, audio.version, audio.sample_rate, audio.channels);

  if (strstr (audio.mime, "audio/mpeg")) {
    if (audio.version == 1) {  	// mp3
      caps = gst_caps_new_simple ("audio/mpeg",
          "channels", G_TYPE_INT, audio.channels,
          "rate", G_TYPE_INT, audio.sample_rate,
          "mpegversion", G_TYPE_INT, audio.version,
          "layer", G_TYPE_INT, audio.user_info, NULL);
    } else {                    // aac
      gchar *format = NULL;

      if (audio.user_info == 0)
        format = g_strdup ("raw");
      else if (audio.user_info == 1)
        format = g_strdup ("adts");
      else if (audio.user_info == 2)
        format = g_strdup ("adif");

      caps = gst_caps_new_simple ("audio/mpeg",
          "channels", G_TYPE_INT, audio.channels,
          "rate", G_TYPE_INT, audio.sample_rate,
          "mpegversion", G_TYPE_INT, audio.version,
          "stream-format", G_TYPE_STRING, format, NULL);

      g_free (format);
      format = NULL;
    }
  }
#if 0
  else if (strstr (audio.mime, "audio/x-raw-int")) {
    caps = gst_caps_new_simple ("audio/x-raw-int",
        "width", G_TYPE_INT, audio.width,
        "depth", G_TYPE_INT, audio.depth,
        "endianness", G_TYPE_INT, audio.endianness,
        "signed", G_TYPE_BOOLEAN, audio.signedness,
        "channels", G_TYPE_INT, audio.channels,
        "rate", G_TYPE_INT, audio.sample_rate, NULL);
  } else {
    caps = gst_caps_new_simple (audio.mime,
        "channels", G_TYPE_INT, audio.channels,
        "rate", G_TYPE_INT, audio.sample_rate, NULL);
  }
#endif

  if (audio.extradata_size) {
    GstBuffer *buf = NULL;
    GstMapInfo buff_info = GST_MAP_INFO_INIT;

    buf = gst_buffer_new_and_alloc (audio.extradata_size);

    if (gst_buffer_map (buf, &buff_info, GST_MAP_READ)) {
      memcpy (buff_info.data, audio.codec_extradata, audio.extradata_size);
      gst_buffer_unmap (buf, &buff_info);
    }

    gst_caps_set_simple (caps, "codec_data", GST_TYPE_BUFFER, buf, NULL);
    gst_buffer_unref (buf);
  }

  g_free ((char *) audio.mime);

  player->a_stream_caps = gst_caps_copy (caps);
  gst_caps_unref (caps);

  MMPLAYER_FLEAVE ();

  return MM_ERROR_NONE;
}

int
_mmplayer_set_subtitle_info (MMHandleType hplayer,
    MMPlayerSubtitleStreamInfo * subtitle)
{
#if 0                           //todo

  mm_player_t *player = MM_PLAYER_CAST (hplayer);
  GstCaps *caps = NULL;

  MMPLAYER_FENTER ();

  MMPLAYER_RETURN_VAL_IF_FAIL (player, MM_ERROR_PLAYER_NOT_INITIALIZED);
  MMPLAYER_RETURN_VAL_IF_FAIL (info, MM_ERROR_PLAYER_NOT_INITIALIZED);

  LOGD ("set subtitle player[%p] info [%p]", player, info);


  caps = gst_caps_new_simple (info->mime, NULL, NULL);  // TO CHECK
  if (NULL == caps)
    return FALSE;

  if (strstr (info->mime, "application/x-xsub")) {
    gst_caps_set_simple (caps, "codec_tag", G_TYPE_UINT, info->codec_tag, NULL);
  } else if (strstr (info->mime, "application/x-smpte-text")) {
    if (info->context) {
      gst_caps_set_simple (caps, "ttml_priv_data", G_TYPE_POINTER,
          info->context, NULL);
    }
  }

  player->s_stream_caps = gst_caps_copy (caps);

  gst_caps_unref (caps);
#endif

  MMPLAYER_FLEAVE ();

  return MM_ERROR_NONE;
}
