PLUGIN = ccm-menu-animation
PLUGIN_UI = ccm-menu-animation.ui menu-animation.png
PLUGIN_SCHEMA = ccm-menu-animation.schema-key.in

PLUGIN_SOURCES = ccm-menu-animation.c ccm-menu-animation.h

include $(top_srcdir)/plugin.make

plugin_LTLIBRARIES = \
	libccm_menu_animation.la 

libccm_menu_animation_la_SOURCES = \
	$(PLUGIN_SOURCES)

libccm_menu_animation_la_LDFLAGS =  $(PLUGIN_LDFLAGS)

libccm_menu_animation_la_LIBADD =  $(PLUGIN_LIBADD)