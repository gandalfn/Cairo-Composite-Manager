/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-object.c
 * Copyright (C) Nicolas Bruguier 2010 <gandalfn@club-internet.fr>
 * 
 * libmaia is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * maiawm is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <glib-object.h>

#include "ccm-object.h"

G_DEFINE_ABSTRACT_TYPE (CCMObject, ccm_object, G_TYPE_OBJECT)

static GHashTable* s_ObjectFactory = NULL;

static void
ccm_object_init (CCMObject* inSelf)
{
}

static GObject*
ccm_object_constructor (GType inType, guint inNConstructProperties,
                        GObjectConstructParam* inConstructProperties)
{
    GObject* object;
    GObjectClass* parent_class;
    GType type = inType;

    if (s_ObjectFactory != NULL)
    {
        type = GPOINTER_TO_INT (g_hash_table_lookup (s_ObjectFactory, GINT_TO_POINTER (inType)));

        if (type == 0)
        {
            type = inType;
        }
    }

    parent_class = G_OBJECT_CLASS (ccm_object_parent_class);
    object = parent_class->constructor (type, inNConstructProperties, inConstructProperties);

    return object;
}

static void
ccm_object_class_init (CCMObjectClass * inKlass)
{
    G_OBJECT_CLASS (inKlass)->constructor = ccm_object_constructor;
}

CCMObject*
ccm_object_construct (GType inObjectType)
{
    return g_object_newv (inObjectType, 0, NULL);
}

gboolean
ccm_object_register (GType inObjectType, GType inType)
{
    if (s_ObjectFactory == NULL)
    {
        s_ObjectFactory = g_hash_table_new (NULL, NULL);
    }

    if (g_hash_table_lookup (s_ObjectFactory, GINT_TO_POINTER (inObjectType)) == NULL)
    {
        g_hash_table_insert (s_ObjectFactory, GINT_TO_POINTER (inObjectType),
                             GINT_TO_POINTER (inType));

        return TRUE;
    }

    return FALSE;
}

gboolean
ccm_object_unregister (GType inObjectType)
{
    if (s_ObjectFactory == NULL)
    {
        s_ObjectFactory = g_hash_table_new (NULL, NULL);
    }

    return g_hash_table_remove (s_ObjectFactory, GINT_TO_POINTER (inObjectType));
}
