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

#define MMDNSE_EQ_CUSTOMBAND_MAX	8

/**
	@addtogroup PLAYER_INTERNAL

*/

/**
 * Enumerations of 3D Sound Mode
 */
enum MM3DSoundMode {
	MM_3DSOUND_WIDE = 1,		/**< Wide */
	MM_3DSOUND_DYNAMIC,		/**< Dynamic */
	MM_3DSOUND_SURROUND,	/**< Surround */
};

/**
 * Enumerations of EQ Mode
 */
enum MMEqMode {
	MM_EQ_ROCK = 1,				/**<  Rock mode*/
	MM_EQ_JAZZ,					/**<  Jazz mode*/
	MM_EQ_LIVE,					/**<  Live mode*/
	MM_EQ_CLASSIC,				/**<  Classic mode*/
	MM_EQ_FULL_BASS,				/**<  Bass mode*/
	MM_EQ_FULL_BASS_AND_TREBLE,	/**<  Bass and Treble mode*/
	MM_EQ_DANCE,					/**<  Dance mode*/
	MM_EQ_POP,					/**<  Pop mode*/
	MM_EQ_FULL_TREBLE,			/**<  Treble mode*/
	MM_EQ_CLUB,					/**<  Club mode*/
	MM_EQ_PARTY,					/**<  Party mode*/
	MM_EQ_LARGE_HALL,				/**<  Large Hall mode*/
	MM_EQ_SOFT,					/**<  Soft mode*/
	MM_EQ_SOFT_ROCK,				/**<  Soft Rock mode*/
	MM_EQ_USER,					/**<  User mode*/
};

/**
 * Enumerations of Reverb Mode
 */
enum MMReverbMode {
	MM_REVERB_JAZZ_CLUB = 1,/**<  Jazz club mode*/
	MM_REVERB_CONCERT_HALL,	/**<  Concert Hall mode*/
	MM_REVERB_STADIUM,	/**<  Stadium mode*/
};

/**
 * Enumerations of Filter Type
 */
typedef enum {
	MM_AUDIO_FILTER_NONE			= 0x00000000,	/**<  Filter type None*/
	MM_AUDIO_FILTER_3D		 	= 0x00000001,	/**<  Filter type 3D*/
	MM_AUDIO_FILTER_EQUALIZER	= 0x00000002,	/**<  Filter type Equalizer*/
	MM_AUDIO_FILTER_REVERB	 	= 0x00000004,	/**<  Filter type Reverb*/
	MM_AUDIO_FILTER_SV 			= 0x00000008,	/**<  Filter type Specturm View*/
	MM_AUDIO_FILTER_BE 			= 0x00000010,	/**<  Filter type Bass Enhancement*/
	MM_AUDIO_FILTER_AEQ 			= 0x00000020,	/**<  Filter type AEQ*/
	MM_AUDIO_FILTER_MC			= 0x00000030, 	/**<  Filter type Music Clarity*/
	MM_AUDIO_FILTER_MTMV 		= 0x00000040,	/**<  Filter type M-Theater Movie*/
	MM_AUDIO_FILTER_VOICE    		= 0x00000080,  	/**<  Filter type Voice Booster */
	MM_AUDIO_FILTER_SRSCSHP		= 0x00000100, 	/**<  Filter type 5.1 SRS Circle Surround */
	MM_AUDIO_FILTER_ARKAMYS		= 0x00000200,	/**<  Filter type 5.1 Arkamys */
	MM_AUDIO_FILTER_WOWHD		= 0x00000400,	/**<  Filter type Wow HD */
	MM_AUDIO_FILTER_SOUND_EX	= 0x00000800		/**<  Filter type Sound Externalization */
} MMAudioFilterType;

/**
 * Structure of 3D
 */
typedef struct {
	int mode;	/**<  3D mode*/
} MMAudioFilter3D;

/**
 * Structure of EQ
 */
typedef struct {
	int mode;									/**<  EQ mode*/
	short user_eq[MMDNSE_EQ_CUSTOMBAND_MAX];	/**<  Value of User EQ*/
} MMAudioFilterEq;

/**
 * Structure of Reverb
 */
typedef struct {
	int mode;		/**<  Reverb mode*/
} MMAudioFilterReverb;

/**
 * Enumerations of Output Mode
 */
typedef enum {
	MM_AUDIO_FILTER_OUTPUT_SPK,		/**< Speaker out */
	MM_AUDIO_FILTER_OUTPUT_EAR		/**< Earjack out */
} MMAudioFilterOutputMode;

/**
 * Enumerations of Filter Client
 */
typedef enum {
	MM_AUDIO_FILTER_CLIENT_NONE,
	MM_AUDIO_FILTER_CLIENT_MUSIC_PLAYER,
	MM_AUDIO_FILTER_CLIENT_MAX
}MMAudioFilterClient;

/**
 * Structure of FilterInfo
 */
typedef struct {
	MMAudioFilterClient		app_id;			/**< Filter client*/
	MMAudioFilterType			filter_type;		/**< Filter type*/
	MMAudioFilterOutputMode	output_mode;	/**< Output mode*/
	MMAudioFilter3D 			sound_3d;		/**< 3D sound info*/
	MMAudioFilterEq 			equalizer;		/**< Equalizer info*/
	MMAudioFilterReverb 		reverb;			/**< Reverb info*/
} MMAudioFilterInfo;

/**
 * This function is to apply sound effect.
 *
 * @param	player		[in]	Handle of player.
 * @param   info	    [in] 	Filter info want to apply.
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 *
 * @remark
 * @see		MMAudioFilterInfo
 * @since
 */
int mm_player_apply_sound_filter(MMHandleType player, MMAudioFilterInfo *info);

/**
	@}
 */

#ifdef __cplusplus
	}
#endif

#endif	/* __MM_PLAYER_SNDEFFECT_H__ */
