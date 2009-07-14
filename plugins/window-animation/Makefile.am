PLUGIN = ccm-window-animation
PLUGIN_UI = 
PLUGIN_SCHEMA = ccm-window-animation.schema-key.in

PLUGIN_SOURCES = 
PLUGIN_VALA_SOURCES = ccm-window-animation.vala

include $(top_srcdir)/plugin.make

plugin_LTLIBRARIES = \
	libccm_window_animation.la 

libccm_window_animation_la_SOURCES = \
	$(PLUGIN_SOURCES)

libccm_window_animation_la_LDFLAGS =  $(PLUGIN_LDFLAGS)

libccm_window_animation_la_LIBADD =  $(PLUGIN_LIBADD)