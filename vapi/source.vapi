/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * source.vapi
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

namespace CCM
{
    [CCode (cheader_filename = "ccm-source.h", ref_function = "ccm_source_ref", unref_function = "ccm_source_unref")]
    public class Source : GLib.Source
    {
        public Source(SourceFuncs inFuncs, void* inData);
        public Source.from_pollfd(SourceFuncs inFuncs, GLib.PollFD? inFd, void* inData);
        public Source @ref();
        public void unref();
        public void destroy();
    }

    [CCode (cheader_filename = "ccm-source.h", has_target = false)]
    public delegate bool SourcePrepareFunc (void* inData, out int outTimeout);
    [CCode (cheader_filename = "ccm-source.h", has_target = false)]
    public delegate bool SourceCheckFunc (void* inData);
    [CCode (cheader_filename = "ccm-source.h", has_target = false)]
    public delegate bool SourceDispatchFunc (void* inData, GLib.SourceFunc inCallback);
    [CCode (cheader_filename = "ccm-source.h", has_target = false)]
    public delegate void SourceFinalizeFunc (void* inData);

    [SimpleType]
    [CCode (cheader_filename = "ccm-source.h")]
    public struct SourceFuncs 
    {
        public SourcePrepareFunc prepare;
        public SourceCheckFunc check;
        public SourceDispatchFunc dispatch;
        public SourceFinalizeFunc finalize;
    }
} 
