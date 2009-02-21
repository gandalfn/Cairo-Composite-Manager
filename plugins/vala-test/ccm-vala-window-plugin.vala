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
	public class ValaWindowPlugin : CCM.Plugin, CCM.WindowPlugin
	{
		private enum Options
		{
			ENABLE,
			N
		}
		
		private string[Options.N] options =  {
			"enable"
		};
		
		private weak CCM.Window window;
		
		private bool enabled = false;
		private uint counter = 0;
		
		private CCM.Config[Options.N] configs = new CCM.Config[Options.N]; 
		
		private void
		on_changed(CCM.Config config)
		{
			enabled = config.get_boolean();
			if (!enabled) window.get_screen().damage_all();
		}
		
		/**
		 * Implement load_options window plugin interface
		 **/
		protected void
		load_options(CCM.Window window)
		{
			this.window = window;
			
			var screen = window.get_screen();
		
			/* create configs */
			for (int cpt = 0; cpt < (int)Options.N; ++cpt)
			{
				configs[cpt] = new CCM.Config((int)screen.get_number(), 
											  "vala-window-plugin",
											  options[cpt]);
				if (configs[cpt] != null) 
					configs[cpt].changed += on_changed;
			}
			enabled = configs[(int)Options.ENABLE].get_boolean();
			
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
			if (enabled)
			{
				CCM.Region damaged = window.get_damaged().copy();
			
				if (damaged != null)
				{
					Cairo.Rectangle[] rectangles;
				
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
		