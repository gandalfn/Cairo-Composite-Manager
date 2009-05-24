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
using math;

namespace CCM   
{
    public class ActionPointerPress : CCM.ActionPointer
    {
		public ActionPointerPress(CCM.Screen screen, X.ButtonEvent event, 
								  long time, List<weak CCM.Window> ignore) throws CCM.ActionError
		{			
			CCM.Window window = screen.find_window_at_pos(event.y_root, 
														  event.x_root);
			if (window != null)
			{
				bool found = false;
							
				foreach (CCM.Window item in ignore)
					found |= item.get_xwindow() == window.get_xwindow();
				
				if (!found)
				{
					this.button = (uint)event.state >> 8;
					this.x = event.y_root;
					this.y = event.x_root;
					this.time = (long)((double)time / (double)1000);
				}
				else
				{
					this.unref();
					throw new CCM.ActionError.WINDOW_IGNORE("Window is ignored at pos %i,%i", 
															event.y_root,
															event.x_root);
				}
			}
			else
			{
				this.unref();
				throw new CCM.ActionError.WINDOW_NOT_FOUND("Window not found at pos %i,%i", 
														   event.y_root,
														   event.x_root);
			}
		}
		
        public override string 
		to_string(string format = "%ccma")
		{
			string val = "<pointer-press button=\"" + button.to_string() + 
				         "\" x=\"" + x.to_string() + "\" y=\""+ y.to_string() + 
				         "\" time=\"" + time.to_string() + "\"/>";
			
			return val;
		}
    }
}
