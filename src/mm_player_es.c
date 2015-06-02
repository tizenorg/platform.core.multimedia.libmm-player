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
|  																							|
========================================================================================== */
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
  return_val_if_fail (video, MM_ERROR_INVALID_ARGUMENT);

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
  return_val_if_fail (audio, MM_ERROR_INVALID_ARGUMENT);

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
      debug_error ("media_format_get_audio_info failed");
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
      debug_error ("media_format_get_video_info failed");
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

  return_val_if_fail (player, FALSE);
  return_val_if_fail (fmt, FALSE);

  if (player->v_stream_caps)
  {
    str = gst_caps_get_structure (player->v_stream_caps, 0);
    if ( !gst_structure_get_int (str, "width", &cur_width))
    {
      debug_log ("missing 'width' field in video caps");
    }

    if ( !gst_structure_get_int (str, "height", &cur_height))
    {
      debug_log ("missing 'height' field in video caps");
    }

    media_format_get_video_info(fmt, &mimetype, &width, &height, NULL, NULL);
    if ((cur_width != width) || (cur_height != height))
    {
      debug_warning ("resolution is changed %dx%d -> %dx%d",
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

	return_val_if_fail (player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if ((type < MM_PLAYER_STREAM_TYPE_DEFAULT) || (type > MM_PLAYER_STREAM_TYPE_TEXT))
		return MM_ERROR_INVALID_ARGUMENT;

	if (player->media_stream_buffer_status_cb[type])
	{
		if (!callback)
		{
			debug_log ("[type:%d] will be clear.\n", type);
		}
		else
		{
			debug_log ("[type:%d] will be overwritten.\n", type);
		}
	}

	player->media_stream_buffer_status_cb[type] = callback;
	player->buffer_cb_user_param = user_param;

	debug_log ("player handle %p, type %d, callback %p\n", player, type,
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

	return_val_if_fail (player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if ((type < MM_PLAYER_STREAM_TYPE_DEFAULT) || (type > MM_PLAYER_STREAM_TYPE_TEXT))
		return MM_ERROR_INVALID_ARGUMENT;

	if (player->media_stream_seek_data_cb[type])
	{
		if (!callback)
		{
			debug_log ("[type:%d] will be clear.\n", type);
		}
		else
		{
			debug_log ("[type:%d] will be overwritten.\n", type);
		}
	}

	player->media_stream_seek_data_cb[type] = callback;
	player->buffer_cb_user_param = user_param;

	debug_log ("player handle %p, type %d, callback %p\n", player, type,
		player->media_stream_seek_data_cb[type]);

	MMPLAYER_FLEAVE ();

	return MM_ERROR_NONE;
}

int
_mmplayer_set_media_stream_max_size(MMHandleType hplayer, MMPlayerStreamType type, guint64 max_size)
{
	mm_player_t *player = (mm_player_t *) hplayer;

	MMPLAYER_FENTER ();

	return_val_if_fail (player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if ((type < MM_PLAYER_STREAM_TYPE_DEFAULT) || (type > MM_PLAYER_STREAM_TYPE_TEXT))
		return MM_ERROR_INVALID_ARGUMENT;

	player->media_stream_buffer_max_size[type] = max_size;

	debug_log ("type %d, max_size %llu\n",
					type, player->media_stream_buffer_max_size[type]);

	MMPLAYER_FLEAVE ();

	return MM_ERROR_NONE;
}

int
_mmplayer_get_media_stream_max_size(MMHandleType hplayer, MMPlayerStreamType type, guint64 *max_size)
{
	mm_player_t *player = (mm_player_t *) hplayer;

	MMPLAYER_FENTER ();

	return_val_if_fail (player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail (max_size, MM_ERROR_INVALID_ARGUMENT);

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

	return_val_if_fail (player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if ((type < MM_PLAYER_STREAM_TYPE_DEFAULT) || (type > MM_PLAYER_STREAM_TYPE_TEXT))
		return MM_ERROR_INVALID_ARGUMENT;

	player->media_stream_buffer_min_percent[type] = min_percent;

	debug_log ("type %d, min_per %u\n",
					type, player->media_stream_buffer_min_percent[type]);

	MMPLAYER_FLEAVE ();

	return MM_ERROR_NONE;
}

int
_mmplayer_get_media_stream_min_percent(MMHandleType hplayer, MMPlayerStreamType type, guint *min_percent)
{
	mm_player_t *player = (mm_player_t *) hplayer;

	MMPLAYER_FENTER ();

	return_val_if_fail (player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail (min_percent, MM_ERROR_INVALID_ARGUMENT);

	if ((type < MM_PLAYER_STREAM_TYPE_DEFAULT) || (type > MM_PLAYER_STREAM_TYPE_TEXT))
		return MM_ERROR_INVALID_ARGUMENT;

	*min_percent = player->media_stream_buffer_min_percent[type];

	MMPLAYER_FLEAVE ();

	return MM_ERROR_NONE;
}

int
_mmplayer_submit_packet (MMHandleType hplayer, media_packet_h packet)
{
  int ret = MM_ERROR_NONE;
  GstBuffer *_buffer;
  mm_player_t *player = (mm_player_t *) hplayer;
  guint8 *buf = NULL;
  MMPlayerTrackType streamtype = MM_PLAYER_TRACK_TYPE_AUDIO;
  media_format_h fmt = NULL;

  return_val_if_fail (player, MM_ERROR_PLAYER_NOT_INITIALIZED);
  return_val_if_fail (packet, MM_ERROR_INVALID_ARGUMENT);

  /* get data */
  media_packet_get_buffer_data_ptr (packet, (void **) &buf);

  if (buf != NULL) {
    GstMapInfo buff_info = GST_MAP_INFO_INIT;
    uint64_t size = 0;
    uint64_t pts = 0;
    bool flag = FALSE;

    /* get size */
    media_packet_get_buffer_size (packet, &size);

    _buffer = gst_buffer_new_and_alloc (size);
    if (gst_buffer_map (_buffer, &buff_info, GST_MAP_READWRITE)) {

      memcpy (buff_info.data, buf, size);
      buff_info.size = size;

      gst_buffer_unmap (_buffer, &buff_info);
    }

    /* get pts */
    media_packet_get_pts (packet, &pts);
    GST_BUFFER_PTS (_buffer) = (GstClockTime) (pts * 1000000);

    /* get stream type if audio or video */
    media_packet_is_audio (packet, &flag);
    if (flag) {
      streamtype = MM_PLAYER_TRACK_TYPE_AUDIO;
    } else {
      media_packet_is_video (packet, &flag);

      if (flag)
        streamtype = MM_PLAYER_TRACK_TYPE_VIDEO;
      else
        streamtype = MM_PLAYER_TRACK_TYPE_TEXT;
    }

    if (streamtype == MM_PLAYER_TRACK_TYPE_AUDIO) {
#if 0                           // TO CHECK : has gone (set to pad)
      if (GST_CAPS_IS_SIMPLE (player->a_stream_caps))
        GST_BUFFER_CAPS (_buffer) = gst_caps_copy (player->a_stream_caps);
      else
        debug_error ("External Demuxer case: Audio Buffer Caps not set.");
#endif
      if (player->pipeline->mainbin[MMPLAYER_M_2ND_SRC].gst)
        gst_app_src_push_buffer (GST_APP_SRC (player->pipeline->mainbin[MMPLAYER_M_2ND_SRC].gst), _buffer);
      else if (g_strrstr (GST_ELEMENT_NAME (player->pipeline->mainbin[MMPLAYER_M_SRC].gst), "audio_appsrc"))
        gst_app_src_push_buffer (GST_APP_SRC (player->pipeline->mainbin[MMPLAYER_M_SRC].gst), _buffer);
    } else if (streamtype == MM_PLAYER_TRACK_TYPE_VIDEO) {
#if 0                           // TO CHECK : has gone (set to pad)
      if (GST_CAPS_IS_SIMPLE (player->v_stream_caps))
        GST_BUFFER_CAPS (_buffer) = gst_caps_copy (player->v_stream_caps);
      else
        debug_error ("External Demuxer case: Video Buffer Caps not set.");
#endif
      /* get format to check video format */
      media_packet_get_format (packet, &fmt);
      if (fmt)
      {
        gboolean ret = FALSE;
        ret = _mmplayer_update_video_info(hplayer, fmt);
        if (ret)
        {
          g_object_set(G_OBJECT(player->pipeline->mainbin[MMPLAYER_M_SRC].gst),
                                    "caps", player->v_stream_caps, NULL);
        }
      }

      gst_app_src_push_buffer (GST_APP_SRC (player->pipeline->mainbin[MMPLAYER_M_SRC].gst), _buffer);
    } else if (streamtype == MM_PLAYER_TRACK_TYPE_TEXT) {
#if 0                           // TO CHECK : has gone (set to pad)
      if (GST_CAPS_IS_SIMPLE (player->s_stream_caps))
        GST_BUFFER_CAPS (_buffer) = gst_caps_copy (player->s_stream_caps);
      else
        debug_error ("External Demuxer case: Subtitle Buffer Caps not set.");
#endif
      gst_app_src_push_buffer (GST_APP_SRC (player->pipeline->mainbin[MMPLAYER_M_SUBSRC].gst), _buffer);
    } else {
      debug_error ("Not a valid packet from external demux");
      return FALSE;
    }
  } else {
    debug_log ("Sending EOS on pipeline...");
    if (streamtype == MM_PLAYER_TRACK_TYPE_AUDIO) {
      if (player->pipeline->mainbin[MMPLAYER_M_2ND_SRC].gst)
        g_signal_emit_by_name (player->pipeline->
            mainbin[MMPLAYER_M_2ND_SRC].gst, "end-of-stream", &ret);
      else
        g_signal_emit_by_name (player->pipeline->mainbin[MMPLAYER_M_SRC].gst,
            "end-of-stream", &ret);
    } else if (streamtype == MM_PLAYER_TRACK_TYPE_VIDEO) {
      g_signal_emit_by_name (player->pipeline->mainbin[MMPLAYER_M_SRC].gst,
          "end-of-stream", &ret);
    } else if (streamtype == MM_PLAYER_TRACK_TYPE_TEXT) {
      g_signal_emit_by_name (player->pipeline->mainbin[MMPLAYER_M_SUBSRC].gst,
          "end-of-stream", &ret);
    }
  }

  if (MMPLAYER_PENDING_STATE (player) == MM_PLAYER_STATE_PLAYING) {
    //ret = __mmplayer_set_state(player, MM_PLAYER_STATE_PLAYING);
  }

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
  return_val_if_fail (player, MM_ERROR_PLAYER_NOT_INITIALIZED);
  return_val_if_fail (video, MM_ERROR_PLAYER_NOT_INITIALIZED);

  debug_log ("width=%d height=%d framerate num=%d, den=%d",
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
    debug_warning ("caps will be updated ");

    gst_caps_unref(player->v_stream_caps);
    player->v_stream_caps = NULL;
  }

  player->v_stream_caps = gst_caps_copy (caps);
  MMPLAYER_LOG_GST_CAPS_TYPE (player->v_stream_caps);
  gst_caps_unref (caps);

  MMPLAYER_FLEAVE ();

  return MM_ERROR_NONE;
}

int
_mmplayer_set_video_info (MMHandleType hplayer, media_format_h format)
{
  mm_player_t *player = MM_PLAYER_CAST (hplayer);
  MMPlayerVideoStreamInfo video = { 0, };
  int ret = MM_ERROR_NONE;

  MMPLAYER_FENTER ();

  return_val_if_fail (player, MM_ERROR_PLAYER_NOT_INITIALIZED);

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

  return_val_if_fail (hplayer, MM_ERROR_PLAYER_NOT_INITIALIZED);

  ret = _parse_media_format (NULL, &audio, format);
  if(ret != MM_ERROR_NONE)
    return ret;

  audio.user_info = 0;           //test

  debug_log ("set audio player[%p] info [%p] version=%d rate=%d channel=%d",
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

  return_val_if_fail (player, MM_ERROR_PLAYER_NOT_INITIALIZED);
  return_val_if_fail (info, MM_ERROR_PLAYER_NOT_INITIALIZED);

  debug_log ("set subtitle player[%p] info [%p]", player, info);


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
