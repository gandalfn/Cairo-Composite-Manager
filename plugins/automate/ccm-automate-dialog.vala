/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2008 <gandalfn@club-internet.fr>
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
using Gtk;
using Cairo;
using CCM;

[CCode (cheader_filename = "gdk/gdkx.h", cname = "GDK_WINDOW_XWINDOW")]
public extern X.Window gdk_window_xwindow (Gdk.Window window);

namespace CCM
{
    class AutomateDialog : GLib.Object
    {
        private const string UI_FILE =
            "/usr/share/cairo-compmgr/ui/ccm-automate.ui";

        private weak CCM.Screen screen;

        private CCM.Timeline timeline;
        private Gtk.Window main;
        private Gtk.Widget close;
        private Gtk.Image close_image;
        private Gtk.CheckButton hint_motion;
        private CCM.StoryBoard story_board;

        public AutomateDialog (CCM.Screen screen)
        {
            this.screen = screen;
            this.timeline = new CCM.Timeline (10, 60);
            this.timeline.set_direction (CCM.TimelineDirection.BACKWARD);

            this.timeline.new_frame += on_new_frame;
            this.timeline.completed += on_completed;

            construct_ui ();
        }

        ~AutomateDialog ()
        {
            main.destroy ();
        }

        private void on_new_frame (CCM.Timeline timeline, int num_frame)
        {
            int width, height;
            int x, y;

            main.window.get_origin (out x, out y);
            main.window.get_size (out width, out height);
            height -= close.allocation.height;
            main.move (x, -(int) ((double) height * timeline.get_progress ()));
        }

        private void on_completed (CCM.Timeline timeline)
        {
            string stock;

            if (timeline.get_direction () == CCM.TimelineDirection.FORWARD)
                stock = Gtk.STOCK_GO_DOWN;
            else
                stock = Gtk.STOCK_GO_UP;
            close_image.set_from_stock (stock, Gtk.IconSize.BUTTON);
        }

        private void on_realize (Gtk.Widget widget)
        {
            int width, height;
            Gdk.Atom atom_enable =
                Gdk.Atom.intern_static_string ("_CCM_SHADOW_ENABLED");
            uchar[]enable = { 1 };

            Gdk.property_change (main.window, atom_enable,
                                 Gdk.x11_xatom_to_atom (Gdk.
                                                        x11_get_xatom_by_name
                                                        ("CARDINAL")), 32,
                                 Gdk.PropMode.REPLACE, enable);

            main.window.get_size (out width, out height);
            main.window.set_override_redirect (true);
            main.move ((screen.get_xscreen ().width / 2) - (width / 2), 0);

            Gdk.Pixmap pixmap = new Gdk.Pixmap (null, width, height, 1);
            Cairo.Context ctx = Gdk.cairo_create (pixmap);
            ctx.set_operator (Cairo.Operator.SOURCE);
            ctx.set_source_rgba (0, 0, 0, 0);
            ctx.paint ();

            ctx.set_operator (Cairo.Operator.OVER);
            ctx.translate (width / 2, height / 2);
            ctx.rotate (-M_PI);
            ctx.translate (-width / 2, -height / 2);
            ctx.set_source_rgba (1, 1, 1, 1);
            ((Cairo.CCMContext) ctx).notebook_page_round (0, 0, width, height,
                                                          0,
                                                          close.allocation.
                                                          width,
                                                          close.allocation.
                                                          height, 6);
            ctx.fill ();
            ctx.get_target ().finish ();
            widget.window.shape_combine_mask ((Gdk.Bitmap) null, 0, 0);
            widget.window.input_shape_combine_mask ((Gdk.Bitmap) null, 0, 0);
            widget.window.shape_combine_mask ((Gdk.Bitmap) pixmap, 0, 0);
            widget.window.input_shape_combine_mask ((Gdk.Bitmap) pixmap, 0, 0);
        }

        private bool on_expose_event (Gtk.Widget widget, Gdk.EventExpose event)
        {
            Cairo.Context ctx = Gdk.cairo_create (widget.window);
            int width, height;

            widget.window.get_size (out width, out height);

            ctx.set_operator (Cairo.Operator.CLEAR);
            ctx.paint ();

            ctx.set_operator (Cairo.Operator.OVER);
            ctx.translate (width / 2, height / 2);
            ctx.rotate (-M_PI);
            ctx.translate (-width / 2, -height / 2);

            ctx.set_source_rgba ((double) widget.style.bg[Gtk.StateType.NORMAL].
                                 red / 65535,
                                 (double) widget.style.bg[Gtk.StateType.NORMAL].
                                 green / 65535,
                                 (double) widget.style.bg[Gtk.StateType.NORMAL].
                                 blue / 65535, 0.85);
            ((Cairo.CCMContext) ctx).notebook_page_round (0, 0, width, height,
                                                          0,
                                                          close.allocation.
                                                          width,
                                                          close.allocation.
                                                          height, 6);
            ctx.fill ();

            ctx.set_source_rgba ((double) widget.style.
                                 bg[Gtk.StateType.SELECTED].red / 65535,
                                 (double) widget.style.bg[Gtk.StateType.
                                                          SELECTED].green /
                                 65535,
                                 (double) widget.style.bg[Gtk.StateType.
                                                          SELECTED].blue /
                                 65535, 1);
            ((Cairo.CCMContext) ctx).notebook_page_round (0, 0, width, height,
                                                          0,
                                                          close.allocation.
                                                          width,
                                                          close.allocation.
                                                          height, 6);
            ctx.stroke ();

            Gtk.Widget child = ((Gtk.Window) widget).get_child ();
            ((Gtk.Container) widget).propagate_expose (child, event);

            return true;
        }

        private bool on_close (Gtk.Widget widget, Gdk.EventButton event)
        {
            if (timeline.get_direction () == CCM.TimelineDirection.FORWARD)
                timeline.set_direction (CCM.TimelineDirection.BACKWARD);
            else
                timeline.set_direction (CCM.TimelineDirection.FORWARD);
            timeline.stop ();
            timeline.start ();

            return false;
        }

        private void on_record_clicked (Gtk.Button button)
        {
            X.Window xwindow = gdk_window_xwindow (main.window);
            List < weak CCM.Window > ignore = new List < weak CCM.Window > ();
            weak CCM.Window window = screen.find_window (xwindow);

            ignore.append (window);
            story_board = new CCM.StoryBoard (screen, "test", ignore);
            story_board.hint_motion = hint_motion.get_active ();
        }

        private void on_stop_clicked (Gtk.Button button)
        {
            if (story_board != null)
                stdout.printf ("%s", story_board.to_string ());
            story_board = null;
        }

        private void construct_ui ()
        {
            try
            {
                // Get builder file
                var builder = new Gtk.Builder ();
                builder.add_from_file (UI_FILE);

                // Get main dialog
                main = builder.get_object ("main") as Gtk.Window;
                main.set_keep_above (true);

                // Set window as rgba
                Gdk.Screen screen = Gdk.Screen.get_default ();
                Gdk.Colormap colormap = screen.get_rgba_colormap ();
                main.set_colormap (colormap);

                ((Gtk.Widget) main).realize += on_realize;
                ((Gtk.Widget) main).expose_event += on_expose_event;

                // Get close event area
                close = builder.get_object ("close") as Gtk.Widget;
                close.button_press_event += on_close;

                close_image = builder.get_object ("close_image") as Gtk.Image;

                // Get record button press
                Gtk.Button record = builder.get_object ("record") as Gtk.Button;
                record.clicked += on_record_clicked;

                // Get record button press
                Gtk.Button stop = builder.get_object ("stop") as Gtk.Button;
                stop.clicked += on_stop_clicked;

                // Get record button press
                hint_motion =
                    builder.get_object ("hint_motion") as Gtk.CheckButton;
            }
            catch (GLib.Error ex)
            {
                CCM.log ("Error on create automate dialog: %s", ex.message);
            }
        }

        public void show ()
        {
            main.show ();
        }

        public void hide ()
        {
            main.hide ();
        }
    }
}
