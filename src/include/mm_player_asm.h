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

#ifndef __MM_PLAYER_ASM_H__
#define __MM_PLAYER_ASM_H__

#include <glib.h>
#include <mm_types.h>

#include <mm_session.h>
#include <mm_session_private.h>
#include <audio-session-manager.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int handle;
	int pid;
	bool by_asm_cb;
	int antishock;
	int event_src;
	int skip_session;
	bool keep_last_pos;
	int user_route_policy;
	ASM_sound_states_t state;
	ASM_sound_events_t event;
	ASM_resource_t resource;
	bool exit_cb;
	bool cb_pending;
} MMPlayerASM;

gint _mmplayer_asm_register(MMPlayerASM* sm, ASM_sound_cb_t callback, void* param);
gint _mmplayer_asm_unregister(MMPlayerASM* sm);
gint _mmplayer_asm_set_state(MMHandleType player, ASM_sound_states_t state, gboolean enable_safety_vol);
gint _mmplayer_asm_ignore_session(MMHandleType player);
gint _mmplayer_asm_set_sound_resource(MMHandleType player, MMPlayerSoundResource mode);
ASM_cb_result_t __mmplayer_asm_callback(int handle, ASM_event_sources_t event_src, ASM_sound_commands_t command, unsigned int sound_status, void* cb_data);

#ifdef __cplusplus
}
#endif

#endif /* __MM_PLAYER_ASM_H__ */
