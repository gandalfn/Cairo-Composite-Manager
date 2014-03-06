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
    ccm-object.h \
    ccm-config.h \
    ccm-source.h \
    ccm-watch.h \
    ccm-timeline.h \
    ccm-config-key.h \
    ccm-config-schema.h \
    ccm-cairo-utils.h

if HAVE_GTK
cairo_compmgr_include_HEADERS +=  \
    eggaccelerators.h \
    ccm-config-check-button.h \
    ccm-config-adjustment.h \
    ccm-config-color-button.h \
    ccm-config-entry-shortcut.h
endif

noinst_LTLIBRARIES = \
    libccm-timeline.la \
    libccm-watch.la

libccm_timeline_la_VALAFLAGS = \
    --header=ccm-timeline.h \
    --vapidir=${top_srcdir}/vapi \
    --target-glib=2.32 \
    --pkg=config \
    --pkg=source \
    --pkg=posix \
    --pkg=cairo \
    --pkg=gdk-2.0

libccm_timeline_la_SOURCES =  \
    ccm-buffer-surface.vala \
    ccm-set.vala \
    ccm-timeline.vala \
    ccm-timeout-pool.vala \
    ccm-timeout-interval.vala \
    ccm-timeout.vala

libccm_timeline_la_LIBADD = \
    $(CAIRO_COMPMGR_LIBS) \
    $(M_LIBS)

libccm_watch_la_VALAFLAGS = \
    --header=ccm-watch.h \
    --vapidir=${top_srcdir}/vapi \
    --pkg=config \
    --pkg=source

libccm_watch_la_SOURCES =  \
    ccm-watch.vala

libccm_watch_la_LIBADD = \
    $(CAIRO_COMPMGR_LIBS) \
    $(M_LIBS)

libcairo_compmgr_la_SOURCES =  \
    eggaccelerators.c \
    ccm-object.c \
    ccm-config.c \
    ccm-source.c \
    ccm-cairo-utils.c \
    ccm-config-key.c \
    ccm-config-schema.c

if HAVE_GTK
libcairo_compmgr_la_SOURCES +=  \
    ccm-config-check-button.c \
    ccm-config-adjustment.c \
    ccm-config-color-button.c \
    ccm-config-entry-shortcut.c
endif

libcairo_compmgr_la_LDFLAGS = -Wl,--as-needed -version-info @LIBS_VERSION@

libcairo_compmgr_la_LIBADD = \
    $(CAIRO_COMPMGR_LIBS) \
    $(M_LIBS) \
    libccm-timeline.la \
    libccm-watch.la

if ENABLE_GCONF
libcairo_compmgr_la_SOURCES += 	\
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

noinst_PROGRAMS = test-boxed-blur test-config test-timeline

if HAVE_GTK
noinst_PROGRAMS = test-config-widget
endif

test_boxed_blur_SOURCES = test-boxed-blur.c

test_boxed_blur_LDADD = $(CAIRO_COMPMGR_LIBS) libcairo_compmgr.la

test_config_SOURCES = test-config.c

test_config_LDADD = $(CAIRO_COMPMGR_LIBS) libcairo_compmgr.la

if HAVE_GTK
test_config_widget_SOURCES = test-config-widget.c

test_config_widget_LDADD = $(CAIRO_COMPMGR_LIBS) libcairo_compmgr.la
endif

test_timeline_SOURCES = test-timeline.c

test_timeline_LDADD = $(CAIRO_COMPMGR_LIBS) libcairo_compmgr.la

VALAFILES = \
    $(filter %.vala,$(libccm_timeline_la_SOURCES)) \
    $(filter %.vala,$(libccm_watch_la_SOURCES))

CLEANFILES = \
    ccm-timeline.h \
    ccm-watch.h \
    libccm_timeline_la_vala.stamp \
    libccm_watch_la_vala.stamp \
    $(VALAFILES:.vala=.c)
