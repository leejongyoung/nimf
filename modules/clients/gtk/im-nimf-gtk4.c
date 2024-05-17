/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */
/*
 * im-nimf-gtk4.c
 * This file is part of Nimf and originally based on code from im-nimf.c
 *
 * Copyright (C) 2024 Gooroom <gooroom@gooroom.kr>
 *
 * Nimf is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nimf is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program;  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <gtk/gtkimmodule.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <nimf.h>
#include <X11/XKBlib.h>

#include <gdk/x11/gdkx.h>


#define NIMF_GTK_TYPE_IM_CONTEXT  (nimf_gtk_im_context_get_type ())
#define NIMF_GTK_IM_CONTEXT(obj)  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NIMF_GTK_TYPE_IM_CONTEXT, NimfGtkIMContext))

typedef struct _NimfGtkIMContext      NimfGtkIMContext;
typedef struct _NimfGtkIMContextClass NimfGtkIMContextClass;

struct _NimfGtkIMContext
{
  GtkIMContext  parent_instance;

  NimfIM       *im;
  GtkIMContext *simple;
  GtkWidget    *client_window;
};

struct _NimfGtkIMContextClass
{
  GtkIMContextClass parent_class;
};

G_DEFINE_DYNAMIC_TYPE (NimfGtkIMContext, nimf_gtk_im_context, GTK_TYPE_IM_CONTEXT);

static NimfEvent *
translate_gdk_event_key (GdkEvent *event)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfEvent *nimf_event = nimf_event_new (NIMF_EVENT_NOTHING);

  if (gdk_event_get_event_type (event) == GDK_KEY_PRESS)
    nimf_event->key.type = NIMF_EVENT_KEY_PRESS;
  else
    nimf_event->key.type = NIMF_EVENT_KEY_RELEASE;

  nimf_event->key.state  = gdk_event_get_modifier_state (event);
  nimf_event->key.keyval = gdk_key_event_get_keyval (event);
  nimf_event->key.hardware_keycode = gdk_key_event_get_keycode (event);

  return nimf_event;
}

static gboolean
nimf_gtk_im_context_filter_keypress (GtkIMContext *context,
                                     GdkEvent     *event)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  gboolean   retval;
  NimfEvent *nimf_event;

  nimf_event = translate_gdk_event_key (event);
  retval = nimf_im_filter_event (NIMF_GTK_IM_CONTEXT (context)->im, nimf_event);
  nimf_event_free (nimf_event);

  if (retval == FALSE)
    return gtk_im_context_filter_keypress (NIMF_GTK_IM_CONTEXT (context)->simple, event);

  return retval;
}

static void
nimf_gtk_im_context_reset (GtkIMContext *context)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  nimf_im_reset (NIMF_GTK_IM_CONTEXT (context)->im);
  gtk_im_context_reset (NIMF_GTK_IM_CONTEXT (context)->simple);
}

static void
nimf_gtk_im_context_set_client_widget (GtkIMContext *context,
                                       GtkWidget    *client)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfGtkIMContext *nimf_context = NIMF_GTK_IM_CONTEXT (context);

  if (nimf_context->client_window)
  {
    g_object_unref (nimf_context->client_window);
    nimf_context->client_window = NULL;
  }

  if (client)
    nimf_context->client_window = g_object_ref (client);

  if (nimf_context->simple)
    gtk_im_context_set_client_widget (nimf_context->simple, client);
}

static void
nimf_gtk_im_context_get_preedit_string (GtkIMContext   *context,
                                        gchar         **str,
                                        PangoAttrList **attrs,
                                        gint           *cursor_pos)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfPreeditAttr **preedit_attrs;
  gchar *preedit_str;

  nimf_im_get_preedit_string (NIMF_GTK_IM_CONTEXT (context)->im,
                              &preedit_str, &preedit_attrs, cursor_pos);

  if (str)
    *str = preedit_str;

  if (attrs)
  {
    PangoAttribute *attr;
    gchar          *ptr1;
    gchar          *ptr2;
    gint            i;

    *attrs = pango_attr_list_new ();

    for (i = 0; preedit_attrs[i] != NULL; i++)
    {
      ptr1 = g_utf8_offset_to_pointer (preedit_str, preedit_attrs[i]->start_index);
      ptr2 = g_utf8_offset_to_pointer (preedit_str, preedit_attrs[i]->end_index);

      switch (preedit_attrs[i]->type)
      {
        case NIMF_PREEDIT_ATTR_UNDERLINE:
          attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
          break;
        case NIMF_PREEDIT_ATTR_HIGHLIGHT:
          attr = pango_attr_background_new (0, 0xffff, 0);
          attr->start_index = ptr1 - preedit_str;
          attr->end_index   = ptr2 - preedit_str;
          pango_attr_list_insert (*attrs, attr);

          attr = pango_attr_foreground_new (0, 0, 0);
          break;
        default:
          attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
          break;
      }

      attr->start_index = ptr1 - preedit_str;
      attr->end_index   = ptr2 - preedit_str;
      pango_attr_list_insert (*attrs, attr);
    }
  }

  nimf_preedit_attr_freev (preedit_attrs);
}

static void
nimf_gtk_im_context_focus_in (GtkIMContext *context)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfGtkIMContext *nimf_context = NIMF_GTK_IM_CONTEXT (context);
  nimf_im_focus_in (nimf_context->im);
}

static void
nimf_gtk_im_context_focus_out (GtkIMContext *context)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfGtkIMContext *nimf_context = NIMF_GTK_IM_CONTEXT (context);
  nimf_im_focus_out (nimf_context->im);
}

static void
nimf_gtk_im_context_set_cursor_location (GtkIMContext *context,
                                         GdkRectangle *area)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GdkRectangle root_area = *area;

  nimf_im_set_cursor_location (NIMF_GTK_IM_CONTEXT (context)->im,
                               (const NimfRectangle *) &root_area);
}

static void
nimf_gtk_im_context_set_use_preedit (GtkIMContext *context,
                                     gboolean      use_preedit)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  nimf_im_set_use_preedit (NIMF_GTK_IM_CONTEXT (context)->im, use_preedit);
}

static void
nimf_gtk_im_context_set_surrounding (GtkIMContext *context,
                                     const char   *text,
                                     gint          len,
                                     gint          cursor_index)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  nimf_im_set_surrounding (NIMF_GTK_IM_CONTEXT (context)->im,
                           text, len, g_utf8_strlen (text, cursor_index));
}

static void
on_commit (NimfIM           *im,
           const gchar      *text,
           NimfGtkIMContext *context)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  g_signal_emit_by_name (context, "commit", text);
}

static gboolean
on_delete_surrounding (NimfIM           *im,
                       gint              offset,
                       gint              n_chars,
                       NimfGtkIMContext *context)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  gboolean retval;
  g_signal_emit_by_name (context,
                         "delete-surrounding", offset, n_chars, &retval);
  return retval;
}

static void
on_preedit_changed (NimfIM           *im,
                    NimfGtkIMContext *context)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);
  g_signal_emit_by_name (context, "preedit-changed");
}

static void
on_preedit_end (NimfIM           *im,
                NimfGtkIMContext *context)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);
  g_signal_emit_by_name (context, "preedit-end");
}

static void
on_preedit_start (NimfIM           *im,
                  NimfGtkIMContext *context)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);
  g_signal_emit_by_name (context, "preedit-start");
}

static gboolean
on_retrieve_surrounding (NimfIM           *im,
                         NimfGtkIMContext *context)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  gboolean retval;
  g_signal_emit_by_name (context, "retrieve-surrounding", &retval);

  return retval;
}

static void
on_beep (NimfIM           *im,
         NimfGtkIMContext *context)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  gdk_display_beep (gdk_display_get_default ());
}

static void
nimf_gtk_im_context_init (NimfGtkIMContext *context)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  context->im = nimf_im_new ();
  context->simple = gtk_im_context_simple_new ();

  g_signal_connect (context->im, "commit",
                    G_CALLBACK (on_commit), context);
  g_signal_connect (context->im, "delete-surrounding",
                    G_CALLBACK (on_delete_surrounding), context);
  g_signal_connect (context->im, "preedit-changed",
                    G_CALLBACK (on_preedit_changed), context);
  g_signal_connect (context->im, "preedit-end",
                    G_CALLBACK (on_preedit_end), context);
  g_signal_connect (context->im, "preedit-start",
                    G_CALLBACK (on_preedit_start), context);
  g_signal_connect (context->im, "retrieve-surrounding",
                    G_CALLBACK (on_retrieve_surrounding), context);
  g_signal_connect (context->im, "beep",
                    G_CALLBACK (on_beep), context);

  g_signal_connect (context->simple, "commit",
                    G_CALLBACK (on_commit), context);
  g_signal_connect (context->simple, "delete-surrounding",
                    G_CALLBACK (on_delete_surrounding), context);
  g_signal_connect (context->simple, "preedit-changed",
                    G_CALLBACK (on_preedit_changed), context);
  g_signal_connect (context->simple, "preedit-end",
                    G_CALLBACK (on_preedit_end), context);
  g_signal_connect (context->simple, "preedit-start",
                    G_CALLBACK (on_preedit_start), context);
  g_signal_connect (context->simple, "retrieve-surrounding",
                    G_CALLBACK (on_retrieve_surrounding), context);
}

static void
nimf_gtk_im_context_finalize (GObject *object)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfGtkIMContext *nimf_context = NIMF_GTK_IM_CONTEXT (object);

  g_object_unref (nimf_context->im);
  g_object_unref (nimf_context->simple);

  if (nimf_context->client_window)
    g_object_unref (nimf_context->client_window);

  if (nimf_context->simple) {
      g_object_unref (nimf_context->simple);
      nimf_context->simple = NULL;
  }

  G_OBJECT_CLASS (nimf_gtk_im_context_parent_class)->finalize (object);
}

static void
nimf_gtk_im_context_class_init (NimfGtkIMContextClass *class)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (class);

  im_context_class->set_client_widget   = nimf_gtk_im_context_set_client_widget;
  im_context_class->get_preedit_string  = nimf_gtk_im_context_get_preedit_string;
  im_context_class->filter_keypress     = nimf_gtk_im_context_filter_keypress;
  im_context_class->focus_in            = nimf_gtk_im_context_focus_in;
  im_context_class->focus_out           = nimf_gtk_im_context_focus_out;
  im_context_class->reset               = nimf_gtk_im_context_reset;
  im_context_class->set_cursor_location = nimf_gtk_im_context_set_cursor_location;
  im_context_class->set_use_preedit     = nimf_gtk_im_context_set_use_preedit;
  im_context_class->set_surrounding     = nimf_gtk_im_context_set_surrounding;

  object_class->finalize = nimf_gtk_im_context_finalize;
}

static void
nimf_gtk_im_context_class_finalize (NimfGtkIMContextClass *class)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);
}

G_MODULE_EXPORT void g_io_module_load (GIOModule *module)
{
	g_type_module_use (G_TYPE_MODULE (module));
	nimf_gtk_im_context_register_type (G_TYPE_MODULE (module));
	g_io_extension_point_implement (GTK_IM_MODULE_EXTENSION_POINT_NAME,
                                    NIMF_GTK_TYPE_IM_CONTEXT,
                                    "nimf",
                                    50);
}

G_MODULE_EXPORT void g_io_module_unload (GIOModule *module)
{
	g_type_module_unuse (G_TYPE_MODULE (module));
}
