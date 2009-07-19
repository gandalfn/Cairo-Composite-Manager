#include <gdk/gdk.h>

#include "ccm-cairo-utils.h"
#include <time.h>
#include <sys/time.h>

static double
timevalToSeconds (struct timeval inTimeval)
{
    double ret =
        (double) inTimeval.tv_sec + ((double) inTimeval.tv_usec) / 1000000.;
    return ret;
}

static double
diff_timeval_seconds (struct timeval inFirst, struct timeval inSecond)
{
    double first = timevalToSeconds (inFirst), second =
        timevalToSeconds (inSecond);

    return MAX (first, second) - MIN (first, second);
}

#define GET_TIME_START struct timeval start, end; gettimeofday(&start, 0);
#define GET_TIME_END double diff; gettimeofday(&end, 0); diff = diff_timeval_seconds(start,end)*1000.;
#define GET_TIME_DIFF diff

int
main (int argc, char **argv)
{
    cairo_surface_t *surface;
    cairo_surface_t *blur;
    cairo_t *cr;
    cairo_path_t *path;
    int width = 300, height = 300;

    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create (surface);
    cairo_set_source_rgba (cr, 0, 0, 0, 1);
    cairo_rectangle (cr, 50, 50, 200, 200);
    cairo_fill (cr);
    cairo_destroy (cr);
    {
        GET_TIME_START blur =
            cairo_image_surface_blur (surface, 5, 2.5, 0, 0, -1, -1);
        GET_TIME_END g_print ("pixman blur = %f\n", GET_TIME_DIFF);
    }
    cairo_surface_write_to_png (blur, "test-blur.png");
    cairo_surface_destroy (surface);
    cairo_surface_destroy (blur);

    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create (surface);
    cairo_set_source_rgba (cr, 0, 0, 0, 1);
    cairo_rectangle (cr, 50, 50, 200, 200);
    cairo_fill (cr);
    cairo_destroy (cr);
    {
        GET_TIME_START blur =
            cairo_image_surface_blur2 (surface, 5, 0, 0, -1, -1);
        GET_TIME_END g_print ("cairo blur = %f\n", GET_TIME_DIFF);
    }
    cairo_surface_write_to_png (blur, "test-blur2.png");
    cairo_surface_destroy (surface);
    cairo_surface_destroy (blur);

    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create (surface);
    cairo_set_source_rgba (cr, 0, 0, 0, 1);
    cairo_rectangle (cr, 50, 50, 200, 200);
    path = cairo_copy_path (cr);
    {
        GET_TIME_START cairo_set_source_rgba (cr, 0, 0, 0, 1);
        cairo_surface_t *mask =
            cairo_blur_path (surface, path, NULL, 15, 1, 300, 300);
        cairo_mask_surface (cr, mask, 0, 0);
        GET_TIME_END g_print ("cairo fake blur = %f\n", GET_TIME_DIFF);
    }
    cairo_surface_write_to_png (surface, "test-blur3.png");
    cairo_surface_destroy (surface);

    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 400, 400);
    cr = cairo_create (surface);
    cairo_set_source_rgb (cr, 1, 0, 0);
    cairo_notebook_page_round (cr, 100, 100, 200, 200, 30, 50, 50, 10);
    cairo_stroke (cr);
    cairo_destroy (cr);

    cairo_surface_write_to_png (surface, "test-round.png");
    cairo_surface_destroy (surface);

    return 0;
}
