/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-timeout.vala
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

enum CCM.TimeoutFlags
{
    NONE = 0,
    READY = 1 << 1,
    MASTER = 1 << 2
}

internal class CCM.Timeout
{
    private TimeoutFlags m_Flags;

    internal TimeoutInterval interval;
    internal TimeoutFunc callback;
    internal void* data;
    internal new DestroyNotify notify;

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
        interval = TimeoutInterval (inFps);
        m_Flags = TimeoutFlags.NONE;
    }

    ~Timeout ()
    {
        if (notify != null) notify (data);
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

        return inA.interval.compare(inB.interval);
    }

    internal bool
    prepare (Source inSource, out int outNextTimeout)
    {
        uint64 now = inSource.get_time();

        return interval.prepare(now, out outNextTimeout);
    }
}
