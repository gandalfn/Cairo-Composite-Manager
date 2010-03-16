PLUGIN = ccm-vala-window-plugin
PLUGIN_UI = 
PLUGIN_SCHEMA = ccm-vala-window-plugin.schema-key.in

PLUGIN_SOURCES = ccm-vala-window-plugin.vala

include $(top_srcdir)/build/plugin.am

plugin_LTLIBRARIES = libccm_vala_window_plugin.la 

libccm_vala_window_plugin_la_SOURCES = $(PLUGIN_SOURCES)

libccm_vala_window_plugin_la_LDFLAGS =  $(PLUGIN_LDFLAGS)

libccm_vala_window_plugin_la_LIBADD =  $(PLUGIN_LIBADD)