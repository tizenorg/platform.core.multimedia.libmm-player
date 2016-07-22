/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Heechul Jeon <heechul.jeon@samsung.com>
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

#ifndef __MM_PLAYER_RESOURCE_H__
#define __MM_PLAYER_RESOURCE_H__

#include <murphy/plugins/resource-native/libmurphy-resource/resource-api.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MMPLAYER_RESOURCE_TIMEOUT 5

#define MMPLAYER_GET_RESOURCE_LOCK(rm)	(&((MMPlayerResourceManager *)rm)->lock)
#define MMPLAYER_RESOURCE_LOCK(rm)		(g_mutex_lock(MMPLAYER_GET_RESOURCE_LOCK(rm)))
#define MMPLAYER_RESOURCE_UNLOCK(rm)	(g_mutex_unlock(MMPLAYER_GET_RESOURCE_LOCK(rm)))

#define MMPLAYER_GET_RESOURCE_COND(rm)	(&((MMPlayerResourceManager *)rm)->cond)
#define MMPLAYER_RESOURCE_WAIT(rm)		g_cond_wait(MMPLAYER_GET_RESOURCE_COND(rm), MMPLAYER_GET_RESOURCE_LOCK(rm)
#define MMPLAYER_RESOURCE_WAIT_UNTIL(rm, end_time) \
	g_cond_wait_until(MMPLAYER_GET_RESOURCE_COND(rm), MMPLAYER_GET_RESOURCE_LOCK(rm), end_time)
#define MMPLAYER_RESOURCE_SIGNAL(rm)	g_cond_signal(MMPLAYER_GET_RESOURCE_COND(rm));

typedef enum {
	RESOURCE_TYPE_VIDEO_DECODER,
	RESOURCE_TYPE_VIDEO_OVERLAY,
} MMPlayerResourceType;

typedef enum {
	RESOURCE_STATE_NONE,
	RESOURCE_STATE_INITIALIZED,
	RESOURCE_STATE_PREPARED,
	RESOURCE_STATE_ACQUIRED,
	RESOURCE_STATE_MAX,
} MMPlayerResourceState;

typedef struct {
	mrp_mainloop_t *mloop;
	mrp_res_context_t *context;
	mrp_res_resource_set_t *rset;
	MMPlayerResourceState state;
	bool is_connected;
	void *user_data;
	bool by_rm_cb;
	GCond cond;
	GMutex lock;
} MMPlayerResourceManager;

int _mmplayer_resource_manager_init(MMPlayerResourceManager *resource_manager, void *user_data);
int _mmplayer_resource_manager_prepare(MMPlayerResourceManager *resource_manager, MMPlayerResourceType resource_type);
int _mmplayer_resource_manager_acquire(MMPlayerResourceManager *resource_manager);
int _mmplayer_resource_manager_release(MMPlayerResourceManager *resource_manager);
int _mmplayer_resource_manager_unprepare(MMPlayerResourceManager *resource_manager);
int _mmplayer_resource_manager_deinit(MMPlayerResourceManager *resource_manager);
int _mmplayer_resource_manager_get_state(MMPlayerResourceManager *resource_manager, MMPlayerResourceState *state);


#ifdef __cplusplus
}
#endif

#endif /* __MM_PLAYER_RESOURCE_H__ */
