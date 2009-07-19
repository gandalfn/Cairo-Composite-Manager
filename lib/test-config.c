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

#include "ccm-config.h"

GSList *plugins = NULL;
gboolean r = FALSE;

gboolean
on_timeout (CCMConfig * config)
{
    GSList *item;

    plugins = ccm_config_get_string_list (config, NULL);
    for (item = plugins; item; item = item->next)
    {
        g_print ("%s,", (gchar *) item->data);
    }
    g_print ("\n");
    if (r)
        plugins = g_slist_remove_link (plugins, plugins);
    else
        plugins = g_slist_prepend (plugins, g_strdup ("vala-window-plugin"));
    r = !r;

    ccm_config_set_string_list (config, plugins, NULL);
    g_slist_foreach (plugins, (GFunc) g_free, NULL);
    g_slist_free (plugins);

    return TRUE;
}

void
on_config_changed (CCMConfig * config)
{
    GError *error = NULL;

    gchar *value = ccm_config_get_string (config, &error);
    if (value)
    {
        gchar *key;

        g_object_get (config, "key", &key, NULL);
        g_print ("%s changed new value = %s\n", key, value);
        g_free (key);
        g_free (value);
    }
    else
    {
        g_warning ("Error on get value: %s", error->message);
        g_error_free (error);
    }
}

int
main (gint argc, gchar ** argv)
{
    CCMConfig *config, *other, *plugin;
    GMainLoop *loop;

    g_type_init ();


    loop = g_main_loop_new (NULL, FALSE);

    ccm_config_set_backend ("key");

    config = ccm_config_new (-1, NULL, "enable");
    g_signal_connect (config, "changed", G_CALLBACK (on_config_changed), NULL);
//    config = ccm_config_new(0, NULL, "backend");
//    g_signal_connect(config, "changed", G_CALLBACK(on_config_changed), NULL);
    other = ccm_config_new (0, NULL, "plugins");
    GSList *item;
    plugins = ccm_config_get_string_list (other, NULL);
    for (item = plugins; item; item = item->next)
    {
        g_print ("%s\n", (gchar *) item->data);
        g_free (item->data);
    }
    g_slist_free (plugins);
    //g_timeout_add_seconds(2, on_timeout, other);
    g_signal_connect (other, "changed", G_CALLBACK (on_config_changed), NULL);
    plugin = ccm_config_new (0, "fade", "duration");
    g_signal_connect (plugin, "changed", G_CALLBACK (on_config_changed), NULL);

    g_main_loop_run (loop);

    return 0;
}
