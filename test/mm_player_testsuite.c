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
//#define MTRACE;
#include <glib.h>
#include <mm_types.h>
#include <mm_error.h>
#include <mm_message.h>
#include <mm_player.h>
#include <mm_sound.h> // set earphone sound path for dnse
#include <iniparser.h>
#include <mm_ta.h>
#include <mm_player_sndeffect.h>
#include <mm_player_internal.h> 
#include <pthread.h>
#include <mm_util_imgp.h> // video capture

#ifdef MTRACE
#include <unistd.h> //mtrace
#include <stdlib.h> //mtrace
#endif

#include <dlfcn.h>

#if defined(_USE_EFL)
#include <appcore-efl.h>
//#include <Ecore_X.h>
#include <Elementary.h>

#elif defined(_USE_GTK)
#include <gtk/gtk.h>
#endif

#include <mm_session.h>

gboolean quit_pushing;

#define PACKAGE "mm_player_testsuite"

/*===========================================================================================
|																							|
|  LOCAL DEFINITIONS AND DECLARATIONS FOR MODULE											|
|  																							|
========================================================================================== */
/*---------------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS:											|
---------------------------------------------------------------------------*/
#if defined(_USE_GTK_TEMP)
GMainLoop *g_loop;
#endif

char g_file_list[9][256];

#define PCM_DUMP

#ifdef PCM_DUMP
FILE* g_pcm_dump_fp;
#endif
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
#define MAX_STRING_LEN		2048

#ifdef GST_EXT_OVERLAYSINK_WQVGA // sbs:+:081203
#define FULL_WIDTH			240
#define FULL_HEIGHT		180
#define FULL_LCD_WIDTH		240
#define FULL_LCD_HEIGHT	400
#else
#define FULL_WIDTH			320
#define FULL_HEIGHT		240
#define FULL_LCD_WIDTH		320
#define FULL_LCD_HEIGHT	320
#endif

#ifdef _USE_V4L2SINK
#ifndef _MM_PROJECT_VOLANS
#define TES_WIDTH			800
#define TES_HEIGHT			400
#define TES_X				0
#define TES_Y				0
#define TES_VISIBLE		TRUE
#else	// for Volans test
#define TES_WIDTH			400
#define TES_HEIGHT			240
#define TES_X				0
#define TES_Y				0
#define TES_VISIBLE		TRUE
#endif
#endif

#ifdef PCM_DUMP
#define DUMP_PCM_NAME "/opt/test.pcm"
#endif

#define TS_ROTATE_DEGREE		MM_DISPLAY_ROTATION_270

/* macro */
#define MMTESTSUITE_INI_GET_STRING( x_item, x_ini, x_default ) \
do \
{ \
	gchar* str = iniparser_getstring(dict, x_ini, x_default); \
 \
	if ( str &&  \
		( strlen( str ) > 1 ) && \
		( strlen( str ) < 80 ) ) \
	{ \
		strcpy ( x_item, str ); \
	} \
	else \
	{ \
		strcpy ( x_item, x_default ); \
	} \
}while(0)

#define SET_TOGGLE( flag ) flag = ( flag ) ? FALSE : TRUE;

#define	CHECK_RET_SET_ATTRS(x_err_attrs_name)	\
	if ( x_err_attrs_name )	\
		g_print("failed to set %s", (*x_err_attrs_name)); \
                free(x_err_attrs_name);


#define R2VS_TEST_EACH_FILTER_MODE	//hjkim:+: 090309

//#define AUDIO_FILTER_EFFECT // temp for debianize

#define MMTS_SAMPLELIST_INI_DEFAULT_PATH "/opt/etc/mmts_filelist.ini"
#define MMTS_DEFAULT_INI	\
"\
[list] \n\
\n\
sample1 = /opt/media/Sounds and Music/Music/Over the horizon.mp3\n\
\n\
sample2 = /opt/media/Images and videos/My video clips/Helicopter.mp4\n\
\n\
sample3 = \n\
\n\
sample4 = \n\
\n\
sample5 = \n\
\n\
sample6 = \n\
\n\
sample7 = \n\
\n\
sample8 = \n\
\n\
sample9 = \n\
\n\
"
#define DEFAULT_SAMPLE_PATH ""
#define INI_SAMPLE_LIST_MAX 9

/*---------------------------------------------------------------------------
|    LOCAL CONSTANT DEFINITIONS:											|
---------------------------------------------------------------------------*/
enum
{
	CURRENT_STATUS_MAINMENU,
	CURRENT_STATUS_FILENAME,
	CURRENT_STATUS_VOLUME,
	CURRENT_STATUS_POSITION_TIME,
	CURRENT_STATUS_POSITION_PERCENT,
	CURRENT_STATUS_DISPLAYMETHOD,
	CURRENT_STATUS_DISPLAY_VISIBLE,
	CURRENT_STATUS_PLAYCOUNT,
	CURRENT_STATUS_SPEED_PLAYBACK,
	CURRENT_STATUS_SECTION_REPEAT,
#ifdef AUDIO_FILTER_EFFECT
	CURRENT_STATUS_R2VS,
#endif
	CURRENT_STATUS_SUBTITLE_FILENAME,
	CURRENT_STATUS_RESIZE_VIDEO,	
};
/*---------------------------------------------------------------------------
|    LOCAL DATA TYPE DEFINITIONS:											|
---------------------------------------------------------------------------*/

#ifdef _USE_EFL
struct appdata
{
	Evas *evas;
	Ecore_Evas *ee;
	Evas_Object *win;

	Evas_Object *layout_main; /* layout widget based on EDJ */
	Ecore_X_Window xid;

	/* add more variables here */
};

static void win_del(void *data, Evas_Object *obj, void *event)
{
		elm_exit();
}

static Evas_Object* create_win(const char *name)
{
		Evas_Object *eo;
		int w, h;

		printf ("[%s][%d] name=%s\n", __func__, __LINE__, name);

		eo = elm_win_add(NULL, name, ELM_WIN_BASIC);
		if (eo) {
				elm_win_title_set(eo, name);
				elm_win_borderless_set(eo, EINA_TRUE);
				evas_object_smart_callback_add(eo, "delete,request",win_del, NULL);
				ecore_x_window_size_get(ecore_x_window_root_first_get(),&w, &h);
				evas_object_resize(eo, w, h);
		}

		return eo;
}

static int app_create(void *data)
{
		struct appdata *ad = data;
		Evas_Object *win;
		//Evas_Object *ly;
		//int r;

		/* create window */
		win = create_win(PACKAGE);
		if (win == NULL)
				return -1;
		ad->win = win;

#if 0
		/* load edje */
		ly = load_edj(win, EDJ_FILE, GRP_MAIN);
		if (ly == NULL)
				return -1;
		elm_win_resize_object_add(win, ly);
		edje_object_signal_callback_add(elm_layout_edje_get(ly),
						"EXIT", "*", main_quit_cb, NULL);
		ad->ly_main = ly;
		evas_object_show(ly);

		/* init internationalization */
		r = appcore_set_i18n(PACKAGE, LOCALEDIR);
		if (r)
				return -1;
		lang_changed(ad);
#endif
		evas_object_show(win);

#if 0
		/* add system event callback */
		appcore_set_event_callback(APPCORE_EVENT_LANG_CHANGE, lang_changed, ad);
#endif 
		return 0;
}

static int app_terminate(void *data)
{
		struct appdata *ad = data;

		if (ad->win)
				evas_object_del(ad->win);

		return 0;
}



struct appcore_ops ops = {
		.create = app_create,
		.terminate = app_terminate, 	
};

//int r;
struct appdata ad;
#endif


/*---------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS:											|
---------------------------------------------------------------------------*/

#ifdef _USE_XVIMAGESINK
static xid = 0;
#endif

void * overlay = NULL;

int			g_current_state;
int			g_menu_state = CURRENT_STATUS_MAINMENU;
bool			g_bArgPlay = FALSE;
static MMHandleType g_player = 0;
unsigned int		g_video_xid = 0;

int g_audio_dsp = FALSE;
int g_video_dsp = FALSE;

char g_subtitle_uri[MAX_STRING_LEN];
int g_subtitle_width = 800;
int g_subtitle_height = 480;
bool g_subtitle_silent = FALSE;
unsigned int g_subtitle_xid = 0;
char *g_err_name = NULL;

/*---------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:												|
---------------------------------------------------------------------------*/
static void player_play();
gboolean timeout_quit_program(void* data);

void init_file_path(void);
gboolean ts_generate_default_ini(void);
void toggle_audiosink_fadeup();

#ifndef PROTECTOR_VODA_3RD
void TestFileInfo (char* filename);
#endif

bool testsuite_sample_cb(void *stream, int stream_size, void *user_param);

/*===========================================================================================
|																							|
|  FUNCTION DEFINITIONS																		|
|  																							|
========================================================================================== */
#ifdef _USE_XVIMAGESINK //tskim:~:ImplementationFullscreen_090119
void change_fullscreen(GtkWidget* widget);
gboolean softkey_cb_select_and_back(GtkWidget *widget, SoftkeyPosition position, gpointer data);
#endif //tskim:~:ImplementationFullscreen_090119


/*---------------------------------------------------------------------------
  |    LOCAL FUNCTION DEFINITIONS:											|
  ---------------------------------------------------------------------------*/
		gboolean
ts_generate_default_ini(void)
{
	FILE* fp = NULL;
	gchar* default_ini = MMTS_DEFAULT_INI;

	/* create new file */
	fp = fopen(MMTS_SAMPLELIST_INI_DEFAULT_PATH, "wt");

	if ( !fp )
	{
		return FALSE;
	}

	/* writing default ini file */
	if ( strlen(default_ini) != fwrite(default_ini, 1, strlen(default_ini), fp) )
	{
		fclose(fp);
		return FALSE;
	}

	fclose(fp);
	return TRUE;

}

void
init_file_path(void)
{
	dictionary * dict = NULL;

	dict = iniparser_load(MMTS_SAMPLELIST_INI_DEFAULT_PATH);

	if ( !dict )
	{
		printf("No inifile found. player testsuite will create default inifile.\n");
		if ( FALSE == ts_generate_default_ini() )
		{
			printf("Creating default inifile failed. Player testsuite will use default values.\n");
		}
		else
		{
			/* load default ini */
			dict = iniparser_load(MMTS_SAMPLELIST_INI_DEFAULT_PATH);
		}
	}

	if ( dict )
	{
		int idx;

		for ( idx = 1 ; idx <= INI_SAMPLE_LIST_MAX ; idx++ )
		{
			char buf[12];

			sprintf(buf, "list:sample%d", idx);

			MMTESTSUITE_INI_GET_STRING( g_file_list[idx-1], buf, DEFAULT_SAMPLE_PATH );
		}
	}
}


//#define CAPTUERD_IMAGE_SAVE_PATH	"./capture_image"
int	_save(unsigned char * src, int length)
{
	//unlink(CAPTUERD_IMAGE_SAVE_PATH);
	FILE* fp;
	char filename[256] = {0,};
	static int WRITE_COUNT = 0;
	
	//gchar *filename  = CAPTUERD_IMAGE_SAVE_PATH;

	sprintf (filename, "VIDEO_CAPTURE_IMAGE_%d.rgb", WRITE_COUNT);
	WRITE_COUNT++;
			
	fp=fopen(filename, "w+");

	if(fp==NULL)
	{
		printf("file open error!!\n");	
		return;
	}
	else
	{
		printf("open success\n");
		
		if(fwrite(src, length, 1, fp )!=1)
		{
			printf("file write error!!\n");
			fclose(fp);
			return;
		}
		printf("write success(%s)\n", filename);
		fclose(fp);
	}
			
	return TRUE;
}


static bool msg_callback(int message, MMMessageParamType *param, void *user_param)
{
	switch (message) {
		case MM_MESSAGE_ERROR:
			quit_pushing = TRUE;
			g_print("error : code = %x\n", param->code);
			if (param->code == MM_ERROR_PLAYER_CODEC_NOT_FOUND)
				g_print("##  error string = %s\n", param->data);
			//g_print("Got MM_MESSAGE_ERROR, testsuite will be exit\n");
			//quit_program ();							// 090519
			break;

		case MM_MESSAGE_WARNING:
			// g_print("warning : code = %d\n", param->code);
			break;

		case MM_MESSAGE_END_OF_STREAM:
			g_print("end of stream\n");
			mm_player_stop(g_player);		//bw.jang :+:
			//bw.jang :-: MMPlayerUnrealize(g_player);
			//bw.jang :-: MMPlayerDestroy(g_player);
			//bw.jang :-: g_player = 0;

			if (g_bArgPlay == TRUE ) {

				g_timeout_add(100, timeout_quit_program, 0);

//				quit_program();
			}
			break;

		case MM_MESSAGE_STATE_CHANGED:
			g_current_state = param->state.current;
			//bw.jang :=:
			//g_print("current state : %d\n", g_current_state);
			//-->
			switch(g_current_state)
			{
				case MM_PLAYER_STATE_NONE:
					g_print("                                                            ==> [MediaPlayerApp] Player is [NULL]\n");
					break;
				case MM_PLAYER_STATE_READY:
					g_print("                                                            ==> [MediaPlayerApp] Player is [READY]\n");
					break;
				case MM_PLAYER_STATE_PLAYING:
					g_print("                                                            ==> [MediaPlayerApp] Player is [PLAYING]\n");
					break;
				case MM_PLAYER_STATE_PAUSED:
					g_print("                                                            ==> [MediaPlayerApp] Player is [PAUSED]\n");
					break;
			}
			//::
			break;
		case MM_MESSAGE_BEGIN_OF_STREAM:
		{
					g_print("                                                            ==> [MediaPlayerApp] BOS\n");
		}
		break;

		case MM_MESSAGE_RESUMED_BY_REW:
			g_print("resumed by fast rewind duing trick play\n");
			break;
			
		case MM_MESSAGE_VIDEO_CAPTURED:
		{
			/* NOTE : video capture sample 
			 * 1. get original video frame as rgb888 in the case of C110 HW Codec
			 * Othrewise, format is I420. 
			 * 2. resize it as half size
			 * 3. save resized image
			 *
			 * CAUTION : Application should free received buffer from framework. 
			 */
			unsigned char *dst = NULL;
			int src_w;
			int src_h;
			int dst_width;
			int dst_height;
			int dst_size;
			mm_util_img_format img_fmt;

			MMPlayerVideoCapture* capture = (MMPlayerVideoCapture *)param->data;

			mm_player_get_attribute(g_player, 
					&g_err_name, 
					"content_video_width", &src_w,
					"content_video_height", &src_h, 
					NULL);

			dst_width = src_w/2;
			dst_height = src_h/2;

			g_print("video capture src w=%d, h=%d\n", src_w, src_h);			
			g_print("video capture dst w=%d, h=%d\n", dst_width, dst_height);
				
			if (capture->fmt == MM_PLAYER_COLORSPACE_RGB888) //due to ST12 
			{
				img_fmt = MM_UTIL_IMG_FMT_RGB888;
				
				mm_util_get_image_size(img_fmt, dst_width, dst_height, &dst_size);
				
				dst = (unsigned char *)g_malloc(dst_size);
				
				mm_util_resize_image (capture->data, src_w, src_h, img_fmt, dst, &dst_width, &dst_height);

				_save(dst, dst_size);
			}
			else
			{
				_save(capture->data, capture->size);
			}

			if (capture->data)
			{
				g_free(capture->data);
				capture->data = NULL;
			}

			if (dst)
			{
				g_free(dst);
				dst = NULL;
			}
		}
		break;
		
		case MM_MESSAGE_SEEK_COMPLETED:
			g_print("                                                            ==> [MediaPlayerApp] SEEK_COMPLETED\n");
			break;

		case MM_MESSAGE_UPDATE_SUBTITLE:
			break;			

		case MM_MESSAGE_DRM_NOT_AUTHORIZED:
			g_print("Got MM_MESSAGE_DRM_NOT_AUTHORIZED\n");
			quit_program ();
			break;			

		default:
			return FALSE;
	}

	return TRUE;
}

bool
testsuite_audio_cb(void *stream, int stream_size, void *user_param)
{
	#ifdef PCM_DUMP
	if(fwrite((char *)stream, stream_size, 1, g_pcm_dump_fp )!=1)
	{
		printf("file write error!!\n");
		fclose(g_pcm_dump_fp);
		return FALSE;
	}
	printf("write success\n");	
	return TRUE;
	#endif
}

bool
testsuite_video_cb(void *stream, int stream_size, void *user_param, int width, int height)
{
	static int count = 0;
	static int org_stream_size;
	int org_w, org_h;

	if (!count)
	{
		org_stream_size = stream_size;
		mm_player_get_attribute(g_player, &g_err_name,
								"content_video_width", &org_w,
								"content_video_height", &org_h,
								NULL);
		
		g_print("stream_size = %d[w:%d, h:%d]\n", stream_size, width, height);
		g_print("content width = %d, height = %d\n", org_w, org_h);
		count++;
	}

	if (org_stream_size != stream_size)
	{
		mm_player_get_attribute(g_player, &g_err_name,
							"content_video_width", &org_w,
							"content_video_height", &org_h,
							NULL);
		
		g_print("stream_size = %d[w:%d, h:%d]\n", stream_size, width, height);
		g_print("content width = %d, height = %d\n", org_w, org_h);
	}

	return TRUE;
}

static void input_filename(char *filename)
{
	int len = strlen(filename);
	int err;
	int ret = MM_ERROR_NONE;
	MMPlayerAttrsInfo one;

	MMHandleType prop;
	MMHandleType audio_prop, video_prop;
	MMHandleType prof_prop;
	MMHandleType content_prop = 0;
	MMHandleType subtitle_prop = 0;

#ifdef _USE_XVIMAGESINK
	int *val;
#endif

	if ( len < 0 || len > MAX_STRING_LEN )
		return;

	mm_player_unrealize(g_player);
	mm_player_destroy(g_player);
	g_player = 0;

	if ( mm_player_create(&g_player) != MM_ERROR_NONE )
	{
		g_print("player create is failed\n");
	}
	
	mm_player_set_attribute(g_player,
								&g_err_name,
								"subtitle_uri", g_subtitle_uri, strlen(g_subtitle_uri),
								"subtitle_silent", g_subtitle_silent,
								NULL
								);
	strcpy(g_subtitle_uri,"");

#if defined(APPSRC_TEST)
	gchar uri[100];
	gchar *ext;
	gsize file_size;
	GMappedFile *file;
	GError *error = NULL;
	guint8* g_media_mem = NULL;

	ext = filename;

	file = g_mapped_file_new (ext, FALSE, &error);
	file_size = g_mapped_file_get_length (file);
	g_media_mem = (guint8 *) g_mapped_file_get_contents (file);

	g_sprintf(uri, "mem://ext=%s,size=%d", ext ? ext : "", file_size);
	g_print("[uri] = %s\n", uri);

	mm_player_set_attribute(g_player,
								&g_err_name,
								"profile_uri", uri, strlen(uri),
								"profile_user_param", g_media_mem, file_size
								NULL);
#else
	mm_player_set_attribute(g_player,
								&g_err_name,
								"profile_uri", filename, strlen(filename),
								NULL);
#endif /* APPSRC_TEST */

#if defined(_USE_XVIMAGESINK)
	mm_player_set_attribute(g_player,
								&g_err_name,
								"display_overlay", &xid, sizeof(xid),
								NULL);
#elif defined(_USE_V4L2SINK)
	mm_player_set_attribute(g_player,
								&g_err_name,
								"display_width", TES_WIDTH,
								"display_height", TES_HEIGHT,
								"display_x", TES_X,
								"display_y", TES_Y,
								"display_rotation", TS_ROTATE_DEGREE,
								"display_visible", TES_VISIBLE,
								NULL);
#elif defined(_USE_EFL)
	mm_player_set_attribute(g_player,
								&g_err_name,
								"display_overlay", (void*)&ad.xid, sizeof(ad.xid),
								"display_rotation", TS_ROTATE_DEGREE,
								NULL);
#endif /* _USE_XVIMAGESINK */

	mm_player_set_message_callback(g_player, msg_callback, (void*)g_player);

	if ( mm_player_realize(g_player) != MM_ERROR_NONE )
	{
		g_print("realize is failed\n");
	}
	/* wait until realized */
	//bw.jang :-:
	//while (g_current_state != MM_PLAYER_STATE_READY);

	//if (g_bArgPlay)
	//	player_play();
}

static void input_subtitle_filename(char *subtitle_filename)
{
	int len = strlen(subtitle_filename);

	if ( len < 1 || len > MAX_STRING_LEN )
		return;

	strcpy (g_subtitle_uri, subtitle_filename);

	g_print("subtitle uri is set to %s\n", g_subtitle_uri);
}

static void toggle_subtitle_silent(bool silent)
{
	if ( mm_player_set_subtitle_silent(g_player, silent) != MM_ERROR_NONE )
	{
		g_print("failed to set subtitle silent\n");
	}
}



static void set_volume(MMPlayerVolumeType *pvolume)
{
	if ( mm_player_set_volume(g_player, pvolume) != MM_ERROR_NONE )
	{
		g_print("failed to set volume\n");
	}
}


static void get_volume(MMPlayerVolumeType* pvolume)
{
	int i;

	mm_player_get_volume(g_player, pvolume);

	for (i = 0; i < MM_VOLUME_CHANNEL_NUM; i++)
	{
		g_print("                                                            ==> [MediaPlayerApp] channel [%d] = %f\n", i, pvolume->level[i]);
	}
}

#ifdef MTRACE
static gboolean
progress_timer_cb(gpointer u_data) // @
{
	int format = MM_PLAYER_POS_FORMAT_TIME;
	int position = 0;
	int duration = 0;
	MMHandleType content_prop = 0;

	mm_player_get_position(g_player, format, &position);

	if (position >= 10000)
	{
		char str[50];
		pid_t pid = getpid();
		sprintf(str, "memps -t %d", pid);
		g_print ("hyunil pos = [%d], pid = [%d], str=[%s] \n", position, pid,str);
		system (str);
		muntrace();


		return FALSE;
	}

	return TRUE;
}
#endif

static void player_play()
{
	int bRet = FALSE;

	bRet = mm_player_start(g_player);
#ifdef MTRACE
	g_timeout_add( 500,  progress_timer_cb, g_player );
#endif
}

static void player_capture()
{	
	if(mm_player_do_video_capture(g_player) != MM_ERROR_NONE)
	{
		printf("failed to capture\n");
	}
}

static void player_stop()
{
	int bRet = FALSE;

	bRet = mm_player_stop(g_player);
}

static void player_resume()
{
	int bRet = FALSE;

	bRet = mm_player_resume(g_player);
}

static void player_pause()
{
	int bRet = FALSE;

	bRet = mm_player_pause(g_player);
}


static void player_rotate()
{
	static int degree = 0;
	degree++;

	if (degree == 4) degree = 0;
	
	mm_player_set_attribute(g_player, &g_err_name, "display_rotation", degree, NULL);
}
	

#ifdef MTRACE
//	usleep (1000000);
//	g_print ("aaaaa\n");
//	g_timeout_add( 500,  progress_timer_cb, g_player );
#endif

static void get_position()
{
	int format = MM_PLAYER_POS_FORMAT_TIME;
	int position = 0;
	int duration = 0;
	MMHandleType content_prop = 0;

	mm_player_get_position(g_player, format, &position);

	mm_player_get_attribute(g_player, &g_err_name, "content_duration", &duration, NULL);

	g_print("                                                            ==> [MediaPlayerApp] Pos: [%d / %d] msec\n", position, duration);
}

static void set_position(int position, int format)
{
	if ( mm_player_set_position(g_player, format, position) != MM_ERROR_NONE )
	{
		g_print("failed to set position\n");
	}
}

static void set_display_method(int option)
{
	mm_player_set_attribute(g_player, &g_err_name, "display_method", option, NULL);
}

static void set_display_visible(int option)
{
	mm_player_set_attribute(g_player, &g_err_name, "display_visible", option, NULL);
}

static void resize_video(int option)
{
	int dst_width;
	int dst_height;
	
	switch (option)
	{
		case 1: //qcif
			dst_width = 176;
			dst_height = 144;
			break;
			
		case 2: //qvga
			dst_width = 320;
			dst_height = 240;		
			break;
			
		default:
			break;
	}
	mm_player_set_attribute(g_player, &g_err_name, "display_width", dst_width, "display_height", dst_height, NULL);
}

void set_audio_callback()
{
	int start_pos = 0;
	int end_pos = 0;
	int samplerate = 0; // Hz

	g_print("start pos?\n");
	scanf("%d", &start_pos);
	
	g_print("end pos?\n");
	scanf("%d", &end_pos);
	
	g_print("samplerate?\n");
	scanf("%d", &samplerate);	

	mm_player_set_attribute(g_player,
								&g_err_name,
								"pcm_extraction", TRUE,
								"pcm_extraction_start_msec", start_pos,
								"pcm_extraction_end_msec", end_pos,
								"pcm_extraction_samplerate", samplerate,
								NULL);
	
	mm_player_set_audio_stream_callback(g_player, testsuite_audio_cb, NULL);	
}

void set_video_callback()
{
	 mm_player_set_video_stream_callback(g_player, testsuite_video_cb, NULL);
}
#ifdef AUDIO_FILTER_EFFECT
static int set_r2vs(char* char_mode)
{
	int idx_filter = 0, ret = 0;

	MMAudioFilterInfo filter_info;
	memset(&filter_info, 0, sizeof(filter_info));

	int mode = MM_AUDIO_FILTER_NONE, r2vs_mode = 0;
	int len = strlen(char_mode);
	char tmp_mode[MAX_STRING_LEN];
	strncpy(tmp_mode, char_mode, len);

	//g_print("set_r2vs - char_mode[%d][%s][%s]--------------------\n", len, char_mode, tmp_mode);

	filter_info.output_mode = MM_AUDIO_FILTER_OUTPUT_SPK;

	for(idx_filter = 0; idx_filter < len; idx_filter++)
	{
		//g_print("tmp_mode[%d]=[%c]\n", idx_filter, tmp_mode[idx_filter]);

		if(strncmp(tmp_mode+idx_filter, "0", 1 ) == 0)
		{
			mode = MM_AUDIO_FILTER_NONE;
			r2vs_mode = 0;
		}
#ifndef R2VS_TEST_EACH_FILTER_MODE
		else if(strncmp(tmp_mode+idx_filter, "1", 1 ) == 0)
		{
			mode = mode | MM_AUDIO_FILTER_3D;
			r2vs_mode = 3;
		}
		else if(strncmp(tmp_mode+idx_filter, "2", 1 ) == 0)
		{
			mode = mode | MM_AUDIO_FILTER_EQUALIZER;
			r2vs_mode = 3;
		}
		else if(strncmp(tmp_mode+idx_filter, "3", 1 ) == 0)
		{
			mode = mode | MM_AUDIO_FILTER_REVERB;
			r2vs_mode = 3;
		}
#else
		else if(strncmp(tmp_mode+idx_filter, "1", 1 ) == 0)
		{
			mode = mode | MM_AUDIO_FILTER_3D;
			idx_filter++;
			g_print("3D filter mode = [%c]\n", tmp_mode[idx_filter]);
			if (strncmp(tmp_mode+idx_filter, "1", 1 ) == 0)
				filter_info.sound_3d.mode = MM_3DSOUND_WIDE;
			else if (strncmp(tmp_mode+idx_filter, "2", 1 ) == 0)
				filter_info.sound_3d.mode = MM_3DSOUND_DYNAMIC;
			else if (strncmp(tmp_mode+idx_filter, "3", 1 ) == 0)
				filter_info.sound_3d.mode = MM_3DSOUND_SURROUND;
			else
				filter_info.sound_3d.mode = MM_3DSOUND_WIDE;

			r2vs_mode = 3;
		}
		else if(strncmp(tmp_mode+idx_filter, "2", 1 ) == 0)
		{
			mode = mode | MM_AUDIO_FILTER_EQUALIZER;
			idx_filter++;
			g_print("EQ filter mode = [%c]\n", tmp_mode[idx_filter]);
			if (strncmp(tmp_mode+idx_filter, "1", 1 ) == 0)
				filter_info.equalizer.mode = MM_EQ_ROCK;
			else if (strncmp(tmp_mode+idx_filter, "2", 1 ) == 0)
				filter_info.equalizer.mode = MM_EQ_JAZZ;
			else if (strncmp(tmp_mode+idx_filter, "3", 1 ) == 0)
				filter_info.equalizer.mode = MM_EQ_LIVE;
			else if (strncmp(tmp_mode+idx_filter, "4", 1 ) == 0)
				filter_info.equalizer.mode = MM_EQ_CLASSIC;
			else if (strncmp(tmp_mode+idx_filter, "5", 1 ) == 0)
				filter_info.equalizer.mode = MM_EQ_FULL_BASS;
			else if (strncmp(tmp_mode+idx_filter, "6", 1 ) == 0)
				filter_info.equalizer.mode = MM_EQ_FULL_BASS_AND_TREBLE;
			else if (strncmp(tmp_mode+idx_filter, "7", 1 ) == 0)
				filter_info.equalizer.mode = MM_EQ_DANCE;
			else if (strncmp(tmp_mode+idx_filter, "8", 1 ) == 0)
				filter_info.equalizer.mode = MM_EQ_POP;
			else if (strncmp(tmp_mode+idx_filter, "9", 1 ) == 0)
				filter_info.equalizer.mode = MM_EQ_FULL_TREBLE;
			else
				filter_info.equalizer.mode = MM_EQ_ROCK;

			r2vs_mode = 3;
		}
		else if(strncmp(tmp_mode+idx_filter, "3", 1 ) == 0)
		{
			mode = mode | MM_AUDIO_FILTER_REVERB;
			idx_filter++;
			g_print("Reberb filter mode = [%c]\n", tmp_mode[idx_filter]);
			if (strncmp(tmp_mode+idx_filter, "1", 1 ) == 0)
				filter_info.reverb.mode = MM_REVERB_JAZZ_CLUB;
			else if (strncmp(tmp_mode+idx_filter, "2", 1 ) == 0)
				filter_info.reverb.mode = MM_REVERB_CONCERT_HALL;
			else if (strncmp(tmp_mode+idx_filter, "3", 1 ) == 0)
				filter_info.reverb.mode = MM_REVERB_STADIUM;
			else
				filter_info.reverb.mode = MM_REVERB_JAZZ_CLUB;

			r2vs_mode = 3;
		}
#endif
		else if(strncmp(tmp_mode+idx_filter, "4", 1 ) == 0)
		{
			mode = MM_AUDIO_FILTER_BE;
			r2vs_mode = 2;
		}
		else if(strncmp(tmp_mode+idx_filter, "5", 1 ) == 0)
		{
			mode = mode |MM_AUDIO_FILTER_SV;
			r2vs_mode = 3;
		}
//hjkim:+:090203, Add for SPK mode test
		else if(strncmp(tmp_mode+idx_filter, "6", 1 ) == 0)
		{
			filter_info.output_mode = MM_AUDIO_FILTER_OUTPUT_EAR;
		}
		else if(strncmp(tmp_mode+idx_filter, "7", 1 ) == 0 )
		{
			mode = MM_AUDIO_FILTER_MTMV;
			r2vs_mode = 4;
		}
		else if(strncmp(tmp_mode+idx_filter, "8", 1 ) == 0 )
		{
			mode = MM_AUDIO_FILTER_SRSCSHP;
			r2vs_mode = 5;
		}
		else if(strncmp(tmp_mode+idx_filter, "9", 1 ) == 0 )
		{
			mode = MM_AUDIO_FILTER_ARKAMYS;
			r2vs_mode = 6;
		}
		else if(strncmp(tmp_mode+idx_filter, "a", 1 ) == 0 )
		{
			mode = MM_AUDIO_FILTER_WOWHD;
			r2vs_mode = 7;
		}
		else if(strncmp(tmp_mode+idx_filter, "b", 1 ) == 0 )
		{
			mode = MM_AUDIO_FILTER_SOUND_EX;
			r2vs_mode = 8;
		}
		else
			return -1;
	}
	//g_print("set_r2vs - r2vs_mode[%d]--------------------\n", r2vs_mode);

	if(r2vs_mode == 0)
	{
		filter_info.filter_type = MM_AUDIO_FILTER_NONE;
		//g_print("Off All R2VS Effect--------------------[%d]\n", filter_info.filter_type);
	}
	else if(r2vs_mode == 1)
	{
		filter_info.filter_type = MM_AUDIO_FILTER_3D | MM_AUDIO_FILTER_EQUALIZER |MM_AUDIO_FILTER_REVERB | MM_AUDIO_FILTER_SV;
		filter_info.sound_3d.mode = MM_3DSOUND_WIDE;
		filter_info.equalizer.mode = MM_EQ_ROCK;
		filter_info.reverb.mode = MM_REVERB_JAZZ_CLUB;
		//g_print("Set 3D Sound, Equalizer, Reverb All--------------------[%d][%d][%d][%d]\n", filter_info.filter_type, filter_info.sound_3d.mode, filter_info.equalizer.mode, filter_info.reverb.mode);
	}
	else if(r2vs_mode == 2)
	{
		filter_info.filter_type = MM_AUDIO_FILTER_BE;
		//g_print("Set Base Enhacement Only--------------------[%d]\n", filter_info.filter_type);
	}
	else if(r2vs_mode == 3)
	{
		filter_info.filter_type = mode;
		//g_print("r2vs_mode == 3--------------------[%d]\n", filter_info.filter_type);
#ifndef R2VS_TEST_EACH_FILTER_MODE
		if(mode & MM_AUDIO_FILTER_3D)
			filter_info.sound_3d.mode = MM_3DSOUND_WIDE;
		if(mode & MM_AUDIO_FILTER_EQUALIZER)
			filter_info.equalizer.mode = MM_EQ_ROCK;
		if(mode & MM_AUDIO_FILTER_REVERB)
			filter_info.reverb.mode = MM_REVERB_JAZZ_CLUB;
#endif
	}
	else if (r2vs_mode == 4)
	{
		filter_info.filter_type = MM_AUDIO_FILTER_MTMV;
	}
	else if (r2vs_mode == 5)
	{
		filter_info.filter_type = MM_AUDIO_FILTER_SRSCSHP;
	}
	else if (r2vs_mode == 6)
	{
		filter_info.filter_type = MM_AUDIO_FILTER_ARKAMYS;
	}
	else if (r2vs_mode == 7)
	{
		filter_info.filter_type = MM_AUDIO_FILTER_WOWHD;
	}
	else if (r2vs_mode == 8)
	{
		filter_info.filter_type = MM_AUDIO_FILTER_SOUND_EX;
	}
	else
	{
		g_print("R2VS Input Mode Error [%d]\n", r2vs_mode);
		return -1;
	}

	ret = mm_player_apply_sound_filter(g_player,  &filter_info);	//hjkim, MM_AUDIO_FILTER_NONE is for temporary

	if(ret < 0)
	{
		g_print("failed update R2VS\n");
		return -1;
	}
	else
	{
		//g_print("Success update R2VS\n");
	}

	return 0;
}
#endif

static void print_info()
{
#if 0
	MMHandleType tag_prop = 0, content_prop = 0;

#if 1 // hcjeon:-:enable it later. // enabled!!!:+:100112
	/* set player configuration */
	MMPlayerGetAttrs(g_player, MM_PLAYER_ATTRS_TAG, &tag_prop);
	MMPlayerGetAttrs(g_player, MM_PLAYER_ATTRS_CONTENT, &content_prop);

	if (tag_prop == 0)
		return;

	char* tmp_string = NULL;
	int tmp_string_len = 0;
	int tmp_int = 0;

	printf( "====================================================================================\n" );
	MMAttrsGetString (tag_prop, MM_PLAYER_TAG_ARTIST, &tmp_string, &tmp_string_len);
	printf("artist: %s\n", tmp_string);

	MMAttrsGetString (tag_prop, MM_PLAYER_TAG_TITLE, &tmp_string, &tmp_string_len);
	printf("title: %s\n", tmp_string);

	MMAttrsGetString (tag_prop, MM_PLAYER_TAG_ALBUM, &tmp_string, &tmp_string_len);
	printf("album: %s\n", tmp_string);

	MMAttrsGetString (tag_prop, MM_PLAYER_TAG_GENRE, &tmp_string, &tmp_string_len);
	printf("genre: %s\n", tmp_string);


	// printf("author: %s\n", mmf_attrs_get_string(tag_prop, MM_FILE_TAG_AUTHOR));

	MMAttrsGetString (tag_prop, MM_PLAYER_TAG_COPYRIGHT, &tmp_string, &tmp_string_len);
	printf("copyright: %s\n", tmp_string);

	MMAttrsGetString (tag_prop, MM_PLAYER_TAG_DATE, &tmp_string, &tmp_string_len);
	printf("date: %s\n", tmp_string);

	MMAttrsGetString (tag_prop, MM_PLAYER_TAG_DESCRIPTION, &tmp_string, &tmp_string_len);
	printf("description: %s\n", tmp_string);

	MMAttrsGetInt (tag_prop, MM_PLAYER_TAG_TRACK_NUM,&tmp_int);
	printf("track num: %d\n", tmp_int);



	MMAttrsGetString (content_prop, MM_PLAYER_CONTENT_VIDEO_CODEC, &tmp_string, &tmp_string_len);
	printf("video codec: %s\n", tmp_string);

	MMAttrsGetInt (content_prop, MM_PLAYER_CONTENT_AUDIO_TRACK_NUM,&tmp_int);
	printf("audio_track_num: %d\n", tmp_int);
	if ( tmp_int > 0 )
    {
		printf( "------------------------------------------------------------------------------------\n" );
		MMAttrsGetString (content_prop, MM_PLAYER_CONTENT_AUDIO_CODEC, &tmp_string, &tmp_string_len);
		printf("audio codec: %s\n", tmp_string);

		MMAttrsGetInt (content_prop, MM_PLAYER_CONTENT_AUDIO_SAMPLERATE,&tmp_int);
		printf("audio samplerate: %d\n", tmp_int);

		MMAttrsGetInt (content_prop, MM_PLAYER_CONTENT_AUDIO_BITRATE,&tmp_int);
		printf("audio bitrate: %d\n", tmp_int);

		MMAttrsGetInt (content_prop, MM_PLAYER_CONTENT_AUDIO_CHANNELS,&tmp_int);
		printf("audio channel: %d\n", tmp_int);

		MMAttrsGetInt (content_prop, MM_PLAYER_CONTENT_AUDIO_TRACK_ID,&tmp_int);
		printf("audio track id: %d\n", tmp_int);
		printf( "------------------------------------------------------------------------------------\n" );
	}


	MMAttrsGetInt (content_prop, MM_PLAYER_CONTENT_VIDEO_TRACK_NUM,&tmp_int);
	printf("video_track_num: %d\n", tmp_int);
	if ( tmp_int > 0 )
	{
		printf( "------------------------------------------------------------------------------------\n" );
		MMAttrsGetString (content_prop, MM_PLAYER_CONTENT_VIDEO_CODEC, &tmp_string, &tmp_string_len);
		printf("video codec: %s\n", tmp_string);

		MMAttrsGetInt (content_prop, MM_PLAYER_CONTENT_VIDEO_WIDTH,&tmp_int);
		printf("video width: %d\n", tmp_int);

		MMAttrsGetInt (content_prop, MM_PLAYER_CONTENT_VIDEO_HEIGHT,&tmp_int);
		printf("video height: %d\n", tmp_int);

		MMAttrsGetInt (content_prop, MM_PLAYER_CONTENT_VIDEO_FPS,&tmp_int);
		printf("video fps: %d\n", tmp_int);

		MMAttrsGetInt (content_prop, MM_PLAYER_CONTENT_VIDEO_TRACK_ID,&tmp_int);
		printf("video track id: %d\n", tmp_int);
		printf( "------------------------------------------------------------------------------------\n" );
	}
	printf( "====================================================================================\n" );
	printf("Time analysis...\n");

#endif
	//MMTA_ACUM_ITEM_SHOW_RESULT();
#endif
}

void quit_program()
{
	MMTA_ACUM_ITEM_SHOW_RESULT_TO(MMTA_SHOW_STDOUT);
	MMTA_RELEASE();

	mm_player_unrealize(g_player);
	mm_player_destroy(g_player);
	g_player = 0;

#ifdef _USE_GTK_TEMP
	g_main_loop_quit(g_loop);
#endif

#ifdef _USE_EFL
	elm_exit();
#endif
}

void destroy_player()
{
        mm_player_unrealize(g_player);
        mm_player_destroy(g_player);
        g_player = 0;
	g_print("player is destroyed.\n");
}

void display_sub_additional()
{

}

void display_sub_basic()
{
	int idx;

	g_print("\n");
	g_print("=========================================================================================\n");
	g_print("                          MM-PLAYER Testsuite (press q to quit) \n");
	g_print("-----------------------------------------------------------------------------------------\n");
	g_print("*. Sample List in [%s]\n", MMTS_SAMPLELIST_INI_DEFAULT_PATH);

	for( idx = 1; idx <= INI_SAMPLE_LIST_MAX ; idx++ )
	{
		if (strlen (g_file_list[idx-1]) > 0)
			g_print("%d. Play [%s]\n", idx, g_file_list[idx-1]);
	}

	g_print("-----------------------------------------------------------------------------------------\n");
	g_print("[playback] a. Initialize Media\t");
	g_print("b. Play  \t");
	g_print("c. Stop  \t");
	g_print("d. Resume\t");
	g_print("e. Pause \n");

	g_print("[ volume ] f. Set Volume\t");
	g_print("g. Get Volume\n");

	g_print("[position] h. Set Position (T)\t");
	g_print("i. Get Position\t");
	g_print("r. Set Position (%%)\n");

	g_print("[ preset ] o. Toggle Audio DSP\t");
	g_print("p. Toggle Video DSP\t");
	g_print("m. Set Play Count\n");

	g_print("[ option ] sp. Speed Playback \t");
	g_print("sr. Togle Section Play\n");
	g_print("[display ]x. Change display geometry method\t");
	g_print("dv. display visible\t");
	g_print("rv. resize video - fimcconvert only\n");	

	g_print("[ sound  ] k. Toggle Fadeup\t");
	g_print("z. Apply DNSE   \t");



#if 0	// not available now
  	g_print("        Progressive Download\n");
  	g_print("----------------------\n");
	g_print("!. set Progressive Download enable \n");
	g_print("@. set full-contents-size  \n");
	g_print("#. set Download Complete() \n");
	g_print("----------------------\n");
#endif

	g_print("[subtitle] $. Set subtitle uri\t");
	g_print("%%. Toggle subtitle silent  \n");

	g_print("[   etc  ] j. Information\t");
	g_print("l. Buffering Mode\n");
	g_print("[callback] ac. audio stream callback(segment extraction)\t");
	g_print("vc. video stream callback\n");	
	
	g_print("[  video ] ca. capture\t");
	g_print("ro. rotate\n");		
	g_print("\n");
	g_print("=========================================================================================\n");
}

void display_sub_dnse()
{
	g_print("*** input DNSE mode - Speaker Mode\n");
	g_print("Choose Effect You Want\n");
//		g_print("1,2,3 can be mixed together (ex 13 or 123)\n");
	g_print("1X,2X,3X,5,6 can be mixed together (ex 1132 or 112537 or 1256)\n");
	g_print("---------------------------------\n");
	g_print("0. No DNSE Effect\n");
#ifndef R2VS_TEST_EACH_FILTER_MODE
	g_print("1. 3D Sound\n");
	g_print("2. Equalizer\n");
	g_print("3. Reverb\n");
#else
	g_print("3D Sound-------------------------\n");
	g_print("11. 3D Sound -Wide\n");
	g_print("12. 3D Sound - Dynamic\n");
	g_print("13. 3D Sound - Surround\n");
	g_print("Equalizer-------------------------\n");
	g_print("21. Equalizer - Rock\n");
	g_print("22. Equalizer - Jazz\n");
	g_print("23. Equalizer - Live\n");
	g_print("24. Equalizer - Classic \n");
	g_print("25. Equalizer - Full Bass\n");
	g_print("26. Equalizer - Full Bass and Treble\n");
	g_print("27. Equalizer - Dance\n");
	g_print("28. Equalizer - Pop\n");
	g_print("29. Equalizer - Full Treble\n");
	g_print("Reverb-------------------------\n");
	g_print("31. Reverb - Jazz Club\n");
	g_print("32. Reverb - Concert Hall\n");
	g_print("33. Reverb - Stadium\n");
	g_print("---------------------------------\n");
#endif
	g_print("4. Base Enhancement\n");
	g_print("5. Spectrum View\n");	//hjkim:+:090203, Add Specturm view value check menu
	g_print("6. Speaker Mode On (spk default)\n");	//hjkim:+:090203
	g_print("7. mTheater Movie\n");
    g_print("8. SRS CSHeadPhone\n");
    g_print("9. Arkamys\n");
	g_print("a. WOW HD\n");
	g_print("b. Sound Externalization\n");
}
static void displaymenu()
{
	if (g_menu_state == CURRENT_STATUS_MAINMENU)
	{
		display_sub_basic();
	}
	else if (g_menu_state == CURRENT_STATUS_FILENAME)
	{
		g_print("*** input mediapath.\n");
	}
	else if (g_menu_state == CURRENT_STATUS_VOLUME)
	{
		g_print("*** input volume value.\n");
	}
	else if (g_menu_state == CURRENT_STATUS_PLAYCOUNT)
	{
		g_print("*** input count num.\n");
	}
	else if (g_menu_state == CURRENT_STATUS_POSITION_TIME)
	{
		g_print("*** input position value(msec)\n");
	}
	else if (g_menu_state == CURRENT_STATUS_POSITION_PERCENT)
	{
		g_print("*** input position percent(%%)\n");
	}

	else if ( g_menu_state == CURRENT_STATUS_DISPLAYMETHOD)
	{
		g_print("*** input option(0,1,2)\n");
	}
	else if ( g_menu_state == CURRENT_STATUS_DISPLAY_VISIBLE)
	{
		g_print("*** input value(0,1)\n");
	}
	else if ( g_menu_state == CURRENT_STATUS_RESIZE_VIDEO)
	{
		g_print("*** input one (1:QCIF, 2:QVGA)\n");
	}
	else if (g_menu_state == CURRENT_STATUS_SPEED_PLAYBACK)
	{
		g_print("*** input playback rate.\n");
	}
	else if (g_menu_state == CURRENT_STATUS_PLAYCOUNT)
	{
#ifdef AUDIO_FILTER_EFFECT
	}
	else if (g_menu_state == CURRENT_STATUS_R2VS)
	{
		display_sub_dnse();
#endif
	}
	else if (g_menu_state == CURRENT_STATUS_SUBTITLE_FILENAME)
	{
		g_print(" ** input  subtitle file path.\n");
	}
	else
	{
		g_print("*** unknown status.\n");
		quit_program();
	}
	g_print(" >>> ");
}

gboolean timeout_menu_display(void* data)
{
	displaymenu();
	return FALSE;
}

gboolean timeout_quit_program(void* data)
{
	quit_program();
	return FALSE;
}

void set_playcount(int count)
{
	mm_player_set_attribute(g_player, &g_err_name, "profile_play_count", count, NULL);
}

void toggle_audiosink_fadeup()
{
	static gboolean flag_fadeup = FALSE;

	SET_TOGGLE(flag_fadeup);

	g_print("fadeup value to set : %d\n", flag_fadeup);

	mm_player_set_attribute(g_player, &g_err_name, "sound_fadeup", flag_fadeup, NULL);

}

void toggle_sound_spk_out_only()
{
	static gboolean flag_spk_out = FALSE;

	SET_TOGGLE(flag_spk_out);

	g_print("flag_spk_out value to set : %d\n", flag_spk_out);

	mm_player_set_attribute(g_player, &g_err_name, "sound_spk_out_only", flag_spk_out, NULL);

}

void toggle_section_repeat(void)
{
	static gboolean flag_sr = FALSE;
	int pos;
	int offset = 4000;

	SET_TOGGLE(flag_sr);

	if ( flag_sr )
	{
		g_print("section repeat activated\n");

		mm_player_get_position(g_player, MM_PLAYER_POS_FORMAT_TIME, &pos);

		mm_player_activate_section_repeat(g_player, pos, pos+offset);
	}
	else
	{
		g_print("section repeat deactivated\n");

		mm_player_deactivate_section_repeat(g_player);
	}
}

void reset_menu_state(void)
{
	g_menu_state = CURRENT_STATUS_MAINMENU;
}


void play_with_ini(char *file_path)
		{
	input_filename(file_path);
				player_play();
}

gboolean start_pushing_buffers;
guint64 offset;
guint size_to_be_pushed;

gboolean seek_flag;

char filename_push[256];

void enough_data(void *player)
{
	g_print("__enough_data\n");
	start_pushing_buffers = FALSE;

}

void feed_data(guint size, void *player)
{
	g_print("__feed_data:%d\n", size);
	start_pushing_buffers = TRUE;
	size_to_be_pushed = size;
}
void seek_data(guint64 _offset, void *player)
{
	g_print("__seek_data:%lld\n", _offset);
	start_pushing_buffers = TRUE;
	offset=_offset;
	seek_flag=TRUE;
}

void push_buffer()
{
	FILE *fp=fopen(filename_push, "rb");
	unsigned char *buf=NULL;
	int size;
	guint64 read_position=0;
	//int err;
	g_print("filename:%s\n", filename_push);
	if(fp==NULL)
	{
		g_print("not a valid filename\n");
		quit_program();
		return;
	}

	seek_flag=FALSE;

	while(!quit_pushing)
	{
		if(!start_pushing_buffers)
		{
			usleep(10000);
			continue;
		}

		//g_print("trying to push\n");
            if (read_position != offset && seek_flag==TRUE)
		{
	            guint64 res;

	            res = fseek (fp, offset, SEEK_SET);

	            read_position = offset;

			g_print("seeking to %lld\n", offset);
            }

		seek_flag=FALSE;

		if(size_to_be_pushed)
			size = size_to_be_pushed;
		else
			size = 4096;

		buf = (unsigned char *)malloc(size);
		if(buf==NULL)
		{
			g_print("mem alloc failed\n");
			break;
		}
		size  = fread(buf, 1, size, fp);

		if(size<=0)
		{
			g_print("EOS\n");
			start_pushing_buffers=FALSE;
			mm_player_push_buffer((MMHandleType)g_player, buf, size);
			break;
//			continue;
		}
		//g_print("pushing:%d\n", size);
		read_position += size;
		size_to_be_pushed=0;
		mm_player_push_buffer((MMHandleType)g_player, buf, size);
		//usleep(10000);
	}
	fclose(fp);

}

void _interpret_main_menu(char *cmd)
{
	if ( strlen(cmd) == 1 )
	{
		if (strncmp(cmd, "a", 1) == 0)
		{
			g_menu_state = CURRENT_STATUS_FILENAME;
		}
		else if (strncmp(cmd, "l", 1) == 0)
		{
			//play_with_ini(g_file_list[0]);
				input_filename("buff://");
				mm_player_set_buffer_need_data_callback(g_player, (mm_player_buffer_need_data_callback )(feed_data), NULL);
				mm_player_set_buffer_seek_data_callback(g_player, (mm_player_buffer_seek_data_callback )(seek_data), NULL);
				mm_player_set_buffer_enough_data_callback(g_player, (mm_player_buffer_enough_data_callback )(enough_data), NULL);
				pthread_t tid1;
				printf("provide the filename:");
				scanf("%s", filename_push);

				pthread_create(&tid1,NULL,push_buffer,NULL);

			//	sleep(2);

				player_play();
		}
		else if (strncmp(cmd, "1", 1) == 0)
		{
			play_with_ini(g_file_list[0]);
		}
		else if (strncmp(cmd, "2", 1) == 0)
		{
			play_with_ini(g_file_list[1]);
		}
		else if (strncmp(cmd, "3", 1) == 0)
		{
			play_with_ini(g_file_list[2]);
		}
		else if (strncmp(cmd, "4", 1) == 0)
		{
			play_with_ini(g_file_list[3]);
		}
		else if (strncmp(cmd, "5", 1) == 0)
		{
			play_with_ini(g_file_list[4]);
		}
		else if (strncmp(cmd, "6", 1) == 0)
		{
			play_with_ini(g_file_list[5]);
		}
		else if (strncmp(cmd, "7", 1) == 0)
		{
			play_with_ini(g_file_list[6]);
		}
		else if (strncmp(cmd, "8", 1) == 0)
		{
			play_with_ini(g_file_list[7]);
		}
		else if (strncmp(cmd, "9", 1) == 0)
		{
			play_with_ini(g_file_list[8]);
		}
		else if (strncmp(cmd, "b", 1) == 0)
		{
				player_play();
		}
		else if (strncmp(cmd, "c", 1) == 0)
		{
				player_stop();
		}
		else if (strncmp(cmd, "d", 1) == 0)
		{
				player_resume();
		}
		else if (strncmp(cmd, "e", 1) == 0)
		{
				player_pause();
		}
		else if (strncmp(cmd, "f", 1) == 0)
		{
				g_menu_state = CURRENT_STATUS_VOLUME;
		}
		else if (strncmp(cmd, "g", 1) == 0)
		{
			MMPlayerVolumeType volume = {0, };
			get_volume(&volume);
		}
		else if (strncmp(cmd, "h", 1) == 0 )
		{
				g_menu_state = CURRENT_STATUS_POSITION_TIME;
		}
		else if (strncmp(cmd, "r", 1) == 0 )
		{
				g_menu_state = CURRENT_STATUS_POSITION_PERCENT;
		}
		else if (strncmp(cmd, "i", 1) == 0 )
		{
				get_position();
		}
		else if (strncmp(cmd, "j", 1) == 0)
		{
			 	print_info(); // enabled!!!:+:100112
		}
		else if (strncmp(cmd, "o", 1) == 0)
		{
				gboolean old = g_audio_dsp;
				if (old)
					g_audio_dsp =0;
				else
					g_audio_dsp =1;

			 	g_print (">> g_audio_dsp = [%d] => [%d]\n", old, g_audio_dsp);
		}
		else if (strncmp(cmd, "p", 1) == 0)
		{
				gboolean old = g_video_dsp;
				if (old)
					g_video_dsp =0;
				else
					g_video_dsp =1;

			 	g_print (">> g_video_dsp = [%d] => [%d]\n", old, g_video_dsp);
		}
		else if ( strncmp(cmd, "k", 1) == 0 )
		{
				toggle_audiosink_fadeup();
		}
		else if (strncmp(cmd, "m", 1) == 0)
		{
				g_menu_state = CURRENT_STATUS_PLAYCOUNT;
		}
		else if (strncmp (cmd, "x", 1) == 0)
		{
				g_menu_state = CURRENT_STATUS_DISPLAYMETHOD;
		}
#ifdef _USE_XVIMAGESINK //tskim:~:ImplementationFullscreen_090119
		else if (strncmp (cmd, "x", 1) == 0)
		{
				change_fullscreen(overlay);
			}
		else if (strncmp (cmd, "y", 1) == 0)
		{
				change_fullscreen(overlay);
		}
#endif
#ifdef AUDIO_FILTER_EFFECT
		else if (strncmp (cmd, "z", 1) == 0)
		{
				g_menu_state = CURRENT_STATUS_R2VS;
		}
#endif
		else if (strncmp(cmd, "u", 1) == 0)
		{
				destroy_player();
		}
		else if (strncmp(cmd, "q", 1) == 0)
		{
				quit_pushing = TRUE;
				quit_program();
		}
		else if (strncmp(cmd, "[", 1) == 0)
		{
			toggle_sound_spk_out_only ();
		}
		else if (strncmp(cmd, "{", 1) == 0)
		{
			g_print ("mm_session_init(MM_SESSION_TYPE_SHARE) = %d\n", mm_session_init (MM_SESSION_TYPE_SHARE));
		}
		else if (strncmp(cmd, "}", 1) == 0)
		{
			g_print ("mm_session_init(MM_SESSION_TYPE_EXCLUSIVE) = %d\n", mm_session_init (MM_SESSION_TYPE_EXCLUSIVE));
		}
		else if(strncmp(cmd, "$", 1) == 0)
		{
			g_menu_state=CURRENT_STATUS_SUBTITLE_FILENAME;
		}
		else if(strncmp(cmd, "%", 1) == 0)
		{
			if (g_subtitle_silent)
				g_subtitle_silent = FALSE;
			else
				g_subtitle_silent = TRUE;

			toggle_subtitle_silent (g_subtitle_silent);

			reset_menu_state();
		}
		else
		{
				g_print("unknown menu \n");
		}
	}
	else
	{
		if (strncmp(cmd, "sp", 2) == 0)
		{
			g_menu_state = CURRENT_STATUS_SPEED_PLAYBACK;
		}
		else if (strncmp(cmd, "ca", 2) == 0)
		{
			player_capture();
		}
		else if (strncmp(cmd, "ro", 2) == 0)
		{
			player_rotate();
		}
		else if (strncmp(cmd, "sr", 2) == 0)
		{
			toggle_section_repeat();
		}
		else if (strncmp(cmd, "ac", 2) == 0)
		{
			/* It should be called before starting play. */
			set_audio_callback();
		}
		else if (strncmp(cmd, "vc", 2) == 0)
		{
			set_video_callback();
		}		
		else if (strncmp (cmd, "dv", 2) == 0)
		{
			g_menu_state = CURRENT_STATUS_DISPLAY_VISIBLE;
		}
		else if (strncmp (cmd, "rv", 2) == 0)
		{
			g_menu_state = CURRENT_STATUS_RESIZE_VIDEO;
		}		

		else if (strncmp(cmd, "help", 4) == 0)
		{
			g_timeout_add(100, timeout_menu_display, 0);
		}
	}
}

static void interpret (char *cmd)
{
	switch (g_menu_state)
	{
		case CURRENT_STATUS_MAINMENU:
		{
			_interpret_main_menu(cmd);
		}
			break;

		case CURRENT_STATUS_FILENAME:
		{
			input_filename(cmd);

			reset_menu_state();
		}
			break;

		case CURRENT_STATUS_VOLUME:
			{
			MMPlayerVolumeType volume;
				int i;
				float level = atof(cmd);

				g_print ("level = [%f]\n", level);

				for(i=0; i<MM_VOLUME_CHANNEL_NUM; i++)
					volume.level[i] = (gfloat)level;

				set_volume(&volume);

				reset_menu_state();
			}
			break;

		case CURRENT_STATUS_SPEED_PLAYBACK:
		{
			float rate = atof(cmd);

			g_print("playback rate = %f", rate);

			mm_player_set_play_speed(g_player, rate);

			reset_menu_state();
		}
		break;

		case CURRENT_STATUS_POSITION_TIME:
			{
				unsigned long position = atol(cmd);
				set_position(position, MM_PLAYER_POS_FORMAT_TIME);

			reset_menu_state();
			}
			break;

		case CURRENT_STATUS_POSITION_PERCENT:
		{
			unsigned long position = atol(cmd);
			set_position(position, MM_PLAYER_POS_FORMAT_PERCENT);

			reset_menu_state();
		}
		break;

		case CURRENT_STATUS_DISPLAYMETHOD:
		{
				unsigned long option = atol(cmd);
				set_display_method(option);

				reset_menu_state();
		}
		break;

		case CURRENT_STATUS_DISPLAY_VISIBLE:
		{
				unsigned long option = atol(cmd);
				set_display_visible(option);
				reset_menu_state();
		}
		break;

		case CURRENT_STATUS_RESIZE_VIDEO:
		{
				unsigned int option = atoi(cmd);
				resize_video(option);
				reset_menu_state();	
		}
		break;

#ifdef AUDIO_FILTER_EFFECT
		case CURRENT_STATUS_R2VS:
			{
				int ret = 0;
				ret = set_r2vs(cmd);

				if (ret == -1)
					g_print("unknown menu - R2VS\n");

			reset_menu_state();
			}
			break;
#endif
		case CURRENT_STATUS_PLAYCOUNT:
			{
				int count = atoi(cmd);
				set_playcount(count);

			reset_menu_state();
			}
			break;

		case CURRENT_STATUS_SUBTITLE_FILENAME:
		{
			input_subtitle_filename(cmd);

			reset_menu_state();
		}

	}

	g_timeout_add(100, timeout_menu_display, 0);
}

gboolean input (GIOChannel *channel)
{
    char buf[MAX_STRING_LEN + 3];
    gsize read;

    g_io_channel_read(channel, buf, MAX_STRING_LEN, &read);
    buf[read] = '\0';
	g_strstrip(buf);
    interpret (buf);
    return TRUE;
}

#ifdef _USE_XVIMAGESINK
static gboolean
        overlay_expose( GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    //unsigned long xid;
    g_printf("\n overlay expose \n");

    xid = gtk_overlay_drawing_get_resource(widget);
    g_print(">>Window ID : %d\n", xid);

    return FALSE;
}
#endif

#ifdef _USE_XVIMAGESINK //tskim:~:ImplementationFullscreen_090119
GtkWidget *event_win;

void change_fullscreen(GtkWidget* widget)
{
	static gboolean is_full = FALSE;
	GtkWidget  *main_window;
	GtkWidget * form;

	is_full = (is_full)?FALSE:TRUE;

	main_window = GTK_MAIN_WINDOW(gtk_widget_get_toplevel(widget));
	form = gtk_main_window_get_current_form(GTK_MAIN_WINDOW(main_window));

	if(is_full)
	{
		gtk_main_window_set_title_style(main_window, GTK_WIN_TITLE_STYLE_NONE);
		gtk_form_set_has_softkey(GTK_FORM(form), FALSE);
		gtk_window_fullscreen(GTK_WINDOW(main_window));

		gtk_widget_show_all(event_win);
	}
	else
	{
		gtk_main_window_set_title_style(main_window, GTK_WIN_TITLE_STYLE_TEXT_ICON);
		gtk_form_set_has_softkey(GTK_FORM(form), TRUE);

		gtk_form_add_softkey(GTK_FORM(form), "Full Screen", NULL, SOFTKEY_CALLBACK, softkey_cb_select_and_back, NULL);

		gtk_window_unfullscreen(GTK_WINDOW(main_window));

		gtk_widget_hide_all(event_win);
	}
}

gboolean
softkey_cb_select_and_back(GtkWidget *widget, SoftkeyPosition position, gpointer data)
{
	GtkWidget  *main_window;
	GtkWidget * form;

	#if 0
	main_window = GTK_MAIN_WINDOW(gtk_widget_get_toplevel(widget));
	if( GTK_IS_WIDGET(main_window) )
	{
		form = gtk_main_window_get_current_form(GTK_MAIN_WINDOW(main_window));
	}
	#endif

	switch(position) {
	case SOFTKEY1:
		change_fullscreen(widget);
		break;
	case SOFTKEY2:
	case SOFTKEY3:
	case SOFTKEY4:
	default:
	    return FALSE;
	}

	return FALSE;
}
#endif

void make_window()
{

#if defined(_USE_XVIMAGESINK) && defined(_USE_GTK)
	//Create Overlay Widget
	gtk_init(&argc, &argv);

	root = gtk_main_window_new(GTK_WIN_STYLE_DEFAULT);
	form = gtk_form_new(TRUE);
	overlay = gtk_overlay_drawing_new();

	gtk_main_window_set_title_style(root, GTK_WIN_TITLE_STYLE_TEXT_ICON);

	gtk_form_add_softkey(GTK_FORM(form), "Full Screen", NULL, SOFTKEY_CALLBACK, softkey_cb_select_and_back, overlay);

	gtk_main_window_add_form(root, form);
	gtk_main_window_set_current_form(root, form);
	gtk_container_add(GTK_CONTAINER(form), overlay);

	g_signal_connect (overlay, "expose_event", G_CALLBACK(overlay_expose), NULL);

	gtk_widget_show_all(root);
	gdk_gc_set_alpha(root->style->bg_gc[GTK_STATE_NORMAL], 0);
	gdk_gc_set_alpha(overlay->style->bg_gc[GTK_STATE_NORMAL], 0);

	/*Create button window*/
	GtkWidget *button;

	event_win = gtk_event_box_new();
	button=gtk_button_new_with_label("BackToNormal");

	g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (change_fullscreen), (gpointer)NULL);

	gtk_container_add(GTK_CONTAINER(event_win), button);
	gtk_container_add(GTK_CONTAINER(overlay), event_win);
	gtk_widget_set_uposition(event_win, 50, 50);
#elif defined(_USE_GTK) && !defined(_USE_XVIMAGESINK) && !defined(_USE_V4L2SINK)
	GtkAllocation allocation;

  	root = gtk_main_window_new(GTK_WIN_STYLE_DEFAULT);

  	gtk_widget_set_colormap(root, gdk_screen_get_rgba_colormap(gtk_widget_get_screen(root)));

	overlay = gtk_overlay_drawing_new();
	gtk_overlay_drawing_set_type(overlay, GTK_OVERLAY_TYPE_YUV420);
	gtk_widget_set_size_request(overlay, FULL_WIDTH, FULL_HEIGHT); // sbs:+:081203

	allocation.x = 0;
	allocation.y = 0;

	// sbs:+:081203
	allocation.width = FULL_WIDTH;
	allocation.height = FULL_HEIGHT;

	gtk_widget_size_allocate(overlay, &allocation);

	fixed = gtk_fixed_new();
	form = gtk_form_new(FALSE);

	gtk_main_window_add_form(GTK_MAIN_WINDOW(root), GTK_FORM(form));
	gtk_main_window_set_current_form(GTK_MAIN_WINDOW(root), GTK_FORM(form));
	gtk_container_add(GTK_CONTAINER(form), fixed);
	gtk_fixed_put (GTK_FIXED (fixed), overlay, 0, 0);

	gtk_widget_show_all(root);
	gdk_gc_set_alpha(root->style->bg_gc[GTK_STATE_NORMAL], 0);
	gdk_gc_set_alpha(overlay->style->bg_gc[GTK_STATE_NORMAL], 0);
#endif /* _USE_XVIMAGESINK */
}

// format:
// mm_player_testsuite [media-filename [-NUM]]
// NUM is the number of iterations to perform
int main(int argc, char *argv[])
{
		int ret = 0;
		GIOChannel *stdin_channel;
		MMTA_INIT();

#ifdef MTRACE
		mtrace();
		MMHandleType prop;
		GError *error = NULL;
#endif

		stdin_channel = g_io_channel_unix_new(0);
		g_io_add_watch(stdin_channel, G_IO_IN, (GIOFunc)input, NULL);

		make_window();
		//init_file_path();

		#ifdef PCM_DUMP
		g_pcm_dump_fp = fopen(DUMP_PCM_NAME, "w+");
		#endif

#ifdef __arm__
		// initialize sound path for using ear
#if 0
		ret = MMSoundSetPathEx2(MM_SOUND_GAIN_KEYTONE, MM_SOUND_PATH_SPK, MM_SOUND_PATH_NONE, MM_SOUND_PATH_OPTION_AUTO_HEADSET_CONTROL);

		if(ret < 0)
		{
				printf("path error\n");
				return -1;
		}
#endif

#endif // __arm__

		displaymenu();

#if defined(_USE_GTK)
		g_loop = g_main_loop_new(NULL, FALSE);
		g_main_loop_run(g_loop);
		return 0;
#elif defined(_USE_EFL)
		memset(&ad, 0x0, sizeof(struct appdata));
		ops.data = &ad;

		return appcore_efl_main(PACKAGE, &argc, &argv, &ops);
#endif

}

