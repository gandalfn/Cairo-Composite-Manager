/* -*- Mode: Vala; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-buffer-surface.vala
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
 *
 * Backported from granite https://launchpad.net/granite
 *
 * Original Authors: Robert Dyer,
 *                   Rico Tzschichholz <ricotz@ubuntu.com>
 */

namespace CCM
{
    /**
     * A buffer containing an internal Cairo-usable surface and context, designed
     * for usage with large, rarely updated draw operations.
     */
    public class BufferSurface : GLib.Object
    {
        // constants
        const int cAlphaPrecision = 16;
        const int cParamPrecision = 7;

        // properties
        private Cairo.Surface m_Surface;
        private Cairo.Context m_Context;

        // accessors
        /**
         * The {@link Cairo.Surface} which will store the results of all drawing operations
         * made with {@link SSI.BufferSurface.context}.
         */
        public Cairo.Surface surface {
            get {
                if (m_Surface == null)
                {
                    m_Surface = new Cairo.ImageSurface (Cairo.Format.ARGB32, width, height);
                }
                return m_Surface;
            }
            private set {
                m_Surface = value;
            }
        }

        /**
         * The width of the {@link SSI.BufferSurface}, in pixels.
         */
        public int width { get; private set; }

        /**
         * The height of the BufferSurface, in pixels.
         */
        public int height { get; private set; }

        /**
         * The {@link Cairo.Context} for the internal surface. All drawing operations done on this
         * {@link SSI.BufferSurface} should use this context.
         */
        public Cairo.Context context {
            get {
                if (m_Context == null)
                    m_Context = new Cairo.Context (surface);
                return m_Context;
            }
        }

        // methods
        /**
         * Constructs a new, empty {@link SSI.BufferSurface} with the supplied dimensions.
         *
         * @param inWidth the width of {@link SSI.BufferSurface}, in pixels
         * @param inHeight the height of the {@link SSI.BufferSurface}, in pixels
         */
        public BufferSurface (int inWidth, int inHeight)
            requires (inWidth >= 0 && inHeight >= 0)
        {
            width = inWidth;
            height = inHeight;
        }

        /**
         * Constructs a new, empty {@link SSI.BufferSurface} with the supplied dimensions, using
         * the supplied {@link Cairo.Surface} as a model.
         *
         * @param width the width of the new {@link SSI.BufferSurface}, in pixels
         * @param height the height of the new {@link SSI.BufferSurface}, in pixels
         * @param model the {@link Cairo.Surface} to use as a model for the internal {@link Cairo.Surface}
         */
        public BufferSurface.with_surface (int inWidth, int inHeight, Cairo.Surface inSurface)
            requires (inWidth >= 0 && inHeight >= 0)
            requires (inSurface != null)
        {
            this (inWidth, inHeight);
            m_Surface = new Cairo.Surface.similar (inSurface, Cairo.Content.COLOR_ALPHA, inWidth, inHeight);
        }

        /**
         * Constructs a new, empty {@link SSI.BufferSurface} with the supplied dimensions, using
         * the supplied {@link SSI.BufferSurface} as a model.
         *
         * @param width the width of the new {@link SSI.BufferSurface}, in pixels
         * @param height the height of the new {@link SSI.BufferSurface}, in pixels
         * @param model the {@link SSI.BufferSurface} to use as a model for the internal {@link Cairo.Surface}
         */
        public BufferSurface.with_buffer_surface (int inWidth, int inHeight, BufferSurface inSurface)
            requires (inWidth >= 0 && inHeight >= 0)
            requires (inSurface != null)
        {
            this (inWidth, inHeight);
            m_Surface = new Cairo.Surface.similar (inSurface.m_Surface, Cairo.Content.COLOR_ALPHA, inWidth, inHeight);
        }

        private void
        exponential_blur_columns (uint8* inPixels, int inWidth, int inHeight, int inStartCol, int inEndCol, int inStartY, int inEndY, int inAlpha) {

            for (var columnIndex = inStartCol; columnIndex < inEndCol; columnIndex++)
            {
                // blur columns
                uint8 *column = inPixels + columnIndex * 4;

                var zA = column[0] << cParamPrecision;
                var zR = column[1] << cParamPrecision;
                var zG = column[2] << cParamPrecision;
                var zB = column[3] << cParamPrecision;

                // Top to Bottom
                for (var index = inWidth * (inStartY + 1); index < (inEndY - 1) * inWidth; index += inWidth)
                    exponential_blur_inner (&column[index * 4], ref zA, ref zR, ref zG, ref zB, inAlpha);

                // Bottom to Top
                for (var index = (inEndY - 2) * inWidth; index >= inStartY; index -= inWidth)
                    exponential_blur_inner (&column[index * 4], ref zA, ref zR, ref zG, ref zB, inAlpha);
            }
        }

        private void
        exponential_blur_rows (uint8* inPixels, int inWidth, int inHeight, int inStartRow, int inEndRow, int inStartX, int inEndX, int inAlpha) {

            for (var rowIndex = inStartRow; rowIndex < inEndRow; rowIndex++) {
                // Get a pointer to our current row
                uint8* row = inPixels + rowIndex * inWidth * 4;

                var zA = row[inStartX + 0] << cParamPrecision;
                var zR = row[inStartX + 1] << cParamPrecision;
                var zG = row[inStartX + 2] << cParamPrecision;
                var zB = row[inStartX + 3] << cParamPrecision;

                // Left to Right
                for (var index = inStartX + 1; index < inEndX; index++)
                    exponential_blur_inner (&row[index * 4], ref zA, ref zR, ref zG, ref zB, inAlpha);

                // Right to Left
                for (var index = inEndX - 2; index >= inStartX; index--)
                    exponential_blur_inner (&row[index * 4], ref zA, ref zR, ref zG, ref zB, inAlpha);
            }
        }

        private static inline void
        exponential_blur_inner (uint8* inPixels, ref int inZA, ref int inZR, ref int inZG, ref int inZB, int inAlpha)
        {

            inZA += (inAlpha * ((inPixels[0] << cParamPrecision) - inZA)) >> cAlphaPrecision;
            inZR += (inAlpha * ((inPixels[1] << cParamPrecision) - inZR)) >> cAlphaPrecision;
            inZG += (inAlpha * ((inPixels[2] << cParamPrecision) - inZG)) >> cAlphaPrecision;
            inZB += (inAlpha * ((inPixels[3] << cParamPrecision) - inZB)) >> cAlphaPrecision;

            inPixels[0] = (uint8) (inZA >> cParamPrecision);
            inPixels[1] = (uint8) (inZR >> cParamPrecision);
            inPixels[2] = (uint8) (inZG >> cParamPrecision);
            inPixels[3] = (uint8) (inZB >> cParamPrecision);
        }

        private void
        gaussian_blur_horizontal (double* inSrc, double* inDest, double* inKernel, int inGaussWidth,
                                  int inWidth, int inHeight, int startRow, int endRow, int[,] inShift)
        {
            uint32 cur_pixel = startRow * inWidth * 4;

            for (var y = startRow; y < endRow; y++)
            {
                for (var x = 0; x < inWidth; x++)
                {
                    for (var k = 0; k < inGaussWidth; k++)
                    {
                        var source = cur_pixel + inShift[x, k];

                        inDest[cur_pixel + 0] += inSrc[source + 0] * inKernel[k];
                        inDest[cur_pixel + 1] += inSrc[source + 1] * inKernel[k];
                        inDest[cur_pixel + 2] += inSrc[source + 2] * inKernel[k];
                        inDest[cur_pixel + 3] += inSrc[source + 3] * inKernel[k];
                    }

                    cur_pixel += 4;
                }
            }
        }

        private void
        gaussian_blur_vertical (double* inSrc, double* inDest, double* inKernel, int inGaussWidth,
                                int inWidth, int inHeight, int inStartCol, int inEndCol, int[,] inShift)
        {
            uint32 cur_pixel = inStartCol * 4;

            for (var y = 0; y < inHeight; y++)
            {
                for (var x = inStartCol; x < inEndCol; x++)
                {
                    for (var k = 0; k < inGaussWidth; k++)
                    {
                        var source = cur_pixel + inShift[y, k];

                        inDest[cur_pixel + 0] += inSrc[source + 0] * inKernel[k];
                        inDest[cur_pixel + 1] += inSrc[source + 1] * inKernel[k];
                        inDest[cur_pixel + 2] += inSrc[source + 2] * inKernel[k];
                        inDest[cur_pixel + 3] += inSrc[source + 3] * inKernel[k];
                    }

                    cur_pixel += 4;
                }
                cur_pixel += (inWidth - inEndCol + inStartCol) * 4;
            }
        }

        private static double[]
        build_gaussian_kernel (int inGaussWidth)
            requires (inGaussWidth % 2 == 1)
        {
            var inKernel = new double[inGaussWidth];

            // Maximum value of curve
            var sd = 255.0;

            // inWidth of curve
            var range = inGaussWidth;

            // Average value of curve
            var mean = range / sd;

            for (var i = 0; i < inGaussWidth / 2 + 1; i++)
                inKernel[inGaussWidth - i - 1] = inKernel[i] = Math.pow (Math.sin (((i + 1) * (Math.PI / 2) - mean) / range), 2) * sd;

            // normalize the values
            var gaussSum = 0.0;
            foreach (var d in inKernel)
                gaussSum += d;

            for (var i = 0; i < inKernel.length; i++)
                inKernel[i] = inKernel[i] / gaussSum;

            return inKernel;
        }

        /**
         * Clears the internal {@link Cairo.Surface}, making all pixels fully transparent.
         */
        public void clear ()
        {
            context.save ();

            m_Context.set_source_rgba (0, 0, 0, 0);
            m_Context.set_operator (Cairo.Operator.SOURCE);
            m_Context.paint ();

            m_Context.restore ();
        }

        /**
         * Creates a {@link Gdk.Pixbuf} from internal {@link Cairo.Surface}.
         *
         * @return the {@link Gdk.Pixbuf}
         */
        public Gdk.Pixbuf
        load_to_pixbuf ()
        {
            var image_surface = new Cairo.ImageSurface (Cairo.Format.ARGB32, width, height);
            var cr = new Cairo.Context (image_surface);

            cr.set_operator (Cairo.Operator.SOURCE);
            cr.set_source_surface (surface, 0, 0);
            cr.paint ();

            var width = image_surface.get_width ();
            var height = image_surface.get_height ();

            var pb = new Gdk.Pixbuf (Gdk.Colorspace.RGB, true, 8, width, height);
            pb.fill (0x00000000);

            uint8 *data = image_surface.get_data ();
            uint8 *pixels = pb.get_pixels ();
            var length = width * height;

            if (image_surface.get_format () == Cairo.Format.ARGB32)
            {
                for (var i = 0; i < length; i++)
                {
                    // if alpha is 0 set nothing
                    if (data[3] > 0) {
                        pixels[0] = (uint8) (data[2] * 255 / data[3]);
                        pixels[1] = (uint8) (data[1] * 255 / data[3]);
                        pixels[2] = (uint8) (data[0] * 255 / data[3]);
                        pixels[3] = data[3];
                    }

                    pixels += 4;
                    data += 4;
                }
            }
            else if (image_surface.get_format () == Cairo.Format.RGB24)
            {
                for (var i = 0; i < length; i++)
                {
                    pixels[0] = data[2];
                    pixels[1] = data[1];
                    pixels[2] = data[0];
                    pixels[3] = data[3];

                    pixels += 4;
                    data += 4;
                }
            }

            return pb;
        }

        /**
         * Performs a blur operation on the internal {@link Cairo.Surface}, using the
         * fast-blur algorithm found here [[http://incubator.quasimondo.com/processing/superfastblur.pde]].
         *
         * @param radius the blur radius
         * @param process_count the number of times to perform the operation
         */
        public void
        fast_blur (int inRadius, int inProcessCount = 1)
        {
            if (inRadius < 1 || inProcessCount < 1)
                return;

            var w = width;
            var h = height;
            var channels = 4;

            if (inRadius > w - 1 || inRadius > h - 1)
                return;

            var original = new Cairo.ImageSurface (Cairo.Format.ARGB32, w, h);
            var cr = new Cairo.Context (original);

            cr.set_operator (Cairo.Operator.SOURCE);
            cr.set_source_surface (surface, 0, 0);
            cr.paint ();

            uint8 *pixels = original.get_data ();
            var buffer = new uint8[w * h * channels];

            var vmin = new int[int.max (w, h)];
            var vmax = new int[int.max (w, h)];

            var div = 2 * inRadius + 1;
            var dv = new uint8[256 * div];
            for (var i = 0; i < dv.length; i++)
                dv[i] = (uint8) (i / div);

            while (inProcessCount-- > 0)
            {
                for (var x = 0; x < w; x++)
                {
                    vmin[x] = int.min (x + inRadius + 1, w - 1);
                    vmax[x] = int.max (x - inRadius, 0);
                }

                for (var y = 0; y < h; y++)
                {
                    var asum = 0, rsum = 0, gsum = 0, bsum = 0;

                    uint32 cur_pixel = y * w * channels;

                    asum += inRadius * pixels[cur_pixel + 0];
                    rsum += inRadius * pixels[cur_pixel + 1];
                    gsum += inRadius * pixels[cur_pixel + 2];
                    bsum += inRadius * pixels[cur_pixel + 3];

                    for (var i = 0; i <= inRadius; i++)
                    {
                        asum += pixels[cur_pixel + 0];
                        rsum += pixels[cur_pixel + 1];
                        gsum += pixels[cur_pixel + 2];
                        bsum += pixels[cur_pixel + 3];

                        cur_pixel += channels;
                    }

                    cur_pixel = y * w * channels;

                    for (var x = 0; x < w; x++)
                    {
                        uint32 p1 = (y * w + vmin[x]) * channels;
                        uint32 p2 = (y * w + vmax[x]) * channels;

                        buffer[cur_pixel + 0] = dv[asum];
                        buffer[cur_pixel + 1] = dv[rsum];
                        buffer[cur_pixel + 2] = dv[gsum];
                        buffer[cur_pixel + 3] = dv[bsum];

                        asum += pixels[p1 + 0] - pixels[p2 + 0];
                        rsum += pixels[p1 + 1] - pixels[p2 + 1];
                        gsum += pixels[p1 + 2] - pixels[p2 + 2];
                        bsum += pixels[p1 + 3] - pixels[p2 + 3];

                        cur_pixel += channels;
                    }
                }

                for (var y = 0; y < h; y++)
                {
                    vmin[y] = int.min (y + inRadius + 1, h - 1) * w;
                    vmax[y] = int.max (y - inRadius, 0) * w;
                }

                for (var x = 0; x < w; x++)
                {
                    var asum = 0, rsum = 0, gsum = 0, bsum = 0;

                    uint32 cur_pixel = x * channels;

                    asum += inRadius * buffer[cur_pixel + 0];
                    rsum += inRadius * buffer[cur_pixel + 1];
                    gsum += inRadius * buffer[cur_pixel + 2];
                    bsum += inRadius * buffer[cur_pixel + 3];

                    for (var i = 0; i <= inRadius; i++)
                    {
                        asum += buffer[cur_pixel + 0];
                        rsum += buffer[cur_pixel + 1];
                        gsum += buffer[cur_pixel + 2];
                        bsum += buffer[cur_pixel + 3];

                        cur_pixel += w * channels;
                    }

                    cur_pixel = x * channels;

                    for (var y = 0; y < h; y++)
                    {
                        uint32 p1 = (x + vmin[y]) * channels;
                        uint32 p2 = (x + vmax[y]) * channels;

                        pixels[cur_pixel + 0] = dv[asum];
                        pixels[cur_pixel + 1] = dv[rsum];
                        pixels[cur_pixel + 2] = dv[gsum];
                        pixels[cur_pixel + 3] = dv[bsum];

                        asum += buffer[p1 + 0] - buffer[p2 + 0];
                        rsum += buffer[p1 + 1] - buffer[p2 + 1];
                        gsum += buffer[p1 + 2] - buffer[p2 + 2];
                        bsum += buffer[p1 + 3] - buffer[p2 + 3];

                        cur_pixel += w * channels;
                    }
                }
            }

            original.mark_dirty ();

            context.set_operator (Cairo.Operator.SOURCE);
            context.set_source_surface (original, 0, 0);
            context.paint ();
            context.set_operator (Cairo.Operator.OVER);
        }

        /**
         * Performs a blur operation on the internal {@link Cairo.Surface}, using an
         * exponential blurring algorithm. This method is usually the fastest
         * and produces good-looking results (though not quite as good as gaussian's).
         *
         * @param radius the blur radius
         */
        public void
        exponential_blur (int inRadius)
        {
            if (inRadius < 1)
                return;

            var alpha = (int) ((1 << cAlphaPrecision) * (1.0 - Math.exp (-2.3 / (inRadius + 1.0))));
            var height = this.height;
            var width = this.width;

            var original = new Cairo.ImageSurface (Cairo.Format.ARGB32, width, height);
            var cr = new Cairo.Context (original);

            cr.set_operator (Cairo.Operator.SOURCE);
            cr.set_source_surface (surface, 0, 0);
            cr.paint ();

            uint8 *pixels = original.get_data ();

            try
            {
                // Process Rows
                exponential_blur_rows (pixels, width, height, height / 2, height, 0, width, alpha);

                var th = new Thread<void*>.try (null, () => {
                    exponential_blur_rows (pixels, width, height, 0, height / 2, 0, width, alpha);
                    return null;
                });

                th.join ();

                // Process Columns
                exponential_blur_columns (pixels, width, height, width / 2, width, 0, height, alpha);

                var th2 = new Thread<void*>.try (null, () => {
                    exponential_blur_columns (pixels, width, height, 0, width / 2, 0, height, alpha);
                    return null;
                });

                th2.join ();
            }
            catch (Error err)
            {
                warning (err.message);
            }

            original.mark_dirty ();

            context.set_operator (Cairo.Operator.SOURCE);
            context.set_source_surface (original, 0, 0);
            context.paint ();
            context.set_operator (Cairo.Operator.OVER);
        }

        /**
         * Performs a blur operation on the internal {@link Cairo.Surface}, using a
         * gaussian blurring algorithm. This method is very slow, albeit producing
         * debatably the best-looking results, and in most cases developers should
         * use the exponential blurring algorithm instead.
         *
         * @param radius the blur radius
         */
        public void
        gaussian_blur (int inRadius)
        {
            var gausswidth = inRadius * 2 + 1;
            var kernel = build_gaussian_kernel (gausswidth);

            var width = this.width;
            var height = this.height;

            var original = new Cairo.ImageSurface (Cairo.Format.ARGB32, width, height);
            var cr = new Cairo.Context (original);

            cr.set_operator (Cairo.Operator.SOURCE);
            cr.set_source_surface (surface, 0, 0);
            cr.paint ();

            uint8 *src = original.get_data ();

            var size = height * original.get_stride ();

            var abuffer = new double[size];
            var bbuffer = new double[size];

            // Copy image to double[] for faster horizontal pass
            for (var i = 0; i < size; i++)
                abuffer[i] = (double) src[i];

            // Precompute horizontal shifts
            var shiftar = new int[int.max (width, height), gausswidth];
            for (var x = 0; x < width; x++)
            {
                for (var k = 0; k < gausswidth; k++)
                {
                    var shift = k - inRadius;
                    if (x + shift <= 0 || x + shift >= width)
                        shiftar[x, k] = 0;
                    else
                        shiftar[x, k] = shift * 4;
                }
            }

            try
            {
                // Horizontal Pass
                gaussian_blur_horizontal (abuffer, bbuffer, kernel, gausswidth, width, height, height / 2, height, shiftar);

                var th = new Thread<void*>.try (null, () => {
                    gaussian_blur_horizontal (abuffer, bbuffer, kernel, gausswidth, width, height, 0, height / 2, shiftar);
                    return null;
                });

                th.join ();

                // Clear buffer
                Posix.memset (abuffer, 0, sizeof(double) * size);

                // Precompute vertical shifts
                shiftar = new int[int.max (width, height), gausswidth];
                for (var y = 0; y < height; y++)
                {
                    for (var k = 0; k < gausswidth; k++)
                    {
                        var shift = k - inRadius;
                        if (y + shift <= 0 || y + shift >= height)
                            shiftar[y, k] = 0;
                        else
                            shiftar[y, k] = shift * width * 4;
                    }
                }

                // Vertical Pass
                gaussian_blur_vertical (bbuffer, abuffer, kernel, gausswidth, width, height, width / 2, width, shiftar);

                var th2 = new Thread<void*>.try (null, () => {
                    gaussian_blur_vertical (bbuffer, abuffer, kernel, gausswidth, width, height, 0, width / 2, shiftar);
                    return null;
                });

                th2.join ();
            }
            catch (Error err)
            {
                message (err.message);
            }

            // Save blurred image to original uint8[]
            for (var i = 0; i < size; i++)
                src[i] = (uint8) abuffer[i];

            original.mark_dirty ();

            context.set_operator (Cairo.Operator.SOURCE);
            context.set_source_surface (original, 0, 0);
            context.paint ();
            context.set_operator (Cairo.Operator.OVER);
        }
    }
}
