dnl Process this file with autoconf to produce a configure script.

AC_INIT(configure.ac)
AM_INIT_AUTOMAKE(eek, 0.1)
AM_CONFIG_HEADER(config.h)

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC

dnl Check for pkg-config dependencies
PKG_CHECK_MODULES(GLADE, libglade-2.0)
PKG_CHECK_MODULES(GTK, gtk+-2.0 >= 2.6.0)
PKG_CHECK_MODULES(GCONF, gconf-2.0)
PKG_CHECK_MODULES(GLIB, glib-2.0)

dnl Check for zlib
have_zlib=yes;
AC_CHECK_HEADER(zlib.h, , have_zlib=no)
AC_CHECK_LIB(z, inflateEnd, , have_zlib=no)
if test "x$have_zlib" = "xno"; then
   AC_MSG_WARN(Compiling without zlib support)
   AC_MSG_WARN(You will not be able to load compressed UEF files)
else
   AC_DEFINE(HAVE_ZLIB, 1, [Defined if zlib is available])
fi;

dnl Set PACKAGE_SOURCE_DIR in config.h.
packagesrcdir=`cd $srcdir && pwd`
AC_DEFINE_UNQUOTED(PACKAGE_SOURCE_DIR, "${packagesrcdir}",
                                       [Define to the source directory])

dnl GConf settings
AC_PATH_PROG(GCONFTOOL, gconftool-2)
AM_GCONF_SOURCE_2

dnl Use -Wall if we have gcc.
changequote(,)dnl
if test "x$GCC" = "xyes"; then
  case " $CFLAGS " in
  *[\ \	]-Wall[\ \	]*) ;;
  *) CFLAGS="$CFLAGS -Wall" ;;
  esac
fi
changequote([,])dnl

AC_OUTPUT([
Makefile
src/Makefile
glade/Makefile
gconf/Makefile
])
