dnl Copyright (C) 2012 Diego Veralli <diegoveralli@yahoo.co.uk>
dnl
dnl  This file is part of midi_board.
dnl  
dnl  midi_board is free software: you can redistribute it and/or modify
dnl  it under the terms of the GNU General Public License as published by
dnl  the Free Software Foundation, either version 3 of the License, or
dnl  (at your option) any later version.
dnl
dnl  midi_board is distributed in the hope that it will be useful,
dnl  but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl  GNU General Public License for more details.
dnl
dnl  You should have received a copy of the GNU General Public License
dnl  along with midi_board. If not, see <http://www.gnu.org/licenses/>.

#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([FULL-PACKAGE-NAME], [VERSION], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([bluetooth], [bt_malloc])
AC_CHECK_LIB([cwiid], [cwiid_open])
AC_CHECK_LIB([jack], [jack_midi_event_write])
AC_CHECK_LIB([pthread], [pthread_create])

PKG_CHECK_MODULES([DEPS], [cwiid jack])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([pthread.h])
AC_CHECK_HEADER(jack/session.h, jack_session_found="yes", jack_session_found="no") 
if test "$jack_session_found" = "yes"; then 
   AC_DEFINE(JACK_SESSION, 0, [Define to enable jack session support]) 
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UINT16_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([strdup])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
