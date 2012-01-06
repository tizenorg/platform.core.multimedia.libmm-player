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

#include "utc_mm_player_start_func.h"

struct tet_testlist tet_testlist[] = {
	{utc_mm_player_start_func_01, 1},
	{utc_mm_player_start_func_02, 2},	
	{NULL, 0}
};


/**
* @brief	This tests int mm_player_start() API with valid parameter
* 		Create a player handle with valid parameter & Test the handle by playing
* @par ID	utc_mm_player_start_func_01
* @param	[in] &player = handle of player to be populated
* @return	This function returns zero on success, or negative value with error code
*/
void utc_mm_player_start_func_01()
{
	MMHandleType player = 0;
	int ret = 0;

	UTC_MM_PLAYER_CREATE(&player, ret);
	
	mm_player_set_attribute(player,
							NULL,
							"profile_uri", MP3_FILE, strlen(MP3_FILE),
							NULL);
	
	UTC_MM_PLAYER_REALIZE(player, ret);
	
	ret = mm_player_start(player);
	
	dts_check_eq(__func__, ret, MM_ERROR_NONE, "err=%x", ret );

	UTC_MM_PLAYER_UNREALIZE(player, ret);
	UTC_MM_PLAYER_DESTROY(player, ret);	
	
	return;
}


/**
* @brief 		This tests int mm_player_start() API with invalid parameter
* 			Create a player handle with a NULL out param
* @par ID	utc_mm_player_start_func_02
* @param	[in] &player = NULL
* @return	error code on success 
*/
void utc_mm_player_start_func_02()
{	
	MMHandleType player = 0;
	int ret = 0;

	UTC_MM_PLAYER_CREATE(&player, ret);
	
	mm_player_set_attribute(player,
							NULL,
							"profile_uri", MP3_FILE, strlen(MP3_FILE),
							NULL);
	
	UTC_MM_PLAYER_REALIZE(player, ret);

	ret = mm_player_start(NULL);
	
	dts_check_ne(__func__, ret, MM_ERROR_NONE, "err=%x", ret );

	UTC_MM_PLAYER_UNREALIZE(player, ret);
	UTC_MM_PLAYER_DESTROY(player, ret);
	
	return ;
}
