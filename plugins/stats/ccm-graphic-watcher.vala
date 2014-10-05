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
    public class GraphicWatcher : Watcher
    {
        // properties
        unowned Screen m_Screen;
#if HAVE_NVML
        Nvml.Device m_Device;
#endif
        Region m_SpinArea;
        uint m_Frames;
        uint m_Fps;
        int  m_Index;

        // methods
        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        public GraphicWatcher (Screen inScreen, int inX, int inY)
        {
            uint nb_devices = 0;
#if HAVE_NVML
            Nvml.init ();
            Nvml.Device.get_count (out nb_devices);
#endif

            Region chart_area = new Region (inX, inY + 70 + 10, 300, 200);
            Chart chart = new Chart (inScreen, "Graphics", chart_area, 60, nb_devices > 0 ? 3 : 1, 60);

            GLib.Rand rand = new GLib.Rand ();

            chart.set_limit (0, 0, 60);
            chart.set_chart_color (0, "#%02x%02x%02x".printf (rand.int_range (0, 128), rand.int_range (0, 128), rand.int_range (0, 128)));

            GLib.Object (chart: chart);

#if HAVE_NVML
            if (nb_devices > 0)
            {
                Nvml.Device.get_handle_by_index (0, out m_Device);
                chart.set_limit (1, 0, 100);
                chart.set_chart_color (1, "#%02x%02x%02x".printf (rand.int_range (0, 128), rand.int_range (0, 128), rand.int_range (0, 128)));
                chart.set_limit (2, 0, 100);
                chart.set_chart_color (2, "#%02x%02x%02x".printf (rand.int_range (0, 128), rand.int_range (0, 128), rand.int_range (0, 128)));
            }
#endif
            m_Screen = inScreen;
            m_SpinArea = new Region (inX, inY, 70, 70);
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        ~GraphicWatcher ()
        {
#if HAVE_NVML
            Nvml.shutdown ();
#endif
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        protected override void
        on_refresh ()
        {
            m_Frames++;
            m_Screen.damage_region (m_SpinArea);
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        protected override void
        on_elapsed ()
        {
            m_Fps = (uint)((m_Frames / elapsed) * 1000);
            int nb_miss = int.max ((int)(m_Screen.refresh_rate - m_Frames), 0);
            chart[0, m_Index] = nb_miss;
            chart.set_legend_label (0, "%i Frames".printf (nb_miss));

#if HAVE_NVML
            if (chart.nb_charts > 1)
            {
                Nvml.Utilization use;
                m_Device.get_utilization_rates (out use);

                chart[1, m_Index] = (int)use.gpu;
                chart.set_legend_label (1, "GPU %u %%".printf (use.gpu));
                chart[2, m_Index] = (int)use.memory;
                chart.set_legend_label (2, "Mem %u %%".printf (use.memory));
            }
#endif

            m_Index++;
            m_Frames = 0;
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        public override void
        paint (Cairo.Context inCtx, uint inNumFrame)
        {
            Cairo.Rectangle area;
            m_SpinArea.get_clipbox (out area);

            Gdk.Color fg;
            Gdk.Color.parse ("#89B2DF", out fg);

            inCtx.set_source_rgba ((double)fg.red / 65535.0, (double)fg.green / 65535.0, (double)fg.blue / 65535.0, 0.8);
            ((CCMContext)inCtx).rectangle_round (area.x, area.y, area.width, area.height, 12, Corners.ALL);
            inCtx.fill ();

            area.x += 5;
            area.y += 5;
            area.width -= 10;
            area.height -= 10;

            double xcenter = area.x + area.width / 2.0;
            double ycenter = area.y + area.height / 2.0;
            double radial = double.min (area.width, area.height) / 2;

            Gdk.Color color;
            Gdk.Color.parse ("#FF990E", out color);
            inCtx.set_line_width (10);
            Gdk.cairo_set_source_color (inCtx, color);

            double angle = (2 * GLib.Math.PI) / (m_Screen.refresh_rate / 5);
            double start = angle * (inNumFrame / 5);

            inCtx.arc (xcenter, ycenter, radial, start, start + angle);
            inCtx.stroke ();

            string str = "%02u".printf (m_Fps);
            Pango.Layout layout = Pango.cairo_create_layout (inCtx);
            layout.set_text (str, (int)str.length);

            Pango.FontDescription desc = Pango.FontDescription.from_string ("Sans Bold 24");
            layout.set_font_description (desc);

            inCtx.set_source_rgba (0, 0, 0, 1);
            Pango.cairo_update_layout (inCtx, layout);
            int width, height;
            layout.get_pixel_size (out width, out height);
            inCtx.move_to (xcenter - width / 2, ycenter - height / 2);
            Pango.cairo_show_layout (inCtx, layout);

            base.paint (inCtx, inNumFrame);
        }
    }
}
