/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-vala-window-plugin.vala
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

using GLib;
using Cairo;
using CCM;

namespace CCM
{
    enum Options
    {
        ENABLED,
        N
    }

    class ValaWindowOptions : PluginOptions
    {
        public bool enabled = false;

        public override void
        changed(CCM.Config config)
        {
            try
            {
                enabled = config.get_boolean ();
            }
            catch (GLib.Error err)
            {
                CCM.log("%s", err);
            }
        }
    }

    private class ValaWindowPlugin : CCM.Plugin, CCM.WindowPlugin
    {
        const string[] options_key = {
            "enabled"
        };

        weak CCM.Window window;

        uint counter = 0;

        class construct
        {
            type_options = typeof (ValaWindowOptions);
        }

        ~ValaWindowPlugin ()
        {
            options_unload ();
        }

        void
        option_changed (int index)
        {
            if (!((ValaWindowOptions) get_option ()).enabled)
                window.get_screen ().damage_all ();
        }

        /**
         * Implement load_options window plugin interface
         **/
        void
        window_load_options (CCM.Window window)
        {
            this.window = window;

            options_load ("vala-window-plugin", options_key, 
                          (PluginOptionsChangedFunc)option_changed);

            /* Chain call to next plugin */
            ((CCM.WindowPlugin) parent).window_load_options (window);
        }

        /**
         * Implement paint window plugin interface
         **/
        bool 
        window_paint (CCM.Window window, Cairo.Context ctx, Cairo.Surface surface)
        {
            bool ret = false;

            /* Chain call to next plugin */
            ret = ((CCM.WindowPlugin) parent).window_paint (window, ctx, surface);

            /* Paint damaged area */
            if (((ValaWindowOptions) get_option ()).enabled)
            {
                weak CCM.Region damaged = window.damaged;

                if (damaged != null)
                {
                    unowned Cairo.Rectangle[] rectangles;

                    damaged.get_rectangles (out rectangles);

                    switch (counter)
                    {
                        case 0:
                            ctx.set_source_rgba (1, 0, 0, 0.5);
                            break;
                        case 1:
                            ctx.set_source_rgba (0, 1, 0, 0.5);
                            break;
                        case 2:
                            ctx.set_source_rgba (0, 0, 1, 0.5);
                            break;
                        default:
                            break;
                    }
                    if (++counter > 2)
                        counter = 0;

                    foreach (Cairo.Rectangle rectangle in rectangles)
                    {
                        ctx.rectangle (rectangle.x, rectangle.y,
                                       rectangle.width, rectangle.height);
                        ctx.fill ();
                    }
                    rectangles_free (rectangles);
                }
            }

            return ret;
        }
    }
}

/**
 * Init plugin
 **/
[ModuleInit]
public Type
ccm_vala_window_plugin_get_plugin_type (TypeModule module)
{
    return typeof (ValaWindowPlugin);
}
