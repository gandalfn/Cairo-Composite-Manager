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
    public TimeVal m_StartTime;
    public uint    m_FrameCount;
    public uint    m_Fps;

    public TimeoutInterval(uint inFps)
    {
        m_StartTime.get_current_time();
        m_Fps = inFps;
        m_FrameCount = 0;
    }

    private inline ulong
    get_ticks (TimeVal inCurrentTime)
    {
        return (ulong)((inCurrentTime.tv_sec - m_StartTime.tv_sec) * 1000 +
                       (inCurrentTime.tv_usec - m_StartTime.tv_usec) / 1000);
    }

    public bool
    prepare (TimeVal inCurrentTime, out int outDelay)
    {
        bool ret = false;
        ulong elapsed_time = get_ticks (inCurrentTime);
        uint new_frame_num = (uint)(elapsed_time * m_Fps / 1000);

        if (new_frame_num < m_FrameCount || new_frame_num - m_FrameCount > 2)
        {
            long frame_time = (1000 + m_Fps - 1) / m_Fps;

            m_StartTime = inCurrentTime;
            m_StartTime.add(-frame_time * 1000);

            m_FrameCount = 0;
            outDelay = 0;
            ret = true;
        }
        else if (new_frame_num > m_FrameCount)
        {
            outDelay = 0;
            ret = true;
        }
        else
        {
            outDelay = (int)((m_FrameCount + 1) * 1000 / m_Fps - elapsed_time);
        }

        return ret;
    }

    public bool
    dispatch (TimeoutFunc inCallback, void* inData)
    {
        bool ret = false;

        if (inCallback(inData))
        {
            m_FrameCount++;
            ret = true;
        }

        return ret;
    }

    public int
    compare (TimeoutInterval inTimeoutInterval)
    {
        long a_delay = 1000 / m_Fps;
        long b_delay = 1000 / inTimeoutInterval.m_Fps;
        long b_difference;

        b_difference = ((m_StartTime.tv_sec - inTimeoutInterval.m_StartTime.tv_sec) * 1000
                        + (m_StartTime.tv_usec - inTimeoutInterval.m_StartTime.tv_usec) / 1000);

        return (int)(((m_FrameCount + 1) * a_delay) -
                           ((inTimeoutInterval.m_FrameCount + 1) * b_delay + b_difference));
    }
}
