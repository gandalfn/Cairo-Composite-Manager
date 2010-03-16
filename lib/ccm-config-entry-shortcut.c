/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007-2010 <gandalfn@club-internet.fr>
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

#include <config.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "ccm-config.h"
#include "ccm-config-entry-shortcut.h"
#include "eggaccelerators.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

enum
{
    PROP_0,
    PROP_MOUSE,
    PROP_KEY,
    PROP_PLUGIN,
    PROP_SCREEN
};

G_DEFINE_TYPE (CCMConfigEntryShortcut, ccm_config_entry_shortcut, GTK_TYPE_ENTRY);

struct _CCMConfigEntryShortcutPrivate
{
    gboolean edited;
    gchar *old_value;
    guint button;
    gboolean mouse;

    CCMConfig *config;
    gchar *key;
    gchar *plugin;
    gint screen;
};

#define CCM_CONFIG_ENTRY_SHORTCUT_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_CONFIG_ENTRY_SHORTCUT, CCMConfigEntryShortcutPrivate))

static void ccm_config_entry_shortcut_on_changed (CCMConfigEntryShortcut * self,
                                                  CCMConfig * config);
static gboolean ccm_config_entry_shortcut_focus_out_event (GtkWidget * widget,
                                                           GdkEventFocus *
                                                           event);
static gboolean ccm_config_entry_shortcut_button_press_event (GtkWidget *
                                                              widget,
                                                              GdkEventButton *
                                                              event);
static gboolean ccm_config_entry_shortcut_scroll_event (GtkWidget * widget,
                                                        GdkEventScroll * event);
static gboolean ccm_config_entry_shortcut_key_release_event (GtkWidget * widget,
                                                             GdkEventKey *
                                                             event);

static void
ccm_config_entry_shortcut_init (CCMConfigEntryShortcut * self)
{
    self->priv = CCM_CONFIG_ENTRY_SHORTCUT_GET_PRIVATE (self);
    self->priv->edited = FALSE;
    self->priv->old_value = NULL;
    self->priv->button = 0;
    self->priv->mouse = FALSE;
    self->priv->config = NULL;
    self->priv->key = NULL;
    self->priv->plugin = NULL;
    self->priv->screen = -1;
}

static void
ccm_config_entry_shortcut_finalize (GObject * object)
{
    CCMConfigEntryShortcut *self = CCM_CONFIG_ENTRY_SHORTCUT (object);

    if (self->priv->old_value)
        g_free (self->priv->old_value);
    if (self->priv->config)
        g_object_unref (self->priv->config);
    if (self->priv->key)
        g_free (self->priv->key);
    if (self->priv->plugin)
        g_free (self->priv->plugin);

    G_OBJECT_CLASS (ccm_config_entry_shortcut_parent_class)->finalize (object);
}

static void
ccm_config_entry_shortcut_set_property (GObject * object, guint prop_id,
                                        const GValue * value,
                                        GParamSpec * pspec)
{
    g_return_if_fail (CCM_IS_CONFIG_ENTRY_SHORTCUT (object));

    CCMConfigEntryShortcut *self = CCM_CONFIG_ENTRY_SHORTCUT (object);

    switch (prop_id)
    {
        case PROP_MOUSE:
            self->priv->mouse = g_value_get_boolean (value);
            break;
        case PROP_KEY:
            if (self->priv->key) g_free (self->priv->key);
            self->priv->key = g_value_dup_string (value);
            break;
        case PROP_PLUGIN:
            if (self->priv->plugin) g_free (self->priv->plugin);
            self->priv->plugin = g_value_dup_string (value);
            break;
        case PROP_SCREEN:
            self->priv->screen = g_value_get_int (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }

    if (self->priv->key)
    {
        if (self->priv->config)
            g_object_unref (self->priv->config);
        self->priv->config = ccm_config_new (self->priv->screen, self->priv->plugin,
                                             self->priv->key);
        if (self->priv->config)
        {
            g_signal_connect_swapped (self->priv->config, "changed",
                                      G_CALLBACK(ccm_config_entry_shortcut_on_changed), 
                                      self);
            ccm_config_entry_shortcut_on_changed (self, self->priv->config);
        }
    }
}

static void
ccm_config_entry_shortcut_get_property (GObject * object, guint prop_id,
                                        GValue * value, GParamSpec * pspec)
{
    g_return_if_fail (CCM_IS_CONFIG_ENTRY_SHORTCUT (object));

    CCMConfigEntryShortcut *self = CCM_CONFIG_ENTRY_SHORTCUT (object);

    switch (prop_id)
    {
        case PROP_MOUSE:
            g_value_set_boolean (value, self->priv->mouse);
            break;
        case PROP_KEY:
            g_value_set_string (value, self->priv->key);
            break;
        case PROP_PLUGIN:
            g_value_set_string (value, self->priv->plugin);
            break;
        case PROP_SCREEN:
            g_value_set_int (value, self->priv->screen);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
ccm_config_entry_shortcut_class_init (CCMConfigEntryShortcutClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMConfigEntryShortcutPrivate));

    object_class->finalize = ccm_config_entry_shortcut_finalize;
    object_class->set_property = ccm_config_entry_shortcut_set_property;
    object_class->get_property = ccm_config_entry_shortcut_get_property;

    GTK_WIDGET_CLASS (klass)->focus_out_event = ccm_config_entry_shortcut_focus_out_event;
    GTK_WIDGET_CLASS (klass)->button_press_event = ccm_config_entry_shortcut_button_press_event;
    GTK_WIDGET_CLASS (klass)->scroll_event = ccm_config_entry_shortcut_scroll_event;
    GTK_WIDGET_CLASS (klass)->key_release_event = ccm_config_entry_shortcut_key_release_event;

    g_object_class_install_property (object_class, PROP_MOUSE,
                                     g_param_spec_boolean ("mouse", "Use mouse",
                                                           "Shortcut with mouse button",
                                                           FALSE,
                                                           G_PARAM_READWRITE));

    g_object_class_install_property (object_class, PROP_KEY,
                                     g_param_spec_string ("key", "Config key",
                                                          "Config key name", "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (object_class, PROP_PLUGIN,
                                     g_param_spec_string ("plugin",
                                                          "Plugin name ",
                                                          "Plugin name",
                                                          NULL,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (object_class, PROP_SCREEN,
                                     g_param_spec_int ("screen",
                                                       "Screen number",
                                                       "Screen number", -1, 10,
                                                       -1, G_PARAM_READWRITE));
}

static void
ccm_config_entry_shortcut_on_changed (CCMConfigEntryShortcut * self,
                                      CCMConfig * config)
{
    g_return_if_fail (CCM_IS_CONFIG_ENTRY_SHORTCUT (self));
    g_return_if_fail (CCM_IS_CONFIG (config));

    if (self->priv->old_value)
        g_free (self->priv->old_value);
    self->priv->old_value = ccm_config_get_string (config, NULL);

    if (self->priv->old_value)
        gtk_entry_set_text (GTK_ENTRY (self), self->priv->old_value);
    else
        gtk_entry_set_text (GTK_ENTRY (self), _("Disabled"));
}

static gboolean
ccm_config_entry_shortcut_focus_out_event (GtkWidget * widget,
                                           GdkEventFocus * event)
{
    CCMConfigEntryShortcut *self = CCM_CONFIG_ENTRY_SHORTCUT (widget);
    gboolean ret = FALSE;

    if (self->priv->edited)
    {
        gtk_entry_set_text (GTK_ENTRY (self), self->priv->old_value);
        gdk_keyboard_ungrab (GDK_CURRENT_TIME);
        gdk_pointer_ungrab (GDK_CURRENT_TIME);
        self->priv->edited = FALSE;
        self->priv->button = 0;
    }

    if (GTK_WIDGET_CLASS (ccm_config_entry_shortcut_parent_class)->focus_out_event)
        ret = GTK_WIDGET_CLASS (ccm_config_entry_shortcut_parent_class)->focus_out_event (widget, event);

    return ret;
}

static gboolean
ccm_config_entry_shortcut_button_press_event (GtkWidget * widget,
                                              GdkEventButton * event)
{
    CCMConfigEntryShortcut *self = CCM_CONFIG_ENTRY_SHORTCUT (widget);

    if (event->type == GDK_BUTTON_PRESS)
    {
        if (!self->priv->edited)
        {
            if (self->priv->old_value)
                g_free (self->priv->old_value);
            self->priv->old_value = g_strdup (gtk_entry_get_text (GTK_ENTRY (self)));
            gtk_entry_set_text (GTK_ENTRY (self),
                                self->priv->mouse ? _("<Press keys and mouse shortcut...>") : 
                                                    _("<Press keys shortcut...>"));
            gdk_keyboard_grab (GTK_WIDGET (self)->window, TRUE,
                               GDK_CURRENT_TIME);
            gdk_pointer_grab (GTK_WIDGET (self)->window, TRUE,
                              GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK,
                              GTK_WIDGET (self)->window, NULL,
                              GDK_CURRENT_TIME);
            gtk_widget_grab_focus (GTK_WIDGET (self));
            self->priv->edited = TRUE;
        }
    }

    return FALSE;
}

static gboolean
ccm_config_entry_shortcut_scroll_event (GtkWidget * widget,
                                        GdkEventScroll * event)
{
    CCMConfigEntryShortcut *self = CCM_CONFIG_ENTRY_SHORTCUT (widget);
    gboolean ret = FALSE;

    if (self->priv->edited)
    {
        if (event->direction == GDK_SCROLL_UP)
            self->priv->button = GDK_BUTTON4_MASK;
        else if (event->direction == GDK_SCROLL_DOWN)
            self->priv->button = GDK_BUTTON5_MASK;
    }


    if (GTK_WIDGET_CLASS (ccm_config_entry_shortcut_parent_class)->scroll_event)
        ret =
        GTK_WIDGET_CLASS (ccm_config_entry_shortcut_parent_class)->
        scroll_event (widget, event);


    return ret;
}

static gboolean
ccm_config_entry_shortcut_key_release_event (GtkWidget * widget,
                                             GdkEventKey * event)
{
    CCMConfigEntryShortcut *self = CCM_CONFIG_ENTRY_SHORTCUT (widget);
    gboolean ret = FALSE;

    if (self->priv->edited && event->type == GDK_KEY_RELEASE)
    {
        gchar *value = g_strdup ("");
        gchar *tmp;

        if (event->keyval == GDK_BackSpace)
            value = g_strdup (_("Disabled"));
        else if (event->keyval == GDK_Escape)
            value = g_strdup (self->priv->old_value);
        else
        {
            guint keyval = 0;
            GdkModifierType mods = 0;
            GdkModifierType consumed_modifiers;
            EggVirtualModifierType modifier;

            consumed_modifiers = 0;
            keyval = gdk_keyval_to_lower (event->keyval);
            if (keyval == GDK_ISO_Left_Tab)
                keyval = GDK_Tab;
            gdk_keymap_translate_keyboard_state (gdk_keymap_get_default (),
                                                 event->hardware_keycode,
                                                 event->state, event->group,
                                                 &keyval, NULL, NULL,
                                                 &consumed_modifiers);
            if (event->keyval != keyval && (consumed_modifiers & GDK_SHIFT_MASK))
                consumed_modifiers &= ~(GDK_SHIFT_MASK);

            mods = event->state & GDK_MODIFIER_MASK & ~(consumed_modifiers);
            egg_keymap_virtualize_modifiers (gdk_keymap_get_default (), mods,
                                             &modifier);

            switch (keyval)
            {
                case GDK_Shift_L:
                case GDK_Shift_R:
                    modifier |= EGG_VIRTUAL_SHIFT_MASK;
                    keyval = 0;
                    break;
                case GDK_Control_L:
                case GDK_Control_R:
                    modifier |= EGG_VIRTUAL_CONTROL_MASK;
                    keyval = 0;
                    break;
                case GDK_Alt_L:
                case GDK_Alt_R:
                    modifier |= EGG_VIRTUAL_ALT_MASK;
                    keyval = 0;
                    break;
                case GDK_Super_L:
                case GDK_Super_R:
                    modifier |= EGG_VIRTUAL_SUPER_MASK;
                    keyval = 0;
                    break;
                default:
                    break;
            }

            if (event->is_modifier)
                keyval = 0;

            modifier &= ~(EGG_VIRTUAL_META_MASK);
            modifier &= ~(EGG_VIRTUAL_HYPER_MASK);

            value = egg_virtual_accelerator_name (keyval, 0, modifier);

            if (self->priv->mouse)
            {
                if (event->state & GDK_BUTTON1_MASK)
                {
                    tmp = g_strconcat (value, "Button1", NULL);
                    g_free (value);
                    value = tmp;
                }
                if (event->state & GDK_BUTTON2_MASK)
                {
                    tmp = g_strconcat (value, "Button2", NULL);
                    g_free (value);
                    value = tmp;
                }
                if (event->state & GDK_BUTTON3_MASK)
                {
                    tmp = g_strconcat (value, "Button3", NULL);
                    g_free (value);
                    value = tmp;
                }
                if (self->priv->button & GDK_BUTTON4_MASK)
                {
                    tmp = g_strconcat (value, "Button4", NULL);
                    g_free (value);
                    value = tmp;
                }
                if (self->priv->button & GDK_BUTTON5_MASK)
                {
                    tmp = g_strconcat (value, "Button5", NULL);
                    g_free (value);
                    value = tmp;
                }
            }
        }

        self->priv->edited = FALSE;
        gdk_keyboard_ungrab (GDK_CURRENT_TIME);
        gdk_pointer_ungrab (GDK_CURRENT_TIME);
        gtk_entry_set_text (GTK_ENTRY (self), value);
        if (self->priv->config)
            ccm_config_set_string (self->priv->config, value, NULL);
        g_free (value);
    }


    if (GTK_WIDGET_CLASS (ccm_config_entry_shortcut_parent_class)->key_release_event)
        ret = GTK_WIDGET_CLASS (ccm_config_entry_shortcut_parent_class)->key_release_event (widget, event);

    return ret;
}

GtkWidget *
ccm_config_entry_shortcut_new (gboolean mouse, gint screen, gchar * plugin,
                               gchar * key)
{
    g_return_val_if_fail (key != NULL, NULL);

    CCMConfigEntryShortcut *self = g_object_new (CCM_TYPE_CONFIG_ENTRY_SHORTCUT,
                                                 "mouse", mouse,
                                                 "screen", screen,
                                                 "plugin", plugin,
                                                 "key", key, NULL);

    gtk_entry_set_editable (GTK_ENTRY (self), FALSE);
    gtk_entry_set_text (GTK_ENTRY (self), "");
    gtk_widget_add_events (GTK_WIDGET (self), GDK_BUTTON_PRESS_MASK);

    return GTK_WIDGET (self);
}
