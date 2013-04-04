/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>,YeJin Cho <cho.yejin@samsung.com>, YoungHwan An <younghwan_.an@samsung.com>
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
#include <mm_error.h>

#include "mm_player_audioeffect.h"
#include "mm_player_ini.h"
#include "mm_player_priv.h"
#include <mm_sound.h>


int
mm_player_get_foreach_present_supported_effect_type(MMHandleType player, MMAudioEffectType effect_type, mmplayer_supported_audio_effect_cb foreach_cb, void *user_data)
{
	int result = MM_ERROR_NONE;
	mm_sound_device_in device_in;
	mm_sound_device_out device_out;
	int i = 0;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* get status if speaker is activated */
	result = mm_sound_get_active_device(&device_in, &device_out);
	if ( result ) {
		debug_fleave();
		debug_error("mm_sound_get_active_device() failed [%x]!!", result);
		return result;
	}

	/* preset */
	if (effect_type == MM_AUDIO_EFFECT_TYPE_PRESET)
	{
		for ( i = 0; i < MM_AUDIO_EFFECT_PRESET_NUM; i++ )
		{
			if (PLAYER_INI()->audio_effect_preset_list[i] )
			{
				if (device_out == MM_SOUND_DEVICE_OUT_SPEAKER &&
					PLAYER_INI()->audio_effect_preset_earphone_only_list[i])
				{
					continue;
				}
				if (!foreach_cb(effect_type,i, user_data))
				{
					goto CALLBACK_ERROR;
				}
			}
		}
	}
	/* custom */
	else if (effect_type == MM_AUDIO_EFFECT_TYPE_CUSTOM)
	{
		for ( i = 0; i < MM_AUDIO_EFFECT_CUSTOM_NUM; i++ )
		{
			if (PLAYER_INI()->audio_effect_custom_list[i] )
			{
				if (device_out == MM_SOUND_DEVICE_OUT_SPEAKER &&
					PLAYER_INI()->audio_effect_custom_earphone_only_list[i])
				{
					continue;
				}
				if (!foreach_cb(effect_type,i, user_data))
				{
					goto CALLBACK_ERROR;
				}
			}
		}
	}
	else
	{
		debug_error("invalid effect type(%d)", effect_type);
		result = MM_ERROR_INVALID_ARGUMENT;
	}

	debug_fleave();

	return result;

CALLBACK_ERROR:
	debug_error("foreach callback returned error");
	debug_fleave();
	return MM_ERROR_PLAYER_INTERNAL;
}


int
__mmplayer_set_harmony_effect(mm_player_t *player, GstElement *audio_effect_element)
{
	gint *ext_effect_level_list = NULL;
	int count = 1;		/* start from 1, because of excepting eq index */
	int ext_level_index = 0;
	int result = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( audio_effect_element, MM_ERROR_INVALID_ARGUMENT );

	/* Custom EQ */
	if( PLAYER_INI()->audio_effect_custom_eq_band_num )
	{
		debug_log("pass custom EQ level list to audio effect plugin");
		/* set custom-equalizer level list */
		g_object_set(audio_effect_element, "custom-eq", player->audio_effect_info.custom_eq_level, NULL);
	}
	else
	{
		debug_warning("no custom EQ");
	}

	/* Custom Extension effects */
	if( PLAYER_INI()->audio_effect_custom_ext_num )
	{
		debug_log("pass custom extension level list to audio effect plugin");
		ext_effect_level_list = player->audio_effect_info.custom_ext_level_for_plugin;
		if (!ext_effect_level_list) {
			ext_effect_level_list = (gint*) malloc (sizeof(gint)*PLAYER_INI()->audio_effect_custom_ext_num);
			if (!ext_effect_level_list)
			{
				debug_error("memory allocation for extension effect list failed");
				return MM_ERROR_OUT_OF_MEMORY;
			}
			else
			{
				memset (ext_effect_level_list, 0, PLAYER_INI()->audio_effect_custom_ext_num);
			}
		}

		while ( count < MM_AUDIO_EFFECT_CUSTOM_NUM )
		{
			if ( PLAYER_INI()->audio_effect_custom_list[count] )
			{
				ext_effect_level_list[ext_level_index] = player->audio_effect_info.custom_ext_level[count-1];
				ext_level_index++;
				if (ext_level_index == PLAYER_INI()->audio_effect_custom_ext_num)
				{
					break;
				}
			}
			count++;
		}

		/* set custom-extension effects level list */
		g_object_set(audio_effect_element, "custom-ext", ext_effect_level_list, NULL);
	}
	else
	{
		debug_warning("no custom extension effect");
	}

	/* order action to audio effect plugin */
	g_object_set(audio_effect_element, "filter-action", MM_AUDIO_EFFECT_TYPE_CUSTOM, NULL);
	debug_log("filter-action = %d", MM_AUDIO_EFFECT_TYPE_CUSTOM);

	debug_fleave();

	return result;
}


gboolean
__mmplayer_is_earphone_only_effect_type(mm_player_t *player, MMAudioEffectType effect_type, int effect)
{
	gboolean result = FALSE;
	int i = 0;

	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	/* preset */
	if (effect_type == MM_AUDIO_EFFECT_TYPE_PRESET)
	{
		if (PLAYER_INI()->audio_effect_preset_earphone_only_list[effect])
		{
			debug_msg("this preset effect(%d) is only available with earphone", effect);
			result = TRUE;
		}
	}
	/* custom */
	else if (effect_type == MM_AUDIO_EFFECT_TYPE_CUSTOM)
	{
		for (i = 1; i < MM_AUDIO_EFFECT_CUSTOM_NUM; i++) /* it starts from 1(except testing for EQ) */
		{
			if (PLAYER_INI()->audio_effect_custom_earphone_only_list[i])
			{
				/* check if the earphone only custom effect was set */
				if (player->audio_effect_info.custom_ext_level[i-1])
				{
					debug_msg("this custom effect(%d) is only available with earphone", i);
					result = TRUE;
				}
			}
		}
	}
	else
	{
		debug_error("invalid effect type(%d)", effect_type);
	}

	debug_fleave();

	return result;
}


gboolean
_mmplayer_is_supported_effect_type(MMAudioEffectType effect_type, int effect)
{
	gboolean result = TRUE;
	mm_sound_device_in device_in;
	mm_sound_device_out device_out;
	int ret = MM_ERROR_NONE;

	debug_fenter();

	/* get status if speaker is activated */
	ret = mm_sound_get_active_device(&device_in, &device_out);
	if ( ret ) {
		debug_fleave();
		debug_error("mm_sound_get_active_device() failed [%x]!!", ret);
		result = FALSE;
	}
	else
	{
		/* preset */
		if (effect_type == MM_AUDIO_EFFECT_TYPE_PRESET)
		{
			if ( effect < MM_AUDIO_EFFECT_PRESET_AUTO || effect >= MM_AUDIO_EFFECT_PRESET_NUM )
			{
				debug_error("out of range, preset effect(%d)", effect);
				result = FALSE;
			}
			if (!PLAYER_INI()->audio_effect_preset_list[effect])
			{
				debug_error("this effect(%d) is not supported", effect);
				result = FALSE;
			}
			else
			{
				if (device_out == MM_SOUND_DEVICE_OUT_SPEAKER &&
						PLAYER_INI()->audio_effect_preset_earphone_only_list[effect])
				{
					result = FALSE;
				}
			}
		}
		/* custom */
		else if (effect_type == MM_AUDIO_EFFECT_TYPE_CUSTOM)
		{
			if ( effect < MM_AUDIO_EFFECT_CUSTOM_EQ || effect >= MM_AUDIO_EFFECT_CUSTOM_NUM )
			{
				debug_error("out of range, custom effect(%d)", effect);
				result = FALSE;
			}
			if (!PLAYER_INI()->audio_effect_custom_list[effect])
			{
				debug_error("this custom effect(%d) is not supported", effect);
				result = FALSE;
			}
			else
			{
				if (device_out == MM_SOUND_DEVICE_OUT_SPEAKER &&
						PLAYER_INI()->audio_effect_custom_earphone_only_list[effect])
				{
					result = FALSE;
				}
			}
		}
		else
		{
			debug_error("invalid effect type(%d)", effect_type);
			result = FALSE;
		}
	}

	debug_fleave();

	return result;
}


int
_mmplayer_audio_effect_preset_apply(mm_player_t *player, MMAudioEffectPresetType effect_type)
{
	GstElement *audio_effect_element = NULL;
	int result = MM_ERROR_NONE;
	int output_type = 0;
	mm_sound_device_in device_in;
	mm_sound_device_out device_out;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* Music Player can set audio effect value before Audiobin is created. */
	if ( !player->pipeline || !player->pipeline->audiobin )
	{
		debug_warning("effect element is not created yet.");

		player->bypass_audio_effect = FALSE;

		/* store audio effect setting in order to apply it when audio effect plugin is created */
		player->audio_effect_info.effect_type = MM_AUDIO_EFFECT_TYPE_PRESET;
		player->audio_effect_info.preset = effect_type;
	}
	else
	{
		return_val_if_fail( player->pipeline->audiobin, MM_ERROR_PLAYER_NOT_INITIALIZED );

		audio_effect_element = player->pipeline->audiobin[MMPLAYER_A_FILTER].gst;

		/* get status if speaker is activated */
		result = mm_sound_get_active_device(&device_in, &device_out);
		if ( result ) {
			debug_error("mm_sound_get_active_device() failed [%x]!!", result);
			debug_fleave();
			return result;
		}

		/* SPEAKER case */
		if (device_out == MM_SOUND_DEVICE_OUT_SPEAKER)
		{
			output_type = MM_AUDIO_EFFECT_OUTPUT_SPK;
			if (__mmplayer_is_earphone_only_effect_type(player, MM_AUDIO_EFFECT_TYPE_PRESET, effect_type))
			{
				debug_error("earphone is not equipped, this filter will not be applied");
				debug_fleave();
				return MM_ERROR_PLAYER_SOUND_EFFECT_INVALID_STATUS;
			}
		}
		/* Other case, include WIRED_ACCESSORY, BLUETOOTH, DOCK */
		else
		{
			output_type = MM_AUDIO_EFFECT_OUTPUT_EAR;
		}

		/* set filter output mode as SPEAKER or EARPHONE */
		g_object_set(audio_effect_element, "filter-output-mode", output_type, NULL);
		debug_log("filter-output-mode = %d (0:spk,1:ear)", output_type);

		if (effect_type == MM_AUDIO_EFFECT_PRESET_AUTO) {
			/* TODO: Add codes about auto selecting preset mode according to ID3 tag */
			/* set effect preset mode */
			g_object_set(audio_effect_element, "preset-mode", 0, NULL); /* forced set to 0(normal) temporarily */
			debug_log("preset-mode = %d", effect_type);

		} else {
			/* set effect preset mode */
			g_object_set(audio_effect_element, "preset-mode", effect_type-1, NULL); /* effect_type-1, because of _PRESET_AUTO in MSL/CAPI which does not exist in soundAlive plugin */
			debug_log("preset-mode = %d", effect_type);
		}

		/* order action to audio effect plugin */
		g_object_set(audio_effect_element, "filter-action", MM_AUDIO_EFFECT_TYPE_PRESET, NULL);
		debug_log("filter-action = %d", MM_AUDIO_EFFECT_TYPE_PRESET);

	}

	debug_fleave();

	return result;
}


int
_mmplayer_audio_effect_custom_apply(mm_player_t *player)
{
	GstElement *audio_effect_element = NULL;
	int result = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* Music Player can set audio effect value before Audiobin is created. */
	if ( !player->pipeline || !player->pipeline->audiobin )
	{
		debug_warning("effect element is not created yet.");

		player->bypass_audio_effect = FALSE;

		/* store audio effect setting in order to apply it when audio effect plugin is created */
		player->audio_effect_info.effect_type = MM_AUDIO_EFFECT_TYPE_CUSTOM;
	}
	else
	{
		int output_type = 0;
		mm_sound_device_in device_in;
		mm_sound_device_out device_out;

		return_val_if_fail( player->pipeline->audiobin, MM_ERROR_PLAYER_NOT_INITIALIZED );

		audio_effect_element = player->pipeline->audiobin[MMPLAYER_A_FILTER].gst;

		/* get status if speaker is activated */
		result = mm_sound_get_active_device(&device_in, &device_out);
		if ( result ) {
			debug_error("mm_sound_get_active_device() failed [%x]!!", result);
			debug_fleave();
			return result;
		}

		/* SPEAKER case */
		if (device_out == MM_SOUND_DEVICE_OUT_SPEAKER)
		{
			output_type = MM_AUDIO_EFFECT_OUTPUT_SPK;
			if (__mmplayer_is_earphone_only_effect_type(player, MM_AUDIO_EFFECT_TYPE_CUSTOM, 0))
			{
				debug_error("earphone is not equipped, some custom effect should operate with earphone");
				debug_fleave();
				return MM_ERROR_PLAYER_SOUND_EFFECT_INVALID_STATUS;
			}
		}
		/* Other case, include WIRED_ACCESSORY, BLUETOOTH, DOCK */
		else
		{
			output_type = MM_AUDIO_EFFECT_OUTPUT_EAR;
		}

		/* set filter output mode as SPEAKER or EARPHONE */
		g_object_set(audio_effect_element, "filter-output-mode", output_type, NULL);
		debug_log("filter-output-mode = %d (0:spk,1:ear)", output_type);

		result = __mmplayer_set_harmony_effect(player, audio_effect_element);
		if ( result )
		{
			debug_error("_set_harmony_effect() failed(%x)", result);
			debug_fleave();
			return result;
		}
	}

	debug_fleave();

	return result;
}


int
mm_player_audio_effect_custom_clear_eq_all(MMHandleType hplayer)
{
	int result = MM_ERROR_NONE;
	mm_player_t* player = (mm_player_t*)hplayer;

	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	/* clear EQ custom effect */
	memset(player->audio_effect_info.custom_eq_level, MM_AUDIO_EFFECT_CUSTOM_LEVEL_INIT, sizeof(int)*MM_AUDIO_EFFECT_EQ_BAND_NUM_MAX);

	debug_msg("All the EQ bands clearing success");

	debug_fleave();

	return result;
}


int
mm_player_audio_effect_custom_clear_ext_all(MMHandleType hplayer)
{
	int i;
	int result = MM_ERROR_NONE;
	mm_player_t* player = (mm_player_t*)hplayer;

	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	/* clear ALL custom effects, except EQ */
	for ( i = 0 ; i < MM_AUDIO_EFFECT_CUSTOM_NUM - 1 ; i++ )
	{
		player->audio_effect_info.custom_ext_level[i] = MM_AUDIO_EFFECT_CUSTOM_LEVEL_INIT;
	}

	debug_msg("All the extension effects clearing success");

	debug_fleave();

	return result;
}


int
mm_player_is_supported_preset_effect_type(MMHandleType hplayer, MMAudioEffectPresetType effect)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if ( !_mmplayer_is_supported_effect_type( MM_AUDIO_EFFECT_TYPE_PRESET, effect ) )
	{
		result = MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	debug_fleave();

	return result;
}


int
mm_player_is_supported_custom_effect_type(MMHandleType hplayer, MMAudioEffectCustomType effect)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if ( !_mmplayer_is_supported_effect_type( MM_AUDIO_EFFECT_TYPE_CUSTOM, effect ) )
	{
		result = MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	debug_fleave();

	return result;
}


int
mm_player_audio_effect_preset_apply(MMHandleType hplayer, MMAudioEffectPresetType type)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if (!PLAYER_INI()->use_audio_effect_preset)
	{
		debug_error("audio effect(preset) is not supported", type);
		debug_fleave();
		return MM_ERROR_NOT_SUPPORT_API;
	}

	if (type < MM_AUDIO_EFFECT_PRESET_AUTO || type >= MM_AUDIO_EFFECT_PRESET_NUM)
	{
		debug_error("out of range, type(%d)", type);
		debug_fleave();
		return MM_ERROR_INVALID_ARGUMENT;
	}

	/* check if this effect type is supported */
	if ( !_mmplayer_is_supported_effect_type( MM_AUDIO_EFFECT_TYPE_PRESET, type ) )
	{
		debug_fleave();
		return MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	result = _mmplayer_audio_effect_preset_apply(player, type);

	debug_fleave();

	return result;
}


int
mm_player_audio_effect_custom_apply(MMHandleType hplayer)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if (!PLAYER_INI()->use_audio_effect_custom)
	{
		debug_error("audio effect(custom) is not supported");
		debug_fleave();
		return MM_ERROR_NOT_SUPPORT_API;
	}

	result = _mmplayer_audio_effect_custom_apply(player);

	debug_fleave();

	return result;
}


int
mm_player_audio_effect_bypass (MMHandleType hplayer)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;
	GstElement *audio_effect_element = NULL;

	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if ( !PLAYER_INI()->use_audio_effect_preset && !PLAYER_INI()->use_audio_effect_custom )
	{
		debug_error("audio effect(preset/custom) is not supported");
		debug_fleave();
		return MM_ERROR_NOT_SUPPORT_API;
	}
	if ( !player->pipeline || !player->pipeline->audiobin )
	{
		debug_warning("effect element is not created yet.");
	}
	else
	{
		return_val_if_fail( player->pipeline->audiobin, MM_ERROR_PLAYER_NOT_INITIALIZED );
		audio_effect_element = player->pipeline->audiobin[MMPLAYER_A_FILTER].gst;

		/* order action to audio effect plugin */
		g_object_set(audio_effect_element, "filter-action", MM_AUDIO_EFFECT_TYPE_NONE, NULL);
		debug_log("filter-action = %d", MM_AUDIO_EFFECT_TYPE_NONE);
	}

	debug_fleave();

	return result;
}


int
_mmplayer_audio_effect_custom_set_level_ext(mm_player_t *player, MMAudioEffectCustomType custom_effect_type, int level)
{
	int effect_level_max = 0;
	int effect_level_min = 0;
	int count = 1;			/* start from 1, because of excepting eq index */
	int ext_level_index = 1;	/* start from 1, because of excepting eq index */
	int result = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check if EQ is supported */
	if ( !_mmplayer_is_supported_effect_type( MM_AUDIO_EFFECT_TYPE_CUSTOM, custom_effect_type ) )
	{
		debug_fleave();
		return MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	while ( count < MM_AUDIO_EFFECT_CUSTOM_NUM )
	{
		if ( PLAYER_INI()->audio_effect_custom_list[count] )
		{
			if ( count == custom_effect_type )
			{
				effect_level_min = PLAYER_INI()->audio_effect_custom_min_level_list[ext_level_index];
				effect_level_max = PLAYER_INI()->audio_effect_custom_max_level_list[ext_level_index];
				debug_msg("level min value(%d), level max value(%d)", effect_level_min, effect_level_max);
				break;
			}
			ext_level_index++;
			if (ext_level_index == PLAYER_INI()->audio_effect_custom_ext_num + 1)
			{
				debug_error("could not find min, max value. maybe effect information in ini file is not proper for audio effect plugin");
				break;
			}
		}
		count++;
	}

	if ( level < effect_level_min || level > effect_level_max )
	{
		debug_error("out of range, level(%d)", level);
		result = MM_ERROR_INVALID_ARGUMENT;
	}
	else
	{
		player->audio_effect_info.custom_ext_level[custom_effect_type-1] = level;
		debug_msg("set ext[%d] = %d", custom_effect_type-1, level);
	}

	debug_fleave();

	return result;
}


int
_mmplayer_audio_effect_custom_set_level_eq(mm_player_t *player, int index, int level)
{
	gint eq_level_max = 0;
	gint eq_level_min = 0;
	int result = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check if EQ is supported */
	if ( !_mmplayer_is_supported_effect_type( MM_AUDIO_EFFECT_TYPE_CUSTOM, MM_AUDIO_EFFECT_CUSTOM_EQ ) )
	{
		debug_fleave();
		return MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	if ( index < 0 || index > PLAYER_INI()->audio_effect_custom_eq_band_num - 1 )
	{
		debug_error("out of range, index(%d)", index);
		result = MM_ERROR_INVALID_ARGUMENT;
	}
	else
	{
		eq_level_min = PLAYER_INI()->audio_effect_custom_min_level_list[MM_AUDIO_EFFECT_CUSTOM_EQ];
		eq_level_max = PLAYER_INI()->audio_effect_custom_max_level_list[MM_AUDIO_EFFECT_CUSTOM_EQ];
		debug_msg("EQ level min value(%d), EQ level max value(%d)", eq_level_min, eq_level_max);

		if ( level < eq_level_min || level > eq_level_max )
		{
			debug_error("out of range, EQ level(%d)", level);
			result =  MM_ERROR_INVALID_ARGUMENT;
		}
		else
		{
			player->audio_effect_info.custom_eq_level[index] = level;
			debug_msg("set EQ[%d] = %d", index, level);
		}
	}

	debug_fleave();

	return result;
}


/* NOTE : parameter eq_index is only used for _set_eq_level() */
int
mm_player_audio_effect_custom_set_level(MMHandleType hplayer, MMAudioEffectCustomType effect_custom_type, int eq_index, int level)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check if this effect type is supported */
	if ( !_mmplayer_is_supported_effect_type( MM_AUDIO_EFFECT_TYPE_CUSTOM, effect_custom_type ) )
	{
		result = MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}
	else
	{
		if (effect_custom_type == MM_AUDIO_EFFECT_CUSTOM_EQ)
		{
			result = _mmplayer_audio_effect_custom_set_level_eq(player, eq_index, level);
		}
		else if (effect_custom_type > MM_AUDIO_EFFECT_CUSTOM_EQ || effect_custom_type < MM_AUDIO_EFFECT_CUSTOM_NUM)
		{
			result = _mmplayer_audio_effect_custom_set_level_ext(player, effect_custom_type, level);
		}
		else
		{
			debug_error("out of range, effect type(%d)", effect_custom_type);
			result = MM_ERROR_INVALID_ARGUMENT;
		}
	}

	debug_fleave();

	return result;
}


int
mm_player_audio_effect_custom_get_eq_bands_number(MMHandleType hplayer, int *bands)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check if EQ is supported */
	if ( !_mmplayer_is_supported_effect_type( MM_AUDIO_EFFECT_TYPE_CUSTOM, MM_AUDIO_EFFECT_CUSTOM_EQ ) )
	{
		debug_fleave();
		return MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	*bands = PLAYER_INI()->audio_effect_custom_eq_band_num;
	debug_log("number of custom EQ band = %d", *bands);

	debug_fleave();

	return result;
}


int
mm_player_audio_effect_custom_get_eq_bands_width(MMHandleType hplayer, int band_idx, int *width)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;
	unsigned int eq_num = 0;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check if EQ is supported */
	if ( !_mmplayer_is_supported_effect_type( MM_AUDIO_EFFECT_TYPE_CUSTOM, MM_AUDIO_EFFECT_CUSTOM_EQ ) )
	{
		debug_fleave();
		return MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	eq_num = PLAYER_INI()->audio_effect_custom_eq_band_num;
	if (band_idx < 0 || band_idx > eq_num-1)
	{
		debug_error("out of range, invalid band_idx(%d)", band_idx);
		result = MM_ERROR_INVALID_ARGUMENT;
	}
	else
	{
		/* set the width of EQ band */
		*width = PLAYER_INI()->audio_effect_custom_eq_band_width[band_idx];
		debug_log("width of band_idx(%d) = %dHz", band_idx, *width);
	}

	debug_fleave();

	return result;
}


int
mm_player_audio_effect_custom_get_eq_bands_freq(MMHandleType hplayer, int band_idx, int *freq)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;
	unsigned int eq_num = 0;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check if EQ is supported */
	if ( !_mmplayer_is_supported_effect_type( MM_AUDIO_EFFECT_TYPE_CUSTOM, MM_AUDIO_EFFECT_CUSTOM_EQ ) )
	{
		debug_fleave();
		return MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	eq_num = PLAYER_INI()->audio_effect_custom_eq_band_num;
	if (band_idx < 0 || band_idx > eq_num-1)
	{
		debug_error("out of range, invalid band_idx(%d)", band_idx);
		result = MM_ERROR_INVALID_ARGUMENT;
	}
	else
	{
		/* set the frequency of EQ band */
		*freq = PLAYER_INI()->audio_effect_custom_eq_band_freq[band_idx];
		debug_log("frequency of band_idx(%d) = %dHz", band_idx, *freq);
	}

	debug_fleave();

	return result;
}


int
mm_player_audio_effect_custom_get_level(MMHandleType hplayer, MMAudioEffectCustomType type, int eq_index, int *level)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail( level, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check if this effect type is supported */
	if ( !_mmplayer_is_supported_effect_type( MM_AUDIO_EFFECT_TYPE_CUSTOM, type ) )
	{
		debug_fleave();
		return MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	if (type == MM_AUDIO_EFFECT_CUSTOM_EQ)
	{
		if ( eq_index < 0 || eq_index > PLAYER_INI()->audio_effect_custom_eq_band_num - 1 )
		{
			debug_error("out of range, EQ index(%d)", eq_index);
			result = MM_ERROR_INVALID_ARGUMENT;
		}
		else
		{
			*level = player->audio_effect_info.custom_eq_level[eq_index];
			debug_log("EQ index = %d, level = %d", eq_index, *level);
		}
	}
	else if ( type > MM_AUDIO_EFFECT_CUSTOM_EQ && type < MM_AUDIO_EFFECT_CUSTOM_NUM )
	{
		*level = player->audio_effect_info.custom_ext_level[type-1];
		debug_log("extension effect index = %d, level = %d", type, *level);
	}
	else
	{
		debug_error("out of range, type(%d)", type);
		result = MM_ERROR_INVALID_ARGUMENT;
	}

	debug_fleave();

	return result;
}


int
mm_player_audio_effect_custom_get_level_range(MMHandleType hplayer, MMAudioEffectCustomType type, int *min, int *max)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;
	int count = 1;			/* start from 1, because of excepting eq index */
	int ext_level_index = 1;	/* start from 1, because of excepting eq index */

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail( min, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail( max, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check if this effect type is supported */
	if ( !_mmplayer_is_supported_effect_type( MM_AUDIO_EFFECT_TYPE_CUSTOM, type ) )
	{
		debug_fleave();
		return MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	if ( type == MM_AUDIO_EFFECT_CUSTOM_EQ )
	{
		*min = PLAYER_INI()->audio_effect_custom_min_level_list[MM_AUDIO_EFFECT_CUSTOM_EQ];
		*max = PLAYER_INI()->audio_effect_custom_max_level_list[MM_AUDIO_EFFECT_CUSTOM_EQ];
		debug_log("EQ min level = %d, max level = %d", *min, *max);
	}
	else
	{
		while ( count < MM_AUDIO_EFFECT_CUSTOM_NUM )
		{
			if ( PLAYER_INI()->audio_effect_custom_list[count] )
			{
				if ( count == type )
				{
					*min = PLAYER_INI()->audio_effect_custom_min_level_list[ext_level_index];
					*max = PLAYER_INI()->audio_effect_custom_max_level_list[ext_level_index];
					debug_msg("Extension effect(%d) min level = %d, max level = %d", count, *min, *max);
					break;
				}
				ext_level_index++;
				if ( ext_level_index == PLAYER_INI()->audio_effect_custom_ext_num + 1 )
				{
					debug_error("could not find min, max value. maybe effect information in ini file is not proper for audio effect plugin");
					break;
				}
			}
			count++;
		}
	}

	debug_fleave();

	return result;
}


int
mm_player_audio_effect_custom_set_level_eq_from_list(MMHandleType hplayer, int *level_list, int size)
{
	mm_player_t *player = (mm_player_t*)hplayer;
	gint i = 0;
	gint eq_level_min = 0;
	gint eq_level_max = 0;
	int result = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check if EQ is supported */
	if ( !_mmplayer_is_supported_effect_type( MM_AUDIO_EFFECT_TYPE_CUSTOM, MM_AUDIO_EFFECT_CUSTOM_EQ ) )
	{
		debug_fleave();
		return MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	if ( size != PLAYER_INI()->audio_effect_custom_eq_band_num )
	{
		debug_error("input size variable(%d) does not match with number of eq band(%d)", size, PLAYER_INI()->audio_effect_custom_eq_band_num);
		result = MM_ERROR_INVALID_ARGUMENT;
	}
	else
	{
		eq_level_min = PLAYER_INI()->audio_effect_custom_min_level_list[MM_AUDIO_EFFECT_CUSTOM_EQ];
		eq_level_max = PLAYER_INI()->audio_effect_custom_max_level_list[MM_AUDIO_EFFECT_CUSTOM_EQ];

		for ( i = 0 ; i < size ; i++ )
		{
			if ( level_list[i] < eq_level_min || level_list[i] > eq_level_max)
			{
				debug_error("out of range, level[%d]=%d", i, level_list[i]);
				result = MM_ERROR_INVALID_ARGUMENT;
				break;
			}
			player->audio_effect_info.custom_eq_level[i] = level_list[i];
		}
	}
	debug_fleave();

	return result;
}
