dnl **********************************************************************
dnl *
dnl * pg_rest - Rest api for PostgreSQL
dnl * https://github.com/robertmu/pg_rest
dnl * Copyright 2015 Robert Mu
dnl *
dnl * This is free software; you can redistribute and/or modify it under
dnl * the terms of the GNU General Public Licence. See the COPYING file.
dnl *
dnl **********************************************************************

AC_INIT()
AC_COPYRIGHT([Copyright (c) 2015-2016, Robert Mu])
AC_CONFIG_HEADERS([include/pg_rest_header.h jansson/jansson_private_config.h])
AC_CONFIG_MACRO_DIR([macros])


dnl Invoke libtool: we do this as it is the easiest way to find the PIC
dnl flags required to build pg_rest
AC_PROG_LIBTOOL

dnl
dnl Compilers
dnl
AC_PROG_CC
AC_PROG_CPP
AC_PROG_GREP
AM_CONDITIONAL([GCC], [test x$GCC = xyes])

dnl
dnl Define PIC flags in PICFLAGS (note: this variable is set as part of libtool initialisation above)
dnl
PICFLAGS="$lt_prog_compiler_pic"
AC_SUBST([PICFLAGS])

dnl
dnl For GCC enable additional warning flags -Wall and -Wmissing-prototypes (using macro included with libtool)
dnl
WARNFLAGS=""
AC_LIBTOOL_COMPILER_OPTION([if $compiler supports -Wall], [_cv_wall], [-Wall], [], [WARNFLAGS="$WARNFLAGS -Wall"], []) 
AC_LIBTOOL_COMPILER_OPTION([if $compiler supports -Wmissing-prototypes], [_cv_misprot], [-Wmissing-prototypes], [], [WARNFLAGS="$WARNFLAGS -Wmissing-prototypes"], []) 
AC_SUBST([WARNFLAGS])

dnl
dnl Version Information imported from META.json 
dnl
PGREST_EXTENSION=`grep -m 1 '"name":' META.json | sed -e 's/[[[:space:]]]*"name":[[[:space:]]]*"\([[^"]]*\)",/\1/'`
PGREST_VERSION=`grep -m 1 '"version":' META.json | sed -e 's/[[[:space:]]]*"version":[[[:space:]]]*"\([[^"]]*\)",/\1/'`
AC_DEFINE_UNQUOTED([PGREST_VERSION], ["$PGREST_VERSION"], [pg_rest version])
AC_SUBST([PGREST_EXTENSION])
AC_SUBST([PGREST_VERSION])

AC_PATH_PROG([PERL], [perl], [])
if test "x$PERL" = "x"; then
        AC_MSG_ERROR([Perl was not found. Building pg_rest requires Perl.])
fi
AC_SUBST([PERL])

# OS X Lion started deprecating the system openssl. Let's just disable
# all deprecation warnings on OS X.
case "$host_os" in

 darwin*)
    CFLAGS="$CFLAGS -Wno-deprecated-declarations"
    ;;
esac

dnl ===========================================================================
dnl Detect libevent if it is installed 
dnl ===========================================================================

EVENT_CPPFLAGS=""
EVENT_LDFLAGS=""

AC_ARG_WITH([libevent], 
	[AS_HELP_STRING([--with-libevent=PATH], [specify a path to non-default libevent installation])], 
	[LIBEVENT_PATH="$withval"], [LIBEVENT_PATH=""])

LDFLAGS_SAVE="$LDFLAGS"
CFLAGS_SAVE="$CFLAGS"

if test "x$LIBEVENT_PATH" != "x"; then
	AC_MSG_RESULT([checking user-specified libevent location: $LIBEVENT_PATH])
	EVENT_CPPFLAGS="-I$LIBEVENT_PATH/include"
	EVENT_LDFLAGS="-L$LIBEVENT_PATH/lib"
	LDFLAGS="$EVENT_LDFLAGS $LDFLAGS"
	CFLAGS="$EVENT_CPPFLAGS $CFLAGS"
fi

HAVE_EVENT=no
AC_CHECK_HEADER([event.h], [HAVE_EVENT=yes], [])

dnl If we find the header file, try and link against the library
if test "x$HAVE_EVENT" = "xyes"; then
	dnl libevent defines event_base_new, so we'll check that directly
	AC_CHECK_LIB([event_core], [event_self_cbarg], [EVENT_LDFLAGS="$EVENT_LDFLAGS -levent_core" HAVE_LIBEVENT=1], [])
	if test "x$HAVE_LIBEVENT" = "x"; then
        dnl No event library was found; issue a warning to the console
		dnl AC_MSG_ERROR([Could not find libevent. Please install libevent and libevent-devel.])
		AC_MSG_ERROR([library 'libevent' (version >= 2.1.1) is required. Please upgrade your libevent])
	fi
else
	dnl No event header was found; issue a warning to the console
	AC_MSG_ERROR([Could not find event.h header. Please install libevent and libevent-devel (version >= 2.1.1 is required).])
fi

LDFLAGS="$LDFLAGS_SAVE"
CFLAGS="$CFLAGS_SAVE"

dnl Only define HAVE_LIBEVENT in pg_rest_header.h if we detect event sucessfully
if test "x$HAVE_LIBEVENT" != "x"; then
	AC_DEFINE_UNQUOTED([HAVE_LIBEVENT], [$HAVE_LIBEVENT], [Defined if libevent headers and library are present])
fi

AC_SUBST([EVENT_LDFLAGS])
AC_SUBST([EVENT_CPPFLAGS])
AC_SUBST([HAVE_EVENT])

dnl ===========================================================================
dnl Detect the version of PostgreSQL installed on the system, if needed
dnl ===========================================================================

AC_ARG_VAR(PG_CONFIG, [PostgreSQL configure command to determine Postgres version to build against.])
AC_ARG_WITH([pgconfig],
	[AS_HELP_STRING([--with-pgconfig=FILE], [specify an alternative pg_config file ])],
	[PG_CONFIG="$withval"], [])

if test "x$PG_CONFIG" = "x"; then
	dnl PG_CONFIG was not specified, so search within the current path
	AC_PATH_PROG([PG_CONFIG], [pg_config])

	dnl If we couldn't find pg_config, display an error
	if test "x$PG_CONFIG" = "x"; then
		AC_MSG_ERROR([could not find pg_config within the current path. You may need to re-run configure with a --with-pgconfig parameter.])
	fi
else
	dnl PG_CONFIG was specified; display a message to the user
	if test "x$PG_CONFIG" = "xyes"; then
		AC_MSG_ERROR([you must specify a parameter to --with-pgconfig, e.g. --with-pgconfig=/path/to/pg_config])
	else
		if test -f $PG_CONFIG; then
			AC_MSG_RESULT([Using user-specified pg_config file: $PG_CONFIG])
		else
			AC_MSG_ERROR([the user-specified pg_config file $PG_CONFIG does not exist])
		fi
	fi
fi

dnl ===========================================================================
dnl Ensure that $PG_CONFIG --pgxs points to a valid file. This is because some 
dnl distributions such as Debian also include pg_config as part of libpq-dev 
dnl packages, but don't install the Makefile it points to unless
dnl the postgresql-server-dev packages are installed :)
dnl ===========================================================================

PGXS=`$PG_CONFIG --pgxs`
if test ! -f $PGXS; then
  AC_MSG_ERROR([the PGXS Makefile $PGXS cannot be found. Please install the PostgreSQL server development packages and re-run configure.])
fi

AC_SUBST([PGXS])

dnl Extract the version information from pg_config
dnl Note: we extract the major & minor separately, ensure they are numeric, and then combine to give
dnl the final version. This is to guard against user error... 
PGSQL_MAJOR_VERSION=`$PG_CONFIG --version | sed 's/[[^0-9]]*\([[0-9]]\)\.\([[0-9]]\).*/\1/'`
PGSQL_MINOR_VERSION=`$PG_CONFIG --version | sed 's/[[^0-9]]*\([[0-9]]\)\.\([[0-9]]\).*/\2/'`
PGSQL_FULL_VERSION=`$PG_CONFIG --version`
PGSQL_VERSION="$PGSQL_MAJOR_VERSION$PGSQL_MINOR_VERSION"

PGSQL_PKGLIBDIR=`$PG_CONFIG --pkglibdir`
PGSQL_LIBDIR=`$PG_CONFIG --libdir`
PGSQL_SHAREDIR=`$PG_CONFIG --sharedir`

AC_MSG_RESULT([checking PostgreSQL version... $PGSQL_FULL_VERSION])
dnl Ensure that we are using PostgreSQL >= 9.3
if test $PGSQL_VERSION -lt 93; then
  AC_MSG_ERROR([pg_rest requires PostgreSQL >= 9.3])
fi

dnl Extract the include flags for the backend (libpgcommon)
PGSRV_INC=`$PG_CONFIG --includedir-server`
PGSQL_BE_CPPFLAGS="-I${PGSRV_INC}"
AC_SUBST([PGSQL_BE_CPPFLAGS])

dnl Extract the documentation and man page directories
PGSQL_DOCDIR=`$PG_CONFIG --docdir`
PGSQL_MANDIR=`$PG_CONFIG --mandir`

AC_SUBST([PGSQL_DOCDIR])
AC_SUBST([PGSQL_MANDIR])

dnl Extract the locale directory
PGSQL_LOCALEDIR=`$PG_CONFIG --localedir`
AC_DEFINE_UNQUOTED([PGSQL_LOCALEDIR], ["$PGSQL_LOCALEDIR"], [Location of PostgreSQL locale directory])

dnl Extract the executable directory
PGSQL_BINDIR=`$PG_CONFIG --bindir`

AC_SUBST([PGSQL_BINDIR])

dnl Extract the share directory
PGSQL_SHAREDIR=`$PG_CONFIG --sharedir`

AC_SUBST([PGSQL_SHAREDIR])

AC_DEFINE_UNQUOTED([PGSQL_VERSION], [$PGSQL_VERSION], [PostgreSQL server version])	
AC_SUBST([PGSQL_VERSION])

dnl ===========================================================================
dnl Display the configuration status information
dnl ===========================================================================

if test "$prefix" != "NONE"; then
  AC_MSG_RESULT([------------------------------------------------------------------------])
  AC_MSG_RESULT([  WARNING: You have set the --prefix to '$prefix'. But we mostly    ])
  AC_MSG_RESULT([  ignore the --prefix. For your info, using the values determined from ])
  AC_MSG_RESULT([  $PG_CONFIG we will be installing:   ])
  AC_MSG_RESULT([    * pg_rest shared library in $PGSQL_LIBDIR ])
  AC_MSG_RESULT([    * pg_rest SQL files in $PGSQL_SHAREDIR/extension ])
  AC_MSG_RESULT([    * pg_rest executables in $PGSQL_BINDIR ])
  AC_MSG_RESULT([------------------------------------------------------------------------])
fi


dnl ===========================================================================
dnl Detect libxml2 if it is installed
dnl ===========================================================================

HAVE_XML=no
AC_ARG_WITH([libxml], 
    [AS_HELP_STRING([--with-libxml], [build with XML support(default=no)])], 
    [WITH_LIBXML="$withval"], [WITH_LIBXML=""])

if test ! "x$WITH_LIBXML" = "x"; then 
    dnl XML2CONFIG was not specified, so search within the current path
    AC_PATH_PROG([XML2CONFIG], [xml2-config])

    dnl If we couldn't find xml2-config, display a warning
    if test "x$XML2CONFIG" = "x"; then 
        AC_MSG_ERROR([could not find xml2-config from libxml2 within the current path. Please install libxml2 and libxml2-devel.])
    fi

	AC_DEFINE([HAVE_LIBXML], 1, [Define to 1 if libxml2 is present])

    dnl Extract the linker and include flags 
    XML2_LDFLAGS=`$XML2CONFIG --libs`
    XML2_CPPFLAGS=`$XML2CONFIG --cflags`

    dnl Extract the version
    PGREST_LIBXML2_VERSION=`$XML2CONFIG --version`

    dnl Check headers file
    CPPFLAGS_SAVE="$CPPFLAGS"
    CPPFLAGS="$XML2_CPPFLAGS"
    AC_CHECK_HEADERS([libxml/tree.h libxml/parser.h libxml/xpath.h libxml/xpathInternals.h],
            [HAVE_XML=yes], [AC_MSG_ERROR([could not find headers include related to libxml2])])
    CPPFLAGS="$CPPFLAGS_SAVE"

    dnl Ensure we can link against libxml2
    LIBS_SAVE="$LIBS"
    LIBS="$XML2_LDFLAGS"
    AC_CHECK_LIB([xml2], [xmlInitParser], [HAVE_XML=yes], [AC_MSG_ERROR([could not find libxml2])], [])
    LIBS="$LIBS_SAVE"

    AC_DEFINE_UNQUOTED([PGREST_LIBXML2_VERSION], ["$PGREST_LIBXML2_VERSION"], [pg_rest libxml2 version])
    AC_SUBST([PGREST_LIBXML2_VERSION])
    AC_SUBST([HAVE_XML])
fi


dnl ===========================================================================
dnl setup jansson compile options
dnl ===========================================================================

# Checks for header files.
AC_CHECK_HEADERS([endian.h fcntl.h locale.h sched.h unistd.h sys/param.h sys/stat.h sys/time.h sys/types.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT32_T
AC_TYPE_UINT32_T
AC_TYPE_UINT16_T
AC_TYPE_UINT8_T
AC_TYPE_LONG_LONG_INT

AC_C_INLINE
case $ac_cv_c_inline in
    yes) json_inline=inline;;
    no) json_inline=;;
    *) json_inline=$ac_cv_c_inline;;
esac
AC_SUBST([json_inline])

# Checks for library functions.
AC_CHECK_FUNCS([close getpid gettimeofday localeconv open read sched_yield strtoll])

AC_MSG_CHECKING([for gcc __sync builtins])
have_sync_builtins=no
AC_TRY_LINK(
  [], [unsigned long val; __sync_bool_compare_and_swap(&val, 0, 1);],
  [have_sync_builtins=yes],
)
if test "x$have_sync_builtins" = "xyes"; then
  AC_DEFINE([HAVE_SYNC_BUILTINS], [1],
    [Define to 1 if gcc's __sync builtins are available])
fi
AC_MSG_RESULT([$have_sync_builtins])

AC_MSG_CHECKING([for gcc __atomic builtins])
have_atomic_builtins=no
AC_TRY_LINK(
  [], [char l; unsigned long v; __atomic_test_and_set(&l, __ATOMIC_RELAXED); __atomic_store_n(&v, 1, __ATOMIC_RELEASE); __atomic_load_n(&v, __ATOMIC_ACQUIRE);],
  [have_atomic_builtins=yes],
)
if test "x$have_atomic_builtins" = "xyes"; then
  AC_DEFINE([HAVE_ATOMIC_BUILTINS], [1],
    [Define to 1 if gcc's __atomic builtins are available])
fi
AC_MSG_RESULT([$have_atomic_builtins])

case "$ac_cv_type_long_long_int$ac_cv_func_strtoll" in
     yesyes) json_have_long_long=1;;
     *) json_have_long_long=0;;
esac
AC_SUBST([json_have_long_long])

case "$ac_cv_header_locale_h$ac_cv_func_localeconv" in
     yesyes) json_have_localeconv=1;;
     *) json_have_localeconv=0;;
esac
AC_SUBST([json_have_localeconv])

# Features
AC_ARG_ENABLE([urandom],
  [AS_HELP_STRING([--disable-urandom],
    [Don't use /dev/urandom to seed the hash function])],
  [use_urandom=$enableval], [use_urandom=yes])

if test "x$use_urandom" = xyes; then
AC_DEFINE([USE_URANDOM], [1],
  [Define to 1 if /dev/urandom should be used for seeding the hash function])
fi

AC_DEFINE([USE_WINDOWS_CRYPTOAPI], [1],
  [Define to 1 if CryptGenRandom should be used for seeding the hash function])

initial_hashtable_order=3
AC_DEFINE_UNQUOTED([INITIAL_HASHTABLE_ORDER], [$initial_hashtable_order],
  [Number of buckets new object hashtables contain is 2 raised to this power. E.g. 3 -> 2^3 = 8.])

AC_CACHE_CHECK([for printf format archetype], prac_cv_printf_archetype,
[ac_save_c_werror_flag=$ac_c_werror_flag
ac_c_werror_flag=yes
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[extern int
prac_write(int ignore, const char *fmt,...)
__attribute__((format(gnu_printf, 2, 3)));], [])],
                  [prac_cv_printf_archetype=gnu_printf],
                  [prac_cv_printf_archetype=printf])
ac_c_werror_flag=$ac_save_c_werror_flag])
AC_SUBST([prac_cv_printf_archetype])


dnl ===========================================================================
dnl Detect if openssl installed
dnl ===========================================================================

USE_OPENSSL=no
AC_ARG_WITH([openssl], 
    [AS_HELP_STRING([--with-openssl], [build with OpenSSL support(default=no)])], 
    [WITH_LIBOPENSSL="$withval"], [WITH_LIBOPENSSL=""])

if test ! "x$WITH_LIBOPENSSL" = "x"; then 
    dnl Check headers file
    AC_CHECK_HEADERS([openssl/bio.h],
            [USE_OPENSSL=yes], [AC_MSG_ERROR([could not find headers include related to openssl])])

    dnl Ensure we can link against openssl
    LIBS_SAVE="$LIBS"
    LIBS=""
    AC_SEARCH_LIBS([SSL_new], [ssl],
	    [USE_OPENSSL=yes
	    OPENSSL_LDFLAGS="$LIBS -lcrypto "
	    AC_DEFINE(HAVE_OPENSSL, 1, [Define to 1 if OpenSSL is present])],
	    [USE_OPENSSL=no],
	    [-lcrypto ])
    LIBS="$LIBS_SAVE"

    AC_SUBST([USE_OPENSSL])
    AC_SUBST([OPENSSL_LDFLAGS])
fi


dnl ===========================================================================
dnl Types, structures, compiler characteristics
dnl ===========================================================================

AC_CACHE_CHECK(
    [for tcp keepalive options],
    [pgr_cv_keepalive_tunable],
    [AC_LINK_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
        #include <stdio.h>
		#include <sys/socket.h>
		#include <netinet/in.h>
		#include <netinet/tcp.h>
	    ]],
	    [[
		setsockopt(0, IPPROTO_TCP, TCP_KEEPIDLE, NULL, 0);
        setsockopt(0, IPPROTO_TCP, TCP_KEEPINTVL, NULL, 0);
        setsockopt(0, IPPROTO_TCP, TCP_KEEPCNT, NULL, 0);
	    ]]
	)],
	[pgr_cv_keepalive_tunable=yes],
	[pgr_cv_keepalive_tunable=no]
    )]
)
if test "$pgr_cv_keepalive_tunable" = "yes" ; then
    AC_DEFINE([HAVE_KEEPALIVE_TUNABLE], [1], [Do we have tcp TCP_KEEPIDLE TCP_KEEPINTVL TCP_KEEPCNT options()?])
fi

AC_CACHE_CHECK(
    [for socket setfib option],
    [pgr_cv_setfib],
    [AC_LINK_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
        #include <stdio.h>
		#include <sys/socket.h>
	    ]],
	    [[
        setsockopt(0, SOL_SOCKET, SO_SETFIB, NULL, 0);
	    ]]
	)],
	[pgr_cv_setfib=yes],
	[pgr_cv_setfib=no]
    )]
)
if test "$pgr_cv_setfib" = "yes" ; then
    AC_DEFINE([HAVE_SETFIB], [1], [Do we have socket SO_SETFIB option()?])
fi

AC_CACHE_CHECK(
    [for socket reuseport option],
    [pgr_cv_reuseport],
    [AC_LINK_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
        #include <stdio.h>
		#include <sys/socket.h>
	    ]],
	    [[
        setsockopt(0, SOL_SOCKET, SO_REUSEPORT, NULL, 0);
	    ]]
	)],
	[pgr_cv_reuseport=yes],
	[pgr_cv_reuseport=no]
    )]
)
if test "$pgr_cv_reuseport" = "yes" ; then
    AC_DEFINE([HAVE_REUSEPORT], [1], [Do we have socket SO_REUSEPORT option()?])
fi

AC_CACHE_CHECK(
    [for socket acceptfilter option],
    [pgr_cv_acceptfilter],
    [AC_LINK_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
        #include <stdio.h>
		#include <sys/socket.h>
	    ]],
	    [[
        setsockopt(0, SOL_SOCKET, SO_ACCEPTFILTER, NULL, 0);
	    ]]
	)],
	[pgr_cv_acceptfilter=yes],
	[pgr_cv_acceptfilter=no]
    )]
)
if test "$pgr_cv_acceptfilter" = "yes" ; then
    AC_DEFINE([HAVE_DEFERRED_ACCEPT], [1], [Do we have SO_ACCEPTFILTER option()?])
fi

AC_CACHE_CHECK(
    [for tcp defer_accept option],
    [pgr_cv_defer_accept],
    [AC_LINK_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
        #include <stdio.h>
		#include <sys/socket.h>
		#include <netinet/in.h>
		#include <netinet/tcp.h>
	    ]],
	    [[
        setsockopt(0, IPPROTO_TCP, TCP_DEFER_ACCEPT, NULL, 0);
	    ]]
	)],
	[pgr_cv_defer_accept=yes],
	[pgr_cv_defer_accept=no]
    )]
)
if test "$pgr_cv_defer_accept" = "yes" ; then
    AC_DEFINE([HAVE_DEFERRED_ACCEPT], [1], [Do we have TCP_DEFER_ACCEPT option()?])
fi

AC_CACHE_CHECK(
    [for tcp fastopen option],
    [pgr_cv_fastopen],
    [AC_LINK_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
        #include <stdio.h>
		#include <sys/socket.h>
		#include <netinet/in.h>
		#include <netinet/tcp.h>
	    ]],
	    [[
        setsockopt(0, IPPROTO_TCP, TCP_FASTOPEN, NULL, 0);
	    ]]
	)],
	[pgr_cv_fastopen=yes],
	[pgr_cv_fastopen=no]
    )]
)
if test "$pgr_cv_fastopen" = "yes" ; then
    AC_DEFINE([HAVE_TCP_FASTOPEN], [1], [Do we have TCP_FASTOPEN option()?])
fi

AC_CACHE_CHECK(
    [for accept4],
    [pgr_cv_accept4],
    [AC_LINK_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
        #include <stdio.h>
		#include <sys/socket.h>
	    ]],
	    [[
        accept4(0, NULL, NULL, SOCK_NONBLOCK);
	    ]]
	)],
	[pgr_cv_accept4=yes],
	[pgr_cv_accept4=no]
    )]
)
if test "$pgr_cv_accept4" = "yes" ; then
    AC_DEFINE([HAVE_ACCEPT4], [1], [Do we have accept4()?])
fi


dnl ===========================================================================
dnl Allow the user to enable debugging with --enable-debug
dnl ===========================================================================

AC_ARG_ENABLE([debug], AC_HELP_STRING([--enable-debug], [Enable verbose debugging messages]), 
	[PGREST_DEBUG=1], [PGREST_DEBUG=0]) 

AC_DEFINE_UNQUOTED([PGREST_DEBUG], [$PGREST_DEBUG], [pg_rest library debug (0=disabled)])

dnl ===========================================================================
dnl Allow the user to enable pg_rest profiling with --enable-profile
dnl ===========================================================================

AC_ARG_ENABLE([profile], AC_HELP_STRING([--enable-profile], [Enable pg_rest profiling messages]), 
	[PGREST_PROFILE=1], [PGREST_PROFILE=0]) 

if test $PGSQL_VERSION -eq 1; then
    AC_MSG_ERROR([--enable-profile is supported only when using GCC])
fi

AC_DEFINE_UNQUOTED([PGREST_PROFILE], [$PGREST_PROFILE], [Enable pg_rest profiling (0=disabled)])

dnl ===========================================================================
dnl enables generation of code coverage metrics with gcov --enable-coverage
dnl ===========================================================================

AC_ARG_ENABLE([coverage], AC_HELP_STRING([--enable-coverage], [Enable build with coverage testing instrumentation]), 
	[PGREST_COVERAGE=yes], [PGREST_COVERAGE=no]) 

if test "$PGREST_COVERAGE" = "yes"; then
  if test ! "$GCC" = "yes"; then
    AC_MSG_ERROR([--enable-coverage is supported only when using GCC])
  fi
fi

AC_SUBST([PGREST_COVERAGE])


dnl ===========================================================================
dnl Define build date macros
dnl ===========================================================================

PGREST_BUILD_DATE=`date -u "+%Y-%m-%d %H:%M:%S"`
AC_DEFINE_UNQUOTED([PGREST_BUILD_DATE], ["$PGREST_BUILD_DATE"], [pg_rest build date])
AC_SUBST([PGREST_BUILD_DATE])

dnl ===========================================================================
dnl Other parameters 
dnl ===========================================================================

CPPFLAGS="$PGSQL_CPPFLAGS $EVENT_CPPFLAGS $XML2_CPPFLAGS $CPPFLAGS"
dnl AC_MSG_RESULT([CPPFLAGS: $CPPFLAGS])

SHLIB_LINK="$PGSQL_LDFLAGS $EVENT_LDFLAGS $XML2_LDFLAGS $OPENSSL_LDFLAGS"
AC_SUBST([SHLIB_LINK])
dnl AC_MSG_RESULT([SHLIB_LINK: $SHLIB_LINK])

dnl ===========================================================================
dnl Output the relevant files
dnl ===========================================================================

AC_OUTPUT([Makefile
   jansson/src/jansson_config.h
   pg_rest.control])
    
AC_MSG_RESULT()
AC_MSG_RESULT([  pg_rest is now configured for ${host}])
AC_MSG_RESULT()
AC_MSG_RESULT([ -------------- Compiler Info ------------- ])
AC_MSG_RESULT([  C compiler:           ${CC} ${CFLAGS}])
AC_MSG_RESULT([ -------------- Dependencies -------------- ])
AC_MSG_RESULT([  PostgreSQL config:    ${PG_CONFIG}])
AC_MSG_RESULT([  PostgreSQL version:   ${PGSQL_FULL_VERSION}])
AC_MSG_RESULT([  OpenSSL support:      ${USE_OPENSSL}])
AC_MSG_RESULT([  Libxml2 support:      ${HAVE_XML}])
if test "x$HAVE_XML" = "xyes"; then
    AC_MSG_RESULT([  Libxml2 config:       ${XML2CONFIG}])
    AC_MSG_RESULT([  Libxml2 version:      ${PGREST_LIBXML2_VERSION}])
fi
AC_MSG_RESULT([  Debug support:        ${PGREST_DEBUG}])
AC_MSG_RESULT([  Profiling support:    ${PGREST_PROFILE}])
AC_MSG_RESULT([  Coverage testing:     ${PGREST_COVERAGE}])
