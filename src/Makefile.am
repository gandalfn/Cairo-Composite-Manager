uidir = $(datadir)/cairo-compmgr/ui
cairo_compmgr_includedir = $(includedir)/cairo-compmgr

INCLUDES = \
    -I${top_srcdir}/lib \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_PLUGIN_DIR=\""$(libdir)/cairo-compmgr"\" \
    -DPACKAGE_PIXMAP_DIR=\""$(datadir)/pixmaps/cairo-compmgr"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
    -DUI_DIR=\""$(uidir)"\" $(CAIRO_COMPMGR_CFLAGS)

if HAVE_XRENDER_BACKEND    
INCLUDES += $(CCM_XRENDER_BACKEND_CFLAGS)
else
INCLUDES += -DDISABLE_XRENDER_BACKEND
endif

if HAVE_OPENVG_BACKEND    
INCLUDES += $(CCM_OPENVG_BACKEND_CFLAGS)
else
INCLUDES += -DDISABLE_OPENVG_BACKEND
endif

if HAVE_GLITZ_BACKEND
INCLUDES += $(CCM_GLITZ_BACKEND_CFLAGS)
else
INCLUDES += -DDISABLE_GLITZ_BACKEND
endif

if HAVE_GLITZ_TFP_BACKEND
INCLUDES += -DENABLE_GLITZ_TFP_BACKEND
endif

if HAVE_GLITZ_TFP_BACKEND
INCLUDES += -DENABLE_GLITZ_TFP_BACKEND
endif

if ENABLE_GOBJECT_INTROSPECTION
INCLUDES += $(CCM_GOBJECT_INTROSPECTION_CFLAGS) -DENABLE_GOBJECT_INTROSPECTION
endif 

ccm-marshallers.c: ccm-marshallers.list
	glib-genmarshal --prefix ccm_cclosure_marshal --body $< > $@

ccm-marshallers.h: ccm-marshallers.list
	glib-genmarshal --prefix ccm_cclosure_marshal --header $< > $@

BUILT_SOURCES = ccm-marshallers.h ccm-marshallers.c

bin_PROGRAMS = cairo-compmgr

cairo_compmgr_include_HEADERS =  \
    ccm-debug.h \
    ccm-region.h \
    ccm-drawable.h \
    ccm-image.h \
    ccm-pixmap.h \
    ccm-window.h \
    ccm-window-plugin.h \
    ccm-screen.h \
    ccm-screen-plugin.h \
    ccm-display.h \
    ccm-keybind.h \
    ccm-plugin.h \
    ccm-preferences.h \
    ccm-preferences-page-plugin.h \
    ccm-preferences-page.h \
    ccm.h
    
cairo_compmgr_SOURCES = \
    $(BUILT_SOURCES) \
    ccm-debug.h \
    ccm-debug.c \
    ccm-region.c \
    ccm-region.h \
    ccm-plugin.h \
    ccm-plugin.c \
    ccm-drawable.h \
    ccm-drawable.c \
    ccm-image.h \
    ccm-image.c \
    ccm-pixmap.h \
    ccm-pixmap.c \
    ccm-pixmap-backend.h \
    ccm-pixmap-backend.c \
    ccm-pixmap-image.h \
    ccm-pixmap-image.c \
    ccm-pixmap-buffered-image.h \
    ccm-pixmap-buffered-image.c \
    ccm-window.h \
    ccm-window.c \
    ccm-window-backend.h \
    ccm-window-backend.c \
    ccm-window-plugin.h \
    ccm-window-plugin.c \
    ccm-screen.h \
    ccm-screen.c \
    ccm-screen-plugin.h \
    ccm-screen-plugin.c \
    ccm-display.h \
    ccm-display.c \
    ccm-watch.c \
    ccm-watch.h \
    ccm-extension.h \
    ccm-extension.c \
    ccm-extension-loader.h \
    ccm-extension-loader.c \
    ccm-tray-menu.h \
    ccm-tray-menu.c \
    ccm-tray-icon.h \
    ccm-tray-icon.c \
    ccm-keybind.h \
    ccm-keybind.c \
    ccm-property-async.h \
    ccm-property-async.c \
    ccm.h \
    cairo-compmgr.c \
    ccm-preferences-page-plugin.h \
    ccm-preferences-page-plugin.c \
    ccm-preferences.h \
    ccm-preferences.c \
    ccm-cell-extension.h \
    ccm-cell-extension.c \
    ccm-cell-renderer-extension.h \
    ccm-cell-renderer-extension.c \
    ccm-preferences-page.h \
    ccm-preferences-page.c
	
if HAVE_XRENDER_BACKEND    
cairo_compmgr_SOURCES += \
    ccm-pixmap-xrender.h \
    ccm-pixmap-xrender.c \
    ccm-window-xrender.h \
	ccm-window-xrender.c
endif

if HAVE_OPENVG_BACKEND    
cairo_compmgr_SOURCES += \
	ccm-window-openvg.h \
	ccm-window-openvg.c
endif

if HAVE_GLITZ_BACKEND    
cairo_compmgr_SOURCES += \
	ccm-window-glitz.h \
	ccm-window-glitz.c
endif

if HAVE_GLITZ_TFP_BACKEND    
cairo_compmgr_SOURCES += \
	ccm-pixmap-glitz.h \
	ccm-pixmap-glitz.c
endif

cairo_compmgr_LDFLAGS = -rdynamic

cairo_compmgr_LDADD = $(CAIRO_COMPMGR_LIBS) $(M_LIBS) ../lib/libcairo_compmgr.la

if ENABLE_PERF_PLUGIN
cairo_compmgr_LDADD += $(CCM_PERF_PLUGIN_LIBS)
endif

if HAVE_XRENDER_BACKEND    
cairo_compmgr_LDADD += $(CCM_XRENDER_BACKEND_LIBS)
endif

if HAVE_OPENVG_BACKEND    
cairo_compmgr_LDADD += $(CCM_OPENVG_BACKEND_LIBS)
endif

if HAVE_GLITZ_BACKEND
cairo_compmgr_LDADD += $(CCM_GLITZ_BACKEND_LIBS) -lGL
endif

if ENABLE_GOBJECT_INTROSPECTION
cairo_compmgr_LDADD += $(CCM_GOBJECT_INTROSPECTION_LIBS)
endif 

EXTRA_DIST = ccm-marshallers.list

