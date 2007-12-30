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

#include "ccm-pixmap-backend.h"
#include "ccm-display.h"
#include "ccm.h"

GType
ccm_pixmap_backend_get_type(CCMScreen* screen)
{
	GType type = 0;
	CCMDisplay* display = ccm_screen_get_display (screen);
	
	if (_ccm_display_use_xshm (display))
		type = ccm_pixmap_shm_get_type();
	else
		type = ccm_pixmap_image_get_type();
	
	if (_ccm_screen_native_pixmap_bind(screen))
	{
		gchar* backend;
	
		backend = _ccm_screen_get_window_backend(screen);
		
#ifndef DISABLE_XRENDER_BACKEND
		if (!g_ascii_strcasecmp(backend, "xrender"))
			type = ccm_pixmap_xrender_get_type();
#endif
#ifndef DISABLE_GLITZ_BACKEND
		if (!g_ascii_strcasecmp(backend, "glitz"))
			type = ccm_pixmap_glitz_get_type();
#endif
	}
	
	return type;
}
