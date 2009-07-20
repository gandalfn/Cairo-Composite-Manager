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

#ifndef _CCM_WINDOW_BACKEND_H
#define _CCM_WINDOW_BACKEND_H

#include <glib.h>

#include "ccm-screen.h"

#ifndef DISABLE_GLITZ_BACKEND
#include "ccm-window-glitz.h"
#endif

#ifndef DISABLE_OPENVG_BACKEND
#include "ccm-window-openvg.h"
#endif

#ifndef DISABLE_XRENDER_BACKEND
#include "ccm-window-xrender.h"
#endif

G_BEGIN_DECLS typedef union _CCMWindowBackend CCMWindowBackend;
typedef union _CCMWindowBackendClass CCMWindowBackendClass;

#define CCM_TYPE_WINDOW_BACKEND(screen)     (ccm_window_backend_get_type (screen))
#define CCM_WINDOW_BACKEND(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_WINDOW_BACKEND, CCMWindowBackend))
#define CCM_WINDOW_BACKEND_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_WINDOW_BACKEND, CCMWindowBackendClass))
#define CCM_IS_WINDOW_BACKEND(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_WINDOW_BACKEND))
#define CCM_IS_WINDOW_BACKEND_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_WINDOW_BACKEND))
#define CCM_WINDOW_BACKEND_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_WINDOW_BACKEND, CCMWindowBackendClass))

union _CCMWindowBackendClass
{
#ifndef DISABLE_GLITZ_BACKEND
    CCMWindowGlitzClass glitz_class;
#endif
#ifndef DISABLE_OPENVG_BACKEND
    CCMWindowOpenVGClass openvg_class;
#endif
#ifndef DISABLE_XRENDER_BACKEND
    CCMWindowXRenderClass xrender_class;
#endif
};

union _CCMWindowBackend
{
#ifndef DISABLE_GLITZ_BACKEND
    CCMWindowGlitz glitz;
#endif
#ifndef DISABLE_OPENVG_BACKEND
    CCMWindowOpenVG openvg;
#endif
#ifndef DISABLE_XRENDER_BACKEND
    CCMWindowXRender xrender;
#endif
};

GType ccm_window_backend_get_type (CCMScreen* screen) G_GNUC_CONST;

G_END_DECLS

#endif                          /* _CCM_WINDOW_BACKEND_H */
