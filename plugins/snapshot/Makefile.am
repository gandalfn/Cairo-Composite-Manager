PLUGIN = ccm-snapshot
PLUGIN_UI = ccm-snapshot.ui
PLUGIN_SCHEMA = ccm-snapshot.schema-key.in

PLUGIN_SOURCES = \
    ccm-snapshot.c ccm-snapshot.h \
    ccm-snapshot-dialog.c ccm-snapshot-dialog.h

include $(top_srcdir)/build/plugin.am

plugin_LTLIBRARIES = libccm_snapshot.la 

libccm_snapshot_la_SOURCES = $(PLUGIN_SOURCES)

libccm_snapshot_la_LDFLAGS =  $(PLUGIN_LDFLAGS)

libccm_snapshot_la_LIBADD =  $(PLUGIN_LIBADD)