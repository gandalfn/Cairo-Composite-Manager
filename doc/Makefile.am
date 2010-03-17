SUBDIRS=reference

.PHONY: doc

doc:
	cd reference && $(MAKE) $(AM_MAKEFLAGS) doc

man_MANS = \
	cairo-compmgr.1

EXTRA_DIST = $(man_MANS)