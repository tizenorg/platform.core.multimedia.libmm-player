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

#include <glib.h>
#include <mm_debug.h>
#include "mm_player_priv.h"
#include "mm_player_asm.h"

static ASM_sound_events_t __mmplayer_asm_get_event_type(gint type);

gint mmplayer_asm_register(MMPlayerASM* sm, ASM_sound_cb_t callback, void* param)
{
	/* read mm-session type */
	gint sessionType = MM_SESSION_TYPE_SHARE;
	gint errorcode = MM_ERROR_NONE;
	gint asm_handle = -1;
	gint event_type = ASM_EVENT_NONE;
	gint pid = -1;

	debug_log("\n");

	if ( ! sm )
	{
		debug_error("invalid session handle\n");
		return MM_ERROR_PLAYER_NOT_INITIALIZED;
	}

	/* check if it's running on the media_server */
	if ( sm->pid > 0 )
	{
		pid = sm->pid;
		debug_log("mm-player is running on different process. Just faking pid to [%d]. :-p\n", pid);
	}
	else
	{
		debug_log("no pid has assigned. using default(current) context\n");
	}

	/* read session type */
	errorcode = _mm_session_util_read_type(pid, &sessionType);
	if ( errorcode )
	{
		debug_warning("Read MMSession Type failed. use default \"share\" type\n");
		sessionType = MM_SESSION_TYPE_SHARE;

		/* init session */
		errorcode = mm_session_init(sessionType);
		if ( errorcode )
		{
				debug_critical("mm_session_init() failed\n");
				return errorcode;
		}
	}

	/* check if it's CALL */
	if ( sessionType == MM_SESSION_TYPE_CALL )
	{
		debug_log("session type is CALL\n");
		sm->event = ASM_EVENT_CALL;
		return MM_ERROR_NONE;
	}
	else if ( sessionType == MM_SESSION_TYPE_VIDEOCALL )
	{
		debug_log("session type is VIDEOCALL\n");
		sm->event = ASM_EVENT_VIDEOCALL;
		return MM_ERROR_NONE;
	}

	/* interpret session type */
	event_type = __mmplayer_asm_get_event_type(sessionType);



	/* register audio-session-manager callback */
	if( ! ASM_register_sound(pid, &asm_handle, event_type, ASM_STATE_NONE, callback, (void*)param, ASM_RESOURCE_NONE, &errorcode))
	{
		debug_critical("ASM_register_sound() failed\n");
		return errorcode;
	}

	/* now succeed to register our callback. take result */
	sm->handle = asm_handle;
	sm->state = ASM_STATE_NONE;
	sm->event = event_type;

	return MM_ERROR_NONE;
}

gint mmplayer_asm_deregister(MMPlayerASM* sm)
{
	gint event_type = ASM_EVENT_NONE;
	gint errorcode = 0;
	gint pid = -1;

	if ( ! sm )
	{
		debug_error("invalid session handle\n");
		return MM_ERROR_PLAYER_NOT_INITIALIZED;
	}

	/* check if it's running on the media_server */
	if ( sm->pid > 0 )
	{
		pid = sm->pid;
		debug_log("mm-player is running on different process. Just faking pid to [%d]. :-p\n", pid);
	}
	else
	{
		debug_log("no pid has assigned. using default(current) context\n");
	}
	/* check if it's CALL */
	if(sm->event == ASM_EVENT_CALL || sm->event == ASM_EVENT_VIDEOCALL)
	{
		debug_log("session type is VOICE or VIDEO CALL (%d)\n", sm->event); 
		return MM_ERROR_NONE;
	}
	event_type = sm->event;

	if( ! ASM_unregister_sound( sm->handle, event_type, &errorcode) )
	{
		debug_error("Unregister sound failed 0x%X\n", errorcode);
		return MM_ERROR_POLICY_INTERNAL;
	}

	return MM_ERROR_NONE;
}

gint mmplayer_asm_set_state(MMHandleType hplayer, ASM_sound_states_t state)
{
	gint event_type = ASM_EVENT_NONE;
	gint pid = -1;
	ASM_resource_t resource = ASM_RESOURCE_NONE;
	mm_player_t *player = (mm_player_t *)hplayer;
	MMPlayerASM* sm	 = &player->sm;

	if ( ! sm )
	{
		debug_error("invalid session handle\n");
		return MM_ERROR_PLAYER_NOT_INITIALIZED;
	}

	/* check if it's running on the media_server */
	if ( sm->pid > 0 )
	{
		pid = sm->pid;
		debug_log("mm-player is running on different process. Just faking pid to [%d]. :-p\n", pid);
	}
	else
	{
		debug_log("no pid has assigned. using default(current) context\n");
	}
	/* check if it's CALL */
	if(sm->event == ASM_EVENT_CALL || sm->event == ASM_EVENT_VIDEOCALL)
	{
		debug_log("session type is VOICE or VIDEO CALL (%d)\n", sm->event); 
		return MM_ERROR_NONE;
	}

	if ( ! sm->by_asm_cb )
	{
		int ret = 0;

		event_type = sm->event;
		/* check if there is video */
		/* NOTE: resource can be set as NONE when it's not occupied or unknown resource is used. */
		if(ASM_STATE_PLAYING == state || ASM_STATE_PAUSE == state)
		{
			if(player->pipeline && player->pipeline->videobin)
			resource = ASM_RESOURCE_VIDEO_OVERLAY | ASM_RESOURCE_HW_DECODER;
		}

		if( ! ASM_set_sound_state( sm->handle, event_type, state, resource, &ret) )
		{
			debug_error("Set state to [%d] failed 0x%X\n", state, ret);
			return MM_ERROR_POLICY_BLOCKED;
		}

		sm->state = state;
	}
	else
	{
		sm->by_asm_cb = 0;
		sm->state = state;
	}

	return MM_ERROR_NONE;
}


ASM_sound_events_t __mmplayer_asm_get_event_type(gint type)
{
	gint event_type = ASM_EVENT_NONE;

	/* interpret session type */
		switch(type)
		{
			case MM_SESSION_TYPE_SHARE:
				event_type = ASM_EVENT_SHARE_MMPLAYER;
			break;

			case MM_SESSION_TYPE_EXCLUSIVE:
				event_type = ASM_EVENT_EXCLUSIVE_MMPLAYER;
			break;

			case MM_SESSION_TYPE_NOTIFY:
				event_type = ASM_EVENT_NOTIFY;
			break;
			
			case MM_SESSION_TYPE_ALARM:
				event_type = ASM_EVENT_ALARM;
			break;
			default:
				debug_critical("unexpected case!\n");
				g_assert(0);
		}

		return event_type;
}
