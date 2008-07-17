INCLUDES = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_PLUGIN_DIR=\""$(libdir)/cairo-compmgr"\" \
    -DPACKAGE_PIXMAP_DIR=\""$(datadir)/pixmaps/cairo-compmgr"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
    $(CAIRO_COMPMGR_CFLAGS)

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

bin_PROGRAMS = cairo-compmgr

cairo_compmgr_SOURCES = \
    eggaccelerators.c \
    eggaccelerators.h \
	ccm-debug.h \
    ccm-debug.c \
    ccm-region.c \
	ccm-region.h \
	ccm-drawable.h \
	ccm-drawable.c \
	ccm-pixmap.h \
	ccm-pixmap.c \
	ccm-window.h \
	ccm-window.c \
	cairo-compmgr.c\
	ccm-screen.h \
	ccm-screen.c \
	ccm-display.h \
	ccm-display.c \
	ccm-watch.c \
	ccm-watch.h \
	ccm-plugin.h \
	ccm-plugin.c \
	ccm-window-plugin.h \
	ccm-window-plugin.c \
	ccm-config.h \
	ccm-config.c \
	ccm-extension.h \
	ccm-extension.c \
	ccm-extension-loader.h \
	ccm-extension-loader.c \
	ccm-screen-plugin.h \
	ccm-screen-plugin.c \
	ccm-window-backend.h \
	ccm-window-backend.c \
	ccm.h \
	ccm-tray-menu.h \
	ccm-tray-menu.c \
	ccm-tray-icon.h \
	ccm-tray-icon.c \
	ccm-pixmap-backend.h \
	ccm-pixmap-backend.c \
	ccm-image.h \
	ccm-image.c \
    ccm-pixmap-image.h \
	ccm-pixmap-image.c \
    ccm-pixmap-buffered-image.h \
	ccm-pixmap-buffered-image.c \
	ccm-keybind.h \
	ccm-keybind.c \
	ccm-property-async.h \
	ccm-property-async.c \
	ccm-timeline.h \
	ccm-timeline.c \
	ccm-timeout-pool.c \
	ccm-timeout-pool.h

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

cairo_compmgr_LDADD = $(CAIRO_COMPMGR_LIBS) $(M_LIBS)

if HAVE_XRENDER_BACKEND    
cairo_compmgr_LDADD += $(CCM_XRENDER_BACKEND_LIBS)
endif

if HAVE_OPENVG_BACKEND    
cairo_compmgr_LDADD += $(CCM_OPENVG_BACKEND_LIBS)
endif

if HAVE_GLITZ_BACKEND
cairo_compmgr_LDADD += $(CCM_GLITZ_BACKEND_LIBS) -lGL
endif

