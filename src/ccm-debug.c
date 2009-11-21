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

/* addr2line.c -- convert addresses to line number and function name
   Copyright 1997, 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
   Contributed by Ulrich Lauther <Ulrich.Lauther@mchp.siemens.de>

   This file was part of GNU Binutils.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <config.h>

#ifdef HAVE_EDEBUG
#define fatal(a, b) exit(1)
#define bfd_fatal(a) exit(1)
#define bfd_nonfatal(a) exit(1)
#define list_matching_formats(a) exit(1)

/* 2 characters for each byte, plus 1 each for 0, x, and NULL */
#define PTRSTR_LEN (sizeof(void *) * 2 + 3)
#define true 1
#define false 0

#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>
#include <bfd.h>
#include <libiberty.h>
#include <dlfcn.h>
#include <link.h>
#endif /* HAVE_EDEBUG */
 
#include <unistd.h>
#include <execinfo.h>
#include <glib.h>

#include "ccm-debug.h"
#include "ccm-drawable.h"
#include "ccm-window.h"
#include "ccm-display.h"

static GTimer *timer = NULL;

#ifdef HAVE_EDEBUG
static asymbol **syms;		/* Symbol table.  */

/* 150 isn't special; it's just an arbitrary non-ASCII char value.  */
#define OPTION_DEMANGLER	(150)

static void slurp_symtab(bfd * abfd);
static void find_address_in_section(bfd *abfd, asection *section, void *data);

/* Read in the symbol table.  */

static void slurp_symtab(bfd * abfd)
{
	long symcount;
	unsigned int size;

	if ((bfd_get_file_flags(abfd) & HAS_SYMS) == 0)
		return;

	symcount = bfd_read_minisymbols(abfd, false, (PTR) & syms, &size);
	if (symcount == 0)
		symcount = bfd_read_minisymbols(abfd, true /* dynamic */ ,
						(PTR) & syms, &size);

	if (symcount < 0)
		bfd_fatal(bfd_get_filename(abfd));
}

/* These global variables are used to pass information between
   translate_addresses and find_address_in_section.  */

static bfd_vma pc;
static const char *filename;
static const char *functionname;
static unsigned int line;
static int found;

/* Look for an address in a section.  This is called via
   bfd_map_over_sections.  */

static void 
find_address_in_section(bfd *abfd, asection *section, void *data __attribute__ ((__unused__)) )
{
	bfd_vma vma;
	bfd_size_type size;

	if (found)
		return;

	if ((bfd_get_section_flags(abfd, section) & SEC_ALLOC) == 0)
		return;

	vma = bfd_get_section_vma(abfd, section);
	if (pc < vma)
		return;

	size = bfd_section_size(abfd, section);
	if (pc >= vma + size)
		return;

	found = bfd_find_nearest_line(abfd, section, syms, pc - vma,
				      &filename, &functionname, &line);
}

static char** 
translate_addresses_buf(bfd * abfd, bfd_vma *addr, int naddr)
{
	int naddr_orig = naddr;
	char b;
	int total  = 0;
	enum { Count, Print } state;
	char *buf = &b;
	int len = 0;
	char **ret_buf = NULL;

	/* iterate over the formating twice.
	 * the first time we count how much space we need
	 * the second time we do the actual printing */
	for (state=Count; state<=Print; state++) 
	{
		if (state == Print) 
		{
			ret_buf = malloc(total + sizeof(char*)*naddr);
			buf = (char*)(ret_buf + naddr);
			len = total;
		}
		while (naddr) 
		{
			if (state == Print)
				ret_buf[naddr-1] = buf;
			pc = addr[naddr-1];

			found = false;
			bfd_map_over_sections(abfd, find_address_in_section,
			                      (PTR) NULL);

			if (!found) 
			{
				total += snprintf(buf, len, "[0x%llx] \?\?() \?\?:0",(long long unsigned int) addr[naddr-1]) + 1;
			} 
			else 
			{
				const char *name;

				name = functionname;
				if (name == NULL || *name == '\0')
					name = "??";
				if (filename != NULL) 
				{
					char *h;

					h = strrchr(filename, '/');
					if (h != NULL)
						filename = h + 1;
				}
				total += snprintf(buf, len, "%s:%u\t%s()", filename ? filename : "??",
				                  line, name) + 1;

			}
			if (state == Print) 
			{
				/* set buf just past the end of string */
				buf = buf + total + 1;
			}
			naddr--;
		}
		naddr = naddr_orig;
	}
	return ret_buf;
}

/* Process a file.  */
static char **
process_file(const char *file_name, bfd_vma *addr, int naddr)
{
	bfd *abfd;
	char **matching;
	char **ret_buf;

	abfd = bfd_openr(file_name, NULL);
	
	if (abfd == NULL)
		bfd_fatal(file_name);

	if (bfd_check_format(abfd, bfd_archive))
		fatal("%s: can not get addresses from archive", file_name);

	if (!bfd_check_format_matches(abfd, bfd_object, &matching)) 
	{
		bfd_nonfatal(bfd_get_filename(abfd));
		if (bfd_get_error() == bfd_error_file_ambiguously_recognized) 
		{
			list_matching_formats(matching);
			free(matching);
		}
		xexit(1);
	}

	slurp_symtab(abfd);

	ret_buf = translate_addresses_buf(abfd, addr, naddr);

	if (syms != NULL) 
	{
		free(syms);
		syms = NULL;
	}

	bfd_close(abfd);
	return ret_buf;
}

#define MAX_DEPTH 16

struct file_match 
{
	const char *file;
	void *address;
	void *base;
	void *hdr;
};

static int 
find_matching_file(struct dl_phdr_info *info,
                   size_t size, void *data)
{
	struct file_match *match = data;
	/* This code is modeled from Gfind_proc_info-lsb.c:callback() from libunwind */
	long n;
	const ElfW(Phdr) *phdr;
	ElfW(Addr) load_base = info->dlpi_addr;
	phdr = info->dlpi_phdr;
	for (n = info->dlpi_phnum; --n >= 0; phdr++) 
	{
		if (phdr->p_type == PT_LOAD) 
		{
			ElfW(Addr) vaddr = phdr->p_vaddr + load_base;
			if (match->address >= (void*)vaddr && match->address < (void*)(vaddr + phdr->p_memsz)) 
			{
				/* we found a match */
				match->file = info->dlpi_name;
				match->base = (void*)info->dlpi_addr;
			}
		}
	}
	return 0;
}

static char **
ccm_backtrace_symbols(void *const *buffer, int size)
{
	int stack_depth = size - 1;
	int x,y;
	/* discard calling function */
	int total = 0;

	char ***locations;
	char **final;
	char *f_strings;

	locations = malloc(sizeof(char**) * (stack_depth+1));

	bfd_init();
	for(x=stack_depth, y=0; x>=0; x--, y++)
	{
		struct file_match match = { .address = buffer[x] };
		char **ret_buf;
		bfd_vma addr;
		dl_iterate_phdr(find_matching_file, &match);
		addr = buffer[x] - match.base;
		if (match.file && strlen(match.file))
			ret_buf = process_file(match.file, &addr, 1);
		else
			ret_buf = process_file("/proc/self/exe", &addr, 1);
		locations[x] = ret_buf;
		total += strlen(ret_buf[0]) + 1;
	}

	/* allocate the array of char* we are going to return and extra space for
	 * all of the strings */
	final = malloc(total + (stack_depth + 1) * sizeof(char*));
	/* get a pointer to the extra space */
	f_strings = (char*)(final + stack_depth + 1);

	/* fill in all of strings and pointers */
	for(x=stack_depth; x>=0; x--)
	{
		strcpy(f_strings, locations[x][0]);
		free(locations[x]);
		final[x] = f_strings;
		f_strings += strlen(f_strings) + 1;
	}

	free(locations);

	return final;
}
#endif /* HAVE_EDEBUG */

static void
ccm_print_log (const char *format, ...)
{
    va_list args;
    gchar *formatted;

    if (!timer)
        timer = g_timer_new ();

    va_start (args, format);
    formatted = g_strdup_vprintf (format, args);
    va_end (args);

    g_print ("%f: %s", g_timer_elapsed (timer, NULL), formatted);
    g_free (formatted);
}

void
ccm_log (const char *format, ...)
{
    va_list args;
    gchar *formatted;

    va_start (args, format);
    formatted = g_strdup_vprintf (format, args);
    va_end (args);

    ccm_print_log ("%s\n", formatted);
    g_free (formatted);
}

void
ccm_log_window (CCMWindow * window, const char *format, ...)
{
    va_list args;
    gchar *formatted;

    va_start (args, format);
    formatted = g_strdup_vprintf (format, args);
    va_end (args);

    ccm_print_log ("%s: 0x%lx\n", formatted, CCM_WINDOW_XWINDOW (window));
    g_free (formatted);
}

void
ccm_log_atom (CCMDisplay * display, Atom atom, const char *format, ...)
{
    va_list args;
    gchar *formatted;

    va_start (args, format);
    formatted = g_strdup_vprintf (format, args);
    va_end (args);

    ccm_print_log ("%s: %s\n", formatted,
                   XGetAtomName (CCM_DISPLAY_XDISPLAY (display), atom));
    g_free (formatted);
}

void
ccm_log_region (CCMDrawable * drawable, const char *format, ...)
{
    va_list args;
    gchar *formatted;
    const CCMRegion *damaged, *geometry;
    va_start (args, format);
    formatted = g_strdup_vprintf (format, args);
    va_end (args);
    cairo_rectangle_t *rects = NULL;
    gint cpt, nb_rects;

    ccm_print_log ("%s: 0x%lx\n", formatted, ccm_drawable_get_xid (drawable));
    geometry = ccm_drawable_get_geometry (drawable);
    if (geometry)
    {
        ccm_log ("-> geometry : ");
        ccm_region_get_rectangles ((CCMRegion *) geometry, &rects, &nb_rects);

        for (cpt = 0; cpt < nb_rects; ++cpt)
            ccm_log ("--> %i, %i, %i, %i", (int) rects[cpt].x,
                     (int) rects[cpt].y, (int) rects[cpt].width,
                     (int) rects[cpt].height);
        if (rects) cairo_rectangles_free (rects, nb_rects);
		rects = NULL;
    }

    g_object_get (drawable, "damaged", &damaged, NULL);
    if (damaged)
    {
        ccm_log ("-> damaged : ");
        ccm_region_get_rectangles ((CCMRegion *) damaged, &rects, &nb_rects);

        for (cpt = 0; cpt < nb_rects; ++cpt)
            ccm_log ("--> %i, %i, %i, %i", (int) rects[cpt].x,
                     (int) rects[cpt].y, (int) rects[cpt].width,
                     (int) rects[cpt].height);
        if (rects) cairo_rectangles_free (rects, nb_rects);
    }

    g_free (formatted);
}

void
ccm_log_print_backtrace ()
{
    void *array[30];
    int size;
    char **strings;
    int i;

    size = backtrace (array, 30);
#ifdef HAVE_EDEBUG
    strings = ccm_backtrace_symbols (array, size);
#else
	strings = backtrace_symbols (array, size);
#endif /* HAVE_EDEBUG */

    for (i = 0; i < size; i++)
        ccm_log (strings[i]);

    g_free (strings);
}
