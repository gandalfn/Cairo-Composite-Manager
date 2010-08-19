PLUGIN = ccm-perf
PLUGIN_UI = 
PLUGIN_SCHEMA = ccm-perf.schema-key.in

PLUGIN_SOURCES = ccm-perf.c ccm-perf.h

if ENABLE_PERF_PLUGIN
include $(top_srcdir)/build/plugin.am

plugin_LTLIBRARIES = libccm_perf.la 

libccm_perf_la_CFLAGS = \
    -DPACKAGE_PIXMAP_DIR=\""$(datadir)/pixmaps/cairo-compmgr"\" \
    $(CCM_PERF_PLUGIN_CFLAGS)

if ENABLE_CUDA_PERF
    libccm_perf_la_CFLAGS += -DHAVE_CUDA
endif

libccm_perf_la_SOURCES = $(PLUGIN_SOURCES)

libccm_perf_la_LDFLAGS =  $(PLUGIN_LDFLAGS)

libccm_perf_la_LIBADD =  $(PLUGIN_LIBADD) $(CCM_PERF_PLUGIN_LIBS)
else
EXTRA_DIST = $(PLUGIN_SCHEMA) \
             $(PLUGIN_SOURCES) \
             $(PLUGIN).desktop.in
endif

