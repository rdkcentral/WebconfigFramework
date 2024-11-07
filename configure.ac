##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2015 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
#                                              -*- Autoconf -*-
# Process this file with autoconf to produce configure script.
#

AC_PREREQ([2.65])
AC_INIT([ccsp_common], [1.0], [BUG-REPORT-ADDRESS])
AM_INIT_AUTOMAKE([foreign])
LT_INIT

AC_PREFIX_DEFAULT(`pwd`)
AC_ENABLE_SHARED
AC_DISABLE_STATIC

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_CC_C_O
AM_PROG_LIBTOOL(libtool)

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE

# Checks for library functions.
AC_FUNC_MALLOC

ENABLE_CCSP_SUPPORT=false
AC_ARG_ENABLE([ccspsupport],
              AS_HELP_STRING([--enable-ccspsupport],[enable ccsp support (default is no)]),
              [
                case "${enableval}" in
                 yes) ENABLE_CCSP_SUPPORT=true;;
                  no) AC_MSG_ERROR([ccsp support is disabled]) ;;
                   *) AC_MSG_ERROR([bad value ${enableval} for --enable-ccspsupport]) ;;
                esac
              ],
              [echo "ccsp support is disabled"])

AM_CONDITIONAL([ENABLE_CCSP_SUPPORT], [test x$ENABLE_CCSP_SUPPORT = xtrue])

AC_CONFIG_FILES(
    source/Makefile
	Makefile


)


AC_OUTPUT

