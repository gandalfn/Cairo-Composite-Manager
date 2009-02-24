SUBDIRS = lib src tools plugins doc gir vapi data po

doc: all
	cd doc && $(MAKE) $(AM_MAKEFLAGS) doc
	
cairo_compmgrdocdir = ${prefix}/doc/cairo-compmgr
cairo_compmgrdoc_DATA = \
	README\
	COPYING\
	AUTHORS\
	ChangeLog\
	INSTALL\
	NEWS

EXTRA_DIST = $(cairo_compmgrdoc_DATA)\
			 intltool-extract.in \
			 intltool-merge.in \
			 intltool-update.in \
			 gtk-doc.make \
			 plugin.make \
			 dolt.m4 \
			 shave.m4 \
			 shave.in \
			 shave-libtool.in \
             debian/changelog \
             debian/compat \
             debian/control \
             debian/copyright \
             debian/rules \
             debian/cairo-compmgr.install \
             debian/cairo-compmgr-plugins.install \
             debian/cairo-compmgr-dev.install \
             debian/cairo-compmgr-vala.install \
             debian/cairo-compmgr-doc.install \
             debian/libcairo-compmgr0.install \
             debian/libcairo-compmgr-dev.install
             
DISTCLEANFILES = intltool-extract \
				 intltool-merge \
				 intltool-update \
				 doltlibtool \
				 doltcompile \
    			 shave.in \
    			 shave-libtool.in

DISTCHECK_CONFIGURE_FLAGS = --enable-perf-plugin --enable-glitz --enable-gobject-introspection --enable-gtk-doc

# Copy all the spec files. Of cource, only one is actually used.
dist-hook:
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done

