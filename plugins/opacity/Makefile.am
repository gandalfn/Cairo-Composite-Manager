PLUGIN = ccm-opacity
PLUGIN_UI = 
PLUGIN_SCHEMA = ccm-opacity.schema-key.in

PLUGIN_SOURCES = ccm-opacity.c ccm-opacity.h

include $(top_srcdir)/plugin.make

plugin_LTLIBRARIES = \
	libccm_opacity.la 

libccm_opacity_la_SOURCES = \
	$(PLUGIN_SOURCES)

libccm_opacity_la_LDFLAGS =  $(PLUGIN_LDFLAGS)

libccm_opacity_la_LIBADD =  $(PLUGIN_LIBADD)