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

namespace CCM
{
    class StoryBoard : GLib.Object
    {
		public bool hint_motion { get; set; }
		
        private CCM.Screen screen;
        private string name;
		private List<weak CCM.Window> ignore;
        private List<CCM.Action> actions;
		private GLib.TimeVal time;
		
        public StoryBoard (CCM.Screen screen, string name, List<weak CCM.Window> ignore)
        {
			this.hint_motion = false;
            this.screen = screen;
            this.name = name;
			this.actions = new List<CCM.Action> ();
			this.time = GLib.TimeVal();
			this.ignore = ignore.copy();
			
			CCM.Display display = screen.get_display();
			display.event += on_event;
        }
        
        private void
		on_event(CCM.Display display, X.Event event)
		{
			GLib.TimeVal current = GLib.TimeVal();
			long diff = (current.tv_sec * 1000000 + current.tv_usec) - 
						(time.tv_sec * 1000000 + time.tv_usec);
			
			if (event.type == X.EventType.MotionNotify)
			{
				CCM.Action action = actions.last() != null ? actions.last().data : null;
				bool insert = true;
				
				if (hint_motion && action != null)
				{
					if (action is CCM.ActionPointerMotion)
					{
						((CCM.ActionPointerMotion)action).event = &event;
						((CCM.ActionPointerMotion)action).time = (int)((double)diff / (double)1000);
						insert = false;
					}
				}
				
				if (insert)
				{
					action = new CCM.ActionPointerMotion(screen, event, diff);
					actions.append(action);
				}
			}
			else if (event.type == X.EventType.ButtonPress)
			{
				try
				{
					var action = new CCM.ActionPointerPress(screen, event, diff, ignore);
					actions.append(action);
				}
				catch (CCM.ActionError ex)
				{
					CCM.debug(ex.message);
				}
			}
			else if (event.type == X.EventType.ButtonRelease)
			{
				try
				{
					var action = new CCM.ActionPointerRelease(screen, event, diff, ignore);
					actions.append(action);
				}
				catch (CCM.ActionError ex)
				{
					CCM.debug(ex.message);
				}
			}
			time = current;
		}
		
		public string 
		to_string(string format = "%ccmsb")
		{
			string val = "<story-board>\n";
			
			foreach (CCM.Action action in actions)
			{
				val += "  " + action.to_string() + "\n";
			}
			val += "</story-board>\n";
			
			return val;
		}
    }
}
