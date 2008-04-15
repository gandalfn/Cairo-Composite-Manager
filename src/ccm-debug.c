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

#include <unistd.h>
#include <glib.h>

#include "ccm-debug.h"
#include "ccm-window.h"
#include "ccm-display.h"

void
ccm_log (const char *format, ...)
{
	va_list args;
	gchar* formatted;

	va_start (args, format);
	formatted = g_strdup_vprintf (format, args);
	va_end (args);
	
	g_print("%s\n", formatted);
	g_free(formatted);
} 

void
ccm_log_window (CCMWindow* window, const char *format, ...)
{
	va_list args;
	gchar* formatted;

	va_start (args, format);
	formatted = g_strdup_vprintf (format, args);
	va_end (args);
	
	g_print("%s: 0x%lx %s\n", formatted, CCM_WINDOW_XWINDOW(window), 
			ccm_window_get_name(window));
	g_free(formatted);
} 

void
ccm_log_atom (CCMDisplay* display, Atom atom, const char *format, ...)
{
	va_list args;
	gchar* formatted;

	va_start (args, format);
	formatted = g_strdup_vprintf (format, args);
	va_end (args);
	
	g_print("%s: %s\n", formatted, 
			XGetAtomName (CCM_DISPLAY_XDISPLAY(display), atom));
	g_free(formatted);
} 
