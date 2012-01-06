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
 
#include "utc_mm_player_set_volume_func.h"




struct tet_testlist tet_testlist[] = {
	{utc_mm_player_set_volume_func_01, 1},
	{utc_mm_player_set_volume_func_02, 2},		
	{NULL, 0}
};

GMainLoop *g_set_vol_loop;
bool g_is_set_vol_positive = 0;

bool msg_callback(int message, MMMessageParamType *param, void *user_param)
{
	int ret = 0;
	MMHandleType player = 0;
	player = (MMHandleType) user_param;
	MMPlayerVolumeType volume;

	switch (message) {
		case MM_MESSAGE_STATE_CHANGED:
			switch(param->state.current)
			{
				case MM_PLAYER_STATE_NONE:
					g_print("                                                            ==> [PLAYER_UTC] Player is [NULL]\n");
					break;
				case MM_PLAYER_STATE_READY:
					g_print("                                                            ==> [PLAYER_UTC] Player is [READY]\n");
					break;
				case MM_PLAYER_STATE_PLAYING:
					g_print("                                                            ==> [PLAYER_UTC] Player is [PLAYING]\n");
					int pos = 4000; // 4sec
					if (g_is_set_vol_positive)
					{
						volume.level[MM_VOLUME_CHANNEL_LEFT] = volume.level[MM_VOLUME_CHANNEL_RIGHT] = 0.5;
						ret = mm_player_set_volume(player, &volume);
						dts_check_eq(__func__, ret, MM_ERROR_NONE, "err=%x", ret );
					}
					else
					{
						ret = mm_player_set_volume(player, NULL);
						dts_check_ne(__func__, ret, MM_ERROR_NONE, "err=%x", ret );
					}

					// XX_fucn_01 can return when g_main_loop is quit.
					UTC_MM_PLAYER_QUIT_LOOP(g_set_vol_loop);
				
					break;
				case MM_PLAYER_STATE_PAUSED:
					g_print("                                                            ==> [PLAYER_UTC] Player is [PAUSED]\n");
					break;
			}
			break;
		}

	return 1;
}

/**
* @brief	This tests int mm_player_set_volume() API with valid parameter
* 		Create a player handle with valid parameter & Test the handle by playing
* @par ID	utc_mm_player_set_volume_func_01
* @param	[in] &player = handle of player to be populated
* @return	This function returns zero on success, or negative value with error code
*/
void utc_mm_player_set_volume_func_01()
{
	MMHandleType player = 0;
	int ret = 0;
	MMPlayerVolumeType volume;

	UTC_MM_PLAYER_CREATE(&player, ret);
	
	mm_player_set_attribute(player,
							NULL,
							"profile_uri", MP3_FILE, strlen(MP3_FILE),
							NULL);

	mm_player_set_message_callback(player, msg_callback, (void*)player);
	
	UTC_MM_PLAYER_REALIZE(player, ret);
	UTC_MM_PLAYER_START(player, ret);	

	// XX_fucn_01 is blocking here until message callback receives PLAYING message. 
	UTC_MM_PLAYER_RUN_LOOP(g_set_vol_loop);

	UTC_MM_PLAYER_UNREALIZE(player, ret);
	UTC_MM_PLAYER_DESTROY(player, ret);
	
	return;
}


/**
* @brief 		This tests int mm_player_set_volume() API with invalid parameter
* 			Create a player handle with a NULL out param
* @par ID	utc_mm_player_set_volume_func_02
* @param	[in] &player = NULL
* @return	error code on success 
*/
void utc_mm_player_set_volume_func_02()
{	
	MMHandleType player = 0;
	int ret = 0;

	UTC_MM_PLAYER_CREATE(&player, ret);
	
	mm_player_set_attribute(player,
							NULL,
							"profile_uri", MP3_FILE, strlen(MP3_FILE),
							NULL);

	mm_player_set_message_callback(player, msg_callback, (void*)player);	
	
	UTC_MM_PLAYER_REALIZE(player, ret);
	UTC_MM_PLAYER_START(player, ret);

	// XX_fucn_01 is blocking here until message callback receives PLAYING message. 
	UTC_MM_PLAYER_RUN_LOOP(g_set_vol_loop);
	
	UTC_MM_PLAYER_UNREALIZE(player, ret);
	UTC_MM_PLAYER_DESTROY(player, ret);

	return ;
}
