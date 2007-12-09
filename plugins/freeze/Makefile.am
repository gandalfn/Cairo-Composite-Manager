## File created by the gnome-build tools

INCLUDES = \
	-I../../src \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" $(CAIRO_COMPMGR_CFLAGS)

AM_CFLAGS =\
     -O2 \
     -Wall \
	 -fPIC
	 
plugindir = $(libdir)/cairo-compmgr

plugin_LTLIBRARIES = \
	libccm_freeze.la 

libccm_freeze_la_SOURCES = \
	ccm-freeze.c ccm-freeze.h

libccm_freeze_la_LDFLAGS =  -module -avoid-version -no-undefined

plugin_in_files = ccm-freeze.plugin.desktop.in
%.plugin: %.plugin.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = ccm-freeze.plugin

schemasdir = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = ccm-freeze.schemas.in
schemas_DATA = $(schemas_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
# don't do this if we are building in eg. rpm
install-data-local:
	if test -z "$(DESTDIR)" ; then \
	  for p in $(schemas_DATA) ; do \
	    GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/plugins/freeze/$$p ; \
	  done \
	fi
else
install-data-local:
endif

EXTRA_DIST = $(schemas_in_files) \
			 $(plugin_DATA) \
			 $(plugin_in_files)

