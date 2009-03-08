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
using X;
using math;

namespace CCM
{
    public class ActionPointerMotion : CCM.ActionPointer
    {
		public X.MotionEvent event {
			set 
			{
				this.button = (uint)value.state >> 8;
				this.x = value.y_root;
				this.y = value.x_root;
			}
		}
		
		public ActionPointerMotion(CCM.Screen screen, X.MotionEvent event, 
								   long time)
		{			
			this.event = event;
			this.time = (int)((double)time / (double)1000);
		}
		
        public override string 
		to_string(string format = "%ccma")
		{
			string val = "<pointer-motion button=\"" + button.to_string() + 
				         "\" x=\"" + x.to_string() + "\" y=\""+ y.to_string() + 
				         "\" time=\"" + time.to_string() + "\"/>";
			
			return val;
		}
    }
}
