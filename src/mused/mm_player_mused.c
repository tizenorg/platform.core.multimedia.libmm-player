/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: YoungHwan An <younghwan_.an@samsung.com>
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

/*===========================================================================================
|																							|
|  INCLUDE FILES																			|
|  																							|
========================================================================================== */
#include <glib.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/videooverlay.h>
#ifdef HAVE_WAYLAND
#include <gst/wayland/wayland.h>
#endif
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>

#include <mm_error.h>
#include <mm_attrs.h>
#include <mm_attrs_private.h>
#include <mm_debug.h>

#include "mm_player_priv.h"
#include "mm_player_ini.h"
#include "mm_player_attrs.h"
#include "mm_player_utils.h"
#include <sched.h>

/*===========================================================================================
|																							|
|  LOCAL DEFINITIONS AND DECLARATIONS FOR MODULE											|
|  																							|
========================================================================================== */

/*---------------------------------------------------------------------------
|    GLOBAL CONSTANT DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    IMPORTED VARIABLE DECLARATIONS:										|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    IMPORTED FUNCTION DECLARATIONS:										|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL #defines:														|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL CONSTANT DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL DATA TYPE DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:												|
---------------------------------------------------------------------------*/
static gboolean _mmplayer_mused_init_gst(mm_player_t* player);
static int _mmplayer_mused_realize(mm_player_t *player, char *string_caps);
static int _mmplayer_mused_unrealize(mm_player_t *player);
static MMHandleType _mmplayer_mused_construct_attribute(mm_player_t *player);
static int _mmplayer_mused_update_video_param(mm_player_t *player);
static int _mmplayer_get_raw_video_caps(mm_player_t *player, char **caps);
static int __mmplayer_mused_gst_destroy_pipeline(mm_player_t *player);
static int _mmplayer_mused_gst_pause(mm_player_t *player);
static int _mmplayer_set_shm_stream_path(MMHandleType hplayer, const char *path);
/*===========================================================================================
|																							|
|  FUNCTION DEFINITIONS																		|
|  																							|
========================================================================================== */

int mm_player_mused_create(MMHandleType *player)
{
	int result = MM_ERROR_NONE;
	mm_player_t* new_player = NULL;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	/* alloc player structure */
	new_player = g_malloc0(sizeof(mm_player_t));
	if ( ! new_player )
	{
		debug_error("Cannot allocate memory for player\n");
		goto ERROR;
	}

	/* create player lock */
	g_mutex_init(&new_player->cmd_lock);

	/* load ini files */
	result = mm_player_ini_load(&new_player->ini);
	if(result != MM_ERROR_NONE)
	{
		debug_error("can't load ini");
		goto ERROR;
	}

	/* initialize player state */
	MMPLAYER_CURRENT_STATE(new_player) = MM_PLAYER_STATE_NONE;
	MMPLAYER_PREV_STATE(new_player) = MM_PLAYER_STATE_NONE;
	MMPLAYER_PENDING_STATE(new_player) = MM_PLAYER_STATE_NONE;
	MMPLAYER_TARGET_STATE(new_player) = MM_PLAYER_STATE_NONE;

	/* check current state */
	if (__mmplayer_check_state ( new_player, MMPLAYER_COMMAND_CREATE )
			!= MM_ERROR_NONE)
		goto ERROR;

	/* construct attributes */
	new_player->attrs = _mmplayer_mused_construct_attribute(new_player);

	if ( !new_player->attrs )
	{
		debug_error("Failed to construct attributes\n");
		goto ERROR;
	}

	/* initialize gstreamer with configured parameter */
	if ( ! _mmplayer_mused_init_gst(new_player) )
	{
		debug_error("Initializing gstreamer failed\n");
		goto ERROR;
	}
	MMPLAYER_SET_STATE ( new_player, MM_PLAYER_STATE_NULL );
	MMPLAYER_STATE_CHANGE_TIMEOUT(new_player) = new_player->ini.localplayback_state_change_timeout;

	*player = (MMHandleType)new_player;
	return result;
ERROR:
	if( new_player ) {
		g_mutex_clear(&new_player->cmd_lock);
		_mmplayer_deconstruct_attribute(new_player);
		g_free( new_player );
	}

	*player = NULL;
	return MM_ERROR_PLAYER_NO_FREE_SPACE;
}

static gboolean
_mmplayer_mused_init_gst(mm_player_t *player)
{
	static gboolean initialized = FALSE;
	static const int max_argc = 50;
  	gint* argc = NULL;
	gchar** argv = NULL;
	gchar** argv2 = NULL;
	GError *err = NULL;
	int i = 0;
	int arg_count = 0;

	if ( initialized )
	{
		debug_log("gstreamer already initialized.\n");
		return TRUE;
	}

	/* alloc */
	argc = malloc( sizeof(int) );
	argv = malloc( sizeof(gchar*) * max_argc );
	argv2 = malloc( sizeof(gchar*) * max_argc );

	if ( !argc || !argv || !argv2)
		goto GST_INIT_EXIT;

	memset( argv, 0, sizeof(gchar*) * max_argc );
	memset( argv2, 0, sizeof(gchar*) * max_argc );

	/* add initial */
	*argc = 1;
	argv[0] = g_strdup( "mmplayer_mused" );

	/* we would not do fork for scanning plugins */
	argv[*argc] = g_strdup("--gst-disable-registry-fork");
	(*argc)++;

	arg_count = *argc;

	for ( i = 0; i < arg_count; i++ )
		argv2[i] = argv[i];

	/* initializing gstreamer */
	if ( ! gst_init_check (argc, &argv, &err))
	{
		debug_error("Could not initialize GStreamer: %s\n", err ? err->message : "unknown error occurred");
		if (err)
		{
			g_error_free (err);
		}
	}

	initialized = TRUE;
GST_INIT_EXIT:
	/* release */
	for ( i = 0; i < arg_count; i++ )
	{
		MMPLAYER_FREEIF( argv2[i] );
	}

	MMPLAYER_FREEIF( argv );
	MMPLAYER_FREEIF( argv2 );
	MMPLAYER_FREEIF( argc );

	return initialized;
}

int mm_player_mused_destroy(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	/* destroy can called at anytime */
	MMPLAYER_CHECK_STATE_RETURN_IF_FAIL ( player, MMPLAYER_COMMAND_DESTROY );

	__mmplayer_mused_gst_destroy_pipeline(player);

	/* release attributes */
	if( !_mmplayer_deconstruct_attribute( player ) ) {
		debug_error("failed to deconstruct attribute");
		result = MM_ERROR_PLAYER_INTERNAL;
	}

	MMPLAYER_CMD_UNLOCK( player );

	g_mutex_clear(&((mm_player_t*)player)->cmd_lock);
	g_free( player );
	return result;
}


int mm_player_mused_realize(MMHandleType player, char *caps)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_mused_realize((mm_player_t *)player, caps);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

static int _mmplayer_mused_realize(mm_player_t *player, char *string_caps)
{
	int result = MM_ERROR_NONE;
	GstElement *src;
	GstElement *sink;
	GstBus *bus;
	GstCaps *caps;
	MMPlayerGstElement *mainbin = NULL;
	MMHandleType attrs = MMPLAYER_GET_ATTRS(player);
	int width = 0;
	int height = 0;
	gboolean link;
	char *stream_path = NULL;
	int attr_ret;
	int surface_type = 0;
	gchar *videosink_element = NULL;
	gchar *videosrc_element = NULL;

	/* check current state */
	MMPLAYER_CHECK_STATE_RETURN_IF_FAIL( player, MMPLAYER_COMMAND_REALIZE );

	/* create pipeline handles */
	if ( player->pipeline )
	{
		debug_warning("pipeline should be released before create new one\n");
		return result;
	}
	/* alloc handles */
	player->pipeline = (MMPlayerGstPipelineInfo*) g_malloc0( sizeof(MMPlayerGstPipelineInfo) );
	if (player->pipeline == NULL)
		return MM_ERROR_PLAYER_NO_FREE_SPACE;

	/* create mainbin */
	mainbin = (MMPlayerGstElement*) g_malloc0( sizeof(MMPlayerGstElement) * MMPLAYER_M_NUM );
	if (!mainbin) {
		result = MM_ERROR_OUT_OF_MEMORY;
		goto REALIZE_ERROR;
	}

	/* create pipeline */
	mainbin[MMPLAYER_M_PIPE].id = MMPLAYER_M_PIPE;
	mainbin[MMPLAYER_M_PIPE].gst = gst_pipeline_new("playerClient");
	debug_log("gst new %p", mainbin[MMPLAYER_M_PIPE].gst);
	if ( ! mainbin[MMPLAYER_M_PIPE].gst )
	{
		debug_error("failed to create pipeline\n");
		result = MM_ERROR_PLAYER_INTERNAL;
		goto REALIZE_ERROR;
	}

	if (strlen(player->ini.videosrc_element_remote) > 0)
		videosrc_element = player->ini.videosrc_element_remote;
	else {
		debug_error("fail to find source element");
		result = MM_ERROR_PLAYER_INTERNAL;
		goto REALIZE_ERROR;
	}

	/* create source */
	src = gst_element_factory_make(videosrc_element, videosrc_element);
	if ( !src ) {
		debug_error("faile to create %s", videosrc_element);
		result = MM_ERROR_PLAYER_INTERNAL;
		goto REALIZE_ERROR;
	}
	if(strcmp(videosrc_element, "shmsrc") == 0) {
		attr_ret = mm_attrs_get_string_by_name ( attrs, "shm_stream_path", &stream_path );
		if(attr_ret == MM_ERROR_NONE && stream_path) {
			LOGD("stream path : %s", stream_path);
			g_object_set(G_OBJECT(src),
					"socket-path", stream_path,
					"is-live", TRUE,
					NULL);
		} else {
			result = MM_ERROR_PLAYER_INTERNAL;
			goto REALIZE_ERROR;
		}
	}

	mainbin[MMPLAYER_M_SRC].id = MMPLAYER_M_SRC;
	mainbin[MMPLAYER_M_SRC].gst = src;

	/* create sink */
	mm_attrs_get_int_by_name (player->attrs, "display_surface_type", &surface_type);
	switch(surface_type)
	{
		case MM_DISPLAY_SURFACE_X:
			if (strlen(player->ini.videosink_element_x) > 0)
				videosink_element = player->ini.videosink_element_x;
			else {
				result = MM_ERROR_PLAYER_NOT_INITIALIZED;
				goto REALIZE_ERROR;
			}
			break;
		default:
			debug_error("Not support surface type %d", surface_type);
			result = MM_ERROR_INVALID_ARGUMENT;
			goto REALIZE_ERROR;
	}
	sink = gst_element_factory_make(videosink_element, videosink_element);
	if ( !src ) {
		debug_error("faile to create %s", videosink_element);
		result = MM_ERROR_PLAYER_INTERNAL;
		goto REALIZE_ERROR;
	}
	mainbin[MMPLAYER_M_V_SINK].id = MMPLAYER_M_V_SINK;
	mainbin[MMPLAYER_M_V_SINK].gst = sink;

	/* now we have completed mainbin. take it */
	player->pipeline->mainbin = mainbin;

	result = _mmplayer_mused_update_video_param(player);
	if(result != MM_ERROR_NONE)
		goto REALIZE_ERROR;

	/* add and link */
	gst_bin_add_many(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst),
			mainbin[MMPLAYER_M_SRC].gst,
			mainbin[MMPLAYER_M_V_SINK].gst,
			NULL);

	mm_attrs_get_int_by_name(attrs, "wl_window_render_width", &width);
	mm_attrs_get_int_by_name(attrs, "wl_window_render_height", &height);

	caps = gst_caps_from_string(string_caps);

	link = gst_element_link_filtered(mainbin[MMPLAYER_M_SRC].gst, mainbin[MMPLAYER_M_V_SINK].gst, caps);
	gst_caps_unref(caps);
	if(!link) {
		debug_error("element link error");
		result = MM_ERROR_PLAYER_INTERNAL;
		goto REALIZE_ERROR;
	}

	/* connect bus callback */
	bus = gst_pipeline_get_bus(GST_PIPELINE(mainbin[MMPLAYER_M_PIPE].gst));
	if ( !bus ) {
		debug_error ("cannot get bus from pipeline.\n");
		result = MM_ERROR_PLAYER_INTERNAL;
		goto REALIZE_ERROR;
	}

	player->bus_watcher = gst_bus_add_watch(bus, (GstBusFunc)__mmplayer_gst_callback, player);

	player->context.thread_default = g_main_context_get_thread_default();

	if (NULL == player->context.thread_default)
	{
		player->context.thread_default = g_main_context_default();
		debug_log("thread-default context is the global default context");
	}
	debug_log("bus watcher thread context = %p, watcher : %d", player->context.thread_default, player->bus_watcher);

	/* set sync handler to get tag synchronously */
	gst_bus_set_sync_handler(bus, __mmplayer_bus_sync_callback, player, NULL);

	gst_object_unref(GST_OBJECT(bus));

	/* warm up */
	if ( GST_STATE_CHANGE_FAILURE ==
			gst_element_set_state(mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_READY ) ) {
		debug_error("failed to set state(READY) to pipeline");
		result = MM_ERROR_PLAYER_INVALID_STATE;
		goto REALIZE_ERROR;
	}
	/* run */
	if (GST_STATE_CHANGE_FAILURE ==
			gst_element_set_state (mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_PAUSED)) {
		debug_error("failed to set state(PAUSE) to pipeline");
		result = MM_ERROR_PLAYER_INVALID_STATE;
		goto REALIZE_ERROR;
	}
	if (GST_STATE_CHANGE_FAILURE ==
			gst_element_set_state (mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_PLAYING)) {
		debug_error("failed to set state(PLAYING) to pipeline");
		result = MM_ERROR_PLAYER_INVALID_STATE;
		goto REALIZE_ERROR;
	}

	MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_READY );
	return result;

REALIZE_ERROR:
	__mmplayer_mused_gst_destroy_pipeline(player);
	return result;
}

static int _mmplayer_mused_update_video_param(mm_player_t *player)
{
	MMHandleType attrs = 0;
	int surface_type = 0;
	MMPlayerGstElement* mainbin = player->pipeline->mainbin;

	if( !mainbin ) {
		debug_error("mainbin was not created");
		return MM_ERROR_PLAYER_INTERNAL;
	}
	attrs = MMPLAYER_GET_ATTRS(player);
	if ( !attrs ) {
		debug_error("cannot get content attribute");
		return MM_ERROR_PLAYER_INTERNAL;
	}
	/* update display surface */
	mm_attrs_get_int_by_name(attrs, "display_surface_type", &surface_type);
	debug_log("check display surface type attribute: %d", surface_type);

	/* configuring display */
	switch ( surface_type )
	{
		case MM_DISPLAY_SURFACE_X:
		{
			/* ximagesink or xvimagesink */
			void *surface = NULL;

#ifdef HAVE_WAYLAND
			/*set wl_display*/
			void* wl_display = NULL;
			GstContext *context = NULL;
			int wl_window_x = 0;
			int wl_window_y = 0;
			int wl_window_width = 0;
			int wl_window_height = 0;

			mm_attrs_get_data_by_name(attrs, "wl_display", &wl_display);
			if (wl_display)
				context = gst_wayland_display_handle_context_new(wl_display);
			if (context)
				gst_element_set_context(GST_ELEMENT(mainbin[MMPLAYER_M_V_SINK].gst), context);

			/*It should be set after setting window*/
			mm_attrs_get_int_by_name(attrs, "wl_window_render_x", &wl_window_x);
			mm_attrs_get_int_by_name(attrs, "wl_window_render_y", &wl_window_y);
			mm_attrs_get_int_by_name(attrs, "wl_window_render_width", &wl_window_width);
			mm_attrs_get_int_by_name(attrs, "wl_window_render_height", &wl_window_height);
#endif
			mm_attrs_get_data_by_name(attrs, "display_overlay", &surface);
			if ( surface ) {
#ifdef HAVE_WAYLAND
				guintptr wl_surface = (guintptr)surface;
				debug_log("set video param : surface %p", wl_surface);
				gst_video_overlay_set_window_handle(
						GST_VIDEO_OVERLAY( mainbin[MMPLAYER_M_V_SINK].gst ),
						wl_surface );
				/* After setting window handle, set render	rectangle */
				gst_video_overlay_set_render_rectangle(
					 GST_VIDEO_OVERLAY( mainbin[MMPLAYER_M_V_SINK].gst ),
					 wl_window_x,wl_window_y,wl_window_width,wl_window_height);
#else
				int xwin_id = 0;
				xwin_id = *(int*)surface;
				debug_log("set video param : xid %d", xwin_id);
				if (xwin_id)
				{
					gst_video_overlay_set_window_handle(
							GST_VIDEO_OVERLAY( player->pipeline->videobin[MMPLAYER_V_SINK].gst ),
							xwin_id );
				}
#endif
			}
			else
				debug_warning("still we don't have surface on player attribute. create it's own surface.");
		}
		break;
		default:
			debug_log("Noting to update");
		break;
	}

	return MM_ERROR_NONE;
}

int mm_player_mused_unrealize(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_mused_unrealize(player);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

static int _mmplayer_mused_unrealize(mm_player_t *player)
{
	int ret = MM_ERROR_NONE;

	/* check current state */
	MMPLAYER_CHECK_STATE_RETURN_IF_FAIL( player, MMPLAYER_COMMAND_UNREALIZE );

	ret = __mmplayer_mused_gst_destroy_pipeline(player);

	MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_NULL );

	return ret;
}

static int __mmplayer_mused_gst_destroy_pipeline(mm_player_t *player)
{
	int ret = MM_ERROR_NONE;
	/* cleanup gst stuffs */
	if ( player->pipeline ) {
		MMPlayerGstElement* mainbin = player->pipeline->mainbin;

		/* disconnecting bus watch */
		if ( player->bus_watcher )
			__mmplayer_remove_g_source_from_context(
					player->context.thread_default, player->bus_watcher);
		player->bus_watcher = 0;

		if ( mainbin ) {
			gint timeout;
			GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (mainbin[MMPLAYER_M_PIPE].gst));
			gst_bus_set_sync_handler (bus, NULL, NULL, NULL);
			gst_object_unref(bus);

			timeout = MMPLAYER_STATE_CHANGE_TIMEOUT(player);
			ret = __mmplayer_gst_set_state ( player, mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_NULL, FALSE, timeout );
			if ( ret != MM_ERROR_NONE ) {
				debug_error("fail to change state to NULL\n");
				return MM_ERROR_PLAYER_INTERNAL;
			}
			debug_log("succeeded in chaning state to NULL\n");

			debug_log("gst unref %p", mainbin[MMPLAYER_M_PIPE].gst);
			gst_object_unref(GST_OBJECT(mainbin[MMPLAYER_M_PIPE].gst));

			debug_log("free mainbin");
			MMPLAYER_FREEIF( player->pipeline->mainbin );
		}
		debug_log("free pipelin");
		MMPLAYER_FREEIF( player->pipeline );
	}
	debug_log("finished destroy pipeline");

	return ret;
}

static int _mmplayer_mused_gst_pause(mm_player_t *player)
{
	/* check current state */
	MMPLAYER_CHECK_STATE_RETURN_IF_FAIL( player, MMPLAYER_COMMAND_UNREALIZE );

	int ret = MM_ERROR_NONE;
	if ( player->pipeline ) {
		MMPlayerGstElement* mainbin = player->pipeline->mainbin;

		if ( mainbin ) {
			gint timeout;

			timeout = MMPLAYER_STATE_CHANGE_TIMEOUT(player);
			ret = __mmplayer_gst_set_state ( player, mainbin[MMPLAYER_M_PIPE].gst, GST_STATE_PAUSED, FALSE, timeout );
			if ( ret != MM_ERROR_NONE ) {
				debug_error("fail to change state to PAUSED");
				return MM_ERROR_PLAYER_INTERNAL;
			}
			debug_log("succeeded in chaning state to PAUSED");
			MMPLAYER_SET_STATE ( player, MM_PLAYER_STATE_PAUSED );
		}
	}

	return MM_ERROR_NONE;
}

int mm_player_mused_pre_unrealize(MMHandleType player)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_mused_gst_pause(player);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

static MMHandleType _mmplayer_mused_construct_attribute(mm_player_t *player)
{
	int idx = 0;
	MMHandleType attrs = 0;
	int num_of_attrs = 0;
	mmf_attrs_construct_info_t *base = NULL;

	MMPlayerAttrsSpec player_attrs[] =
	{
		{
			"display_surface_type",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) MM_DISPLAY_SURFACE_NULL,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			MM_DISPLAY_SURFACE_X,
			MM_DISPLAY_SURFACE_X_EXT
		},
		{
			"display_overlay",
			MM_ATTRS_TYPE_DATA,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
#ifdef HAVE_WAYLAND
		{
			"wl_display",
			MM_ATTRS_TYPE_DATA,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"wl_window_render_x",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"wl_window_render_y",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"wl_window_render_width",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
		{
			"wl_window_render_height",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) 0,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			MMPLAYER_MAX_INT
		},
#endif
		{
			"shm_stream_path",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		}
	};
	num_of_attrs = ARRAY_SIZE(player_attrs);
	base = (mmf_attrs_construct_info_t* )malloc(num_of_attrs * sizeof(mmf_attrs_construct_info_t));
	if ( !base )
	{
		debug_error("failed to alloc attrs constructor");
		return 0;
	}

	/* initialize values of attributes */
	for ( idx = 0; idx < num_of_attrs; idx++ )
	{
		base[idx].name = player_attrs[idx].name;
		base[idx].value_type = player_attrs[idx].value_type;
		base[idx].flags = player_attrs[idx].flags;
		base[idx].default_value = player_attrs[idx].default_value;
	}

	attrs = mmf_attrs_new_from_data(
					"mmplayer_attrs",
					base,
					num_of_attrs,
					NULL,
					NULL);

	/* clean */
	MMPLAYER_FREEIF(base);

	if ( !attrs )
	{
		debug_error("failed to create player attrs");
		return 0;
	}

	/* set validity type and range */
	for ( idx = 0; idx < num_of_attrs; idx++ )
	{
		switch ( player_attrs[idx].valid_type)
		{
			case MM_ATTRS_VALID_TYPE_INT_RANGE:
			{
				mmf_attrs_set_valid_type (attrs, idx, MM_ATTRS_VALID_TYPE_INT_RANGE);
				mmf_attrs_set_valid_range (attrs, idx,
						player_attrs[idx].value_min,
						player_attrs[idx].value_max,
						(int)(intptr_t)(player_attrs[idx].default_value));
			}
			break;

			case MM_ATTRS_VALID_TYPE_INT_ARRAY:
			case MM_ATTRS_VALID_TYPE_DOUBLE_ARRAY:
			case MM_ATTRS_VALID_TYPE_DOUBLE_RANGE:
			default:
			break;
		}
	}

	/* commit */
	mmf_attrs_commit(attrs);

	return attrs;
}

/*
 * Server uses functions
 */
int mm_player_get_raw_video_caps(MMHandleType player, char **caps)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(caps, MM_ERROR_PLAYER_NOT_INITIALIZED);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_get_raw_video_caps(player, caps);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

static int _mmplayer_get_raw_video_caps(mm_player_t *player, char **caps)
{
	GstCaps *v_caps = NULL;
	GstPad *pad = NULL;
	GstElement *gst;
	gint stype = 0;

	if(!player->videosink_linked) {
		debug_log("No video sink");
		return MM_ERROR_NONE;
	}
	mm_attrs_get_int_by_name (player->attrs, "display_surface_type", &stype);

	if (stype == MM_DISPLAY_SURFACE_NULL) {
		debug_log("Display type is NULL");
		if(!player->video_fakesink) {
			debug_error("No fakesink");
			return MM_ERROR_PLAYER_INVALID_STATE;
		}
		gst = player->video_fakesink;
	}
	else {
		if ( !player->pipeline || !player->pipeline->videobin ||
				!player->pipeline->videobin[MMPLAYER_V_SINK].gst ) {
			debug_error("No video pipeline");
			return MM_ERROR_PLAYER_INVALID_STATE;
		}
		gst = player->pipeline->videobin[MMPLAYER_V_SINK].gst;
	}
	pad = gst_element_get_static_pad(gst, "sink");
	if(!pad) {
		debug_error("static pad is NULL");
		return MM_ERROR_PLAYER_INVALID_STATE;
	}
	v_caps = gst_pad_get_current_caps(pad);
	gst_object_unref( pad );

	if(!v_caps) {
		debug_error("fail to get caps");
		return MM_ERROR_PLAYER_INVALID_STATE;
	}

	*caps = gst_caps_to_string(v_caps);

	gst_caps_unref(v_caps);

	return MM_ERROR_NONE;
}

/*
 * Server and client both use functions
 */
int mm_player_set_shm_stream_path(MMHandleType player, const char *path)
{
	int result = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(path, MM_ERROR_INVALID_ARGUMENT);

	MMPLAYER_CMD_LOCK( player );

	result = _mmplayer_set_shm_stream_path(player, path);

	MMPLAYER_CMD_UNLOCK( player );

	return result;

}

static int _mmplayer_set_shm_stream_path(MMHandleType hplayer, const char *path)
{
	mm_player_t* player = (mm_player_t*) hplayer;
	int result;

	MMPLAYER_FENTER();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail(path, MM_ERROR_INVALID_ARGUMENT);

	result = mm_attrs_set_string_by_name(player->attrs, "shm_stream_path", path)

	MMPLAYER_FLEAVE();
	return result;
}
