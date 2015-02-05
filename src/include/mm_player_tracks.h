/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, YeJin Cho <cho.yejin@samsung.com>,
 * YoungHwan An <younghwan_.an@samsung.com>
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

#ifndef __MM_PLAYER_TRACKS_H__
#define	__MM_PLAYER_TRACKS_H__

#include "mm_player_priv.h"

#ifdef __cplusplus
	extern "C" {
#endif

#define DEFAULT_TRACK 0
#ifdef _MULTI_TRACK
typedef bool (*_mmplayer_track_selected_subtitle_language_cb)(int track_num, void *user_data);
#endif
void _mmplayer_track_initialize(mm_player_t *player);

void _mmplayer_track_destroy(mm_player_t *player);

void _mmplayer_track_update_info(mm_player_t *player, MMPlayerTrackType type, GstPad *sinkpad);

int _mmplayer_get_track_count(MMHandleType hplayer,  MMPlayerTrackType type, int *count);

int _mmplayer_select_track(MMHandleType hplayer, MMPlayerTrackType type, int index);
#ifdef _MULTI_TRACK
int _mmplayer_track_add_subtitle_language(MMHandleType hplayer, int index);

int _mmplayer_track_remove_subtitle_language(MMHandleType hplayer, int index);
#endif
int _mmplayer_get_track_language_code(MMHandleType hplayer, MMPlayerTrackType type, int index, char **code);

int _mmplayer_get_current_track(MMHandleType hplayer, MMPlayerTrackType type, int *index);
#ifdef _MULTI_TRACK
int _mmplayer_track_foreach_selected_subtitle_language(MMHandleType hplayer,_mmplayer_track_selected_subtitle_language_cb callback, void *user_data);
#endif
#ifdef __cplusplus
	}
#endif

#endif /* __MM_PLAYER_TRACKS_H__ */
