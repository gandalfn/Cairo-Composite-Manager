/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-preferences.c
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

#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "ccm-marshallers.h"
#include "ccm-cairo-utils.h"
#include "ccm-timed-dialog.h"
#include "ccm-preferences.h"
#include "ccm-preferences-page.h"

enum
{
    RELOAD,
    CLOSED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (CCMPreferences, ccm_preferences, G_TYPE_OBJECT);

#define CCM_PREFERENCES_GET_PRIVATE(o)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_PREFERENCES, CCMPreferencesPrivate))

typedef struct
{
    CCMPreferences *self;
    CCMPreferencesPage *page;
    CCMNeedRestartFunc func;
    gpointer data;
} CCMPreferencesNeedRestartData;

struct _CCMPreferencesPrivate
{
    int nb_screens;

    int width;
    int height;

    GtkBuilder *builder;
    CCMPreferencesPage **screen_pages;
    GtkWidget **screen_titles;
};

static void
ccm_preferences_init (CCMPreferences * self)
{
    self->priv = CCM_PREFERENCES_GET_PRIVATE (self);

    self->priv->nb_screens = 0;
    self->priv->width = 0;
    self->priv->height = 0;
    self->priv->builder = NULL;
    self->priv->screen_titles = NULL;
    self->priv->screen_pages = NULL;
}

static void
ccm_preferences_finalize (GObject * object)
{
    CCMPreferences *self = CCM_PREFERENCES (object);
    gint cpt;

    for (cpt = 0; cpt < self->priv->nb_screens; ++cpt)
    {
        g_object_unref (self->priv->screen_titles[cpt]);
        g_object_unref (self->priv->screen_pages[cpt]);
    }
    g_free (self->priv->screen_titles);
    g_free (self->priv->screen_pages);

    if (self->priv->builder)
        g_object_unref (self->priv->builder);

    G_OBJECT_CLASS (ccm_preferences_parent_class)->finalize (object);
}

static void
ccm_preferences_class_init (CCMPreferencesClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMPreferencesPrivate));

    signals[RELOAD] = g_signal_new ("reload", G_OBJECT_CLASS_TYPE (object_class),
                                    G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                    ccm_cclosure_marshal_BOOLEAN__VOID, G_TYPE_BOOLEAN, 0);

    signals[CLOSED] = g_signal_new ("closed", G_OBJECT_CLASS_TYPE (object_class),
                                    G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                    g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    object_class->finalize = ccm_preferences_finalize;
}

static void
on_need_restart_dialog_response (GtkDialog * dialog, int response,
                                 CCMPreferencesNeedRestartData * data)
{
    gboolean ret = FALSE;

    data->func (data->page, response != GTK_RESPONSE_YES, data->data);
    if (response != GTK_RESPONSE_YES)
        g_signal_emit (data->self, signals[RELOAD], 0, &ret);
    g_free (data);

    gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
on_need_restart_dialog_close (GtkDialog * dialog,
                              CCMPreferencesNeedRestartData * data)
{
    data->func (data->page, TRUE, data->data);
    g_signal_emit (data->self, signals[RELOAD], 0);
    g_free (data);

    gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
ccm_preferences_on_need_restart (CCMPreferences * self, CCMNeedRestartFunc func,
                                 gpointer data, CCMPreferencesPage * page)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (page != NULL);

    gboolean ret = FALSE;

    g_signal_emit (self, signals[RELOAD], 0, &ret);
    if (func)
    {
        if (ret)
        {
            GtkWidget *widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                                                    "shell"));
            GtkWidget *dialog = ccm_timed_dialog_new (widget, 20);
            CCMPreferencesNeedRestartData *nr_data =
                g_new (CCMPreferencesNeedRestartData, 1);
            nr_data->self = self;
            nr_data->page = page;
            nr_data->func = func;
            nr_data->data = data;
            g_signal_connect (G_OBJECT (dialog), "response",
                              G_CALLBACK (on_need_restart_dialog_response),
                              nr_data);
            g_signal_connect (G_OBJECT (dialog), "close",
                              G_CALLBACK (on_need_restart_dialog_close),
                              nr_data);
            gtk_widget_show (dialog);
        }
        else
        {
            func (page, FALSE, data);
        }
    }
}

static void
ccm_preferences_create_page (CCMPreferences * self, int screen_num)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (screen_num >= 0);

    GtkBuilder *builder;
    GtkBox *screen_button_box;
    GtkLabel *screen_name;
    GtkNotebook *notebook;
    GtkWidget *page, *screen_title_event;
    gchar *str;

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, UI_DIR "/ccm-preferences.ui", NULL);

    notebook = GTK_NOTEBOOK (gtk_builder_get_object (self->priv->builder,
                                                     "notebook"));
    if (!notebook)
        return;


    self->priv->screen_titles[screen_num] = GTK_WIDGET (gtk_builder_get_object (builder, "screen_title_frame"));
    if (!self->priv->screen_titles[screen_num])
        return;
    gtk_widget_show (self->priv->screen_titles[screen_num]);

    screen_name = GTK_LABEL (gtk_builder_get_object (builder, "screen_name"));
    if (!screen_name)
        return;
    str = g_strdup_printf ("<span size='large'><b>Screen %i</b></span>",
                           screen_num);
    gtk_label_set_markup (screen_name, str);
    g_free (str);

    self->priv->screen_pages[screen_num] = ccm_preferences_page_new (screen_num);
    g_signal_connect_swapped (G_OBJECT (self->priv->screen_pages[screen_num]),
                              "need-restart",
                              G_CALLBACK (ccm_preferences_on_need_restart),
                              self);
    page = ccm_preferences_page_get_widget (self->priv->screen_pages[screen_num]);
    gtk_widget_show (page);
    gtk_notebook_append_page (notebook, page, self->priv->screen_titles[screen_num]);

    g_object_unref (builder);
}

static gboolean
ccm_preferences_on_delete_event (CCMPreferences * self, GdkEvent* event,
                                 GtkWidget * widget)
{
    ccm_preferences_hide (self);
    g_signal_emit (self, signals[CLOSED], 0);
    return TRUE;
}

static void
ccm_preferences_on_response (CCMPreferences * self, gint response,
                             GtkWidget * widget)
{
    if (response != GTK_RESPONSE_DELETE_EVENT)
        ccm_preferences_hide (self);
    g_signal_emit (self, signals[CLOSED], 0);
}

CCMPreferences *
ccm_preferences_new (void)
{
    CCMPreferences *self = g_object_new (CCM_TYPE_PREFERENCES, NULL);
    GdkDisplay *display = gdk_display_get_default ();
    GdkColormap *colormap;
    GdkScreen *screen = gdk_screen_get_default ();
    GtkWidget *shell;
    gint cpt;

    self->priv->builder = gtk_builder_new ();
    if (!self->priv->builder)
    {
        g_object_unref (self);
        return NULL;
    }
    if (!gtk_builder_add_from_file
        (self->priv->builder, UI_DIR "/ccm-preferences.ui", NULL))
    {
        g_object_unref (self);
        return NULL;
    }

    self->priv->nb_screens = gdk_display_get_n_screens (display);

    self->priv->screen_pages = g_new0 (CCMPreferencesPage *, self->priv->nb_screens);
    self->priv->screen_titles = g_new0 (GtkWidget *, self->priv->nb_screens);

    for (cpt = 0; cpt < self->priv->nb_screens; ++cpt)
        ccm_preferences_create_page (self, cpt);

    shell = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "shell"));
    colormap = gdk_screen_get_rgba_colormap (screen);
    gtk_widget_set_colormap (shell, colormap);
    gtk_window_set_keep_above (GTK_WINDOW (shell), TRUE);
    gtk_window_set_focus_on_map (GTK_WINDOW (shell), TRUE);

    g_signal_connect_swapped (shell, "delete-event", 
                              G_CALLBACK (ccm_preferences_on_delete_event), self);
    g_signal_connect_swapped (shell, "response",
                              G_CALLBACK (ccm_preferences_on_response), self);

    return self;
}

void
ccm_preferences_show (CCMPreferences * self)
{
    g_return_if_fail (self != NULL);

    gint cpt;
    GtkWidget *widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                                            "shell"));

    gtk_widget_show (widget);
    for (cpt = 0; cpt < self->priv->nb_screens; cpt++)
    {
        ccm_preferences_page_set_current_section (self->priv->screen_pages[cpt],
                                                  CCM_PREFERENCES_PAGE_SECTION_GENERAL);
    }
}

void
ccm_preferences_hide (CCMPreferences * self)
{
    g_return_if_fail (self != NULL);

    GtkWidget *widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                                            "shell"));

    gtk_widget_hide (widget);
}
