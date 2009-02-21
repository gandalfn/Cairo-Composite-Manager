PLUGIN = ccm-fade
PLUGIN_UI = 
PLUGIN_SCHEMA = ccm-fade.schema-key.in

PLUGIN_SOURCES = ccm-fade.c ccm-fade.h

include $(top_srcdir)/plugin.make

plugin_LTLIBRARIES = \
	libccm_fade.la 

libccm_fade_la_SOURCES = \
	$(PLUGIN_SOURCES)

libccm_fade_la_LDFLAGS =  $(PLUGIN_LDFLAGS)

libccm_fade_la_LIBADD =  $(PLUGIN_LIBADD)