/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007-2010 <gandalfn@club-internet.fr>
 * 
 * cairo-compmgr is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * cairo-compmgr is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with cairo-compmgr.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

using Gtk;
using X;

static WindowRecorder recorder = null;

public class WindowRecorder : Gtk.Window
{
    int width = 400; 
    int height = 400;
    Gtk.EventBox event_box;
    Gtk.Label label;
    unowned Gdk.Window clone_window = null;

    public X.Window xclone {
        set {
            int window_width, window_height;
            clone_window = Gdk.Window.foreign_new ((Gdk.NativeWindow)value);
            clone_window.get_size(out window_width,  out window_height);
            get_size(out width, out height);
            event_box.remove (label);
            label = null;
            clone (true);
            clone_offset (0, 0);
            clone_scale (width / (double)window_width, height / (double)window_height);
            queue_draw ();
        }
    }

    public WindowRecorder ()
    {
        resize (width, height);
        event_box = new Gtk.EventBox ();
        event_box.show ();
        event_box.above_child = false;
        event_box.visible_window = true;
        add (event_box);
        label = new Gtk.Label ("Click on the window to clone");
        label.show ();
        event_box.add (label);
    }

    public override void
    realize ()
    {
        base.realize ();

        get_clone_window_xid ();
    }

    public override bool 
    configure_event(Gdk.EventConfigure event)
    {
        bool ret = base.configure_event (event);

        if (clone_window != null && 
            (event.width != width || event.height != height))
        {
            width = event.width;
            height = event.height;

            clone (false);

            int window_width, window_height;
            clone_window.get_size(out window_width,  out window_height);
            get_size(out width, out height);
            clone (true);
            clone_offset (0, 0);
            clone_scale ((double)width / window_width, (double)height / window_height);
        }

        return ret;
    }

    public override bool
    delete_event (Gdk.Event event)
    {
        clone (false);
        Gtk.main_quit ();
        return false;
    }

    private Gdk.Event
    clone_message (Gdk.Atom type, long val)
    {
        Gdk.Event event = new Gdk.Event (Gdk.EventType.CLIENT_EVENT);
        Gdk.Atom ccm_atom = Gdk.Atom.intern_static_string("_CCM_CLIENT_MESSAGE");

        event.client.type = Gdk.EventType.CLIENT_EVENT;
        event.client.send_event = (char)true;
        event.client.window = window.ref () as Gdk.Window;
        event.client.message_type = ccm_atom;
        event.client.data_format = 32;

        long data[5];
        data[0] = (long)Gdk.x11_atom_to_xatom(type);
        data[1] = (long)Gdk.x11_drawable_get_xid(clone_window);
        data[2] = (long)Gdk.x11_drawable_get_xid(event_box.window);
        data[3] = (long)val;
        data[4] = (long)Gdk.x11_drawable_get_xid(window);
        Memory.copy (&event.client.data, data, sizeof (long) * 5);

        return event;
    }

    public void
    clone(bool enable)
    {
        Gdk.Atom clone_atom = enable ? 
            Gdk.Atom.intern_static_string("_CCM_CLONE_ENABLE") : 
            Gdk.Atom.intern_static_string("_CCM_CLONE_DISABLE");

        Gdk.Event event = clone_message (clone_atom, event_box.window.get_depth());

        event.send_clientmessage_toall ();

        queue_draw();
    }

    public void
    clone_offset(int x, int y)
    {
        Gdk.Atom offset_x_atom = Gdk.Atom.intern_static_string("_CCM_CLONE_OFFSET_X");
        Gdk.Atom offset_y_atom = Gdk.Atom.intern_static_string("_CCM_CLONE_OFFSET_Y");

        Gdk.Event event = clone_message (offset_x_atom, (long)x);
        event.send_clientmessage_toall ();
        event = clone_message (offset_y_atom, (long)y);
        event.send_clientmessage_toall ();

        queue_draw();
    }

    public void
    clone_scale(double scale_x, double scale_y)
    {
        Gdk.Atom scale_x_atom = Gdk.Atom.intern_static_string("_CCM_CLONE_SCALE_X");
        Gdk.Atom scale_y_atom = Gdk.Atom.intern_static_string("_CCM_CLONE_SCALE_Y");

        Gdk.Event event = clone_message (scale_x_atom, (long)(scale_x * 100));
        event.send_clientmessage_toall ();
        event = clone_message (scale_y_atom, (long)(scale_y * 100));
        event.send_clientmessage_toall ();

        queue_draw();
    }
}

static Gdk.FilterReturn
on_filter_func (X.Event xevent, Gdk.Event event)
{
    xevent.xany.display.allow_events(X.AllowEventsMode.SyncPointer,
                                     Gdk.CURRENT_TIME);

    switch (xevent.type) 
    {
        case X.EventType.ButtonPress:
            recorder.xclone = xevent.xbutton.subwindow;
            xevent.xany.display.ungrab_pointer(Gdk.CURRENT_TIME);
            ((Gdk.Window)null).remove_filter ((Gdk.FilterFunc)on_filter_func);
            break;
        default:
            break;
    }

    return Gdk.FilterReturn.CONTINUE;
}

void
get_clone_window_xid ()
{
    Gdk.Display display = Gdk.Display.get_default ();
    Gdk.Screen screen = Gdk.Screen.get_default ();
    Gdk.Window root = screen.get_root_window ();
    Gdk.Cursor cursor = new Gdk.Cursor.for_display(display, Gdk.CursorType.CROSSHAIR);
    X.Display xdisplay = Gdk.x11_display_get_xdisplay (display);
    xdisplay.grab_pointer((X.Window)Gdk.x11_drawable_get_xid (root), 
                          false,
                          X.EventMask.ButtonPressMask | 
                          X.EventMask.ButtonReleaseMask, 
                          X.GrabMode.Sync,
                          X.GrabMode.Async, 
                          (X.Window)Gdk.x11_drawable_get_xid (root), 
                          (uint32)Gdk.x11_cursor_get_xcursor (cursor), 
                          Gdk.CURRENT_TIME);

    ((Gdk.Window)null).add_filter ((Gdk.FilterFunc)on_filter_func);
}

static int
main (string[] args)
{
    Gtk.init (ref args);

    recorder = new WindowRecorder ();
    recorder.show_all ();

    Gtk.main ();

    return 0;
}
