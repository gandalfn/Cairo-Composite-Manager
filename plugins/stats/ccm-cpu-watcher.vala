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
    public class CPUWatcher : Watcher
    {
        // types
        private struct CPUAvg
        {
            public uint64 total;
            public uint64 used;
        }

        // properties
        int      m_Index;
        CPUAvg   m_Avg;

        // methods
        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        public CPUWatcher (Screen inScreen, int inX, int inY)
        {
            Region chart_area = new Region (inX, inY, 300, 200);
            Chart chart = new Chart (inScreen, "CPU / Memory", chart_area, 60, 2, 60);
            GLib.Object (chart: chart);

            GLib.Rand rand = new GLib.Rand ();
            chart.set_limit (0, 0, 100);
            chart.set_chart_color (0, "#%02x%02x%02x".printf (rand.int_range (0, 128), rand.int_range (0, 128), rand.int_range (0, 128)));

            chart.set_limit (1, 0, 100);
            chart.set_chart_color (1, "#%02x%02x%02x".printf (rand.int_range (0, 128), rand.int_range (0, 128), rand.int_range (0, 128)));
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        protected override void
        on_elapsed ()
        {
            GLibTop.cpu cpu;
            GLibTop.get_cpu (out cpu);

            uint64 ctotal = cpu.total;
            uint64 cused = cpu.user + cpu.nice + cpu.sys;
            uint64 total = ctotal - m_Avg.total;
            uint64 used = cused - m_Avg.used;

            chart[0, m_Index] = (int)(((double)used / double.max ((double)total, 1.0)) * 100.0);
            chart.set_legend_label (0, "CPU %i %%".printf ((int)(((double)used / double.max ((double)total, 1.0)) * 100.0)));

            m_Avg.total = ctotal;
            m_Avg.used = cused;

            GLibTop.mem mem;
            GLibTop.get_mem (out mem);
            chart[1, m_Index] = (int)(((double)mem.used / (double)mem.total) * 100.0);
            chart.set_legend_label (1, "Mem %i %%".printf ((int)(((double)mem.used / (double)mem.total) * 100.0)));

            m_Index++;
        }
    }
}
