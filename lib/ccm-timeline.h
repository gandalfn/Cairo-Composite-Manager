/* ccmtimeline.h: Class for time based operations
 *
 * Copyright (C) 2008 OpenedHand
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Based on similar code from Clutter
 * Authors:
 *      Matthew Allum <mallum@openedhand.com>
 *      Emmanuele Bassi <ebassi@openedhand.com>
 */

#ifndef _CCM_TIMELINE_H_
#define _CCM_TIMELINE_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define CCM_TYPE_TIMELINE               (ccm_timeline_get_type ())
#define CCM_TIMELINE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_TIMELINE, CCMTimeline))
#define CCM_IS_TIMELINE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_TIMELINE))
#define CCM_TIMELINE_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_TIMELINE, CCMTimelineClass))
#define CCM_IS_TIMELINE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_TIMELINE))
#define CCM_TIMELINE_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_TIMELINE, CCMTimelineClass))

typedef enum {
    CCM_TIMELINE_FORWARD,
    CCM_TIMELINE_BACKWARD
} CCMTimelineDirection;

typedef struct _CCMTimeline        CCMTimeline;
typedef struct _CCMTimelineClass   CCMTimelineClass; 
typedef struct _CCMTimelinePrivate CCMTimelinePrivate;

struct _CCMTimeline
{
    GObject parent;

    CCMTimelinePrivate *priv;
};

struct _CCMTimelineClass
{
    GObjectClass parent_class;

    void (* started)        (CCMTimeline *timeline);
    void (* completed)      (CCMTimeline *timeline);
    void (* paused)         (CCMTimeline *timeline);

    void (* new_frame)      (CCMTimeline *timeline,
                             gint         frame_num);

    void (* marker_reached) (CCMTimeline *timeline,
                             const gchar *marker_name,
                            gint         frame_num);
};

GType                ccm_timeline_get_type            (void) G_GNUC_CONST;

CCMTimeline *        ccm_timeline_new                 (guint        n_frames,
                                                       guint        fps);
CCMTimeline *        ccm_timeline_new_for_duration    (guint        msecs);
CCMTimeline *        ccm_timeline_clone               (CCMTimeline *timeline);

guint                ccm_timeline_get_duration        (CCMTimeline *timeline);
void                 ccm_timeline_set_duration        (CCMTimeline *timeline,
                                                       guint        msecs);
guint                ccm_timeline_get_speed           (CCMTimeline *timeline);
void                 ccm_timeline_set_speed           (CCMTimeline *timeline,
                                                       guint        fps);
CCMTimelineDirection ccm_timeline_get_direction       (CCMTimeline *timeline);
void                 ccm_timeline_set_direction       (CCMTimeline *timeline,
                                                       CCMTimelineDirection direction);
void                 ccm_timeline_start               (CCMTimeline *timeline);
void                 ccm_timeline_pause               (CCMTimeline *timeline);
void                 ccm_timeline_stop                (CCMTimeline *timeline);
void                 ccm_timeline_set_loop            (CCMTimeline *timeline,
                                                       gboolean     loop);
gboolean             ccm_timeline_get_loop            (CCMTimeline *timeline);
void                 ccm_timeline_rewind              (CCMTimeline *timeline);
void                 ccm_timeline_skip                (CCMTimeline *timeline,
                                                       guint        n_frames);
void                 ccm_timeline_advance             (CCMTimeline *timeline,
                                                       guint        frame_num);
gint                 ccm_timeline_get_current_frame   (CCMTimeline *timeline);
gdouble              ccm_timeline_get_progress        (CCMTimeline *timeline);
void                 ccm_timeline_set_n_frames        (CCMTimeline *timeline,
                                                       guint        n_frames);
guint                ccm_timeline_get_n_frames        (CCMTimeline *timeline);
gboolean             ccm_timeline_is_playing          (CCMTimeline *timeline);
void                 ccm_timeline_set_delay           (CCMTimeline *timeline,
                                                       guint        msecs);
guint                ccm_timeline_get_delay           (CCMTimeline *timeline);
guint                ccm_timeline_get_delta           (CCMTimeline *timeline,
                                                       guint       *msecs);

void                 ccm_timeline_add_marker_at_frame (CCMTimeline *timeline,
                                                       const gchar *marker_name,
                                                       guint        frame_num);
void                 ccm_timeline_add_marker_at_time  (CCMTimeline *timeline,
                                                       const gchar *marker_name,
                                                       guint        msecs);
void                 ccm_timeline_remove_marker       (CCMTimeline *timeline,
                                                       const gchar *marker_name);
gchar **             ccm_timeline_list_markers        (CCMTimeline *timeline,
                                                       gint         frame_num,
                                                       guint       *n_markers) G_GNUC_MALLOC;
void                 ccm_timeline_advance_to_marker   (CCMTimeline *timeline,
                                                       const gchar *marker_name);
void                 ccm_timeline_set_master          (CCMTimeline *self, 
                                                       gboolean master);
gboolean             ccm_timeline_get_master          (CCMTimeline *self);

G_END_DECLS

#endif /* _CCM_TIMELINE_H_ */
