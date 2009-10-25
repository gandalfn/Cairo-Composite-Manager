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
using Vala;
using Math;
using Config;
using CCM;

namespace CCM
{
	enum Options
    {
        SPACING,
		SHORTCUT,
		DURATION,
        N
    }

    const string[] options_key = {
        "spacing",
		"shortcut",
		"duration"
    };

    class MosaicOptions : PluginOptions
    {
        public int spacing = 5;
		public string shortcut = "<Super>Tab";
		public double duration = 0.3;

		////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////
		protected override void changed(CCM.Config config)
		{
			if (config == get_config (Options.SPACING))
            {
                var real_spacing = 5;
				try
				{
            		real_spacing = config.get_integer ();
				}
				catch (GLib.Error err)
				{
					real_spacing = 5;
				}
                var spacing = int.min (50, real_spacing);
                spacing = int.max (0, spacing);

                this.spacing = spacing;
                if (spacing != real_spacing)
                {
                    try
                    {
                        config.set_integer (spacing);
                    }
                    catch (GLib.Error err)
                    {
                        CCM.log ("Error on set spacing config: %s",
                                 err.message);
                    }
                }
            }

			if (config == get_config (Options.SHORTCUT))
			{
				shortcut = "<Super>Tab";

		        try
		        {
		            shortcut = config.get_string ();
		        }
		        catch (GLib.Error ex)
		        {
		            CCM.log ("Error on get shortcut config get default");
		        }
			}

			if (config == get_config (Options.DURATION))
            {
                var real_duration = 0.3;
				try
				{
            		real_duration = config.get_float ();
				}
				catch (GLib.Error err)
				{
					real_duration = 0.3;
				}
				
                var duration = double.max (0.1, real_duration);
                duration = double.min (0.9, duration);

                this.duration = duration;
                if (duration != real_duration)
                {
                    try
                    {
                        config.set_float ((float)duration);
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
	
	class MosaicArea
	{
		public Cairo.Rectangle geometry;
		public weak CCM.Window window;
		public weak CCM.Mosaic plugin;
	}
	
	class Mosaic : CCM.Plugin, CCM.ScreenPlugin, CCM.WindowPlugin, CCM.PreferencesPagePlugin
	{
		weak CCM.MosaicArea?    area = null;
		weak CCM.Screen			screen;
		bool					enabled = false;
		CCM.Keybind				keybind;
		ArrayList <MosaicArea>  areas;
		bool					mouse_over = false;
		Timeline				timeline;
		double					progress;
		Gtk.Builder				builder = null;

		////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////
		class construct
		{
			type_options = typeof (MosaicOptions);
		}

		////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////
		~Mosaic ()
        {
			if (screen != null)
	            options_unload ();
        }

		////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////
		private void
		switch_keep_above(CCM.Window window, bool keep_above)
		{
			X.Event event = X.Event();
			weak CCM.Display display = window.get_display();
			weak CCM.Screen screen = window.get_screen();
			weak CCM.Window root = screen.get_root_window();

			event.xclient.type = X.EventType.ClientMessage;
			event.xclient.message_type = window.state_atom;
			event.xclient.display = display.get_xdisplay();
			event.xclient.window = window.child != X.None ? window.child : window.get_xwindow();
			event.xclient.send_event = true;
			event.xclient.format = 32;
			event.xclient.data.l[0] = keep_above ? 1 : 0;
			event.xclient.data.l[1] = (long)window.state_above_atom;
			event.xclient.data.l[2] = 0;
			event.xclient.data.l[3] = 0;
			event.xclient.data.l[4] = 0;

			display.get_xdisplay().send_event (root.get_xwindow(), true, 
			                                   X.EventMask.SubstructureRedirectMask | 
			                                   X.EventMask.SubstructureNotifyMask, 
			                                   ref event);
			display.flush();
		}
			
		////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////
		private void
		find_area(CCM.Window window)
		{
			Cairo.Rectangle* win_area = window.get_area();
			weak MosaicArea? found = null;
			double search = double.MAX;
				
			// Calculate the less scale area
			foreach (MosaicArea area in areas)
			{
				double dist = pow(area.geometry.x - win_area->x, 2) +
				              pow(area.geometry.y - win_area->y, 2);
				if (dist < search)
				{
					if (area.window == null)
					{
						found = area;					
						search = dist;
						break;
					}
					else
					{
						
						Cairo.Rectangle* awa = area.window.get_area();
						if (dist < pow(area.geometry.x - awa->x, 2) +
  				                   pow(area.geometry.y - awa->y, 2))
						{
							weak CCM.Window conflict = area.window;
							area.window = window;
							find_area(conflict);
							found = area;
							search = dist;
							break;
						}
					}
				}
			}
			if (found == null) 
			{
				foreach (MosaicArea area in areas)
				{
					if (area.window == null)
					{
						area.window = window;
						area.plugin = (CCM.Mosaic)window.get_plugin(typeof(Mosaic));
						area.plugin.area = area;
						area.plugin.enabled = true;
						area.plugin.mouse_over = false;
						area.plugin.timeline = new CCM.Timeline.for_duration((int)(((MosaicOptions) get_option ()).duration * 1000.0));
						area.plugin.timeline.new_frame += area.plugin.on_window_animation_new_frame;
						break;
					}
				}
			}
			else
			{
				found.window = window;
				found.plugin = (CCM.Mosaic)window.get_plugin(typeof(Mosaic));
				found.plugin.area = found;
				found.plugin.enabled = true;
				found.plugin.timeline = new CCM.Timeline.for_duration((int)(((MosaicOptions) get_option ()).duration * 1000.0));
				found.plugin.timeline.new_frame += found.plugin.on_window_animation_new_frame;
			}
		}
		
		////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////
		private void
		create_areas()
		{
			// Create or clear old areas
			if (areas == null)
				areas = new ArrayList <MosaicArea>();
			else
				areas.clear();

			// Count windows
			int nb_windows = 0;
			foreach (CCM.Window window in screen.get_windows())
			{
				if (window.is_decorated() && window.is_managed() &&
				    window.is_viewable() &&
				    window.get_hint_type() == CCM.WindowType.NORMAL)
					nb_windows++;
			}
			
			// Count lines
			int lines = (int)sqrt(nb_windows + 1);

			// Calculate areas size
			int x, y, width, height;
			int spacing = ((MosaicOptions) get_option ()).spacing;
			y = spacing;
			height = (screen.get_xscreen().height - (lines + 1) * spacing) / lines;
			for (int i = 0; i < lines; ++i)
			{
				int n = int.min(nb_windows - areas.size, (int)ceilf((float)nb_windows / lines));
				x = spacing;
				width = (screen.get_xscreen().width - (n + 1) * spacing) / n;
				for (int j = 0; j < n; ++j)
				{
					MosaicArea area = new MosaicArea();
					area.geometry.x = x;
					area.geometry.y = y;
					area.geometry.width = width;
					area.geometry.height = height;
					areas.add(area);
					x += width + spacing;
				}
				y += height + spacing;
			}
			
			// Place window in areas
			foreach (CCM.Window window in screen.get_windows())
			{
				if (window.is_decorated() && window.is_managed() &&
				    window.is_viewable() &&
				    window.get_hint_type() == CCM.WindowType.NORMAL)
					find_area(window);
			}
		}
		
		////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////
		private void on_screen_animation_new_frame (int frame)
		{
			foreach (MosaicArea area in areas)
			{
				Cairo.Rectangle win_area;
				if (area.window.get_device_geometry_clipbox(out win_area))
				{
					double progress = timeline.get_progress();
					
					// Calculate window scale
					double scale = double.min(area.geometry.width / win_area.width,
					                          area.geometry.height / win_area.height);

					// Add progress scale
					scale += (1.0 - scale) * progress;

					// Calculate window position
					double x = (area.geometry.x - win_area.x) - 
								(((win_area.width * scale) - area.geometry.width) / 2.0);
					double y = (area.geometry.y - win_area.y) - 
								(((win_area.height * scale) - area.geometry.height) / 2.0);

					// Add progress offset to position
					x += ((win_area.x - x) * progress) - ((progress) * win_area.x);
					y += ((win_area.y - y) * progress) - ((progress) * win_area.y);

					// Apply transformation to window
					Cairo.Matrix matrix = Cairo.Matrix(scale, 0, 0, scale, x, y);					
					area.window.push_matrix("CCMMosaic", matrix);
					area.window.block_mouse_redirect_event = true;
					area.plugin.progress = 1.0 - progress;
				}
			}	    

			screen.damage_all();
		}

		////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////
		private void on_screen_animation_completed ()
		{
			if (timeline.get_direction() == CCM.TimelineDirection.BACKWARD)
			{
				foreach (MosaicArea area in areas)
				{
					if (area.window is CCM.Window)
					{
						// Apply final transformation to window
						Cairo.Rectangle win_area;
						if (area.window.get_device_geometry_clipbox(out win_area))
						{
							// Calculate window scale
							double scale = double.min(area.geometry.width / win_area.width,
									                  area.geometry.height / win_area.height);

							// Calculate window position
							double x = (area.geometry.x - win_area.x) - 
										(((win_area.width * scale) - area.geometry.width) / 2.0);
							double y = (area.geometry.y - win_area.y) - 
										(((win_area.height * scale) - area.geometry.height) / 2.0);

							// Apply transformation to window
							Cairo.Matrix matrix = Cairo.Matrix(scale, 0, 0, scale, x, y);
							area.window.push_matrix("CCMMosaic", matrix);
						}
					}
				}	 
				
				// Window below the cursor
				weak CCM.Window? mouse;
				int x_mouse, y_mouse;
				screen.query_pointer(out mouse, out x_mouse, out y_mouse);
				foreach (MosaicArea area in areas)
				{
					if (area.window is CCM.Window)
					{
						area.plugin.mouse_over = area.window == mouse;			
						if (area.plugin.mouse_over)
						{
							area.plugin.timeline.set_direction(CCM.TimelineDirection.FORWARD);
							area.plugin.timeline.rewind();
							area.plugin.timeline.start();
							switch_keep_above(area.window, true);
							area.window.damage();
						}
					}
				}
			}
			else
			{
				foreach (MosaicArea area in areas)
				{
					// Remove window transformation
					if (area.window is CCM.Window)
					{
						area.window.pop_matrix("CCMMosaic");
						area.window.block_mouse_redirect_event = false;
						area.plugin.enabled = false;
						area.plugin.area = null;
						switch_keep_above(area.window, false);
					}
				}
				areas.clear();
			}
			screen.damage_all();
		}

		////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////
		private void on_window_animation_new_frame (int frame)
		{
			if (area != null && area.window is CCM.Window)
			{
				Cairo.Rectangle win_area;
				if (area.window.get_device_geometry_clipbox(out win_area))
				{
					double progress = timeline.get_progress();
					double spacing = ((MosaicOptions) get_option ()).spacing;
			
					double width = area.geometry.width + ((2.5 * spacing) * progress);
					double height = area.geometry.height + ((2.5 * spacing) * progress);
					
					// Calculate window scale
					double scale = double.min(width / win_area.width,
					                          height / win_area.height);

					// Calculate window position
					double x = (area.geometry.x - win_area.x) - 
								(((win_area.width * scale) - width) / 2.0);
					double y = (area.geometry.y - win_area.y) - 
								(((win_area.height * scale) - height) / 2.0);

					// Recenter window if in border 
					x -= (width - area.geometry.width) / 2.0;
					y -= (height - area.geometry.height) / 2.0;
					
					// Apply transformation to window
					area.window.damage();
					Cairo.Matrix matrix = Cairo.Matrix(scale, 0, 0, scale, x, y);					
					area.window.push_matrix("CCMMosaic", matrix);
					area.window.damage();

					this.progress = 1.0 - progress;
				}
			}
		}

		////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////
		private void on_window_activate_notify (CCM.Window window)
		{
			if (enabled)
			{
				enabled = false;

				if (timeline.is_playing())
				{
					timeline.stop();
				}
				timeline.set_direction(CCM.TimelineDirection.FORWARD);
				timeline.rewind();
				timeline.start();
			}
		}
		
		////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////
		private void on_shortcut_pressed ()
		{
			enabled = !enabled;

			if (timeline.is_playing())
			{
				timeline.stop();
			}
			if (enabled) 
			{
				create_areas();
			}
			else
			{
				foreach (MosaicArea area in areas)
				{
					if (area is CCM.Window &&
					    area.window.keep_above())
					{
						switch_keep_above(area.window, false);
					}
				}
			}
			timeline.set_direction(enabled ? CCM.TimelineDirection.BACKWARD : CCM.TimelineDirection.FORWARD);
			timeline.rewind();
			timeline.start();
		}
		
		////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////
		private void option_changed (int index)
	    {
			switch (index)
			{
				case CCM.Options.SHORTCUT:
					keybind = new CCM.Keybind (screen, 
					                           ((MosaicOptions) get_option ()).shortcut, 
					                           true);
					keybind.key_press += on_shortcut_pressed;
					break;
				case CCM.Options.DURATION:
					timeline = 
						new Timeline.for_duration((int)(((MosaicOptions) get_option ()).duration * 1000.0));
					timeline.new_frame += on_screen_animation_new_frame;
					timeline.completed += on_screen_animation_completed;
					break;
				default:
					break;
			}
	    }
		
		////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////
		protected void screen_load_options (CCM.Screen screen)
	    {
	        this.screen = screen;

			// load options
	        options_load ("mosaic", options_key,
	                      (PluginOptionsChangedFunc)option_changed);
			
	 		((CCM.ScreenPlugin) parent).screen_load_options (screen);

			// set mouse over on enter/leave window
			screen.enter_window_notify += (window) => { 
				CCM.Mosaic plugin = ((CCM.Mosaic)window.get_plugin(typeof(Mosaic)));
				if (plugin.enabled)
				{
					plugin.timeline.set_direction(CCM.TimelineDirection.FORWARD);
					plugin.timeline.rewind();
					plugin.timeline.start();
					switch_keep_above(window, true);
					plugin.mouse_over = true; 
					window.damage();
				}
			};
			screen.leave_window_notify += (window) => { 
				CCM.Mosaic plugin = ((CCM.Mosaic)window.get_plugin(typeof(Mosaic)));
				if (plugin.enabled)
				{
					plugin.mouse_over = false; 
					plugin.timeline.set_direction(CCM.TimelineDirection.BACKWARD);
					plugin.timeline.rewind();
					plugin.timeline.start();
					switch_keep_above(window, false);
					window.damage();
				}
			};

			// disable mosaic on window activate
			screen.activate_window_notify += on_window_activate_notify;
		}

		////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////
		protected bool window_paint (CCM.Window window, Cairo.Context ctx,
                                     Cairo.Surface surface, bool y_invert)
        {
            bool ret = false;

            /* Chain call to next plugin */
            ret = ((CCM.WindowPlugin) parent).window_paint (window, ctx, 
                                                            surface, y_invert);
			if (ret && enabled && !mouse_over)
			{
				unowned Cairo.Rectangle[] rectangles;

				CCM.Region area = window.get_area_geometry();
				ctx.save();
				ctx.set_source_rgba (0, 0, 0, 0.5 * progress);
				area.get_rectangles (out rectangles);
				foreach (Cairo.Rectangle rectangle in rectangles)
                {
                    ctx.rectangle (rectangle.x, rectangle.y,
                                   rectangle.width, rectangle.height);
                }
				ctx.fill();
				rectangles_free (rectangles);
				ctx.restore();
			}

			return ret;
		}

		////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////
		protected void
		init_desktop_section(CCM.PreferencesPage preferences, Gtk.Widget desktop_section)
		{
			builder = new Gtk.Builder();
			try
			{
				builder.add_from_file(UI_DIR + "/ccm-mosaic.ui");
				var widget = builder.get_object ("mosaic") as Gtk.Widget;
				if (widget != null)
				{
					int screen_num = preferences.get_screen_num();
					
					((Gtk.Box)desktop_section).pack_start(widget, false, true, 0);

					var duration = builder.get_object ("duration-adjustment") as CCM.ConfigAdjustment;
					duration.screen = screen_num;

					var spacing = builder.get_object ("spacing-adjustment") as CCM.ConfigAdjustment;
					spacing.screen = screen_num;

					var shortcut = builder.get_object ("shortcut") as CCM.ConfigEntryShortcut;
					shortcut.screen = screen_num;

					preferences.section_register_widget(PreferencesPageSection.DESKTOP,
					                                    widget, "mosaic");
				}
			}
			catch (GLib.Error err)
			{
				CCM.log("%s", err.message);
			}

			((CCM.PreferencesPagePlugin) parent).init_desktop_section (preferences,
			                                                           desktop_section);
		}
	}
}

/**
 * Init plugin
 **/
[ModuleInit]
public Type
ccm_mosaic_get_plugin_type (TypeModule module)
{
    return typeof (Mosaic);
}
