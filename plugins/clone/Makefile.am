PLUGIN = ccm-clone
PLUGIN_UI = 
PLUGIN_SCHEMA = 

PLUGIN_SOURCES = ccm-clone.vala

include $(top_srcdir)/build/plugin.am

plugin_LTLIBRARIES = \
    libccm_clone.la 

libccm_clone_la_SOURCES = $(PLUGIN_SOURCES)

libccm_clone_la_LDFLAGS = $(PLUGIN_LDFLAGS)

libccm_clone_la_LIBADD = $(PLUGIN_LIBADD)
