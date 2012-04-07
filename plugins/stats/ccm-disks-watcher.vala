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
using Vala;

namespace CCM
{
    public class DisksWatcher : Watcher
    {
        // properties
        int      m_Index;
        string[] m_Mounts;
        uint64   m_Read;
        uint64   m_Write;
        uint64   m_Max = 100;

        // methods
        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        public DisksWatcher (Screen inScreen, int inX, int inY)
        {
            Region chart_area = new Region (inX, inY, 300, 200);
            Chart chart = new Chart (inScreen, "Disks", chart_area, 60, 2, 60);
            GLib.Object (chart: chart);

            GLib.Rand rand = new GLib.Rand ();
            chart.set_limit (0, 0, (int)m_Max);
            chart.set_chart_color (0, "#%02x%02x%02x".printf (rand.int_range (0, 128), rand.int_range (0, 128), rand.int_range (0, 128)));

            chart.set_limit (1, 0, (int)m_Max);
            chart.set_chart_color (1, "#%02x%02x%02x".printf (rand.int_range (0, 128), rand.int_range (0, 128), rand.int_range (0, 128)));

            // get mount list
            m_Mounts = {};
            GLibTop.mountlist mountlist;
            unowned GLibTop.mountentry[] entries = (GLibTop.mountentry[])GLibTop.get_mountlist (out mountlist, 1);
            for (int cpt = 0; cpt < mountlist.number; ++cpt)
            {
                if (entries[cpt].type != "smbfs" && entries[cpt].type != "nfs" && entries[cpt].type != "cifs")
                {
                    m_Mounts += entries[cpt].mountdir;
                    GLibTop.fsusage usage;
                    GLibTop.get_fsusage (out usage, entries[cpt].mountdir);
                    m_Read += (usage.read * usage.block_size) / (1024 * 1024);
                    m_Write += (usage.write * usage.block_size) / (1024 * 1024);
                }
            }
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        protected override void
        on_elapsed ()
        {
            uint64 read = 0;
            uint64 write = 0;

            foreach (string mount in m_Mounts)
            {
                GLibTop.fsusage usage;
                GLibTop.get_fsusage (out usage, mount);

                read += (usage.read * usage.block_size) / (1024 * 1024);
                write += (usage.write * usage.block_size) / (1024 * 1024);
            }

            uint64 max = uint64.max ((int)(read - m_Read), (int)(write - m_Write));
            if (max > m_Max)
            {
                m_Max = max;
                chart.set_limit (0, 0, (int)max);
                chart.set_limit (1, 0, (int)max);
            }

            chart[0, m_Index] = (int)(read - m_Read);
            chart.set_legend_label (0, "Read:\n%i Mb".printf ((int)(read - m_Read)));

            chart[1, m_Index] = (int)(write - m_Write);
            chart.set_legend_label (1, "Write:\n%i Mb".printf ((int)(write - m_Write)));

            m_Read = read;
            m_Write = write;
            m_Index++;
        }

    }
}
