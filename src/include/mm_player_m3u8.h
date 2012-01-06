/* GStreamer
 * Copyright (C) 2010 Marc-Andre Lureau <marcandre.lureau@gmail.com>
 * Copyright (C) 2010 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * m3u8.h:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* This source code is taken from m3u8.c, which is licensed by GNU Library General Public License.
 * AES-128 bit decryption features are changed from m3u8.c.
 * - gst_m3u8_update and gst_m3u8_media_file_new are modified.
 * - gst_m3u8_getIV_from_mediasequence is added.
 * For convenience,
 * - gst_m3u8_client_get_next_fragment is modified.
 * - gst_m3u8_client_check_next_fragment is added.
 * File name is changed to mm_player_m3u8.c
 */


#ifndef __MM_PLAYER_M3U8_H__
#define __MM_PLAYER_M3U8_H__

#include <glib.h>

G_BEGIN_DECLS typedef struct _GstM3U8 GstM3U8;
typedef struct _GstM3U8MediaFile GstM3U8MediaFile;
typedef struct _GstM3U8Client GstM3U8Client;

#define GST_M3U8_MEDIA_FILE(f) ((GstM3U8MediaFile*)f)

struct _GstM3U8
{
  gchar *uri;

  gboolean endlist;             /* if ENDLIST has been reached */
  gint version;                 /* last EXT-X-VERSION */
  gint targetduration;          /* last EXT-X-TARGETDURATION */
  gchar *allowcache;            /* last EXT-X-ALLOWCACHE */

  gint bandwidth;
  gint program_id;
  gchar *codecs;
  gint width;
  gint height;
  GList *files;

  /*< private > */
  gchar *last_data;
  GList *lists;                 /* list of GstM3U8 from the main playlist */
  GstM3U8 *parent;              /* main playlist (if any) */
  guint mediasequence;          /* EXT-X-MEDIA-SEQUENCE & increased with new media file */
};

struct _GstM3U8MediaFile
{
  gchar *title;
  gint duration;
  gchar *uri;
  guint sequence;               /* the sequence nb of this file */

  gchar *key_url;
  unsigned char key[16];
  unsigned char iv[16];
  
};

struct _GstM3U8Client
{
  GstM3U8 *main;                /* main playlist */
  GstM3U8 *current;
  guint update_failed_count;
  gint sequence;                /* the next sequence for this client */
};


GstM3U8Client *gst_m3u8_client_new (const gchar * uri);
void gst_m3u8_client_free (GstM3U8Client * client);
gboolean gst_m3u8_client_update (GstM3U8Client * client, gchar * data);
void gst_m3u8_client_set_current (GstM3U8Client * client, GstM3U8 * m3u8);
const GstM3U8MediaFile *gst_m3u8_client_get_next_fragment (GstM3U8Client * client,  gboolean * discontinuity);
#define gst_m3u8_client_get_uri(Client) ((Client)->main->uri)
#define gst_m3u8_client_has_variant_playlist(Client) ((Client)->main->lists)
#define gst_m3u8_client_is_live(Client) (!(Client)->current->endlist)
#define gst_m3u8_client_allow_cache(Client) ((Client)->current->allowcache)

const gboolean gst_m3u8_client_check_next_fragment (GstM3U8Client * client);

G_END_DECLS
#endif /* __MM_PLAYER_M3U8_H__ */
