/* -*- Mode: Vala; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007-2012 <gandalfn@club-internet.fr>
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
using Cairo;
using CCM;
using X;

namespace CCM
{
    public abstract class Watcher : GLib.Object
    {
        // properties
        private Chart m_DisplayGraph;
        private GLib.Timer m_Timer;
        private double m_Elapsed;

        // accessors
        public double elapsed {
            get {
                return m_Elapsed;
            }
        }

        public Chart chart {
            get {
                return m_DisplayGraph;
            }
            construct {
                m_DisplayGraph = value;
            }
        }

        // methods
        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        construct
        {
            m_Timer = new GLib.Timer ();
            m_Timer.start ();
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        protected virtual void
        on_elapsed ()
        {
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        protected virtual void
        on_refresh ()
        {
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        public void
        watch ()
        {
            m_Elapsed = m_Timer.elapsed () * 1000;
            if (m_Elapsed > 1000)
            {
                on_elapsed ();
                m_Timer.start ();
            }
            on_refresh ();
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        public virtual void
        paint (Cairo.Context inCtx, uint inNumFrame)
        {
            m_DisplayGraph.refresh (inCtx);
        }
    }
}
