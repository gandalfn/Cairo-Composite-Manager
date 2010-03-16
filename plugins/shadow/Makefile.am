PLUGIN = ccm-shadow
PLUGIN_UI = ccm-shadow.ui shadow.svg
PLUGIN_SCHEMA = ccm-shadow.schema-key.in

PLUGIN_SOURCES = ccm-shadow.c ccm-shadow.h

include $(top_srcdir)/build/plugin.am

plugin_LTLIBRARIES = libccm_shadow.la 

libccm_shadow_la_SOURCES = $(PLUGIN_SOURCES)

libccm_shadow_la_LDFLAGS =  $(PLUGIN_LDFLAGS)

libccm_shadow_la_LIBADD =  $(PLUGIN_LIBADD)