AC_INIT(cairo-compmgr, 0.1.1)

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

IT_PROG_INTLTOOL
AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC

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
CAIRO_XLIB_REQUIRED=1.4.10
CAIRO_XLIB_XRENDER_REQUIRED=1.4.10
CAIRO_GLITZ_REQUIRED=1.4.10
GLITZ_REQUIRED=0.5.6
GLITZ_GLX_REQUIRED=0.5.6

PKG_CHECK_MODULES(CAIRO_COMPMGR, 
       [xcomposite,
        xdamage,
        xext,
        cairo >= $CAIRO_REQUIRED,
        gtk+-2.0 >= $GTK_REQUIRED,
        libgnomeui-2.0,
        gconf-2.0])
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

PKG_CHECK_MODULES(CCM_GLITZ_BACKEND,
 		[glitz >= $GLITZ_REQUIRED,
		 glitz-glx >= $GLITZ_GLX_REQUIRED,
		 cairo-glitz >= $CAIRO_GLITZ_REQUIRED],
		 [glitzbackend="yes"],
		 [glitzbackend="no"])
AC_SUBST(CCM_GLITZ_BACKEND_CFLAGS)
AC_SUBST(CCM_GLITZ_BACKEND_LIBS)

AM_CONDITIONAL(HAVE_GLITZ_BACKEND, [test x$glitzbackend = xyes])

AC_OUTPUT([
Makefile
src/Makefile
po/Makefile.in
plugins/Makefile
plugins/shadow/Makefile
plugins/opacity/Makefile
plugins/fade/Makefile
data/Makefile
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

if [ test x$glitzbackend = xyes ]; then
        echo "Building Glitz backend: .....................YES"
else
        echo "Building Glitz backend: .....................NO"
fi
