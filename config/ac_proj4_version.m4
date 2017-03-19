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

dnl
dnl Return the PROJ.4 version number
dnl

AC_DEFUN([AC_PROJ_VERSION], [
	AC_RUN_IFELSE(
        	[AC_LANG_PROGRAM([
		#ifdef HAVE_STDINT_H
        		#include <stdio.h>
		#endif
		#include "proj_api.h"
	], 
	[
		FILE *fp; 

		fp = fopen("conftest.out", "w"); 
		fprintf(fp, "%d\n", PJ_VERSION); 
		fclose(fp)])
	],
        [
		dnl The program ran successfully, so return the version number in the form MAJORMINOR
		$1=`cat conftest.out | sed 's/\([[0-9]]\)\([[0-9]]\)\([[0-9]]\)/\1\2/'`
	],
        [
		dnl The program failed so return an empty variable
		$1=""
	]
        )
])

