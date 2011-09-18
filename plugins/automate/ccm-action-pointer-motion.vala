/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-action-pointer-motion.vala
 * Copyright (C) Nicolas Bruguier 2007-2011 <gandalfn@club-internet.fr>
 * 
 * cairo-compmgr is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * cairo-compmgr is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

using GLib;
using Gtk;
using Cairo;
using CCM;
using math;

namespace CCM
{
    class ActionPointerMotion : CCM.ActionPointer
    {
        public X.Event *event
        {
            set
            {
                this.button = (uint) value->xmotion.state >> 8;
                this.x = value->xmotion.y_root;
                this.y = value->xmotion.x_root;
            }
        }

        public ActionPointerMotion (CCM.Screen screen, X.Event event, long time)
        {
            this.event = &event;
            this.time = (int) ((double) time / (double) 1000);
        }

        public override string to_string (string format = "%ccma")
        {
            string val =
                "<pointer-motion button=\"" + button.to_string () + "\" x=\"" +
                x.to_string () + "\" y=\"" + y.to_string () + "\" time=\"" +
                time.to_string () + "\"/>";

            return val;
        }
    }
}
