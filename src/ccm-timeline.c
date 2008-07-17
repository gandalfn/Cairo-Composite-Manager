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

#include "ccm-debug.h"
#include "ccm-timeout-pool.h"
#include "ccm-timeline.h"

G_DEFINE_TYPE (CCMTimeline, ccm_timeline, G_TYPE_OBJECT);

#define FPS_TO_INTERVAL(f)          (1000 / (f))
#define CCM_TIMELINE_PRIORITY       (G_PRIORITY_DEFAULT + 30)

struct _CCMTimelinePrivate
{
    CCMTimelineDirection direction;

    guint timeout_id;
    guint delay_id;

    gint current_frame_num;

    guint fps;
    guint n_frames;
    guint delay;
    guint duration;

    gint skipped_frames;

    GTimeVal prev_frame_timeval;
    guint  msecs_delta;

    GHashTable *markers_by_frame;
    GHashTable *markers_by_name;

    guint loop : 1;
};

typedef struct {
    gchar *name;
    guint frame_num;
    GQuark quark;
} TimelineMarker;

enum
{
    PROP_0,

    PROP_FPS,
    PROP_NUM_FRAMES,
    PROP_LOOP,
    PROP_DELAY,
    PROP_DURATION,
    PROP_DIRECTION
};

enum
{
    NEW_FRAME,
    STARTED,
    PAUSED,
    COMPLETED,
    MARKER_REACHED,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

#define CCM_TIMELINE_GET_PRIVATE(o)  \
   ((CCMTimelinePrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_TIMELINE, CCMTimelineClass))

static CCMTimeoutPool *timeline_pool = NULL;

static guint
timeout_add (guint          interval, GSourceFunc    func,
             gpointer       data,
             GDestroyNotify notify)
{
    return ccm_timeout_pool_add (timeline_pool, interval, 
                                 func, data, notify);
}

static void
timeout_remove (guint tag)
{
    ccm_timeout_pool_remove (timeline_pool, tag);
}

static TimelineMarker *
timeline_marker_new (const gchar *name,
                     guint        frame_num)
{
    TimelineMarker *marker = g_slice_new0 (TimelineMarker);

    marker->name = g_strdup (name);
    marker->quark = g_quark_from_string (marker->name);
    marker->frame_num = frame_num;

    return marker;
}

static void
timeline_marker_free (gpointer data)
{
    if (G_LIKELY (data))
    {
        TimelineMarker *marker = data;

        g_free (marker->name);
        g_slice_free (TimelineMarker, marker);
    }
}

static void
ccm_timeline_set_property (GObject* object, guint prop_id,
                           const GValue *value, GParamSpec *pspec)
{
    CCMTimeline *self = CCM_TIMELINE (object);
    
    switch (prop_id)
    {
        case PROP_FPS:
            ccm_timeline_set_speed (self, g_value_get_uint (value));
            break;
        case PROP_NUM_FRAMES:
            self->priv->n_frames = g_value_get_uint (value);
            break;
        case PROP_LOOP:
            self->priv->loop = g_value_get_boolean (value);
            break;
        case PROP_DELAY:
            self->priv->delay = g_value_get_uint (value);
            break;
        case PROP_DURATION:
            ccm_timeline_set_duration (self, g_value_get_uint (value));
            break;
        case PROP_DIRECTION:
            ccm_timeline_set_direction (self, g_value_get_enum (value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
ccm_timeline_get_property (GObject *object, guint prop_id,
                           GValue *value, GParamSpec *pspec)
{
    CCMTimeline *self= CCM_TIMELINE (object);
    
    switch (prop_id)
    {
        case PROP_FPS:
            g_value_set_uint (value, self->priv->fps);
            break;
        case PROP_NUM_FRAMES:
            g_value_set_uint (value, self->priv->n_frames);
            break;
        case PROP_LOOP:
            g_value_set_boolean (value, self->priv->loop);
            break;
        case PROP_DELAY:
            g_value_set_uint (value, self->priv->delay);
            break;
        case PROP_DURATION:
            g_value_set_uint (value, self->priv->duration);
            break;
        case PROP_DIRECTION:
            g_value_set_enum (value, self->priv->direction);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
ccm_timeline_finalize (GObject *object)
{
    CCMTimeline* self = CCM_TIMELINE(object);
    
    g_hash_table_destroy (self->priv->markers_by_frame);
    g_hash_table_destroy (self->priv->markers_by_name);

    G_OBJECT_CLASS (ccm_timeline_parent_class)->finalize (object);
}

static void
ccm_timeline_dispose (GObject *object)
{
    CCMTimeline *self = CCM_TIMELINE(object);
    
    if (self->priv->delay_id)
    {
        timeout_remove (self->priv->delay_id);
        self->priv->delay_id = 0;
    }

    if (self->priv->timeout_id)
    {
        timeout_remove (self->priv->timeout_id);
        self->priv->timeout_id = 0;
    }

    G_OBJECT_CLASS (ccm_timeline_parent_class)->dispose (object);
}

static void
ccm_timeline_class_init (CCMTimelineClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    timeline_pool = ccm_timeout_pool_new (CCM_TIMELINE_PRIORITY);
    
    g_type_class_add_private (klass, sizeof (CCMTimelinePrivate));

    object_class->set_property = ccm_timeline_set_property;
    object_class->get_property = ccm_timeline_get_property;
    object_class->finalize     = ccm_timeline_finalize;
    object_class->dispose      = ccm_timeline_dispose;
    
    g_object_class_install_property (object_class,
                                     PROP_FPS,
                                     g_param_spec_uint ("fps",
                                                        "Frames Per Second",
                                                        "Timeline frames per second",
                                                        1, 1000,
                                                        60,
                                                        G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_NUM_FRAMES,
                                     g_param_spec_uint ("num-frames",
                                                        "Total number of frames",
                                                        "Timelines total number of frames",
                                                        1, G_MAXUINT,
                                                        1,
                                                        G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_LOOP,
                                     g_param_spec_boolean ("loop",
                                                           "Loop",
                                                           "Should the timeline automatically restart",
                                                           FALSE,
                                                           G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_DELAY,
                                     g_param_spec_uint ("delay",
                                                        "Delay",
                                                        "Delay before start",
                                                        0, G_MAXUINT,
                                                        0,
                                                        G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_DURATION,
                                     g_param_spec_uint ("duration",
                                                        "Duration",
                                                        "Duration of the timeline in milliseconds",
                                                        0, G_MAXUINT,
                                                        1000,
                                                        G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_DIRECTION,
                                     g_param_spec_uint ("direction",
                                                        "Direction",
                                                        "Direction of the timeline",
                                                        CCM_TIMELINE_FORWARD,
                                                        CCM_TIMELINE_BACKWARD,
                                                        CCM_TIMELINE_FORWARD,
                                                        G_PARAM_READWRITE));

    signals[NEW_FRAME] = g_signal_new ("new-frame",
                                       G_TYPE_FROM_CLASS (object_class),
                                       G_SIGNAL_RUN_LAST,
                                       G_STRUCT_OFFSET (CCMTimelineClass, new_frame),
                                       NULL, NULL,
                                       g_cclosure_marshal_VOID__INT,
                                       G_TYPE_NONE, 1, G_TYPE_INT);
    signals[COMPLETED] = g_signal_new ("completed",     
                                       G_TYPE_FROM_CLASS (object_class),
                                       G_SIGNAL_RUN_LAST,
                                       G_STRUCT_OFFSET (CCMTimelineClass, completed),
                                       NULL, NULL,
                                       g_cclosure_marshal_VOID__VOID,
                                       G_TYPE_NONE, 0);
    signals[STARTED] = g_signal_new ("started",
                                     G_TYPE_FROM_CLASS (object_class),
                                     G_SIGNAL_RUN_LAST,
                                     G_STRUCT_OFFSET (CCMTimelineClass, started),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
    signals[PAUSED] = g_signal_new ("paused",
                                    G_TYPE_FROM_CLASS (object_class),
                                    G_SIGNAL_RUN_LAST,
                                    G_STRUCT_OFFSET (CCMTimelineClass, paused),
                                    NULL, NULL,
                                    g_cclosure_marshal_VOID__VOID,
                                    G_TYPE_NONE, 0);
    signals[MARKER_REACHED] = g_signal_new ("marker-reached",
                                            G_TYPE_FROM_CLASS (object_class),
                                            G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | 
                                            G_SIGNAL_DETAILED | G_SIGNAL_NO_HOOKS,
                                            G_STRUCT_OFFSET (CCMTimelineClass, marker_reached),
                                            NULL, NULL,
                                            g_cclosure_marshal_VOID__UINT_POINTER,
                                            G_TYPE_NONE, 2, 
                                            G_TYPE_UINT, G_TYPE_POINTER);
}

static void
ccm_timeline_init (CCMTimeline *self)
{
    self->priv = CCM_TIMELINE_GET_PRIVATE(self);
    self->priv->fps = 60;
    self->priv->n_frames = 0;
    self->priv->msecs_delta = 0;

    self->priv->markers_by_frame = g_hash_table_new (NULL, NULL);
    self->priv->markers_by_name = g_hash_table_new_full (g_str_hash, 
                                                         g_str_equal,
                                                         NULL,
                                                         timeline_marker_free);
}

static gboolean
timeline_timeout_func (CCMTimeline *self)
{
    GTimeVal timeval;
    guint n_frames;
    gulong msecs;

    g_object_ref (self);

    g_get_current_time (&timeval);

    if (!self->priv->prev_frame_timeval.tv_sec)
        self->priv->prev_frame_timeval = timeval;
    
    msecs = (timeval.tv_sec - self->priv->prev_frame_timeval.tv_sec) * 1000;
    msecs += (timeval.tv_usec - self->priv->prev_frame_timeval.tv_usec) / 1000;
    self->priv->msecs_delta = msecs;
    n_frames = msecs / (1000 / self->priv->fps);
    if (n_frames == 0)
        n_frames = 1;

    self->priv->skipped_frames = n_frames - 1;

    self->priv->prev_frame_timeval = timeval;

    if (self->priv->direction == CCM_TIMELINE_FORWARD)
        self->priv->current_frame_num += n_frames;
    else
        self->priv->current_frame_num -= n_frames;

    if (!(((self->priv->direction == CCM_TIMELINE_FORWARD) &&
           (self->priv->current_frame_num >= self->priv->n_frames)) ||
          ((self->priv->direction == CCM_TIMELINE_BACKWARD) &&
           (self->priv->current_frame_num <= 0))))
    {
        gint i;

        g_signal_emit (self, signals[NEW_FRAME], 0,
                       self->priv->current_frame_num);

        for (i = self->priv->skipped_frames; i >= 0; i--)
        {
            gint frame_num = self->priv->current_frame_num - i;
            GSList *markers, *l;
          
            markers = g_hash_table_lookup (self->priv->markers_by_frame,
                                           GUINT_TO_POINTER (frame_num));
            for (l = markers; l; l = l->next)
            {
                TimelineMarker *marker = l->data;

                g_signal_emit (self, signals[MARKER_REACHED],
                               marker->quark,
                               marker->frame_num,
                               marker->name);
            }
        }

        if (!self->priv->timeout_id)
        {
            g_object_unref (self);
            return FALSE;
        }
        
        g_object_unref (self);
        return TRUE;
    }
    else
    {
        CCMTimelineDirection saved_direction = self->priv->direction;
        guint overflow_frame_num = self->priv->current_frame_num;
        gint end_frame;
  
        if (self->priv->direction == CCM_TIMELINE_FORWARD)
            self->priv->current_frame_num = self->priv->n_frames;
        else if (self->priv->direction == CCM_TIMELINE_BACKWARD)
            self->priv->current_frame_num = 0;

        end_frame = self->priv->current_frame_num;

        g_signal_emit (self, signals[NEW_FRAME], 0,
                       self->priv->current_frame_num);

        if (self->priv->current_frame_num != end_frame)
        {
            g_object_unref (self);
            return TRUE;
        }

        if (!self->priv->loop && self->priv->timeout_id)
        {
            timeout_remove (self->priv->timeout_id);
            self->priv->timeout_id = 0;
        }

        g_signal_emit (self, signals[COMPLETED], 0);

        if (self->priv->current_frame_num != end_frame && 
            !((self->priv->current_frame_num == 0 && 
               end_frame == self->priv->n_frames) ||
              (self->priv->current_frame_num == self->priv->n_frames && 
               end_frame == 0)))
        {
            g_object_unref (self);
            return TRUE;
        }

        if (self->priv->loop)
        {
            if (saved_direction == CCM_TIMELINE_FORWARD)
                self->priv->current_frame_num = overflow_frame_num - 
                                                self->priv->n_frames;
            else
                self->priv->current_frame_num = self->priv->n_frames +
                                                overflow_frame_num;

            if (self->priv->direction != saved_direction)
            {
                self->priv->current_frame_num = self->priv->n_frames -
                                                self->priv->current_frame_num;
            }

            g_object_unref (self);
            return TRUE;
        }
        else
        {
            ccm_timeline_rewind (self);

            self->priv->prev_frame_timeval.tv_sec = 0;
            self->priv->prev_frame_timeval.tv_usec = 0;

            g_object_unref (self);
            return FALSE;
        }
    }
}

static guint
timeline_timeout_add (CCMTimeline *self, guint interval, GSourceFunc func,
                      gpointer data, GDestroyNotify notify)
{
    GTimeVal timeval;

    if (self->priv->prev_frame_timeval.tv_sec == 0)
    {
        g_get_current_time (&timeval);
        self->priv->prev_frame_timeval = timeval;
    }
    self->priv->skipped_frames   = 0;
    self->priv->msecs_delta      = 0;

    return timeout_add (interval, func, data, notify);
}

static gboolean
delay_timeout_func (CCMTimeline* self)
{
    self->priv->delay_id = 0;

    self->priv->timeout_id = timeline_timeout_add (self,
                                                   FPS_TO_INTERVAL (self->priv->fps),
                                                   (GSourceFunc)timeline_timeout_func,
                                                   self, NULL);

    g_signal_emit (self, signals[STARTED], 0);

    return FALSE;
}

void
ccm_timeline_start (CCMTimeline *self)
{
    g_return_if_fail (CCM_IS_TIMELINE (self));

    if (self->priv->delay_id || self->priv->timeout_id)
        return;

    if (self->priv->n_frames == 0)
        return;

    if (self->priv->delay)
    {
        self->priv->delay_id = timeout_add (self->priv->delay,
                                            (GSourceFunc)delay_timeout_func,
                                            self, NULL);
    }
    else
    {
        self->priv->timeout_id = timeline_timeout_add (self,
                                                       FPS_TO_INTERVAL (self->priv->fps),
                                                       (GSourceFunc)timeline_timeout_func,
                                                       self, NULL);

        g_signal_emit (self, signals[STARTED], 0);
    }
}

void
ccm_timeline_pause (CCMTimeline *self)
{
    g_return_if_fail (CCM_IS_TIMELINE (self));

    if (self->priv->delay_id)
    {
        timeout_remove (self->priv->delay_id);
        self->priv->delay_id = 0;
    }

    if (self->priv->timeout_id)
    {
        timeout_remove (self->priv->timeout_id);
        self->priv->timeout_id = 0;
    }

    self->priv->prev_frame_timeval.tv_sec = 0;
    self->priv->prev_frame_timeval.tv_usec = 0;

    g_signal_emit (self, signals[PAUSED], 0);
}

void
ccm_timeline_stop (CCMTimeline *self)
{
    ccm_timeline_pause (self);
    ccm_timeline_rewind (self);
}

void
ccm_timeline_set_loop (CCMTimeline *self, gboolean loop)
{
    g_return_if_fail (CCM_IS_TIMELINE (self));

    if (self->priv->loop != loop)
    {
        self->priv->loop = loop;

        g_object_notify (G_OBJECT (self), "loop");
    }
}

gboolean
ccm_timeline_get_loop (CCMTimeline *self)
{
    g_return_val_if_fail (CCM_IS_TIMELINE (self), FALSE);

    return self->priv->loop;
}

void
ccm_timeline_rewind (CCMTimeline *self)
{
    g_return_if_fail (CCM_IS_TIMELINE (self));

    if (self->priv->direction == CCM_TIMELINE_FORWARD)
        ccm_timeline_advance (self, 0);
    else if (self->priv->direction == CCM_TIMELINE_BACKWARD)
        ccm_timeline_advance (self, self->priv->n_frames);
}

void
ccm_timeline_skip (CCMTimeline *self, guint n_frames)
{
  g_return_if_fail (CCM_IS_TIMELINE (self));

    if (self->priv->direction == CCM_TIMELINE_FORWARD)
    {
        self->priv->current_frame_num += n_frames;

        if (self->priv->current_frame_num > self->priv->n_frames)
            self->priv->current_frame_num = 1;
    }
    else if (self->priv->direction == CCM_TIMELINE_BACKWARD)
    {
        self->priv->current_frame_num -= n_frames;

        if (self->priv->current_frame_num < 1)
            self->priv->current_frame_num = self->priv->n_frames - 1;
    }
}

void
ccm_timeline_advance (CCMTimeline *self, guint frame_num)
{
    g_return_if_fail (CCM_IS_TIMELINE (self));

    self->priv->current_frame_num = CLAMP (frame_num, 0, self->priv->n_frames);
}

gint
ccm_timeline_get_current_frame (CCMTimeline *self)
{
    g_return_val_if_fail (CCM_IS_TIMELINE (self), 0);

    return self->priv->current_frame_num;
}

guint
ccm_timeline_get_n_frames (CCMTimeline *self)
{
    g_return_val_if_fail (CCM_IS_TIMELINE (self), 0);

    return self->priv->n_frames;
}

void
ccm_timeline_set_n_frames (CCMTimeline *self, guint n_frames)
{
    g_return_if_fail (CCM_IS_TIMELINE (self));
    g_return_if_fail (n_frames > 0);

    if (self->priv->n_frames != n_frames)
    {
        self->priv->n_frames = n_frames;

        g_object_notify (G_OBJECT (self), "num-frames");
    }
}

void
ccm_timeline_set_speed (CCMTimeline *self, guint fps)
{
    g_return_if_fail (CCM_IS_TIMELINE (self));
    g_return_if_fail (fps > 0);

    if (self->priv->fps != fps)
    {
        g_object_ref (self);

        self->priv->fps = fps;

        if (self->priv->timeout_id)
        {
            timeout_remove (self->priv->timeout_id);

            self->priv->timeout_id = timeline_timeout_add (self,
                                                           FPS_TO_INTERVAL (self->priv->fps),
                                                           (GSourceFunc)timeline_timeout_func,
                                                           self, NULL);
        }

        g_object_notify (G_OBJECT (self), "fps");
        g_object_unref (self);
    }
}

guint
ccm_timeline_get_speed (CCMTimeline *self)
{
    g_return_val_if_fail (CCM_IS_TIMELINE (self), 0);

    return self->priv->fps;
}

gboolean
ccm_timeline_is_playing (CCMTimeline *self)
{
    g_return_val_if_fail (CCM_IS_TIMELINE (self), FALSE);

    return (self->priv->timeout_id != 0);
}

CCMTimeline *
ccm_timeline_clone (CCMTimeline *self)
{
    CCMTimeline *copy;

    g_return_val_if_fail (CCM_IS_TIMELINE (self), NULL);

    copy = g_object_new (CCM_TYPE_TIMELINE,
                         "fps", ccm_timeline_get_speed (self),
                         "num-frames", ccm_timeline_get_n_frames (self),
                         "loop", ccm_timeline_get_loop (self),
                         "delay", ccm_timeline_get_delay (self),
                         "direction", ccm_timeline_get_direction (self),
                         NULL);

    return copy;
}

CCMTimeline *
ccm_timeline_new_for_duration (guint msecs)
{
    g_return_val_if_fail (msecs > 0, NULL);

    return g_object_new (CCM_TYPE_TIMELINE, "duration", msecs, NULL);
}

CCMTimeline*
ccm_timeline_new (guint n_frames, guint fps)
{
    g_return_val_if_fail (n_frames > 0, NULL);
    g_return_val_if_fail (fps > 0, NULL);

    return g_object_new (CCM_TYPE_TIMELINE, "fps", fps, 
                         "num-frames", n_frames,
                         NULL);
}

guint
ccm_timeline_get_delay (CCMTimeline *self)
{
    g_return_val_if_fail (CCM_IS_TIMELINE (self), 0);

    return self->priv->delay;
}

void
ccm_timeline_set_delay (CCMTimeline *self, guint msecs)
{
    g_return_if_fail (CCM_IS_TIMELINE (self));

    if (self->priv->delay != msecs)
    {
        self->priv->delay = msecs;
        g_object_notify (G_OBJECT (self), "delay");
    }
}

guint
ccm_timeline_get_duration (CCMTimeline *self)
{
  g_return_val_if_fail (CCM_IS_TIMELINE (self), 0);

  return self->priv->duration;
}

void
ccm_timeline_set_duration (CCMTimeline *self, guint msecs)
{
    g_return_if_fail (CCM_IS_TIMELINE (self));

    if (self->priv->duration != msecs)
    {
        g_object_ref (self);

        g_object_freeze_notify (G_OBJECT (self));

        self->priv->duration = msecs;

        self->priv->n_frames = self->priv->duration * self->priv->fps / 1000;

        g_object_notify (G_OBJECT (self), "num-frames");
        g_object_notify (G_OBJECT (self), "duration");

        g_object_thaw_notify (G_OBJECT (self));
        g_object_unref (self);
    }
}

gdouble
ccm_timeline_get_progress (CCMTimeline *self)
{
    g_return_val_if_fail (CCM_IS_TIMELINE (self), 0.);

    if (!ccm_timeline_is_playing (self))
    {
        if (self->priv->direction == CCM_TIMELINE_FORWARD)
            return 0.0;
        else
            return 1.0;
    }

    return CLAMP ((gdouble)self->priv->current_frame_num /
                  (gdouble)self->priv->n_frames, 0.0, 1.0);
}

CCMTimelineDirection
ccm_timeline_get_direction (CCMTimeline *self)
{
    g_return_val_if_fail (CCM_IS_TIMELINE (self), CCM_TIMELINE_FORWARD);

    return self->priv->direction;
}

void
ccm_timeline_set_direction (CCMTimeline *self, CCMTimelineDirection direction)
{
    g_return_if_fail (CCM_IS_TIMELINE (self));

    if (self->priv->direction != direction)
    {
        self->priv->direction = direction;

        if (self->priv->current_frame_num == 0)
            self->priv->current_frame_num = self->priv->n_frames;

        g_object_notify (G_OBJECT (self), "direction");
    }
}

guint
ccm_timeline_get_delta (CCMTimeline *self, guint *msecs)
{
    g_return_val_if_fail (CCM_IS_TIMELINE (self), 0);

    if (!ccm_timeline_is_playing (self))
    {
        if (msecs)
            *msecs = 0;

        return 0;
    }

    if (msecs)
        *msecs = self->priv->msecs_delta;

    return self->priv->skipped_frames + 1;
}

static inline void
ccm_timeline_add_marker_internal (CCMTimeline *self,
                                  const gchar *marker_name,
                                  guint        frame_num)
{
    TimelineMarker *marker;
    GSList *markers;

    marker = g_hash_table_lookup (self->priv->markers_by_name, marker_name);
    if (G_UNLIKELY (marker))
    {
        g_warning ("A marker named `%s' already exists on frame %d",
                   marker->name,
                   marker->frame_num);
        return;
    }

    marker = timeline_marker_new (marker_name, frame_num);
    g_hash_table_insert (self->priv->markers_by_name, marker->name, marker);

    markers = g_hash_table_lookup (self->priv->markers_by_frame,
                                   GUINT_TO_POINTER (frame_num));
    if (!markers)
    {
        markers = g_slist_prepend (NULL, marker);
        g_hash_table_insert (self->priv->markers_by_frame,
                             GUINT_TO_POINTER (frame_num),
                             markers);
    }
    else
    {
        markers = g_slist_prepend (markers, marker);
        g_hash_table_replace (self->priv->markers_by_frame,
                              GUINT_TO_POINTER (frame_num),
                              markers);
    }
}

void
ccm_timeline_add_marker_at_frame (CCMTimeline *self,
                                  const gchar *marker_name,
                                  guint        frame_num)
{
    g_return_if_fail (CCM_IS_TIMELINE (self));
    g_return_if_fail (marker_name != NULL);
    g_return_if_fail (frame_num <= ccm_timeline_get_n_frames (self));

    ccm_timeline_add_marker_internal (self, marker_name, frame_num);
}

void
ccm_timeline_add_marker_at_time (CCMTimeline *self,
                                 const gchar *marker_name,
                                 guint        msecs)
{
    guint frame_num;

    g_return_if_fail (CCM_IS_TIMELINE (self));
    g_return_if_fail (marker_name != NULL);
    g_return_if_fail (msecs <= ccm_timeline_get_duration (self));

    frame_num = msecs * self->priv->fps / 1000;

    ccm_timeline_add_marker_internal (self, marker_name, frame_num);
}

gchar **
ccm_timeline_list_markers (CCMTimeline *self,
                           gint         frame_num,
                           guint       *n_markers)
{
    gchar **retval = NULL;
    gint i;

	g_return_val_if_fail (CCM_IS_TIMELINE (self), NULL);

    if (frame_num < 0)
    {
        GList *markers, *l;

        markers = g_hash_table_get_keys (self->priv->markers_by_name);
        retval = g_new0 (gchar*, g_list_length (markers) + 1);

        for (i = 0, l = markers; l != NULL; i++, l = l->next)
            retval[i] = g_strdup (l->data);

        g_list_free (markers);
    }
    else
    {
        GSList *markers, *l;

        markers = g_hash_table_lookup (self->priv->markers_by_frame,
                                       GUINT_TO_POINTER (frame_num));
        retval = g_new0 (gchar*, g_slist_length (markers) + 1);

        for (i = 0, l = markers; l != NULL; i++, l = l->next)
            retval[i] = g_strdup (l->data);
    }

    if (n_markers)
        *n_markers = i;

    return retval;
}

void
ccm_timeline_advance_to_marker (CCMTimeline *self, const gchar *marker_name)
{
    TimelineMarker *marker;

    g_return_if_fail (CCM_IS_TIMELINE (self));
    g_return_if_fail (marker_name != NULL);

    marker = g_hash_table_lookup (self->priv->markers_by_name, marker_name);
    if (!marker)
    {
        g_warning ("No marker named `%s' found.", marker_name);
        return;
    }

    ccm_timeline_advance (self, marker->frame_num);
}

void
ccm_timeline_remove_marker (CCMTimeline *self, const gchar *marker_name)
{
    TimelineMarker *marker;
    GSList *markers;

    g_return_if_fail (CCM_IS_TIMELINE (self));
    g_return_if_fail (marker_name != NULL);

    marker = g_hash_table_lookup (self->priv->markers_by_name, marker_name);
    if (!marker)
    {
        g_warning ("No marker named `%s' found.", marker_name);
        return;
    }

    markers = g_hash_table_lookup (self->priv->markers_by_frame,
                                   GUINT_TO_POINTER (marker->frame_num));
    if (G_LIKELY (markers))
    {
        markers = g_slist_remove (markers, marker);
        if (!markers)
        {
            g_hash_table_remove (self->priv->markers_by_frame,
                                 GUINT_TO_POINTER (marker->frame_num));
        }
        else
            g_hash_table_replace (self->priv->markers_by_frame,
                                  GUINT_TO_POINTER (marker->frame_num),
                                  markers);
    }
    else
    {
        g_warning ("Dangling marker %s at frame %d",
                   marker->name,
                   marker->frame_num);
    }

    g_hash_table_remove (self->priv->markers_by_name, marker_name);
}
