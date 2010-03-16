/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2009 <nicolas.bruguier@supersonicimagine.fr>
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

/*
 * Standard gettext macros.
 */
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

#include "ccm-timed-dialog.h"

G_DEFINE_TYPE (CCMTimedDialog, ccm_timed_dialog, GTK_TYPE_DIALOG);

#define CCM_TIMED_DIALOG_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_TIMED_DIALOG, CCMTimedDialogPrivate))

struct _CCMTimedDialogPrivate
{
    guint time;
    guint timeout;
    gboolean timed_out;
    GtkWidget *label_sec;
};

static void
ccm_timed_dialog_init (CCMTimedDialog * self)
{
    self->priv = CCM_TIMED_DIALOG_GET_PRIVATE (self);
    self->priv->time = 0;
    self->priv->timeout = 0;
    self->priv->timed_out = FALSE;
    self->priv->label_sec = NULL;
}

static void
ccm_timed_dialog_finalize (GObject * object)
{
    CCMTimedDialog *self = CCM_TIMED_DIALOG (object);

    self->priv->time = 0;
    if (self->priv->timeout)
        g_source_remove (self->priv->timeout);
    self->priv->timeout = 0;
    self->priv->timed_out = TRUE;
    self->priv->label_sec = NULL;

    G_OBJECT_CLASS (ccm_timed_dialog_parent_class)->finalize (object);
}

static void
ccm_timed_dialog_class_init (CCMTimedDialogClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMTimedDialogPrivate));

    object_class->finalize = ccm_timed_dialog_finalize;
}

static void
ccm_timed_dialog_set_timeout_string (CCMTimedDialog * self)
{
    gchar *string =
        g_strdup_printf (_("Testing the new settings. If you don't respond in %d second the previous settings will be restored."),
                         self->priv->time);

    g_object_set_data_full (G_OBJECT (self), "TimeoutString", string, g_free);
    gtk_label_set_text (GTK_LABEL (self->priv->label_sec), string);
}

static gboolean
ccm_timed_dialog_timeout_callback (CCMTimedDialog * self)
{
    if (self->priv->timeout == 0) return FALSE;

    if (!self->priv->timed_out)
    {
        self->priv->time--;
        ccm_timed_dialog_set_timeout_string (self);
        if (self->priv->time == 0)
        {
            self->priv->timed_out = TRUE;
            gtk_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_NO);
            self->priv->timeout = 0;
        }
    }

    return !self->priv->timed_out;
}

GtkWidget *
ccm_timed_dialog_new (GtkWidget * parent, guint seconds)
{
    g_return_val_if_fail (parent != NULL, NULL);
    g_return_val_if_fail (seconds > 0, NULL);

    CCMTimedDialog *self = g_object_new (CCM_TYPE_TIMED_DIALOG, NULL);

    GtkWidget *hbox;
    GtkWidget *vbox;
    GtkWidget *label;
    GtkWidget *image;

    self->priv->time = seconds;

    gtk_window_set_transient_for (GTK_WINDOW (self), GTK_WINDOW (parent));
    gtk_window_set_destroy_with_parent (GTK_WINDOW (self), TRUE);
    gtk_window_set_modal (GTK_WINDOW (self), TRUE);
    gtk_container_set_border_width (GTK_CONTAINER (self), 12);
    gtk_dialog_set_has_separator (GTK_DIALOG (self), FALSE);
    gtk_window_set_title (GTK_WINDOW (self), _("Keep Settings"));
    gtk_window_set_position (GTK_WINDOW (self), GTK_WIN_POS_CENTER_ALWAYS);

    label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label),
                          _("<b>Do you want to keep these settings?</b>"));
    image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_QUESTION,
                                      GTK_ICON_SIZE_DIALOG);
    gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);

    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    gtk_label_set_selectable (GTK_LABEL (label), TRUE);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

    self->priv->label_sec = gtk_label_new ("");
    ccm_timed_dialog_set_timeout_string (self);
    gtk_label_set_line_wrap (GTK_LABEL (self->priv->label_sec), TRUE);
    gtk_label_set_selectable (GTK_LABEL (self->priv->label_sec), TRUE);
    gtk_misc_set_alignment (GTK_MISC (self->priv->label_sec), 0.0, 0.5);

    hbox = gtk_hbox_new (FALSE, 6);
    vbox = gtk_vbox_new (FALSE, 6);

    gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), self->priv->label_sec, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
    vbox = gtk_dialog_get_content_area (GTK_DIALOG (self));
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    gtk_dialog_add_buttons (GTK_DIALOG (self), _("Use _previous settings"),
                            GTK_RESPONSE_NO, _("_Keep settings"),
                            GTK_RESPONSE_YES, NULL);

    gtk_widget_show_all (hbox);

    self->priv->timeout = g_timeout_add_seconds (1,
                                                 (GSourceFunc) ccm_timed_dialog_timeout_callback,
                                                 self);

    return (GtkWidget *) self;
}
