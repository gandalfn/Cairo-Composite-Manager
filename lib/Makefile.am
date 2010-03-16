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
    ccm-config-check-button.h \
    ccm-config-adjustment.h \
    ccm-config-color-button.h \
    ccm-config-entry-shortcut.h \
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
    ccm-timeout-interval.c \
    ccm-timeout-interval.h \
    ccm-cairo-utils.c \
    ccm-cairo-utils.h \
    ccm-config-key.h \
    ccm-config-key.c \
    ccm-config-schema.h \
    ccm-config-schema.c \
    ccm-config-check-button.h \
    ccm-config-check-button.c \
    ccm-config-adjustment.h \
    ccm-config-adjustment.c \
    ccm-config-color-button.h \
    ccm-config-color-button.c \
    ccm-config-entry-shortcut.h \
    ccm-config-entry-shortcut.c

libcairo_compmgr_la_LDFLAGS = -version-info @LIBS_VERSION@ 

libcairo_compmgr_la_LIBADD = $(CAIRO_COMPMGR_LIBS) $(M_LIBS)

if ENABLE_GCONF
libcairo_compmgr_la_SOURCES += 	\
    ccm-config-gconf.h \
    ccm-config-gconf.c

libcairo_compmgr_la_LIBADD += $(CCM_GCONF_LIBS)
endif

if ENABLE_GLADE
glademodule_LTLIBRARIES = libgladeccm.la

libgladeccm_la_CFLAGS = $(CCM_GLADE_CFLAGS)

libgladeccm_la_SOURCES =  \
    glade-ccm.c

libgladeccm_la_LDFLAGS = -module -avoid-version -no-undefined

libgladeccm_la_LIBADD = $(CCM_GLADE_LIBS) libcairo_compmgr.la
endif

noinst_PROGRAMS = test-boxed-blur test-config test-config-widget test-timeline

test_boxed_blur_SOURCES = test-boxed-blur.c

test_boxed_blur_LDADD = $(CAIRO_COMPMGR_LIBS) libcairo_compmgr.la

test_config_SOURCES = test-config.c

test_config_LDADD = $(CAIRO_COMPMGR_LIBS) libcairo_compmgr.la

test_config_widget_SOURCES = test-config-widget.c

test_config_widget_LDADD = $(CAIRO_COMPMGR_LIBS) libcairo_compmgr.la

test_timeline_SOURCES = test-timeline.c

test_timeline_LDADD = $(CAIRO_COMPMGR_LIBS) libcairo_compmgr.la
