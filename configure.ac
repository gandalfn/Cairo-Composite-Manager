m4_define(ccm_version_major, 0)
m4_define(ccm_version_minor, 2)
m4_define(ccm_version_micro, 3)
AC_INIT(cairo-compmgr, ccm_version_major.ccm_version_minor.ccm_version_micro)

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE
dnl Enable debug
AC_ARG_ENABLE(debug,
  [  --disable-debug        Disable debug],
  [use_debug=$enableval], [use_debug=yes])

IT_PROG_INTLTOOL
AC_ISC_POSIX
AC_PROG_CC
if test "x$GCC" = "xyes" ; then
    if test "x$use_debug" = "xno"; then
        CFLAGS="-O2 -Wall -fPIC -fomit-frame-pointer -ffast-math -fno-strict-aliasing"
    else
        CFLAGS="-g -O2 -Wall -fPIC"
    fi
fi
AM_PROG_CC_STDC
AC_HEADER_STDC
dnl AC_PROG_CC_C99

LIBS_VERSION="`expr ccm_version_major + ccm_version_micro`:ccm_version_minor:ccm_version_micro"
AC_SUBST(LIBS_VERSION)

dnl ***************************************************************************
dnl Internatinalization
dnl ***************************************************************************
GETTEXT_PACKAGE=cairo-compmgr
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [GETTEXT package name])
AM_GLIB_GNU_GETTEXT

AM_PROG_LIBTOOL

dnl gconf checks
AC_PATH_PROG(GCONFTOOL, gconftool-2, no)

if test x"$GCONFTOOL" = xno; then
  AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
fi

AM_GCONF_SOURCE_2

GTK_REQUIRED=2.10.0
CAIRO_REQUIRED=1.4.10
PIXMAN_REQUIRED=0.9.6
CAIRO_XLIB_REQUIRED=1.4.10
CAIRO_XLIB_XRENDER_REQUIRED=1.4.10
CAIRO_GLITZ_REQUIRED=1.4.10
CAIRO_OPENVG_REQUIRED=1.5.7
GLITZ_REQUIRED=0.5.6
GLITZ_GLX_REQUIRED=0.5.6
GL_PROTO_REQUIRED=1.4.9

PKG_CHECK_MODULES(CAIRO_COMPMGR, 
       [xcomposite,
        xdamage,
        xext,
        cairo >= $CAIRO_REQUIRED,
        pixman-1 >= $PIXMAN_REQUIRED,
        gtk+-2.0 >= $GTK_REQUIRED,
        libgnomeui-2.0,
        gconf-2.0,
        libgtop-2.0])
AC_SUBST(CAIRO_COMPMGR_CFLAGS)
AC_SUBST(CAIRO_COMPMGR_LIBS)

PKG_CHECK_MODULES(CCM_XRENDER_BACKEND,
 		[xrender,
		 cairo-xlib >= $CAIRO_XLIB_REQUIRED,
		 cairo-xlib-xrender >= $CAIRO_XLIB_XRENDER_REQUIRED],
		 [xrenderbackend="yes"],
		 [xrenderbackend="no"])
AC_SUBST(CCM_XRENDER_BACKEND_CFLAGS)
AC_SUBST(CCM_XRENDER_BACKEND_LIBS)

AM_CONDITIONAL(HAVE_XRENDER_BACKEND, [test x$xrenderbackend = xyes])

PKG_CHECK_MODULES(CCM_OPENVG_BACKEND,
 		[xrender,
		 cairo-openvg >= $CAIRO_OPENVG_REQUIRED,
         glproto >= $GL_PROTO_REQUIRED],
		 [openvgbackend="yes"],
		 [openvgbackend="no"])
AC_SUBST(CCM_OPENVG_BACKEND_CFLAGS)
AC_SUBST(CCM_OPENVG_BACKEND_LIBS)

AM_CONDITIONAL(HAVE_OPENVG_BACKEND, [test x$openvgbackend = xyes])

PKG_CHECK_MODULES(CCM_GLITZ_BACKEND,
 		[glitz >= $GLITZ_REQUIRED,
		 glitz-glx >= $GLITZ_GLX_REQUIRED,
		 cairo-glitz >= $CAIRO_GLITZ_REQUIRED],
		 [glitzbackend="yes"],
		 [glitzbackend="no"])
AC_SUBST(CCM_GLITZ_BACKEND_CFLAGS)
AC_SUBST(CCM_GLITZ_BACKEND_LIBS)

AM_CONDITIONAL(HAVE_GLITZ_BACKEND, [test x$glitzbackend = xyes])

AC_ARG_ENABLE(glitz-tfp,
  [  --disable-glitz-tfp        Disable glitz tfp],
  [glitztfpbackend=$enableval], [glitztfpbackend=yes])

if [ test x$glitzbackend = xyes ]; then
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

AC_CHECK_LIB(m, round, [M_LIBS="-lm"], AC_MSG_ERROR([libm is required to
build cairo-compmgr. This lib comes with GNU C libraries (glibc) and you can
find the lastest version from http://www.gnu.org/software/libc/]))
AC_SUBST(M_LIBS)


AC_OUTPUT([
Makefile
lib/Makefile
src/Makefile
po/Makefile.in
plugins/Makefile
plugins/shadow/Makefile
plugins/opacity/Makefile
plugins/fade/Makefile
plugins/freeze/Makefile
plugins/menu-animation/Makefile
plugins/magnifier/Makefile
plugins/mosaic/Makefile
plugins/perf/Makefile
data/Makefile
data/cairo-compmgr.desktop.in
])
echo " "
echo "-------------------------------------------------------------------"
echo "Conditionally built backends:"
echo "-------------------------------------------------------------------"
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
