/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include <gtk/gtk.h>
#include "ccm-config.h"

gint
main (gint argc, gchar ** argv)
{
    GtkWidget *window;
    GtkBuilder *builder;
    GError *error = NULL;

    gtk_init (&argc, &argv);

    ccm_config_set_backend ("key");

    builder = gtk_builder_new ();
    if (!gtk_builder_add_from_file (builder, "./test-config-widget.ui", &error))
    {
        g_warning ("Error %s", error->message);
        g_error_free (error);
        return 1;
    }

    window = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));
    gtk_widget_show (window);
    gtk_main ();

    return 0;
}
