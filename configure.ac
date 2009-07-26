m4_define(ccm_version_major, 0)
m4_define(ccm_version_minor, 2)
m4_define(ccm_version_micro, 96)
AC_INIT(cairo-compmgr, ccm_version_major.ccm_version_minor.ccm_version_micro)

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

dnl ****************************************************************************
dnl Packages version required
dnl ****************************************************************************
GTK_REQUIRED=2.10.0
CAIRO_REQUIRED=1.8.0
PIXMAN_REQUIRED=0.12.0
CAIRO_XLIB_REQUIRED=1.8.0
CAIRO_XLIB_XRENDER_REQUIRED=1.8.0
CAIRO_GLITZ_REQUIRED=1.8.0
CAIRO_OPENVG_REQUIRED=1.5.7
GLITZ_REQUIRED=0.5.7
GLITZ_GLX_REQUIRED=0.5.7
GL_PROTO_REQUIRED=1.4.9
VALA_REQUIRED=0.7.2
GOBJECT_INTROSPECTION_REQUIRED=0.6.2
GLADEUI_REQUIRED=3.5.7

dnl ****************************************************************************
dnl Check for debug flags
dnl ****************************************************************************
AC_ARG_ENABLE(debug,
  [  --disable-debug        Disable debug],
  [use_debug=$enableval], [use_debug=yes])

dnl ****************************************************************************
dnl Check for profiler flags
dnl ****************************************************************************
AC_ARG_ENABLE(profile,
  [  --disable-profile        Enable profile],
  [use_profile=$enableval], [use_profile=no])

dnl ****************************************************************************
dnl Check for build
dnl ****************************************************************************
IT_PROG_INTLTOOL
AC_ISC_POSIX
AC_PROG_CC
if test "x$GCC" = "xyes" ; then
    if test "x$use_debug" = "xno"; then
        CFLAGS="-O2 -Wall -fPIC -fomit-frame-pointer -ffast-math -fno-strict-aliasing"
    else
        CFLAGS="-g -O2 -Wall -fPIC"
    fi
    if test "x$use_profile" = "xyes"; then
        CFLAGS="$CFLAGS -pg"
    fi
fi
AM_PROG_CC_STDC
AC_HEADER_STDC

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
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [GETTEXT package name])
AM_GLIB_GNU_GETTEXT

AM_PROG_LIBTOOL
DOLT

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
        gtk+-2.0 >= $GTK_REQUIRED])
CAIRO_COMPMGR_CFLAGS="$CAIRO_COMPMGR_CFLAGS -DG_DISABLE_ASSERT -DG_DISABLE_CHECKS -DG_DISABLE_CAST_CHECKS"
AC_SUBST(CAIRO_COMPMGR_CFLAGS)
AC_SUBST(CAIRO_COMPMGR_LIBS)

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
PKG_CHECK_MODULES(CCM_GCONF, [gconf-2.0], [gconf="yes"], [gconf="no"])
if test x"$gconf" = xyes; then
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
dnl Check for vala
dnl ****************************************************************************
PKG_CHECK_MODULES(CAIRO_COMPMGR_VALA, [vala-1.0 >= $VALA_REQUIRED],
                      [vala="yes"], [vala="no"])
if test x"$vala" = xyes; then
    AC_PATH_PROG(VALAC, valac, no)
    if test x"$VALAC" = xno; then
        AC_MSG_ERROR([valac executable not found in your path - should be installed with vala])
    fi
    AC_SUBST(VALAC)
    CAIRO_COMPMGR_CFLAGS="$CAIRO_COMPMGR_CFLAGS $CAIRO_COMPMGR_VALA_CFLAGS"
    CAIRO_COMPMGR_LIBS="$CAIRO_COMPMGR_CFLAGS $CAIRO_COMPMGR_VALA_LIBS"
fi
AM_CONDITIONAL(HAVE_VALA, [test x$vala = xyes])

dnl ****************************************************************************
dnl Check gobject introspection
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

dnl ****************************************************************************
dnl Check for openvg backend
dnl ****************************************************************************
PKG_CHECK_MODULES(CCM_OPENVG_BACKEND,
 		[xrender,
		 cairo-openvg >= $CAIRO_OPENVG_REQUIRED,
 		 glproto >= $GL_PROTO_REQUIRED],
		 [openvgbackend="yes"],
		 [openvgbackend="no"])
AC_SUBST(CCM_OPENVG_BACKEND_CFLAGS)
AC_SUBST(CCM_OPENVG_BACKEND_LIBS)

AM_CONDITIONAL(HAVE_OPENVG_BACKEND, [test x$openvgbackend = xyes])

dnl ****************************************************************************
dnl Check for glitz backend
dnl ****************************************************************************
AC_ARG_ENABLE(glitz,
  [  --enable-glitz        Enable glitz],
  [glitzbackend=$enableval], [glitzbackend=no])

if [ test x$glitzbackend = xyes ]; then
    PKG_CHECK_MODULES(CCM_GLITZ_BACKEND,
     		[glitz >= $GLITZ_REQUIRED,
		     glitz-glx >= $GLITZ_GLX_REQUIRED,
		     cairo-glitz >= $CAIRO_GLITZ_REQUIRED],
		     [glitzbackend="yes"],
		     [glitzbackend="no"])
    AC_SUBST(CCM_GLITZ_BACKEND_CFLAGS)
    AC_SUBST(CCM_GLITZ_BACKEND_LIBS)
fi

AM_CONDITIONAL(HAVE_GLITZ_BACKEND, [test x$glitzbackend = xyes])

dnl ****************************************************************************
dnl Check for glitz tfp backend
dnl ****************************************************************************
if [ test x$glitzbackend = xyes ]; then
	AC_ARG_ENABLE(glitz-tfp,
	  [  --disable-glitz-tfp        Disable glitz tfp],
	  [glitztfpbackend=$enableval], [glitztfpbackend=yes])

    if [ test x$glitztfpbackend = xyes ]; then
    	cflags_save=$CFLAGS 
    	libs_save=$LIBS 
	CFLAGS="$CFLAGS $CCM_GLITZ_BACKEND_CFLAGS" 
	LIBS="$LIBS $CCM_GLITZ_BACKEND_LIBS" 

	AC_CHECK_FUNC(glitz_glx_create_drawable_for_pixmap, 
        	          [glitztfpbackend="yes"],
	                  [glitztfpbackend="no"])
	CFLAGS=$cflags_save 
	LIBS=$libs_save 
    fi
fi                  
AM_CONDITIONAL(HAVE_GLITZ_TFP_BACKEND, [test x$glitztfpbackend = xyes])

SHAVE_INIT([.],[enable])

AC_OUTPUT([
shave
shave-libtool
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
echo " "
echo "-------------------------------------------------------------------"
echo "Conditionally built backends:"
echo "-------------------------------------------------------------------"
if [ test x$gir = xyes ]; then
        echo "Enable GObject introspection: ...............YES"
else
        echo "Enable GObject introspection: ...............NO"
fi

if [ test x$vala = xyes ]; then
        echo "Enable Vala bindings:         ...............YES"
else
        echo "Enable Vala bindings:         ...............NO"
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

if [ test x$openvgbackend = xyes ]; then
        echo "Building OpenVG backend: .....................YES"
else
        echo "Building OpenVG backend: .....................NO"
fi

if [ test x$glitzbackend = xyes ]; then
        echo "Building Glitz backend: .....................YES"
else
        echo "Building Glitz backend: .....................NO"
fi

if [ test x$glitztfpbackend = xyes ]; then
        echo "Building Glitz TFP backend: .................YES"
else
        echo "Building Glitz TFP backend: .................NO"
fi
