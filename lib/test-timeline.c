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

#include <gtk/gtk.h>
#include "ccm-timeline.h"

GTimer* timer = NULL;

void
on_new_frame (CCMTimeline* timeline, int frame_num)
{
    g_print("Frame num = %i, %f\n", frame_num, g_timer_elapsed(timer, NULL));
    g_timer_reset(timer);
}

gint
main (gint argc, gchar ** argv)
{
    gtk_init (&argc, &argv);

    timer = g_timer_new();
    
    CCMTimeline* timeline = ccm_timeline_new(10, 10);
    g_signal_connect (timeline, "new-frame", G_CALLBACK (on_new_frame), NULL);
    ccm_timeline_set_master (timeline, TRUE);
    ccm_timeline_set_loop (timeline, TRUE);
    ccm_timeline_start (timeline);
    gtk_main ();

    return 0;
}
