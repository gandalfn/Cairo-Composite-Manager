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

#ifndef _CCM_PIXMAP_BACKEND_H_
#define _CCM_PIXMAP_BACKEND_H_

#include <glib-object.h>

#include "ccm-screen.h"

#ifdef ENABLE_GLITZ_TFP_BACKEND
#include "ccm-pixmap-glitz.h"
#endif

#ifndef DISABLE_XRENDER_BACKEND
#include "ccm-pixmap-xrender.h"
#endif

#include "ccm-pixmap-image.h"
#include "ccm-pixmap-buffered-image.h"

G_BEGIN_DECLS

#define CCM_TYPE_PIXMAP_BACKEND             (ccm_pixmap_backend_get_type ())
#define CCM_PIXMAP_BACKEND(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_PIXMAP_BACKEND, CCMPixmapBackend))
#define CCM_PIXMAP_BACKEND_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_PIXMAP_BACKEND, CCMPixmapBackendClass))
#define CCM_IS_PIXMAP_BACKEND(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_PIXMAP_BACKEND))
#define CCM_IS_PIXMAP_BACKEND_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_PIXMAP_BACKEND))
#define CCM_PIXMAP_BACKEND_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_PIXMAP_BACKEND, CCMPixmapBackendClass))
#define CCM_IS_PIXMAP_BUFFERED(obj)			(CCM_IS_PIXMAP_BUFFERED_IMAGE (obj))

typedef union _CCMPixmapBackendClass CCMPixmapBackendClass;
typedef union _CCMPixmapBackend CCMPixmapBackend;

union _CCMPixmapBackendClass
{
    CCMPixmapBufferedImageClass buffered_image_class;
    CCMPixmapImageClass image_class;
#ifdef ENABLE_GLITZ_TFP_BACKEND
    CCMPixmapGlitzClass glitz_class;
#endif
#ifndef DISABLE_XRENDER_BACKEND
    CCMPixmapXRenderClass xrender_class;
#endif
};

union _CCMPixmapBackend
{
    CCMPixmapImage image;
    CCMPixmapBufferedImage buffered_image;
#ifdef ENABLE_GLITZ_TFP_BACKEND
    CCMPixmapGlitz glitz;
#endif
#ifndef DISABLE_XRENDER_BACKEND
    CCMPixmapXRender xrender;
#endif
};

GType ccm_pixmap_backend_get_type (CCMScreen * screen) G_GNUC_CONST;

G_END_DECLS

#endif                          /* _CCM_PIXMAP_BACKEND_H_ */
