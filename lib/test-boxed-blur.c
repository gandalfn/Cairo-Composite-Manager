#include <gdk/gdk.h>

#include "ccm-cairo-utils.h"

int
main(int argc, char**argv)
{
    cairo_surface_t* png;
    cairo_surface_t* surface;
    cairo_surface_t* blur;
    cairo_t* cr;
    int width, height;
    
    png = cairo_image_surface_create_from_png("../data/cairo-compmgr.png");
    
    width = cairo_image_surface_get_width(png);
    height = cairo_image_surface_get_height(png);

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    blur = cairo_image_surface_blur(png, 9, 4.5, 0, 0, -1, -1);

    cr = cairo_create(surface);
    cairo_set_source_surface(cr, png, 0, 0);
    cairo_paint(cr);
    cairo_set_source_surface(cr, blur, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    
    cairo_surface_write_to_png(surface, "test-blur.png");
    cairo_surface_destroy(surface);
    cairo_surface_destroy(png);
    cairo_surface_destroy(blur);
    
    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 400, 400);
    cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1, 0, 0);
    cairo_notebook_page_round(cr, 100, 100, 200, 200, 30, 50, 50, 10);
    cairo_stroke(cr);
    cairo_destroy(cr);
    
    cairo_surface_write_to_png(surface, "test-round.png");
    cairo_surface_destroy(surface);
    
    return 0;
}