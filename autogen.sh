#! /bin/sh -e
test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

autoreconf --force --install --verbose "$srcdir"
intltoolize --copy --force --automake
test -n "$NOCONFIGURE" || "$srcdir/configure" --enable-maintainer-mode "$@"