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

#ifndef __MM_PLAYER_AUDIOEFFECT_H__
#define __MM_PLAYER_AUDIOEFFECT_H__

#include <mm_types.h>

#ifdef __cplusplus
	extern "C" {
#endif

#define MM_AUDIO_EFFECT_EQ_BAND_NUM_MAX		10
#define MM_AUDIO_EFFECT_CUSTOM_LEVEL_INIT	0

#define MM_AUDIO_EFFECT_SQUARE_VALUE_MAX	4
#define MM_AUDIO_EFFECT_SQUARE_VALUE_MIN	0

/**
	@addtogroup PLAYER_INTERNAL

*/

/**
 * Enumerations of Audio Effect Preset Type
 */
typedef enum {
	MM_AUDIO_EFFECT_PRESET_AUTO = 0,     /**<  Preset type Auto */
	MM_AUDIO_EFFECT_PRESET_NORMAL,       /**<  Preset type Normal */
	MM_AUDIO_EFFECT_PRESET_POP,          /**<  Preset type Pop */
	MM_AUDIO_EFFECT_PRESET_ROCK,         /**<  Preset type Rock */
	MM_AUDIO_EFFECT_PRESET_DANCE,        /**<  Preset type Dance */
	MM_AUDIO_EFFECT_PRESET_JAZZ,         /**<  Preset type Jazz */
	MM_AUDIO_EFFECT_PRESET_CLASSIC,      /**<  Preset type Classic */
	MM_AUDIO_EFFECT_PRESET_VOCAL,        /**<  Preset type Vocal */
	MM_AUDIO_EFFECT_PRESET_BASS_BOOST,   /**<  Preset type Bass Boost */
	MM_AUDIO_EFFECT_PRESET_TREBLE_BOOST, /**<  Preset type Treble Boost */
	MM_AUDIO_EFFECT_PRESET_MTHEATER,     /**<  Preset type MTheater */
	MM_AUDIO_EFFECT_PRESET_EXT,          /**<  Preset type Externalization */
	MM_AUDIO_EFFECT_PRESET_CAFE,         /**<  Preset type Cafe */
	MM_AUDIO_EFFECT_PRESET_CONCERT_HALL, /**<  Preset type Concert Hall */
	MM_AUDIO_EFFECT_PRESET_VOICE,        /**<  Preset type Voice */
	MM_AUDIO_EFFECT_PRESET_MOVIE,        /**<  Preset type Movie */
	MM_AUDIO_EFFECT_PRESET_VIRT51,       /**<  Preset type Virtual 5.1 */
	MM_AUDIO_EFFECT_PRESET_HIPHOP,       /**<  Preset type HipHop */
	MM_AUDIO_EFFECT_PRESET_RNB,          /**<  Preset type R&B */
	MM_AUDIO_EFFECT_PRESET_FLAT,         /**<  Preset type Flat */
	MM_AUDIO_EFFECT_PRESET_TUBE,         /**<  Preset type Tube */
	MM_AUDIO_EFFECT_PRESET_VIRT71,       /**<  Preset type Virtual 7.1 */
	MM_AUDIO_EFFECT_PRESET_STUDIO,       /**<  Preset type Studio */
	MM_AUDIO_EFFECT_PRESET_CLUB,         /**<  Preset type Club */
	MM_AUDIO_EFFECT_PRESET_NUM,          /**<  Number of Preset type */
} MMAudioEffectPresetType;

/**
 * Enumerations of Audio Effect Custom Type
 */
typedef enum {
	MM_AUDIO_EFFECT_CUSTOM_EQ = 0,       /**<  Custom type Equalizer */
	MM_AUDIO_EFFECT_CUSTOM_3D,           /**<  Custom type 3D */
	MM_AUDIO_EFFECT_CUSTOM_BASS,         /**<  Custom type Bass */
	MM_AUDIO_EFFECT_CUSTOM_ROOM_SIZE,    /**<  Custom type Room Size */
	MM_AUDIO_EFFECT_CUSTOM_REVERB_LEVEL, /**<  Custom type Reverb Level */
	MM_AUDIO_EFFECT_CUSTOM_CLARITY,      /**<  Custom type Clarity */
	MM_AUDIO_EFFECT_CUSTOM_NUM,          /**<  Number of Custom type */
} MMAudioEffectCustomType;

/**
 * Enumerations of Audio Effect Type
 */
typedef enum {
	MM_AUDIO_EFFECT_TYPE_NONE,
	MM_AUDIO_EFFECT_TYPE_PRESET,
	MM_AUDIO_EFFECT_TYPE_CUSTOM,
	MM_AUDIO_EFFECT_TYPE_SQUARE,
} MMAudioEffectType;


/**
 * Enumerations of Output Mode
 */
typedef enum {
	MM_AUDIO_EFFECT_OUTPUT_SPK,    /**< Speaker out */
	MM_AUDIO_EFFECT_OUTPUT_EAR,    /**< Earjack out */
	MM_AUDIO_EFFECT_OUTPUT_OTHERS, /**< MIRRORING out */
	MM_AUDIO_EFFECT_OUTPUT_BT,
	MM_AUDIO_EFFECT_OUTPUT_DOCK,
	MM_AUDIO_EFFECT_OUTPUT_MULTIMEDIA_DOCK,
	MM_AUDIO_EFFECT_OUTPUT_USB_AUDIO,
	MM_AUDIO_EFFECT_OUTPUT_HDMI,
	MM_AUDIO_EFFECT_OUTPUT_NUM
} MMAudioEffectOutputMode;


/**
 * Structure of AudioEffectInfo
 */
typedef struct {
	MMAudioEffectType effect_type;      /**< effect type, (NONE,PRESET,CUSTOM)*/
	MMAudioEffectPresetType preset;     /**< for preset type*/
	int *custom_ext_level_for_plugin;   /**< for custom type, level value list of Extension effects*/
	int custom_eq_level[MM_AUDIO_EFFECT_EQ_BAND_NUM_MAX];   /**< for custom type, EQ info*/
	int custom_ext_level[MM_AUDIO_EFFECT_CUSTOM_NUM-1];     /**< for custom type, Extension effect info*/
	int square_row;    /**< for square type, square row configure*/
	int square_col;    /**< for square type, square col configure*/
} MMAudioEffectInfo;


/**
 * @brief Called to get each supported audio effect.
 *
 * @param	effect_type	[in]	Type of effect (preset effect or custom effect).
 * @param	effect		[in]	Supported audio effect.
 * @param	user_data	[in]	Pointer of user data.
 *
 * @return	True to continue with the next iteration of the loop, False to break out of the loop.
 * @see		mm_player_get_foreach_present_supported_effect_type()
 */
typedef bool (*mmplayer_supported_audio_effect_cb) (int effect_type, int type, void *user_data);

/**
 * This function is to get supported effect type.
 *
 * @param	hplayer		[in]	Handle of player.
 * @param	effect_type	[in]	Type of effect.
 * @param	foreach_cb	[in]	Callback function to be passed the result.
 * @param	user_data	[in]	Pointer of user data.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_get_foreach_present_supported_effect_type(MMHandleType player, MMAudioEffectType effect_type, mmplayer_supported_audio_effect_cb foreach_cb, void *user_data);

/**
 * This function is to bypass audio effect.
 *
 * @param	hplayer		[in]	Handle of player.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_audio_effect_bypass (MMHandleType hplayer);

/**
 * This function is to apply custom effect(Equalizer and Extension effects).
 *
 * @param	hplayer		[in]	Handle of player.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_audio_effect_custom_apply(MMHandleType hplayer);

/**
 * This function is to clear Equalizer custom effect.
 *
 * @param	hplayer		[in]	Handle of player.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_audio_effect_custom_clear_eq_all(MMHandleType hplayer);

/**
 * This function is to clear Extension custom effects.
 *
 * @param	hplayer		[in]	Handle of player.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_audio_effect_custom_clear_ext_all(MMHandleType hplayer);

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
int mm_player_audio_effect_custom_get_eq_bands_number(MMHandleType hplayer, int *bands);

/**
 * This function is to get width of equalizer band of each index.
 *
 * @param	hplayer		[in]	Handle of player.
 * @param	band_idx	[in]	Index of band.
 * @param	width		[out]	Value of width.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_audio_effect_custom_get_eq_bands_width(MMHandleType hplayer, int band_idx, int *width);

/**
 * This function is to get frequency of equalizer band of each index.
 *
 * @param	hplayer		[in]	Handle of player.
 * @param	band_idx	[in]	Index of band.
 * @param	freq		[out]	Value of frequency.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_audio_effect_custom_get_eq_bands_freq(MMHandleType hplayer, int band_idx, int *freq);

/**
 * This function is to get the level of the custom effect.
 *
 * @param	hplayer		[in]	Handle of player.
 * @param	type		[in]	Custom type effect.
 * @param	eq_index	[in]	Equalizer band index. This parameter is available only when the type is MM_AUDIO_EFFECT_CUSTOM_EQ.
 * @param	level		[out]	The level of the custom effect.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see		MMAudioEffectCustomType
 * @since
 */
int mm_player_audio_effect_custom_get_level(MMHandleType hplayer, MMAudioEffectCustomType type, int eq_index, int *level);

/**
 * This function is to get range of the level of the custom effect.
 *
 * @param	hplayer		[in]	Handle of player.
 * @param	type		[in]	Custom type effect.
 * @param	min		[out]	Minimal value of level.
 * @param	max		[out]	Maximum value of level.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see		MMAudioEffectCustomType
 * @since
 */
int mm_player_audio_effect_custom_get_level_range(MMHandleType hplayer, MMAudioEffectCustomType type, int *min, int *max);

/**
 * This function is to set the level of the custom effect.
 *
 * @param	hplayer		[in]	Handle of player.
 * @param	type		[in]	Custom type effect.
 * @param	eq_index	[in]	Equalizer band index. This parameter is available only when the type is MM_AUDIO_EFFECT_CUSTOM_EQ.
 * @param	level		[in]	The level of the custom effect.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see		MMAudioEffectCustomType
 * @since
 */
int mm_player_audio_effect_custom_set_level(MMHandleType hplayer, MMAudioEffectCustomType effect_custom_type, int eq_index, int level);

/**
 * This function is to set the bands level of equalizer custom effect using input list.
 *
 * @param	hplayer		[in]	Handle of player.
 * @param	level_list	[in]	list of bands level of equalizer custom audio_effect want to set.
 * @param	size		[in]	size of level_list.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_audio_effect_custom_set_level_eq_from_list(MMHandleType hplayer, int *level_list, int size);

/**
 * This function is to decide if the preset type effect is supported or not
 *
 * @param	hplayer		[in]	Handle of player.
 * @param	effect		[in]	Preset type effect.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_is_supported_preset_effect_type(MMHandleType hplayer, MMAudioEffectPresetType effect);

/**
 * This function is to decide if the custom type effect is supported or not
 *
 * @param	hplayer		[in]	Handle of player.
 * @param	effect		[in]	Custom type effect.
 *
 * @return	This function returns zero on success, or negative value with error code.
 *
 * @remark
 * @see
 * @since
 */
int mm_player_is_supported_custom_effect_type(MMHandleType hplayer, MMAudioEffectCustomType effect);

/**
	@}
 */

#ifdef __cplusplus
	}
#endif

#endif	/* __MM_PLAYER_AUDIOEFFECT_H__ */
