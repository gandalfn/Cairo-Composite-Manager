/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-automate.vala
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

using GLib;
using Cairo;
using CCM;

namespace CCM
{
    enum Options
    {
        SHOW_SHORTCUT,
        N
    }

    class AutomateOptions : PluginOptions
    {
        public string show_shortcut;

        public override void
        changed(CCM.Config config)
        {
            show_shortcut = "<Super>a";

            try
            {
                show_shortcut = config.get_string ();
            }
            catch (GLib.Error ex)
            {
                CCM.log ("Error on get show shortcut config get default");
            }
        }
    }

    class Automate : CCM.Plugin, CCM.ScreenPlugin
    {
        const string[] options_key = {
            "show"
        };

        weak CCM.Screen screen;

        bool enable = false;

        CCM.Keybind show_keybind;

        CCM.AutomateDialog dialog;

        class construct
        {
            type_options = typeof (AutomateOptions);
        }

        ~Automate ()
        {
            options_unload ();
        }

        void
        option_changed (int index)
        {
            // Reload show shortcut
            get_show_shortcut ();
        }

        void
        on_show_shortcut_pressed ()
        {
            enable = !enable;

            if (enable)
                dialog.show ();
            else
                dialog.hide ();
        }

        void
        get_show_shortcut ()
        {
            show_keybind = new CCM.Keybind (screen, 
                                            ((AutomateOptions) get_option ()).show_shortcut, 
                                            true);
            show_keybind.key_press.connect (on_show_shortcut_pressed);
        }

        void
        screen_load_options (CCM.Screen screen)
        {
            this.screen = screen;

            this.dialog = new CCM.AutomateDialog (screen);

            // load options
            options_load ("automate", options_key,
                          (PluginOptionsChangedFunc)option_changed);

            ((CCM.ScreenPlugin) parent).screen_load_options (screen);
        }
    }
}

[ModuleInit]
public Type
ccm_automate_get_plugin_type (TypeModule module)
{
    return typeof (Automate);
}
