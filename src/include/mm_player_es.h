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

#ifndef __MM_PLAYER_ES_H__
#define __MM_PLAYER_ES_H__

/*=======================================================================================
| INCLUDE FILES										|
========================================================================================*/
#include <mm_types.h>
#include "mm_player_priv.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*=======================================================================================
| GLOBAL FUNCTION PROTOTYPES								|
========================================================================================*/
int _mmplayer_set_video_info (MMHandleType player, media_format_h format);

int _mmplayer_set_audio_info (MMHandleType player, media_format_h format);

int _mmplayer_set_subtitle_info (MMHandleType player, MMPlayerSubtitleStreamInfo * info);

int _mmplayer_submit_packet (MMHandleType player, media_packet_h packet);

int _mmplayer_set_media_stream_buffer_status_cb (MMHandleType player,
                                                 MMPlayerStreamType type,
                                                 mm_player_media_stream_buffer_status_callback callback,
                                                 void * user_param);

int _mmplayer_set_media_stream_seek_data_cb (MMHandleType player,
                                             MMPlayerStreamType type,
                                             mm_player_media_stream_seek_data_callback callback,
                                             void * user_param);

int _mmplayer_set_media_stream_max_size (MMHandleType hplayer,
                                         MMPlayerStreamType type,
                                         guint64 max_size);

int _mmplayer_get_media_stream_max_size(MMHandleType hplayer,
                                        MMPlayerStreamType type,
                                        guint64 *max_size);

int _mmplayer_set_media_stream_min_percent(MMHandleType hplayer,
                                           MMPlayerStreamType type,
                                           guint min_percent);

int _mmplayer_get_media_stream_min_percent(MMHandleType hplayer,
                                           MMPlayerStreamType type,
                                           guint *min_percent);
int _mmplayer_set_media_stream_dynamic_resolution(MMHandleType hplayer, bool drc);

#ifdef __cplusplus
}
#endif

#endif
