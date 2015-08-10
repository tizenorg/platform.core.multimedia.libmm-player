/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, naveen cherukuri <naveen.ch@samsung.com>,
 * YeJin Cho <cho.yejin@samsung.com>, YoungHwan An <younghwan_.an@samsung.com>
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
#include <mm_debug.h>
#include <mm_error.h>
#include <mm_attrs_private.h>
#include "mm_player_utils.h"
#include "mm_player_tracks.h"

/*---------------------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:								                      |
---------------------------------------------------------------------------------------*/
static int __mmplayer_track_get_language(mm_player_t* player, MMPlayerTrackType type, gint stream_index, gchar **code);


/*=======================================================================================
|  FUNCTION DEFINITIONS									                     |
=======================================================================================*/
int _mmplayer_get_track_count(MMHandleType hplayer,  MMPlayerTrackType type, int *count)
{
	mm_player_t* player = (mm_player_t*) hplayer;
	MMHandleType attrs = 0;
	int ret = MM_ERROR_NONE;

	MMPLAYER_FENTER();

	/* check player handle */
	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(count, MM_ERROR_COMMON_INVALID_ARGUMENT);
	return_val_if_fail((MMPLAYER_CURRENT_STATE(player) != MM_PLAYER_STATE_PAUSED)
		 ||(MMPLAYER_CURRENT_STATE(player) != MM_PLAYER_STATE_PLAYING),
		MM_ERROR_PLAYER_INVALID_STATE);

	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs )
	{
		debug_error("cannot get content attribute");
		return MM_ERROR_PLAYER_INTERNAL;
	}

	switch (type)
	{
		case MM_PLAYER_TRACK_TYPE_AUDIO:
			{
				/*if function called for normal file [no multi audio] */
				if(player->selector[MM_PLAYER_TRACK_TYPE_AUDIO].total_track_num <= 0)
				{
					*count = 0;
					break;
				}
				ret = mm_attrs_get_int_by_name(attrs, "content_audio_track_num", count);
			}
			break;
		case MM_PLAYER_TRACK_TYPE_TEXT:
			ret = mm_attrs_get_int_by_name(attrs, "content_text_track_num", count);
			break;
		default:
			ret = MM_ERROR_COMMON_INVALID_ARGUMENT;
			break;
	}

	debug_log ("%d track num : %d\n", type, *count);

	MMPLAYER_FLEAVE();

	return ret;
}

int _mmplayer_select_track(MMHandleType hplayer, MMPlayerTrackType type, int index)
{
	int ret = MM_ERROR_NONE;
	mm_player_t* player = (mm_player_t*) hplayer;
	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	MMPLAYER_FENTER();


	if (type == MM_PLAYER_TRACK_TYPE_TEXT && player->subtitle_language_list)
	{
		GstElement *subparse = NULL;
		MMPlayerLangStruct *temp = NULL;
		unsigned long cur_time = 0;

		if(!player->pipeline || !player->pipeline->textbin[MMPLAYER_T_FAKE_SINK].gst)
		{
			ret = MM_ERROR_PLAYER_NOT_INITIALIZED;
			goto EXIT;
		}

		_mmplayer_get_position (hplayer, MM_PLAYER_POS_FORMAT_TIME, &cur_time);
		temp = g_list_nth_data (player->subtitle_language_list, index);

		subparse = player->pipeline->mainbin[MMPLAYER_M_SUBPARSE].gst;
		debug_log("setting to language %s", temp->language_code);
		g_object_set (G_OBJECT (subparse), "current-language", temp->language_key, NULL);

		_mmplayer_sync_subtitle_pipeline(player);

	}
	else
	{
		ret = _mmplayer_change_track_language (hplayer, type, index);
	}

EXIT:
	MMPLAYER_FLEAVE();
	return ret;
}

#ifdef _MULTI_TRACK
int _mmplayer_track_add_subtitle_language(MMHandleType hplayer, int index)
{
	int ret = MM_ERROR_NONE;
	mm_player_t* player = (mm_player_t*) hplayer;
	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	MMPLAYER_FENTER();

	if(!player->pipeline || !player->pipeline->mainbin[MMPLAYER_M_T_SUBMUX_EXTERNAL].gst)
	{
		ret = MM_ERROR_PLAYER_NOT_INITIALIZED;
		goto EXIT;
	}

	if (player->subtitle_language_list)
	{
		GstElement *subparse = NULL;
		MMPlayerLangStruct *temp = NULL;

		temp = g_list_nth_data (player->subtitle_language_list, index);
		temp->active = TRUE;

		subparse = player->pipeline->mainbin[MMPLAYER_M_T_SUBMUX_EXTERNAL].gst;
		debug_log("adding to language %s", temp->language_code);
		g_object_set (G_OBJECT (subparse), "current-language", temp->language_key, NULL);
		g_object_set (G_OBJECT (subparse), "lang-list", player->subtitle_language_list, NULL);

		_mmplayer_sync_subtitle_pipeline(player);
	}
	else
	{
		debug_warning("It is for just subtitle track");
		ret = MM_ERROR_PLAYER_NO_OP;
		goto EXIT;
	}

EXIT:
	MMPLAYER_FLEAVE();
	return ret;
}

int _mmplayer_track_remove_subtitle_language(MMHandleType hplayer, int index)
{
	int ret = MM_ERROR_NONE;
	mm_player_t* player = (mm_player_t*) hplayer;
	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	MMPLAYER_FENTER();

	if(!player->pipeline || !player->pipeline->mainbin[MMPLAYER_M_T_SUBMUX_EXTERNAL].gst)
	{
		ret = MM_ERROR_PLAYER_NOT_INITIALIZED;
		goto EXIT;
	}

	if (player->subtitle_language_list)
	{
		GstElement *subparse = NULL;
		MMPlayerLangStruct *temp = NULL;

		temp = g_list_nth_data (player->subtitle_language_list, index);
		temp->active = FALSE;

		subparse = player->pipeline->mainbin[MMPLAYER_M_T_SUBMUX_EXTERNAL].gst;
		debug_log("removing to language %s", temp->language_code);
		g_object_set (G_OBJECT (subparse), "current-language", temp->language_key, NULL);
		g_object_set (G_OBJECT (subparse), "lang-list", player->subtitle_language_list, NULL);

		_mmplayer_sync_subtitle_pipeline(player);
	}
	else
	{
		debug_warning("It is for just subtitle track");
		ret = MM_ERROR_PLAYER_NO_OP;
		goto EXIT;
	}

EXIT:
	MMPLAYER_FLEAVE();
	return ret;
}
#endif
int _mmplayer_get_current_track(MMHandleType hplayer, MMPlayerTrackType type, int *index)
{
	int ret = MM_ERROR_NONE;
	mm_player_t* player = (mm_player_t*) hplayer;
	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	MMPLAYER_FENTER();

	if (type >= MM_PLAYER_TRACK_TYPE_MAX)
	{
		ret = MM_ERROR_INVALID_ARGUMENT;
		debug_log("Not a proper type [type:%d] \n", type);
		goto EXIT;
	}

	if (type == MM_PLAYER_TRACK_TYPE_TEXT && player->subtitle_language_list)
	{
		GstElement *subparse = NULL;
		int total_track_count = 0;
		gchar* current_language = NULL;
		MMPlayerLangStruct *temp = NULL;
		MMHandleType attrs = 0;

		attrs = MMPLAYER_GET_ATTRS(player);
		if (!attrs)
		{
			debug_error("cannot get content attribute");
			ret = MM_ERROR_PLAYER_INTERNAL;
			goto EXIT;
		}

		mm_attrs_get_int_by_name(attrs, "content_text_track_num", &total_track_count);

		subparse = player->pipeline->mainbin[MMPLAYER_M_SUBPARSE].gst;
		g_object_get (G_OBJECT (subparse), "current-language", &current_language, NULL);
		debug_log("current language is %s ",current_language);
		while (total_track_count)
		{
			temp = g_list_nth_data (player->subtitle_language_list, total_track_count - 1);
			if (temp)
			{
				debug_log("find the list");
				if (!strcmp(temp->language_key, current_language))
				{
					*index = total_track_count - 1;
					debug_log("current lang index  is %d", *index);
					break;
				}
			}
			total_track_count--;
		}
	}
	else
	{
		if (player->selector[type].total_track_num <= 0)
		{
			ret = MM_ERROR_PLAYER_NO_OP;
			debug_log("there is no track information [type:%d] \n", type);
			goto EXIT;
		}

		*index = player->selector[type].active_pad_index;
	}

EXIT:
	MMPLAYER_FLEAVE();
	return ret;
}

int _mmplayer_get_track_language_code(MMHandleType hplayer, MMPlayerTrackType type, int index, char **code)
{
	int ret = MM_ERROR_NONE;

	return_val_if_fail(hplayer, MM_ERROR_PLAYER_NOT_INITIALIZED);
	mm_player_t* player = (mm_player_t*) hplayer;
	MMPLAYER_FENTER();

	if (type == MM_PLAYER_TRACK_TYPE_TEXT && player->subtitle_language_list)
	{
		int language_code_size = 3;/*Size of ISO-639-1*/
		MMPlayerLangStruct *language_list = NULL;

		*code = (char*)malloc(language_code_size * sizeof(char));
		if (*code == NULL)
		{
			ret = MM_ERROR_PLAYER_INTERNAL;
			goto EXIT;
		}
		memset(*code, 0, language_code_size * sizeof(char));

		language_list = g_list_nth_data (player->subtitle_language_list, index);
		if (language_list == NULL)
		{
			debug_log ("%d is not a proper index \n", index);
			goto EXIT;
		}
		strncpy(*code, language_list->language_code, language_code_size);
	}
	else
	{
		if (player->selector[type].total_track_num <= 0)
		{
			ret = MM_ERROR_PLAYER_NO_OP;
			debug_log("language list is not available. [type:%d] \n", type);
			goto EXIT;
		}

		if(index < 0 || index >= player->selector[type].total_track_num)
		{
			ret = MM_ERROR_INVALID_ARGUMENT;
			debug_log("Not a proper index : %d \n", index);
			goto EXIT;
		}

		ret = __mmplayer_track_get_language(player, type, index, code);
	}

EXIT:
	MMPLAYER_FLEAVE();
	return ret;
}

void _mmplayer_track_initialize(mm_player_t* player)
{
	MMPlayerTrackType type = MM_PLAYER_TRACK_TYPE_AUDIO;

	MMPLAYER_FENTER();

	for (;type<MM_PLAYER_TRACK_TYPE_MAX ; type++)
	{
		/* active_pad_index is initialized when player is created or destroyed.
		   and the value can be set by calling _mmplayer_change_track_language()
		   before pipeline is created.*/
		player->selector[type].total_track_num = 0;
		player->selector[type].channels = g_ptr_array_new();
	}
}

void _mmplayer_track_destroy(mm_player_t* player)
{
	MMPlayerTrackType type = MM_PLAYER_TRACK_TYPE_AUDIO;
	MMHandleType attrs = 0;
	MMPLAYER_FENTER();

	attrs = MMPLAYER_GET_ATTRS(player);
	if (attrs)
	{
		mm_attrs_set_int_by_name(attrs, "content_audio_track_num", 0);
		mm_attrs_set_int_by_name(attrs, "content_video_track_num", 0);
		mm_attrs_set_int_by_name(attrs, "content_text_track_num", 0);

		if (mmf_attrs_commit (attrs))
			debug_error("failed to commit.\n");
	}

	for (;type<MM_PLAYER_TRACK_TYPE_MAX ; type++)
	{
		player->selector[type].active_pad_index = 0;
		player->selector[type].total_track_num = 0;

		if (player->selector[type].channels)
			g_ptr_array_free (player->selector[type].channels, TRUE);
		player->selector[type].channels = NULL;
	}
}

void _mmplayer_track_update_info(mm_player_t* player, MMPlayerTrackType type, GstPad *sinkpad)
{
	MMPLAYER_FENTER();

	player->selector[type].total_track_num++;
	g_ptr_array_add (player->selector[type].channels, sinkpad);

	debug_log ("type:%d, total track:%d\n", type, player->selector[type].total_track_num);
}

static int __mmplayer_track_get_language(mm_player_t* player, MMPlayerTrackType type, gint stream_index, gchar **code)
{
	int ret = MM_ERROR_NONE;

	GstTagList *tag_list = NULL;
	gchar* tag = NULL;
	GstPad *sinkpad = NULL;
	gint language_code_size = 3; /*Size of ISO-639-1*/

	MMPLAYER_FENTER();

	*code = (char *)malloc(language_code_size*sizeof(char));
	if(*code == NULL)
	{
		ret = MM_ERROR_PLAYER_INTERNAL;
		goto EXIT;
	}
	memset(*code,0,language_code_size*sizeof(char));

	debug_log ("total track num : %d , req idx : %d\n", player->selector[type].total_track_num, stream_index);

	if (stream_index < player->selector[type].total_track_num)
	{
		sinkpad = g_ptr_array_index (player->selector[type].channels, stream_index);
	}
	else
	{
		ret = MM_ERROR_INVALID_ARGUMENT;
		goto EXIT;
	}

	g_object_get (sinkpad, "tags", &tag_list, NULL);
	//secure_debug_log ("[%s]\n", gst_tag_list_to_string(tag_list));

	gst_tag_list_get_string (tag_list, GST_TAG_LANGUAGE_CODE, &tag);

	if(!tag)
	{
		debug_log("there is no lang info - und\n");
		strncpy(*code, "und", language_code_size);
	}
	else
	{
		debug_log("language information[%d] code: %s, len: %d \n", type, tag, strlen(tag));
		strncpy(*code, tag, /*strlen(tag)*/language_code_size);
		g_free (tag);
	}

	if (tag_list)
		gst_tag_list_free (tag_list);

EXIT:
	MMPLAYER_FLEAVE();
	return ret;
}
#ifdef _MULTI_TRACK
int _mmplayer_track_foreach_selected_subtitle_language(MMHandleType hplayer,_mmplayer_track_selected_subtitle_language_cb foreach_cb, void *user_data)
{
	int ret = MM_ERROR_NONE;
	mm_player_t* player = (mm_player_t*) hplayer;
	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	MMPLAYER_FENTER();

	int index = -1;

	if (player->subtitle_language_list)
	{
		int total_track_count = 0;
		MMPlayerLangStruct *temp = NULL;
		MMHandleType attrs = 0;

		attrs = MMPLAYER_GET_ATTRS(player);
		if (!attrs)
		{
			debug_error("cannot get content attribute");
			ret = MM_ERROR_PLAYER_INTERNAL;
			goto EXIT;
		}
		mm_attrs_get_int_by_name(attrs, "content_text_track_num", &total_track_count);

		if(!total_track_count)
		{
			debug_warning("There are no subtitle track selected.");
			ret = MM_ERROR_PLAYER_NO_OP;
			goto EXIT;
		}

		while (total_track_count)
		{
			temp = g_list_nth_data (player->subtitle_language_list, total_track_count - 1);
			if (temp)
			{
				debug_log("find the list");
				if (temp->active)
				{
					index = total_track_count - 1;
					debug_log("active subtitle track index is %d", index);
					if (!foreach_cb(index, user_data))
					{
						ret = MM_ERROR_PLAYER_INTERNAL;
						goto CALLBACK_ERROR;
					}
				}
			}
			total_track_count--;
		}

		debug_log("we will return -1 for notifying the end to user");

		/* After returning all selected indexs, we will return -1 for notifying the end to user */
		if (!foreach_cb(-1, user_data))
		{
			ret = MM_ERROR_PLAYER_INTERNAL;
			goto CALLBACK_ERROR;
		}
	}

CALLBACK_ERROR:
	debug_error("foreach callback returned error");

EXIT:
	MMPLAYER_FLEAVE();
	return ret;


}
#endif
