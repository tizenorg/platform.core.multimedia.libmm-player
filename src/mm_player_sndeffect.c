/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, YeJin Cho <cho.yejin@samsung.com>, YoungHwan An <younghwan_.an@samsung.com>
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

#include "mm_player_sndeffect.h"
#include "mm_player_ini.h"
#include "mm_player_priv.h"
#include <mm_sound.h>


int
mm_player_get_foreach_present_supported_filter_type(MMHandleType player, MMAudioFilterType filter_type, mmplayer_supported_sound_filter_cb foreach_cb, void *user_data)
{
	debug_fenter();
	int result = MM_ERROR_NONE;
	gboolean is_earphone = NULL;
	int i = 0;

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* get status if earphone is activated */
	result = mm_sound_is_route_available(MM_SOUND_ROUTE_OUT_WIRED_ACCESSORY, &is_earphone);
	if ( result ) {
		debug_error("mm_sound_is_route_available() failed [%x]!!\n", result);
		return result;
	}

	/* preset */
	if (filter_type == MM_AUDIO_FILTER_TYPE_PRESET)
	{
		for ( i = 0; i < MM_AUDIO_FILTER_PRESET_NUM; i++ )
		{
			if (is_earphone) {
				if (PLAYER_INI()->audio_filter_preset_list[i])
				{
					if (!foreach_cb(filter_type, i, user_data))
					{
						goto CALLBACK_ERROR;
					}
				}
			}
			else
			{
				if (PLAYER_INI()->audio_filter_preset_list[i] && !PLAYER_INI()->audio_filter_preset_earphone_only_list[i])
				{
					if (!foreach_cb(filter_type, i, user_data))
					{
						goto CALLBACK_ERROR;
					}
				}

			}
		}
	}
	/* custom */
	else if (filter_type == MM_AUDIO_FILTER_TYPE_CUSTOM)
	{
		for ( i = 0; i < MM_AUDIO_FILTER_CUSTOM_NUM; i++ )
		{
			if (is_earphone)
			{
				if (PLAYER_INI()->audio_filter_custom_list[i])
				{
					if (!foreach_cb(filter_type, i, user_data))
					{
						goto CALLBACK_ERROR;
					}
				}
			}
			else
			{
				if (PLAYER_INI()->audio_filter_custom_list[i] && !PLAYER_INI()->audio_filter_custom_earphone_only_list[i])
				{
					if (!foreach_cb(filter_type,i, user_data))
					{
						goto CALLBACK_ERROR;
					}
				}
			}
		}
	}
	else
	{
		debug_error("invalid filter type(%d)\n", filter_type);
		result = MM_ERROR_INVALID_ARGUMENT;
	}

	return result;

CALLBACK_ERROR:
	debug_error("foreach cb returned error\n");
	return MM_ERROR_PLAYER_INTERNAL;
}


int
__mmplayer_set_harmony_filter(mm_player_t *player, GstElement *filter_element)
{
	debug_fenter();
	gint *ext_filter_level_list = NULL;
	int count = 1;		/* start from 1, because of excepting eq index */
	int ext_level_index = 0;
	int result = MM_ERROR_NONE;
	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( filter_element, MM_ERROR_INVALID_ARGUMENT );

	/* Custom EQ */
	if( PLAYER_INI()->audio_filter_custom_eq_num )
	{
		debug_log("pass custom EQ level list to sound effect plugin\n");
		/* set custom-equalizer level list */
		g_object_set(filter_element, "custom-eq", player->audio_filter_info.custom_eq_level, NULL);
	}
	else
	{
		debug_warning("no custom EQ\n");
	}

	/* Custom Extension filters */
	if( PLAYER_INI()->audio_filter_custom_ext_num )
	{
		debug_log("pass custom extension level list to sound effect plugin\n");
		ext_filter_level_list = player->audio_filter_info.custom_ext_level_for_plugin;
		if (!ext_filter_level_list) {
			ext_filter_level_list = (gint*) malloc (sizeof(gint)*PLAYER_INI()->audio_filter_custom_ext_num);
			if (!ext_filter_level_list)
			{
				debug_error("memory allocation for extension filter list failed\n");
				return MM_ERROR_OUT_OF_MEMORY;
			}
			else
			{
				memset (ext_filter_level_list, 0, PLAYER_INI()->audio_filter_custom_ext_num);
			}
		}

		while ( count < MM_AUDIO_FILTER_CUSTOM_NUM )
		{
			if ( PLAYER_INI()->audio_filter_custom_list[count] )
			{
				ext_filter_level_list[ext_level_index] = player->audio_filter_info.custom_ext_level[count-1];
				ext_level_index++;
				if (ext_level_index == PLAYER_INI()->audio_filter_custom_ext_num)
				{
					break;
				}
			}
			count++;
		}

		/* set custom-extension filters level list */
		g_object_set(filter_element, "custom-ext", ext_filter_level_list, NULL);
	}
	else
	{
		debug_warning("no custom extension fliter\n");
	}

	/* order action to sound effect plugin */
	g_object_set(filter_element, "filter-action", MM_AUDIO_FILTER_TYPE_CUSTOM, NULL);
	debug_log("filter-action = %d\n", MM_AUDIO_FILTER_TYPE_CUSTOM);

	debug_fleave();

	return result;
}


gboolean
__mmplayer_is_earphone_only_filter_type(mm_player_t *player, MMAudioFilterType filter_type, int filter)
{
	gboolean result = FALSE;
	int i = 0;
	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	/* preset */
	if (filter_type == MM_AUDIO_FILTER_TYPE_PRESET)
	{
		if (PLAYER_INI()->audio_filter_preset_earphone_only_list[filter])
		{
			debug_msg("this preset filter(%d) is only available with earphone\n", filter);
			result = TRUE;
		}
	}
	/* custom */
	else if (filter_type == MM_AUDIO_FILTER_TYPE_CUSTOM)
	{
		for (i = 1; i < MM_AUDIO_FILTER_CUSTOM_NUM; i++) /* it starts from 1(except testing for EQ) */
		{
			if (PLAYER_INI()->audio_filter_custom_earphone_only_list[i])
			{
				/* check if the earphone only custom filter was set */
				if (player->audio_filter_info.custom_ext_level[i-1])
				{
					debug_msg("this custom filter(%d) is only available with earphone\n", i);
					result = TRUE;
				}
			}
		}
	}
	else
	{
		debug_error("invalid filter type(%d)\n", filter_type);
	}

	return result;
}


gboolean
_mmplayer_is_supported_filter_type(MMAudioFilterType filter_type, int filter)
{
	gboolean result = TRUE;

	debug_fenter();

	/* preset */
	if (filter_type == MM_AUDIO_FILTER_TYPE_PRESET)
	{
		if ( filter < MM_AUDIO_FILTER_PRESET_AUTO || filter >= MM_AUDIO_FILTER_PRESET_NUM )
		{
			debug_error("out of range, preset filter(%d)\n", filter);
			result = FALSE;
		}
		if (!PLAYER_INI()->audio_filter_preset_list[filter])
		{
			debug_error("this filter(%d) is not supported\n", filter);
			result = FALSE;
		}
	}
	/* custom */
	else if (filter_type == MM_AUDIO_FILTER_TYPE_CUSTOM)
	{
		if ( filter < MM_AUDIO_FILTER_CUSTOM_EQ || filter >= MM_AUDIO_FILTER_CUSTOM_NUM )
		{
			debug_error("out of range, custom filter(%d)\n", filter);
			result = FALSE;
		}
		if (!PLAYER_INI()->audio_filter_custom_list[filter])
		{
			debug_error("this custom filter(%d) is not supported\n", filter);
			result = FALSE;
		}
	}
	else
	{
		debug_error("invalid filter type(%d)\n", filter_type);
		result = FALSE;
	}

	return result;
}


int
_mmplayer_sound_filter_preset_apply(mm_player_t *player, MMAudioFilterPresetType filter_type)
{
	GstElement *filter_element = NULL;
	int result = MM_ERROR_NONE;
	int output_type = 0;
	bool is_earphone = FALSE;
	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* Music Player can set sound effect value before Audiobin is created. */
	if ( !player->pipeline || !player->pipeline->audiobin )
	{
		debug_warning("filter element is not created yet.\n");

		player->bypass_sound_effect = FALSE;

		/* store sound effect setting in order to apply it when audio filter is created */
		player->audio_filter_info.filter_type = MM_AUDIO_FILTER_TYPE_PRESET;
		player->audio_filter_info.preset = filter_type;
	}
	else
	{
		return_val_if_fail( player->pipeline->audiobin, MM_ERROR_PLAYER_NOT_INITIALIZED );

		filter_element = player->pipeline->audiobin[MMPLAYER_A_FILTER].gst;

		/* get status if earphone is activated */
		result = mm_sound_is_route_available(MM_SOUND_ROUTE_OUT_WIRED_ACCESSORY, &is_earphone);
		if ( result ) {
			debug_error("mm_sound_is_route_available() failed [%x]!!\n", result);
			return result;
		}

		if (is_earphone)
		{
			output_type = MM_AUDIO_FILTER_OUTPUT_EAR;
		}
		else
		{
			output_type = MM_AUDIO_FILTER_OUTPUT_SPK;
			if (__mmplayer_is_earphone_only_filter_type(player, MM_AUDIO_FILTER_TYPE_PRESET, filter_type))
			{
				debug_error("earphone is not equipped, this filter will not be applied\n");
				return MM_ERROR_PLAYER_SOUND_EFFECT_INVALID_STATUS;
			}
		}

		/* set filter output mode as SPEAKER or EARPHONE */
		g_object_set(filter_element, "filter-output-mode", output_type, NULL);
		debug_log("filter-output-mode = %d (0:spk,1:ear)\n", output_type);

		if (filter_type == MM_AUDIO_FILTER_PRESET_AUTO) {
			/* TODO: Add codes about auto selecting preset mode according to ID3 tag */
			/* set filter preset mode */
			g_object_set(filter_element, "preset-mode", 0, NULL); /* forced set to 0(normal) temporarily */
			debug_log("preset-mode = %d\n", filter_type);

		} else {
			/* set filter preset mode */
			g_object_set(filter_element, "preset-mode", filter_type-1, NULL); /* filter_type-1, because of _PRESET_AUTO in MSL/CAPI which does not exist in soundAlive plugin */
			debug_log("preset-mode = %d\n", filter_type);
		}

		/* order action to sound effect plugin */
		g_object_set(filter_element, "filter-action", MM_AUDIO_FILTER_TYPE_PRESET, NULL);
		debug_log("filter-action = %d\n", MM_AUDIO_FILTER_TYPE_PRESET);

	}
	debug_fleave();

	return result;
}


int
_mmplayer_sound_filter_custom_apply(mm_player_t *player)
{
	GstElement *filter_element = NULL;
	int result = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* Music Player can set sound effect value before Audiobin is created. */
	if ( !player->pipeline || !player->pipeline->audiobin )
	{
		debug_warning("filter element is not created yet.\n");

		player->bypass_sound_effect = FALSE;

		/* store sound effect setting in order to apply it when audio filter is created */
		player->audio_filter_info.filter_type = MM_AUDIO_FILTER_TYPE_CUSTOM;
	}
	else
	{
		int output_type;
		bool is_earphone = FALSE;
		return_val_if_fail( player->pipeline->audiobin, MM_ERROR_PLAYER_NOT_INITIALIZED );

		filter_element = player->pipeline->audiobin[MMPLAYER_A_FILTER].gst;

		/* get status if earphone is activated */
		result = mm_sound_is_route_available(MM_SOUND_ROUTE_OUT_WIRED_ACCESSORY, &is_earphone);
		if ( result ) {
			debug_error("mm_sound_is_route_available() failed [%x]!!\n", result);
			return result;
		}

		if (is_earphone)
		{
			output_type = MM_AUDIO_FILTER_OUTPUT_EAR;
		}
		else
		{
			output_type = MM_AUDIO_FILTER_OUTPUT_SPK;
			if (__mmplayer_is_earphone_only_filter_type(player, MM_AUDIO_FILTER_TYPE_CUSTOM, NULL))
			{
				debug_error("earphone is not equipped, some custom filter should operate with earphone(%x)\n", result);
				return MM_ERROR_PLAYER_SOUND_EFFECT_INVALID_STATUS;
			}
		}

		/* set filter output mode as SPEAKER or EARPHONE */
		g_object_set(filter_element, "filter-output-mode", output_type, NULL);
		debug_log("filter output mode = %d(0:spk,1:ear)\n", output_type);

		result = __mmplayer_set_harmony_filter(player, filter_element);
		if ( result )
		{
			debug_error("_set_harmony_filter() failed(%x)\n", result);
			return result;
		}
	}
	debug_fleave();

	return result;
}


int
mm_player_sound_filter_custom_clear_eq_all(MMHandleType hplayer)
{
	int result = MM_ERROR_NONE;
	mm_player_t* player = (mm_player_t*)hplayer;
	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	/* clear EQ custom filter */
	memset(player->audio_filter_info.custom_eq_level, MM_AUDIO_FILTER_CUSTOM_LEVEL_INIT, sizeof(int)*MM_AUDIO_FILTER_EQ_BAND_MAX);

	debug_msg("All the EQ bands clearing success\n");

	return result;
}


int
mm_player_sound_filter_custom_clear_ext_all(MMHandleType hplayer)
{
	int i;
	int result = MM_ERROR_NONE;
	mm_player_t* player = (mm_player_t*)hplayer;
	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	/* clear ALL custom filters, except EQ */
	for ( i = 0 ; i < MM_AUDIO_FILTER_CUSTOM_NUM - 1 ; i++ )
	{
		player->audio_filter_info.custom_ext_level[i] = MM_AUDIO_FILTER_CUSTOM_LEVEL_INIT;
	}

	debug_msg("All the extension filters clearing success\n");

	return result;
}


int
mm_player_is_supported_preset_filter_type(MMHandleType hplayer, MMAudioFilterPresetType filter)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;
	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if ( !_mmplayer_is_supported_filter_type( MM_AUDIO_FILTER_TYPE_PRESET, filter ) )
	{
		result = MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	return result;
}


int
mm_player_is_supported_custom_filter_type(MMHandleType hplayer, MMAudioFilterCustomType filter)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;
	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if ( !_mmplayer_is_supported_filter_type( MM_AUDIO_FILTER_TYPE_CUSTOM, filter ) )
	{
		result = MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	return result;
}


int
mm_player_sound_filter_preset_apply(MMHandleType hplayer, MMAudioFilterPresetType type)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;
	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if (!PLAYER_INI()->use_audio_filter_preset)
	{
		debug_error("sound filter(preset) is not suppported\n", type);
		return MM_ERROR_NOT_SUPPORT_API;
	}

	if (type < MM_AUDIO_FILTER_PRESET_AUTO || type >= MM_AUDIO_FILTER_PRESET_NUM)
	{
		debug_error("out of range, type(%d)\n", type);
		return MM_ERROR_INVALID_ARGUMENT;
	}

	/* check if this filter type is supported */
	if ( !_mmplayer_is_supported_filter_type( MM_AUDIO_FILTER_TYPE_PRESET, type ) )
	{
		return MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	result = _mmplayer_sound_filter_preset_apply(player, type);

	return result;
}


int
mm_player_sound_filter_custom_apply(MMHandleType hplayer)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;
	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if (!PLAYER_INI()->use_audio_filter_custom)
	{
		debug_error("sound filter(custom) is not suppported\n");
		return MM_ERROR_NOT_SUPPORT_API;
	}

	result = _mmplayer_sound_filter_custom_apply(player);

	return result;
}


int
mm_player_sound_filter_bypass (MMHandleType hplayer)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;
	GstElement *filter_element = NULL;
	debug_fenter();

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	if ( !PLAYER_INI()->use_audio_filter_preset && !PLAYER_INI()->use_audio_filter_custom )
	{
		debug_error("sound filter(preset/custom) is not suppported\n");
		return MM_ERROR_NOT_SUPPORT_API;
	}
	if ( !player->pipeline || !player->pipeline->audiobin )
	{
		debug_warning("filter element is not created yet.\n");
	}
	else
	{
		return_val_if_fail( player->pipeline->audiobin, MM_ERROR_PLAYER_NOT_INITIALIZED );
		filter_element = player->pipeline->audiobin[MMPLAYER_A_FILTER].gst;

		/* order action to sound effect plugin */
		g_object_set(filter_element, "filter-action", MM_AUDIO_FILTER_TYPE_NONE, NULL);
		debug_log("filter-action = %d\n", MM_AUDIO_FILTER_TYPE_NONE);
	}

	debug_fleave();
	return result;
}


int
_mmplayer_sound_filter_custom_set_level_ext(mm_player_t *player, MMAudioFilterCustomType custom_filter_type, int level)
{
	int filter_level_max = 0;
	int filter_level_min = 0;
	int count = 1;			/* start from 1, because of excepting eq index */
	int ext_level_index = 1;	/* start from 1, because of excepting eq index */
	int result = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check if EQ is supported */
	if ( !_mmplayer_is_supported_filter_type( MM_AUDIO_FILTER_TYPE_CUSTOM, custom_filter_type ) )
	{
		return MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	while ( count < MM_AUDIO_FILTER_CUSTOM_NUM )
	{
		if ( PLAYER_INI()->audio_filter_custom_list[count] )
		{
			if ( count == custom_filter_type )
			{
				filter_level_min = PLAYER_INI()->audio_filter_custom_min_level_list[ext_level_index];
				filter_level_max = PLAYER_INI()->audio_filter_custom_max_level_list[ext_level_index];
				debug_msg("level min value(%d), level max value(%d)\n", filter_level_min, filter_level_max);
				break;
			}
			ext_level_index++;
			if (ext_level_index == PLAYER_INI()->audio_filter_custom_ext_num + 1)
			{
				debug_error("could not find min, max value. maybe filter information in ini file is not proper for sound effect plugin\n");
				break;
			}
		}
		count++;
	}

	if ( level < filter_level_min || level > filter_level_max )
	{
		debug_error("out of range, level(%d)\n", level);
		result = MM_ERROR_INVALID_ARGUMENT;
	}
	else
	{
		player->audio_filter_info.custom_ext_level[custom_filter_type-1] = level;
		debug_msg("set ext[%d] = %d\n", custom_filter_type-1, level);
	}

	debug_fleave();
	return result;
}


int
_mmplayer_sound_filter_custom_set_level_eq(mm_player_t *player, int index, int level)
{
	gint eq_level_max = 0;
	gint eq_level_min = 0;
	int result = MM_ERROR_NONE;
	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check if EQ is supported */
	if ( !_mmplayer_is_supported_filter_type( MM_AUDIO_FILTER_TYPE_CUSTOM, MM_AUDIO_FILTER_CUSTOM_EQ ) )
	{
		return MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	if ( index < 0 || index > PLAYER_INI()->audio_filter_custom_eq_num - 1 )
	{
		debug_error("out of range, index(%d)\n", index);
		result = MM_ERROR_INVALID_ARGUMENT;
	}
	else
	{
		eq_level_min = PLAYER_INI()->audio_filter_custom_min_level_list[MM_AUDIO_FILTER_CUSTOM_EQ];
		eq_level_max = PLAYER_INI()->audio_filter_custom_max_level_list[MM_AUDIO_FILTER_CUSTOM_EQ];
		debug_msg("EQ level min value(%d), EQ level max value(%d)\n", eq_level_min, eq_level_max);

		if ( level < eq_level_min || level > eq_level_max )
		{
			debug_error("out of range, EQ level(%d)\n", level);
			result =  MM_ERROR_INVALID_ARGUMENT;
		}
		else
		{
			player->audio_filter_info.custom_eq_level[index] = level;
			debug_msg("set EQ[%d] = %d\n", index, level);
		}
	}
	debug_fleave();

	return result;
}


/* NOTE : parameter eq_index is only used for _set_eq_level() */
int
mm_player_sound_filter_custom_set_level(MMHandleType hplayer, MMAudioFilterCustomType filter_custom_type, int eq_index, int level)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;
	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check if this filter type is supported */
	if ( !_mmplayer_is_supported_filter_type( MM_AUDIO_FILTER_TYPE_CUSTOM, filter_custom_type ) )
	{
		return MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	if (filter_custom_type == MM_AUDIO_FILTER_CUSTOM_EQ)
	{
		result = _mmplayer_sound_filter_custom_set_level_eq(player, eq_index, level);
	}
	else if (filter_custom_type > MM_AUDIO_FILTER_CUSTOM_EQ || filter_custom_type < MM_AUDIO_FILTER_CUSTOM_NUM)
	{
		result = _mmplayer_sound_filter_custom_set_level_ext(player, filter_custom_type, level);
	}
	else
	{
		debug_error("out of range, filter type(%d)\n", filter_custom_type);
		result = MM_ERROR_INVALID_ARGUMENT;
	}
	return result;
}


int
mm_player_sound_filter_custom_get_eq_bands_number(MMHandleType hplayer, int *bands)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;
	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check if EQ is supported */
	if ( !_mmplayer_is_supported_filter_type( MM_AUDIO_FILTER_TYPE_CUSTOM, MM_AUDIO_FILTER_CUSTOM_EQ ) )
	{
		return MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	*bands = PLAYER_INI()->audio_filter_custom_eq_num;
	debug_log("number of custom eq band = %d\n", *bands);

	debug_fleave();

	return result;
}


int
mm_player_sound_filter_custom_get_level(MMHandleType hplayer, MMAudioFilterCustomType type, int eq_index, int *level)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail( level, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check if this filter type is supported */
	if ( !_mmplayer_is_supported_filter_type( MM_AUDIO_FILTER_TYPE_CUSTOM, type ) )
	{
		return MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	if (type == MM_AUDIO_FILTER_CUSTOM_EQ)
	{
		if ( eq_index < 0 || eq_index > PLAYER_INI()->audio_filter_custom_eq_num - 1 )
		{
			debug_error("out of range, eq index(%d)\n", eq_index);
			result = MM_ERROR_INVALID_ARGUMENT;
		}
		else
		{
			*level = player->audio_filter_info.custom_eq_level[eq_index];
			debug_log("EQ index = %d, level = %d\n", eq_index, *level);
		}
	}
	else if ( type > MM_AUDIO_FILTER_CUSTOM_EQ && type < MM_AUDIO_FILTER_CUSTOM_NUM )
	{
		*level = player->audio_filter_info.custom_ext_level[type-1];
		debug_log("extention filter index = %d, level = %d\n", type, *level);
	}
	else
	{
		debug_error("out of range, type(%d)\n", type);
		result = MM_ERROR_INVALID_ARGUMENT;
	}

	debug_fleave();

	return result;
}


int
mm_player_sound_filter_custom_get_level_range(MMHandleType hplayer, MMAudioFilterCustomType type, int *min, int *max)
{
	mm_player_t* player = (mm_player_t*)hplayer;
	int result = MM_ERROR_NONE;
	int count = 1;			/* start from 1, because of excepting eq index */
	int ext_level_index = 1;	/* start from 1, because of excepting eq index */

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail( min, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail( max, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check if this filter type is supported */
	if ( !_mmplayer_is_supported_filter_type( MM_AUDIO_FILTER_TYPE_CUSTOM, type ) )
	{
		return MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	if ( type == MM_AUDIO_FILTER_CUSTOM_EQ )
	{
		*min = PLAYER_INI()->audio_filter_custom_min_level_list[MM_AUDIO_FILTER_CUSTOM_EQ];
		*max = PLAYER_INI()->audio_filter_custom_max_level_list[MM_AUDIO_FILTER_CUSTOM_EQ];
		debug_log("EQ min level = %d, max level = %d\n", *min, *max);
	}
	else
	{
		while ( count < MM_AUDIO_FILTER_CUSTOM_NUM )
		{
			if ( PLAYER_INI()->audio_filter_custom_list[count] )
			{
				if ( count == type )
				{
					*min = PLAYER_INI()->audio_filter_custom_min_level_list[ext_level_index];
					*max = PLAYER_INI()->audio_filter_custom_max_level_list[ext_level_index];
					debug_msg("Extension filter(%d) min level = %d, max level = %d\n", count, *min, *max);
					break;
				}
				ext_level_index++;
				if ( ext_level_index == PLAYER_INI()->audio_filter_custom_ext_num + 1 )
				{
					debug_error("could not find min, max value. maybe filter information in ini file is not proper for sound effect plugin\n");
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
mm_player_sound_filter_custom_set_level_eq_from_list(MMHandleType hplayer, int *level_list, int size)
{
	mm_player_t *player = (mm_player_t*)hplayer;
	gint i = 0;
	gint eq_level_min = 0;
	gint eq_level_max = 0;
	int result = MM_ERROR_NONE;

	debug_fenter();

	return_val_if_fail( player, MM_ERROR_PLAYER_NOT_INITIALIZED );

	/* check if EQ is supported */
	if ( !_mmplayer_is_supported_filter_type( MM_AUDIO_FILTER_TYPE_CUSTOM, MM_AUDIO_FILTER_CUSTOM_EQ ) )
	{
		return MM_ERROR_PLAYER_SOUND_EFFECT_NOT_SUPPORTED_FILTER;
	}

	if ( size != PLAYER_INI()->audio_filter_custom_eq_num )
	{
		debug_error("input size variable(%d) does not match with number of eq band(%d)\n", size, PLAYER_INI()->audio_filter_custom_eq_num);
		result = MM_ERROR_INVALID_ARGUMENT;
	}
	else
	{
		eq_level_min = PLAYER_INI()->audio_filter_custom_min_level_list[MM_AUDIO_FILTER_CUSTOM_EQ];
		eq_level_max = PLAYER_INI()->audio_filter_custom_max_level_list[MM_AUDIO_FILTER_CUSTOM_EQ];

		for ( i = 0 ; i < size ; i++ )
		{
			if ( level_list[i] < eq_level_min || level_list[i] > eq_level_max)
			{
				debug_error("out of range, level[%d]=%d\n", i, level_list[i]);
				result = MM_ERROR_INVALID_ARGUMENT;
				break;
			}
			player->audio_filter_info.custom_eq_level[i] = level_list[i];
		}
	}
	debug_fleave();

	return result;
}
