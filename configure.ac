dnl Process this file with autoconf to produce a configure script.
dnl Created by Anjuta application wizard.

AC_INIT(cairo-compmgr, 0.1.0)

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_PROG_INTLTOOL
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

PKG_CHECK_MODULES(CAIRO_COMPMGR, [gtk+-2.0 cairo xcomposite xrender xdamage glitz cairo-glitz glitz-glx xext libgnomeui-2.0 gconf-2.0])
AC_SUBST(CAIRO_COMPMGR_CFLAGS)
AC_SUBST(CAIRO_COMPMGR_LIBS)




AC_OUTPUT([
Makefile
src/Makefile
po/Makefile.in
plugins/Makefile
plugins/shadow/Makefile
plugins/opacity/Makefile
data/Makefile
])
