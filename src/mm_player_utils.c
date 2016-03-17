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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unicode/ucsdet.h>
#include <dlog.h>

#include "mm_player_utils.h"

int util_exist_file_path(const char *file_path)
{
	int fd = 0;
	struct stat stat_results = {0, };

	if (!file_path || !strlen(file_path))
		return MM_ERROR_PLAYER_FILE_NOT_FOUND;

	fd = open (file_path, O_RDONLY);

	if (fd < 0)
	{
		char str_error[256];
		strerror_r(errno, str_error, sizeof(str_error));
		LOGE("failed to open file by %s (%d)", str_error, errno);

		if (EACCES == errno)
			return MM_ERROR_PLAYER_PERMISSION_DENIED;

		return MM_ERROR_PLAYER_FILE_NOT_FOUND;
	}

	if (fstat(fd, &stat_results) < 0)
	{
		LOGE("failed to get file status");
	}
	else if (stat_results.st_size == 0)
	{
		LOGE("file size is zero");
		close(fd);
		return MM_ERROR_PLAYER_FILE_NOT_FOUND;
	}
	else
	{
		LOGW("file size : %lld bytes", (long long)stat_results.st_size);
	}

	close(fd);

	return MM_ERROR_NONE;
}

bool util_write_file_backup(const char *backup_path, char *data_ptr, int data_size)
{
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

		LOGE("No space to write!\n");

		return FALSE;
	}

	return TRUE;
}

bool util_remove_file_backup(const char *backup_path)
{
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
int util_is_midi_type_by_mem(void *mem, int size)
{
	const char *p = (const char *)mem;

	if (size < DETECTION_PREFIX_SIZE)
		return MM_AUDIO_CODEC_INVALID;

	/* mmf file detection */
	if (p[0] == 'M' && p[1] == 'M' && p[2] == 'M' && p[3] == 'D') {
		LOGD("MM_AUDIO_CODEC_MMF\n");
		return MM_AUDIO_CODEC_MMF;
	}

	/* midi file detection */
	if (p[0] == 'M' && p[1] == 'T' && p[2] == 'h' && p[3] == 'd') {
		LOGD ("MM_AUDIO_CODEC_MIDI, %d\n", MM_AUDIO_CODEC_MIDI);
		return MM_AUDIO_CODEC_MIDI;
	}
	/* mxmf file detection */
	if (p[0] == 'X' && p[1] == 'M' && p[2] == 'F' && p[3] == '_') {
		LOGD ("MM_AUDIO_CODEC_MXMF\n");
		return MM_AUDIO_CODEC_MXMF;
	}

	/* wave file detection */
	if (p[0] == 'R' && p[1] == 'I' && p[2] == 'F' && p[3] == 'F' &&
		p[8] == 'W' && p[9] == 'A' && p[10] == 'V' && p[11] == 'E' &&
		p[12] == 'f' && p[13] == 'm' && p[14] == 't') {
		LOGD ("MM_AUDIO_CODEC_WAVE\n");
		return MM_AUDIO_CODEC_WAVE;
	}
	/* i-melody file detection */
	if (memcmp(p, "BEGIN:IMELODY", 13) == 0)
	{
		LOGD ("MM_AUDIO_CODEC_IMELODY\n");
		return MM_AUDIO_CODEC_IMELODY;
	}

	return MM_AUDIO_CODEC_INVALID;
}

int util_is_midi_type_by_file(const char *file_path)
{
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

char**
util_get_cookie_list ( const char *cookies )
{
	char **cookie_list = NULL;
	char *temp = NULL;
	gint i = 0;

	if ( !cookies || !strlen(cookies) )
		return NULL;

	SECURE_LOGD("cookies : %d[bytes] - %s \n", strlen(cookies), cookies);

	temp = g_strdup(cookies);

	/* trimming. it works inplace */
	g_strstrip(temp);

	/* split */
	cookie_list = g_strsplit(temp, ";", 100);

	for ( i = 0; i < g_strv_length(cookie_list); i++ )
	{
		if (cookie_list[i])
		{
			if (strlen(cookie_list[i]))
			{
				g_strstrip(cookie_list[i]);
				SECURE_LOGD("cookie_list[%d] : %d[bytes] - %s \n", i, strlen(cookie_list[i]), cookie_list[i]);
			}
			else
			{
				cookie_list[i][0]='\0';
			}
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

	MMPLAYER_RETURN_VAL_IF_FAIL ( proxy, FALSE );
	MMPLAYER_RETURN_VAL_IF_FAIL ( strlen(proxy), FALSE );

	if ( inet_aton(proxy, &proxy_addr) != 0 )
	{
		LOGW("invalid proxy is set. \n");
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

	MMPLAYER_FENTER();

	MMPLAYER_RETURN_VAL_IF_FAIL ( path, FALSE );

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
		LOGW ( "path is too short.\n" );
		return ret;
	}

	/* first, check extension name */
	ret = g_str_has_suffix ( uri, "sdp" );

	/* second, if no suffix is there, check it's contents */
	if ( ! ret )
	{
		/* FIXIT : do it soon */
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
util_factory_rank_compare(GstPluginFeature *f1, GstPluginFeature *f2)
{
	const gchar *klass;
	int f1_rank_inc=0, f2_rank_inc=0;

	klass = gst_element_factory_get_klass(GST_ELEMENT_FACTORY(f1));
	f1_rank_inc = util_get_rank_increase ( klass );

	klass = gst_element_factory_get_klass(GST_ELEMENT_FACTORY(f2));
	f2_rank_inc = util_get_rank_increase ( klass );

	return (gst_plugin_feature_get_rank(f2)+f2_rank_inc) - (gst_plugin_feature_get_rank(f1)+f1_rank_inc );
}

const char*
util_get_charset(const char *file_path)
{
	UCharsetDetector* ucsd;
	const UCharsetMatch* ucm;
	UErrorCode status = U_ZERO_ERROR;

	const char* charset = NULL;
	char *buf = NULL;
	FILE* fin =0;
	size_t n_size = 0;

	fin = fopen(file_path, "r");
	if (!fin)
	{
		SECURE_LOGE("fail to open file %s\n", file_path);
		return NULL;
	}

	ucsd = ucsdet_open( &status );
	if( U_FAILURE(status) ) {
		LOGE("fail to ucsdet_open\n");
		goto done;
	}

	ucsdet_enableInputFilter( ucsd, TRUE );

	buf = g_malloc(1024*1024);
	if (!buf)
	{
		LOGE("fail to alloc\n");
		goto done;
	}

	n_size = fread( buf, 1, 1024*1024, fin );

	if (!n_size)
		goto done;

	ucsdet_setText( ucsd, buf, strlen(buf), &status );
	if( U_FAILURE(status) ) {
		LOGE("fail to ucsdet_setText\n");
		goto done;
	}

	ucm = ucsdet_detect( ucsd, &status );
	if( U_FAILURE(status) ) {
		LOGE("fail to ucsdet_detect\n");
		goto done;
	}

	charset = ucsdet_getName( ucm, &status );
	if( U_FAILURE(status) ) {
		LOGE("fail to ucsdet_getName\n");
		goto done;
	}

	/* CP949 encoding is an extension of the EUC-KR and it is backwards compatible.*/
	if(charset && !strcmp(charset, "EUC-KR")) {
		charset = "CP949";
	}

done:
	if(fin)
		fclose(fin);

	if(ucsd)
		ucsdet_close( ucsd );

	if (buf)
		g_free(buf);

	return charset;
}

int util_get_pixtype(unsigned int fourcc)
{
	int pixtype = MM_PIXEL_FORMAT_INVALID;

    /*
	char *pfourcc = (char*)&fourcc;

	LOGD("fourcc(%c%c%c%c)",
	                 pfourcc[0], pfourcc[1], pfourcc[2], pfourcc[3]);
    */

	switch (fourcc) {
	case GST_MAKE_FOURCC ('S', 'N', '1', '2'):
	case GST_MAKE_FOURCC ('N', 'V', '1', '2'):
		pixtype = MM_PIXEL_FORMAT_NV12;
		break;
	case GST_MAKE_FOURCC ('S', 'T', '1', '2'):
		pixtype = MM_PIXEL_FORMAT_NV12T;
		break;
	case GST_MAKE_FOURCC ('S', 'N', '2', '1'):
	case GST_MAKE_FOURCC ('N', 'V', '2', '1'):
		pixtype = MM_PIXEL_FORMAT_NV21;
		break;
	case GST_MAKE_FOURCC ('S', 'U', 'Y', 'V'):
	case GST_MAKE_FOURCC ('Y', 'U', 'Y', 'V'):
	case GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'):
		pixtype = MM_PIXEL_FORMAT_YUYV;
		break;
	case GST_MAKE_FOURCC ('S', 'Y', 'V', 'Y'):
	case GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y'):
		pixtype = MM_PIXEL_FORMAT_UYVY;
		break;
	case GST_MAKE_FOURCC ('S', '4', '2', '0'):
	case GST_MAKE_FOURCC ('I', '4', '2', '0'):
		pixtype = MM_PIXEL_FORMAT_I420;
		break;
	case GST_MAKE_FOURCC ('Y', 'V', '1', '2'):
		pixtype = MM_PIXEL_FORMAT_YV12;
		break;
	case GST_MAKE_FOURCC ('4', '2', '2', 'P'):
		pixtype = MM_PIXEL_FORMAT_422P;
		break;
	case GST_MAKE_FOURCC ('R', 'G', 'B', 'P'):
		pixtype = MM_PIXEL_FORMAT_RGB565;
		break;
	case GST_MAKE_FOURCC ('R', 'G', 'B', '3'):
		pixtype = MM_PIXEL_FORMAT_RGB888;
		break;
	case GST_MAKE_FOURCC ('A', 'R', 'G', 'B'):
	case GST_MAKE_FOURCC ('x', 'R', 'G', 'B'):
		pixtype = MM_PIXEL_FORMAT_ARGB;
		break;
	case GST_MAKE_FOURCC ('B', 'G', 'R', 'A'):
	case GST_MAKE_FOURCC ('B', 'G', 'R', 'x'):
	case GST_MAKE_FOURCC ('S', 'R', '3', '2'):
		pixtype = MM_PIXEL_FORMAT_RGBA;
		break;
	case GST_MAKE_FOURCC ('J', 'P', 'E', 'G'):
	case GST_MAKE_FOURCC ('P', 'N', 'G', ' '):
		pixtype = MM_PIXEL_FORMAT_ENCODED;
		break;
	case GST_MAKE_FOURCC ('I', 'T', 'L', 'V'):
		pixtype = MM_PIXEL_FORMAT_ITLV_JPEG_UYVY;
		break;
	default:
		LOGE("Not supported fourcc type(%c%c%c%c)",
		               fourcc, fourcc>>8, fourcc>>16, fourcc>>24);
		pixtype = MM_PIXEL_FORMAT_INVALID;
		break;
	}

	return pixtype;
}
