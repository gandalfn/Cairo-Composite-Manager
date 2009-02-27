cairo_compmgr_includedir = $(includedir)/cairo-compmgr

INCLUDES = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_PLUGIN_DIR=\""$(libdir)/cairo-compmgr"\" \
    -DPACKAGE_PIXMAP_DIR=\""$(datadir)/pixmaps/cairo-compmgr"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
    $(CAIRO_COMPMGR_CFLAGS)

if ENABLE_GCONF
INCLUDES += $(CCM_GCONF_CFLAGS) -DENABLE_GCONF
endif

lib_LTLIBRARIES = libcairo_compmgr.la

cairo_compmgr_include_HEADERS =  \
	ccm-config.h \
	ccm-timeline.h \
	ccm-cairo-utils.h

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
	ccm-cairo-utils.h \
	ccm-config-key.h \
	ccm-config-key.c \
	ccm-config-schema.h \
	ccm-config-schema.c

libcairo_compmgr_la_LDFLAGS = -version-info @LIBS_VERSION@ 

libcairo_compmgr_la_LIBADD = $(CAIRO_COMPMGR_LIBS) $(M_LIBS)

if ENABLE_GCONF
libcairo_compmgr_la_SOURCES += 	\
    ccm-config-gconf.h \
	ccm-config-gconf.c

libcairo_compmgr_la_LIBADD += $(CCM_GCONF_LIBS)
endif

noinst_PROGRAMS = test-boxed-blur test-config

test_boxed_blur_SOURCES = test-boxed-blur.c

test_boxed_blur_LDADD = $(CAIRO_COMPMGR_LIBS) libcairo_compmgr.la

test_config_SOURCES = test-config.c

test_config_LDADD = $(CAIRO_COMPMGR_LIBS) libcairo_compmgr.la
