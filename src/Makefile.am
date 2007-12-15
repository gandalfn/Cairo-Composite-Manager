INCLUDES = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_PLUGIN_DIR=\""$(libdir)/cairo-compmgr"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
    $(CAIRO_COMPMGR_CFLAGS)

if HAVE_XRENDER_BACKEND    
INCLUDES += $(CCM_XRENDER_BACKEND_CFLAGS)
else
INCLUDES += -DDISABLE_XRENDER_BACKEND
endif

if HAVE_GLITZ_BACKEND
INCLUDES += $(CCM_GLITZ_BACKEND_CFLAGS)
else
INCLUDES += -DDISABLE_GLITZ_BACKEND
endif

AM_CFLAGS =\
     -O2 \
     -Wall \
	 -fPIC

bin_PROGRAMS = cairo-compmgr

cairo_compmgr_SOURCES = \
    async-getprop.c \
	async-getprop.h \
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
	ccm-animation.h \
	ccm-animation.c

if HAVE_XRENDER_BACKEND    
cairo_compmgr_SOURCES += \
	ccm-window-xrender.h \
	ccm-window-xrender.c
endif

if HAVE_GLITZ_BACKEND    
cairo_compmgr_SOURCES += \
	ccm-window-glitz.h \
	ccm-window-glitz.c
endif

cairo_compmgr_LDFLAGS = -rdynamic

cairo_compmgr_LDADD = $(CAIRO_COMPMGR_LIBS) $(M_LIBS)

if HAVE_XRENDER_BACKEND    
cairo_compmgr_LDADD += $(CCM_XRENDER_BACKEND_LIBS)
endif

if HAVE_GLITZ_BACKEND
cairo_compmgr_LDADD += $(CCM_GLITZ_BACKEND_LIBS) -lGL
endif

