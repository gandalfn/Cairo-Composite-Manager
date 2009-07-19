/*
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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
 * CCMTimeoutPool: pool of timeout functions using the same slice of
 *                     the GLib main loop
 *
 * Author: Emmanuele Bassi <ebassi@openedhand.com>
 *
 * Based on similar code by Tristan van Berkom
 * Based on clutter_timeout_pool
 */

#include <gdk/gdk.h>

#include "ccm-timeout-pool.h"
#include "ccm-timeout-interval.h"

typedef struct _CCMTimeout CCMTimeout;
typedef enum
{
    CCM_TIMEOUT_NONE = 0,
    CCM_TIMEOUT_READY = 1 << 1,
    CCM_TIMEOUT_MASTER = 1 << 2
} CCMTimeoutFlags;

struct _CCMTimeout
{
    guint id;
    CCMTimeoutFlags flags;
    gint refcount;

    CCMTimeoutInterval interval;

    GSourceFunc func;
    gpointer data;
    GDestroyNotify notify;
};

struct _CCMTimeoutPool
{
    GSource source;

    guint next_id;

    GTimeVal start_time;
    GList *timeouts, *dispatched_timeouts;
    gint ready;

    guint id;
};

#define TIMEOUT_READY(timeout)   (timeout->flags & CCM_TIMEOUT_READY)
#define TIMEOUT_MASTER(timeout)  (timeout->flags & CCM_TIMEOUT_MASTER)

static gboolean ccm_timeout_pool_prepare (GSource * source,
                                          gint * next_timeout);
static gboolean ccm_timeout_pool_check (GSource * source);
static gboolean ccm_timeout_pool_dispatch (GSource * source,
                                           GSourceFunc callback, gpointer data);
static void ccm_timeout_pool_finalize (GSource * source);

static GSourceFuncs ccm_timeout_pool_funcs = {
    ccm_timeout_pool_prepare,
    ccm_timeout_pool_check,
    ccm_timeout_pool_dispatch,
    ccm_timeout_pool_finalize
};

static gint
ccm_timeout_sort (gconstpointer a, gconstpointer b)
{
    const CCMTimeout *t_a = a;
    const CCMTimeout *t_b = b;

    /* Keep 'ready' timeouts at the front */
    if (TIMEOUT_READY (t_a))
        return -1;

    if (TIMEOUT_READY (t_b))
        return 1;

    /* Keep 'master' timeouts at the front */
    if (TIMEOUT_MASTER (t_a))
        return -1;

    if (TIMEOUT_MASTER (t_b))
        return 1;

    return _ccm_timeout_interval_compare_expiration (&t_a->interval,
                                                     &t_b->interval);
}

static gint
ccm_timeout_find_by_id (gconstpointer a, gconstpointer b)
{
    const CCMTimeout *t_a = a;

    return t_a->id == GPOINTER_TO_UINT (b) ? 0 : 1;
}

static CCMTimeout *
ccm_timeout_new (guint fps)
{
    CCMTimeout *timeout;

    timeout = g_slice_new0 (CCMTimeout);
    _ccm_timeout_interval_init (&timeout->interval, fps);
    timeout->flags = CCM_TIMEOUT_NONE;
    timeout->refcount = 1;

    return timeout;
}

static gboolean
ccm_timeout_prepare (CCMTimeoutPool * pool, CCMTimeout * timeout,
                     gint * next_timeout)
{
    GTimeVal now;

    g_source_get_current_time (&pool->source, &now);

    return _ccm_timeout_interval_prepare (&now, &timeout->interval,
                                          next_timeout);
}

static CCMTimeout *
ccm_timeout_ref (CCMTimeout * timeout)
{
    g_return_val_if_fail (timeout != NULL, timeout);
    g_return_val_if_fail (timeout->refcount > 0, timeout);

    timeout->refcount += 1;

    return timeout;
}

static void
ccm_timeout_unref (CCMTimeout * timeout)
{
    g_return_if_fail (timeout != NULL);
    g_return_if_fail (timeout->refcount > 0);

    timeout->refcount -= 1;

    if (timeout->refcount == 0)
    {
        if (timeout->notify)
            timeout->notify (timeout->data);

        g_slice_free (CCMTimeout, timeout);
    }
}

static void
ccm_timeout_free (CCMTimeout * timeout)
{
    if (G_LIKELY (timeout))
    {
        if (timeout->notify)
            timeout->notify (timeout->data);

        g_slice_free (CCMTimeout, timeout);
    }
}

static gboolean
ccm_timeout_pool_prepare (GSource * source, gint * next_timeout)
{
    CCMTimeoutPool *pool = (CCMTimeoutPool *) source;
    GList *l = pool->timeouts;

    /* the pool is ready if the first timeout is ready */
    if (l && l->data)
    {
        CCMTimeout *timeout = l->data;
        return ccm_timeout_prepare (pool, timeout, next_timeout);
    }
    else
    {
        *next_timeout = -1;
        return FALSE;
    }
}

static gboolean
ccm_timeout_pool_check (GSource * source)
{
    CCMTimeoutPool *pool = (CCMTimeoutPool *) source;
    GList *l = pool->timeouts;

    gdk_threads_enter ();

    for (l = pool->timeouts; l; l = l->next)
    {
        CCMTimeout *timeout = l->data;

        /* since the timeouts are sorted by expiration, as soon
         * as we get a check returning FALSE we know that the
         * following timeouts are not expiring, so we break as
         * soon as possible
         */
        if (ccm_timeout_prepare (pool, timeout, NULL))
        {
            timeout->flags |= CCM_TIMEOUT_READY;
            pool->ready += 1;
        }
        else
            break;
    }

    gdk_threads_leave ();

    return (pool->ready > 0);
}

static gboolean
ccm_timeout_pool_dispatch (GSource * source, GSourceFunc func, gpointer data)
{
    CCMTimeoutPool *pool = (CCMTimeoutPool *) source;
    GList *dispatched_timeouts;

    /* the main loop might have predicted this, so we repeat the
     * check for ready timeouts.
     */
    if (!pool->ready)
        ccm_timeout_pool_check (source);

    gdk_threads_enter ();

    /* Iterate by moving the actual start of the list along so that it
     * can cope with adds and removes while a timeout is being dispatched
     */
    while (pool->timeouts && pool->timeouts->data && pool->ready-- > 0)
    {
        CCMTimeout *timeout = pool->timeouts->data;
        GList *l;

        /* One of the ready timeouts may have been removed during dispatch,
         * in which case pool->ready will be wrong, but the ready timeouts
         * are always kept at the start of the list so we can stop once
         * we've reached the first non-ready timeout
         */
        if (!(TIMEOUT_READY (timeout)))
            break;

        /* Add a reference to the timeout so it can't disappear
         * while it's being dispatched
         */
        ccm_timeout_ref (timeout);

        timeout->flags &= ~CCM_TIMEOUT_READY;

        /* Move the list node to a list of dispatched timeouts */
        l = pool->timeouts;
        if (l->next)
            l->next->prev = NULL;

        pool->timeouts = l->next;

        if (pool->dispatched_timeouts)
            pool->dispatched_timeouts->prev = l;

        l->prev = NULL;
        l->next = pool->dispatched_timeouts;
        pool->dispatched_timeouts = l;

        if (!_ccm_timeout_interval_dispatch
            (&timeout->interval, timeout->func, timeout->data))
        {
            /* The timeout may have already been removed, but nothing
             * can be added to the dispatched_timeout list except in this
             * function so it will always either be at the head of the
             * dispatched list or have been removed
             */
            if (pool->dispatched_timeouts
                && pool->dispatched_timeouts->data == timeout)
            {
                pool->dispatched_timeouts =
                    g_list_delete_link (pool->dispatched_timeouts,
                                        pool->dispatched_timeouts);

                /* Remove the reference that was held by it being in the list */
                ccm_timeout_unref (timeout);
            }
        }

        ccm_timeout_unref (timeout);
    }

    /* Re-insert the dispatched timeouts in sorted order */
    dispatched_timeouts = pool->dispatched_timeouts;
    while (dispatched_timeouts)
    {
        CCMTimeout *timeout = dispatched_timeouts->data;
        GList *next = dispatched_timeouts->next;

        if (timeout)
            pool->timeouts =
                g_list_insert_sorted (pool->timeouts, timeout,
                                      ccm_timeout_sort);

        dispatched_timeouts = next;
    }

    g_list_free (pool->dispatched_timeouts);
    pool->dispatched_timeouts = NULL;

    pool->ready = 0;

    gdk_threads_leave ();

    return TRUE;
}

static void
ccm_timeout_pool_finalize (GSource * source)
{
    CCMTimeoutPool *pool = (CCMTimeoutPool *) source;

    /* force destruction */
    g_list_foreach (pool->timeouts, (GFunc) ccm_timeout_free, NULL);
    g_list_free (pool->timeouts);
}

CCMTimeoutPool *
ccm_timeout_pool_new (gint priority)
{
    CCMTimeoutPool *pool;
    GSource *source;

    source = g_source_new (&ccm_timeout_pool_funcs, sizeof (CCMTimeoutPool));
    if (!source)
        return NULL;

    if (priority != G_PRIORITY_DEFAULT)
        g_source_set_priority (source, priority);

    pool = (CCMTimeoutPool *) source;
    g_get_current_time (&pool->start_time);
    pool->next_id = 1;
    pool->id = g_source_attach (source, NULL);
    g_source_unref (source);

    return pool;
}

guint
ccm_timeout_pool_add (CCMTimeoutPool * pool, guint fps, GSourceFunc func,
                      gpointer data, GDestroyNotify notify)
{
    CCMTimeout *timeout;
    guint retval = 0;

    timeout = ccm_timeout_new (fps);

    retval = timeout->id = pool->next_id++;

    timeout->func = func;
    timeout->data = data;
    timeout->notify = notify;

    pool->timeouts =
        g_list_insert_sorted (pool->timeouts, timeout, ccm_timeout_sort);

    return retval;
}

void
ccm_timeout_pool_remove (CCMTimeoutPool * pool, guint id)
{
    GList *l;

    if ((l =
         g_list_find_custom (pool->timeouts, GUINT_TO_POINTER (id),
                             ccm_timeout_find_by_id)))
    {
        ccm_timeout_unref (l->data);
        pool->timeouts = g_list_delete_link (pool->timeouts, l);
    }
    else if ((l =
              g_list_find_custom (pool->dispatched_timeouts,
                                  GUINT_TO_POINTER (id),
                                  ccm_timeout_find_by_id)))
    {
        ccm_timeout_unref (l->data);
        pool->dispatched_timeouts =
            g_list_delete_link (pool->dispatched_timeouts, l);
    }
}

void
ccm_timeout_set_master (CCMTimeoutPool * pool, guint id)
{
    GList *l;

    if ((l =
         g_list_find_custom (pool->timeouts, GUINT_TO_POINTER (id),
                             ccm_timeout_find_by_id)))
        ((CCMTimeout *) (l->data))->flags |= CCM_TIMEOUT_MASTER;
    else if ((l =
              g_list_find_custom (pool->dispatched_timeouts,
                                  GUINT_TO_POINTER (id),
                                  ccm_timeout_find_by_id)))
        ((CCMTimeout *) (l->data))->flags |= CCM_TIMEOUT_MASTER;
}
