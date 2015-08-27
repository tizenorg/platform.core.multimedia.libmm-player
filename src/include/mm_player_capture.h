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

#ifndef __MM_PLAYER_CAPTURE_H__
#define __MM_PLAYER_CAPTURE_H__

/*=======================================================================================
| INCLUDE FILES										|
========================================================================================*/
#include <mm_types.h>
#include "mm_player_priv.h"

#ifdef __cplusplus
	extern "C" {
#endif

/*=======================================================================================
| GLOBAL FUNCTION PROTOTYPES								|
========================================================================================*/
/**
 * This function is to initialize video capture
 *
 * @param[in]	handle		Handle of player.
 * @return	This function returns zero on success, or negative value with errors.
 * @remarks
 * @see
 *
 */
int _mmplayer_initialize_video_capture(mm_player_t* player);
/**
 * This function is to release video capture
 *
 * @param[in]	handle		Handle of player.
 * @return	This function returns zero on success, or negative value with errors.
 * @remarks
 * @see
 *
 */
int _mmplayer_release_video_capture(mm_player_t* player);
/**
 * This function is to get video snapshot during playback.
 *
 * @param[in]	handle		Handle of player.
 * @return	This function returns zero on success, or negative value with errors.
 * @remarks
 * @see
 *
 */
int _mmplayer_do_video_capture(MMHandleType hplayer);

#ifdef __cplusplus
	}
#endif

#endif

