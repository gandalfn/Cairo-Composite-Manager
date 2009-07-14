/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2009 <gandalfn@club-internet.fr>
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
 
#ifndef _CCM_OBJECT_H_
#define _CCM_OBJECT_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define CCM_OBJECT_PREFETCH 5

#define CCM_DEFINE_TYPE_EXTENDED(class_name, prefix, parent_class_type, CODE) \
\
static void     prefix##_init              (class_name        *self); \
static void     prefix##_class_init        (class_name##Class *klass); \
static gpointer prefix##_parent_class = NULL; \
static void     prefix##_class_intern_init (gpointer klass) \
{ \
  prefix##_parent_class = g_type_class_peek_parent (klass); \
  prefix##_class_init ((class_name##Class*) klass); \
} \
\
GType \
prefix##_get_type (void) \
{ \
  static volatile gsize g_define_type_id__volatile = 0; \
  if (g_once_init_enter (&g_define_type_id__volatile))  \
  { \
    static const GTypeInfo type_info = { \
			sizeof (class_name##Class), \
			NULL, \
			NULL, \
			(GClassInitFunc)prefix##_class_intern_init, \
			NULL, \
			NULL, \
			sizeof (class_name), \
			CCM_OBJECT_PREFETCH, \
			(GInstanceInitFunc)prefix##_init \
		}; \
	GType g_define_type_id = \
			g_type_register_static(parent_class_type, #class_name, \
			                       &type_info, (GTypeFlags) 0); \
    { \
		CODE; \
	} \
    g_once_init_leave (&g_define_type_id__volatile, g_define_type_id); \
  }					\
  return g_define_type_id__volatile;	\
}

#define CCM_DEFINE_TYPE(class_name, prefix, parent_class_type) CCM_DEFINE_TYPE_EXTENDED (class_name, prefix, parent_class_type, {})

G_END_DECLS

#endif /* _CCM_OBJECT_H_ */
