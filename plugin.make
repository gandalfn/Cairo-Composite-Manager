uidir = $(datadir)/cairo-compmgr/ui

INCLUDES = \
	-I${top_srcdir}/src \
	-I${top_srcdir}/lib \
	-DUI_DIR=\"$(uidir)\" \
	$(CAIRO_COMPMGR_CFLAGS)

plugindir = $(libdir)/cairo-compmgr

PLUGIN_LDFLAGS = -module -avoid-version -no-undefined
PLUGIN_LIBADD = ${top_builddir}/lib/libcairo_compmgr.la

if HAVE_VALA
BUILT_SOURCES = \
    $(shell if [ ! "$(PLUGIN_VALA_SOURCES)" = "" ]; then echo $(PLUGIN).vala.stamp; fi)

PLUGIN_SOURCES += \
    $(PLUGIN_VALA_SOURCES:.vala=.c)

$(PLUGIN).vala.stamp: $(PLUGIN_VALA_SOURCES)
	$(VALAC) --vapidir=${top_srcdir}/vapi --pkg=config --pkg=cairo-compmgr -C $^
	touch $@
endif

plugin_in_files = $(PLUGIN).plugin.desktop.in
%.plugin: %.plugin.desktop $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = $(PLUGIN).plugin

schemakeydir = $(datadir)/cairo-compmgr/schemas
schemakey_DATA = $(PLUGIN_SCHEMA:.schema-key.in=.schema-key)

%.schema-key: %.schema-key.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

if ENABLE_GCONF
schemasdir = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = $(PLUGIN_SCHEMA:.schema-key.in=.schemas.in)
schemas_DATA = $(schemas_in_files:.schemas.in=.schemas)

%.schemas.in: %.schema-key
	${top_builddir}/tools/ccm-schema-key-to-gconf --plugin --schema-key=$< --schema-gconf=$@

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)" ; then \
	  for p in $(schemas_DATA) ; do \
	    GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p ; \
	  done \
	fi
else
install-data-local:
endif
endif

ui_DATA = $(PLUGIN_UI)

EXTRA_DIST = $(PLUGIN_VALA_SOURCES) \
             $(PLUGIN_SCHEMA) \
             $(ui_DATA) \
             $(plugin_in_files)

CLEANFILES = $(PLUGIN).vala.stamp \
             $(PLUGIN_VALA_SOURCES:.vala=.c) \
	         $(PLUGIN_VALA_SOURCES:.vala=.h) \
             $(schemakey_DATA) \
             $(plugin_DATA)

if ENABLE_GCONF
CLEANFILES += $(schemas_DATA) \
              $(schemas_in_files)
endif
