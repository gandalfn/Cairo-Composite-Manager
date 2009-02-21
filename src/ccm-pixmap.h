/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007 <nicolas.bruguier@supersonicimagine.fr>
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

#ifndef _CCM_PIXMAP_H_
#define _CCM_PIXMAP_H_

#include <glib-object.h>

#include "ccm-drawable.h"

G_BEGIN_DECLS

#define CCM_TYPE_PIXMAP             (ccm_pixmap_get_type ())
#define CCM_PIXMAP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_PIXMAP, CCMPixmap))
#define CCM_PIXMAP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_PIXMAP, CCMPixmapClass))
#define CCM_IS_PIXMAP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_PIXMAP))
#define CCM_IS_PIXMAP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_PIXMAP))
#define CCM_PIXMAP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_PIXMAP, CCMPixmapClass))

#define CCM_PIXMAP_XPIXMAP(obj)     (ccm_drawable_get_xid(CCM_DRAWABLE(obj)))

struct _CCMPixmapClass
{
	CCMDrawableClass parent_class;
	
	void	   (*bind)    (CCMPixmap* self);
	void	   (*release) (CCMPixmap* self);
};

typedef struct _CCMPixmapPrivate CCMPixmapPrivate;

struct _CCMPixmap
{
	CCMDrawable 		parent_instance;
	
	CCMPixmapPrivate* 	priv;
};

GType 	    ccm_pixmap_get_type 	    (void) G_GNUC_CONST;
CCMPixmap*  ccm_pixmap_new      	    (CCMDrawable* drawable, 
					                     Pixmap xpixmap);
CCMPixmap*  ccm_pixmap_image_new   	    (CCMDrawable* drawable, 
				                         Pixmap xpixmap);
CCMPixmap*  ccm_pixmap_new_from_visual  (CCMScreen* screen, 
                                         Visual* visual, 
                                         Pixmap xpixmap);
                                         
G_END_DECLS

#endif /* _CCM_PIXMAP_H_ */
