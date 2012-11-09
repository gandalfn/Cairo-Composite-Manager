/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-timeout-interval.vala
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

[CCode (has_target = false)]
internal delegate bool CCM.TimeoutFunc(void* inData);

internal struct CCM.TimeoutInterval
{
    public uint64  m_StartTime;
    public long    m_Interval;
    public int     m_Delay;

    public TimeoutInterval(uint inFps)
    {
        m_StartTime = GLib.get_monotonic_time ();
        m_Interval = (long)(1000.0 / (double)inFps);
        m_Delay = (int)m_Interval;
    }

    private inline ulong
    get_ticks (uint64 inCurrentTime)
    {
        return (ulong)(inCurrentTime - m_StartTime) / 1000;
    }

    public bool
    prepare (uint64 inCurrentTime, out int outDelay)
    {
        bool ret = false;
        ulong elapsed_time = get_ticks (inCurrentTime);
        double diff = (double)elapsed_time / (double)m_Interval;

        if (diff >= 1.0)
        {
            double delta = (double)((int)diff) - diff;

            m_StartTime = inCurrentTime + (uint64)((delta * (double)m_Interval) * 1000.0);

            m_Delay = 0;
            ret = true;
        }
        else
        {
            m_Delay = int.max (((int)m_Interval - ((int)elapsed_time % (int)m_Interval)), 0);
        }

        outDelay = m_Delay;

        return ret;
    }

    public bool
    dispatch (TimeoutFunc inCallback, void* inData)
    {
        bool ret = false;

        if (inCallback(inData))
        {
            ret = true;
        }

        return ret;
    }

    public int
    compare (TimeoutInterval inTimeoutInterval)
    {
        return (int)(m_Delay - inTimeoutInterval.m_Delay);
    }
}
