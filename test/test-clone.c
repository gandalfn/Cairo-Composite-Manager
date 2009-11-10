/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <X11/extensions/Xdamage.h>

static GdkPixmap* pixmap = NULL;
static GtkWidget* main_window = NULL;
static GdkWindow* clone_window = NULL;
static Damage damage = None;
int event_base;
int error_base;
int width = 400, height = 400;
int count = 0;

void
on_realize(GtkWidget* widget, gpointer data)
{
    GdkDisplay* display = gdk_display_get_default();
    GdkScreen* screen = gdk_screen_get_default();
    GdkWindow* root = gdk_screen_get_root_window(screen);
    GdkCursor* cursor = gdk_cursor_new_for_display(display, GDK_CROSSHAIR);
    cairo_t* clone;
    
    XGrabPointer(GDK_DISPLAY_XDISPLAY(display), GDK_WINDOW_XWINDOW(root), False,
                 ButtonPressMask | ButtonReleaseMask, GrabModeSync,
                 GrabModeAsync, GDK_WINDOW_XWINDOW(root), 
                 GDK_CURSOR_XCURSOR(cursor), GDK_CURRENT_TIME);

    pixmap = gdk_pixmap_new(widget->window, width, height, -1);
    clone = gdk_cairo_create(GDK_DRAWABLE(pixmap));
    cairo_set_operator(clone, CAIRO_OPERATOR_CLEAR);
    cairo_paint(clone);
    cairo_destroy(clone);
    
    XDamageQueryExtension (GDK_DISPLAY_XDISPLAY(display), 
                           &event_base, &error_base);
    
    damage = XDamageCreate(GDK_DISPLAY_XDISPLAY(display), 
                           GDK_PIXMAP_XID(pixmap), 
                           XDamageReportBoundingBox);
    
    gtk_window_resize(GTK_WINDOW(main_window), width, height);
}

gboolean
on_delete_event(GtkWidget* widget, GdkEvent* event, gpointer data)
{
    gtk_main_quit();
    return FALSE;
}

gboolean
on_expose_event(GtkWidget* widget, GdkEventExpose* event, gpointer data)
{
    gboolean ret = FALSE;
    
    if (clone_window)
    {
        cairo_t* clone = gdk_cairo_create(GDK_DRAWABLE(pixmap));
        cairo_t* ctx = gdk_cairo_create(GDK_DRAWABLE(widget->window));

        cairo_set_operator(ctx, CAIRO_OPERATOR_CLEAR);
        cairo_paint(ctx);
        cairo_set_operator(ctx, CAIRO_OPERATOR_OVER);
        cairo_set_source_surface(ctx, cairo_get_target(clone), 0, 0);
        gchar* filename = g_strdup_printf("%05d.png", count);
        cairo_surface_write_to_png(cairo_get_target(clone), filename);
        count++;
        g_free(filename);
        cairo_paint(ctx);
        cairo_destroy(ctx);
        cairo_destroy(clone);
        ret = TRUE;
    }

    return ret;
}

void
clone(GdkWindow* window, gboolean enable)
{
    GdkEvent event;
    GdkAtom ccm_atom = gdk_atom_intern_static_string("_CCM_CLIENT_MESSAGE");
    GdkAtom clone_atom = enable ? 
                         gdk_atom_intern_static_string("_CCM_CLONE_ENABLE") :
                         gdk_atom_intern_static_string("_CCM_CLONE_DISABLE");
    
    event.client.type = GDK_CLIENT_EVENT;
    event.client.window = window;
    event.client.send_event = TRUE;
    event.client.message_type = ccm_atom;
    event.client.data_format = 32;
    event.client.data.l[0] = gdk_x11_atom_to_xatom(clone_atom);
    event.client.data.l[1] = GDK_WINDOW_XWINDOW(window);
    event.client.data.l[2] = GDK_DRAWABLE_XID(pixmap);
    event.client.data.l[3] = gdk_drawable_get_depth(GDK_DRAWABLE(pixmap));
    event.client.data.l[4] = GDK_WINDOW_XWINDOW(main_window->window);

    gdk_event_send_clientmessage_toall(&event);
    gtk_widget_queue_draw(main_window);
}
      
gboolean 
on_configure_event(GtkWidget* widget, GdkEventConfigure* event, gpointer data)
{
    if (clone_window && 
        (event->width != width || event->height != height))
    {
        GdkDisplay* display = gdk_display_get_default();
        cairo_t* ctx;
        
        width = event->width;
        height = event->height;

        
        clone (clone_window, FALSE);
        if (pixmap) g_object_unref(pixmap);
        XDamageDestroy(GDK_DISPLAY_XDISPLAY(display), damage);

        pixmap = gdk_pixmap_new(widget->window, width, height, -1);
        ctx = gdk_cairo_create(GDK_DRAWABLE(pixmap));
        cairo_set_operator(ctx, CAIRO_OPERATOR_CLEAR);
        cairo_paint(ctx);
        cairo_destroy(ctx);

        damage = XDamageCreate(GDK_DISPLAY_XDISPLAY(display), 
                                   GDK_PIXMAP_XID(pixmap), 
                                   XDamageReportNonEmpty);
        
        clone (clone_window, TRUE);
    }

    return FALSE;
}

GdkFilterReturn
on_filter_event(XEvent* xevent, GdkEvent* event, gpointer data)
{
    XAllowEvents(xevent->xany.display, SyncPointer, GDK_CURRENT_TIME);
    
    switch (xevent->type) 
    {
        case ButtonPress:
        {
            int width, height;
            clone_window = gdk_window_foreign_new(xevent->xbutton.subwindow);
            gdk_drawable_get_size(clone_window, &width, &height);
            double ratio = (double)width/(double)height;
            gtk_window_get_size(GTK_WINDOW(main_window), &width, &height);
            gtk_window_resize(GTK_WINDOW(main_window), width*ratio, height);
            clone (clone_window, TRUE);
            XUngrabPointer(xevent->xany.display, GDK_CURRENT_TIME);
        }
        break;
        default:
        break;
    }

    if (xevent->type == event_base + XDamageNotify)
    {
        XDamageSubtract(xevent->xany.display, damage, None, None);
        gtk_widget_queue_draw(main_window);
    }
    
    return GDK_FILTER_CONTINUE;
}

gint
main(gint argc, gchar** argv)
{
    GtkWidget* label;
    gtk_init(&argc, &argv);

    main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect(main_window, "realize", G_CALLBACK(on_realize), NULL);
    g_signal_connect(main_window, "delete-event", G_CALLBACK(on_delete_event), NULL);
    g_signal_connect(main_window, "expose-event", G_CALLBACK(on_expose_event), NULL);
    g_signal_connect(main_window, "configure-event", G_CALLBACK(on_configure_event), NULL);

    label = gtk_label_new("Click on the window to clone");
    gtk_widget_show(label);
    gtk_container_add(GTK_CONTAINER(main_window), label);
    
    gdk_window_add_filter(NULL, (GdkFilterFunc)on_filter_event, NULL);

    gtk_widget_set_app_paintable(main_window, TRUE);
    gtk_widget_realize(main_window);
    gdk_window_set_back_pixmap(main_window->window, NULL, FALSE);
    gtk_widget_show(main_window);

    gtk_main();

    clone (clone_window, FALSE);
    if (pixmap) g_object_unref(pixmap);
            
    return 0;
}
