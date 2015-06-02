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

#include <mm_debug.h>
#include <mm_error.h>
#include "mm_player_utils.h"
#include "mm_player_priv.h"
#include "mm_player_asm.h"
#include <vconf.h>
#include <vconf-internal-sound-keys.h>

#define MMPLAYER_CHECK_SESSION_SKIP(x_player_asm) \
do \
{ \
	if (x_player_asm->skip_session == TRUE) \
	{ \
		debug_log("skip session"); \
		return MM_ERROR_NONE; \
	} \
}while(0);

#define MMPLAYER_CHECK_SESSION_INSTANCE(x_player_asm) \
do \
{ \
	if (!x_player_asm) \
	{ \
		debug_log("no session instance");\
		return MM_ERROR_SOUND_NOT_INITIALIZED; \
	} \
}while(0);

static ASM_sound_events_t __mmplayer_asm_get_event_type(gint type);

const gchar *
__mmplayer_asm_get_state_name ( int state )
{
	switch ( state )
	{
		case ASM_STATE_NONE:
			return "NONE";
		case ASM_STATE_PLAYING:
			return "PLAYING";
		case ASM_STATE_PAUSE:
			return "PAUSED";
		case ASM_STATE_STOP:
			return "STOP";
		case ASM_STATE_WAITING:
			return "BUFFERING";
		default:
			return "INVAID";
	}
}

#define MMPLAYER_ASM_STATE_GET_NAME(state)      __mmplayer_asm_get_state_name(state)

gint
_mmplayer_asm_register(MMPlayerASM* sm, ASM_sound_cb_t callback, void* param)
{
	return MM_ERROR_NONE;

	gint session_type = MM_SESSION_TYPE_MEDIA;
	gint session_options = 0;
	gint errorcode = MM_ERROR_NONE;
	gint asm_handle = -1;
	gint event_type = ASM_EVENT_NONE;
	gint pid = -1;

	MMPLAYER_FENTER();

	MMPLAYER_CHECK_SESSION_INSTANCE(sm);
	MMPLAYER_CHECK_SESSION_SKIP(sm);

	/* check if it's running on the media_server */
	if ( sm->pid > 0 )
	{
		pid = sm->pid;
		debug_log("mm-player is running on different process. Just faking pid to [%d]. :-p\n", pid);
	}

	/* read session information */
	errorcode = _mm_session_util_read_information(pid, &session_type, &session_options);
	if ( errorcode )
	{
		debug_warning("Read Session Type failed. use default \"media\" type\n");
		session_type = MM_SESSION_TYPE_MEDIA;
	}

	/* interpret session information */
	event_type = __mmplayer_asm_get_event_type(session_type);

	/* check if it's one of CALL series */
	if ( event_type == ASM_EVENT_CALL ||
		event_type == ASM_EVENT_VIDEOCALL ||
		event_type == ASM_EVENT_VOIP)
	{
		debug_warning("session type is one of CALL series(%d), skip registering ASM\n", session_type);
		sm->event = event_type;
		sm->skip_session = TRUE;
		return MM_ERROR_NONE;
	}

	/* register audio-session-manager handle and callback */
	if( ! ASM_register_sound(pid, &asm_handle, event_type, ASM_STATE_NONE, callback, (void*)param, ASM_RESOURCE_NONE, &errorcode))
	{
		debug_error("ASM_register_sound() failed, error(%x)\n", errorcode);
		return errorcode;
	}
	/* set session options */
	if (session_options)
	{
		if( ! ASM_set_session_option(asm_handle, session_options, &errorcode))
		{
			debug_error("ASM_set_session_options() failed, error(%x)\n", errorcode);
			return errorcode;
		}
	}

	/* now succeed to register our callback. take result */
	sm->handle = asm_handle;
	sm->state = ASM_STATE_NONE;
	sm->event = event_type;

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

gint
_mmplayer_asm_unregister(MMPlayerASM* sm)
{
	gint event_type = ASM_EVENT_NONE;
	gint errorcode = 0;
	gint pid = -1;

	MMPLAYER_FENTER();

	MMPLAYER_CHECK_SESSION_INSTANCE(sm);
	MMPLAYER_CHECK_SESSION_SKIP(sm);

	/* check if it's running on the media_server */
	if ( sm->pid > 0 )
	{
		pid = sm->pid;
		debug_log("mm-player is running on different process. Just faking pid to [%d]. :-p\n", pid);
	}

	event_type = sm->event;

	if (sm->handle)
	{
		if( ! ASM_unregister_sound( sm->handle, event_type, &errorcode) )
		{
			debug_error("Unregister sound failed 0x%X\n", errorcode);
			return MM_ERROR_POLICY_INTERNAL;
		}
	}
	debug_warning("asm unregistered");

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

gint _mmplayer_asm_set_state(MMHandleType hplayer, ASM_sound_states_t state, gboolean enable_safety_vol)
{
	return MM_ERROR_NONE;

	gint event_type = ASM_EVENT_NONE;
	gint pid = -1;
//	int vconf_safety_vol_val = 0;
	ASM_resource_t resource = ASM_RESOURCE_NONE;
	mm_player_t *player = (mm_player_t *)hplayer;
	MMPlayerASM* sm	 = &player->sm;
	MMPLAYER_FENTER();

#if 0
	if (player->set_mode.safety_volume)
	{
		/* get safety volume */
		if (vconf_get_int(VCONFKEY_SOUND_ENABLE_SAFETY_VOL, &vconf_safety_vol_val))
		{
			debug_error ("failed to get safety volume");
		}

		if (enable_safety_vol)
		{
			vconf_safety_vol_val = vconf_safety_vol_val | VCONFKEY_SOUND_SAFETY_VOL_FW_MMPLAYER;
		}
		else
		{
			vconf_safety_vol_val = vconf_safety_vol_val & ~VCONFKEY_SOUND_SAFETY_VOL_FW_MMPLAYER;
		}

		/* set safety volume */
		if (vconf_set_int(VCONFKEY_SOUND_ENABLE_SAFETY_VOL, vconf_safety_vol_val))
		{
			debug_error ("failed to set safety volume");
		}
		debug_log("safety vol : %d(0:false, 1:true), current result of vconf val : 0x%x", enable_safety_vol, vconf_safety_vol_val);
	}
#endif
	MMPLAYER_CHECK_SESSION_INSTANCE(sm);
	MMPLAYER_CHECK_SESSION_SKIP(sm);

	/* check if it's running on the media_server */
	if ( sm->pid > 0 )
	{
		pid = sm->pid;
		debug_log("mm-player is running on different process. Just faking pid to [%d]. :-p\n", pid);
	}

	/* in case of stop, it should be stop first and post interrupt message to application */
	if ( !sm->by_asm_cb || state == ASM_STATE_STOP)//|| sm->state == ASM_STATE_PLAYING )
	{
		int ret = 0;
		event_type = sm->event;
		resource = sm->resource;

		/* check if there is video */
		/* NOTE: resource can be set as NONE when it's not occupied or unknown resource is used. */
		if(ASM_STATE_PLAYING == state || ASM_STATE_PAUSE == state || ASM_STATE_WAITING == state)
		{
			int surface_type = 0;
			mm_attrs_get_int_by_name (player->attrs, "display_surface_type", &surface_type);
			debug_log("surface type = %d", surface_type);

			if (player->pipeline && player->pipeline->videobin)
			{
				if(surface_type == MM_DISPLAY_SURFACE_X)
				{
					resource = ASM_RESOURCE_VIDEO_OVERLAY | ASM_RESOURCE_HW_DECODER;
				}
				else if (surface_type == MM_DISPLAY_SURFACE_EVAS)
				{
					resource = ASM_RESOURCE_HW_DECODER;
				}
			}

			if( __mmplayer_is_streaming (player))
				resource = resource | ASM_RESOURCE_STREAMING;

			/* reset */
			sm->keep_last_pos = FALSE;
		}

		if( ((sm->state != state) || (sm->resource != resource)) && ! ASM_set_sound_state( sm->handle, event_type, state, resource, &ret) )
		{
			gint retval = MM_ERROR_POLICY_INTERNAL;

			debug_error("Set state to [%d] failed 0x%X\n", state, ret);
			switch(ret)
			{
				case ERR_ASM_POLICY_CANNOT_PLAY:
				case ERR_ASM_POLICY_CANNOT_PLAY_BY_CALL:
				case ERR_ASM_POLICY_CANNOT_PLAY_BY_ALARM:
					retval = MM_ERROR_POLICY_BLOCKED;
					break;
				default:
					retval = MM_ERROR_POLICY_INTERNAL;
					break;
			}

			return retval;
		}
	}
	else
	{
		sm->by_asm_cb = FALSE;
	}

	sm->state = state;

	/* waiting to be changed because callback can be called */
	if (ASM_STATE_STOP == state)
	{
		ASM_sound_states_t session_state;
		ASM_get_sound_state(sm->handle, event_type, &session_state, NULL);
	}

	debug_error("ASM state changed to [%s]", MMPLAYER_ASM_STATE_GET_NAME(state));
	MMPLAYER_FLEAVE();
	return MM_ERROR_NONE;
}

static ASM_sound_events_t
__mmplayer_asm_get_event_type(gint type)
{
	gint event_type = ASM_EVENT_NONE;

	/* interpret session type */
	switch(type)
	{
		case MM_SESSION_TYPE_CALL:
			event_type = ASM_EVENT_CALL;
			break;

		case MM_SESSION_TYPE_VIDEOCALL:
			event_type = ASM_EVENT_VIDEOCALL;
			break;

		case MM_SESSION_TYPE_VOIP:
			event_type = ASM_EVENT_VOIP;
			break;

		case MM_SESSION_TYPE_MEDIA:
//		case MM_SESSION_TYPE_MEDIA_RECORD:
			event_type = ASM_EVENT_MEDIA_MMPLAYER;
			break;

		case MM_SESSION_TYPE_NOTIFY:
			event_type = ASM_EVENT_NOTIFY;
			break;

		case MM_SESSION_TYPE_ALARM:
			event_type = ASM_EVENT_ALARM;
			break;

		case MM_SESSION_TYPE_EMERGENCY:
			event_type = ASM_EVENT_EMERGENCY;
			break;
#if 0
		case MM_SESSION_TYPE_RECORD_VIDEO:
		case MM_SESSION_TYPE_RECORD_AUDIO:
			event_type = ASM_EVENT_MEDIA_MMPLAYER;
			break;
#endif

		default:
			debug_msg("unexpected case!\n");
			event_type = ASM_EVENT_MEDIA_MMPLAYER;
			break;
	}

	return event_type;
}

gint
_mmplayer_asm_ignore_session(MMHandleType hplayer)
{
	mm_player_t *player = (mm_player_t *)hplayer;

	MMPLAYER_FENTER();

	return_val_if_fail (player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	/* check state */
	if (player->state != MM_PLAYER_STATE_NULL)
	{
		debug_log("invalid state to make session mix");
		return MM_ERROR_PLAYER_INVALID_STATE;
	}

	if (player->sm.skip_session == FALSE && player->sm.handle)
	{
		int error_code = 0;

		if (!ASM_unregister_sound(player->sm.handle, player->sm.event, &error_code))
		{
			debug_error("Unregister sound failed 0x%X", error_code);
			return MM_ERROR_POLICY_INTERNAL;
		}
		player->sm.skip_session = TRUE;
		player->sm.handle = 0;

		debug_log("session skip enabled");
	}

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}
