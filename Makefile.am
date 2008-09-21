SUBDIRS = lib src plugins data po

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
			 intltool-update.in

DISTCLEANFILES = intltool-extract \
				 intltool-merge \
				 intltool-update

# Copy all the spec files. Of cource, only one is actually used.
dist-hook:
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done

