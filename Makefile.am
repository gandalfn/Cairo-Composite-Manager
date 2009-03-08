SUBDIRS = lib src tools plugins doc gir vapi data po

doc: all
	cd doc && $(MAKE) $(AM_MAKEFLAGS) doc
	
deb-source: changelog dist 
	mkdir -p packages
	cd packages; \
	rm -rf $(VERSION)-source; \
	mkdir $(VERSION)-source; \
	cd $(VERSION)-source; \
	tar zxvf ../../cairo-compmgr-$(VERSION).tar.gz; \
	cd cairo-compmgr-$(VERSION); \
	echo y | debuild -S -sa -kDE69A0FB && \
	cd .. && \
	rm -rf cairo-compmgr-$(VERSION)

deb: changelog dist
	mkdir -p packages
	cd packages; \
	rm -rf $(VERSION); \
	mkdir $(VERSION); \
	cd $(VERSION); \
	tar zxvf ../../cairo-compmgr-$(VERSION).tar.gz; \
	cd cairo-compmgr-$(VERSION); \
	echo y | debuild -sa && \
	cd .. && \
	rm -rf cairo-compmgr-$(VERSION) && \
	rm -f *$(VERSION)*source* && \
	rm -f *$(VERSION)*.build && \
	rm -f *$(VERSION)*.changes
	
changelog:
	if [ ! "$(shell dpkg-parsechangelog | grep ^Version | sed 's/.*: //; s/-.*//;')" = "$(VERSION)" ]; \
	then \
	    debchange --distribution $(shell lsb_release -c | awk '{print $$2}') -m -v "$(VERSION)-0ubuntu1~ppa1" "New upstream release."; \
	    grep -v "^=" NEWS | awk 'BEGIN {show=0}{if ($$0 != ""){ if ($$1 == "Overview"){show++;} else if (show == 1) {gsub("* ", ""); cmd = sprintf("debchange \"%s\"", $$0); system(cmd);}}}'; \
	fi	    

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
			 gitlog-to-changelog \
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

dist-hook: gen-ChangeLog
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done

.PHONY: gen-ChangeLog
gen-ChangeLog:
	if test -d .git; then						           \
	  $(top_srcdir)/gitlog-to-changelog > $(distdir)/cl-t; \
	  rm -f $(distdir)/ChangeLog;					       \
	  mv $(distdir)/cl-t $(distdir)/ChangeLog;		       \
	fi
