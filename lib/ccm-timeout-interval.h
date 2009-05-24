/*
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

#ifndef __CCM_TIMEOUT_INTERVAL_H__
#define __CCM_TIMEOUT_INTERVAL_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _CCMTimeoutInterval CCMTimeoutInterval;

struct _CCMTimeoutInterval
{
    GTimeVal start_time;
    guint frame_count, fps;
};

void _ccm_timeout_interval_init (CCMTimeoutInterval *self, guint fps);

gboolean _ccm_timeout_interval_prepare (const GTimeVal *current_time,
                                        CCMTimeoutInterval *self,
                                        gint *delay);

gboolean _ccm_timeout_interval_dispatch (CCMTimeoutInterval *self,
                                         GSourceFunc        callback,
                                         gpointer           user_data);

gint _ccm_timeout_interval_compare_expiration (const CCMTimeoutInterval *a,
                                               const CCMTimeoutInterval *b);

G_END_DECLS

#endif /* __CCM_TIMEOUT_INTERVAL_H__ */
