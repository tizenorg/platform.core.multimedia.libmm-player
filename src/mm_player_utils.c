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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unicode/ucsdet.h>

#include <mm_debug.h>
#include "mm_player_utils.h"

bool util_exist_file_path(const char *file_path)
{
	int fd = NULL;
	struct stat stat_results = {0, };

	if (!file_path || !strlen(file_path))
		return FALSE;

	fd = open (file_path, O_RDONLY);

	if (fd < 0)
	{
		debug_error("failed to open %s, %s", file_path, strerror(errno));
		return FALSE;
	}

	if (fstat(fd, &stat_results) < 0)
	{
		debug_error("failed to get file status");
	}
	else
	{
		debug_warning("file size : %lld bytes", (long long)stat_results.st_size); //need to chech file validity
	}

	close(fd);

	return TRUE;
}

bool util_write_file_backup(const char *backup_path, char *data_ptr, int data_size)
{
	debug_log("\n");

	FILE *fp = NULL;
	int wsize = 0;

	fp = fopen(backup_path, "wb");
	if (!fp)
		return FALSE;

	wsize = fwrite(data_ptr, sizeof(char), data_size, fp);

	fclose(fp);

	if (wsize != data_size) {
		if (!access(backup_path, R_OK))
			remove(backup_path);

		debug_error("No space to write!\n");

		return FALSE;
	}

	return TRUE;
}

bool util_remove_file_backup(const char *backup_path)
{
	debug_log("\n");

	if (!backup_path || !strlen(backup_path))
		return FALSE;

	int res = access(backup_path, R_OK);
	if (!res)
	{
		if (remove(backup_path) == -1)
			return FALSE;
	}

	return TRUE;
}

#define DETECTION_PREFIX_SIZE	20
//bool util_is_midi_type_by_mem(void *mem, int size)
int util_is_midi_type_by_mem(void *mem, int size)
{
	debug_log("\n");
	
	const char *p = (const char *)mem;

	if (size < DETECTION_PREFIX_SIZE)
		return MM_AUDIO_CODEC_INVALID;

	/* mmf file detection */
	if (p[0] == 'M' && p[1] == 'M' && p[2] == 'M' && p[3] == 'D') {
		debug_log("MM_AUDIO_CODEC_MMF\n");
		return MM_AUDIO_CODEC_MMF;
	}

	/* midi file detection */
	if (p[0] == 'M' && p[1] == 'T' && p[2] == 'h' && p[3] == 'd') {
		debug_log ("MM_AUDIO_CODEC_MIDI, %d\n", MM_AUDIO_CODEC_MIDI);
		return MM_AUDIO_CODEC_MIDI;
	}
	/* mxmf file detection */
	if (p[0] == 'X' && p[1] == 'M' && p[2] == 'F' && p[3] == '_') {
		debug_log ("MM_AUDIO_CODEC_MXMF\n");
		return MM_AUDIO_CODEC_MXMF;
	}

	/* wave file detection */
	if (p[0] == 'R' && p[1] == 'I' && p[2] == 'F' && p[3] == 'F' &&
		p[8] == 'W' && p[9] == 'A' && p[10] == 'V' && p[11] == 'E' &&
		p[12] == 'f' && p[13] == 'm' && p[14] == 't') {
		debug_log ("MM_AUDIO_CODEC_WAVE\n");
		return MM_AUDIO_CODEC_WAVE;
	}
	/* i-melody file detection */
	if (memcmp(p, "BEGIN:IMELODY", 13) == 0)
	{
		debug_log ("MM_AUDIO_CODEC_IMELODY\n");
		return MM_AUDIO_CODEC_IMELODY;
	}

	return MM_AUDIO_CODEC_INVALID;
}

//bool util_is_midi_type_by_file(const char *file_path)
int util_is_midi_type_by_file(const char *file_path)
{
	debug_log("\n");
	
	struct stat file_attrib;
	FILE *fp = NULL;
	char prefix[DETECTION_PREFIX_SIZE] = {0,};
	int size;

	if (!file_path)
		return FALSE;

	fp = fopen(file_path, "r");

	if (!fp)
	return FALSE;

	memset(&file_attrib, 0, sizeof(file_attrib));

	if (stat(file_path, &file_attrib) != 0)
	{
		fclose(fp);
		return FALSE;
	}

	size = (int) file_attrib.st_size;

	if (size < DETECTION_PREFIX_SIZE)
	{
		fclose(fp);
		return FALSE;
	}
	
	size = fread(prefix, sizeof(char), DETECTION_PREFIX_SIZE, fp);

	fclose(fp);

	return util_is_midi_type_by_mem(prefix, size);
}

/* messages are treated as warnings bcz those code should not be checked in. 
 * and no error handling will supported for same manner. 
 */
gboolean 
__util_gst_pad_probe(GstPad *pad, GstBuffer *buffer, gpointer u_data)
{
	gint flag = (gint) u_data;
	GstElement* parent = NULL;
	gboolean ret = TRUE;
	
	/* show name as default */
	parent = (GstElement*)gst_object_get_parent(GST_OBJECT(pad));
	debug_warning("PAD PROBE : %s:%s\n", GST_ELEMENT_NAME(parent), GST_PAD_NAME(pad));
	
	/* show time stamp */
	if ( flag & MM_PROBE_TIMESTAMP )
	{
		debug_warning("ts : %u:%02u:%02u.%09u\n",  GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buffer)));
	}

	/* show buffer size */
	if ( flag & MM_PROBE_BUFFERSIZE )
	{
		debug_warning("buffer size : %ud\n", GST_BUFFER_SIZE(buffer));
	}

	/* show buffer duration */
	if ( flag & MM_PROBE_BUFFER_DURATION )
	{
		debug_warning("dur : %lld\n", GST_BUFFER_DURATION(buffer));
	}

	/* show buffer caps */
	if ( flag & MM_PROBE_CAPS )
	{
		debug_warning("caps : %s\n", gst_caps_to_string(GST_BUFFER_CAPS(buffer)));
	}

	/* drop buffer if flag is on */
	if ( flag & MM_PROBE_DROP_BUFFER )
	{
		debug_warning("dropping\n");
		ret = FALSE;
	}
		
	/* show clock time */
	if ( flag & MM_PROBE_CLOCK_TIME )
	{
		GstClock* clock = NULL;
		GstClockTime now = GST_CLOCK_TIME_NONE;

		clock = GST_ELEMENT_CLOCK ( parent );

		if ( clock )
		{
			now = gst_clock_get_time( clock );
			debug_warning("clock time : %" GST_TIME_FORMAT "\n", GST_TIME_ARGS( now ));
		}
	}

	if ( parent )
		gst_object_unref(parent);

	return ret;
}

char** 
util_get_cookie_list ( const char *cookies )
{
	char **cookie_list = NULL;
	char *temp = NULL;
	gint i = 0;

	if ( !cookies || !strlen(cookies) )
		return NULL;

	debug_log("cookies : %d[bytes] - %s \n", strlen(cookies), cookies);

	temp = g_strdup(cookies);

	/* trimming. it works inplace */
	g_strstrip(temp);

	/* split */
	cookie_list = g_strsplit(temp, ";", 100);

	for ( i = 0; i < g_strv_length(cookie_list); i++ )
	{
		if ( cookie_list[i] && strlen(cookie_list[i]) )
		{
			g_strstrip(cookie_list[i]);
			debug_log("cookie_list[%d] : %d[bytes] - %s \n", i, strlen(cookie_list[i]), cookie_list[i]);
		}
		else
		{
			cookie_list[i][0]='\0';
		}
	}

	if (temp)
		g_free (temp);
	temp=NULL;

	return cookie_list;
}

bool util_check_valid_url ( const char *proxy )
{
	struct in_addr proxy_addr;
	bool ret = TRUE;
	
	return_val_if_fail ( proxy, FALSE );
	return_val_if_fail ( strlen(proxy), FALSE );

       if ( inet_aton(proxy, &proxy_addr) != 0 )
	{
		debug_warning("invalid proxy is set. \n");
		ret = FALSE;
	}
	   
	return ret;
}

/* check the given path is indicating sdp file */
bool 
util_is_sdp_file ( const char *path )
{
	gboolean ret = FALSE;
	gchar* uri = NULL;
	
	debug_fenter();
	
	return_val_if_fail ( path, FALSE );

	uri = g_ascii_strdown ( path, -1 );

	if ( uri == NULL)
	{
		return FALSE;
	}

	/* trimming */
	g_strstrip( uri );

	/* strlen(".sdp") == 4 */
	if ( strlen( uri ) <= 4 )
	{
		debug_warning ( "path is too short.\n" );
		return ret;
	}

	/* first, check extension name */
	ret = g_str_has_suffix ( uri, "sdp" );

	/* second, if no suffix is there, check it's contents */
	if ( ! ret )
	{
		/* FIXIT : do it soon */
		debug_log("determining whether it's sdp or not with it's content is not implemented yet. ;)\n");
	}

	g_free( uri);
	uri = NULL;

	return ret;
}

int64_t 
util_get_time ( void )
{
	struct timeval tv;
	gettimeofday(&tv,NULL);
	return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

int 
util_get_rank_increase ( const char *factory_class )
{
	gint rank_pri_inc = 20;
	gint rank_sec_inc = 10;
	gint ret = 0;

	if ( g_strrstr(factory_class,"Dsp") ) 
		ret = rank_pri_inc;
	else if ( g_strrstr(factory_class,"HW") ) 
		ret = rank_pri_inc;
	else if ( g_strrstr(factory_class,"Arm") )
		ret = rank_sec_inc;

	return ret;
}

int 
util_factory_rank_compare(GstPluginFeature *f1, GstPluginFeature *f2) // @
{
	const gchar *klass;
    	int f1_rank_inc=0, f2_rank_inc=0;

    	klass = gst_element_factory_get_klass(GST_ELEMENT_FACTORY(f1));
	f1_rank_inc = util_get_rank_increase ( klass );

    	klass = gst_element_factory_get_klass(GST_ELEMENT_FACTORY(f2));
   	f2_rank_inc = util_get_rank_increase ( klass );

    	return (gst_plugin_feature_get_rank(f2)+f2_rank_inc) - (gst_plugin_feature_get_rank(f1)+f1_rank_inc );
}

char*
util_get_charset(const char *file_path)
{
	UCharsetDetector* ucsd;
	const UCharsetMatch* ucm;
	UErrorCode status = U_ZERO_ERROR;

	const char* charset = NULL;
	char *buf = NULL;
	FILE* fin;

	fin = fopen(file_path, "r");
	if (!fin)
	{
		debug_error("fail to open file %s\n", file_path);
		return NULL;
	}

	ucsd = ucsdet_open( &status );
	if( U_FAILURE(status) ) {
		debug_error("fail to ucsdet_open\n");
		return NULL;
	}

	ucsdet_enableInputFilter( ucsd, TRUE );

	buf = g_malloc(1024*1024);
	if (!buf)
	{
		debug_error("fail to alloc\n");
		goto done;
	}

	fread( buf, 1, 1024*1024, fin );
	fclose(fin);

	ucsdet_setText( ucsd, buf, strlen(buf), &status );
	if( U_FAILURE(status) ) {
		debug_error("fail to ucsdet_setText\n");
		goto done;
	}

	ucm = ucsdet_detect( ucsd, &status );
	if( U_FAILURE(status) ) {
		debug_error("fail to ucsdet_detect\n");
		goto done;
	}

	charset = ucsdet_getName( ucm, &status );
	if( U_FAILURE(status) ) {
		debug_error("fail to ucsdet_getName\n");
		goto done;
	}

done:
	ucsdet_close( ucsd );

	if (buf)
		g_free(buf);

	return charset;
}

