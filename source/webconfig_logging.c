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

#if defined(ENABLE_RDKC_SUPPORT)

const char *rdk_logger_module_fetch(void)
{
    return "LOG.RDK.WEBCONFIG";
}

#elif !defined(CCSP_SUPPORT_ENABLED)

#include "webconfig_logging.h"


void wbTraceLogAPI(const char *format, ...)
{
        va_list args;
        char tempChar[MAX_LOG_SIZE] = {0};
        memset(tempChar,0,sizeof(tempChar));
        va_start(args, format);
        vsnprintf(tempChar,MAX_LOG_SIZE,format,args);
        printf("%s",tempChar);
        va_end(args);
}

#endif
