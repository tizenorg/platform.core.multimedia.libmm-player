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

/*===========================================================================================
|																							|
|  INCLUDE FILES																			|
|  																							|
========================================================================================== */
#include "mm_player_utils.h"
#include "mm_player_capture.h"
#include "mm_player_priv.h"
#include "mm_player_utils.h"

#include <mm_util_imgp.h>
#include <gst/video/video-info.h>

/*---------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS for internal								|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:												|
---------------------------------------------------------------------------*/
static GstPadProbeReturn __mmplayer_video_capture_probe (GstPad *pad, GstPadProbeInfo *info, gpointer u_data);
static int  __mmplayer_get_video_frame_from_buffer(mm_player_t* player, GstPad *pad, GstBuffer *buffer);
static gpointer __mmplayer_capture_thread(gpointer data);
static void __csc_tiled_to_linear_crop(unsigned char *yuv420_dest, unsigned char *nv12t_src, int yuv420_width, int yuv420_height, int left, int top, int right, int buttom);
static int __tile_4x2_read(int x_size, int y_size, int x_pos, int y_pos);
static int __mm_player_convert_colorspace(mm_player_t* player, unsigned char* src_data, mm_util_img_format src_fmt, unsigned int src_w, unsigned int src_h, mm_util_img_format dst_fmt);

/*===========================================================================================
|																							|
|  FUNCTION DEFINITIONS																		|
|  																							|
========================================================================================== */
int
_mmplayer_initialize_video_capture(mm_player_t* player)
{
	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	/* create capture mutex */
	g_mutex_init(&player->capture_thread_mutex);

	/* create capture cond */
	g_cond_init(&player->capture_thread_cond);


	player->capture_thread_exit = FALSE;

	/* create video capture thread */
	player->capture_thread =
			g_thread_try_new ("capture_thread",__mmplayer_capture_thread, (gpointer)player, NULL);

	if ( ! player->capture_thread )
	{
		goto ERROR;
	}

	return MM_ERROR_NONE;

ERROR:
	/* capture thread */
	g_mutex_clear(&player->capture_thread_mutex );

	g_cond_clear (&player->capture_thread_cond );

	return MM_ERROR_PLAYER_INTERNAL;
}

int
_mmplayer_release_video_capture(mm_player_t* player)
{
	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	/* release capture thread */
	g_mutex_lock(&player->capture_thread_mutex);
	player->capture_thread_exit = TRUE;
	g_cond_signal( &player->capture_thread_cond );
	g_mutex_unlock(&player->capture_thread_mutex);

	debug_log("waitting for capture thread exit");
	g_thread_join ( player->capture_thread );
	g_mutex_clear(&player->capture_thread_mutex );
	g_cond_clear(&player->capture_thread_cond );
	debug_log("capture thread released");

	return MM_ERROR_NONE;
}

int
_mmplayer_do_video_capture(MMHandleType hplayer)
{
	mm_player_t* player = (mm_player_t*) hplayer;
	int ret = MM_ERROR_NONE;
	GstPad *pad = NULL;

	MMPLAYER_FENTER();

	return_val_if_fail(player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED);

	/* capturing or not */
	if (player->video_capture_cb_probe_id || player->capture.data
			|| player->captured.data[0] || player->captured.data[1]
			)
	{
		debug_warning("capturing... we can't do any more");
		return MM_ERROR_PLAYER_INVALID_STATE;
	}

	/* check if video pipeline is linked or not */
	if (!player->pipeline->videobin)
	{
		debug_warning("not ready to capture");
		return MM_ERROR_PLAYER_INVALID_STATE;
	}

	/* check if drm file */
	if (player->is_drm_file)
	{
		debug_warning("not supported in drm file");
		return MM_ERROR_PLAYER_DRM_OUTPUT_PROTECTION;
	}

	pad = gst_element_get_static_pad(player->pipeline->videobin[MMPLAYER_V_SINK].gst, "sink" );

	if (player->state != MM_PLAYER_STATE_PLAYING)
	{
		if (player->state == MM_PLAYER_STATE_PAUSED) // get last buffer from video sink
		{
			GstSample *sample = NULL;

			gst_element_get_state(player->pipeline->mainbin[MMPLAYER_M_PIPE].gst, NULL, NULL, 5 * GST_SECOND); //5 seconds

			g_object_get(player->pipeline->videobin[MMPLAYER_V_SINK].gst, "last-sample", &sample, NULL);

			if (sample)
			{
				GstBuffer *buf = NULL;
				buf = gst_sample_get_buffer(sample);

				if (buf)
				{
					ret = __mmplayer_get_video_frame_from_buffer(player, pad, buf);
				}
				else
				{
					debug_warning("failed to get video frame");
				}
				gst_sample_unref(sample);
			}
			return ret;
		}
		else
		{
			debug_warning("invalid state(%d) to capture", player->state);
			return MM_ERROR_PLAYER_INVALID_STATE;
		}
	}

	/* register probe */
	player->video_capture_cb_probe_id = gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
		__mmplayer_video_capture_probe, player, NULL);

	gst_object_unref(GST_OBJECT(pad));
	pad = NULL;

	MMPLAYER_FLEAVE();

	return ret;
}

int
__mmplayer_handle_orientation (mm_player_t* player, int orientation, int format)
{
    unsigned char *src_buffer = NULL;
    int ret = MM_ERROR_NONE;
    unsigned char *dst_frame = NULL;
    unsigned int dst_width = 0;
    unsigned int dst_height = 0;
    unsigned int dst_size = 0;
    mm_util_img_rotate_type rot_enum = MM_UTIL_ROTATE_NUM;

	player->capture.orientation = orientation;

    if (orientation == 90 || orientation == 270)
    {
		dst_width = player->captured.height[0];
		dst_height = player->captured.width[0];
		debug_log ("interchange width & height");
    }
    else if (orientation == 180)
    {
		dst_width = player->captured.width[0];
		dst_height = player->captured.height[0];
    }
    else if (orientation == 0)
    {
		debug_error ("no need handle orientation : %d", orientation);
		player->capture.width = player->captured.width[0];
		player->capture.height = player->captured.height[0];
		return MM_ERROR_NONE;
    }
    else
    {
		debug_error ("wrong orientation value...");
    }

    /* height & width will be interchanged for 90 and 270 orientation */
    ret = mm_util_get_image_size(format, dst_width, dst_height, &dst_size);
    if (ret != MM_ERROR_NONE)
    {
		debug_error("failed to get destination frame size");
		return ret;
    }

    debug_log ("before rotation : dst_width = %d and dst_height = %d", dst_width, dst_height);

    dst_frame = (unsigned char*) malloc (dst_size);
    if (!dst_frame)
    {
      debug_error("failed to allocate memory");
      return MM_ERROR_PLAYER_NO_FREE_SPACE;
    }

    src_buffer = (unsigned char*)player->capture.data;

	/* convert orientation degree into enum here */
	switch(orientation)
	{
		case 0:
			rot_enum = MM_UTIL_ROTATE_0;
		break;
		case 90:
			rot_enum = MM_UTIL_ROTATE_90;
		break;
		case 180:
			rot_enum = MM_UTIL_ROTATE_180;
		break;
		case 270:
			rot_enum = MM_UTIL_ROTATE_270;
		break;
		default:
			debug_error("wrong rotate value");
		break;
	}

    debug_log ("source buffer for rotation = %p and rotation = %d", src_buffer, rot_enum);

    ret = mm_util_rotate_image (src_buffer,
			player->captured.width[0], player->captured.height[0], format,
			dst_frame, &dst_width, &dst_height, rot_enum);
    if (ret != MM_ERROR_NONE)
    {
      debug_error("failed to do rotate image");
      return ret;
    }

    debug_log ("after rotation same stride: dst_width = %d and dst_height = %d", dst_width, dst_height);

    g_free (src_buffer);

    player->capture.data = dst_frame;
    player->capture.size = dst_size;
	player->capture.orientation = orientation;
	player->capture.width = dst_width;
	player->capture.height= dst_height;

    player->captured.width[0] = player->captured.stride_width[0] = dst_width;
    player->captured.height[0] = player->captured.stride_height[0] = dst_height;

    return ret;
}


static gpointer
__mmplayer_capture_thread(gpointer data)
{
	mm_player_t* player = (mm_player_t*) data;
	MMMessageParamType msg = {0, };
	unsigned char * linear_y_plane = NULL;
	unsigned char * linear_uv_plane = NULL;
	int orientation = 0;
	int ret = 0;

	return_val_if_fail(player, NULL);

	while (!player->capture_thread_exit)
	{
		debug_log("capture thread started. waiting for signal");

		g_mutex_lock(&player->capture_thread_mutex);
		g_cond_wait(&player->capture_thread_cond, &player->capture_thread_mutex );

		if ( player->capture_thread_exit )
		{
			debug_log("exiting capture thread");
			goto EXIT;
		}
		debug_log("capture thread is recieved signal");

		/* NOTE: Don't use MMPLAYER_CMD_LOCK() here.
		 * Because deadlock can be happened if other player api is used in message callback.
		 */
		if (player->video_cs == MM_PLAYER_COLORSPACE_NV12_TILED)
		{
			/* Colorspace conversion : NV12T-> NV12-> RGB888 */
			int ret = 0;
			int linear_y_plane_size;
			int linear_uv_plane_size;
			int width = player->captured.width[0];
			int height = player->captured.height[0];
			unsigned char * src_buffer = NULL;

			linear_y_plane_size = (width * height);
			linear_uv_plane_size = (width * height / 2);

			linear_y_plane = (unsigned char*) g_try_malloc(linear_y_plane_size);
			if (linear_y_plane == NULL)
			{
				msg.code = MM_ERROR_PLAYER_NO_FREE_SPACE;
				goto ERROR;
			}

			linear_uv_plane = (unsigned char*) g_try_malloc(linear_uv_plane_size);
			if (linear_uv_plane == NULL)
			{
				msg.code = MM_ERROR_PLAYER_NO_FREE_SPACE;
				goto ERROR;
			}
			/* NV12 tiled to linear */
			__csc_tiled_to_linear_crop(linear_y_plane,
					player->captured.data[0], width, height, 0,0,0,0);
			__csc_tiled_to_linear_crop(linear_uv_plane,
					player->captured.data[1], width, height / 2, 0,0,0,0);

			MMPLAYER_FREEIF(player->captured.data[0]);
			MMPLAYER_FREEIF(player->captured.data[1]);

			src_buffer = (unsigned char*) g_try_malloc(linear_y_plane_size+linear_uv_plane_size);

			if (src_buffer == NULL)
			{
				msg.code = MM_ERROR_PLAYER_NO_FREE_SPACE;
				goto ERROR;
			}
			memset(src_buffer, 0x00, linear_y_plane_size+linear_uv_plane_size);
			memcpy(src_buffer, linear_y_plane, linear_y_plane_size);
			memcpy(src_buffer+linear_y_plane_size, linear_uv_plane, linear_uv_plane_size);

			/* NV12 linear to RGB888 */
			ret = __mm_player_convert_colorspace(player, src_buffer, MM_UTIL_IMG_FMT_NV12,
				width, height, MM_UTIL_IMG_FMT_RGB888);

			if (ret != MM_ERROR_NONE)
			{
				debug_error("failed to convert nv12 linear");
				goto ERROR;
			}
			/* clean */
			MMPLAYER_FREEIF(src_buffer);
			MMPLAYER_FREEIF(linear_y_plane);
			MMPLAYER_FREEIF(linear_uv_plane);
		} else if (MM_PLAYER_COLORSPACE_NV12 == player->video_cs) {
			#define MM_ALIGN(x, a)       (((x) + (a) - 1) & ~((a) - 1))
			int ret = 0;
			char *src_buffer = NULL;
			/* using original width otherwises, app can't know aligned to resize */
			int width_align = player->captured.width[0];
			int src_buffer_size = width_align * player->captured.height[0] * 3/2;
			int i, j;
			char*temp = NULL;
			char*dst_buf = NULL;

			if (!src_buffer_size) {
				debug_error("invalid data size");
				goto ERROR;
			}

			src_buffer = (char*) g_try_malloc(src_buffer_size);

			if (!src_buffer) {
				msg.code = MM_ERROR_PLAYER_NO_FREE_SPACE;
				goto ERROR;
			}

			memset(src_buffer, 0x00, src_buffer_size);

			temp = player->captured.data[0];
			dst_buf = src_buffer;

			/* set Y plane */
			for (i = 0; i < player->captured.height[0]; i++) {
				memcpy(dst_buf, temp, width_align);
				dst_buf += width_align;
				temp += player->captured.stride_width[0];
			}

			temp = player->captured.data[1];

			/* set UV plane*/
			for (j = 0; j < player->captured.height[1]; j++) {
				memcpy(dst_buf, temp, width_align);
				dst_buf += width_align;
				temp += player->captured.stride_width[0];
			}

			/* free captured buf */
			MMPLAYER_FREEIF(player->captured.data[0]);
			MMPLAYER_FREEIF(player->captured.data[1]);

			/* NV12 -> RGB888 */
			ret = __mm_player_convert_colorspace(player, (unsigned char*)src_buffer, MM_UTIL_IMG_FMT_NV12,
				width_align, player->captured.height[0], MM_UTIL_IMG_FMT_RGB888);
			if (ret != MM_ERROR_NONE)
			{
				debug_error("failed to convert nv12 linear");
				goto ERROR;
			}

		}

		ret = _mmplayer_get_video_rotate_angle ((MMHandleType)player, &orientation);
		if (ret != MM_ERROR_NONE)
		{
			debug_error("failed to get rotation angle");
			goto ERROR;
		}

		debug_log ("orientation value = %d", orientation);

		ret = __mmplayer_handle_orientation (player, orientation, MM_UTIL_IMG_FMT_RGB888);
		if (ret != MM_ERROR_NONE)
		{
			debug_error("failed to convert nv12 linear");
			goto ERROR;
		}

		player->capture.fmt = MM_PLAYER_COLORSPACE_RGB888;
		msg.data = &player->capture;
		msg.size = player->capture.size;
//		msg.captured_frame.width = player->capture.width;
//		msg.captured_frame.height = player->capture.height;
//		msg.captured_frame.orientation = player->capture.orientation;

		if (player->cmd >= MMPLAYER_COMMAND_START)
		{
			MMPLAYER_POST_MSG( player, MM_MESSAGE_VIDEO_CAPTURED, &msg );
			debug_log("returned from capture message callback");
		}

		g_mutex_unlock(&player->capture_thread_mutex);

		//MMPLAYER_FREEIF(player->capture.data);
		continue;
ERROR:
		if (player->video_cs == MM_PLAYER_COLORSPACE_NV12_TILED)
		{
			/* clean */
			MMPLAYER_FREEIF(linear_y_plane);
			MMPLAYER_FREEIF(linear_uv_plane);
			MMPLAYER_FREEIF(player->captured.data[0]);
			MMPLAYER_FREEIF(player->captured.data[1]);
		}

		msg.union_type = MM_MSG_UNION_CODE;

		g_mutex_unlock(&player->capture_thread_mutex);
		MMPLAYER_POST_MSG( player, MM_MESSAGE_VIDEO_NOT_CAPTURED, &msg );
	}
	return NULL;
EXIT:
	g_mutex_unlock(&player->capture_thread_mutex);
	return NULL;
}

/**
  * The output is fixed as RGB888
  */
static int
__mmplayer_get_video_frame_from_buffer(mm_player_t* player, GstPad *pad, GstBuffer *buffer)
{
	gint yplane_size = 0;
	gint uvplane_size = 0;
	gint src_width = 0;
	gint src_height = 0;
	GstCaps *caps = NULL;
	GstStructure *structure = NULL;
	GstMapInfo mapinfo = GST_MAP_INFO_INIT;
	GstMemory *memory = NULL;
	mm_util_img_format src_fmt = MM_UTIL_IMG_FMT_YUV420;
	mm_util_img_format dst_fmt = MM_UTIL_IMG_FMT_RGB888; // fixed

	MMPLAYER_FENTER();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( buffer, MM_ERROR_INVALID_ARGUMENT );

	/* get fourcc */
	caps = gst_pad_get_current_caps(pad);

	return_val_if_fail ( caps, MM_ERROR_INVALID_ARGUMENT );
	MMPLAYER_LOG_GST_CAPS_TYPE(caps);

	structure = gst_caps_get_structure (caps, 0);

	return_val_if_fail (structure != NULL, MM_ERROR_PLAYER_INTERNAL);

	/* init capture image buffer */
	memset(&player->capture, 0x00, sizeof(MMPlayerVideoCapture));

	gst_structure_get_int (structure, "width", &src_width);
	gst_structure_get_int (structure, "height", &src_height);

	/* check rgb or yuv */
	if (gst_structure_has_name(structure, "video/x-raw"))
	{
		/* NV12T */
		if(!g_strcmp0(gst_structure_get_string(structure, "format"), "ST12"))
		{
			debug_msg ("captured format is ST12\n");

			MMVideoBuffer *proved = NULL;
			player->video_cs = MM_PLAYER_COLORSPACE_NV12_TILED;

			/* get video frame info from proved buffer */
			memory = gst_buffer_get_all_memory(buffer);
			gst_memory_map(memory, &mapinfo, GST_MAP_READ);
			proved = (MMVideoBuffer *)mapinfo.data;

			if ( !proved || !proved->data[0] || !proved->data[1] )
				return MM_ERROR_PLAYER_INTERNAL;

			yplane_size = proved->size[0];
			uvplane_size = proved->size[1];

			debug_msg ("yplane_size=%d, uvplane_size=%d\n", yplane_size, uvplane_size);
			memset(&player->captured, 0x00, sizeof(MMVideoBuffer));
			memcpy(&player->captured, proved, sizeof(MMVideoBuffer));

			player->captured.data[0] = g_try_malloc(yplane_size);
			if ( !player->captured.data[0] ) {
				gst_memory_unmap(memory, &mapinfo);
				return MM_ERROR_SOUND_NO_FREE_SPACE;
			}

			player->captured.data[1] = g_try_malloc(uvplane_size);
			if ( !player->captured.data[1] ) {
				gst_memory_unmap(memory, &mapinfo);
				return MM_ERROR_SOUND_NO_FREE_SPACE;
			}

			memcpy(player->captured.data[0], proved->data[0], yplane_size);
			memcpy(player->captured.data[1], proved->data[1], uvplane_size);

			gst_memory_unmap(memory, &mapinfo);

			goto DONE;
		}
		else
		{
			GstVideoInfo format_info;
			gst_video_info_from_caps(&format_info, caps);

			switch(GST_VIDEO_INFO_FORMAT(&format_info))
			{
				case GST_VIDEO_FORMAT_I420:
					src_fmt = MM_UTIL_IMG_FMT_I420;
					break;
				case GST_VIDEO_FORMAT_BGRA:
					src_fmt = MM_UTIL_IMG_FMT_BGRA8888;
					break;
				case GST_VIDEO_FORMAT_BGRx:
					src_fmt = MM_UTIL_IMG_FMT_BGRX8888;
					break;
				default:
					goto UNKNOWN;
					break;
			}
		}

		#if 0
		case GST_MAKE_FOURCC ('S', 'N', '1', '2'):
		{
			MMPlayerMPlaneImage *proved = NULL;
			player->video_cs = MM_PLAYER_COLORSPACE_NV12;

			/* get video frame info from proved buffer */
			proved = (MMPlayerMPlaneImage *)GST_BUFFER_MALLOCDATA(buffer);

			if (!proved || !proved->a[0] || !proved->a[1])
				return MM_ERROR_PLAYER_INTERNAL;

			memset(&player->captured, 0x00, sizeof(MMPlayerMPlaneImage));
			memcpy(&player->captured, proved, sizeof(MMPlayerMPlaneImage));

			player->captured.y_size = proved->s[0] * proved->h[0]; // must get data including padding
			player->captured.uv_size = proved->s[0] * proved->h[1];

			debug_msg ("y plane_size : %d, uv plane_size : %d", player->captured.y_size, player->captured.uv_size);

			player->captured.a[0] = g_try_malloc(player->captured.y_size);

			if ( !player->captured.a[0] ) {
				return MM_ERROR_SOUND_NO_FREE_SPACE;
			}

			player->captured.a[1] = g_try_malloc(player->captured.uv_size);

			if ( !player->captured.a[1] ) {
				return MM_ERROR_SOUND_NO_FREE_SPACE;
			}

			memcpy(player->captured.a[0], proved->a[0], player->captured.y_size);
			memcpy(player->captured.a[1], proved->a[1], player->captured.uv_size);

			goto DONE;
		}
		break;
		#endif
	}
	else
	{
		goto UNKNOWN;
	}

	gst_buffer_map(buffer, &mapinfo, GST_MAP_READ);
	__mm_player_convert_colorspace(player, mapinfo.data, src_fmt, src_width, src_height, dst_fmt);
	gst_buffer_unmap(buffer, &mapinfo);

DONE:
	/* do convert colorspace */
	g_cond_signal( &player->capture_thread_cond );

	MMPLAYER_FLEAVE();

	return MM_ERROR_NONE;

UNKNOWN:
	debug_error("unknown format to capture\n");
	return MM_ERROR_PLAYER_INTERNAL;
}

static GstPadProbeReturn
__mmplayer_video_capture_probe (GstPad *pad, GstPadProbeInfo *info, gpointer u_data)
{
	mm_player_t* player = (mm_player_t*) u_data;
	GstBuffer *buffer = NULL;
	int ret = MM_ERROR_NONE;

	return_val_if_fail (info->data, GST_PAD_PROBE_REMOVE);
	MMPLAYER_FENTER();

	buffer = gst_pad_probe_info_get_buffer(info);
	ret = __mmplayer_get_video_frame_from_buffer(player, pad, buffer);

	if ( ret != MM_ERROR_NONE)
	{
		debug_error("failed to get video frame");
		return GST_PAD_PROBE_REMOVE;
	}

	/* remove probe to be called at one time */
	if (player->video_capture_cb_probe_id)
	{
		gst_pad_remove_probe(pad, player->video_capture_cb_probe_id);
		player->video_capture_cb_probe_id = 0;
	}

	MMPLAYER_FLEAVE();

	return GST_PAD_PROBE_OK;
}

static int
__mm_player_convert_colorspace(mm_player_t* player, unsigned char* src_data, mm_util_img_format src_fmt, unsigned int src_w, unsigned int src_h, mm_util_img_format dst_fmt)
{
	unsigned char *dst_data = NULL;
	unsigned int dst_size;
	int ret = MM_ERROR_NONE;

	return_val_if_fail(player, MM_ERROR_PLAYER_INTERNAL);
	ret = mm_util_get_image_size(dst_fmt, src_w, src_h, &dst_size);

	if (ret != MM_ERROR_NONE)
	{
		debug_error("failed to get image size for capture, %d\n", ret);
		return MM_ERROR_PLAYER_INTERNAL;
	}

	secure_debug_log("width: %d, height: %d to capture, dest size: %d\n", src_w, src_h, dst_size);

	dst_data = (unsigned char*)g_malloc0(dst_size);

	if (!dst_data)
	{
		debug_error("no free space to capture\n");
		return MM_ERROR_PLAYER_NO_FREE_SPACE;
	}

	ret = mm_util_convert_colorspace(src_data, src_w, src_h, src_fmt, dst_data, dst_fmt);

	if (ret != MM_ERROR_NONE)
	{
		debug_error("failed to convert for capture, %d\n", ret);
		return MM_ERROR_PLAYER_INTERNAL;
	}

	player->capture.size = dst_size;
	player->capture.data = dst_data;

	return MM_ERROR_NONE;
}

/*
 * Get tiled address of position(x,y)
 *
 * @param x_size
 *   width of tiled[in]
 *
 * @param y_size
 *   height of tiled[in]
 *
 * @param x_pos
 *   x position of tield[in]
 *
 * @param src_size
 *   y position of tield[in]
 *
 * @return
 *   address of tiled data
 */
static int
__tile_4x2_read(int x_size, int y_size, int x_pos, int y_pos)
{
    int pixel_x_m1, pixel_y_m1;
    int roundup_x;
    int linear_addr0, linear_addr1, bank_addr ;
    int x_addr;
    int trans_addr;

    pixel_x_m1 = x_size -1;
    pixel_y_m1 = y_size -1;

    roundup_x = ((pixel_x_m1 >> 7) + 1);

    x_addr = x_pos >> 2;

    if ((y_size <= y_pos+32) && ( y_pos < y_size) &&
        (((pixel_y_m1 >> 5) & 0x1) == 0) && (((y_pos >> 5) & 0x1) == 0)) {
        linear_addr0 = (((y_pos & 0x1f) <<4) | (x_addr & 0xf));
        linear_addr1 = (((y_pos >> 6) & 0xff) * roundup_x + ((x_addr >> 6) & 0x3f));

        if (((x_addr >> 5) & 0x1) == ((y_pos >> 5) & 0x1))
            bank_addr = ((x_addr >> 4) & 0x1);
        else
            bank_addr = 0x2 | ((x_addr >> 4) & 0x1);
    } else {
        linear_addr0 = (((y_pos & 0x1f) << 4) | (x_addr & 0xf));
        linear_addr1 = (((y_pos >> 6) & 0xff) * roundup_x + ((x_addr >> 5) & 0x7f));

        if (((x_addr >> 5) & 0x1) == ((y_pos >> 5) & 0x1))
            bank_addr = ((x_addr >> 4) & 0x1);
        else
            bank_addr = 0x2 | ((x_addr >> 4) & 0x1);
    }

    linear_addr0 = linear_addr0 << 2;
    trans_addr = (linear_addr1 <<13) | (bank_addr << 11) | linear_addr0;

    return trans_addr;
}

/*
 * Converts tiled data to linear
 * Crops left, top, right, buttom
 * 1. Y of NV12T to Y of YUV420P
 * 2. Y of NV12T to Y of YUV420S
 * 3. UV of NV12T to UV of YUV420S
 *
 * @param yuv420_dest
 *   Y or UV plane address of YUV420[out]
 *
 * @param nv12t_src
 *   Y or UV plane address of NV12T[in]
 *
 * @param yuv420_width
 *   Width of YUV420[in]
 *
 * @param yuv420_height
 *   Y: Height of YUV420, UV: Height/2 of YUV420[in]
 *
 * @param left
 *   Crop size of left
 *
 * @param top
 *   Crop size of top
 *
 * @param right
 *   Crop size of right
 *
 * @param buttom
 *   Crop size of buttom
 */
static void
__csc_tiled_to_linear_crop(unsigned char *yuv420_dest, unsigned char *nv12t_src, int yuv420_width, int yuv420_height,
                                int left, int top, int right, int buttom)
{
    int i, j;
    int tiled_offset = 0, tiled_offset1 = 0;
    int linear_offset = 0;
    int temp1 = 0, temp2 = 0, temp3 = 0, temp4 = 0;

    temp3 = yuv420_width-right;
    temp1 = temp3-left;
    /* real width is greater than or equal 256 */
    if (temp1 >= 256) {
        for (i=top; i<yuv420_height-buttom; i=i+1) {
            j = left;
            temp3 = (j>>8)<<8;
            temp3 = temp3>>6;
            temp4 = i>>5;
            if (temp4 & 0x1) {
                /* odd fomula: 2+x+(x>>2)<<2+x_block_num*(y-1) */
                tiled_offset = temp4-1;
                temp1 = ((yuv420_width+127)>>7)<<7;
                tiled_offset = tiled_offset*(temp1>>6);
                tiled_offset = tiled_offset+temp3;
                tiled_offset = tiled_offset+2;
                temp1 = (temp3>>2)<<2;
                tiled_offset = tiled_offset+temp1;
                tiled_offset = tiled_offset<<11;
                tiled_offset1 = tiled_offset+2048*2;
                temp4 = 8;
            } else {
                temp2 = ((yuv420_height+31)>>5)<<5;
                if ((i+32)<temp2) {
                    /* even1 fomula: x+((x+2)>>2)<<2+x_block_num*y */
                    temp1 = temp3+2;
                    temp1 = (temp1>>2)<<2;
                    tiled_offset = temp3+temp1;
                    temp1 = ((yuv420_width+127)>>7)<<7;
                    tiled_offset = tiled_offset+temp4*(temp1>>6);
                    tiled_offset = tiled_offset<<11;
                    tiled_offset1 = tiled_offset+2048*6;
                    temp4 = 8;
                } else {
                    /* even2 fomula: x+x_block_num*y */
                    temp1 = ((yuv420_width+127)>>7)<<7;
                    tiled_offset = temp4*(temp1>>6);
                    tiled_offset = tiled_offset+temp3;
                    tiled_offset = tiled_offset<<11;
                    tiled_offset1 = tiled_offset+2048*2;
                    temp4 = 4;
                }
            }

            temp1 = i&0x1F;
            tiled_offset = tiled_offset+64*(temp1);
            tiled_offset1 = tiled_offset1+64*(temp1);
            temp2 = yuv420_width-left-right;
            linear_offset = temp2*(i-top);
            temp3 = ((j+256)>>8)<<8;
            temp3 = temp3-j;
            temp1 = left&0x3F;
            if (temp3 > 192) {
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset+temp1, 64-temp1);
                temp2 = ((left+63)>>6)<<6;
                temp3 = ((yuv420_width-right)>>6)<<6;
                if (temp2 == temp3) {
                    temp2 = yuv420_width-right-(64-temp1);
                }
                memcpy(yuv420_dest+linear_offset+64-temp1, nv12t_src+tiled_offset+2048, 64);
                memcpy(yuv420_dest+linear_offset+128-temp1, nv12t_src+tiled_offset1, 64);
                memcpy(yuv420_dest+linear_offset+192-temp1, nv12t_src+tiled_offset1+2048, 64);
                linear_offset = linear_offset+256-temp1;
            } else if (temp3 > 128) {
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset+2048+temp1, 64-temp1);
                memcpy(yuv420_dest+linear_offset+64-temp1, nv12t_src+tiled_offset1, 64);
                memcpy(yuv420_dest+linear_offset+128-temp1, nv12t_src+tiled_offset1+2048, 64);
                linear_offset = linear_offset+192-temp1;
            } else if (temp3 > 64) {
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset1+temp1, 64-temp1);
                memcpy(yuv420_dest+linear_offset+64-temp1, nv12t_src+tiled_offset1+2048, 64);
                linear_offset = linear_offset+128-temp1;
            } else if (temp3 > 0) {
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset1+2048+temp1, 64-temp1);
                linear_offset = linear_offset+64-temp1;
            }

            tiled_offset = tiled_offset+temp4*2048;
            j = (left>>8)<<8;
            j = j + 256;
            temp2 = yuv420_width-right-256;
            for (; j<=temp2; j=j+256) {
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, 64);
                tiled_offset1 = tiled_offset1+temp4*2048;
                memcpy(yuv420_dest+linear_offset+64, nv12t_src+tiled_offset+2048, 64);
                memcpy(yuv420_dest+linear_offset+128, nv12t_src+tiled_offset1, 64);
                tiled_offset = tiled_offset+temp4*2048;
                memcpy(yuv420_dest+linear_offset+192, nv12t_src+tiled_offset1+2048, 64);
                linear_offset = linear_offset+256;
            }

            tiled_offset1 = tiled_offset1+temp4*2048;
            temp2 = yuv420_width-right-j;
            if (temp2 > 192) {
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, 64);
                memcpy(yuv420_dest+linear_offset+64, nv12t_src+tiled_offset+2048, 64);
                memcpy(yuv420_dest+linear_offset+128, nv12t_src+tiled_offset1, 64);
                memcpy(yuv420_dest+linear_offset+192, nv12t_src+tiled_offset1+2048, temp2-192);
            } else if (temp2 > 128) {
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, 64);
                memcpy(yuv420_dest+linear_offset+64, nv12t_src+tiled_offset+2048, 64);
                memcpy(yuv420_dest+linear_offset+128, nv12t_src+tiled_offset1, temp2-128);
            } else if (temp2 > 64) {
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, 64);
                memcpy(yuv420_dest+linear_offset+64, nv12t_src+tiled_offset+2048, temp2-64);
            } else {
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, temp2);
            }
        }
    } else if (temp1 >= 64) {
        for (i=top; i<(yuv420_height-buttom); i=i+1) {
            j = left;
            tiled_offset = __tile_4x2_read(yuv420_width, yuv420_height, j, i);
            temp2 = ((j+64)>>6)<<6;
            temp2 = temp2-j;
            linear_offset = temp1*(i-top);
            temp4 = j&0x3;
            tiled_offset = tiled_offset+temp4;
            memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, temp2);
            linear_offset = linear_offset+temp2;
            j = j+temp2;
            if ((j+64) <= temp3) {
                tiled_offset = __tile_4x2_read(yuv420_width, yuv420_height, j, i);
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, 64);
                linear_offset = linear_offset+64;
                j = j+64;
            }
            if ((j+64) <= temp3) {
                tiled_offset = __tile_4x2_read(yuv420_width, yuv420_height, j, i);
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, 64);
                linear_offset = linear_offset+64;
                j = j+64;
            }
            if (j < temp3) {
                tiled_offset = __tile_4x2_read(yuv420_width, yuv420_height, j, i);
                temp2 = temp3-j;
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, temp2);
            }
        }
    } else {
        for (i=top; i<(yuv420_height-buttom); i=i+1) {
            linear_offset = temp1*(i-top);
            for (j=left; j<(yuv420_width-right); j=j+2) {
                tiled_offset = __tile_4x2_read(yuv420_width, yuv420_height, j, i);
                temp4 = j&0x3;
                tiled_offset = tiled_offset+temp4;
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, 2);
                linear_offset = linear_offset+2;
            }
        }
    }
}
