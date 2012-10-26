ACLOCAL_AMFLAGS = -I build ${ACLOCAL_FLAGS}

SUBDIRS = lib src tools plugins test doc gir vapi data po

doc: all
	cd doc && $(MAKE) $(AM_MAKEFLAGS) doc

dist_noinst_SCRIPTS = autogen.sh

include $(top_srcdir)/build/debian.am
include $(top_srcdir)/build/dist-changelog.am

EXTRA_DIST = \
    $(cairo_compmgrdoc_DATA)\
    intltool-extract.in \
    intltool-merge.in \
    intltool-update.in \
    build/debian.am \
    build/dist-changelog.am \
    build/gtk-doc.am \
    build/plugin.am \
    debian/changelog \
    debian/compat \
    debian/control \
    debian/copyright \
    debian/rules \
    debian/cairo-compmgr-core.install \
    debian/cairo-compmgr-plugins.install \
    debian/cairo-compmgr-dev.install \
    debian/cairo-compmgr-vala.install \
    debian/cairo-compmgr-doc.install \
    debian/libcairo-compmgr0.install \
    debian/libcairo-compmgr-dev.install

DISTCLEANFILES = \
    intltool-extract \
    intltool-merge \
    intltool-update

DISTCHECK_CONFIGURE_FLAGS = --enable-perf-plugin --enable-stats-plugin --enable-gtk-doc

