/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007 <gandalfn@club-internet.fr>
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

    const string[] options_key = {
        "duration"
    };

    class WindowAnimationOptions : PluginOptions
    {
        public double duration = 0.4;

		protected override void changed(CCM.Config config)
		{
			if (config == get_config (Options.DURATION))
            {
                var real_duration = 0.4;
                real_duration = config.get_float ();
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
    }

    class WindowAnimation : CCM.Plugin, CCM.WindowPlugin, CCM.PreferencesPagePlugin
    {
        private weak CCM.Window window;
        private CCM.WindowType type;
        private bool desktop_changed;
        private CCM.Timeline timeline;
        private Cairo.Rectangle geometry;
		private Gtk.Builder builder;

        class construct
		{
			type_options = typeof (WindowAnimationOptions);
		}

		~WindowAnimation ()
        {
            options_unload ();
        }

        private void option_changed (int index)
        {
            if (index == Options.DURATION)
            {
				timeline = null;
            }
        }

        private void on_property_changed (CCM.Window window,
                                          CCM.PropertyType property_type)
        {
            if (property_type == CCM.PropertyType.HINT_TYPE)
                this.type = window.get_hint_type ();
        }

        private void on_unlock ()
        {
            window.pop_matrix ("CCMWindowAnimation");
        }

        private void on_desktop_changed (int desktop)
        {
            desktop_changed = true;
        }

        private void on_new_frame (int frame)
        {
            double progress = timeline.get_progress ();
            double x0 = (geometry.width / 2.0) * (1.0 - progress), y0 =
                (geometry.height / 2.0) * (1.0 - progress);
            Cairo.Matrix matrix;

			window.damage ();
            if (timeline.get_direction () == CCM.TimelineDirection.FORWARD)
                matrix = Cairo.Matrix (progress, 0, 0, progress, x0, y0);
            else
                matrix = Cairo.Matrix (progress, 0, 0, 1, x0, 0);
            window.push_matrix ("CCMWindowAnimation", matrix);
            window.damage ();
        }

        private void on_finish ()
        {
			window.redirect = true;
            if (timeline.get_direction () == CCM.TimelineDirection.FORWARD)
            {
                unlock_map ();
                ((CCM.WindowPlugin) window).map (window);
            }
            else
            {
                unlock_unmap ();
                ((CCM.WindowPlugin) window).unmap (window);
            }
        }

        protected new void window_load_options (CCM.Window window)
        {
            this.window = window;
            this.type = CCM.WindowType.UNKNOWN;
            this.window.property_changed += on_property_changed;

            options_load ("window-animation", options_key, 
                          (PluginOptionsChangedFunc)option_changed);

            window.get_screen ().desktop_changed += on_desktop_changed;
            desktop_changed = false;

            ((CCM.WindowPlugin) parent).window_load_options (window);
        }

        protected CCM.Region query_geometry (CCM.Window window)
        {
            CCM.Region region;

            region = ((CCM.WindowPlugin) parent).query_geometry (window);
            if (region != null)
                region.get_clipbox (out geometry);

            return region;
        }

		protected void resize (CCM.Window window, int width, int height)
		{
			geometry.width = width;
			geometry.height = height;
			
			((CCM.WindowPlugin) parent).resize (window, width, height);
		}
		
        protected void map (CCM.Window window)
        {
            if (type == CCM.WindowType.UNKNOWN)
                type = window.get_hint_type ();

            if (!desktop_changed
                && window.is_decorated() &&
                   (type == CCM.WindowType.NORMAL
                    || type == CCM.WindowType.DIALOG))
            {
                int current_frame = 0;

                if (timeline == null)
                {
                    timeline = new CCM.Timeline.for_duration ((uint)(((WindowAnimationOptions) get_option ()).duration * 1000.0));
                    timeline.new_frame += on_new_frame;
                    timeline.completed += on_finish;
                }

                if (timeline.is_playing ())
                {
                    current_frame = timeline.get_current_frame ();
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

                timeline.set_direction (CCM.TimelineDirection.FORWARD);
                timeline.rewind ();
                timeline.start ();
                if (current_frame > 0)
                    timeline.advance (current_frame);
				window.redirect = false;
            }
            desktop_changed = false;

            ((CCM.WindowPlugin) parent).map (window);
        }

        protected void unmap (CCM.Window window)
        {
            if (!desktop_changed
                && window.is_decorated() &&
                   (type == CCM.WindowType.NORMAL
                    || type == CCM.WindowType.DIALOG))
            {
                int current_frame = 0;

                if (timeline == null)
                {
                    timeline = new CCM.Timeline.for_duration ((uint)(((WindowAnimationOptions) get_option ()).duration * 1000.0));
                    timeline.new_frame += on_new_frame;
                    timeline.completed += on_finish;
                }

                if (timeline.is_playing ())
                {
                    current_frame = timeline.get_current_frame ();
                    timeline.stop ();
                    on_finish ();
                }
                else
                    window.pop_matrix ("CCMWindowAnimation");

                lock_unmap (on_unlock);

                timeline.set_direction (CCM.TimelineDirection.BACKWARD);
                timeline.rewind ();
                timeline.start ();
                if (current_frame > 0)
                    timeline.advance (timeline.get_n_frames () - current_frame);
				window.redirect = false;
            }

            desktop_changed = false;

            ((CCM.WindowPlugin) parent).unmap (window);
        }

		////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////
		protected void
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

			((CCM.PreferencesPagePlugin) parent).init_effects_section (preferences,
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
