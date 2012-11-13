/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, YeJin Cho <cho.yejin@samsung.com>, YoungHwan An <younghwan_.an@samsung.com>
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
#include "mm_player_capture.h"
#include "mm_player_priv.h"

#include <mm_util_imgp.h>

/*---------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS for internal								|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:												|
---------------------------------------------------------------------------*/
static gboolean __mmplayer_video_capture_probe (GstPad *pad, GstBuffer *buffer, gpointer u_data);
static int  __mmplayer_get_video_frame_from_buffer(mm_player_t* player, GstBuffer *buffer);
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
	player->capture_thread_mutex = g_mutex_new();
	if ( ! player->capture_thread_mutex )
	{
		debug_critical("Cannot create capture mutex");
		goto ERROR;
	}

	/* create capture cond */
	player->capture_thread_cond = g_cond_new();
	if ( ! player->capture_thread_cond )
	{
		debug_critical("Cannot create capture cond");
		goto ERROR;
	}

	player->capture_thread_exit = FALSE;

	/* create video capture thread */
	player->capture_thread =
		g_thread_create (__mmplayer_capture_thread, (gpointer)player, TRUE, NULL);
	if ( ! player->capture_thread )
	{
		goto ERROR;
	}

	return MM_ERROR_NONE;

ERROR:
	/* capture thread */
	if ( player->capture_thread_mutex )
		g_mutex_free ( player->capture_thread_mutex );

	if ( player->capture_thread_cond )
		g_cond_free ( player->capture_thread_cond );

	return MM_ERROR_PLAYER_INTERNAL;
}

int
_mmplayer_release_video_capture(mm_player_t* player)
{
	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	/* release capture thread */
	if ( player->capture_thread_cond &&
		 player->capture_thread_mutex &&
		 player->capture_thread )
	{
		g_mutex_lock(player->capture_thread_mutex);
		player->capture_thread_exit = TRUE;
		g_cond_signal( player->capture_thread_cond );
		g_mutex_unlock(player->capture_thread_mutex);

		debug_log("waitting for capture thread exit");
		g_thread_join ( player->capture_thread );
		g_mutex_free ( player->capture_thread_mutex );
		g_cond_free ( player->capture_thread_cond );
		debug_log("capture thread released");
	}

	return MM_ERROR_NONE;
}

int
_mmplayer_do_video_capture(MMHandleType hplayer)
{
	mm_player_t* player = (mm_player_t*) hplayer;
	int ret = MM_ERROR_NONE;
	GstPad *pad = NULL;

	debug_fenter();

	return_val_if_fail(player && player->pipeline, MM_ERROR_PLAYER_NOT_INITIALIZED);

	/* capturing or not */
	if (player->video_capture_cb_probe_id || player->capture.data || player->captured.a[0] || player->captured.a[1])
	{
		debug_warning("capturing... we can't do any more");
		return MM_ERROR_PLAYER_INVALID_STATE;
	}

	/* check if video pipeline is linked or not */
	if (!player->pipeline->videobin || !player->sent_bos)
	{
		debug_warning("not ready to capture");
		return MM_ERROR_PLAYER_INVALID_STATE;
	}

	if (player->state != MM_PLAYER_STATE_PLAYING)
	{
		if (player->state == MM_PLAYER_STATE_PAUSED) // get last buffer from video sink
		{
			GstBuffer *buf = NULL;
			g_object_get(player->pipeline->videobin[MMPLAYER_V_SINK].gst, "last-buffer", &buf, NULL);

			if (buf)
			{
				ret = __mmplayer_get_video_frame_from_buffer(player, buf);
				gst_buffer_unref(buf);
			}
			return ret;
		}
		else
		{
			debug_warning("invalid state(%d) to capture", player->state);
			return MM_ERROR_PLAYER_INVALID_STATE;
		}
	}

	pad = gst_element_get_static_pad(player->pipeline->videobin[MMPLAYER_V_SINK].gst, "sink" );

	/* register probe */
	player->video_capture_cb_probe_id = gst_pad_add_buffer_probe (pad,
		G_CALLBACK (__mmplayer_video_capture_probe), player);

	gst_object_unref(GST_OBJECT(pad));
	pad = NULL;

	debug_fleave();

	return ret;
}

static gpointer
__mmplayer_capture_thread(gpointer data)
{
	mm_player_t* player = (mm_player_t*) data;
	MMMessageParamType msg = {0, };
	unsigned char * linear_y_plane = NULL;
	unsigned char * linear_uv_plane = NULL;

	return_if_fail (player);

	while (!player->capture_thread_exit)
	{
		debug_log("capture thread started. waiting for signal");

		g_mutex_lock(player->capture_thread_mutex);
		g_cond_wait( player->capture_thread_cond, player->capture_thread_mutex );

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
			unsigned char * src_buffer = NULL;

			debug_log("w[0]=%d, w[1]=%d", player->captured.w[0], player->captured.w[1]);
			debug_log("h[0]=%d, h[1]=%d", player->captured.h[0], player->captured.h[1]);
			debug_log("s[0]=%d, s[1]=%d", player->captured.s[0], player->captured.s[1]);
			debug_log("e[0]=%d, e[1]=%d", player->captured.e[0], player->captured.e[1]);
			debug_log("a[0]=%p, a[1]=%p", player->captured.a[0], player->captured.a[1]);

			if (mm_attrs_get_int_by_name(player->attrs, "content_video_width", &(player->captured.w[0])) != MM_ERROR_NONE)
			{
				debug_error("failed to get content width attribute");
				goto ERROR;
			}

			if (mm_attrs_get_int_by_name(player->attrs, "content_video_height", &(player->captured.h[0])) != MM_ERROR_NONE)
			{
				debug_error("failed to get content height attribute");
				goto ERROR;
			}

			linear_y_plane_size = (player->captured.w[0] * player->captured.h[0]);
			linear_uv_plane_size = (player->captured.w[0] * player->captured.h[0]/2);

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
			__csc_tiled_to_linear_crop(linear_y_plane, player->captured.a[0], player->captured.w[0], player->captured.h[0], 0,0,0,0);
			__csc_tiled_to_linear_crop(linear_uv_plane, player->captured.a[1], player->captured.w[0], player->captured.h[0]/2, 0,0,0,0);

			MMPLAYER_FREEIF(player->captured.a[0]);
			MMPLAYER_FREEIF(player->captured.a[1]);

			src_buffer = (unsigned char*) g_try_malloc(linear_y_plane_size+linear_uv_plane_size);

			if (src_buffer == NULL)
			{
				msg.code = MM_ERROR_PLAYER_NO_FREE_SPACE;
				goto ERROR;
			}
			memset(src_buffer, 0x00, sizeof(linear_y_plane_size+linear_uv_plane_size));
			memcpy(src_buffer, linear_y_plane, linear_y_plane_size);
			memcpy(src_buffer+linear_y_plane_size, linear_uv_plane, linear_uv_plane_size);

			/* NV12 linear to RGB888 */
			ret = __mm_player_convert_colorspace(player, src_buffer, MM_UTIL_IMG_FMT_NV12,
				player->captured.w[0], player->captured.h[0], MM_UTIL_IMG_FMT_RGB888);

			if (ret != MM_ERROR_NONE)
			{
				debug_error("failed to convert nv12 linear");
				goto ERROR;
			}
			/* clean */
			MMPLAYER_FREEIF(src_buffer);
			MMPLAYER_FREEIF(linear_y_plane);
			MMPLAYER_FREEIF(linear_uv_plane);
		}

		player->capture.fmt = MM_PLAYER_COLORSPACE_RGB888;
		msg.data = &player->capture;
		msg.size = player->capture.size;

		if (player->cmd >= MMPLAYER_COMMAND_START)
		{
			MMPLAYER_POST_MSG( player, MM_MESSAGE_VIDEO_CAPTURED, &msg );
			debug_log("returned from capture message callback");
		}

		g_mutex_unlock(player->capture_thread_mutex);

		//MMPLAYER_FREEIF(player->capture.data);
		continue;
ERROR:
		if (player->video_cs == MM_PLAYER_COLORSPACE_NV12_TILED)
		{
			/* clean */
			MMPLAYER_FREEIF(linear_y_plane);
			MMPLAYER_FREEIF(linear_uv_plane);
			MMPLAYER_FREEIF(player->captured.a[0]);
			MMPLAYER_FREEIF(player->captured.a[1]);
		}

		msg.union_type = MM_MSG_UNION_CODE;

		g_mutex_unlock(player->capture_thread_mutex);
		MMPLAYER_POST_MSG( player, MM_MESSAGE_VIDEO_NOT_CAPTURED, &msg );
	}
	return NULL;
EXIT:
	g_mutex_unlock(player->capture_thread_mutex);
	return NULL;
}

/**
  * The output is fixed as RGB888
  */
static int
__mmplayer_get_video_frame_from_buffer(mm_player_t* player, GstBuffer *buffer)
{
	gint yplane_size = 0;
	gint uvplane_size = 0;
	gint src_width = 0;
	gint src_height = 0;
	guint32 fourcc = 0;
	GstCaps *caps = NULL;
	GstStructure *structure = NULL;
	mm_util_img_format src_fmt = MM_UTIL_IMG_FMT_YUV420;
	mm_util_img_format dst_fmt = MM_UTIL_IMG_FMT_RGB888; // fixed

	debug_fenter();

	return_val_if_fail ( player, MM_ERROR_PLAYER_NOT_INITIALIZED );
	return_val_if_fail ( buffer, MM_ERROR_INVALID_ARGUMENT );

	/* get fourcc */
	caps = GST_BUFFER_CAPS(buffer);

	return_val_if_fail ( caps, MM_ERROR_INVALID_ARGUMENT );
	debug_log("caps to capture: %s\n", gst_caps_to_string(caps));

	structure = gst_caps_get_structure (caps, 0);

	return_val_if_fail (structure != NULL, MM_ERROR_PLAYER_INTERNAL);

	/* init capture image buffer */
	memset(&player->capture, 0x00, sizeof(MMPlayerVideoCapture));

	gst_structure_get_int (structure, "width", &src_width);
	gst_structure_get_int (structure, "height", &src_height);

	/* check rgb or yuv */
	if (gst_structure_has_name(structure, "video/x-raw-yuv"))
	{
		gst_structure_get_fourcc (structure, "format", &fourcc);

		switch(fourcc)
		{
			/* NV12T */
			case GST_MAKE_FOURCC ('S', 'T', '1', '2'):
			{
				debug_msg ("captured format is ST12\n");

				MMPlayerMPlaneImage *proved = NULL;
				player->video_cs = MM_PLAYER_COLORSPACE_NV12_TILED;

				/* get video frame info from proved buffer */
				proved = (MMPlayerMPlaneImage *)GST_BUFFER_MALLOCDATA(buffer);

				if ( !proved || !proved->a[0] || !proved->a[1] )
					return MM_ERROR_PLAYER_INTERNAL;

				yplane_size = (proved->s[0] * proved->e[0]);
				uvplane_size = (proved->s[1] * proved->e[1]);

				memset(&player->captured, 0x00, sizeof(MMPlayerMPlaneImage));
				memcpy(&player->captured, proved, sizeof(MMPlayerMPlaneImage));

				player->captured.a[0] = g_try_malloc(yplane_size);
				if ( !player->captured.a[0] )
					return MM_ERROR_SOUND_NO_FREE_SPACE;

				player->captured.a[1] = g_try_malloc(uvplane_size);
				if ( !player->captured.a[1] )
					return MM_ERROR_SOUND_NO_FREE_SPACE;

				memcpy(player->captured.a[0], proved->a[0], yplane_size);
				memcpy(player->captured.a[1], proved->a[1], uvplane_size);
				goto DONE;
			}
			break;

			case GST_MAKE_FOURCC ('I', '4', '2', '0'):
			{
				src_fmt = MM_UTIL_IMG_FMT_I420;
			}
			break;

			default:
			{
				goto UNKNOWN;
			}
			break;
		}
	}
	else if (gst_structure_has_name(structure, "video/x-raw-rgb"))
	{
		gint bpp;
		gint depth;
		gint endianess;
		gint blue_mask;
		gboolean bigendian = FALSE;
		gboolean isbluefirst = FALSE;

	     /**
		* The followings will be considered.
		* RGBx, xRGB, BGRx, xBGR
		* RGB888, BGR888
		* RGB565
		*
		*/
		gst_structure_get_int (structure, "bpp", &bpp);
		gst_structure_get_int (structure, "depth", &depth);
		gst_structure_get_int (structure, "endianness", &endianess);
		gst_structure_get_int (structure, "blue_mask", &blue_mask);

		if (endianess == 4321)
			bigendian = TRUE;

		if (blue_mask == -16777216)
			isbluefirst = TRUE;

		switch(bpp)
		{
			case 32:
			{
				switch(depth)
				{
					case 32:
						if (bigendian && isbluefirst)
							src_fmt = MM_UTIL_IMG_FMT_BGRA8888;
					case 24:
						if (bigendian && isbluefirst)
							src_fmt = MM_UTIL_IMG_FMT_BGRX8888;
						break;
					default:
						goto UNKNOWN;
						break;
				}
			}
			break;

			case 24:
			default:
			{
				goto UNKNOWN;
			}
			break;
		}
	}
	else
	{
		goto UNKNOWN;
	}
	__mm_player_convert_colorspace(player, GST_BUFFER_DATA(buffer), src_fmt, src_width, src_height, dst_fmt);

DONE:
	/* do convert colorspace */
	g_cond_signal( player->capture_thread_cond );

	debug_fleave();

	return MM_ERROR_NONE;

UNKNOWN:
	debug_error("unknown format to capture\n");
	return MM_ERROR_PLAYER_INTERNAL;
}

static gboolean
__mmplayer_video_capture_probe (GstPad *pad, GstBuffer *buffer, gpointer u_data)
{
	mm_player_t* player = (mm_player_t*) u_data;
	int ret = MM_ERROR_NONE;

	return_val_if_fail ( buffer, FALSE);
	debug_fenter();

	ret = __mmplayer_get_video_frame_from_buffer(player, buffer);

	if ( ret != MM_ERROR_NONE)
	{
		debug_error("faild to get video frame. %x\n", ret);
		return FALSE;
	}

	/* remove probe to be called at one time */
	if (player->video_capture_cb_probe_id)
	{
		gst_pad_remove_buffer_probe (pad, player->video_capture_cb_probe_id);
		player->video_capture_cb_probe_id = 0;
	}

	debug_fleave();

	return TRUE;
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

	debug_log("width: %d, height: %d to capture, dest size: %d\n", src_w, src_h, dst_size);

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
    int roundup_x, roundup_y;
    int linear_addr0, linear_addr1, bank_addr ;
    int x_addr;
    int trans_addr;

    pixel_x_m1 = x_size -1;
    pixel_y_m1 = y_size -1;

    roundup_x = ((pixel_x_m1 >> 7) + 1);
    roundup_y = ((pixel_x_m1 >> 6) + 1);

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
