PLUGIN = ccm-snapshot
PLUGIN_UI = ccm-snapshot.ui ccm-snapshot-dialog.ui
PLUGIN_SCHEMA = ccm-snapshot.schema-key.in

PLUGIN_SOURCES = \
    ccm-snapshot.c ccm-snapshot.h

if ENABLE_SNAPSHOT_PLUGIN
include $(top_srcdir)/build/plugin.am

plugin_LTLIBRARIES = libccm_snapshot.la 

libccm_snapshot_la_SOURCES = $(PLUGIN_SOURCES)

libccm_snapshot_la_LDFLAGS =  $(PLUGIN_LDFLAGS)

libccm_snapshot_la_LIBADD =  $(PLUGIN_LIBADD)

bin_PROGRAMS = ccm-snapshot

ccm_snapshot_SOURCES = \
    ccm-snapshot-main.c \
    ccm-snapshot-dialog.c \
    ccm-snapshot-dialog.h

ccm_snapshot_CFLAGS = \
    $(CCM_SNAPSHOT_UI_CFLAGS)

ccm_snapshot_LDFLAGS = \
    $(CCM_SNAPSHOT_UI_LDFLAGS)

ccm_snapshot_LDADD = \
    $(CCM_SNAPSHOT_UI_LIBS)
else
EXTRA_DIST = $(PLUGIN_SCHEMA) \
             $(PLUGIN_SOURCES) \
             $(PLUGIN_UI) \
             $(PLUGIN).plugin.desktop.in \
             ccm-snapshot-main.c \
             ccm-snapshot-dialog.c \
             ccm-snapshot-dialog.h
endif
