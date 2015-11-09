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

#include "mm_player_utils.h"
#include "mm_player_resource.h"
#include "mm_player_priv.h"
#include <murphy/common/glib-glue.h>

#define MRP_APP_CLASS_FOR_PLAYER   "media"
#define MRP_RESOURCE_TYPE_MANDATORY TRUE
#define MRP_RESOURCE_TYPE_EXCLUSIVE FALSE

enum {
	MRP_RESOURCE_FOR_VIDEO_OVERLAY,
	MRP_RESOURCE_FOR_VIDEO_DECODER,
	MRP_RESOURCE_MAX,
};
const char* resource_str[MRP_RESOURCE_MAX] = {
    "video_overlay",
    "video_decoder",
};

#define MMPLAYER_CHECK_RESOURCE_MANAGER_INSTANCE(x_player_resource_manager) \
do \
{ \
	if (!x_player_resource_manager) \
	{ \
		LOGE("no resource manager instance");\
		return MM_ERROR_INVALID_ARGUMENT; \
	} \
}while(0);

#define MMPLAYER_CHECK_CONNECTION_RESOURCE_MANAGER(x_player_resource_manager) \
do \
{ \
	if (!x_player_resource_manager) \
	{ \
		LOGE("no resource manager instance");\
		return MM_ERROR_INVALID_ARGUMENT; \
	} \
	else \
	{ \
		if (!x_player_resource_manager->is_connected) \
		{ \
			LOGE("not connected to resource server yet"); \
			return MM_ERROR_RESOURCE_NOT_INITIALIZED; \
		} \
	} \
}while(0);

static char *state_to_str(mrp_res_resource_state_t st)
{
	char *state = "unknown";
	switch (st) {
		case MRP_RES_RESOURCE_ACQUIRED:
			state = "acquired";
			break;
		case MRP_RES_RESOURCE_LOST:
			state = "lost";
			break;
		case MRP_RES_RESOURCE_AVAILABLE:
			state = "available";
			break;
		case MRP_RES_RESOURCE_PENDING:
			state = "pending";
			break;
		case MRP_RES_RESOURCE_ABOUT_TO_LOOSE:
			state = "about to loose";
			break;
	}
	return state;
}

static void mrp_state_callback(mrp_res_context_t *context, mrp_res_error_t err, void *user_data)
{
	int i = 0;
	const mrp_res_resource_set_t *rset;
	mrp_res_resource_t *resource;
	mm_player_t* player = NULL;

	MMPLAYER_FENTER();

	if (user_data == NULL)
	{
		LOGE(" - user data is null\n");
		return;
	}
	player = (mm_player_t*)user_data;
	if (err != MRP_RES_ERROR_NONE)
	{
		LOGE(" - error message received from Murphy, for the player(%p), err(0x%x)\n", player, err);
		return;
	}

	switch(context->state)
	{
		case MRP_RES_CONNECTED:
			LOGD(" - connected to Murphy\n");
			if ((rset = mrp_res_list_resources(context)) != NULL)
			{
				mrp_res_string_array_t *resource_names;
				resource_names = mrp_res_list_resource_names(rset);
				if (!resource_names)
				{
					LOGE(" - no resources available\n");
					return;
				}
				for(i = 0; i < resource_names->num_strings; i++)
				{
					resource = mrp_res_get_resource_by_name(rset, resource_names->strings[i]);
					if(resource)
					{
						LOGD(" - available resource: %s", resource->name);
					}
				}
				mrp_res_free_string_array(resource_names);
			}
			player->resource_manager.is_connected = TRUE;
			break;
		case MRP_RES_DISCONNECTED:
			LOGD(" - disconnected from Murphy\n");
			if (player->resource_manager.rset)
			{
				mrp_res_delete_resource_set(player->resource_manager.rset);
				player->resource_manager.rset = NULL;
			}
			mrp_res_destroy(player->resource_manager.context);
			player->resource_manager.context = NULL;
			player->resource_manager.is_connected = FALSE;
			break;
	}

	MMPLAYER_FLEAVE();

	return;
}

static void mrp_rset_state_callback(mrp_res_context_t *cx, const mrp_res_resource_set_t *rs, void *user_data)
{
	int i = 0;
	mm_player_t *player = (mm_player_t *)user_data;
	mrp_res_resource_t *res;

	MMPLAYER_FENTER();

	if(!mrp_res_equal_resource_set(rs, player->resource_manager.rset)){
		LOGW("- resource set(%p) is not same as this player handle's(%p)", rs, player->resource_manager.rset);
		return;
	}

	LOGD(" - resource set state of player(%p) is changed to [%s]\n", player, state_to_str(rs->state));
	for (i = 0; i < MRP_RESOURCE_MAX; i++)
	{
		res = mrp_res_get_resource_by_name(rs, resource_str[i]);
		if(res == NULL){
			LOGW(" -- %s not present in resource set\n", resource_str[i]);
		} else {
			LOGD(" -- resource name [%s] -> [%s]'\n", res->name, state_to_str(res->state));
		}
	}

	mrp_res_delete_resource_set(player->resource_manager.rset);
	player->resource_manager.rset = mrp_res_copy_resource_set(rs);

	MMPLAYER_FLEAVE();
}


static void mrp_resource_release_cb (mrp_res_context_t *cx, const mrp_res_resource_set_t *rs, void *user_data)
{
	int i = 0;
	int result = MM_ERROR_NONE;
	mm_player_t* player = NULL;
	mrp_res_resource_t *res;

	MMPLAYER_FENTER();

	if (user_data == NULL)
	{
		LOGE("- user_data is null\n");
		return;
	}
	player = (mm_player_t*)user_data;

	if(!mrp_res_equal_resource_set(rs, player->resource_manager.rset))
	{
		LOGW("- resource set(%p) is not same as this player handle's(%p)", rs, player->resource_manager.rset);
		return;
	}

	LOGD(" - resource set state of player(%p) is changed to [%s]\n", player, state_to_str(rs->state));
	for (i = 0; i < MRP_RESOURCE_MAX; i++)
	{
		res = mrp_res_get_resource_by_name(rs, resource_str[i]);
		if(res == NULL){
			LOGW(" -- %s not present in resource set\n", resource_str[i]);
		} else {
			LOGD(" -- resource name [%s] -> [%s]'\n", res->name, state_to_str(res->state));
		}
	}

	/* do something to release resource here.
	 * player stop and interrupt forwarding */
	if (!__mmplayer_can_do_interrupt(player))
		LOGW("no need to interrupt, so leave");
	else
	{
		if(player->pipeline->videobin)
		{
			player->resource_manager.by_rm_cb = TRUE;
			LOGD("video resource conflict so, resource will be freed by unrealizing");
			result = _mmplayer_unrealize((MMHandleType)player);
			if (result)
				LOGW("failed to unrealize");
			player->resource_manager.by_rm_cb = FALSE;
		}
		else
			LOGW("could not find videobin");
		MMPLAYER_CMD_UNLOCK(player);
	}

	MMPLAYER_FLEAVE();

	return;
}

static int create_rset(MMPlayerResourceManager *resource_manager)
{
	if (resource_manager->rset)
	{
		LOGE(" - resource set was already created\n");
		return MM_ERROR_RESOURCE_INVALID_STATE;
	}

	resource_manager->rset = mrp_res_create_resource_set(resource_manager->context,
				MRP_APP_CLASS_FOR_PLAYER,
				mrp_rset_state_callback,
				(void*)resource_manager->user_data);
	if(resource_manager->rset == NULL)
	{
		LOGE(" - could not create resource set\n");
		return MM_ERROR_RESOURCE_INTERNAL;
	}

	if(!mrp_res_set_autorelease(TRUE, resource_manager->rset))
	{
		LOGW(" - could not set autorelease flag!\n");
	}

	return MM_ERROR_NONE;
}

static int include_resource(MMPlayerResourceManager *resource_manager, const char *resource_name)
{
	mrp_res_resource_t *resource = NULL;
	resource = mrp_res_create_resource(resource_manager->rset,
				resource_name,
				MRP_RESOURCE_TYPE_MANDATORY,
				MRP_RESOURCE_TYPE_EXCLUSIVE);
	if (resource == NULL)
	{
		LOGE(" - could not include resource[%s]\n", resource_name);
		return MM_ERROR_RESOURCE_INTERNAL;
	}

	LOGD(" - include resource[%s]\n", resource_name);

	return MM_ERROR_NONE;
}

static int set_resource_release_cb(MMPlayerResourceManager *resource_manager)
{
	int ret = MM_ERROR_NONE;
	bool mrp_ret = FALSE;

	if (resource_manager->rset) {
		mrp_ret = mrp_res_set_release_callback(resource_manager->rset, mrp_resource_release_cb, resource_manager->user_data);
		if (!mrp_ret)
		{
			LOGE(" - could not set release callback\n");
			ret = MM_ERROR_RESOURCE_INTERNAL;
		}
	} else {
		LOGE(" - resource set is null\n");
		ret = MM_ERROR_RESOURCE_INVALID_STATE;
	}

	return ret;
}

int _mmplayer_resource_manager_init(MMPlayerResourceManager *resource_manager, void *user_data)
{
	MMPLAYER_FENTER();
	MMPLAYER_CHECK_RESOURCE_MANAGER_INSTANCE(resource_manager);

	resource_manager->mloop = mrp_mainloop_glib_get(g_main_loop_new(NULL, TRUE));
	if (resource_manager->mloop)
	{
		resource_manager->context = mrp_res_create(resource_manager->mloop, mrp_state_callback, user_data);
		if (resource_manager->context == NULL)
		{
			LOGE(" - could not get context for resource manager\n");
			mrp_mainloop_destroy(resource_manager->mloop);
			resource_manager->mloop = NULL;
			return MM_ERROR_RESOURCE_INTERNAL;
		}
		resource_manager->user_data = user_data;
	}
	else
	{
		LOGE("- could not get mainloop for resource manager\n");
		return MM_ERROR_RESOURCE_INTERNAL;
	}

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}

int _mmplayer_resource_manager_prepare(MMPlayerResourceManager *resource_manager, MMPlayerResourceType resource_type)
{
	int ret = MM_ERROR_NONE;
	MMPLAYER_FENTER();
	MMPLAYER_CHECK_RESOURCE_MANAGER_INSTANCE(resource_manager);
	MMPLAYER_CHECK_CONNECTION_RESOURCE_MANAGER(resource_manager);

	if (!resource_manager->rset)
	{
		ret = create_rset(resource_manager);
	}
	if (ret == MM_ERROR_NONE)
	{
		switch (resource_type)
		{
		case RESOURCE_TYPE_VIDEO_OVERLAY:
			ret = include_resource(resource_manager, resource_str[MRP_RESOURCE_FOR_VIDEO_OVERLAY]);
			break;
		case RESOURCE_TYPE_VIDEO_DECODER:
			ret = include_resource(resource_manager, resource_str[MRP_RESOURCE_FOR_VIDEO_DECODER]);
			break;
		}
	}

	MMPLAYER_FLEAVE();

	return ret;
}

int _mmplayer_resource_manager_acquire(MMPlayerResourceManager *resource_manager)
{
	int ret = MM_ERROR_NONE;
	MMPLAYER_FENTER();
	MMPLAYER_CHECK_RESOURCE_MANAGER_INSTANCE(resource_manager);
	MMPLAYER_CHECK_CONNECTION_RESOURCE_MANAGER(resource_manager);

	if (resource_manager->rset == NULL)
	{
		LOGE("- could not acquire resource, resource set is null\n");
		ret = MM_ERROR_RESOURCE_INVALID_STATE;
	}
	else
	{
		ret = set_resource_release_cb(resource_manager);
		if (ret)
		{
			LOGE("- could not set resource release cb, ret(%d)\n", ret);
			ret = MM_ERROR_RESOURCE_INTERNAL;
		}
		else
		{
			ret = mrp_res_acquire_resource_set(resource_manager->rset);
			if (ret)
			{
				LOGE("- could not acquire resource, ret(%d)\n", ret);
				ret = MM_ERROR_RESOURCE_INTERNAL;
			}
		}
	}

	MMPLAYER_FLEAVE();

	return ret;
}

int _mmplayer_resource_manager_release(MMPlayerResourceManager *resource_manager)
{
	int ret = MM_ERROR_NONE;
	MMPLAYER_FENTER();
	MMPLAYER_CHECK_RESOURCE_MANAGER_INSTANCE(resource_manager);
	MMPLAYER_CHECK_CONNECTION_RESOURCE_MANAGER(resource_manager);

	if (resource_manager->rset == NULL)
	{
		LOGE("- could not release resource, resource set is null\n");
		ret = MM_ERROR_RESOURCE_INVALID_STATE;
	}
	else
	{
		if (resource_manager->rset->state != MRP_RES_RESOURCE_ACQUIRED)
		{
			LOGE("- could not release resource, resource set state is [%s]\n", state_to_str(resource_manager->rset->state));
			ret = MM_ERROR_RESOURCE_INVALID_STATE;
		}
		else
		{
			ret = mrp_res_release_resource_set(resource_manager->rset);
			if (ret)
			{
				LOGE("- could not release resource, ret(%d)\n", ret);
				ret = MM_ERROR_RESOURCE_INTERNAL;
			}
		}
	}

	MMPLAYER_FLEAVE();

	return ret;
}

int _mmplayer_resource_manager_unprepare(MMPlayerResourceManager *resource_manager)
{
	int ret = MM_ERROR_NONE;
	MMPLAYER_FENTER();
	MMPLAYER_CHECK_RESOURCE_MANAGER_INSTANCE(resource_manager);
	MMPLAYER_CHECK_CONNECTION_RESOURCE_MANAGER(resource_manager);

	if (resource_manager->rset == NULL)
	{
		LOGE("- could not unprepare for resource_manager, _mmplayer_resource_manager_prepare() first\n");
		ret = MM_ERROR_RESOURCE_INVALID_STATE;
	}
	else
	{
		mrp_res_delete_resource_set(resource_manager->rset);
		resource_manager->rset = NULL;
	}

	MMPLAYER_FLEAVE();

	return ret;
}

int _mmplayer_resource_manager_deinit(MMPlayerResourceManager *resource_manager)
{
	MMPLAYER_FENTER();
	MMPLAYER_CHECK_RESOURCE_MANAGER_INSTANCE(resource_manager);
	MMPLAYER_CHECK_CONNECTION_RESOURCE_MANAGER(resource_manager);

	if (resource_manager->rset)
	{
		if (resource_manager->rset->state == MRP_RES_RESOURCE_ACQUIRED)
		{
			if (mrp_res_release_resource_set(resource_manager->rset))
				LOGE("- could not release resource\n");
		}
		mrp_res_delete_resource_set(resource_manager->rset);
		resource_manager->rset = NULL;
	}
	if (resource_manager->context)
	{
		mrp_res_destroy(resource_manager->context);
		resource_manager->context = NULL;
	}
	if (resource_manager->mloop)
	{
		mrp_mainloop_destroy(resource_manager->mloop);
		resource_manager->mloop = NULL;
	}

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;
}
