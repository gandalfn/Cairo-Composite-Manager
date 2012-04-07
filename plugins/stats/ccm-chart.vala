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
    public class Chart : GLib.Object
    {
        // properties
        private uint            m_FontSize = 8;
        private uint            m_Border = 10;
        private uint            m_TitleSize = 10;
        private string          m_Title;
        private int             m_NbPoints = 0;
        private int             m_NbCharts = 0;
        private int             m_Count = 0;
        private int[,,]         m_Vals;
        private int[,]          m_Limits;
        private string          m_AbsUnit;
        private string          m_OrdUnit;
        private int[]           m_LastPoint;
        private Timeline        m_Timeline;
        private Cairo.Surface   m_Background;
        private Gdk.Color[]     m_LegendColors;
        private string[]        m_LegendLabels;
        private CCM.Region      m_Area;
        private Cairo.Rectangle m_TitleArea;
        private Cairo.Rectangle m_GraphArea;
        private Cairo.Rectangle m_LabelsArea;

        // accessors
        public Region area {
            get {
                return m_Area;
            }
            construct {
                m_Area = value.copy ();

                m_Area.get_clipbox (out m_TitleArea);
                m_TitleArea.x += m_Border;
                m_TitleArea.y += m_Border;
                m_TitleArea.width -= m_Border * 2;
                m_TitleArea.height = m_TitleSize;

                m_Area.get_clipbox (out m_GraphArea);
                m_GraphArea.x += m_Border;
                m_GraphArea.y = m_TitleArea.y + m_TitleArea.height + m_Border;
                m_GraphArea.width -= m_Border * 2;
                m_GraphArea.height -= m_TitleArea.height + m_Border * 3;
                m_GraphArea.height = 4 * m_GraphArea.height / 5;

                m_Area.get_clipbox (out m_LabelsArea);
                m_LabelsArea.x += m_Border;
                m_LabelsArea.y = m_GraphArea.y + m_GraphArea.height + m_Border;
                m_LabelsArea.width -= m_Border * 2;
                m_LabelsArea.height -= m_TitleArea.height + m_Border * 3;
                m_LabelsArea.height = m_GraphArea.height / 5;
            }
        }

        public int nb_points {
            get {
                return m_NbPoints;
            }
            construct {
                m_NbPoints = value;
            }
        }

        public int nb_charts {
            get {
                return m_NbCharts;
            }
            construct {
                m_NbCharts = value;
            }
        }

        // methods
        construct
        {
            // create vals array
            m_Vals = new int[m_NbCharts, m_NbPoints, 2];
            m_Limits = new int[m_NbCharts, 2];
            m_LastPoint = new int[m_NbCharts];

            m_LegendColors = new Gdk.Color[m_NbCharts];
            m_LegendLabels = new string[m_NbCharts];
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        public Chart (CCM.Screen inScreen, string inTitle, CCM.Region inArea, int inNbPoints, int inNbCharts, int inFps)
        {
            GLib.Object (area: inArea, nb_points: inNbPoints, nb_charts: inNbCharts);

            m_Title = inTitle;

            m_Timeline = new Timeline (inNbPoints, inFps);
            m_Timeline.loop = true;
            m_Timeline.new_frame.connect (() => {
                inScreen.damage_region (m_Area);
                m_Count++;
            });
            m_Timeline.start ();
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        private inline uint
        num_bars ()
        {
            uint val = (uint)m_GraphArea.width / (m_FontSize + 14);
            switch (val)
            {
                case 0:
                case 1:
                    return 1;
                case 2:
                case 3:
                    return 2;
                case 4:
                    return 4;
            }

            return 5;
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        private inline int
        prev_pos (int inChartNum, int inPos)
        {
            int cpt = inPos - 1;
            cpt = cpt < 0 ? m_NbPoints + cpt : cpt;
            if (m_Vals[inChartNum, cpt, 1] == 1)
                return cpt;

            return inPos;
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        private void
        draw_title (Cairo.Context inCtx)
        {
            inCtx.save ();
            {
                inCtx.translate (m_TitleArea.x, m_TitleArea.y);

                Pango.Layout layout = Pango.cairo_create_layout (inCtx);
                layout.set_text (m_Title, (int)m_Title.length);

                Pango.FontDescription desc = Pango.FontDescription.from_string ("Sans Bold 10");
                layout.set_font_description (desc);

                inCtx.set_source_rgba (0, 0, 0, 1);
                Pango.cairo_update_layout (inCtx, layout);
                Pango.cairo_show_layout (inCtx, layout);
            }
            inCtx.restore ();
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        private void
        create_chart_background ()
        {
            m_Background = new Cairo.ImageSurface (Cairo.Format.ARGB32, (int)m_GraphArea.width, (int)m_GraphArea.height);
            Cairo.Context ctx = new Cairo.Context (m_Background);
            ctx.set_operator (Cairo.Operator.CLEAR);
            ctx.paint ();
            ctx.set_operator (Cairo.Operator.OVER);

            double[] dash = {};
            dash += 1.0;
            dash += 2.0;
            uint num_bars = num_bars ();
            double y_offset = m_GraphArea.height / num_bars;

            // Set style properties
            ctx.set_line_width (1.0);
            ctx.set_dash (dash, 2);
            ctx.set_font_size (m_FontSize);
            ctx.set_antialias (Cairo.Antialias.NONE);

            // Draw bars
            Gdk.Color fg;
            Gdk.Color.parse ("#000000", out fg);
            string label;

            int max = 0,
            min = 0;
            for (uint cpt = 0; cpt < m_NbCharts; ++cpt)
            {
                max = int.max (max, m_Limits [cpt, 1]);
                min = int.min (min, m_Limits [cpt, 0]);
            }

            for (uint cpt = 0; cpt <= num_bars; ++cpt)
            {
                double y;

                if (cpt == 0)
                    y = 0.5 + m_FontSize / 1.5;
                else if (cpt == num_bars)
                    y = cpt * y_offset + 0.5;
                else
                    y = cpt * y_offset + m_FontSize / 2.0;

                if (m_OrdUnit != null)
                    label = "%u %s".printf (max - cpt * (max / num_bars), m_OrdUnit);
                else
                    label = "%u".printf (max - cpt * (max / num_bars));

                Cairo.TextExtents extents;
                ctx.text_extents (label, out extents);
                ctx.move_to (-extents.width + 24, y);
                Gdk.cairo_set_source_color (ctx, fg);
                ctx.show_text (label);

                ctx.set_source_rgba (0, 0, 0, 0.75);
                ctx.move_to (0, cpt * y_offset + 0.5);
                ctx.line_to (m_GraphArea.width - 0.5, cpt * y_offset + 0.5);
            }
            num_bars = m_NbPoints / 10;
            for (uint cpt = 0; cpt <= num_bars; ++cpt)
            {
                double x = cpt * m_GraphArea.width / num_bars;

                if (m_AbsUnit != null)
                    label = "%u %s".printf ((num_bars - cpt) * 10, m_AbsUnit);
                else
                    label = "%u".printf ((num_bars - cpt) * 10);

                ctx.set_source_rgba (0, 0, 0, 0.75);
                ctx.move_to ((GLib.Math.ceil (x) + 0.5), 0.5);
                ctx.line_to ((GLib.Math.ceil(x) + 0.5), m_GraphArea.height + 4.5);

                if (cpt != num_bars)
                {
                    Cairo.TextExtents extents;
                    ctx.text_extents (label, out extents);
                    ctx.move_to (((GLib.Math.ceil(x) + 0.5)) - (extents.width / 2), m_GraphArea.height - 4);
                    Gdk.cairo_set_source_color (ctx, fg);
                    ctx.show_text (label);
                }
            }
            ctx.stroke ();
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        public void
        draw_legends (Cairo.Context inCtx)
        {
            inCtx.save ();
            {
                double w = (m_LabelsArea.width / m_NbCharts);
                double h = m_LabelsArea.height;

                inCtx.translate (m_LabelsArea.x, m_LabelsArea.y);
                for (int chart = 0; chart < m_NbCharts; ++chart)
                {
                    double pos = (m_Border + w) * chart;
                    Gdk.cairo_set_source_color (inCtx, m_LegendColors[chart]);
                    inCtx.rectangle (pos, 0, h, h);
                    inCtx.fill ();

                    if (m_LegendLabels[chart] != null)
                    {
                        Pango.Layout layout = Pango.cairo_create_layout (inCtx);
                        layout.set_text (m_LegendLabels[chart], (int)m_LegendLabels[chart].length);

                        Pango.FontDescription desc = Pango.FontDescription.from_string ("Sans Bold 7");
                        layout.set_font_description (desc);

                        inCtx.set_source_rgba (0, 0, 0, 1);
                        Pango.cairo_update_layout (inCtx, layout);
                        int width, height;
                        layout.get_pixel_size (out width, out height);
                        inCtx.move_to (pos + height + m_Border * 2, (h - height) / 2);
                        Pango.cairo_show_layout (inCtx, layout);
                    }
                }
            }
            inCtx.restore ();
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        public void
        refresh (Cairo.Context inCtx)
        {
            Cairo.Rectangle a;
            m_Area.get_clipbox (out a);

            Gdk.Color fg;
            Gdk.Color.parse ("#89B2DF", out fg);

            inCtx.set_source_rgba ((double)fg.red / 65535.0, (double)fg.green / 65535.0, (double)fg.blue / 65535.0, 0.8);
            ((CCMContext)inCtx).rectangle_round (a.x, a.y, a.width, a.height, 12, Corners.ALL);
            inCtx.fill ();

            if (m_Background == null)
            {
                create_chart_background ();
            }

            draw_title (inCtx);

            inCtx.save ();
            {
                inCtx.set_line_width (1.8);
                inCtx.set_line_cap (Cairo.LineCap.ROUND);
                inCtx.set_line_join (Cairo.LineJoin.ROUND);

                inCtx.translate (m_GraphArea.x, m_GraphArea.y);
                inCtx.rectangle (0, 0, m_GraphArea.width, m_GraphArea.height);
                inCtx.clip ();

                inCtx.set_source_surface (m_Background, 0, 0);
                inCtx.paint ();

                for (int chart = 0; chart < m_NbCharts; ++chart)
                {
                    inCtx.new_path ();
                    inCtx.save ();
                    {
                        inCtx.scale ((double)(m_GraphArea.width) / (double)m_NbPoints,
                                     -(double)(m_GraphArea.height) / (double)(m_Limits[chart, 1] - m_Limits[chart, 0]));
                        double offset_x = 1.0 - ((double)m_Count / (double)m_NbPoints);
                        inCtx.translate (offset_x, -m_Limits[chart, 1]);

                        for (int point = 0; point < m_NbPoints; ++point)
                        {
                            int pos = m_LastPoint[chart] - point;
                            pos = pos < 0 ? m_NbPoints + pos : pos;

                            if (m_Vals[chart, pos, 1] == 1)
                            {
                                int val = m_Vals[chart, pos, 0];
                                int prev_pos_num = prev_pos (chart, pos);
                                int prev_val = m_Vals [chart, prev_pos_num, 0];

                                inCtx.curve_to (m_NbPoints - point - 0.5, val,
                                                m_NbPoints - point - 0.5, prev_val,
                                                m_NbPoints - point - 1, prev_val);
                            }
                        }
                    }
                    inCtx.restore ();

                    Gdk.cairo_set_source_color (inCtx, m_LegendColors[chart]);
                    inCtx.stroke ();
                }
            }
            inCtx.restore ();

            draw_legends (inCtx);
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        public new void
        set (int inChartNum, int inPointNum, int inVal)
            requires (inChartNum < m_NbCharts)
        {
            int n = inPointNum % m_NbPoints;

            m_Vals [inChartNum, n, 0] = inVal;
            m_Vals [inChartNum, n, 1] = 1;
            m_LastPoint[inChartNum] = n;
            m_Count = 0;

            if (m_Limits[inChartNum, 0] > inVal)
            {
                m_Limits[inChartNum, 0] = inVal;
                m_Background = null;
            }
            if (m_Limits[inChartNum, 1] < inVal)
            {
                m_Limits[inChartNum, 0] = inVal;
                m_Background = null;
            }
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        public void
        set_limit (int inChartNum, int inMin, int inMax)
            requires (inChartNum < m_NbCharts)
        {
            m_Limits[inChartNum, 0] = inMin;
            m_Limits[inChartNum, 1] = inMax;
            m_Background = null;
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        public void
        set_chart_color (int inChartNum, string inColor)
            requires (inChartNum < m_NbCharts)
        {
            Gdk.Color.parse (inColor, out m_LegendColors[inChartNum]);
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        public void
        set_legend_label (int inChartNum, string inLabel)
            requires (inChartNum < m_NbCharts)
        {
            m_LegendLabels[inChartNum] = inLabel;
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        public void
        set_units (string inAbsUnit, string inOrdUnit)
        {
            m_AbsUnit = inAbsUnit;
            m_OrdUnit = inOrdUnit;
        }
    }
}

