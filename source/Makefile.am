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
AM_CFLAGS =
AM_CFLAGS += -DWBCFG_MULTI_COMP_SUPPORT

ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libwebconfig_framework.la 

libwebconfig_framework_la_CPPFLAGS = -I$(top_srcdir)/include \
                                     -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rbus

libwebconfig_framework_la_SOURCES = webconfig_framework.c webconfig_framework_multi_comp.c webconfig_bus_interface.c
libwebconfig_framework_la_LDFLAGS = -lpthread -lrt -lz -lrbus 

if ENABLE_CCSP_SUPPORT
libwebconfig_framework_la_CPPFLAGS += -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/ccsp
libwebconfig_framework_la_LDFLAGS += -lccsp_common
else
libwebconfig_framework_la_SOURCES += webconfig_logging.c
endif
