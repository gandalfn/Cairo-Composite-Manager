pixmapsdir = $(datadir)/pixmaps/cairo-compmgr
uidir = $(datadir)/cairo-compmgr/ui
pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = cairo-compmgr.pc

pixmaps_DATA = \
    cairo-compmgr.png \
    cairo-compmgr-on-24.png \
    cairo-compmgr-off-24.png 

ui_DATA = ccm-preferences.ui

CAIRO_COMPMGR_SCHEMAS = \
    ccm-display.schema-key.in \
    ccm-screen.schema-key.in

schemakeydir = $(datadir)/cairo-compmgr/schemas
schemakey_DATA = $(CAIRO_COMPMGR_SCHEMAS:.schema-key.in=.schema-key)

%.schema-key: %.schema-key.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

if ENABLE_GCONF
schemasdir = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = $(CAIRO_COMPMGR_SCHEMAS:.schema-key.in=.schemas.in)
schemas_DATA = $(schemas_in_files:.schemas.in=.schemas)

ccm-display.schemas.in: ccm-display.schema-key
	${top_builddir}/tools/ccm-schema-key-to-gconf --schema-key=$< --schema-gconf=$@

ccm-screen.schemas.in: ccm-screen.schema-key
	${top_builddir}/tools/ccm-schema-key-to-gconf --plugin --schema-key=$< --schema-gconf=$@

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
# don't do this if we are building in eg. rpm
install-data-local:
	if test -z "$(DESTDIR)" ; then \
	  for p in $(schemas_DATA) ; do \
	    GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/data/$$p ; \
	  done \
	fi
else
install-data-local:
endif
endif

@INTLTOOL_DESKTOP_RULE@

DESKTOP_IN_FILES = cairo-compmgr.desktop.in
DESKTOP_FILES =$(DESKTOP_IN_FILES:.desktop.in=.desktop)

desktopdir = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILES)

gladecatalog_DATA =
noinst_DATA =

if ENABLE_GLADE
gladecatalog_DATA += cairo-compmgr.xml
else
noinst_DATA += cairo-compmgr.xml
endif

EXTRA_DIST = cairo-compmgr.desktop.in \
             $(CAIRO_COMPMGR_SCHEMAS) \
             $(ui_DATA) \
             $(pkgconfig_DATA) \
             $(pixmaps_DATA) \
             $(gladecatalog_DATA) \
             $(noinst_DATA)

CLEANFILES = $(schemakey_DATA) \
             $(pkgconfig_DATA) \
             $(DESKTOP_FILES)

if ENABLE_GCONF
CLEANFILES += $(schemas_DATA) \
              $(schemas_in_files)
endif
