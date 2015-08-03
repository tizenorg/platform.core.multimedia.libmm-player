/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Heechul Jeon <heechul.jeon@samsung.com>
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

#ifndef __MM_PLAYER_SOUND_FOCUS_H__
#define __MM_PLAYER_SOUND_FOCUS_H__

#include <glib.h>
#include <mm_types.h>

#include <mm_session.h>
#include <mm_session_private.h>
#include <mm_sound_focus.h>
#include <mm_sound.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int focus_id;
	int watch_id;
	unsigned int subscribe_id;
	int pid;
	bool by_asm_cb;
	int antishock;
	bool keep_last_pos;
	int user_route_policy;
	bool exit_cb;
	bool cb_pending;
	bool acquired;
	int session_type;
	int session_flags;
	int focus_changed_msg;	// MMPlayerFocusChangedMsg

} MMPlayerSoundFocus;

gint _mmplayer_sound_register(MMPlayerSoundFocus* sound_focus, mm_sound_focus_changed_cb focus_cb, mm_sound_focus_changed_watch_cb watch_cb, void* param);
gint _mmplayer_sound_unregister(MMPlayerSoundFocus* sound_focus);
int _mmplayer_sound_acquire_focus(MMPlayerSoundFocus* sound_focus);
int _mmplayer_sound_release_focus(MMPlayerSoundFocus* sound_focus);

#ifdef __cplusplus
}
#endif

#endif /* __MM_PLAYER_SOUND_FOCUS_H__ */
