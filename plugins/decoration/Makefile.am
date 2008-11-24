## File created by the gnome-build tools

INCLUDES = \
	-I../../src \
	-I../../lib \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" $(CAIRO_COMPMGR_CFLAGS)

plugindir = $(libdir)/cairo-compmgr

plugin_LTLIBRARIES = \
	libccm_decoration.la 

libccm_decoration_la_SOURCES = \
	ccm-decoration.c ccm-decoration.h

libccm_decoration_la_LDFLAGS =  -module -avoid-version -no-undefined

libccm_decoration_la_LIBADD =  ../../lib/libcairo_compmgr.la

plugin_in_files = ccm-decoration.plugin.desktop.in
%.plugin: %.plugin.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = ccm-decoration.plugin

schemasdir = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = ccm-decoration.schemas.in
schemas_DATA = $(schemas_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
# don't do this if we are building in eg. rpm
install-data-local:
	if test -z "$(DESTDIR)" ; then \
	  for p in $(schemas_DATA) ; do \
	    GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/plugins/decoration/$$p ; \
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
