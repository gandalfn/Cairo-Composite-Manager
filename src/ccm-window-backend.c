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

#include "ccm-window-backend.h"
#include "ccm.h"

GType
ccm_window_backend_get_type(void)
{
	static gchar* backend = NULL;
	GType type = ccm_window_xrender_get_type();
	
	if (!backend)
	{
		CCMConfig* config_backend = ccm_config_new(-1, NULL, "backend");
		backend = ccm_config_get_string(config_backend);
		g_object_unref(config_backend);
	}
	
	if (backend)
	{
		if (!g_ascii_strcasecmp(backend, "glitz"))
			type = ccm_window_glitz_get_type();
	}
	
	return type;
}
