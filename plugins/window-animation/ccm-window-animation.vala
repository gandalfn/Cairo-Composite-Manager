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
using Config;
using CCM;

namespace CCM
{
    enum Options
    {
        DURATION,
        N
    }

    class WindowAnimationOptions : PluginOptions
    {
        public double duration = 0.4;

        public override void 
        changed(CCM.Config config)
        {
            var real_duration = 0.4;
            try
            {
                real_duration = config.get_float ();
            }
            catch (GLib.Error err)
            {
                real_duration = 0.4;
            }
            var duration = double.min (2.0, real_duration);
            duration = double.max (0.1, duration);

            this.duration = duration;
            if (duration != real_duration)
            {
                try
                {
                    config.set_float ((float) duration);
                }
                catch (GLib.Error err)
                {
                    CCM.log ("Error on set duration config: %s",
                             err.message);
                }
            }
        }
    }

    class WindowAnimation : CCM.Plugin, CCM.WindowPlugin, CCM.PreferencesPagePlugin
    {
        const string options_key[] = {
            "duration"
        };

        weak CCM.Window window;
        CCM.WindowType type;
        bool desktop_changed;
        CCM.Timeline timeline;
        Cairo.Rectangle geometry;
        Gtk.Builder builder;

        class construct
        {
            type_options = typeof (WindowAnimationOptions);
        }

        ~WindowAnimation ()
        {
            options_unload ();
        }

        void 
        option_changed (int index)
        {
            if (index == Options.DURATION)
            {
                timeline = null;
            }
        }

        void 
        on_property_changed (CCM.Window window,
                             CCM.PropertyType property_type)
        {
            if (property_type == CCM.PropertyType.HINT_TYPE)
                this.type = window.get_hint_type ();
        }

        void
        on_unlock ()
        {
            window.pop_matrix ("CCMWindowAnimation");
        }

        void
        on_desktop_changed (int desktop)
        {
            desktop_changed = true;
        }

        void
        on_new_frame (int frame)
        {
            double progress = timeline.progress;
            double x0 = (geometry.width / 2.0) * (1.0 - progress), 
                   y0 = (geometry.height / 2.0) * (1.0 - progress);
            Cairo.Matrix matrix;
            CCM.Region damaged = new CCM.Region.empty();

            if (timeline.direction == CCM.TimelineDirection.FORWARD)
                matrix = Cairo.Matrix (progress, 0, 0, progress, x0, y0);
            else
                matrix = Cairo.Matrix (progress, 0, 0, 1, x0, 0);

            weak CCM.Region? geometry;
            foreach (CCM.Window transient in window.get_transients())
            {
                geometry = transient.get_geometry();
                if (transient.is_viewable() && !transient.is_input_only() &&
                    geometry != null && !geometry.is_empty())
                    damaged.union(geometry);
            }
            geometry = window.get_geometry();
            if (geometry != null && !geometry.is_empty())
                damaged.union(geometry);

            window.push_matrix ("CCMWindowAnimation", matrix);

            foreach (CCM.Window transient in window.get_transients())
            {
                geometry = transient.get_geometry();
                if (transient.is_viewable() && !transient.is_input_only() &&
                    geometry != null && !geometry.is_empty())
                    damaged.union(geometry);
            }
            geometry = window.get_geometry();
            if (geometry != null && !geometry.is_empty())
                damaged.union(geometry); 

            if (!damaged.is_empty())
                window.get_screen().damage_region(damaged);
        }

        void
        on_finish ()
        {
            window.redirect = true;
            if (timeline.direction == CCM.TimelineDirection.FORWARD)
            {
                unlock_map ();
                (window as CCM.WindowPlugin).map (window);
            }
            else
            {
                unlock_unmap ();
                (window as CCM.WindowPlugin).unmap (window);
            }
        }

        void
        window_load_options (CCM.Window window)
        {
            this.window = window;
            this.type = CCM.WindowType.UNKNOWN;
            this.window.property_changed.connect (on_property_changed);

            options_load ("window-animation", options_key, 
                          (PluginOptionsChangedFunc)option_changed);

            window.get_screen ().desktop_changed.connect(on_desktop_changed);
            desktop_changed = false;

            (parent as CCM.WindowPlugin).window_load_options (window);
        }

        CCM.Region
        query_geometry (CCM.Window window)
        {
            CCM.Region region;

            region = ((CCM.WindowPlugin) parent).query_geometry (window);
            if (region != null)
                region.get_clipbox (out geometry);

            return region;
        }

        void
        resize (CCM.Window window, int width, int height)
        {
            geometry.width = width;
            geometry.height = height;

            (parent as CCM.WindowPlugin).resize (window, width, height);
        }

        void
        map (CCM.Window window)
        {
            if (type == CCM.WindowType.UNKNOWN)
                type = window.get_hint_type ();

            if (!desktop_changed && window.is_decorated() &&
                (type == CCM.WindowType.NORMAL || type == CCM.WindowType.DIALOG))
            {
                uint current_frame = 0;

                if (timeline == null)
                {
                    timeline = new CCM.Timeline.for_duration ((uint)(((WindowAnimationOptions) get_option ()).duration * 1000.0));
                    timeline.new_frame.connect (on_new_frame);
                    timeline.completed.connect (on_finish);
                }

                if (timeline.is_playing)
                {
                    current_frame = timeline.current_frame;
                    timeline.stop ();
                    on_finish ();
                }
                else
                {
                    Cairo.Matrix matrix = Cairo.Matrix.identity ();
                    Cairo.Rectangle area;

                    window.get_geometry_clipbox (out area);

                    matrix.scale (0.0, 0.0);
                    matrix.translate (area.width / 2.0, area.height / 2.0);
                    window.push_matrix ("CCMWindowAnimation", matrix);
                }

                lock_map (on_unlock);

                timeline.direction = CCM.TimelineDirection.FORWARD;
                timeline.rewind ();
                timeline.start ();
                if (current_frame > 0)
                    timeline.advance (current_frame);
                window.redirect = false;
            }
            desktop_changed = false;

            (parent as CCM.WindowPlugin).map (window);
        }

        void 
        unmap (CCM.Window window)
        {
            if (!desktop_changed && window.is_decorated() &&
                (type == CCM.WindowType.NORMAL || type == CCM.WindowType.DIALOG))
            {
                uint current_frame = 0;

                if (timeline == null)
                {
                    timeline = new CCM.Timeline.for_duration ((uint)(((WindowAnimationOptions) get_option ()).duration * 1000.0));
                    timeline.new_frame.connect (on_new_frame);
                    timeline.completed.connect (on_finish);
                }

                if (timeline.is_playing)
                {
                    current_frame = timeline.current_frame;
                    timeline.stop ();
                    on_finish ();
                }
                else
                    window.pop_matrix ("CCMWindowAnimation");

                lock_unmap (on_unlock);

                timeline.direction = CCM.TimelineDirection.BACKWARD;
                timeline.rewind ();
                timeline.start ();
                if (current_frame > 0)
                    timeline.advance (timeline.n_frames - current_frame);
                window.redirect = false;
            }

            desktop_changed = false;

            (parent as CCM.WindowPlugin).unmap (window);
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        void
        init_effects_section(CCM.PreferencesPage preferences, Gtk.Widget effects_section)
        {
            builder = new Gtk.Builder();
            try
            {
                builder.add_from_file(UI_DIR + "/ccm-window-animation.ui");
                var widget = builder.get_object ("window-animation") as Gtk.Widget;
                if (widget != null)
                {
                    int screen_num = preferences.get_screen_num();

                    ((Gtk.Box)effects_section).pack_start(widget, false, true, 0);

                    var duration = builder.get_object ("duration-adjustment") as CCM.ConfigAdjustment;
                    duration.screen = screen_num;

                    preferences.section_register_widget(PreferencesPageSection.EFFECTS,
                                                        widget, "window-animation");
                }
            }
            catch (GLib.Error err)
            {
                CCM.log("%s", err.message);
            }

            (parent as CCM.PreferencesPagePlugin).init_effects_section (preferences,
                                                                        effects_section);
        }
    }
}

/**
 * Init plugin
 **/
[ModuleInit]
public Type
ccm_window_animation_get_plugin_type (TypeModule module)
{
    return typeof (WindowAnimation);
}
