/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-timeout.vala
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

enum CCM.TimeoutFlags
{
    NONE = 0,
    READY = 1 << 1,
    MASTER = 1 << 2
}

internal class CCM.Timeout
{
    private TimeoutFlags m_Flags;

    private TimeoutInterval m_Interval;
    internal TimeoutInterval? interval { 
        get {
            return m_Interval; 
        }
        set {
            m_Interval = value;
        }
    }

    private TimeoutFunc m_Callback;
    internal TimeoutFunc callback {
        get {
            return m_Callback;
        }
        set {
            m_Callback = value;
        }
    }

    private void* m_Data;
    internal void* data {
        get {
            return m_Data;
        }
        set {
            m_Data = value;
        }
    }

    private DestroyNotify m_Notify;
    internal new DestroyNotify notify {
        get {
            return m_Notify;
        }
        set {
            m_Notify = value;
        }
    }

    internal bool ready {
        get {
            return (m_Flags & TimeoutFlags.READY) == TimeoutFlags.READY;
        }
        set {
            if (value)
                m_Flags |= TimeoutFlags.READY;
            else
                m_Flags &= ~(int)TimeoutFlags.READY;
        }
    }

    internal bool master {
        get {
            return (m_Flags & TimeoutFlags.MASTER) == TimeoutFlags.MASTER;
        }
        set {
            if (value)
                m_Flags |= TimeoutFlags.MASTER;
            else
                m_Flags &= ~(int)TimeoutFlags.MASTER;
        }
    }

    internal Timeout (uint inFps)
    {
        m_Interval = TimeoutInterval (inFps);
        m_Flags = TimeoutFlags.NONE;
    }

    ~Timeout ()
    {
        if (m_Notify != null) m_Notify (m_Data);
    }

    internal static int
    compare (Timeout inA, Timeout inB)
    {
        /* Keep 'master' and 'ready' timeouts at the front */
        if (inA.ready && inA.master) return -1;

        if (inB.ready && inB.master) return 1;

        /* Keep 'ready' timeouts at the front */
        if (inA.ready) return -1;

        if (inB.ready) return 1;

        /* Keep 'master' timeouts at the front */
        if (inA.master) return -1;

        if (inB.master) return 1;

        return inA.m_Interval.compare(inB.m_Interval);
    }

    internal bool 
    prepare (Source inSource, out int outNextTimeout)
    {
        TimeVal now;

        inSource.get_current_time(out now);

        return m_Interval.prepare(now, out outNextTimeout);
    }
}
