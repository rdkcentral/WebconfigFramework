/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2015 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/


#ifndef  _WEBCONFIG_LOGGING_API_H
#define  _WEBCONFIG_LOGGING_API_H

#if defined(CCSP_SUPPORT_ENABLED)

#define WbInfo     CcspTraceInfo
#define WbError    CcspTraceError
#define WbWarning  CcspTraceWarning  
#define WbDebug    CcspTraceDebug
#elif defined(ENABLE_RDKC_SUPPORT)
#include <cimplog.h>

#define LOGGING_MODULE                      "WEBCONFIG"
#define EXTRACT_ARGS(msg ...) msg

#define WbError(msg)        cimplog_error(LOGGING_MODULE, EXTRACT_ARGS msg)
#define WbInfo(msg)         cimplog_info(LOGGING_MODULE, EXTRACT_ARGS msg)
#define WbWarning(msg)      cimplog_info(LOGGING_MODULE, EXTRACT_ARGS msg)
#define WbDebug(msg)        cimplog_debug(LOGGING_MODULE, EXTRACT_ARGS msg)
#else

#include<stdio.h>
#include<stdarg.h> 
#include<string.h>

#define EXTRACT_ARGS(msg ...) msg
#define MAX_LOG_SIZE    4096
void wbTraceLogAPI(const char *format, ...);

#define  WbError(msg)  \
            wbTraceLogAPI(EXTRACT_ARGS msg);

#define  WbInfo(msg)  \
            wbTraceLogAPI(EXTRACT_ARGS msg);

#define  WbWarning(msg)  \
            wbTraceLogAPI(EXTRACT_ARGS msg); 

#define  WbDebug(msg)   \
            wbTraceLogAPI(EXTRACT_ARGS msg); 

#endif

#endif
