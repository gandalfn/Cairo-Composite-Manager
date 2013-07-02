/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * test-timeline.c
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
    GMainLoop* loop = g_main_loop_new (NULL, FALSE);
    timer = g_timer_new();

    CCMTimeline* timeline = ccm_timeline_new(10, 10);
    g_signal_connect (timeline, "new-frame", G_CALLBACK (on_new_frame), NULL);
    ccm_timeline_set_master (timeline, TRUE);
    ccm_timeline_set_loop (timeline, TRUE);
    ccm_timeline_start (timeline);
    g_main_loop_run (loop);

    return 0;
}
