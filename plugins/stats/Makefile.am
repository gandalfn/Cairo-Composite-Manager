PLUGIN = ccm-stats
PLUGIN_UI =
PLUGIN_SCHEMA = ccm-stats.schema-key.in

PLUGIN_SOURCES = \
    ccm-stats.vala \
    ccm-chart.vala \
    ccm-watcher.vala \
    ccm-graphic-watcher.vala \
    ccm-cpu-watcher.vala \
    ccm-disks-watcher.vala

if ENABLE_STATS_PLUGIN

include $(top_srcdir)/build/plugin.am

plugin_LTLIBRARIES = \
    libccm_stats.la

libccm_stats_la_CFLAGS = \
    $(CCM_STATS_PLUGIN_CFLAGS)

if HAVE_NVML
libccm_stats_la_VALAFLAGS = \
    -D HAVE_NVML
endif

libccm_stats_la_SOURCES = $(PLUGIN_SOURCES)

libccm_stats_la_LDFLAGS = $(PLUGIN_LDFLAGS)

libccm_stats_la_LIBADD = $(PLUGIN_LIBADD) $(CCM_STATS_PLUGIN_LIBS)
else
EXTRA_DIST = $(PLUGIN_SCHEMA) \
             $(PLUGIN_SOURCES) \
             $(PLUGIN).desktop.in
endif

