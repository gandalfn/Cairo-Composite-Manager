INCLUDES = \
	-I${top_srcdir}/lib \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_PLUGIN_DIR=\""$(libdir)/cairo-compmgr"\" \
    -DPACKAGE_PIXMAP_DIR=\""$(datadir)/pixmaps/cairo-compmgr"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
    $(CAIRO_COMPMGR_CFLAGS)

bin_PROGRAMS = ccm-schema-key-to-gconf

ccm_schema_key_to_gconf_SOURCES = ccm-schema-key-to-gconf.c

ccm_schema_key_to_gconf_LDADD = $(CAIRO_COMPMGR_LIBS) ${top_builddir}/lib/libcairo_compmgr.la

