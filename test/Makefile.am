INCLUDES = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_PLUGIN_DIR=\""$(libdir)/cairo-compmgr"\" \
    -DPACKAGE_PIXMAP_DIR=\""$(datadir)/pixmaps/cairo-compmgr"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
    -I../lib $(CAIRO_COMPMGR_CFLAGS)

noinst_PROGRAMS = test-clone test-clone-screen

test_clone_SOURCES = test-clone.c

test_clone_LDADD = $(CAIRO_COMPMGR_LIBS) ../lib/libcairo_compmgr.la

test_clone_screen_SOURCES = test-clone-screen.c

test_clone_screen_LDADD = $(CAIRO_COMPMGR_LIBS) ../lib/libcairo_compmgr.la

