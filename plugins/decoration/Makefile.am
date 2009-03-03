PLUGIN = ccm-decoration
PLUGIN_UI = ccm-decoration.ui
PLUGIN_SCHEMA = ccm-decoration.schema-key.in

PLUGIN_SOURCES = ccm-decoration.c ccm-decoration.h

include $(top_srcdir)/plugin.make

plugin_LTLIBRARIES = \
	libccm_decoration.la 

libccm_decoration_la_SOURCES = \
	$(PLUGIN_SOURCES)

libccm_decoration_la_LDFLAGS = $(PLUGIN_LDFLAGS)

libccm_decoration_la_LIBADD = $(PLUGIN_LIBADD)
