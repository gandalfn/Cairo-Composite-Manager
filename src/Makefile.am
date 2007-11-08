## Process this file with automake to produce Makefile.in

## Created by Anjuta

INCLUDES = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_PLUGIN_DIR=\""$(libdir)/cairo-compmgr"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" $(CAIRO_COMPMGR_CFLAGS)

AM_CFLAGS =\
	 -Wall \
	 -fPIC

bin_PROGRAMS = cairo-compmgr

cairo_compmgr_SOURCES = \
	ccm-region.c \
	ccm-region.h \
	ccm-drawable.h \
	ccm-drawable.c \
	ccm-pixmap.h \
	ccm-pixmap.c \
	ccm-window.h \
	ccm-window.c \
	ccm-window-glitz.h \
	ccm-window-glitz.c \
	cairo-compmgr.c\
	ccm-screen.h \
	ccm-screen.c \
	ccm-display.h \
	ccm-display.c \
	ccm.h \
	ccm-watch.c \
	ccm-watch.h \
	ccm-window-xrender.h \
	ccm-window-xrender.c \
	ccm-window-backend.h \
	ccm-window-backend.c \
	ccm-plugin.h \
	ccm-plugin.c \
	ccm-window-plugin.h \
	ccm-window-plugin.c \
	ccm-config.h \
	ccm-config.c \
	ccm-extension.h \
	ccm-extension.c \
	ccm-extension-loader.h \
	ccm-extension-loader.c \
	ccm-screen-plugin.h \
	ccm-screen-plugin.c
	
cairo_compmgr_LDFLAGS = -rdynamic

cairo_compmgr_LDADD = $(CAIRO_COMPMGR_LIBS)

