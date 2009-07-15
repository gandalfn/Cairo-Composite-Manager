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
using CCM;

namespace CCM
{
	enum Options
	{
		ENABLED,
		N
	}
	
	const string[Options.N] options_key =  {
		"enabled"
	};
	
	class ValaWindowOptions : PluginOptions
	{
		public bool enabled;
	}
	
	private class ValaWindowPlugin : CCM.Plugin, CCM.WindowPlugin
	{
		private weak CCM.Window window;
		
		private uint counter = 0;
		
		~ValaWindowPlugin()
		{
			options_unload();
		}

		protected override PluginOptions
		options_init()
		{
			ValaWindowOptions options = new ValaWindowOptions();

			options.enabled = false;

			return options;
		}
		
		protected override void
		options_finalize(PluginOptions opts)
		{
			ValaWindowOptions* options = (ValaWindowOptions*)opts;
			delete options;
		}
		
		protected override void
		option_changed(CCM.Config config)
		{
			((ValaWindowOptions)get_option()).enabled = config.get_boolean();
			if (!((ValaWindowOptions)get_option()).enabled) window.get_screen().damage_all();
		}
		
		/**
		 * Implement load_options window plugin interface
		 **/
		protected void
		load_options(CCM.Window window)
		{
			this.window = window;
			
			options_load("vala-window-plugin", options_key);
			
			/* Chain call to next plugin */
			((CCM.WindowPlugin)parent).load_options(window);
		}
		
		/**
		 * Implement paint window plugin interface
		 **/
		protected bool
		paint(CCM.Window window, Cairo.Context ctx, 
			  Cairo.Surface surface, bool y_invert)
		{
			bool ret = false;
			
			/* Chain call to next plugin */
			ret = ((CCM.WindowPlugin)parent).paint(window, ctx, 
												   surface, y_invert);
			
			/* Paint damaged area */
			if (((ValaWindowOptions)get_option()).enabled)
			{
				CCM.Region damaged = window.get_damaged().copy();
			
				if (damaged != null)
				{
					unowned Cairo.Rectangle[] rectangles;
				
					damaged.get_rectangles(out rectangles);
				
					switch (counter)
					{
						case 0:
							ctx.set_source_rgba(1, 0, 0, 0.5);
							break;
						case 1:
							ctx.set_source_rgba(0, 1, 0, 0.5);
							break;
						case 2:
							ctx.set_source_rgba(0, 0, 1, 0.5);
							break;
						default:
							break;
					}
					if (++counter > 2) counter = 0;
					
					foreach (Cairo.Rectangle rectangle in rectangles)
					{
						ctx.rectangle(rectangle.x, rectangle.y,
									  rectangle.width, rectangle.height);
					}
					ctx.fill();
					rectangles_free(rectangles);
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
ccm_vala_window_plugin_get_plugin_type(TypeModule module)
{
	return typeof (ValaWindowPlugin);
}
		
