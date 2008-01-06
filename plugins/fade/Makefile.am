## File created by the gnome-build tools

INCLUDES = \
	-I../../src \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" $(CAIRO_COMPMGR_CFLAGS)

AM_CFLAGS =\
      -O3 \
      -Wall \
 	  -fPIC \
      -ftracer \
      -fomit-frame-pointer \
      -ffast-math \
      -fno-strict-aliasing
     
plugindir = $(libdir)/cairo-compmgr

plugin_LTLIBRARIES = \
	libccm_fade.la 

libccm_fade_la_SOURCES = \
	ccm-fade.c ccm-fade.h

libccm_fade_la_LDFLAGS =  -module -avoid-version -no-undefined

plugin_in_files = ccm-fade.plugin.desktop.in
%.plugin: %.plugin.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = ccm-fade.plugin

schemasdir = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = ccm-fade.schemas.in
schemas_DATA = $(schemas_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
# don't do this if we are building in eg. rpm
install-data-local:
	if test -z "$(DESTDIR)" ; then \
	  for p in $(schemas_DATA) ; do \
	    GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/plugins/fade/$$p ; \
	  done \
	fi
else
install-data-local:
endif

EXTRA_DIST = $(schemas_in_files) \
			 $(plugin_DATA) \
			 $(plugin_in_files)

CLEAN_FILES = $(schemas_DATA) \
              $(plugin_DATA)
