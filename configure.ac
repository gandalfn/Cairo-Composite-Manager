m4_define(ccm_version_major, 0)
m4_define(ccm_version_minor, 3)
m4_define(ccm_version_micro, 0)
AC_INIT([cairo-compmgr],[ccm_version_major.ccm_version_minor.ccm_version_micro], [http://cairo-compmgr.tuxfamily.org/bugs/], [cairo-compmgr], [http://cairo-compmgr.tuxfamily.org])
AC_PREREQ(2.63)

dnl ****************************************************************************
dnl Define default build directory
dnl ****************************************************************************
AC_CONFIG_SRCDIR(src/cairo-compmgr.c)
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build])
AC_CONFIG_HEADERS(build/config.h)

dnl ****************************************************************************
dnl Init automake
dnl ****************************************************************************
AM_INIT_AUTOMAKE([1.11 dist-bzip2 silent-rules])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])
AC_ARG_VAR([ACLOCAL_FLAGS], [aclocal flags, e.g. -I <macro dir>])
AM_MAINTAINER_MODE

dnl ****************************************************************************
dnl define version
dnl ****************************************************************************
VERSION="ccm_version_major.ccm_version_minor.ccm_version_micro"
AC_SUBST(VERSION)
LIBS_VERSION="`expr ccm_version_major + ccm_version_micro`:ccm_version_minor:ccm_version_micro"
AC_SUBST(LIBS_VERSION)

dnl ***************************************************************************
dnl Internationalization
dnl ***************************************************************************
GETTEXT_PACKAGE=cairo-compmgr
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [The gettext translation domain])
IT_PROG_INTLTOOL([0.40.0])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.17])

dnl ****************************************************************************
dnl Packages version required
dnl ****************************************************************************
GTK_REQUIRED=2.16.0
CAIRO_REQUIRED=1.8.0
PIXMAN_REQUIRED=0.16.0
CAIRO_XLIB_REQUIRED=1.8.0
CAIRO_XLIB_XRENDER_REQUIRED=1.8.0
GOBJECT_INTROSPECTION_REQUIRED=0.6.2
GLADEUI_REQUIRED=3.6.7
VALA_REQUIRED=0.7.10

dnl ****************************************************************************
dnl Check for build
dnl ****************************************************************************
AC_LANG([C])
AM_PROG_VALAC($VALA_REQUIRED)
AC_SEARCH_LIBS([strerror],[cposix])

dnl ****************************************************************************
dnl Check for profiler flags
dnl ****************************************************************************
AC_ARG_ENABLE(profile,
  [  --disable-profile        Enable profile],
  [use_profile=$enableval], [use_profile=no])
if test "x$use_profile" = "xyes"; then
    CFLAGS="$CFLAGS -pg"
fi

dnl ****************************************************************************
dnl Check for libtool
dnl ****************************************************************************
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

dnl ****************************************************************************
dnl Check for packages
dnl ****************************************************************************
PKG_CHECK_MODULES(CAIRO_COMPMGR, 
       [xcomposite,
        xdamage,
        xext,
        xi,
        sm,
        cairo >= $CAIRO_REQUIRED,
        pixman-1 >= $PIXMAN_REQUIRED,
        gtk+-2.0 >= $GTK_REQUIRED
        vala-1.0 >= $VALA_REQUIRED])
AC_SUBST(CAIRO_COMPMGR_CFLAGS)
AC_SUBST(CAIRO_COMPMGR_LIBS)

dnl ***************************************************************************
dnl check for bfd
dnl ***************************************************************************
AC_CHECK_LIB(bfd, main, [BFD_LIBS="-lbfd"], [])
AC_CHECK_LIB(iberty, main, [IBERTY_LIBS="-liberty"], [])
EDEBUG_LIBS="$BFD_LIBS $IBERTY_LIBS"
AC_SUBST(EDEBUG_LIBS)
if test "$EDEBUG_LIBS" = "-lbfd -liberty"; then
    AC_DEFINE(HAVE_EDEBUG, 1, [Enhanced debug enabled])
fi

dnl ****************************************************************************
dnl Check for XInput2
dnl *****************************************************************************
LIBS="$CAIRO_COMPMGR_LIBS $LIBS"
AC_CHECK_FUNC(XSetClientPointer,AC_DEFINE(HAVE_XI2,1,[XInput Version 2 Enabled]))
AM_CONDITIONAL(HAVE_XI2, [test "x$ac_cv_func_XSetClientPointer" = "xyes"] )
AC_SUBST(HAVE_XI2)

dnl ****************************************************************************
dnl Check for gtk-doc and docbook
dnl ****************************************************************************
GTK_DOC_CHECK([1.6])

dnl ****************************************************************************
dnl Check for gconf
dnl ****************************************************************************
AC_ARG_ENABLE(gconf,
  [  --enable-gconf        Enable GConf backend],
  [gconf=$enableval], [gconf=no])

if test x"$gconf" = xyes; then
    PKG_CHECK_MODULES(CCM_GCONF, [gconf-2.0], [gconf="yes"], [gconf="no"])
    AC_PATH_PROG(GCONFTOOL, gconftool-2, no)

    if test x"$GCONFTOOL" = xno; then
      AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
    fi
    AM_GCONF_SOURCE_2
    AC_SUBST(CCM_GCONF_CFLAGS)
    AC_SUBST(CCM_GCONF_LIBS)
else
    AM_CONDITIONAL(GCONF_SCHEMAS_INSTALL, [test x$gconf = xyes])
fi
AM_CONDITIONAL(ENABLE_GCONF, [test x$gconf = xyes])

dnl ****************************************************************************
dnl Check for gobject introspection
dnl ****************************************************************************
AC_ARG_ENABLE(gobject-introspection,
  [  --enable-gobject-introspection        Enable GObject introspection],
  [gir=$enableval], [gir=no])

if [ test x$gir = xyes ]; then
    PKG_CHECK_MODULES(CCM_GOBJECT_INTROSPECTION, 
           [gobject-introspection-1.0 >= $GOBJECT_INTROSPECTION_REQUIRED],
           [gir="yes"],
           [gir="no"])
    AC_SUBST(CCM_GOBJECT_INTROSPECTION_CFLAGS)
    AC_SUBST(CCM_GOBJECT_INTROSPECTION_LIBS)
fi

if [ test x$gir = xyes ]; then    
    G_IR_SCANNER=`$PKG_CONFIG --variable=g_ir_scanner gobject-introspection-1.0`
    AC_SUBST(G_IR_SCANNER)
    G_IR_COMPILER=`$PKG_CONFIG --variable=g_ir_compiler gobject-introspection-1.0`
    AC_SUBST(G_IR_COMPILER)
    G_IR_GENERATE=`$PKG_CONFIG --variable=g_ir_generate gobject-introspection-1.0`
    AC_SUBST(G_IR_GENERATE)
    GIRDIR=`$PKG_CONFIG --variable=girdir gobject-introspection-1.0`
    AC_SUBST(GIRDIR)
    TYPELIBDIR="$($PKG_CONFIG --variable=typelibdir gobject-introspection-1.0)"
    AC_SUBST(TYPELIBDIR)
    if test "$prefix" = NONE
    then
        GIRINSTDIR=`$PKG_CONFIG --define-variable=prefix=$prefix --variable=girdir gobject-introspection-1.0`
        AC_SUBST(GIRINSTDIR)
        TYPELIBINSTDIR="$($PKG_CONFIG --define-variable=prefix=$prefix --variable=typelibdir gobject-introspection-1.0)"
        AC_SUBST(TYPELIBINSTDIR)
    fi
fi

AM_CONDITIONAL(ENABLE_GOBJECT_INTROSPECTION, [test x$gir = xyes])

dnl ****************************************************************************
dnl Check glade3 
dnl ****************************************************************************
PKG_CHECK_MODULES(CCM_GLADE, 
           [gladeui-1.0 >= $GLADEUI_REQUIRED],
           [gladeui="yes"],
           [gladeui="no"])

if [ test x$gladeui = xyes ]; then
    if test "$prefix" = NONE
    then
        glademoduledir=`$PKG_CONFIG --variable=moduledir gladeui-1.0`
        gladecatalogdir=`$PKG_CONFIG --variable=catalogdir gladeui-1.0`
    else
        glademoduledir=`$PKG_CONFIG --define-variable=prefix=$prefix --variable=moduledir gladeui-1.0`
        gladecatalogdir=`$PKG_CONFIG --define-variable=prefix=$prefix --variable=catalogdir gladeui-1.0`
    fi
    
    AC_SUBST(glademoduledir)
    AC_SUBST(gladecatalogdir)
    AC_SUBST(CCM_GLADE_CFLAGS)
    AC_SUBST(CCM_GLADE_LIBS)
fi

AM_CONDITIONAL(ENABLE_GLADE, [test x$gladeui = xyes])

dnl ****************************************************************************
dnl Check for build perf plugin
dnl ****************************************************************************
AC_ARG_ENABLE(perf-plugin,
  [  --enable-perf-plugin        Enable perf plugin],
  [perfplugin=$enableval], [perfplugin=no])

if [ test x$perfplugin = xyes ]; then
    PKG_CHECK_MODULES(CCM_PERF_PLUGIN, 
           [libgtop-2.0,
            libwnck-1.0],
           [perfplugin="yes"],
           [perfplugin="no"])
    AC_SUBST(CCM_PERF_PLUGIN_CFLAGS)
    AC_SUBST(CCM_PERF_PLUGIN_LIBS)
fi
AM_CONDITIONAL(ENABLE_PERF_PLUGIN, [test x$perfplugin = xyes])

dnl ****************************************************************************
dnl Check for xrender backend
dnl ****************************************************************************
PKG_CHECK_MODULES(CCM_XRENDER_BACKEND,
        [xrender,
         cairo-xlib >= $CAIRO_XLIB_REQUIRED,
         cairo-xlib-xrender >= $CAIRO_XLIB_XRENDER_REQUIRED],
         [xrenderbackend="yes"],
         [xrenderbackend="no"])
AC_SUBST(CCM_XRENDER_BACKEND_CFLAGS)
AC_SUBST(CCM_XRENDER_BACKEND_LIBS)

AM_CONDITIONAL(HAVE_XRENDER_BACKEND, [test x$xrenderbackend = xyes])

AC_CONFIG_FILES([
Makefile
lib/Makefile
src/Makefile
tools/Makefile
gir/Makefile
vapi/Makefile
po/Makefile.in
doc/Makefile
doc/reference/Makefile
doc/reference/version.xml
plugins/Makefile
plugins/shadow/Makefile
plugins/shadow/ccm-shadow.plugin.desktop
plugins/opacity/Makefile
plugins/opacity/ccm-opacity.plugin.desktop
plugins/fade/Makefile
plugins/fade/ccm-fade.plugin.desktop
plugins/freeze/Makefile
plugins/freeze/ccm-freeze.plugin.desktop
plugins/menu-animation/Makefile
plugins/menu-animation/ccm-menu-animation.plugin.desktop
plugins/window-animation/Makefile
plugins/window-animation/ccm-window-animation.plugin.desktop
plugins/magnifier/Makefile
plugins/magnifier/ccm-magnifier.plugin.desktop
plugins/mosaic/Makefile
plugins/mosaic/ccm-mosaic.plugin.desktop
plugins/perf/Makefile
plugins/perf/ccm-perf.plugin.desktop
plugins/decoration/Makefile
plugins/decoration/ccm-decoration.plugin.desktop
plugins/snapshot/Makefile
plugins/snapshot/ccm-snapshot.plugin.desktop
plugins/vala-test/Makefile
plugins/vala-test/ccm-vala-window-plugin.plugin.desktop
plugins/automate/Makefile
plugins/automate/ccm-automate.plugin.desktop
plugins/clone/Makefile
plugins/clone/ccm-clone.plugin.desktop
test/Makefile
data/Makefile
data/cairo-compmgr.desktop.in
data/cairo-compmgr.pc
])

AC_OUTPUT

echo " "
echo "-------------------------------------------------------------------"
echo "Conditionally built backends:"
echo "-------------------------------------------------------------------"
if [ test x$gir = xyes ]; then
        echo "Enable GObject introspection: ...............YES"
else
        echo "Enable GObject introspection: ...............NO"
fi

if [ test x$gconf = xyes ]; then
        echo "Enable GConf backend : ......................YES"
else
        echo "Enable GConf backend: .......................NO"
fi


if [ test x$perfplugin = xyes ]; then
        echo "Building perf plugin: .......................YES"
else
        echo "Building perf plugin: .......................NO"
fi

if [ test x$xrenderbackend = xyes ]; then
        echo "Building XRender backend: ...................YES"
else
        echo "Building XRender backend: ...................NO"
fi
