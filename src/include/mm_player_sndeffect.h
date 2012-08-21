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

#ifndef __MM_PLAYER_SNDEFFECT_H__
#define __MM_PLAYER_SNDEFFECT_H__

#include <mm_types.h>

#ifdef __cplusplus
	extern "C" {
#endif

#define MM_AUDIO_FILTER_EQ_BAND_MAX		8
#define MM_AUDIO_FILTER_CUSTOM_LEVEL_INIT	0

/**
	@addtogroup PLAYER_INTERNAL

*/

/**
 * Enumerations of Preset Filter Type
 */
typedef enum {
	MM_AUDIO_FILTER_PRESET_AUTO = 0,	/**<  Filter Preset type Auto */
	MM_AUDIO_FILTER_PRESET_NORMAL,		/**<  Filter Preset type Normal */
	MM_AUDIO_FILTER_PRESET_POP,		/**<  Filter Preset type Pop */
	MM_AUDIO_FILTER_PRESET_ROCK,		/**<  Filter Preset type Rock */
	MM_AUDIO_FILTER_PRESET_DANCE,		/**<  Filter Preset type Dance */
	MM_AUDIO_FILTER_PRESET_JAZZ,		/**<  Filter Preset type Jazz */
	MM_AUDIO_FILTER_PRESET_CLASSIC,		/**<  Filter Preset type Classic */
	MM_AUDIO_FILTER_PRESET_VOCAL,		/**<  Filter Preset type Vocal */
	MM_AUDIO_FILTER_PRESET_BASS_BOOST,	/**<  Filter Preset type Bass Boost */
	MM_AUDIO_FILTER_PRESET_TREBLE_BOOST,	/**<  Filter Preset type Treble Boost */
	MM_AUDIO_FILTER_PRESET_MTHEATER,	/**<  Filter Preset type MTheater */
	MM_AUDIO_FILTER_PRESET_EXT,		/**<  Filter Preset type Externalization */
	MM_AUDIO_FILTER_PRESET_CAFE,		/**<  Filter Preset type Cafe */
	MM_AUDIO_FILTER_PRESET_CONCERT_HALL,	/**<  Filter Preset type Concert Hall */
	MM_AUDIO_FILTER_PRESET_VOICE,		/**<  Filter Preset type Voice */
	MM_AUDIO_FILTER_PRESET_MOVIE,		/**<  Filter Preset type Movie */
	MM_AUDIO_FILTER_PRESET_VIRT51,		/**<  Filter Preset type Virtual 5.1 */
	MM_AUDIO_FILTER_PRESET_NUM,		/**<  Number of Filter Preset type */
} MMAudioFilterPresetType;

/**
 * Enumerations of Custom Filter Type
 */
typedef enum {
	MM_AUDIO_FILTER_CUSTOM_EQ = 0,		/**<  Filter Custom type Equalizer */
	MM_AUDIO_FILTER_CUSTOM_3D,		/**<  Filter Custom type 3D */
	MM_AUDIO_FILTER_CUSTOM_BASS,		/**<  Filter Custom type Bass */
	MM_AUDIO_FILTER_CUSTOM_ROOM_SIZE,	/**<  Filter Custom type Room Size */
	MM_AUDIO_FILTER_CUSTOM_REVERB_LEVEL,	/**<  Filter Custom type Reverb Level */
	MM_AUDIO_FILTER_CUSTOM_CLARITY,		/**<  Filter Custom type Clarity */
	MM_AUDIO_FILTER_CUSTOM_NUM,		/**<  Number of Filter Custom type */
} MMAudioFilterCustomType;

/**
 * Enumerations of Filter Type
 */
typedef enum {
	MM_AUDIO_FILTER_TYPE_NONE,
	MM_AUDIO_FILTER_TYPE_PRESET,
	MM_AUDIO_FILTER_TYPE_CUSTOM,
} MMAudioFilterType;


/**
 * Enumerations of Output Mode
 */
typedef enum {
	MM_AUDIO_FILTER_OUTPUT_SPK,		/**< Speaker out */
	MM_AUDIO_FILTER_OUTPUT_EAR			/**< Earjack out */
} MMAudioFilterOutputMode;


/**
 * Structure of FilterInfo
 */
typedef struct {
	MMAudioFilterType filter_type;		/**< Filter type, (NONE,PRESET,CUSTOM)*/
	MMAudioFilterPresetType preset;		/**< for preset type*/
	int *custom_ext_level_for_plugin;	/**< for custom type, level value list of extension filters*/
	int custom_eq_level[MM_AUDIO_FILTER_EQ_BAND_MAX];	/**< for custom type, EQ info*/
	int custom_ext_level[MM_AUDIO_FILTER_CUSTOM_NUM-1];	/**< for custom type, extension filter info*/
} MMAudioFilterInfo;


/**
 * @brief Called to get each supported sound filter.
 *
 * @param	filter_type	[in]	Type of filter (preset filter or custom filter).
 * @param	filter		[in]	Supported sound filter.
 * @param	user_data	[in]	Pointer of user data.
 *
 * @return	True to continue with the next iteration of the loop, False to break outsp of the loop.
 * @see		mm_player_get_foreach_present_supported_filter_type()
 */
typedef bool (*mmplayer_supported_sound_filter_cb) (int filter_type, int type, void *user_data);

/**
 * This function is to get supported filter type.
 *
 * @param	hplayer		[in]	Handle of player.
 * @param	filter_type	[in]	Type of filter.
 * @param	foreach_cb	[in]	Callback function to be passed the result.
 * @param	user_data	[in]	Pointer of user data.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_get_foreach_present_supported_filter_type(MMHandleType player, MMAudioFilterType filter_type, mmplayer_supported_sound_filter_cb foreach_cb, void *user_data);

/**
 * This function is to bypass sound effect.
 *
 * @param	hplayer		[in]	Handle of player.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_sound_filter_bypass (MMHandleType hplayer);

/**
 * This function is to apply preset filter.
 *
 * @param	hplayer		[in]	Handle of player.
 * @param	type		[in]	Preset type filter.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see		MMAudioFilterPresetType
 * @since
 */
int mm_player_sound_filter_preset_apply(MMHandleType hplayer, MMAudioFilterPresetType type);

/**
 * This function is to apply custom filter(Equalizer and Extension filters).
 *
 * @param	hplayer		[in]	Handle of player.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_sound_filter_custom_apply(MMHandleType hplayer);

/**
 * This function is to clear Equalizer custom filter.
 *
 * @param	hplayer		[in]	Handle of player.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_sound_filter_custom_clear_eq_all(MMHandleType hplayer);

/**
 * This function is to clear Extension custom filters.
 *
 * @param	hplayer		[in]	Handle of player.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_sound_filter_custom_clear_ext_all(MMHandleType hplayer);

/**
 * This function is to get the number of equalizer bands.
 *
 * @param	hplayer		[in]	Handle of player.
 * @param	bands		[out]	The number of bands.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_sound_filter_custom_get_eq_bands_number(MMHandleType hplayer, int *bands);

/**
 * This function is to get the level of the custom filter.
 *
 * @param	hplayer		[in]	Handle of player.
 * @param	type		[in]	Custom type filter.
 * @param	eq_index	[in]	Equalizer band index. This parameter is available only when the type is MM_AUDIO_FILTER_CUSTOM_EQ.
 * @param	level		[out]	The level of the custom filter.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see		MMAudioFilterCustomType
 * @since
 */
int mm_player_sound_filter_custom_get_level(MMHandleType hplayer, MMAudioFilterCustomType type, int eq_index, int *level);

/**
 * This function is to get range of the level of the custom filter.
 *
 * @param	hplayer		[in]	Handle of player.
 * @param	type		[in]	Custom type filter.
 * @param	min		[out]	Minimal value of level.
 * @param	max		[out]	Maximum value of level.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see		MMAudioFilterCustomType
 * @since
 */
int mm_player_sound_filter_custom_get_level_range(MMHandleType hplayer, MMAudioFilterCustomType type, int *min, int *max);

/**
 * This function is to set the level of the custom filter.
 *
 * @param	hplayer		[in]	Handle of player.
 * @param	type		[in]	Custom type filter.
 * @param	eq_index	[in]	Equalizer band index. This parameter is available only when the type is MM_AUDIO_FILTER_CUSTOM_EQ.
 * @param	level		[in]	The level of the custom filter.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see		MMAudioFilterCustomType
 * @since
 */
int mm_player_sound_filter_custom_set_level(MMHandleType hplayer, MMAudioFilterCustomType filter_custom_type, int eq_index, int level);

/**
 * This function is to set the bands level of equalizer custom filter using input list.
 *
 * @param	hplayer		[in]	Handle of player.
 * @param	level_list	[in]	list of bands level of equalizer custom filter want to set.
 * @param	size		[in]	size of level_list.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_sound_filter_custom_set_level_eq_from_list(MMHandleType hplayer, int *level_list, int size);

/**
 * This function is to decide if the preset type filter is supported or not
 *
 * @param	hplayer		[in]	Handle of player.
 * @param	filter		[in]	Preset type filter.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_is_supported_preset_filter_type(MMHandleType hplayer, MMAudioFilterPresetType filter);

/**
 * This function is to decide if the custom type filter is supported or not
 *
 * @param	hplayer		[in]	Handle of player.
 * @param	filter		[in]	Custom type filter.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_is_supported_custom_filter_type(MMHandleType hplayer, MMAudioFilterCustomType filter);

/**
	@}
 */

#ifdef __cplusplus
	}
#endif

#endif	/* __MM_PLAYER_SNDEFFECT_H__ */
