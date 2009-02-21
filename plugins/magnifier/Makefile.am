PLUGIN = ccm-magnifier
PLUGIN_UI = 
PLUGIN_SCHEMA = ccm-magnifier.schema-key.in

PLUGIN_SOURCES = ccm-magnifier.c ccm-magnifier.h

include $(top_srcdir)/plugin.make

plugin_LTLIBRARIES = \
	libccm_magnifier.la 

libccm_magnifier_la_SOURCES = \
	$(PLUGIN_SOURCES)

libccm_magnifier_la_LDFLAGS =  $(PLUGIN_LDFLAGS)

libccm_magnifier_la_LIBADD =  $(PLUGIN_LIBADD)