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
#include <mm_error.h>

#include <mm_session.h>
#include <mm_session_private.h>
#include <audio-session-manager.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int handle;
	int pid;
	int by_asm_cb;
	int event_src;
	ASM_sound_states_t state;
	ASM_sound_events_t event;
} MMPlayerASM;

/* returns allocated handle */
gint _mmplayer_asm_register(MMPlayerASM* sm, ASM_sound_cb_t callback, void* param);
gint _mmplayer_asm_deregister(MMPlayerASM* sm);
gint _mmplayer_asm_set_state(MMHandleType player, ASM_sound_states_t state);

#ifdef __cplusplus
}
#endif

#endif /* __MM_PLAYER_ASM_H__ */
