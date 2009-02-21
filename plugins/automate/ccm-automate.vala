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
	public class Automate : CCM.Plugin, CCM.ScreenPlugin
	{
		private enum Options
		{
		    SHOW_SHORTCUT,
			N
		}
		
		private string[Options.N] options =  {
		    "show"
		};
		
		private weak CCM.Screen screen;
		
		private bool enable = false;
		
		private CCM.AutomateDialog dialog;
		private CCM.Keybind show_keybind;
		
		private CCM.Config[Options.N] configs = new CCM.Config[Options.N]; 
		
		private void
		on_show_shortcut_pressed()
		{
			enable = !enable;
			
			if (enable)
				dialog.show();
			else 
				dialog.hide();
		}
		
		private void
		get_show_shortcut()
		{
		    string shortcut = "<Super>a";
		    
		    try
		    {
		        shortcut = configs[Options.SHOW_SHORTCUT].get_string();
		    }
		    catch (GLib.Error ex)
		    {
		        CCM.log("Error on get show shortcut config get default");
		    }
		    show_keybind = new CCM.Keybind(screen, shortcut, true);
		    show_keybind.key_press += on_show_shortcut_pressed;
		}
		
		private void
		on_changed()
		{
			// Reload show shortcut
			get_show_shortcut();
		}
		
		protected void
		load_options(CCM.Screen screen)
		{
			this.screen = screen;
			
			this.dialog = new CCM.AutomateDialog(screen);
			
			// Get config object
			for (int cpt = 0; cpt < (int)Options.N; ++cpt)
			{
				configs[cpt] = new CCM.Config((int)screen.get_number(), 
											  "automate",
											  options[cpt]);
				if (configs[cpt] != null) 
					configs[cpt].changed += on_changed;
			}
			
			// get default shortcut
			get_show_shortcut();
			
			((CCM.ScreenPlugin)parent).load_options(screen);
		}
    }		
}

[ModuleInit]
public Type
ccm_automate_get_plugin_type(TypeModule module)
{
	return typeof (Automate);
}
