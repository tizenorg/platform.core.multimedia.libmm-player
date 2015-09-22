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
|																							|
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
/* setting player state */
#define MMPLAYER_MUSED_SET_STATE( x_player, x_state ) \
debug_log("update state machine to %d\n", x_state); \
__mmplayer_mused_set_state(x_player, x_state);

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
static int __mmplayer_mused_gst_destroy_pipeline(mm_player_t *player);
static int _mmplayer_mused_gst_pause(mm_player_t *player);
static gboolean __mmplayer_mused_gst_callback(GstBus *bus, GstMessage *msg, gpointer data);
static GstBusSyncReply __mmplayer_mused_bus_sync_callback (GstBus * bus, GstMessage * message, gpointer data);
static int __mmplayer_mused_set_state(mm_player_t* player, int state);
/*===========================================================================================
|																							|
|  FUNCTION DEFINITIONS																		|
|																							|
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
	MMPLAYER_MUSED_SET_STATE ( new_player, MM_PLAYER_STATE_NULL );
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
	argv[0] = g_strdup( "mused_client" );

	/* add gst_param */
	for ( i = 0; i < 5; i++ ) /* FIXIT : num of param is now fixed to 5. make it dynamic */
	{
		if ( strlen( player->ini.gst_param[i] ) > 0 )
		{
			argv[*argc] = g_strdup( player->ini.gst_param[i] );
			(*argc)++;
		}
	}

	/* we would not do fork for scanning plugins */
	argv[*argc] = g_strdup("--gst-disable-registry-fork");
	(*argc)++;

	/* check disable registry scan */
	if ( player->ini.skip_rescan )
	{
		argv[*argc] = g_strdup("--gst-disable-registry-update");
		(*argc)++;
	}

	/* check disable segtrap */
	if ( player->ini.disable_segtrap )
	{
		argv[*argc] = g_strdup("--gst-disable-segtrap");
		(*argc)++;
	}

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
	GstElement *conv;
	GstBus *bus;
	GstCaps *caps;
	MMPlayerGstElement *mainbin = NULL;
	MMHandleType attrs = MMPLAYER_GET_ATTRS(player);
	gboolean link;
	char *stream_path = NULL;
	int attr_ret;
	int surface_type = 0;
	gchar *videosink_element = NULL;
	gchar *videosrc_element = NULL;
	gboolean use_tbm = FALSE;
	gchar* video_csc = "videoconvert"; // default colorspace converter

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

	if(string_caps && (strstr(string_caps, "ST12") || strstr(string_caps, "SN12"))) {
		debug_log("using TBM");
		use_tbm = TRUE;
	}

	if(strcmp(videosrc_element, "shmsrc") == 0) {
		attr_ret = mm_attrs_get_string_by_name ( attrs, "shm_stream_path", &stream_path );
		if(attr_ret == MM_ERROR_NONE && stream_path) {
			LOGD("stream path : %s", stream_path);
			g_object_set(G_OBJECT(src),
					"socket-path", stream_path,
					"is-live", TRUE,
					"use-tbm", use_tbm,
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
		case MM_DISPLAY_SURFACE_EVAS:
			if (strlen(player->ini.videosink_element_evas) > 0)
				videosink_element = player->ini.videosink_element_evas;
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
	if ( !sink ) {
		debug_error("faile to create %s", videosink_element);
		result = MM_ERROR_PLAYER_INTERNAL;
		goto REALIZE_ERROR;
	}
	mainbin[MMPLAYER_M_V_SINK].id = MMPLAYER_M_V_SINK;
	mainbin[MMPLAYER_M_V_SINK].gst = sink;

	/* now we have completed mainbin. take it */
	player->pipeline->mainbin = mainbin;

	result = _mmplayer_update_video_param(player);
	if(result != MM_ERROR_NONE)
		goto REALIZE_ERROR;

	caps = gst_caps_from_string(string_caps);

	/* add and link */
	if(surface_type == MM_DISPLAY_SURFACE_EVAS) {
		conv = gst_element_factory_make(video_csc, video_csc);
		if ( !conv ) {
			debug_error("faile to create %s", video_csc);
			result = MM_ERROR_PLAYER_INTERNAL;
			goto REALIZE_ERROR;
		}
		mainbin[MMPLAYER_M_V_CONV].id = MMPLAYER_M_V_CONV;
		mainbin[MMPLAYER_M_V_CONV].gst = conv;
		gst_bin_add_many(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst),
				mainbin[MMPLAYER_M_SRC].gst,
				mainbin[MMPLAYER_M_V_CONV].gst,
				mainbin[MMPLAYER_M_V_SINK].gst,
				NULL);

		link = gst_element_link_filtered(mainbin[MMPLAYER_M_SRC].gst,
				mainbin[MMPLAYER_M_V_CONV].gst,
				caps);
		if(link) {
			link = gst_element_link(mainbin[MMPLAYER_M_V_CONV].gst,
					mainbin[MMPLAYER_M_V_SINK].gst);
		}
		else
			debug_error("gst_element_link_filterd error");


	} else {
		gst_bin_add_many(GST_BIN(mainbin[MMPLAYER_M_PIPE].gst),
				mainbin[MMPLAYER_M_SRC].gst,
				mainbin[MMPLAYER_M_V_SINK].gst,
				NULL);

		link = gst_element_link_filtered(mainbin[MMPLAYER_M_SRC].gst,
				mainbin[MMPLAYER_M_V_SINK].gst,
				caps);
	}

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

	player->bus_watcher = gst_bus_add_watch(bus, (GstBusFunc)__mmplayer_mused_gst_callback, player);

	player->context.thread_default = g_main_context_get_thread_default();

	if (NULL == player->context.thread_default)
	{
		player->context.thread_default = g_main_context_default();
		debug_log("thread-default context is the global default context");
	}
	debug_log("bus watcher thread context = %p, watcher : %d", player->context.thread_default, player->bus_watcher);

	/* set sync handler to get tag synchronously */
	gst_bus_set_sync_handler(bus, __mmplayer_mused_bus_sync_callback, player, NULL);

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

	MMPLAYER_MUSED_SET_STATE ( player, MM_PLAYER_STATE_READY );
	return result;

REALIZE_ERROR:
	__mmplayer_mused_gst_destroy_pipeline(player);
	return result;
}

static gboolean
__mmplayer_get_property_value_for_rotation(mm_player_t* player, int rotation_angle, int *value)
{
	int pro_value = 0; // in the case of expection, default will be returned.
	int dest_angle = rotation_angle;
	int rotation_type = ROTATION_USING_FLIP;
	int surface_type = 0;

	return_val_if_fail(player, FALSE);
	return_val_if_fail(value, FALSE);
	return_val_if_fail(rotation_angle >= 0, FALSE);

	if (rotation_angle >= 360)
	{
		dest_angle = rotation_angle - 360;
	}

	/* chech if supported or not */
	if ( dest_angle % 90 )
	{
		debug_log("not supported rotation angle = %d", rotation_angle);
		return FALSE;
	}

	mm_attrs_get_int_by_name(player->attrs, "display_surface_type", &surface_type);
	debug_log("check display surface type attribute: %d", surface_type);

	if ((surface_type == MM_DISPLAY_SURFACE_X) ||
			(surface_type == MM_DISPLAY_SURFACE_EVAS &&
			 !strcmp(player->ini.videosink_element_evas, "evaspixmapsink")))
		rotation_type = ROTATION_USING_SINK;

	debug_log("using %d type for rotation", rotation_type);

	/* get property value for setting */
	switch(rotation_type) {
	case ROTATION_USING_SINK: // waylandsink, xvimagesink
		switch (dest_angle) {
			case 0:
				break;
			case 90:
				pro_value = 3; // clockwise 90
				break;
			case 180:
				pro_value = 2;
				break;
			case 270:
				pro_value = 1; // counter-clockwise 90
				break;
		}
		break;
	case ROTATION_USING_FLIP: // videoflip
		pro_value = dest_angle / 90;
		break;
	}

	debug_log("setting rotation property value : %d, used rotation type : %d", pro_value, rotation_type);

	*value = pro_value;

	return TRUE;
}
int _mmplayer_update_video_param(mm_player_t *player)
{
	MMHandleType attrs = 0;
	int surface_type = 0;
	int rotation_value = 0;
	int org_angle = 0;
	int user_angle = 0;
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

	__mmplayer_get_video_angle(player, &user_angle, &org_angle);

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
			int display_method = 0;

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
			mm_attrs_get_int_by_name(attrs, "display_method", &display_method);
#endif
			mm_attrs_get_data_by_name(attrs, "display_overlay", &surface);
			if ( surface ) {
#ifdef HAVE_WAYLAND
				guintptr wl_surface = (guintptr)surface;
				debug_log("set video param : surface %p", wl_surface);
				gst_video_overlay_set_window_handle(
						GST_VIDEO_OVERLAY( mainbin[MMPLAYER_M_V_SINK].gst ),
						wl_surface );
				/* After setting window handle, set render rectangle */
				gst_video_overlay_set_render_rectangle(
						GST_VIDEO_OVERLAY( mainbin[MMPLAYER_M_V_SINK].gst ),
						wl_window_x,wl_window_y,wl_window_width,wl_window_height);

				__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);
				g_object_set(mainbin[MMPLAYER_M_V_SINK].gst,
						"display-geometry-method", display_method,
						"rotate", rotation_value,
						NULL);
#else
				int xwin_id = 0;
				xwin_id = *(int*)surface;
				debug_log("set video param : xid %d", xwin_id);
				if (xwin_id)
				{
					gst_video_overlay_set_window_handle(
							GST_VIDEO_OVERLAY( mainbin[MMPLAYER_M_V_SINK].gst ),
							xwin_id );
				}
#endif
			}
			else
				debug_warning("still we don't have surface on player attribute. create it's own surface.");
		}
		break;
		case MM_DISPLAY_SURFACE_EVAS:
		{
			void *object = NULL;
			int scaling = 0;
			gboolean visible = TRUE;
			int display_method = 0;

			/* common case if using evas surface */
			mm_attrs_get_data_by_name(attrs, "display_overlay", &object);
			mm_attrs_get_int_by_name(attrs, "display_visible", &visible);
			mm_attrs_get_int_by_name(attrs, "display_evas_do_scaling", &scaling);
			mm_attrs_get_int_by_name(attrs, "display_method", &display_method);

			/* if evasimagesink */
			if (!strcmp(player->ini.videosink_element_evas,"evasimagesink"))
			{
				if (object)
				{
					/* if it is evasimagesink, we are not supporting rotation */
					if (user_angle!=0)
					{
						mm_attrs_set_int_by_name(attrs, "display_rotation", MM_DISPLAY_ROTATION_NONE);
						if (mmf_attrs_commit (attrs)) /* return -1 if error */
							debug_error("failed to commit\n");
						debug_warning("unsupported feature");
						return MM_ERROR_NOT_SUPPORT_API;
					}
					__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);
					g_object_set(mainbin[MMPLAYER_M_V_SINK].gst,
							"evas-object", object,
							"visible", visible,
							"display-geometry-method", display_method,
							"rotate", rotation_value,
							NULL);
					debug_log("set video param : method %d", display_method);
					debug_log("set video param : evas-object %x, visible %d", object, visible);
					debug_log("set video param : evas-object %x, rotate %d", object, rotation_value);
				}
				else
				{
					debug_error("no evas object");
					return MM_ERROR_PLAYER_INTERNAL;
				}


				/* if evasimagesink using converter */
				if (player->set_mode.video_zc && player->pipeline->videobin[MMPLAYER_V_CONV].gst)
				{
					int width = 0;
					int height = 0;
					int no_scaling = !scaling;

					mm_attrs_get_int_by_name(attrs, "display_width", &width);
					mm_attrs_get_int_by_name(attrs, "display_height", &height);

					/* NOTE: fimcconvert does not manage index of src buffer from upstream src-plugin, decoder gives frame information in output buffer with no ordering */
					g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "src-rand-idx", TRUE, NULL);
					g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "dst-buffer-num", 5, NULL);

					if (no_scaling)
					{
						/* no-scaling order to fimcconvert, original width, height size of media src will be passed to sink plugin */
						g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst,
								"dst-width", 0, /* setting 0, output video width will be media src's width */
								"dst-height", 0, /* setting 0, output video height will be media src's height */
								NULL);
					}
					else
					{
						/* scaling order to fimcconvert */
						if (width)
						{
							g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "dst-width", width, NULL);
						}
						if (height)
						{
							g_object_set(player->pipeline->videobin[MMPLAYER_V_CONV].gst, "dst-height", height, NULL);
						}
						debug_log("set video param : video frame scaling down to width(%d) height(%d)", width, height);
					}
					debug_log("set video param : display_evas_do_scaling %d", scaling);
				}
			}

			/* if evaspixmapsink */
			if (!strcmp(player->ini.videosink_element_evas,"evaspixmapsink"))
			{
				if (object)
				{
					//__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);
					g_object_set(mainbin[MMPLAYER_M_V_SINK].gst,
							"evas-object", object,
							"visible", visible,
							"display-geometry-method", display_method,
							"rotate", rotation_value,
							NULL);
					debug_log("set video param : method %d", display_method);
					debug_log("set video param : evas-object %x, visible %d", object, visible);
					debug_log("set video param : evas-object %x, rotate %d", object, rotation_value);
				}
				else
				{
					debug_error("no evas object");
					return MM_ERROR_PLAYER_INTERNAL;
				}

				int display_method = 0;
				int roi_x = 0;
				int roi_y = 0;
				int roi_w = 0;
				int roi_h = 0;
				int origin_size = !scaling;

				mm_attrs_get_int_by_name(attrs, "display_method", &display_method);
				mm_attrs_get_int_by_name(attrs, "display_roi_x", &roi_x);
				mm_attrs_get_int_by_name(attrs, "display_roi_y", &roi_y);
				mm_attrs_get_int_by_name(attrs, "display_roi_width", &roi_w);
				mm_attrs_get_int_by_name(attrs, "display_roi_height", &roi_h);

				/* get rotation value to set */
				//__mmplayer_get_property_value_for_rotation(player, org_angle+user_angle, &rotation_value);

				g_object_set(mainbin[MMPLAYER_M_V_SINK].gst,
					"origin-size", origin_size,
					"rotate", rotation_value,
					"dst-roi-x", roi_x,
					"dst-roi-y", roi_y,
					"dst-roi-w", roi_w,
					"dst-roi-h", roi_h,
					"display-geometry-method", display_method,
					NULL );

				debug_log("set video param : method %d", display_method);
				debug_log("set video param : dst-roi-x: %d, dst-roi-y: %d, dst-roi-w: %d, dst-roi-h: %d",
								roi_x, roi_y, roi_w, roi_h );
				debug_log("set video param : display_evas_do_scaling %d (origin-size %d)", scaling, origin_size);
			}
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

	MMPLAYER_MUSED_SET_STATE ( player, MM_PLAYER_STATE_NULL );

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
			MMPLAYER_MUSED_SET_STATE ( player, MM_PLAYER_STATE_PAUSED );
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

int mm_player_set_attribute(MMHandleType player,  char **err_attr_name, const char *first_attribute_name, ...)
{
	int result = MM_ERROR_NONE;
	va_list var_args;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(first_attribute_name, MM_ERROR_COMMON_INVALID_ARGUMENT);

	MMPLAYER_CMD_LOCK( player );

	va_start (var_args, first_attribute_name);
	result = _mmplayer_set_attribute(player, err_attr_name, first_attribute_name, var_args);
	va_end (var_args);

	MMPLAYER_CMD_UNLOCK( player );

	return result;
}

int mm_player_get_attribute(MMHandleType player,  char **err_attr_name, const char *first_attribute_name, ...)
{
	int result = MM_ERROR_NONE;
	va_list var_args;

	return_val_if_fail(player, MM_ERROR_PLAYER_NOT_INITIALIZED);
	return_val_if_fail(first_attribute_name, MM_ERROR_COMMON_INVALID_ARGUMENT);

	MMPLAYER_CMD_LOCK( player );

	va_start (var_args, first_attribute_name);
	result = _mmplayer_get_attribute(player, err_attr_name, first_attribute_name, var_args);
	va_end (var_args);

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
			"display_rotation",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) MM_DISPLAY_ROTATION_NONE,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			MM_DISPLAY_ROTATION_NONE,
			MM_DISPLAY_ROTATION_270
		},
		{
			"display_visible",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) FALSE,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			0,
			1
		},
		{
			"display_method",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) MM_DISPLAY_METHOD_LETTER_BOX,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			MM_DISPLAY_METHOD_LETTER_BOX,
			MM_DISPLAY_METHOD_CUSTOM_ROI
		},
		{
			"shm_stream_path",
			MM_ATTRS_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			(void *) NULL,
			MM_ATTRS_VALID_TYPE_NONE,
			0,
			0
		},
		{
			"pipeline_type",
			MM_ATTRS_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			(void *) MM_PLAYER_PIPELINE_LEGACY,
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			MM_PLAYER_PIPELINE_LEGACY,
			MM_PLAYER_PIPELINE_MAX - 1
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


static GstBusSyncReply
__mmplayer_mused_bus_sync_callback (GstBus * bus, GstMessage * message, gpointer data)
{
	mm_player_t *player = (mm_player_t *)data;
	GstBusSyncReply reply = GST_BUS_DROP;

	if ( ! ( player->pipeline && player->pipeline->mainbin ) )
	{
		debug_error("player pipeline handle is null");
		return GST_BUS_PASS;
	}

	if (!__mmplayer_check_useful_message(player, message))
	{
		gst_message_unref (message);
		return GST_BUS_DROP;
	}

	switch (GST_MESSAGE_TYPE (message))
	{
		case GST_MESSAGE_STATE_CHANGED:
			/* post directly for fast launch */
			if (player->sync_handler) {
				__mmplayer_mused_gst_callback(NULL, message, player);
				reply = GST_BUS_DROP;
			}
			else {
				reply = GST_BUS_PASS;
			}
			break;
		default:
			reply = GST_BUS_PASS;
			break;
	}

	if (reply == GST_BUS_DROP)
		gst_message_unref (message);

	return reply;
}

static gboolean
__mmplayer_mused_gst_callback(GstBus *bus, GstMessage *msg, gpointer data) // @
{
	mm_player_t* player = (mm_player_t*) data;
	gboolean ret = TRUE;

	return_val_if_fail ( player, FALSE );
	return_val_if_fail ( msg && GST_IS_MESSAGE(msg), FALSE );

	switch ( GST_MESSAGE_TYPE( msg ) )
	{
		case GST_MESSAGE_UNKNOWN:
			debug_log("unknown message received\n");
		break;

		case GST_MESSAGE_EOS:
		{
			debug_log("GST_MESSAGE_EOS received\n");

			/* NOTE : EOS event is comming multiple time. watch out it */
			/* check state. we only process EOS when pipeline state goes to PLAYING */
			if ( ! (player->cmd == MMPLAYER_COMMAND_START || player->cmd == MMPLAYER_COMMAND_RESUME) )
			{
				debug_log("EOS received on non-playing state. ignoring it\n");
				break;
			}
		}
		break;

		case GST_MESSAGE_ERROR:
		{
			GError *error = NULL;
			gchar* debug = NULL;

			/* generating debug info before returning error */
			MMPLAYER_GENERATE_DOT_IF_ENABLED ( player, "pipeline-status-error" );

			/* get error code */
			gst_message_parse_error( msg, &error, &debug );

			/* traslate gst error code to msl error code. then post it
			 * to application if needed
			 */
			__mmplayer_handle_gst_error( player, msg, error );

			if (debug)
			{
				debug_error ("error debug : %s", debug);
			}

			MMPLAYER_FREEIF( debug );
			g_error_free( error );
		}
		break;

		case GST_MESSAGE_WARNING:
		{
			char* debug = NULL;
			GError* error = NULL;

			gst_message_parse_warning(msg, &error, &debug);

			debug_log("warning : %s\n", error->message);
			debug_log("debug : %s\n", debug);

			MMPLAYER_FREEIF( debug );
			g_error_free( error );
		}
		break;

		case GST_MESSAGE_TAG:
		{
			debug_log("GST_MESSAGE_TAG\n");
		}
		break;

		case GST_MESSAGE_BUFFERING:
		break;

		case GST_MESSAGE_STATE_CHANGED:
		{
			MMPlayerGstElement *mainbin;
			const GValue *voldstate, *vnewstate, *vpending;
			GstState oldstate, newstate, pending;

			if ( ! ( player->pipeline && player->pipeline->mainbin ) )
			{
				debug_error("player pipeline handle is null");
				break;
			}

			mainbin = player->pipeline->mainbin;

			/* we only handle messages from pipeline */
			if( msg->src != (GstObject *)mainbin[MMPLAYER_M_PIPE].gst )
				break;

			/* get state info from msg */
			voldstate = gst_structure_get_value (gst_message_get_structure(msg), "old-state");
			vnewstate = gst_structure_get_value (gst_message_get_structure(msg), "new-state");
			vpending = gst_structure_get_value (gst_message_get_structure(msg), "pending-state");

			oldstate = (GstState)voldstate->data[0].v_int;
			newstate = (GstState)vnewstate->data[0].v_int;
			pending = (GstState)vpending->data[0].v_int;

			debug_log("state changed [%s] : %s ---> %s     final : %s\n",
				GST_OBJECT_NAME(GST_MESSAGE_SRC(msg)),
				gst_element_state_get_name( (GstState)oldstate ),
				gst_element_state_get_name( (GstState)newstate ),
				gst_element_state_get_name( (GstState)pending ) );

			if (oldstate == newstate)
			{
				debug_log("pipeline reports state transition to old state");
				break;
			}

			switch(newstate)
			{
				case GST_STATE_VOID_PENDING:
				break;

				case GST_STATE_NULL:
				break;

				case GST_STATE_READY:
				break;

				case GST_STATE_PAUSED:
				{
					gboolean prepare_async = FALSE;

					if ( ! player->sent_bos && oldstate == GST_STATE_READY) // managed prepare async case
					{
						mm_attrs_get_int_by_name(player->attrs, "profile_prepare_async", &prepare_async);
						debug_log("checking prepare mode for async transition - %d", prepare_async);
					}
				}
				break;

				case GST_STATE_PLAYING:
				break;

				default:
				break;
			}
		}
		break;

		case GST_MESSAGE_CLOCK_LOST:
		{
			GstClock *clock = NULL;
			gst_message_parse_clock_lost (msg, &clock);
			debug_log("GST_MESSAGE_CLOCK_LOST : %s\n", (clock ? GST_OBJECT_NAME (clock) : "NULL"));
		}
		break;

		case GST_MESSAGE_NEW_CLOCK:
		{
			GstClock *clock = NULL;
			gst_message_parse_new_clock (msg, &clock);
			debug_log("GST_MESSAGE_NEW_CLOCK : %s\n", (clock ? GST_OBJECT_NAME (clock) : "NULL"));
		}
		break;

		case GST_MESSAGE_ELEMENT:
		{
			debug_log("GST_MESSAGE_ELEMENT");
		}
		break;

		case GST_MESSAGE_DURATION_CHANGED:
		{
			debug_log("GST_MESSAGE_DURATION_CHANGED");
		}

		break;

		case GST_MESSAGE_ASYNC_START:
		{
			debug_log("GST_MESSAGE_ASYNC_START : %s", GST_ELEMENT_NAME(GST_MESSAGE_SRC(msg)));
		}
		break;

		case GST_MESSAGE_ASYNC_DONE:
		{
			debug_log("GST_MESSAGE_ASYNC_DONE : %s", GST_ELEMENT_NAME(GST_MESSAGE_SRC(msg)));
		}
		break;

		default:
		break;
	}

	/* FIXIT : this cause so many warnings/errors from glib/gstreamer. we should not call it since
	 * gst_element_post_message api takes ownership of the message.
	 */
	//gst_message_unref( msg );

	return ret;
}

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

static int __mmplayer_mused_set_state(mm_player_t* player, int state)
{
	int ret = MM_ERROR_NONE;

	return_val_if_fail ( player, FALSE );

	if ( MMPLAYER_CURRENT_STATE(player) == state )
	{
		debug_warning("already same state(%s)\n", MMPLAYER_STATE_GET_NAME(state));
		MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_NONE;
		return ret;
	}

	/* update player states */
	MMPLAYER_PREV_STATE(player) = MMPLAYER_CURRENT_STATE(player);
	MMPLAYER_CURRENT_STATE(player) = state;

	/* FIXIT : it's better to do like below code
	if ( MMPLAYER_CURRENT_STATE(player) == MMPLAYER_TARGET_STATE(player) )
			MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_NONE;
	and add more code to handling PENDING_STATE.
	*/
	if ( MMPLAYER_CURRENT_STATE(player) == MMPLAYER_PENDING_STATE(player) )
		MMPLAYER_PENDING_STATE(player) = MM_PLAYER_STATE_NONE;

	/* print state */
	MMPLAYER_PRINT_STATE(player);

	/* post message to application */
	if (MMPLAYER_TARGET_STATE(player) == state)
	{
		debug_log ("player reach the target state (%s)", MMPLAYER_STATE_GET_NAME(MMPLAYER_TARGET_STATE(player)));
	}
	else
	{
		debug_log ("intermediate state, do nothing.\n");
		MMPLAYER_PRINT_STATE(player);
		return ret;
	}

	return ret;
}
