INCLUDES = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_PLUGIN_DIR=\""$(libdir)/cairo-compmgr"\" \
    -DPACKAGE_PIXMAP_DIR=\""$(datadir)/pixmaps/cairo-compmgr"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
    $(CAIRO_COMPMGR_CFLAGS)

lib_LTLIBRARIES = libcairo_compmgr.la

libcairo_compmgr_la_SOURCES =  \
    eggaccelerators.c \
    eggaccelerators.h \
	ccm-config.h \
	ccm-config.c \
	ccm-timeline.h \
	ccm-timeline.c \
	ccm-timeout-pool.c \
	ccm-timeout-pool.h \
	ccm-cairo-utils.c \
	ccm-cairo-utils.h

libcairo_compmgr_la_LDFLAGS = -version-info @LIBS_VERSION@ 

libcairo_compmgr_la_LIBADD = $(CAIRO_COMPMGR_LIBS) $(M_LIBS)

noinst_PROGRAMS = test-boxed-blur

test_boxed_blur_SOURCES = test-boxed-blur.c

test_boxed_blur_LDADD = $(CAIRO_COMPMGR_LIBS) ../lib/libcairo_compmgr.la


