/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-snapshot-main.c
 * Copyright (C) Nicolas Bruguier 2007-2011 <gandalfn@club-internet.fr>
 *
 * cairo-compmgr is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * cairo-compmgr is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "ccm-snapshot-dialog.h"

int
main (gint argc, gchar ** argv)
{
    GError *error = NULL;
    static gint xid = 0;
    static gint screen_num = 0;

    GOptionContext *option_context;
    GOptionEntry options[] = {
        {"xid", 'x', 0, G_OPTION_ARG_INT, &xid, "Xid of snapshot pixmap", NULL},
        {"screen", 'x', 0, G_OPTION_ARG_INT, &screen_num, "Screen number of snapshot pixmap", NULL},
        {NULL, '\0', 0, 0, NULL, NULL, NULL}
    };

    g_type_init ();

    option_context = g_option_context_new ("- CCM Snapshot");
    g_option_context_add_group (option_context, gtk_get_option_group (TRUE));
    g_option_context_add_main_entries (option_context, options, "CCMSnapshot");

    if (!g_option_context_parse (option_context, &argc, &argv, &error))
    {
        g_print ("%s\n", error->message);
        return 1;
    }

    if (xid)
    {
        GdkDisplay* display = gdk_display_get_default ();
        GdkScreen* screen = gdk_display_get_screen (display, screen_num);
        GdkPixmap* pixmap = gdk_pixmap_foreign_new ((GdkNativeWindow)xid);
        if (pixmap)
        {
            gdk_drawable_set_colormap (GDK_DRAWABLE (pixmap), gdk_screen_get_default_colormap (screen));
            cairo_t* ctx = gdk_cairo_create (GDK_DRAWABLE (pixmap));
            CCMSnapshotDialog* dialog = ccm_snapshot_dialog_new (cairo_get_target (ctx), screen_num);
            if (dialog)
            {
                gtk_main ();
            }

            g_object_unref (pixmap);
        }
    }

    
    g_option_context_free(option_context);

    return 0;
}
