/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * maia-watch.vala
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

public abstract class CCM.Watch : GLib.Object
{
    // Source event watcher
    private CCM.Source m_Source = null;
    // Poll FD connection watcher
    private GLib.PollFD? m_Fd = null;

    private bool
    on_source_prepare (out int inTimeout)
    {
        inTimeout = -1;

        return check ();
    }

    private bool
    on_source_check ()
    {
        bool ret = false;

        if  ((m_Fd.revents & IOCondition.IN) == IOCondition.IN ||
             (m_Fd.revents & IOCondition.PRI) == IOCondition.PRI)
        {
            ret = check ();
        }

        return ret;
    }

    private bool
    on_source_dispatch(SourceFunc inCallback)
    {
        process_watch ();

        return true;
    }

    public abstract bool
    check ();

    public abstract void
    process_watch ();

    protected void
    watch (int inFd, GLib.MainContext? inContext = null)
    {
        SourceFuncs funcs = { on_source_prepare,
                              on_source_check,
                              on_source_dispatch,
                              null };

        m_Fd = PollFD();
        m_Fd.fd = inFd;
        m_Fd.events = IOCondition.IN | IOCondition.PRI;
        m_Source = new Source.from_pollfd (funcs, m_Fd, this);
        m_Source.attach (inContext);
    }
}
