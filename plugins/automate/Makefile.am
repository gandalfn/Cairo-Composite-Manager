PLUGIN = ccm-automate
PLUGIN_UI = ccm-automate.ui
PLUGIN_SCHEMA = ccm-automate.schema-key.in

PLUGIN_SOURCES =
PLUGIN_VALA_SOURCES = \
    ccm-action.vala \
    ccm-action-pointer.vala \
    ccm-action-pointer-motion.vala \
    ccm-action-pointer-press.vala \
    ccm-action-pointer-release.vala \
    ccm-story-board.vala \
	ccm-automate-dialog.vala \
	ccm-automate.vala
	
include $(top_srcdir)/plugin.make

plugin_LTLIBRARIES = \
	libccm-automate.la 

libccm_automate_la_SOURCES = \
    $(PLUGIN_SOURCES) \
    $(PLUGIN_VALA_SOURCES)
    
libccm_automate_la_LDFLAGS =  $(PLUGIN_LDFLAGS)

libccm_automate_la_LIBADD =  $(PLUGIN_LIBADD)

          
