/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-timeout-pool.vala
 * Copyright (C) Nicolas Bruguier 2010 <gandalfn@club-internet.fr>
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

internal class CCM.TimeoutPool
{
    private CCM.Source            m_Source;
    private CCM.SourceFuncs       m_Funcs;
    private TimeVal                m_StartTime;
    private List<Timeout>          m_Timeouts;
    private int                    m_Ready;

    public TimeoutPool (int inPriority = GLib.Priority.DEFAULT,
                        GLib.MainContext? inContext = null)
    {
        m_Funcs.prepare = prepare;
        m_Funcs.check = check;
        m_Funcs.dispatch = dispatch;
        m_Funcs.finalize = finalize_;

        m_Source = new CCM.Source (m_Funcs, this);
        m_Source.attach (inContext);
        m_Source.get_current_time(out m_StartTime);
        m_Source.set_priority (inPriority);
        m_Source.unref ();
        m_Timeouts = new List<Timeout>();
        m_Ready = 0;
    }

    private bool 
    prepare (out int outTimeOut)
    {
        bool ret = false;

        /* the pool is ready if the first timeout is ready */
        if (m_Timeouts != null && m_Timeouts.data != null)
        {
            ret = m_Timeouts.data.prepare (m_Source, out outTimeOut);
        }
        else
        {
            outTimeOut = -1;
        }

        return ret;
    }

    private bool 
    check ()
    {
        foreach (Timeout timeout in m_Timeouts)
        {
            int val;

            /* since the timeouts are sorted by expiration, as soon
             * as we get a check returning FALSE we know that the
             * following timeouts are not expiring, so we break as
             * soon as possible
             */
            if (timeout.prepare (m_Source, out val))
            {
                timeout.ready = true;
                m_Ready++;
            }
            else
                break;
        }

        return m_Ready > 0;
    }

    private bool 
    dispatch (SourceFunc inCallback)
    {
        List<unowned Timeout?> dispatched = new List<unowned Timeout?> ();

        /* the main loop might have predicted this, so we repeat the
         * check for ready timeouts.
         */
        if (m_Ready <= 0) check ();

        /* Iterate by moving the actual start of the list along so that it
         * can cope with adds and removes while a timeout is being dispatched
         */
        while (m_Timeouts != null && m_Timeouts.data != null && m_Ready-- > 0)
        {
            Timeout timeout = m_Timeouts.data;

            /* One of the ready timeouts may have been removed during dispatch,
             * in which case pool->ready will be wrong, but the ready timeouts
             * are always kept at the start of the list so we can stop once
             * we've reached the first non-ready timeout
             */
            if (!timeout.ready) break;

            timeout.ready = false;

            /* Move the list node to a list of dispatched timeouts */
            dispatched.prepend(m_Timeouts.data);
            m_Timeouts.delete_link(m_Timeouts);

            if (!timeout.interval.dispatch(timeout.callback, timeout.data))
            {
                /* The timeout may have already been removed, but nothing
                 * can be added to the dispatched_timeout list except in this
                 * function so it will always either be at the head of the
                 * dispatched list or have been removed
                 */
                if (dispatched != null && dispatched.data == timeout)
                {
                    dispatched.delete_link (dispatched);
                }
            }
        }

        /* Re-insert the dispatched timeouts in sorted order */
        foreach (Timeout timeout in dispatched)
        {
            if (timeout != null)
            {
                m_Timeouts.insert_sorted (timeout, 
                                          (CompareFunc)Timeout.compare);
            }
        }

        dispatched = null;

        m_Ready = 0;

        return true;
    }

    private void 
    finalize_ ()
    {
        m_Timeouts = null;
    }

    public void
    set_priority (int inPriority)
    {
        m_Source.set_priority (inPriority);
    }

    public Timeout
    add (uint inFps, TimeoutFunc inFunc, void* inData, DestroyNotify? inNotify)
    {
        Timeout timeout = new Timeout (inFps);

        timeout.callback = inFunc;
        timeout.data = inData;
        timeout.notify = inNotify;

        m_Timeouts.insert_sorted(timeout, (CompareFunc)Timeout.compare);

        return timeout;
    }

    public Timeout
    add_master (uint inFps, TimeoutFunc inFunc, void* inData, DestroyNotify? inNotify)
    {
        Timeout timeout = new Timeout (inFps);

        timeout.callback = inFunc;
        timeout.data = inData;
        timeout.notify = inNotify;
        timeout.master = true;

        m_Timeouts.insert_sorted(timeout, (CompareFunc)Timeout.compare);

        return timeout;
    }

    public void
    remove (Timeout inTimeout)
    {
        weak GLib.List<Timeout>? l;

        if ((l = m_Timeouts.find(inTimeout)) != null)
        {
            m_Timeouts.remove (inTimeout);
        }
    }
}
