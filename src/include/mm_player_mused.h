/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>
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

#ifndef __MM_PLAYER_MUSED_H__
#define __MM_PLAYER_MUSED_H__

/*===========================================================================================
|																							|
|  INCLUDE FILES																			|
|																							|
========================================================================================== */
#include "mm_types.h"
#include <Elementary.h>

/*===========================================================================================
|																							|
|  GLOBAL DEFINITIONS AND DECLARATIONS FOR MODULE											|
|																							|
========================================================================================== */

/*---------------------------------------------------------------------------
|    GLOBAL #defines:														|
---------------------------------------------------------------------------*/

#ifdef __cplusplus
	extern "C" {
#endif

int mm_player_mused_create(MMHandleType *player);
int mm_player_mused_destroy(MMHandleType player);
int mm_player_mused_realize(MMHandleType player, char *caps);
int mm_player_mused_unrealize(MMHandleType player);
int mm_player_mused_pre_unrealize(MMHandleType player);
int mm_player_get_state_timeout(MMHandleType player, int *timeout, bool is_streaming);
int mm_player_mused_set_evas_object_cb(MMHandleType player, Evas_Object * eo);
int mm_player_mused_unset_evas_object_cb(MMHandleType player);


/**
 * This function get string of raw video caps.
 * To be used by server.
 *
 * @param	player  [in] Handle of player.
 * @param	caps    [out] String of caps. Should be freed after used.
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 * @see
 * @since
 */
int mm_player_get_raw_video_caps(MMHandleType player, char **caps);

/**
 * This function set "socket-path" element property of shmsink/src.
 * To be used by both server and client.
 *
 * @param	player  [in] Handle of player.
 * @param	path    [in] Local file path.
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 * @see
 * @since
 */
int mm_player_set_shm_stream_path(MMHandleType player, const char *path);

#ifdef __cplusplus
	}
#endif

#endif /* __MM_PLAYER_MUSED_H__ */
