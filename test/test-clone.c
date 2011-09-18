/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * test-clone.c
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

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <X11/extensions/Xdamage.h>

static GdkPixmap* pixmap = NULL;
static GtkWidget* main_window = NULL, *eventbox = NULL;
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
    
    if (0 && clone_window)
    {
        cairo_t* clone = gdk_cairo_create(GDK_DRAWABLE(pixmap));
        cairo_t* ctx = gdk_cairo_create(GDK_DRAWABLE(widget->window));

        cairo_set_operator(ctx, CAIRO_OPERATOR_CLEAR);
        cairo_paint(ctx);
        cairo_set_operator(ctx, CAIRO_OPERATOR_OVER);
        cairo_set_source_surface(ctx, cairo_get_target(clone), 0, 0);
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
    event.client.data.l[2] = GDK_DRAWABLE_XID(eventbox->window);
    event.client.data.l[3] = gdk_drawable_get_depth(GDK_DRAWABLE(eventbox->window));
    event.client.data.l[4] = GDK_WINDOW_XWINDOW(main_window->window);

    gdk_event_send_clientmessage_toall(&event);
    gtk_widget_queue_draw(main_window);
}

void
clone_pos(GdkWindow* window, int x, int y)
{
    GdkEvent event;
    GdkAtom ccm_atom = gdk_atom_intern_static_string("_CCM_CLIENT_MESSAGE");
    GdkAtom offset_x_atom = gdk_atom_intern_static_string("_CCM_CLONE_OFFSET_X");
    GdkAtom offset_y_atom = gdk_atom_intern_static_string("_CCM_CLONE_OFFSET_Y");

    event.client.type = GDK_CLIENT_EVENT;
    event.client.window = window;
    event.client.send_event = TRUE;
    event.client.message_type = ccm_atom;
    event.client.data_format = 32;
    event.client.data.l[0] = gdk_x11_atom_to_xatom(offset_x_atom);
    event.client.data.l[1] = GDK_WINDOW_XWINDOW(window);
    event.client.data.l[2] = GDK_DRAWABLE_XID(eventbox->window);
    event.client.data.l[3] = x;
    event.client.data.l[4] = GDK_WINDOW_XWINDOW(main_window->window);

    gdk_event_send_clientmessage_toall(&event);

    event.client.data.l[0] = gdk_x11_atom_to_xatom(offset_y_atom);
    event.client.data.l[3] = y;
    
    gdk_event_send_clientmessage_toall(&event);
    gtk_widget_queue_draw(main_window);
}

void
clone_scale(GdkWindow* window, double scale_x, int scale_y)
{
    GdkEvent event;
    GdkAtom ccm_atom = gdk_atom_intern_static_string("_CCM_CLIENT_MESSAGE");
    GdkAtom scale_x_atom = gdk_atom_intern_static_string("_CCM_CLONE_SCALE_X");
    GdkAtom scale_y_atom = gdk_atom_intern_static_string("_CCM_CLONE_SCALE_Y");

    event.client.type = GDK_CLIENT_EVENT;
    event.client.window = window;
    event.client.send_event = TRUE;
    event.client.message_type = ccm_atom;
    event.client.data_format = 32;
    event.client.data.l[0] = gdk_x11_atom_to_xatom(scale_x_atom);
    event.client.data.l[1] = GDK_WINDOW_XWINDOW(window);
    event.client.data.l[2] = GDK_DRAWABLE_XID(eventbox->window);
    event.client.data.l[3] = (int)(scale_x * 100);
    event.client.data.l[4] = GDK_WINDOW_XWINDOW(main_window->window);

    gdk_event_send_clientmessage_toall(&event);

    event.client.data.l[0] = gdk_x11_atom_to_xatom(scale_y_atom);
    event.client.data.l[3] = (int)(scale_y * 100);

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
        clone_pos (clone_window, 0, 20);
        clone_scale (clone_window, 0.5, 0.5);
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
            clone_pos (clone_window, 0, 20);
            clone_scale (clone_window, 0.5, 0.5);
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

    eventbox = gtk_event_box_new ();
    gtk_widget_show (eventbox);
    gtk_container_add(GTK_CONTAINER(main_window), eventbox);
    label = gtk_label_new("Click on the window to clone");
    gtk_widget_show(label);
    gtk_container_add(GTK_CONTAINER(eventbox), label);
    
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
