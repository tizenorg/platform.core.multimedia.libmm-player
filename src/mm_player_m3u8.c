/* GStreamer
 * Copyright (C) 2010 Marc-Andre Lureau <marcandre.lureau@gmail.com>
 *
 * m3u8.c:
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

#include <stdlib.h>
#include <errno.h>
#include <glib.h>

#include "mm_player_m3u8.h"
#include <mm_debug.h>
#include <string.h>


static GstM3U8 *gst_m3u8_new (void);
static void gst_m3u8_free (GstM3U8 * m3u8);
static gboolean gst_m3u8_update (GstM3U8 * m3u8, gchar * data,
    gboolean * updated);
static GstM3U8MediaFile *gst_m3u8_media_file_new (gchar * uri, gchar * title, gint duration,  gchar *key_url, gchar *IV, guint sequence);
static void gst_m3u8_media_file_free (GstM3U8MediaFile * self);

static GstM3U8 *
gst_m3u8_new (void)
{
  GstM3U8 *m3u8;

  m3u8 = g_new0 (GstM3U8, 1);

  return m3u8;
}

static void
gst_m3u8_set_uri (GstM3U8 * self, gchar * uri)
{
  g_return_if_fail (self != NULL);

  if (self->uri)
    g_free (self->uri);
  self->uri = uri;
}

static void
gst_m3u8_free (GstM3U8 * self)
{
  g_return_if_fail (self != NULL);

  g_free (self->uri);
  g_free (self->allowcache);
  g_free (self->codecs);

  g_list_foreach (self->files, (GFunc) gst_m3u8_media_file_free, NULL);
  g_list_free (self->files);

  g_free (self->last_data);
  g_list_foreach (self->lists, (GFunc) gst_m3u8_free, NULL);
  g_list_free (self->lists);

  g_free (self);
}

static GstM3U8MediaFile *
gst_m3u8_media_file_new (gchar * uri, gchar * title, gint duration,  gchar *key_url, gchar *IV, guint sequence)
{
  GstM3U8MediaFile *file;

  file = g_new0 (GstM3U8MediaFile, 1);
  file->uri = uri;
  file->title = title;
  file->duration = duration;
  file->sequence = sequence;
  memset (file->key, 0x00, sizeof (file->key));

  //g_print (" uri = %s  / ", uri);

  if (key_url != NULL)
  {
    file->key_url = g_strdup (key_url);
  }
  else
    file->key_url = NULL;

  if (IV != NULL)
  {
    memcpy (file->iv, IV, sizeof (file->iv));
  }
  return file;
}

static void
gst_m3u8_media_file_free (GstM3U8MediaFile * self)
{
  g_return_if_fail (self != NULL);

  if (self->key_url)
    g_free (self->key_url);

  g_free (self->title);
  g_free (self->uri);
  g_free (self);
}

static gchar *
gst_m3u8_getIV_from_mediasequence (GstM3U8 *self)
{
  gchar *IV = NULL;
  gint i = 0;
  
  IV = g_malloc0 (16);
  if (NULL == IV)
  {
    debug_warning ("Failed to allocate memory...");
    return NULL;
  }

  if (self->mediasequence > INT_MAX)
  {
    debug_warning ("media sequnece is greater than INT_MAX...yet to handle");
  }

  IV [15] = (gchar)(self->mediasequence);
  IV [14] = (gchar)(self->mediasequence >> 8);
  IV [13] = (gchar)(self->mediasequence >> 16);
  IV [12] = (gchar)(self->mediasequence >> 24);
   
  return IV;
}

static gboolean
int_from_string (gchar * ptr, gchar ** endptr, gint * val, gint base)
{
  gchar *end;

  g_return_val_if_fail (ptr != NULL, FALSE);
  g_return_val_if_fail (val != NULL, FALSE);

  errno = 0;
  *val = strtol (ptr, &end, base);
  if ((errno == ERANGE && (*val == LONG_MAX || *val == LONG_MIN))
      || (errno != 0 && *val == 0)) {
//    debug_warning (g_strerror (errno));
    return FALSE;
  }

  if (endptr)
    *endptr = end;

  return end != ptr;
}

static gboolean
parse_attributes (gchar ** ptr, gchar ** a, gchar ** v)
{
  gchar *end, *p;

  g_return_val_if_fail (ptr != NULL, FALSE);
  g_return_val_if_fail (*ptr != NULL, FALSE);
  g_return_val_if_fail (a != NULL, FALSE);
  g_return_val_if_fail (v != NULL, FALSE);

  /* [attribute=value,]* */

  *a = *ptr;
  end = p = g_utf8_strchr (*ptr, -1, ',');
  if (end) {
    do {
      end = g_utf8_next_char (end);
    } while (end && *end == ' ');
    *p = '\0';
  }

  *v = p = g_utf8_strchr (*ptr, -1, '=');
  if (*v) {
    *v = g_utf8_next_char (*v);
    *p = '\0';
  } else {
    debug_warning ("missing = after attribute\n");
    return FALSE;
  }

  *ptr = end;
  return TRUE;
}

static gint
_m3u8_compare_uri (GstM3U8 * a, gchar * uri)
{
  g_return_val_if_fail (a != NULL, 0);
  g_return_val_if_fail (uri != NULL, 0);

  return g_strcmp0 (a->uri, uri);
}

static gint
gst_m3u8_compare_playlist_by_bitrate (gconstpointer a, gconstpointer b)
{
  return ((GstM3U8 *) (a))->bandwidth - ((GstM3U8 *) (b))->bandwidth;
}

/*
 * @data: a m3u8 playlist text data, taking ownership
 */
static gboolean
gst_m3u8_update (GstM3U8 * self, gchar * data, gboolean * updated)
{
  gint val, duration;
  gchar *title, *end;
  gboolean discontinuity;
  GstM3U8 *list;
  gchar  *key_url = NULL;
  gchar *IV = NULL;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (updated != NULL, FALSE);

  *updated = TRUE;

  /* check if the data changed since last update */
  if (self->last_data && g_str_equal (self->last_data, data)) {
    g_print ("\n\n\n\t\t ############ Playlist is the same as previous one ############\n\n\n\n");
    *updated = FALSE;
    g_free (data);
    return TRUE;
  }

  if (!g_str_has_prefix (data, "#EXTM3U")) {
    debug_warning ("Data doesn't start with #EXTM3U\n");
    g_free (data);
    return FALSE;
  }

  /* playlist has changed from last time.. update last_data */
  g_free (self->last_data);
  self->last_data = data;

  if (self->files) {
    g_list_foreach (self->files, (GFunc) gst_m3u8_media_file_free, NULL);
    g_list_free (self->files);
    self->files = NULL;
  }

  list = NULL;
  duration = -1;
  title = NULL;
  key_url = NULL;
  data += 7;
  //data += 8;
  while (TRUE) {
	//g_print ("====================================\n");
	//g_print ("data = [%s]\n", data);
    end = g_utf8_strchr (data, -1, '\n');       /* FIXME: support \r\n */
    if (end)
      *end = '\0';

    //g_print ("end = [%s]\n", end);


    if (data[0] != '#') {
      if (duration < 0 && list == NULL) {
        debug_log ("%s: got line without EXTINF or EXTSTREAMINF, dropping\n", data);
        goto next_line;
      }

      if (!gst_uri_is_valid (data)) {
        gchar *slash;
        if (!self->uri) {
          debug_warning ("uri not set, can't build a valid uri\n");
          goto next_line;
        }
        slash = g_utf8_strrchr (self->uri, -1, '/');
        if (!slash) {
          debug_warning ("Can't build a valid uri\n");
          goto next_line;
        }

        *slash = '\0';
        data = g_strdup_printf ("%s/%s", self->uri, data);
        *slash = '/';
      } else
        data = g_strdup (data);

      if (list != NULL) {
        if (g_list_find_custom (self->lists, data,
                (GCompareFunc) _m3u8_compare_uri)) {
          debug_log ("Already have a list with this URI\n");
          gst_m3u8_free (list);
          g_free (data);
        } else {
          gst_m3u8_set_uri (list, data);
          self->lists = g_list_append (self->lists, list);
        }
        list = NULL;
      } else {
        GstM3U8MediaFile *file;
        gchar *send_IV = NULL;

        if (key_url)	
        {
          debug_log ("AES-128 key url = %s", key_url);
          if (NULL == IV)
          {
            /* IV is not present in EXT-X-KEY tag. Prepare IV based on mediasequence */
            debug_log ("IV is not in EXT-X-KEY tag... generating from media_seq_num = %d", self->mediasequence);
            send_IV = gst_m3u8_getIV_from_mediasequence (self);
          }
          else
          {
            send_IV = g_strdup (IV);   
          }
        }
  
        file =
            gst_m3u8_media_file_new (data, title, duration, key_url, send_IV, self->mediasequence++);
        duration = -1;
        title = NULL;
        g_free (send_IV);
        self->files = g_list_append (self->files, file);
      }

    } else if (g_str_has_prefix (data, "#EXT-X-ENDLIST")) {
      self->endlist = TRUE;
    } else if (g_str_has_prefix (data, "#EXT-X-VERSION:")) {
      if (int_from_string (data + 15, &data, &val, 10))
        self->version = val;
    } else if (g_str_has_prefix (data, "#EXT-X-STREAM-INF:")) {
      gchar *v, *a;

      if (list != NULL) {
        debug_warning ("Found a list without a uri..., dropping\n");
        gst_m3u8_free (list);
      }

      list = gst_m3u8_new ();
      data = data + 18;
      while (data && parse_attributes (&data, &a, &v)) {
        if (g_str_equal (a, "BANDWIDTH")) {
          if (!int_from_string (v, NULL, &list->bandwidth, 10))
            debug_warning ("Error while reading BANDWIDTH");
        } else if (g_str_equal (a, "PROGRAM-ID")) {
          if (!int_from_string (v, NULL, &list->program_id, 10))
            debug_warning ("Error while reading PROGRAM-ID");
        } else if (g_str_equal (a, "CODECS")) {
          g_free (list->codecs);
          list->codecs = g_strdup (v);
        } else if (g_str_equal (a, "RESOLUTION")) {
          if (!int_from_string (v, &v, &list->width, 10))
            debug_warning ("Error while reading RESOLUTION width");
          if (!v || *v != '=') {
            debug_warning ("Missing height\n");
          } else {
            v = g_utf8_next_char (v);
            if (!int_from_string (v, NULL, &list->height, 10))
              debug_warning ("Error while reading RESOLUTION height");
          }
        }
      }
    } else if (g_str_has_prefix (data, "#EXT-X-TARGETDURATION:")) {
      if (int_from_string (data + 22, &data, &val, 10))
        self->targetduration = val;
	// g_print ("\n\n\t\t#########################\n");
	 //g_print ("\t\tTarget duration = %d\n", val);
	// g_print ("\n\n\t\t#########################\n");

    } else if (g_str_has_prefix (data, "#EXT-X-MEDIA-SEQUENCE:")) {
      if (int_from_string (data + 22, &data, &val, 10))
        self->mediasequence = val;
    } else if (g_str_has_prefix (data, "#EXT-X-DISCONTINUITY")) {
      discontinuity = TRUE;
    } else if (g_str_has_prefix (data, "#EXT-X-PROGRAM-DATE-TIME:")) {
      /* <YYYY-MM-DDThh:mm:ssZ> */
      debug_log ("FIXME parse date\n");
    } else if (g_str_has_prefix (data, "#EXT-X-ALLOW-CACHE:")) {
      g_free (self->allowcache);
      self->allowcache = g_strdup (data + 19);
    } else if (g_str_has_prefix (data, "#EXTINF:")) {
      if (!int_from_string (data + 8, &data, &val, 10)) {
        debug_warning ("Can't read EXTINF duration\n");
        goto next_line;
      }
      duration = val;
      if (duration > self->targetduration)
        debug_warning ("EXTINF duration > TARGETDURATION\n");
      if (!data || *data != ',')
        goto next_line;
      data = g_utf8_next_char (data);
      if (data != end) {
        g_free (title);
        title = g_strdup (data);
      }
    }else if (g_str_has_prefix (data, "#EXT-X-KEY:")){
      gchar *val, *attr;

	
      g_print ("\n\n Found EXT-X-KEY tag...\n\n");
      /* handling encrypted content */
	  
      data = data + 11; /* skipping "#EXT-X-KEY:" tag */
	  
      while (data && parse_attributes (&data, &attr, &val)) 
      {
        if (g_str_equal (attr, "METHOD")) 
        {
          if (g_str_equal (val, "NONE"))
          {
            g_print ("Non encrypted file...and skipping current line and going to next line\n");
            goto next_line;
          }
          else if (g_str_equal (val, "AES-128"))
          {
            /* media files are encrypted */
            g_print ("media files are encrypted with AES-128");
            // TODO: indicate in flag whether encrypted files or not
          }
        } 
	 else if (g_str_equal (attr, "URI"))
	 {
	   gchar *end_dq = NULL;
	   
	   val = val + 1; /* eliminating first double quote in url */
	   
          end_dq = g_utf8_strrchr (val, -1, '"');
          *end_dq = '\0';

	   g_print ("Key URI = %s\n", val);

          if (!gst_uri_is_valid (val)) 
          {
            gchar *slash;
            if (!self->uri) 
            {
              debug_warning ("uri not set, can't build a valid uri");
              goto next_line;
            }
            slash = g_utf8_strrchr (self->uri, -1, '/');
            if (!slash) 
            {
              debug_warning ("Can't build a valid uri");
              goto next_line;
            }
            *slash = '\0';
            key_url = g_strdup_printf ("%s/%s", self->uri, val);
            *slash = '/';
          } 
          else
          {
            key_url= g_strdup (val);
          }
	 }
	 else if (g_str_equal (attr, "IV"))
	 {
	   gint iv_len = 0;
	   gchar tmp_byte[3];
	   gint tmp_val = 0;
	   gint idx = 0;

          if (IV)
          {
	     g_free (IV);
            IV = NULL;
          }
		  
          IV = g_malloc0 (16);
	   if (NULL == IV)
          {
            debug_error ("Failed to allocate memory...\n");
            return FALSE;
          }
	   
	   /* eliminating 0x/0X prefix */
	   val = val + 2;
          iv_len = g_utf8_strlen(val, 0);
          if (iv_len != 16)
          {
            debug_warning ("Wrong IV...");
            return FALSE;
          }
	   
	   while (iv_len)
	   {
	   	// TODO: val need to incremented I feel.. check again 
            g_utf8_strncpy(tmp_byte, val, 2);
            tmp_byte[2] = '\0';
            tmp_val = 0;
            if (!int_from_string (tmp_byte, NULL, &tmp_val, 16))
              debug_warning ("Error while reading PROGRAM-ID");                    
	     IV[idx] = tmp_val;	
	     idx++;
            iv_len = iv_len - 2;
            val = val + 2;
	   }
	 }
      }

#if 0	  
      if (g_str_has_prefix (data, "METHOD="))
      {
        data = data + 7;
        if (g_str_has_prefix (data, "AES-128"))
        {
          g_print ("AES-128 encrypted media...\n\n");
          data = data + 8;
          if (g_str_has_prefix (data, "URI="))
          {
            gchar *dob_qu = NULL;
            gchar  *tmp_key_url = NULL;

            data = data+5;
			
            dob_qu = g_utf8_strrchr (data, -1, '"');
            *dob_qu = '\0';
			
            tmp_key_url = g_strdup (data);
            *dob_qu = '"';	
            
            g_print ("URI attribute = %s\n\n", tmp_key_url);
			
            if (!gst_uri_is_valid (tmp_key_url)) 
            {
              gchar *slash;
              if (!self->uri) {
                debug_warning ("uri not set, can't build a valid uri");
                goto next_line;
              }
              slash = g_utf8_strrchr (self->uri, -1, '/');
              if (!slash) 
              {
                debug_warning ("Can't build a valid uri");
                goto next_line;
              }

              *slash = '\0';
              key_url = g_strdup_printf ("%s/%s", self->uri, tmp_key_url);
              *slash = '/';
            } 
            else
              key_url = g_strdup (tmp_key_url);

            g_print ("\n\n======= Final key url = %s\n\n\n\n", key_url); 

            data = dob_qu;
	     data = data + 2;
		 
            if ((g_str_has_prefix (data, "IV=0x")) && (g_str_has_prefix (data, "IV=0X")))
            {
              data = data + 5;
              g_print ("\n\nSize of IV = %d\n\n", sizeof (data));
              memcpy (IV, data, sizeof (IV));
            }
            else
            {
              g_print ("\n\n\n Need to generate IV from media sequence...\n\n");
            }
          }
          else
          {
            g_print ("No URI specified...\n\n");
            return FALSE;
          }
        }
        else if (g_str_has_prefix (data, "NONE"))
        {
          g_print ("\n\nNot encrypted.....\n\n\n");
        }
        else
        {
          g_print ("\n\nUnknown EXT-X-KEY METHOD attri = %s\n\n", data);
          return FALSE;
        }
      }

      else
      {
        g_print ("\n\nEXT-X-KEY without METHOD attribute...\n\n");
        return FALSE;
      }
#endif
    }
    else {
      debug_warning ("Ignored line: %s", data);
    }

  next_line:
    if (!end)
      break;
    data = g_utf8_next_char (end);      /* skip \n */
  }

  /* redorder playlists by bitrate */
  if (self->lists)
    self->lists =
        g_list_sort (self->lists,
        (GCompareFunc) gst_m3u8_compare_playlist_by_bitrate);

  return TRUE;
}

GstM3U8Client *
gst_m3u8_client_new (const gchar * uri)
{
  GstM3U8Client *client;

  g_return_val_if_fail (uri != NULL, NULL);

  client = g_new0 (GstM3U8Client, 1);
  client->main = gst_m3u8_new ();
  client->current = NULL;
  client->sequence = -1;
  client->update_failed_count = 0;
  gst_m3u8_set_uri (client->main, g_strdup (uri));

  return client;
}

void
gst_m3u8_client_free (GstM3U8Client * self)
{
  g_return_if_fail (self != NULL);

  gst_m3u8_free (self->main);
  g_free (self);
}

void
gst_m3u8_client_set_current (GstM3U8Client * self, GstM3U8 * m3u8)
{
  g_return_if_fail (self != NULL);

  if (m3u8 != self->current) {
    self->current = m3u8;
    self->update_failed_count = 0;
  }
}

gboolean
gst_m3u8_client_update (GstM3U8Client * self, gchar * data)
{
  GstM3U8 *m3u8;
  gboolean updated = FALSE;

  g_return_val_if_fail (self != NULL, FALSE);

  m3u8 = self->current ? self->current : self->main;

 // g_print ("\n\n");

  if (!gst_m3u8_update (m3u8, data, &updated))
    return FALSE;

 // g_print ("\n\n");

  if (!updated) {
    self->update_failed_count++;
    return FALSE;
  }

  /* select the first playlist, for now */
  if (!self->current) {
    if (self->main->lists) {
      self->current = g_list_first (self->main->lists)->data;
    } else {
      self->current = self->main;
    }
  }

  if (m3u8->files && self->sequence == -1) {
    self->sequence =
        GST_M3U8_MEDIA_FILE (g_list_first (m3u8->files)->data)->sequence;
     debug_log ("Setting first sequence at %d", self->sequence);
  }

  return TRUE;
}

static gboolean
_find_next (GstM3U8MediaFile * file, GstM3U8Client * client)
{
  debug_log ("Found fragment %d\n", file->sequence);
  if (file->sequence >= client->sequence)
    return FALSE;
  return TRUE;
}

const GstM3U8MediaFile *
gst_m3u8_client_get_next_fragment (GstM3U8Client * client,
    gboolean * discontinuity)
{
  GList *l;
  GstM3U8MediaFile *file;

  g_return_val_if_fail (client != NULL, NULL);
  g_return_val_if_fail (client->current != NULL, NULL);
  g_return_val_if_fail (discontinuity != NULL, NULL);

  debug_log ("Looking for fragment %d\n", client->sequence);
  l = g_list_find_custom (client->current->files, client,
      (GCompareFunc) _find_next);
  if (l == NULL)
    return NULL;

  file = GST_M3U8_MEDIA_FILE (l->data);

  *discontinuity = client->sequence != file->sequence;
  client->sequence = file->sequence + 1;

  return file;
}


const gboolean
gst_m3u8_client_check_next_fragment (GstM3U8Client * client)
{
  gint left_duration = 0;

  g_return_val_if_fail (client != NULL, FALSE);
  g_return_val_if_fail (client->current != NULL, FALSE);

  GList* cur = g_list_last (client->current->files);
  while (cur && GST_M3U8_MEDIA_FILE(cur->data)->sequence >= client->sequence)
  {
	  left_duration += GST_M3U8_MEDIA_FILE(cur->data)->duration;
	  cur = g_list_previous (cur);
  }

  debug_log ("left duration = [%d], target duration[%d] * 3 = [%d]\n",
		  left_duration, client->current->targetduration, client->current->targetduration*3);

  return (left_duration > client->current->targetduration*3)? TRUE : FALSE;
  
}
