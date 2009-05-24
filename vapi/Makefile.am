vapidir = $(datadir)/vala/vapi

if HAVE_VALA
vapi_DATA = \
	cairo-compmgr.deps \
	cairo-compmgr.vapi \
	math.vapi

EXTRA_DIST = $(vapi_DATA)
else
noinst_DATA = \
	cairo-compmgr.deps \
	cairo-compmgr.vapi \
	math.vapi

EXTRA_DIST = $(noinst_DATA)
endif
