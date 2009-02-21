/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with main.c; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include <gtk/gtk.h>

#include "config.h"
#include "ccm-config-schema.h"

gint 
main(gint argc, gchar **argv)
{
    CCMConfigSchema* schema;
    gchar* schema_key = NULL, *schema_gconf = NULL;
    gboolean plugin = FALSE;
    GOptionEntry options[] = 
    {
		{ "plugin", 'p', 0, G_OPTION_ARG_NONE, &plugin,
 		  "Indicate if schema file is for plugin",
 		  NULL },
		{ "schema-key", 'k', 0, G_OPTION_ARG_STRING, &schema_key,
 		  "Schema key file",
 		  "file.schema-key" },
		{ "schema-gconf", 'g', 0, G_OPTION_ARG_STRING, &schema_gconf,
 		  "Schema gconf file",
 		  "file.schemas" },
		{ NULL, '\0', 0, 0, NULL, NULL, NULL }
 	};
    GOptionContext*	option_context = NULL;

    g_type_init();
    
    option_context = g_option_context_new ("- ccm-schema-key-to-gconf");
	if (!option_context)
	{
		g_warning("Error on parse args");
		return -1;
	}
	g_option_context_add_main_entries (option_context, options,
					   				   "ccm-schema-key-to-gconf");
	g_option_context_parse (option_context, &argc, &argv, NULL);
	g_option_context_free (option_context);

    if (schema_key && schema_gconf)
    {
        schema = ccm_config_schema_new_from_file (plugin ? 0 : -1, schema_key);
        if (schema)
            ccm_config_schema_write_gconf (schema, schema_gconf);
    }   

    return 0;
}