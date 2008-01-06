pixmapsdir = $(datadir)/pixmaps/cairo-compmgr

pixmaps_DATA = \
    cairo-compmgr.png \
	cairo-compmgr-on-24.png \
	cairo-compmgr-off-24.png 

schemasdir = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = cairo-compmgr.schemas.in
schemas_DATA = $(schemas_in_files:.schemas.in=.schemas)

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

@INTLTOOL_DESKTOP_RULE@

DESKTOP_IN_FILES = cairo-compmgr.desktop.in
DESKTOP_FILES =$(DESKTOP_IN_FILES:.desktop.in=.desktop)

desktopdir = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILES)

EXTRA_DIST = cairo-compmgr.desktop.in \
             $(schemas_in_files) \
			 $(pixmaps_DATA)

CLEANFILES = $(schemas_in_files) \
             $(DESKTOP_FILES)
