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

#ifndef _CCM_PREFERENCES_PAGE_H_
#define _CCM_PREFERENCES_PAGE_H_

#include "ccm-preferences.h"

#include <gtk/gtk.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define CCM_TYPE_PREFERENCES_PAGE             (ccm_preferences_page_get_type ())
#define CCM_PREFERENCES_PAGE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_PREFERENCES_PAGE, CCMPreferencesPage))
#define CCM_PREFERENCES_PAGE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_PREFERENCES_PAGE, CCMPreferencesPageClass))
#define CCM_IS_PREFERENCES_PAGE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_PREFERENCES_PAGE))
#define CCM_IS_PREFERENCES_PAGE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_PREFERENCES_PAGE))
#define CCM_PREFERENCES_PAGE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_PREFERENCES_PAGE, CCMPreferencesPageClass))

typedef struct _CCMPreferencesPageClass CCMPreferencesPageClass;
typedef struct _CCMPreferencesPagePrivate CCMPreferencesPagePrivate;
typedef struct _CCMPreferencesPage CCMPreferencesPage;

struct _CCMPreferencesPageClass
{
    GObjectClass parent_class;
};

struct _CCMPreferencesPage
{
    GObject parent_instance;

    CCMPreferencesPagePrivate *priv;
};

typedef enum
{
    CCM_PREFERENCES_PAGE_SECTION_GENERAL,
    CCM_PREFERENCES_PAGE_SECTION_DESKTOP,
    CCM_PREFERENCES_PAGE_SECTION_WINDOW,
    CCM_PREFERENCES_PAGE_SECTION_EFFECTS,
    CCM_PREFERENCES_PAGE_SECTION_ACCESSIBILTY,
    CCM_PREFERENCES_PAGE_SECTION_UTILITIES,
    CCM_PREFERENCES_PAGE_SECTION_N
} CCMPreferencesPageSection;

typedef gboolean (*CCMNeedRestartFunc) (CCMPreferencesPage * self,
                                        gboolean restore_old, gpointer data);

GType               ccm_preferences_page_get_type   (void) G_GNUC_CONST;

CCMPreferencesPage* ccm_preferences_page_new            (gint screen_num);
GtkWidget* ccm_preferences_page_get_widget     (CCMPreferencesPage* self);
int  ccm_preferences_page_get_screen_num (CCMPreferencesPage* self);
void ccm_preferences_page_set_current_section (CCMPreferencesPage* self,
                                               CCMPreferencesPageSection section);
void ccm_preferences_page_section_p (CCMPreferencesPage* self,
                                     CCMPreferencesPageSection section);
void ccm_preferences_page_section_v (CCMPreferencesPage* self,
                                     CCMPreferencesPageSection section);
void ccm_preferences_page_section_register_widget (CCMPreferencesPage* self,
                                                   CCMPreferencesPageSection section,
                                                   GtkWidget* widget,
                                                   gchar* plugin);
void ccm_preferences_page_need_restart (CCMPreferencesPage* self,
                                        CCMNeedRestartFunc func, gpointer data);

G_END_DECLS

#endif                          /* _CCM_PREFERENCES_PAGE_H_ */
