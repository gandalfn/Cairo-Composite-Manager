/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- */

/*   - Library for asynchronous communication
 *  Copyright (C) 2002  Søren Sandmann (sandmann@daimi.au.dk)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 */

#include <glib.h>
#include "ccm-watch.h"

typedef struct CCMWatch CCMWatch;

struct CCMWatch {
    GSource source;
    GPollFD poll_fd;
    gboolean removed;
    
    CCMWatchCallback read_callback;
    CCMWatchCallback write_callback;
    CCMWatchCallback hangup_callback;
    CCMWatchCallback error_callback;
    CCMWatchCallback priority_callback;

    CCMWatchCallback poll_callback;
    
    gpointer data;
};

static GHashTable *watched_fds;

static void
init (void)
{
    if (!watched_fds)
        watched_fds = g_hash_table_new (g_int_hash, g_int_equal);
}

static CCMWatch *
lookup_watch (gint fd)
{
    init ();
    
    return g_hash_table_lookup (watched_fds, &fd);
}

static void
internal_add_watch (CCMWatch *watch)
{
    gpointer fd = &(watch->poll_fd.fd);
    
    init ();
    
    g_hash_table_insert (watched_fds, fd, watch);
    g_source_add_poll ((GSource *)watch, &(watch->poll_fd));
}

static void
internal_remove_watch (CCMWatch *watch)
{
    gpointer fd = &(watch->poll_fd.fd);
    
    init ();
    
    watch->removed = TRUE;
    g_source_remove_poll ((GSource *)watch, &(watch->poll_fd));
    g_hash_table_remove (watched_fds, fd);
}

static gboolean
watch_prepare (GSource *source,
               gint    *timeout)
{
    CCMWatch *watch = (CCMWatch *)source;

    if (watch->poll_callback)
        watch->poll_callback (watch->data);
    
    *timeout = -1;

    return FALSE;
}

static gboolean
watch_check (GSource *source)
{
    CCMWatch *watch = (CCMWatch *)source;
    gint revents = watch->poll_fd.revents;
    
    if (revents & (G_IO_NVAL))
    {
        /* This can happen if the user closes the file descriptor
         * without first removing the watch. We silently ignore it
         */
        internal_remove_watch (watch);
        g_source_unref (source);
        return FALSE;
    }
    
    if ((revents & G_IO_HUP) && watch->hangup_callback)
        return TRUE;
    
    if ((revents & G_IO_IN) && watch->read_callback)
        return TRUE;
    
    if ((revents & G_IO_PRI) && watch->priority_callback)
        return TRUE;
    
    if ((revents & G_IO_ERR) && watch->error_callback)
        return TRUE;
    
    if ((revents & G_IO_OUT) && watch->write_callback)
        return TRUE;
    
    return FALSE;
}

static gboolean
watch_dispatch (GSource     *source,
                GSourceFunc  callback,
                gpointer     user_data)
{
    CCMWatch *watch = (CCMWatch *)source;
    gint revents = watch->poll_fd.revents;
    gboolean removed;
    
    g_source_ref (source);
    
    if (!watch->removed && (revents & G_IO_IN)  && watch->read_callback)
        watch->read_callback (watch->data);
    
    if (!watch->removed && (revents & G_IO_PRI) && watch->priority_callback)
        watch->priority_callback (watch->data);
    
    if (!watch->removed && (revents & G_IO_OUT) && watch->write_callback)
        watch->write_callback (watch->data);
    
    if (!watch->removed && (revents & G_IO_ERR) && watch->error_callback)
        watch->error_callback (watch->data);
    
    if (!watch->removed && (revents & G_IO_HUP) && watch->hangup_callback)
        watch->hangup_callback (watch->data);
    
    removed = watch->removed;
    
    g_source_unref (source);
    
    if (removed)
        return FALSE;
    
    return TRUE;
}

static void
watch_finalize (GSource *source)
{
}

static void
update_poll_mask (CCMWatch *watch)
{
    gint events = 0;
    
    g_source_remove_poll ((GSource *)watch, &(watch->poll_fd));
    
    if (watch->read_callback)
        events |= G_IO_IN;
    
    if (watch->write_callback)
        events |= G_IO_OUT;
    
    if (watch->priority_callback)
        events |= G_IO_PRI;
    
    if (watch->hangup_callback)
        events |= G_IO_HUP;
    
    if (watch->error_callback)
        events |= G_IO_ERR;
    
    watch->poll_fd.events = events;
    
    g_source_add_poll ((GSource *)watch, &(watch->poll_fd));
}

void
fd_add_watch (gint     fd,
              gpointer data)
{
    static GSourceFuncs watch_funcs = {
        watch_prepare,
        watch_check,
        watch_dispatch,
        watch_finalize,
    };
    CCMWatch *watch = lookup_watch (fd);
    
    g_return_if_fail (fd > 0);
    g_return_if_fail (watch == NULL);
    
    watch = (CCMWatch *)g_source_new (&watch_funcs, sizeof (CCMWatch)); 
    g_source_set_can_recurse ((GSource *)watch, TRUE);
    g_source_attach ((GSource *)watch, NULL);
    			  
    watch->poll_fd.fd = fd;
    watch->poll_fd.events = 0;
    watch->removed = FALSE;
    
    watch->read_callback = NULL;
    watch->write_callback = NULL;
    watch->hangup_callback = NULL;
    watch->error_callback = NULL;
    watch->priority_callback = NULL;
    
    watch->data = data;
    
    internal_add_watch (watch);
}

void
fd_set_read_callback (gint             fd,
                      CCMWatchCallback read_cb)
{
    CCMWatch *watch = lookup_watch (fd);
    
    g_return_if_fail (fd > 0);
    g_return_if_fail (watch != NULL);
    
    if (watch->read_callback != read_cb)
    {
        watch->read_callback = read_cb;
        update_poll_mask (watch);
    }
}

void
fd_set_write_callback (gint             fd,
                       CCMWatchCallback write_cb)
{
    CCMWatch *watch = lookup_watch (fd);
    
    g_return_if_fail (fd > 0);
    g_return_if_fail (watch != NULL);
    
    if (watch->write_callback != write_cb)
    {
        watch->write_callback = write_cb;
        update_poll_mask (watch);
    }
}

void
fd_set_hangup_callback (gint             fd,
                        CCMWatchCallback hangup_cb)
{
    CCMWatch *watch = lookup_watch (fd);
    
    g_return_if_fail (fd > 0);
    g_return_if_fail (watch != NULL);
    
    if (watch->hangup_callback != hangup_cb)
    {
        watch->hangup_callback = hangup_cb;
        update_poll_mask (watch);
    }
}

void
fd_set_error_callback (gint             fd,
                       CCMWatchCallback error_cb)
{
    CCMWatch *watch = lookup_watch (fd);
    
    g_return_if_fail (fd > 0);
    g_return_if_fail (watch != NULL);
    
    if (watch->error_callback != error_cb)
    {
        watch->error_callback = error_cb;
        update_poll_mask (watch);
    }
}

void
fd_set_priority_callback (gint             fd,
                          CCMWatchCallback priority_cb)
{
    CCMWatch *watch = lookup_watch (fd);
    
    g_return_if_fail (fd > 0);
    g_return_if_fail (watch != NULL);
    
    if (watch->priority_callback != priority_cb)
    {
        watch->priority_callback = priority_cb;
        update_poll_mask (watch);
    }
}

void
fd_set_poll_callback (gint          fd,
                      CCMWatchCallback poll_cb)
{
    CCMWatch *watch = lookup_watch (fd);

    g_return_if_fail (fd > 0);
    g_return_if_fail (watch != NULL);

    if (watch->poll_callback != poll_cb)
        watch->poll_callback = poll_cb;
}

void
fd_remove_watch (gint fd)
{
    CCMWatch *watch = lookup_watch (fd);
    
    g_return_if_fail (fd > 0);
    
    if (!watch)
        return;
    
    internal_remove_watch (watch);
    g_source_unref ((GSource *)watch);
}

gboolean
fd_is_watched (gint fd)
{
    g_return_val_if_fail (fd > 0, FALSE);
    
    if (lookup_watch (fd))
        return TRUE;
    else
        return FALSE;
}
