/*
 * Authored By Neil Roberts  <neil@linux.intel.com>
 *
 * Copyright (C) 2009  Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Based on clutter_timeout_interval
 */

#include "ccm-timeout-interval.h"

void
_ccm_timeout_interval_init (CCMTimeoutInterval * self, guint fps)
{
    g_get_current_time (&self->start_time);
    self->fps = fps;
    self->frame_count = 0;
}

static guint
_ccm_timeout_interval_get_ticks (const GTimeVal * current_time,
                                 CCMTimeoutInterval * self)
{
    return ((current_time->tv_sec - self->start_time.tv_sec) * 1000 +
            (current_time->tv_usec - self->start_time.tv_usec) / 1000);
}

gboolean
_ccm_timeout_interval_prepare (const GTimeVal * current_time,
                               CCMTimeoutInterval * self, gint * delay)
{
    guint elapsed_time = _ccm_timeout_interval_get_ticks (current_time, self);
    guint new_frame_num = elapsed_time * self->fps / 1000;

    if (new_frame_num < self->frame_count
        || new_frame_num - self->frame_count > 2)
    {
        guint frame_time = (1000 + self->fps - 1) / self->fps;

        self->start_time = *current_time;
        g_time_val_add (&self->start_time, -(gint) frame_time * 1000);

        self->frame_count = 0;

        if (delay)
            *delay = 0;
        return TRUE;
    }
    else if (new_frame_num > self->frame_count)
    {
        if (delay)
            *delay = 0;
        return TRUE;
    }
    else
    {
        if (delay)
            *delay =
                ((self->frame_count + 1) * 1000 / self->fps - elapsed_time);
        return FALSE;
    }
}

gboolean
_ccm_timeout_interval_dispatch (CCMTimeoutInterval * self, GSourceFunc callback,
                                gpointer user_data)
{
    if ((*callback) (user_data))
    {
        self->frame_count++;
        return TRUE;
    }
    else
        return FALSE;
}

gint
_ccm_timeout_interval_compare_expiration (const CCMTimeoutInterval * a,
                                          const CCMTimeoutInterval * b)
{
    guint a_delay = 1000 / a->fps;
    guint b_delay = 1000 / b->fps;
    glong b_difference;
    gint comparison;

    b_difference =
        ((a->start_time.tv_sec - b->start_time.tv_sec) * 1000 +
         (a->start_time.tv_usec - b->start_time.tv_usec) / 1000);

    comparison =
        ((gint) ((a->frame_count + 1) * a_delay) -
         (gint) ((b->frame_count + 1) * b_delay + b_difference));

    return (comparison < 0 ? -1 : comparison > 0 ? 1 : 0);
}
