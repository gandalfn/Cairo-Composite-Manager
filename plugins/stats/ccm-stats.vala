/* -*- Mode: Vala; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007-2012 <gandalfn@club-internet.fr>
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

using GLib;
using Cairo;
using CCM;
using X;
using Vala;

namespace CCM
{
    enum Options
    {
        SHORTCUT,
        N
    }

    class StatsOptions : PluginOptions
    {
        public string shortcut = "<Super>s";

        ////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////
        public override void
        changed(CCM.Config config)
        {
            if (config == get_config (Options.SHORTCUT))
            {
                shortcut = "<Super>s";

                try
                {
                    shortcut = config.get_string ();
                }
                catch (GLib.Error ex)
                {
                    CCM.log ("Error on get shortcut config get default");
                }
            }
        }
    }

    class Stats : CCM.Plugin, CCM.ScreenPlugin
    {
        // constants
        const string[] cOptionsKey = {
            "shortcut"
        };

        // properties
        unowned CCM.Screen m_Screen;
        CCM.Keybind        m_Keybind;
        bool               m_Enabled = false;
        CCM.Timeline       m_Animation;
        GraphicWatcher     m_GraphicWatcher;
        CPUWatcher         m_CPUWatcher;
        DisksWatcher       m_DisksWatcher;

        ////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////
        class construct
        {
            type_options = typeof (StatsOptions);
        }

        ////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////
        ~Stats ()
        {
            if (m_Screen != null)
                options_unload ();
        }

        ////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////
        void
        on_animation_new_frame (int inNumFrame)
        {
            m_GraphicWatcher.watch ();
            m_CPUWatcher.watch ();
            m_DisksWatcher.watch ();
        }

        ////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////
        void
        on_shortcut_pressed ()
        {
            m_Enabled = !m_Enabled;

            if (!m_Enabled && m_Animation.is_playing)
            {
                m_Animation.stop ();
                m_GraphicWatcher = null;
                m_CPUWatcher = null;
                m_DisksWatcher = null;
            }
            else if (m_Enabled && !m_Animation.is_playing)
            {
                m_Animation.start ();
                m_GraphicWatcher = new GraphicWatcher (m_Screen, 20, 20);
                m_CPUWatcher = new CPUWatcher (m_Screen, 20, 310);
                m_DisksWatcher = new DisksWatcher (m_Screen, 20, 520);
            }
        }

        ////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////
        void
        option_changed (int index)
        {
            switch (index)
            {
                case CCM.Options.SHORTCUT:
                    m_Keybind = new CCM.Keybind (m_Screen,
                                                 ((StatsOptions) get_option ()).shortcut,
                                                 true);
                    m_Keybind.key_press.connect (on_shortcut_pressed);
                    break;
                default:
                    break;
            }
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        void
        screen_load_options (CCM.Screen inScreen)
        {
            m_Screen = inScreen;

            // load options
            options_load ("stats", cOptionsKey, (PluginOptionsChangedFunc)option_changed);

            // create animation timeline
            m_Animation = new CCM.Timeline (m_Screen.refresh_rate, m_Screen.refresh_rate);
            m_Animation.loop = true;
            m_Animation.new_frame.connect (on_animation_new_frame);

            // chainup plugin calls
            ((CCM.ScreenPlugin) parent).screen_load_options (m_Screen);
        }

        ////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////
        bool
        screen_paint (CCM.Screen inScreen, Cairo.Context inCtx)
        {
            // chainup plugin calls
            bool ret = ((CCM.ScreenPlugin) parent).screen_paint (inScreen, inCtx);

            // stats is enabled
            if (ret && m_Enabled)
            {
                m_GraphicWatcher.paint (inCtx, m_Animation.current_frame);
                m_CPUWatcher.paint (inCtx, m_Animation.current_frame);
                m_DisksWatcher.paint (inCtx, m_Animation.current_frame);
            }

            return ret;
        }
    }
}

/**
 * Init plugin
 **/
[ModuleInit]
public Type
ccm_stats_get_plugin_type (TypeModule module)
{
    return typeof (Stats);
}
