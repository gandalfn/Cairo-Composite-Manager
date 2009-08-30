/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007 <gandalfn@club-internet.fr>
 * 
 * cairo-compmgr is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * cairo-compmgr is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with cairo-compmgr.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
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

    const string[] options_key = {
        "show"
    };

    class AutomateOptions : PluginOptions
    {
        public string show_shortcut;

		protected override void changed(CCM.Config config)
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
        private weak CCM.Screen screen;

        private bool enable = false;

        private CCM.Keybind show_keybind;

		private CCM.AutomateDialog dialog;

        class construct
		{
			type_options = typeof (AutomateOptions);
		}

		~Automate ()
        {
            options_unload ();
        }

        private void option_changed (int index)
        {
            // Reload show shortcut
            get_show_shortcut ();
        }

        private void on_show_shortcut_pressed ()
        {
            enable = !enable;

            if (enable)
                dialog.show ();
            else
                dialog.hide ();
        }

        private void get_show_shortcut ()
        {
            show_keybind = new CCM.Keybind (screen, 
                                            ((AutomateOptions) get_option ()).show_shortcut, 
                                            true);
            show_keybind.key_press += on_show_shortcut_pressed;
        }

        protected void screen_load_options (CCM.Screen screen)
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
