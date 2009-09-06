/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2009 <gandalfn@club-internet.fr>
 * 
 * cairo-compmgr is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * cairo-compmgr is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with cairo-compmgr.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

using GLib;
using Cairo;
using CCM;
using X;
using Gee;

namespace CCM
{
    public class Output
    {
        public weak CCM.Window window;
        public CCM.Pixmap pixmap;

        public Output (CCM.Screen screen, CCM.Window window, X.Pixmap xpixmap,
                int depth)
        {
            X.Visual * visual = screen.get_visual_for_depth (depth);

            this.window = window;
            this.pixmap = new CCM.Pixmap.from_visual (screen, *visual, xpixmap);
            this.pixmap.foreign = true;
        }
    }

    class Clone : CCM.Plugin, CCM.ScreenPlugin, CCM.WindowPlugin
    {
        class X.Atom screen_enable_atom;
        class X.Atom screen_disable_atom;
        class X.Atom enable_atom;
        class X.Atom disable_atom;

        weak CCM.Screen screen;
        ArrayList < Output > screen_outputs = null;
        ArrayList < Output > window_outputs = null;

        private void add_screen_output (Output output)
        {
            if (screen_outputs == null)
                screen_outputs = new ArrayList < Output > ();
            screen_outputs.add (output);

            foreach (CCM.Window window in screen.get_windows ())
            {
                if (window != output.window)
                {
                    var clone = (Clone) window.get_plugin (typeof (Clone));

                    if (clone.screen_outputs == null)
                        clone.screen_outputs = new ArrayList < Output > ();

                     clone.screen_outputs.add (output);
                }
            }
        }

        private void remove_screen_output (Output output)
        {
            screen_outputs.remove (output);
            foreach (CCM.Window window in screen.get_windows ())
            {
                var clone = (Clone) window.get_plugin (typeof (Clone));
                clone.screen_outputs.remove (output);
            }
        }

        private void on_composite_message (CCM.Window client, CCM.Window window, 
                                           long l1, long l2, long l3)
        {
            X.Atom atom = (X.Atom) l1;
            X.Pixmap xpixmap = (X.Pixmap) l2;
            int depth = (int) l3;

            if (atom == enable_atom)
            {
                CCM.log ("ENABLE CLONE");
                var clone = (Clone) window.get_plugin (typeof (Clone));
                var output =
                    new Output (window.get_screen (), window, xpixmap, depth);
                if (clone.window_outputs == null)
                    clone.window_outputs = new ArrayList < Output > ();
                clone.window_outputs.add (output);
				client.no_undamage_sibling = true;
                window.damage ();
            }
            else if (atom == disable_atom)
            {
                CCM.log ("DISABLE CLONE");
                var clone = (Clone) window.get_plugin (typeof (Clone));
				client.no_undamage_sibling = false;

                foreach (Output output in clone.window_outputs)
                {
                    if (output.pixmap.get_xid () == (X.ID) xpixmap)
                    {
                        clone.window_outputs.remove (output);
                        break;
                    }
                }
            }
            else if (atom == screen_enable_atom)
            {
                CCM.log ("ENABLE SCREEN CLONE");
                var output =
                    new Output (window.get_screen (), window, xpixmap, depth);
				window.no_undamage_sibling = true;
                add_screen_output (output);
            }
            else if (atom == screen_disable_atom)
            {
                CCM.log ("DISABLE SCREEN CLONE");
                foreach (Output output in screen_outputs)
                {
                    if (output.pixmap.get_xid () == (X.ID) xpixmap)
                    {
						output.window.no_undamage_sibling = false;
                        remove_screen_output (output);
                        break;
                    }
                }
            }
        }

        /**
		 * Implement load_options screen plugin interface
		 **/
        protected void screen_load_options (CCM.Screen screen)
        {
            this.screen = screen;
            this.screen.composite_message += on_composite_message;
            if (screen_enable_atom == 0)
            {
                screen_enable_atom =
                    screen.get_display ().get_xdisplay ().
                    intern_atom ("_CCM_CLONE_SCREEN_ENABLE", false);
                screen_disable_atom =
                    screen.get_display ().get_xdisplay ().
                    intern_atom ("_CCM_CLONE_SCREEN_DISABLE", false);
                enable_atom =
                    screen.get_display ().get_xdisplay ().
                    intern_atom ("_CCM_CLONE_ENABLE", false);
                disable_atom =
                    screen.get_display ().get_xdisplay ().
                    intern_atom ("_CCM_CLONE_DISABLE", false);
            }
            ((CCM.ScreenPlugin) parent).screen_load_options (screen);
        }

        /**
		 * Implement paint window plugin interface
		 **/
        protected bool window_paint (CCM.Window window, Cairo.Context context,
                                     Cairo.Surface surface, bool y_invert)
        {
            bool ret = false;

            /* Chain call to next plugin */
            ret = ((CCM.WindowPlugin) parent).window_paint (window, context,
                                                            surface, y_invert);

            if (((window_outputs != null && window_outputs.size > 0) ||
                 (screen_outputs != null && screen_outputs.size > 0)) && ret)
            {
                var area = window.get_area ();
                Cairo.Rectangle geometry = Cairo.Rectangle ();

                if (area != null && window.get_device_geometry_clipbox (out geometry))
                {
                    if (window_outputs != null)
                    {
                        foreach (Output output in window_outputs)
                        {
                            Cairo.Rectangle clipbox = Cairo.Rectangle ();

                            if (output.pixmap.
                                get_device_geometry_clipbox (out clipbox))
                            {
                                Cairo.Context ctx =
                                    output.pixmap.create_context ();

                                if (ctx != null)
                                {
                                    ctx.scale (clipbox.width / area->width,
                                               clipbox.height / area->height);
                                    ctx.translate (-area->x, -area->y);
                                    window.get_damage_path (ctx);
                                    ctx.clip ();
                                    ctx.translate (area->x, area->y);
                                    ctx.set_source_surface (surface, 
                                                            -(geometry.width - area->width) / 2.0,
                                                            -(geometry.height - area->height) / 2.0);
                                    ctx.paint ();
                                }
                            }
                        }
                    }

                    if (screen_outputs != null)
                    {
                        foreach (Output output in screen_outputs)
                        {
                            Cairo.Rectangle clipbox = Cairo.Rectangle ();
                            int width = window.get_screen ().get_xscreen ().width;
                            int height = window.get_screen ().get_xscreen ().height;

                            if (output.window != window && 
                                output.pixmap.get_device_geometry_clipbox (out clipbox))
                            {
                                Cairo.Context ctx = output.pixmap.create_context ();

                                if (ctx != null)
                                {
                                    Cairo.Matrix matrix;
									
                                    matrix = Cairo.Matrix (clipbox.width / width, 0, 0,
                                                           clipbox.height / height,
                                                           -geometry.x * (1 - clipbox.width / width),
                                                           -geometry.y * (1 - clipbox.height / height));
                                    ctx.set_matrix (matrix);
                                    window.get_damage_path (ctx);
                                    ctx.clip ();
                                    ctx.identity_matrix ();
                                    window.push_matrix ("CCMClone", matrix);
                                    ((CCM.WindowPlugin) parent).window_paint (window, ctx, 
                                                                              surface, y_invert);
                                    window.pop_matrix ("CCMClone");
                                }
                            }
                        }
                    }
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
ccm_clone_get_plugin_type (TypeModule module)
{
    return typeof (Clone);
}
