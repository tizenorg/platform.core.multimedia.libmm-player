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
#include <vconf.h>
#include <vconf-internal-sound-keys.h>

#include "mm_player_utils.h"
#include "mm_player_priv.h"
#include "mm_player_sound_focus.h"

#define MMPLAYER_CHECK_SOUND_FOCUS_INSTANCE(x_player_sound_focus) \
do \
{ \
	if (!x_player_sound_focus) \
	{ \
		debug_log("no sound focus instance");\
		return MM_ERROR_SOUND_NOT_INITIALIZED; \
	} \
}while(0);

void __mmplayer_sound_signal_callback (mm_sound_signal_name_t signal, int value, void *user_data)
{
	MMPlayerSoundFocus *sound_focus = (MMPlayerSoundFocus*)user_data;

	debug_log("sound signal callback %d / %d", signal, value);

	if (signal == MM_SOUND_SIGNAL_RELEASE_INTERNAL_FOCUS)
	{
		if (value == 1)
		{
			if (sound_focus->watch_id > 0)
			{
				debug_log("unset the focus watch cb");

				mm_sound_unset_focus_watch_callback(sound_focus->watch_id);
				sound_focus->watch_id = 0;
				/*
				if (sound_focus->subscribe_id > 0)
					mm_sound_unsubscribe_signal(sound_focus->subscribe_id);
				*/
			}
		}
	}
}

const gchar *
__mmplayer_sound_get_stream_type(gint type)
{
	switch ( type )
	{
		case MM_SESSION_TYPE_CALL:
		case MM_SESSION_TYPE_VIDEOCALL:
		case MM_SESSION_TYPE_VOIP:
			return "ringtone-voip";
		case MM_SESSION_TYPE_MEDIA:
			return "media";
		case MM_SESSION_TYPE_NOTIFY:
			return "notification";
		case MM_SESSION_TYPE_ALARM:
			return "alarm";
		case MM_SESSION_TYPE_EMERGENCY:
			return "emergency";
		default:
			debug_warning("unexpected case!\n");
			return "media";
	}

	return "media";
}

int
_mmplayer_sound_acquire_focus(MMPlayerSoundFocus* sound_focus)
{
	int ret = MM_ERROR_NONE;
	const gchar *stream_type = NULL;

	MMPLAYER_FENTER();
	MMPLAYER_CHECK_SOUND_FOCUS_INSTANCE(sound_focus);

	if (sound_focus->acquired)
	{
		debug_warning("focus is already acquired. can't acquire again.");
		return MM_ERROR_NONE;
	}

	stream_type = __mmplayer_sound_get_stream_type(sound_focus->session_type);

	if ((!strstr(stream_type, "media")) ||
		(sound_focus->session_flags & MM_SESSION_OPTION_PAUSE_OTHERS))
	{

		ret = mm_sound_acquire_focus(sound_focus->focus_id, FOCUS_FOR_BOTH, NULL);
		if (ret != MM_ERROR_NONE)
		{
			debug_error("failed to acquire sound focus\n");
			return ret;
		}

		sound_focus->acquired = TRUE;
	}

	MMPLAYER_FLEAVE();
	return ret;
}

int
_mmplayer_sound_release_focus(MMPlayerSoundFocus* sound_focus)
{
	int ret = MM_ERROR_NONE;
	const gchar *stream_type = NULL;

	MMPLAYER_FENTER();
	MMPLAYER_CHECK_SOUND_FOCUS_INSTANCE(sound_focus);

	if (!sound_focus->acquired)
	{
		debug_warning("focus is not acquired. no need to release.");
		return MM_ERROR_NONE;
	}

	stream_type = __mmplayer_sound_get_stream_type(sound_focus->session_type);

	if ((!strstr(stream_type, "media")) ||
		(sound_focus->session_flags & MM_SESSION_OPTION_PAUSE_OTHERS))
	{
		ret = mm_sound_release_focus(sound_focus->focus_id, FOCUS_FOR_BOTH, NULL);
		if (ret != MM_ERROR_NONE)
		{
			debug_error("failed to release sound focus\n");
			return ret;
		}

		sound_focus->acquired = FALSE;
	}

	MMPLAYER_FLEAVE();
	return MM_ERROR_NONE;
}

gint
_mmplayer_sound_register(MMPlayerSoundFocus* sound_focus,
		mm_sound_focus_changed_cb focus_cb, mm_sound_focus_changed_watch_cb watch_cb, void* param)
{
	gint pid = -1;
	gint ret = MM_ERROR_NONE;
	const gchar *stream_type = NULL;

	MMPLAYER_FENTER();
	MMPLAYER_CHECK_SOUND_FOCUS_INSTANCE(sound_focus);

	/* check if it's running on the media_server */
	if (sound_focus->pid > 0)
	{
		pid = sound_focus->pid;
		debug_log("mm-player is running on different process. Just faking pid to [%d]. :-p\n", pid);
	}

	/* read session information */
	ret = _mm_session_util_read_information(pid, &sound_focus->session_type, &sound_focus->session_flags);
	if (ret != MM_ERROR_NONE)
	{
		debug_error("Read Session Type failed. ret:0x%X \n", ret);

		if (ret == MM_ERROR_INVALID_HANDLE)
		{
			int sig_value = 0;

			mm_sound_get_signal_value(MM_SOUND_SIGNAL_RELEASE_INTERNAL_FOCUS, &sig_value);
			debug_warning("internal focus signal value=%d, id=%d\n", sig_value, sound_focus->subscribe_id);

			if ((sig_value == 0) && (sound_focus->subscribe_id == 0))
			{
				ret = mm_sound_subscribe_signal(MM_SOUND_SIGNAL_RELEASE_INTERNAL_FOCUS, &sound_focus->subscribe_id,
										(mm_sound_signal_callback)__mmplayer_sound_signal_callback, (void*)sound_focus);
				if (ret != MM_ERROR_NONE)
				{
					debug_error("mm_sound_subscribe_signal is failed\n");
					return MM_ERROR_POLICY_BLOCKED;
				}

				debug_log("register focus watch callback for the value is 0, sub_cb id %d\n", sound_focus->subscribe_id);

				ret = mm_sound_set_focus_watch_callback(FOCUS_FOR_BOTH, watch_cb, (void*)param, &sound_focus->watch_id);
				if (ret != MM_ERROR_NONE)
				{
					debug_error("mm_sound_set_focus_watch_callback is failed\n");
					return MM_ERROR_POLICY_BLOCKED;
				}
			}

			return MM_ERROR_NONE;
		}
		else
		{
			return MM_ERROR_POLICY_BLOCKED;
		}
	}

	/* interpret session information */
	stream_type = __mmplayer_sound_get_stream_type(sound_focus->session_type);
	debug_log("fid [%d] wid [%d] type[%s], flags[0x%02X]\n",
		sound_focus->focus_id, sound_focus->watch_id, stream_type, sound_focus->session_flags);

	if (sound_focus->focus_id == 0)
	{
		/* get unique id */
		ret = mm_sound_focus_get_id(&sound_focus->focus_id);
		if (ret != MM_ERROR_NONE)
		{
			debug_error("failed to get unique focus id\n");
			return MM_ERROR_POLICY_BLOCKED;
		}

		/* register sound focus callback */
		ret = mm_sound_register_focus(sound_focus->focus_id, stream_type, focus_cb, (void*)param);
		if (ret != MM_ERROR_NONE)
		{
			debug_error("mm_sound_register_focus is failed\n");
			return MM_ERROR_POLICY_BLOCKED;
		}
	}

	if ((sound_focus->watch_id == 0) &&
		(strstr(stream_type, "media")) &&
		!(sound_focus->session_flags & MM_SESSION_OPTION_PAUSE_OTHERS) &&
		!(sound_focus->session_flags & MM_SESSION_OPTION_UNINTERRUPTIBLE))
	{
		debug_log("register focus watch callback\n");

		ret = mm_sound_set_focus_watch_callback(FOCUS_FOR_BOTH, watch_cb, (void*)param, &sound_focus->watch_id);
		if (ret != MM_ERROR_NONE)
		{
			debug_error("mm_sound_set_focus_watch_callback is failed\n");
			return MM_ERROR_POLICY_BLOCKED;
		}
	}

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

gint
_mmplayer_sound_unregister(MMPlayerSoundFocus* sound_focus)
{
	MMPLAYER_FENTER();

	MMPLAYER_CHECK_SOUND_FOCUS_INSTANCE(sound_focus);

	debug_log("unregister sound focus callback\n");

	if (sound_focus->focus_id > 0)
	{
		mm_sound_unregister_focus(sound_focus->focus_id);
		sound_focus->focus_id = 0;
	}

	if (sound_focus->watch_id > 0)
	{
		mm_sound_unset_focus_watch_callback(sound_focus->watch_id);
		sound_focus->watch_id = 0;
	}

	if (sound_focus->subscribe_id > 0)
	{
		mm_sound_unsubscribe_signal(sound_focus->subscribe_id);
		sound_focus->subscribe_id = 0;
	}

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

