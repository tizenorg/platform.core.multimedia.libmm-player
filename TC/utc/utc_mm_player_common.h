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

#ifndef __UTS_MMF_PLAYER_COMMON_H_
#define __UTS_MMF_PLAYER_COMMON_H_

#include <mm_player.h>
#include <mm_message.h>
#include <mm_error.h>
#include <mm_types.h>
#include <string.h>
#include <tet_api.h>
#include <unistd.h>
#include <glib.h>

#define MP3_FILE "file:///opt/media/Sounds and Music/Music/Over the horizon.mp3"
#define MP4_FILE "file:///opt/media/Images and videos/My video clips/Helicopter.mp4"
#define SUBTITLE_PATH "./test_data/legend.sub"

#define MAX_STRING_LEN 256

#define UTC_MM_LOG(fmt, args...)	tet_printf("[%s(L%d)]:"fmt"\n", __FUNCTION__, __LINE__, ##args)

#define UTC_MM_PLAYER_CREATE(x_player, x_ret) \
do \
{ \
	x_ret = mm_player_create(x_player); \
	dts_check_eq( "mm_player_create", x_ret, MM_ERROR_NONE, "unable to create player handle, error code->%x", x_ret ); \
} \
while(0)

#define UTC_MM_PLAYER_REALIZE(x_player, x_ret)	\
do \
{ \
	x_ret = mm_player_realize(x_player); \
	dts_check_eq( "mm_player_realize", x_ret, MM_ERROR_NONE, "unable to realize player handle, error code->%x", x_ret ); \
} \
while(0)

#define UTC_MM_PLAYER_UNREALIZE(x_player, x_ret) \
do \
{ \
	x_ret = mm_player_unrealize(x_player); \
	dts_check_eq( "mm_player_unrealize", x_ret, MM_ERROR_NONE, "unable to unrealize player handle, error code->%x", x_ret ); \
} \
while(0)

#define UTC_MM_PLAYER_DESTROY(x_player, x_ret) \
do \
{ \
	x_ret = mm_player_destroy(x_player); \
	dts_check_eq( "mm_player_destroy", x_ret, MM_ERROR_NONE, "unable to destroy player handle, error code->%x", x_ret ); \
} \
while(0)

#define UTC_MM_PLAYER_START(x_player, x_ret) \
do \
{ \
	x_ret = mm_player_start(x_player); \
	dts_check_eq( "mm_player_start", x_ret, MM_ERROR_NONE, "unable to start player handle, error code->%x", x_ret ); \
} \
while(0)

#define UTC_MM_PLAYER_RUN_LOOP(x_loop) \
do\
{\
	x_loop = g_main_loop_new(NULL, FALSE);\
	g_main_loop_run(x_loop); \
} \
while(0)

#define UTC_MM_PLAYER_QUIT_LOOP(x_loop) \
do\
{\
	g_main_loop_quit(x_loop); \
} \
while(0)

/** test case startup function */
void Startup();
/** test case clean up function */
void Cleanup();

#endif //__UTS_MMF_PLAYER_COMMON_H_

