/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007-2010 <gandalfn@club-internet.fr>
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
using Vala;

namespace CCM
{
    public class Output
    {
        public weak CCM.Window client;
        public weak CCM.Window window;
        public CCM.Pixmap pixmap;
        public bool paint_parent;
        public int x;
        public int y;
        public int max_width;
        public int max_height;
        public int scale_x;
        public int scale_y;

        public Output (CCM.Screen screen, CCM.Window client, CCM.Window window,
                       X.Pixmap xpixmap, int depth)
        {
            X.Visual * visual = screen.get_visual_for_depth (depth);

            this.client = client;
            this.window = window;
            this.pixmap = new CCM.Pixmap.from_visual (screen, *visual, xpixmap);
            this.pixmap.foreign = true;
            this.paint_parent = true;
            this.x = 0;
            this.y = 0;
            this.max_width = 0;
            this.max_height = 0;
            this.scale_x = 0;
            this.scale_y = 0;
        }
    }

    class Clone : CCM.Plugin, CCM.ScreenPlugin, CCM.WindowPlugin
    {
        class X.Atom screen_enable_atom;
        class X.Atom screen_disable_atom;
        class X.Atom enable_atom;
        class X.Atom disable_atom;
        class X.Atom paint_parent_atom;
        class X.Atom offset_x_atom;
        class X.Atom offset_y_atom;
        class X.Atom max_width_atom;
        class X.Atom max_height_atom;
        class X.Atom scale_x_atom;
        class X.Atom scale_y_atom;

        weak CCM.Screen screen;
        ArrayList < Output > screen_outputs = null;
        ArrayList < Output > window_outputs = null;

        void
        add_screen_output (Output output)
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

        void 
        remove_screen_output (Output output)
        {
            screen_outputs.remove (output);
            foreach (CCM.Window window in screen.get_windows ())
            {
                var clone = (Clone) window.get_plugin (typeof (Clone));
                clone.screen_outputs.remove (output);
            }
        }

        void
        on_composite_message (CCM.Window client, CCM.Window window, 
                              long l1, long l2, long l3)
        {
            X.Atom atom = (X.Atom) l1;
            X.Pixmap xpixmap = (X.Pixmap) l2;

            if (atom == enable_atom)
            {
                int depth = (int) l3;

                CCM.log ("ENABLE CLONE");
                var clone = (Clone) window.get_plugin (typeof (Clone));
                var output =
                    new Output (window.get_screen (), client, window, xpixmap, depth);
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
            else if (atom == offset_x_atom)
            {
                CCM.log ("OFFSET X CLONE %i", (int)l3);
                var clone = (Clone) window.get_plugin (typeof (Clone));

                foreach (Output output in clone.window_outputs)
                {
                    if (output.pixmap.get_xid () == (X.ID) xpixmap)
                    {
                        output.x = (int) l3;
                        break;
                    }
                }
                window.damage ();
            }
            else if (atom == offset_y_atom)
            {
                CCM.log ("OFFSET Y CLONE %i", (int)l3);
                var clone = (Clone) window.get_plugin (typeof (Clone));

                foreach (Output output in clone.window_outputs)
                {
                    if (output.pixmap.get_xid () == (X.ID) xpixmap)
                    {
                        output.y = (int) l3;
                        break;
                    }
                }
                window.damage ();
            }
            else if (atom == max_width_atom)
            {
                CCM.log ("MAX WIDTH CLONE %i", (int)l3);
                var clone = (Clone) window.get_plugin (typeof (Clone));

                foreach (Output output in clone.window_outputs)
                {
                    if (output.pixmap.get_xid () == (X.ID) xpixmap)
                    {
                        output.max_width = (int) l3;
                        break;
                    }
                }
                window.damage ();
            }
            else if (atom == max_height_atom)
            {
                CCM.log ("MAX HEIGHT CLONE %i", (int)l3);
                var clone = (Clone) window.get_plugin (typeof (Clone));

                foreach (Output output in clone.window_outputs)
                {
                    if (output.pixmap.get_xid () == (X.ID) xpixmap)
                    {
                        output.max_height = (int) l3;
                        break;
                    }
                }
                window.damage ();
            }
            else if (atom == scale_x_atom)
            {
                CCM.log ("SCALE X CLONE %i", (int)l3);
                var clone = (Clone) window.get_plugin (typeof (Clone));

                foreach (Output output in clone.window_outputs)
                {
                    if (output.pixmap.get_xid () == (X.ID) xpixmap)
                    {
                        output.scale_x = (int) l3;
                        break;
                    }
                }
                window.damage ();
            }
            else if (atom == scale_y_atom)
            {
                CCM.log ("SCALE Y CLONE %i", (int)l3);
                var clone = (Clone) window.get_plugin (typeof (Clone));

                foreach (Output output in clone.window_outputs)
                {
                    if (output.pixmap.get_xid () == (X.ID) xpixmap)
                    {
                        output.scale_y = (int) l3;
                        break;
                    }
                }
                window.damage ();
            }
            else if (atom == paint_parent_atom)
            {
                CCM.log ("PAINT PARENT CLONE");
                var clone = (Clone) window.get_plugin (typeof (Clone));

                foreach (Output output in clone.window_outputs)
                {
                    if (output.pixmap.get_xid () == (X.ID) xpixmap)
                    {
                        output.paint_parent = (bool) l3;
                        break;
                    }
                }
                window.damage ();
            }
            else if (atom == screen_enable_atom)
            {
                int depth = (int) l3;

                CCM.log ("ENABLE SCREEN CLONE");
                var output =
                    new Output (window.get_screen (), client, window, xpixmap, depth);
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
        void
        screen_load_options (CCM.Screen screen)
        {
            this.screen = screen;
            this.screen.composite_message.connect (on_composite_message);
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
                paint_parent_atom =
                    screen.get_display ().get_xdisplay ().
                    intern_atom ("_CCM_CLONE_PAINT_PARENT", false);
                offset_x_atom =
                    screen.get_display ().get_xdisplay ().
                    intern_atom ("_CCM_CLONE_OFFSET_X", false);
                offset_y_atom =
                    screen.get_display ().get_xdisplay ().
                    intern_atom ("_CCM_CLONE_OFFSET_Y", false);
                max_width_atom =
                    screen.get_display ().get_xdisplay ().
                    intern_atom ("_CCM_CLONE_MAX_WIDTH", false);
                max_height_atom =
                    screen.get_display ().get_xdisplay ().
                    intern_atom ("_CCM_CLONE_MAX_HEIGHT", false);
                scale_x_atom =
                    screen.get_display ().get_xdisplay ().
                    intern_atom ("_CCM_CLONE_SCALE_X", false);
                scale_y_atom =
                    screen.get_display ().get_xdisplay ().
                    intern_atom ("_CCM_CLONE_SCALE_Y", false);
            }
            ((CCM.ScreenPlugin) parent).screen_load_options (screen);
        }

        /**
         * Implement paint window plugin interface
         **/
        bool 
        window_paint (CCM.Window window, Cairo.Context context,
                      Cairo.Surface surface, bool y_invert)
        {
            if (((window_outputs != null && window_outputs.size > 0) ||
                 (screen_outputs != null && screen_outputs.size > 0)))
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

                            if (output.pixmap.get_device_geometry_clipbox (out clipbox))
                            {
                                Cairo.Context ctx = output.pixmap.create_context ();

                                if (ctx != null)
                                {
                                    double width = clipbox.width;
                                    double height = clipbox.height;

                                    if (output.max_width > 0 && width - output.x > output.max_width)
                                         width = output.max_width;

                                    if (output.max_height > 0 && height - output.y > output.max_height)
                                         height = output.max_height;

                                    double scale_x = clipbox.width / area->width;
                                    double scale_y = clipbox.height / area->height;

                                    if (output.scale_x != 0)
                                        scale_x = (double)output.scale_x / (double)100;
                                    if (output.scale_y != 0)
                                        scale_y = (double)output.scale_y / (double)100;

                                    ctx.set_operator (Cairo.Operator.SOURCE);
                                    ctx.rectangle (0, 0, width, height);
                                    ctx.clip ();
                                    ctx.scale (scale_x, scale_y);
                                    ctx.translate (output.x, output.y);
                                    ctx.translate (-area->x, -area->y);
                                    window.get_damage_path (ctx);
                                    ctx.clip ();
                                    ctx.translate (area->x, area->y);
                                    ctx.set_source_surface (surface, 
                                                            - (geometry.width - area->width) / 2.0,
                                                            - (geometry.height - area->height) / 2.0);
                                    ctx.paint ();

                                    if (!output.paint_parent)
                                    {
                                        CCM.Region region = new CCM.Region (-output.x, -output.y, (int)width, (int)height);
                                        window.undamage_region (region);
                                        window.get_damage_path (context);
                                        context.clip ();
                                    }
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

            /* Chain call to next plugin */
            return ((CCM.WindowPlugin) parent).window_paint (window, context,
                                                             surface, y_invert);
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
