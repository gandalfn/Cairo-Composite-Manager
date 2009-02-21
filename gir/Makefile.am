GIR_FILES =
INJECTS = \
    xlib-2.0.injections \
    cairo-1.0.injections

BUILT_SOURCES = $(GIR_SOURCES) $(TYPELIBS)

if ENABLE_GOBJECT_INTROSPECTION
xlib-2.0.gir: xlib-2.0.injections
	$(G_IR_SCANNER) -v --inject $(GIRDIR)/$@ $< $@

cairo-1.0.gir: cairo-1.0.injections
	$(G_IR_SCANNER) -v --inject $(GIRDIR)/$@ $< $@

BUILT_SOURCES += xlib-2.0.gir cairo-1.0.gir

GIR_DEPENDS = \
    ${top_srcdir}/src/ccm.h \
    ${top_srcdir}/src/ccm-plugin.h \
    ${top_srcdir}/src/ccm-drawable.h \
    ${top_srcdir}/src/ccm-pixmap.h \
    ${top_srcdir}/src/ccm-window.h \
    ${top_srcdir}/src/ccm-window-plugin.h \
    ${top_srcdir}/src/ccm-screen.h \
    ${top_srcdir}/src/ccm-screen-plugin.h \
    ${top_srcdir}/src/ccm-display.h \
    ${top_srcdir}/src/ccm-preferences.h \
    ${top_srcdir}/src/ccm-preferences-page.h \
    ${top_srcdir}/src/ccm-preferences-page-plugin.h \
    ${top_srcdir}/src/ccm-region.h \
    ${top_srcdir}/src/ccm-keybind.h \
    ${top_srcdir}/src/ccm-image.h \
    ${top_srcdir}/src/ccm-debug.h \
    ${top_srcdir}/lib/ccm-cairo-utils.h \
    ${top_srcdir}/lib/ccm-config.h \
    ${top_srcdir}/lib/ccm-timeline.h  

CCM-1.0.gir: Makefile xlib-2.0.gir $(GIR_DEPENDS)
	$(G_IR_SCANNER) -v --namespace CCM --nsversion=1.0 \
	--add-include-path=. \
    --include GObject-2.0 \
    --include cairo-1.0 \
    --include Gtk-2.0 \
    --include GConf-2.0 \
    --include xlib-2.0 \
    --pkg gobject-2.0 \
    --pkg gtk+-2.0 \
    --pkg cairo \
    --pkg gconf-2.0 \
    --output=$@ \
    --program=${top_builddir}/src/cairo-compmgr \
    --library=cairo_compmgr \
    $(GIR_DEPENDS)

GIR_FILES += CCM-1.0.gir
endif

%.typelib: %.gir
	$(G_IR_COMPILER) --shared-library=cairo_compmgr --includedir=. $< -o $@

girdir = $(GIRINSTDIR)
gir_DATA = $(GIR_FILES)

typelibsdir = $(TYPELIBINSTDIR)
typelibs_DATA = $(GIR_FILES:.gir=.typelib)

CLEANFILES = $(BUILT_SOURCES) $(typelibs_DATA) $(gir_DATA)

EXTRA_DIST = $(INJECTS)

