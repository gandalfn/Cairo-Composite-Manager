/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2008 <gandalfn@club-internet.fr>
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

using GLib;

[CCode (cheader_filename = "math.h") ]
public const double M_PI;

[CCode (cheader_filename = "cairo.h,ccm.h")]
namespace Cairo 
{
	[CCode (cname = "cairo_rectangle_t")]
	public struct Rectangle 
	{
		public double x;
		public double y;
		public double width;
		public double height;
	}
	public static void rectangles_free(Rectangle[] rects);
	
	[Compact]
	[CCode (cname = "cairo_surface_t", cprefix = "cairo_surface_")]
	public class CCMSurface
	{
		[CCode (cheader_filename = "ccm-cairo-utils.h", cname = "cairo_surface_blur_path")]
		public CCMSurface.blur_path (Cairo.Path path, Cairo.Path clip, int border, double step, double width, double height);
		[CCode (cheader_filename = "ccm-cairo-utils.h", cname = "cairo_image_surface_blur")]
		public CCMSurface.blur (Cairo.Surface surface, int radius, double sigma, int x, int y, int width, int height);
	}
	
	[Compact]
	[CCode (cname = "cairo_t", cprefix = "cairo_")]
	public class CCMContext
	{
		[CCode (cheader_filename = "ccm-cairo-utils.h")]
		public void notebook_page_round (double x, double y, double w, double h, double tx, double tw, double th, int radius);
		[CCode (cheader_filename = "ccm-cairo-utils.h")]
		public void rectangle_round (double x, double y, double w, double h, int radius, Cairo.Corners corners);
	}
	
    [CCode (cprefix = "CAIRO_CORNER_", cheader_filename = "ccm-cairo-utils.h")]
	public enum Corners {
		NONE,
		TOPLEFT,
		TOPRIGHT,
		BOTTOMLEFT,
		BOTTOMRIGHT,
		ALL
	}
}

[CCode (cprefix = "CCM", lower_case_cprefix = "ccm_")]
namespace CCM 
{
	[CCode (cheader_filename = "ccm-config.h")]
	public class Config : GLib.Object 
	{
		[CCode (has_construct_function = false)]
		public Config (int screen, owned string? extension, owned string key);
		
		public bool get_boolean () throws GLib.Error;
		public Gdk.Color get_color () throws GLib.Error;
		public float get_float () throws GLib.Error;
		public int get_integer () throws GLib.Error;
		public GLib.SList get_integer_list () throws GLib.Error;
		public string get_string () throws GLib.Error;
		public GLib.SList get_string_list () throws GLib.Error;
		
		public void set_boolean (bool value) throws GLib.Error;
		public void set_float (float value) throws GLib.Error;
		public void set_integer (int value) throws GLib.Error;
		public void set_integer_list (GLib.SList value) throws GLib.Error;
		public void set_string (string value) throws GLib.Error;
		public void set_string_list (GLib.SList value) throws GLib.Error;
		
		[HasEmitter]
		public signal void changed ();
	}
	
	[CCode (cheader_filename = "ccm-keybind.h")]
	public class Keybind : GLib.Object 
	{
		[CCode (has_construct_function = false)]
		public Keybind (CCM.Screen screen, owned string keystring, bool exclusive);
		
		[HasEmitter]
		public signal void key_motion (int object, int p0);
		[HasEmitter]
		public signal void key_press ();
		[HasEmitter]
		public signal void key_release ();
	}

	public static delegate void PluginUnlockFunc (void* data);

	[Compact]
	[CCode (cheader_filename = "ccm-plugin.h", destroy_function = "g_slice_free")]
	public class PluginOptions	{
	}
	
	[CCode (cheader_filename = "ccm-plugin.h")]
	public abstract class Plugin : GLib.Object
	{
		public GLib.Object parent { get; set; }

		protected abstract weak PluginOptions options_init();
		protected abstract void options_finalize(PluginOptions options);
		protected abstract void option_changed(Config config);

		public void options_load(string name, string[] options_keys);
		public void options_unload();
		
		public unowned PluginOptions get_option();
		public unowned Config get_config(int index);
		
		[CCode (cname = "ccm_plugin_get_type")]
		public static GLib.Type get_type ();
	}
	
	[CCode (cheader_filename = "ccm.h,ccm-display.h")]
	public class Display : GLib.Object 
	{
		public bool shm_shared_pixmap { get; set; }
		public bool use_xshm { get; set; }
		
		[CCode (has_construct_function = false)]
		public Display (owned string display);
        
        public weak X.Display get_xdisplay ();
		public weak CCM.Screen get_screen (uint number);
		
		public int get_shape_notify_event_type ();
		
		public void grab ();
		public void ungrab ();
		public void sync ();
		
		public void trap_error ();
		public int pop_error ();
		
		public bool report_device_event (CCM.Screen screen, bool report);
		
		[HasEmitter]
		public signal void damage_event (X.Event event);
		[HasEmitter]
		public signal void event (X.Event event);
		[HasEmitter]
		public signal void xfixes_cursor_event (X.Event event);
	}

	[CCode (cheader_filename = "ccm-screen-plugin.h")]
	public interface ScreenPlugin : GLib.Object
	{
		[CCode (cname = "_ccm_screen_plugin_get_root")]
		protected weak CCM.ScreenPlugin get_root();

		protected virtual bool add_window (CCM.Screen screen, CCM.Window window);
		protected virtual void damage (CCM.Screen screen, CCM.Region area, CCM.Window window);
		protected virtual void load_options (CCM.Screen screen);
		protected virtual bool paint (CCM.Screen screen, Cairo.Context ctx);
		protected virtual void remove_window (CCM.Screen screen, CCM.Window window);
	}

	[CCode (cheader_filename = "ccm.h,ccm-screen.h")]
	public class Screen : GLib.Object, CCM.ScreenPlugin 
	{
		public string backend { get; }
		public bool buffered_pixmap { get; set; }
		public X.Display display { get; set; }
		public bool filtered_damage { get; set; }
		public bool indirect_rendering { get; }
		public bool native_pixmap_bind { get; }
		public uint number { get; set; }
		public uint refresh_rate { get; }
		public void* window_plugins { get; }
		
		[CCode (has_construct_function = false)]
		public Screen (CCM.Display display, uint number);
		
		public uint get_number ();
		public weak X.Screen get_xscreen ();
		public weak CCM.Display get_display ();
		public weak CCM.Window get_root_window ();
		public weak CCM.Window get_overlay_window ();
		public weak GLib.List get_windows ();
		
		public bool add_window (CCM.Window window);
		public void remove_window (CCM.Window window);
		public weak CCM.Window? find_window (X.Window xwindow);
		public weak CCM.Window? find_window_or_child (X.Window xwindow);
		public weak CCM.Window? find_window_at_pos(int x, int y);
		
		public CCM.Region get_damaged ();
		[CCode (cname = "ccm_screen_damage")]
		public void damage_all ();
		public void damage_region (CCM.Region region);
		public void undamage_region (CCM.Region region);
		public void add_damaged_region (CCM.Region region);
		public void remove_damaged_region (CCM.Region region);
		
		public bool query_pointer (out CCM.Window below, out int x, out int y);
		
		[HasEmitter]
		public signal void plugins_changed ();
		[HasEmitter]
		public signal void refresh_rate_changed ();
		[HasEmitter]
		public signal void window_destroyed ();
		[HasEmitter]
		public signal void desktop_changed (int desktop);
	}

	[CCode (cheader_filename = "ccm.h,ccm-drawable.h")]
	public class Drawable : GLib.Object 
	{
		[NoAccessorMethod]
		public void* damaged { get; }
		public uint depth { get; set construct; }
		public ulong drawable { get; set; }
		public void* geometry { get; set construct; }
		public void* screen { get; set; }
		public Cairo.Matrix transform { get; }
		public void* visual { get; set construct; }

		public unowned CCM.Region? get_damaged() 
		{
			return (CCM.Region)damaged;
		}
		
		public virtual Cairo.Context create_context ();
		public virtual Cairo.Surface get_surface ();
		
		public X.ID get_xid ();
		public X.Visual get_visual ();
		public weak CCM.Display get_display ();
		public weak CCM.Screen get_screen ();
		
		public uint get_depth ();
		public Cairo.Format get_format ();
		
		public Cairo.Matrix get_transform ();
		public void push_matrix (string key, Cairo.Matrix matrix);
		public void pop_matrix (string key);
		
		public unowned CCM.Region? get_geometry ();
		public bool get_geometry_clipbox (out Cairo.Rectangle area);
		public unowned CCM.Region? get_device_geometry ();
		public bool get_device_geometry_clipbox (out Cairo.Rectangle area);
		
		public bool is_damaged ();
		public void damage ();
		public void damage_region (CCM.Region area);
		public void damage_region_silently (CCM.Region area);
		public void undamage_region (CCM.Region region);
		
		public Cairo.Path get_damage_path (Cairo.Context context);
		public Cairo.Path get_geometry_path (Cairo.Context context);
		
		public virtual void query_geometry ();
		public virtual void move (int x, int y);
		public virtual void resize (int width, int height);
		public virtual bool repair (CCM.Region damaged);
		public virtual void flush ();
		public virtual void flush_region (CCM.Region region);
	}
	
	[CCode (cheader_filename = "ccm.h,ccm-pixmap.h")]
	public class Pixmap : CCM.Drawable 
	{
		public bool freeze { get; set; }
		public bool y_invert { get; set; }

		[CCode (has_construct_function = false)]
		public Pixmap (CCM.Drawable drawable, X.Pixmap xpixmap);
		[CCode (has_construct_function = false)]
		public Pixmap.from_visual (CCM.Screen screen, X.Visual visual, X.Pixmap xpixmap);
		[CCode (has_construct_function = false, cname="ccm_pixmap_image_new")]
		public Pixmap.image (CCM.Drawable drawable, X.Pixmap xpixmap);

		public virtual void bind ();
		public virtual void release ();		
	}
	
	public interface WindowPlugin : GLib.Object
	{
		[CCode (cname = "_ccm_window_plugin_get_root")]
		protected CCM.WindowPlugin get_root();
		
		protected virtual void load_options (CCM.Window window);
		protected void lock_load_options(PluginUnlockFunc? func);
		protected void unlock_load_options();

		protected virtual void map (CCM.Window window);
		protected void lock_map(PluginUnlockFunc? func);
		protected void unlock_map();
		
		protected virtual void unmap (CCM.Window window);
		protected void lock_unmap(PluginUnlockFunc? func);
		protected void unlock_unmap();

		protected virtual void move (CCM.Window window, int x, int y);
		protected void lock_move(PluginUnlockFunc? func);
		protected void unlock_move();

		protected virtual bool paint (CCM.Window window, Cairo.Context ctx, Cairo.Surface surface, bool y_invert);
		protected void lock_paint(PluginUnlockFunc? func);
		protected void unlock_paint();
		
		protected virtual CCM.Region query_geometry (CCM.Window window);
		protected void lock_query_geometry(PluginUnlockFunc? func);
		protected void unlock_query_geometry();
		
		protected virtual void query_opacity (CCM.Window window);
		protected void lock_query_opacity(PluginUnlockFunc? func);
		protected void unlock_query_opacity();

		protected virtual void resize (CCM.Window window, int width, int height);
		protected void lock_resize(PluginUnlockFunc? func);
		protected void unlock_resize();

		protected virtual void set_opaque_region (CCM.Window window, CCM.Region area);
		protected void lock_set_opaque_region(PluginUnlockFunc? func);
		protected void unlock_set_opaque_region();

		protected virtual void get_origin (CCM.Window window, out int x, out int y);
		protected void lock_get_origin(PluginUnlockFunc? func);
		protected void unlock_get_origin();
			
		protected virtual CCM.Pixmap get_pixmap (CCM.Window window);
		protected void lock_get_pixmap(PluginUnlockFunc? func);
		protected void unlock_get_pixmap();
	}	

	[CCode (cheader_filename = "ccm.h,ccm-window.h,ccm-window-plugin.h")]
	public class Window : CCM.Drawable, CCM.WindowPlugin 
	{
		public void* child { get; set; }
		public void* input { get; }
		public void* mask { get; set; }
		public int mask_height { get; set; }
		public int mask_width { get; set; }
		public bool redirect { set; }
		public bool use_image { set; }

		[CCode (has_construct_function = false)]
		public Window (CCM.Screen screen, X.Window xwindow);

		[CCode (cname = "CCM_WINDOW_XWINDOW")]
		public weak X.Window get_xwindow();
		
		public void activate (GLib.Time timestamp);
		public virtual CCM.Pixmap create_pixmap (int width, int height, int depth);
		public Cairo.Rectangle* get_area ();
		public uint32 get_child_property (X.Atom property_atom, X.Atom req_type, out uint n_items);
		public void get_frame_extends (out int left_frame, out int right_frame, out int top_frame, out int bottom_frame);
		public weak CCM.Window get_group_leader ();
		public CCM.WindowType get_hint_type ();
		public unowned string get_name ();
		public float get_opacity ();
		public bool get_opaque_clipbox (Cairo.Rectangle clipbox);
		public weak CCM.Region get_opaque_region ();
		public weak CCM.Pixmap get_pixmap ();
		public uint32 get_property (X.Atom property_atom, X.Atom req_type, out uint n_items);
		public GLib.SList get_transients ();
		public bool is_decorated ();
		public bool is_fullscreen ();
		public bool is_input_only ();
		public bool is_managed ();
		public bool is_shaded ();
		public bool is_viewable ();
		public bool keep_above ();
		public bool keep_below ();
		public void make_input_output ();
		public void make_output_only ();
		public void map ();
		public bool paint (Cairo.Context ctx, bool buffered);
		public void query_frame_extends ();
		public void query_hint_type ();
		public void query_mwm_hints ();
		public void query_opacity (bool deleted);
		public void query_state ();
		public void query_transient_for ();
		public void query_wm_hints ();
		public weak X.Window redirect_event (X.Event event, X.Window over);
		public void redirect_input ();
		public void redirect_subwindows ();
		public void set_alpha ();
		public void set_opacity (float opacity);
		public void set_opaque ();
		public void set_opaque_region (CCM.Region region);
		public bool set_state (X.Atom state_atom);
		public bool skip_pager ();
		public bool skip_taskbar ();
		public void switch_state (X.Atom state_atom);
		public bool transform (Cairo.Context ctx, bool y_invert);
		public weak CCM.Window transient_for ();
		public void unmap ();
		public void unredirect ();
		public void unredirect_input ();
		public void unredirect_subwindows ();
		public void unset_state (X.Atom state_atom);
		
		[HasEmitter]
		public signal void error ();
		[HasEmitter]
		public signal void opacity_changed ();
		[HasEmitter]
		public signal void property_changed (CCM.PropertyType type);
	}
	
	[CCode (cheader_filename = "ccm-preferences.h")]
	public class Preferences : GLib.Object
	{
		[CCode (has_construct_function = false)]
		public Preferences ();
		
		public void show ();
		public void hide ();
	}
	
	public interface PreferencesPagePlugin 
	{
		protected virtual void init_general_section (CCM.PreferencesPage preferences, Gtk.Widget general_section);
		protected virtual void init_desktop_section (CCM.PreferencesPage preferences, Gtk.Widget desktop_section);
		protected virtual void init_windows_section (CCM.PreferencesPage preferences, Gtk.Widget windows_section);
		protected virtual void init_effects_section (CCM.PreferencesPage preferences, Gtk.Widget effects_section);
		protected virtual void init_accessibility_section (CCM.PreferencesPage preferences, Gtk.Widget accessibility_section);
		protected virtual void init_utilities_section (CCM.PreferencesPage preferences, Gtk.Widget utilities_section);
	}
	
	[CCode (cheader_filename = "ccm-preferences-page.h,ccm-preferences-page-plugin.h")]
	public class PreferencesPage : GLib.Object, CCM.PreferencesPagePlugin
	{
		[CCode (has_construct_function = false)]
		public PreferencesPage (CCM.Preferences preferences, int screen_num);
	}
	
	[CCode (cheader_filename = "ccm-timeline.h")]
	public class Timeline : GLib.Object 
	{
		public uint delay { get; set; }
		public uint direction { get; set; }
		public uint duration { get; set; }
		public uint fps { get; set; }
		public bool loop { get; set; }
		public uint num_frames { get; set; }
		
		[CCode (has_construct_function = false)]
		public Timeline (uint n_frames, uint fps);
		[CCode (has_construct_function = false)]
		public Timeline.for_duration (uint msecs);
		
		public CCM.Timeline clone ();
		
		public bool is_playing ();
		public int get_current_frame ();
		public uint get_delay ();
		public uint get_delta (out uint msecs);
		public CCM.TimelineDirection get_direction ();
		public uint get_duration ();
		public bool get_loop ();
		public uint get_n_frames ();
		public double get_progress ();
		public uint get_speed ();
		
		public void set_delay (uint msecs);
		public void set_direction (CCM.TimelineDirection direction);
		public void set_duration (uint msecs);
		public void set_loop (bool loop);
		public void set_n_frames (uint n_frames);
		public void set_speed (uint fps);
		
		public void add_marker_at_frame (string marker_name, uint frame_num);
		public void add_marker_at_time (string marker_name, uint msecs);
		public string list_markers (int frame_num, out uint n_markers);
		public void remove_marker (string marker_name);
		
		public void advance (uint frame_num);
		public void advance_to_marker (string marker_name);
		
		public void pause ();
		public void rewind ();
		public void skip (uint n_frames);
		public void start ();
		public void stop ();
		
		[HasEmitter]
		public virtual signal void completed ();
		[HasEmitter]
		public virtual signal void marker_reached (uint object, void* p0);
		[HasEmitter]
		public virtual signal void new_frame (int object);
		[HasEmitter]
		public virtual signal void paused ();
		[HasEmitter]
		public virtual signal void started ();
	}
	
	[Compact]
	[CCode (cheader_filename = "ccm-image.h", free_function = "ccm_image_destroy", cname = "CCMImage")]
	public class Image
	{
	    [CCode (cname = "ccm_image_new")]
    	public Image(CCM.Display display, X.Visual visual, Cairo.Format format, int width, int height, int depth);

    	public uint8 get_data ();
    	public int get_height ();
    	public int get_stride ();
    	public int get_width ();

    	public bool get_image (CCM.Pixmap pixmap, int x, int y);
    	public bool get_sub_image (CCM.Pixmap pixmap, int x, int y, int width, int height);
    	public bool put_image (CCM.Pixmap pixmap, int x_src, int y_src, int x, int y, int width, int height);
    }
    
	[CCode (cheader_filename = "ccm.h, ccm-region.h")]
	public struct RegionBox 
	{
		public int16 x1;
		public int16 y1;
		public int16 x2;
		public int16 y2;
	}

	[Compact]
	[CCode (cheader_filename = "ccm.h", copy_function = "ccm_region_copy", free_function = "ccm_region_destroy", cname = "CCMRegion")]
    public class Region
    {
	    [CCode (cname = "ccm_region_create")]
        public Region (int x, int y, int width, int height);
        [CCode (cname = "ccm_region_new")]
        public Region.empty ();
        [CCode (cname = "ccm_region_rectangle")]
        public Region.rectangle (Cairo.Rectangle rectangle);
	    [CCode (cname = "ccm_region_xrectangle")]  
    	public Region.xrectangle (X.RECTANGLE rectangle);
		public Region copy();
	    
        [CCode (cname = "ccm_region_empty")]
    	public bool is_empty ();
    	public CCM.RegionBox[] get_boxes (out int n_box);
    	public void get_clipbox (out Cairo.Rectangle clipbox);
		public void get_rectangles (out unowned Cairo.Rectangle[] rectangles);
    	public void region_get_xrectangles (out unowned X.RECTANGLE[] rectangles);

    	public void intersect (CCM.Region other);
    	public void offset (int dx, int dy);
    	public bool point_in (int x, int y);
    	public void resize (int width, int height);
    	public void scale (double scale_width, double scale_height);
    	public void subtract (CCM.Region other);
    	public void @union (CCM.Region other);
	    public void union_with_rect (Cairo.Rectangle rect);
	    public void union_with_xrect (X.RECTANGLE rect);

    	public void region_transform (Cairo.Matrix matrix);
    	public void region_transform_invert (Cairo.Matrix matrix);
	    public void device_transform (Cairo.Matrix matrix);
	    public void device_transform_invert (Cairo.Matrix matrix);
    }
    
	[CCode (cprefix = "CCM_PROPERTY_", cheader_filename = "ccm.h")]
	public enum PropertyType {
		HINT_TYPE,
		TRANSIENT,
		MWM_HINTS,
		WM_HINTS,
		OPACITY,
		STATE,
		FRAME_EXTENDS
	}
	
	[CCode (cprefix = "CCM_TIMELINE_", cheader_filename = "ccm.h")]
	public enum TimelineDirection {
		FORWARD,
		BACKWARD
	}
	
	[CCode (cprefix = "CCM_WINDOW_TYPE_", cheader_filename = "ccm.h")]
	public enum WindowType {
		UNKNOWN,
		DESKTOP,
		NORMAL,
		DIALOG,
		SPLASH,
		UTILITY,
		DND,
		TOOLTIP,
		NOTIFICATION,
		TOOLBAR,
		COMBO,
		DROPDOWN_MENU,
		POPUP_MENU,
		MENU,
		DOCK
	}
		
	[CCode (cheader_filename = "ccm-debug.h")]
	public static void debug (string format, ...);
	[CCode (cheader_filename = "ccm-debug.h")]
	public static void debug_atom (CCM.Display display, X.Atom atom, string format, ...);
	[CCode (cheader_filename = "ccm-debug.h")]
	public static void debug_print_backtrace ();
	[CCode (cheader_filename = "ccm-debug.h")]
	public static void debug_region (CCM.Drawable drawable, string format, ...);
	[CCode (cheader_filename = "ccm-debug.h")]
	public static void debug_window (CCM.Window window, string format, ...);
	
	[CCode (cheader_filename = "ccm-debug.h")]
	public static void log (string format, ...);
	[CCode (cheader_filename = "ccm-debug.h")]
	public static void log_atom (CCM.Display display, X.Atom atom, string format, ...);
	[CCode (cheader_filename = "ccm-debug.h")]
	public static void log_print_backtrace ();
	[CCode (cheader_filename = "ccm-debug.h")]
	public static void log_region (CCM.Drawable drawable, string format, ...);
	[CCode (cheader_filename = "ccm-debug.h")]
	public static void log_window (CCM.Window window, string format, ...);
}
