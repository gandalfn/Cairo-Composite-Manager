/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-debug.h
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

#ifndef _CCM_DEBUG_H_
#define _CCM_DEBUG_H_

#include "ccm.h"

//#define CCM_DEBUG_ENABLE

void ccm_log_start_audit();
void ccm_log_audit (const char *format, ...);
void ccm_log (const char *format, ...);
void ccm_log_window (CCMWindow * window, const char *format, ...);
void ccm_log_atom (CCMDisplay * display, Atom atom, const char *format, ...);
void ccm_log_region (CCMDrawable * drawable, const char *format, ...);
void ccm_log_print_backtrace ();

#ifdef CCM_DEBUG_ENABLE

#define ccm_debug(...) ccm_log(__VA_ARGS__)
#define ccm_debug_window(window, format...) ccm_log_window(window, format)
#define ccm_debug_atom(display, atom, format...) ccm_log_atom(display, atom, format)
#define ccm_debug_region(drawable, format...) ccm_log_region(drawable, format)
#define ccm_debug_backtrace() ccm_log_print_backtrace()

#else

#define ccm_debug(...)
#define ccm_debug_window(window, format...)
#define ccm_debug_atom(display, atom, format...)
#define ccm_debug_region(drawable, format...)
#define ccm_debug_backtrace()

#endif

#endif                          /* _CCM_DEBUG_H_ */
