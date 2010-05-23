INCLUDES = \
    -DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
    -DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
    -DPACKAGE_PLUGIN_DIR=\""$(libdir)/cairo-compmgr"\" \
    -DPACKAGE_PIXMAP_DIR=\""$(datadir)/pixmaps/cairo-compmgr"\" \
    -DPACKAGE_DATA_DIR=\""$(datadir)"\" \
    -I../lib $(CAIRO_COMPMGR_CFLAGS)

noinst_PROGRAMS = test-clone test-clone-screen test-recorder

test_clone_SOURCES = test-clone.c

test_clone_LDADD = $(CAIRO_COMPMGR_LIBS) ../lib/libcairo_compmgr.la

test_clone_screen_SOURCES = test-clone-screen.c

test_clone_screen_LDADD = $(CAIRO_COMPMGR_LIBS) ../lib/libcairo_compmgr.la

test_recorder_VALAFLAGS = \
    --pkg gdk-x11-2.0 \
    --pkg gtk+-2.0

test_recorder_SOURCES = test-recorder.vala

test_recorder_LDADD = $(CAIRO_COMPMGR_LIBS) ../lib/libcairo_compmgr.la

CLEANFILES = \
    test_recorder_vala.stamp \
     $(test_recorder_SOURCES:.vala=.c)