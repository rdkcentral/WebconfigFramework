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
#include "webconfig_bus_interface.h"
#include "webconfig_logging.h"

static int gRbusEnabled = 0;
static rbusHandle_t bus_handle_rbus = NULL;
extern char process_name[64] ;

pthread_mutex_t webcfg_rbus_enable = PTHREAD_MUTEX_INITIALIZER;

void rbusInit()
{
	WbInfo(("Entering %s\n", __FUNCTION__));

	int ret = RBUS_ERROR_SUCCESS;
	char comp_name_wbcfg[72];
	memset(comp_name_wbcfg,0,sizeof(comp_name_wbcfg));
	snprintf(comp_name_wbcfg,sizeof(comp_name_wbcfg),"%s_wbcfg",process_name);
	ret = rbus_open(&bus_handle_rbus, comp_name_wbcfg);

	if(ret != RBUS_ERROR_SUCCESS) {
		WbError(("%s: init failed with error code %d \n", __FUNCTION__, ret));
		return ;
	}    
}

int isWebCfgRbusEnabled()
{
	pthread_mutex_lock(&webcfg_rbus_enable);
	if ( gRbusEnabled == 0 )
	{
		if(RBUS_ENABLED == rbus_checkStatus()) 
		{
			rbusInit();
			gRbusEnabled = 1 ;
		}   
	}
    WbInfo(("%s: rbus enabled is %d \n", __FUNCTION__, gRbusEnabled));
	pthread_mutex_unlock(&webcfg_rbus_enable);
    return gRbusEnabled;
}

void* subscribeSubdocForceReset(void* arg) {
	pthread_detach(pthread_self());

	if(1 == isWebCfgRbusEnabled()) {
		bool rc = false;
		int timer_count = 0;

		WbInfo(("%s: Discover component for event '%s' \n", __FUNCTION__, WEBCFG_SUBDOC_FORCE_RESET_EVENT));
		while(!rc) {
			rc = webcfg_rbus_discover_component(WEBCFG_SUBDOC_FORCE_RESET_EVENT);
			sleep(SUBDOC_FORCE_RESET_DISC_COMP_INTERVAL);
			timer_count++;
			if (timer_count >= SUBDOC_FORCE_RESET_DISC_COMP_TIMEOUT_COUNT) {
				WbError(("%s: Unable to discover component for %s\n", __FUNCTION__, WEBCFG_SUBDOC_FORCE_RESET_EVENT));
				return arg;
			}
		}
		WbInfo(("%s: [%s] Component found\n", __FUNCTION__, WEBCFG_SUBDOC_FORCE_RESET_EVENT));

		WbInfo(("%s: subscribing to event %s \n", __FUNCTION__, WEBCFG_SUBDOC_FORCE_RESET_EVENT));
		int ret = RBUS_ERROR_BUS_ERROR;
		ret = rbusEvent_Subscribe(bus_handle_rbus, WEBCFG_SUBDOC_FORCE_RESET_EVENT, subdocForceReset_callbk_rbus, process_name, SUBDOC_FORCE_RESET_SUB_TIMEOUT);
		if ((ret == RBUS_ERROR_SUCCESS) || (ret == RBUS_ERROR_SUBSCRIPTION_ALREADY_EXIST)) {
			WbInfo(("%s: subscribed to event %s \n", __FUNCTION__, WEBCFG_SUBDOC_FORCE_RESET_EVENT));
		}
		else {
			WbError(("%s: Unable to subscribe to event %s with rbus error code : %d\n", __FUNCTION__, WEBCFG_SUBDOC_FORCE_RESET_EVENT, ret));
		}
		WbInfo(("%s: job done\n", __FUNCTION__));
	}
	else {
		WbError(("RBUS disabled. Unable to subscribe to event %s\n", WEBCFG_SUBDOC_FORCE_RESET_EVENT));

	}
	return arg;
}

bool webcfg_rbus_discover_component(char const *pParamName)
{
    rbusError_t rc = RBUS_ERROR_SUCCESS;
    int componentCnt = 0;
    char **pComponentNames;
    bool ret = false;
    char const *rbusModuleList[1];
    int count = 1;
    int i;

    rbusModuleList[0] = pParamName;
	WbInfo(("%s rbusModuleList[%s]\n", __FUNCTION__, rbusModuleList[0]));

    rc = rbus_discoverComponentName(bus_handle_rbus, count, rbusModuleList, &componentCnt, &pComponentNames);

    if(RBUS_ERROR_SUCCESS != rc)
    {
        WbError(("Failed to discover component. Error Code = %d\n", rc));
        return ret;
    }

    for (i = 0; i < componentCnt; i++)
    {
        free(pComponentNames[i]);
    }

    free(pComponentNames);

    if(componentCnt == count)
    {
        ret = true;
    }

    WbInfo(("%s: (%d-%d)ret[%s]\n", __FUNCTION__, componentCnt, count, (ret)?"TRUE":"FALSE"));

    return ret;
}

/*************************************************************************************************************************************

    caller:    callback function for WEBCFG_SUBDOC_FORCE_RESET_EVENT

    prototype:
        void subdocForceReset_callbk_rbus
        (rbusHandle_t handle,
         rbusEvent_t const* event, 
         rbusEventSubscription_t* subscription) 
            
        );

    description :
    callback function for WEBCFG_SUBDOC_FORCE_RESET_EVENT


****************************************************************************************************************************************/

void subdocForceReset_callbk_rbus(rbusHandle_t handle, rbusEvent_t const* event, rbusEventSubscription_t* subscription) {
	WbInfo(("Entering %s\n", __FUNCTION__));

	(void)(handle);
	(void)(subscription);

	const char* eventName = event->name;

    rbusValue_t valBuff;
    valBuff = rbusObject_GetValue(event->data, NULL );
    if(!valBuff)
    {
        WbInfo(("%s FAIL: value is NULL\n", __FUNCTION__));
    }
    else
    {
		char* subdocName;
        char* data = (char*) rbusValue_GetString(valBuff, NULL);
        WbInfo(("rbus event callback Event is %s , data is %s\n",eventName,data));

		while ((subdocName = strtok_r(data, ",", &data))) {
			WbInfo(("%s : Procssing subdoc version reset for '%s'\n",__FUNCTION__, subdocName));
			resetSubdocVersion(subdocName);
		}
    }
    WbInfo(("Exiting %s\n", __FUNCTION__));
}

#ifdef WBCFG_MULTI_COMP_SUPPORT

int gBroadcastSubscribed = 0;
int gMasterSubscribed = 0;
int gSlaveSubscribed = 0;

extern int slaveExecutionCount ;
rbusDataElement_t dataElements_slave[1] = {
                            {MASTER_COMP_SIGNAL_NAME, RBUS_ELEMENT_TYPE_EVENT, {NULL,NULL,NULL,NULL, eventSubHandlerWbFramework, NULL}}
                        };

rbusDataElement_t dataElements_master[1] = {
                 {SLAVE_COMP_SIGNAL_NAME, RBUS_ELEMENT_TYPE_EVENT, {NULL,NULL,NULL,NULL, eventSubHandlerWbFramework, NULL}}
               };   

rbusDataElement_t dataElements_broadcast[1] = {
              {BROADCASTSIGNAL_NAME, RBUS_ELEMENT_TYPE_EVENT, {NULL,NULL,NULL,NULL, eventSubHandlerWbFramework, NULL}}
          };


/*************************************************************************************************************************************

    caller:    callback function for rbus events

    prototype:
        void multiComp_callbk_rbus
        (rbusHandle_t handle,
         rbusEvent_t const* event, 
         rbusEventSubscription_t* subscription) 
            
        );

    description :
    callback function for rbus events, based on the event name it calls the api to parse the data


****************************************************************************************************************************************/

void multiComp_callbk_rbus(rbusHandle_t handle, rbusEvent_t const* event, rbusEventSubscription_t* subscription) {
	WbInfo(("Entering %s\n", __FUNCTION__));

    	(void)(handle);
    	(void)(subscription);

    	const char* eventName = event->name;

    rbusValue_t valBuff;
    valBuff = rbusObject_GetValue(event->data, NULL );
    if(!valBuff)
    {
        WbInfo(("FAIL: value is NULL\n"));
    }
    else
    {
        const char* data = rbusValue_GetString(valBuff, NULL);
        WbInfo(("rbus event callback Event is %s , data is %s\n",eventName,data));

    	if ( strncmp(eventName,BROADCASTSIGNAL_NAME,strlen(BROADCASTSIGNAL_NAME)) == 0 )
    	{
        	parseBroadcastData(data);
    	}
    	else if ( strncmp(eventName,MASTER_COMP_SIGNAL_NAME,strlen(MASTER_COMP_SIGNAL_NAME)) == 0 )
    	{
        	parseMasterData(data);
    	}
    	else if ( strncmp(eventName,SLAVE_COMP_SIGNAL_NAME,strlen(SLAVE_COMP_SIGNAL_NAME)) == 0 )
    	{
        	parseSlaveData(data);
    	}
    }
    	WbInfo(("Exiting %s\n", __FUNCTION__));
}

/*************************************************************************************************************************************

    caller:    event subscription notification

    prototype:
    rbusError_t eventSubHandlerWbFramework
    (
      rbusHandle_t handle, 
      rbusEventSubAction_t action, 
      const char* eventName, 
      rbusFilter_t filter, 
      int32_t interval, 
      bool* autoPublish
      );


    description :
    Call back function notifies when rbus event subscription/unsuscription happens

****************************************************************************************************************************************/


rbusError_t eventSubHandlerWbFramework(rbusHandle_t handle, rbusEventSubAction_t action, const char* eventName, rbusFilter_t filter, int32_t interval, bool* autoPublish)
{
	(void)handle;
    	(void)filter;
    	(void)interval;
    	(void)autoPublish;

    	WbInfo((
        	"eventSubHandlerWbFramework called:\n" \
        	"\taction=%s\n" \
        	"\teventName=%s\n",
        	action == RBUS_EVENT_ACTION_SUBSCRIBE ? "subscribe" : "unsubscribe",
        	eventName));

    	if(!strcmp(BROADCASTSIGNAL_NAME, eventName))
    	{
        	gBroadcastSubscribed = action == RBUS_EVENT_ACTION_SUBSCRIBE ? 1 : 0;
    	}
    	else if(!strcmp(MASTER_COMP_SIGNAL_NAME, eventName))
    	{
        	gMasterSubscribed = action == RBUS_EVENT_ACTION_SUBSCRIBE ? 1 : 0;

          if (gMasterSubscribed == 0 &&  slaveExecutionCount == 0 ) 
          {
                WbInfo(("%s unsubscribed: slaveExecutionCount is 0 , unregistering event\n", MASTER_COMP_SIGNAL_NAME));
                UnregisterFromEvent(SLAVE_COMP_SIGNAL_NAME);
          }
    	}
    	else if(!strcmp(SLAVE_COMP_SIGNAL_NAME, eventName))
    	{
        	gSlaveSubscribed = action == RBUS_EVENT_ACTION_SUBSCRIBE ? 1 : 0;
    	}
    	else
    	{
        	WbError(("provider: eventSubHandlerWbFramework unexpected eventName %s\n", eventName));
    	}

    	return RBUS_ERROR_SUCCESS;
}

/* thread created to subscribe to broadcast event published by master in slave component
It waits till master registers the event it is publishing*/
void*  event_subscribe_bcast(void* arg)
{
    	WbInfo(("Entering %s\n", __FUNCTION__));
    	(void)arg;
    	int ret = 0;
    	while(1)
    	{
        	ret = subscribeToEvent(BROADCASTSIGNAL_NAME);

        	if ( RBUS_ERROR_SUCCESS == ret )
        	{
               		break;
        	}                                    
        	sleep(5);
    	}
    	WbInfo(("Exiting %s\n", __FUNCTION__));
    	return NULL;
}

/*************************************************************************************************************************************

    caller:    initMultiCompSlave

    void eventRegisterSlave
    (
    );

    description :
    Event registration from slave component to receive data from master

****************************************************************************************************************************************/

void eventRegisterSlave()
{
  	WbInfo(("Entering %s\n", __FUNCTION__));
  	if ( 1 == isWebCfgRbusEnabled() )
  	{
      		// creating thread to register to broadcast event 

      		pthread_t bcast_regtid;
      		pthread_create(&bcast_regtid, NULL, event_subscribe_bcast,NULL);
  	}
  	else
  	{

        	#if defined(CCSP_SUPPORT_ENABLED)
            int ret = 0;
          	CcspBaseIf_SetCallback2(bus_handle, BROADCASTSIGNAL_NAME,
                     	multiCompBroadCastSignal_callbk, NULL);

          	ret = CcspBaseIf_Register_Event(bus_handle, NULL, BROADCASTSIGNAL_NAME);
              	if (ret != CCSP_Message_Bus_OK) {
               	WbError(("multiCompBroadCastSignal reg unsuccessfull\n"));
             	} 
          	else 
          	{
                 	WbInfo(("multiCompBroadCastSignal Registration with CCSP Bus successful\n"));
          	}
              	CcspBaseIf_SetCallback2(bus_handle, SLAVE_COMP_SIGNAL_NAME,
                	multiCompSlaveProcessSignal_callbk, NULL);
        	#else
              	WbError(("%s : CCSP_SUPPORT_NOT_ENABLED\n",__FUNCTION__ ));
        	#endif
  	}

}

/*************************************************************************************************************************************

    caller:    initMultiCompMaster

    void eventRegisterMaster
    (
    );

    description :
    Event registration from master component to receive data from slave

****************************************************************************************************************************************/


void eventRegisterMaster()
{

	int ret = 0;
  	WbInfo(("%s for process  %s \n", __FUNCTION__, process_name));
    if ( 1 == isWebCfgRbusEnabled() )
  	{
          	/***
           	* Register data elements with rbus for EVENTS.
           	*/
          	ret = rbus_regDataElements(bus_handle_rbus, 1, dataElements_broadcast);
          	if(ret != RBUS_ERROR_SUCCESS)
          	{
              		WbError(("Failed to register multiCompBroadCastSignal data elements with rbus. Error code : %d\n", ret));
          	}   

  	}
  	else
  	{
        	#if defined(CCSP_SUPPORT_ENABLED)

          	CcspBaseIf_SetCallback2(bus_handle, MASTER_COMP_SIGNAL_NAME,
                	multiCompMasterProcessSignal_callbk, NULL);
        	#else
              	WbError(("%s : CCSP_SUPPORT_NOT_ENABLED\n",__FUNCTION__ ));
        	#endif
  	}
}

/*************************************************************************************************************************************

    caller:    parseBroadcastData,sendBlobExecutionResult , sendDataToSlaveComp 

    void sendDataToEvent
    (
      char* event_name ,
      char* eventData
    );

    description :
    RBUS/DBUS events to send data between master and slave

****************************************************************************************************************************************/


void sendDataToEvent(char* event_name , char* eventData)
{
	 WbInfo(("%s : Event name is %s Data is %s , gRbusEnabled is %d\n", __FUNCTION__,event_name,eventData,gRbusEnabled));
    	if ( gRbusEnabled == 1 )
    	{
            	rbusEvent_t event;
            	rbusObject_t data;
        	rbusValue_t value;
            	rbusError_t ret = RBUS_ERROR_SUCCESS;

            	rbusValue_Init(&value);
            	rbusValue_SetString(value, eventData);
            	rbusObject_Init(&data, NULL);
            	rbusObject_SetValue(data, event_name, value);

            	event.name = event_name;
            	event.data = data;
            	event.type = RBUS_EVENT_GENERAL;
            	ret = rbusEvent_Publish(bus_handle_rbus, &event);
            	if(ret != RBUS_ERROR_SUCCESS) {
                	WbInfo(("rbusEvent_Publish Event1 failed: %d\n", ret));
            	}

            	rbusValue_Release(value);
            	WbInfo(("%s --out\n", __FUNCTION__));
    	}
    	else
    	{
        	#if defined(CCSP_SUPPORT_ENABLED)
            	WbInfo(("%s : calling CcspBaseIf_SendSignal_WithData,event_name is %s\n", __FUNCTION__,event_name));
                int ret = 0;

            	ret = CcspBaseIf_SendSignal_WithData(bus_handle,event_name,eventData);

            	if ( ret != CCSP_SUCCESS )
            	{
                 	WbError(("%s : %s event failed,  ret value is %d\n",__FUNCTION__,event_name,ret));
            	}
            	WbInfo(("%s : return value is %d \n", __FUNCTION__,ret));
        	#else
              	WbError(("%s : CCSP_SUPPORT_NOT_ENABLED\n",__FUNCTION__ ));
        	#endif
          }
}

/*************************************************************************************************************************************

    caller:    messageQueueProcessingMultiComp, messageQueueProcessingMultiCompSlave

    void EventRegister
    (
      char* EventName,
    );

    description :
    RBUS/DBUS event registration in runtime for master and slave communication
****************************************************************************************************************************************/

void EventRegister(char* EventName)
{

	WbInfo(("%s : event name is %s\n", __FUNCTION__,EventName));

    	int ret = 0 ;
    	if (gRbusEnabled == 1 )
    	{
        	/***
            	* Register data elements with rbus for EVENTS.
            	*/            
            	if ( strncmp(EventName,SLAVE_COMP_SIGNAL_NAME,strlen(SLAVE_COMP_SIGNAL_NAME)) == 0 )
            	{
               		ret = rbus_regDataElements(bus_handle_rbus, 1, dataElements_slave);
            	}
            	else if ( strncmp(EventName,MASTER_COMP_SIGNAL_NAME,strlen(MASTER_COMP_SIGNAL_NAME)) == 0 )
            	{
                	ret = rbus_regDataElements(bus_handle_rbus, 1, dataElements_master);
            	}
    	}
    	else
    	{
        	#if defined(CCSP_SUPPORT_ENABLED)
            	ret = CcspBaseIf_Register_Event(bus_handle, NULL, EventName );
            	if (ret != CCSP_Message_Bus_OK) {
                    WbError(("%s reg unsuccessfull\n",EventName));
                    return ;
            	} 
		else {
                    WbInfo(("%s registration with CCSP Bus successful\n",EventName));
            	}

            	WbInfo(("%s : return value is %d \n", __FUNCTION__,ret));
        	#else
              	WbError(("%s : CCSP_SUPPORT_NOT_ENABLED\n",__FUNCTION__ ));
        	#endif
    	}
        WbInfo(("Exiting from %s and return value is %d\n", __FUNCTION__,ret));
	return;
}

/*************************************************************************************************************************************

    caller:    messageQueueProcessingMultiComp

    void UnregisterFromEvent
    (
      char* EventName,
    );

    description :
    API to unsubscribe from rbus events
****************************************************************************************************************************************/

void UnSubscribeFromEvent(char* EventName)
{
	if (gRbusEnabled == 1 )
    	{
        	WbInfo(("%s : event name is %s\n", __FUNCTION__,EventName));
        	int ret = 0 ;
          	ret = rbusEvent_Unsubscribe(bus_handle_rbus, EventName);
          	if ( ret != RBUS_ERROR_SUCCESS )
          	{
              		WbError(("%s Unsubscribe failed\n",EventName));
              		return ;
          	} 
          	else 
		{
              		WbInfo(("%s Unsubscribe with rbus successful\n",EventName));
          	}
    	}
}
/*************************************************************************************************************************************

    caller:    messageQueueProcessingMultiComp, messageQueueProcessingMultiCompSlave

    void UnregisterFromEvent
    (
      char* EventName,
    );

    description :
    API to unregister from events, unpublish events for rbus
****************************************************************************************************************************************/

void UnregisterFromEvent(char* EventName)
{
	WbInfo(("%s : event name is %s\n", __FUNCTION__,EventName));
    	int ret = 0 ;
        if ( gRbusEnabled == 1 )
        {
            	if ( strncmp(EventName,SLAVE_COMP_SIGNAL_NAME,strlen(SLAVE_COMP_SIGNAL_NAME)) == 0 )
                {
                        ret = rbus_unregDataElements(bus_handle_rbus, 1, dataElements_slave);
                        gMasterSubscribed = 0;

                }
                else if ( strncmp(EventName,MASTER_COMP_SIGNAL_NAME,strlen(MASTER_COMP_SIGNAL_NAME)) == 0 )
                {
                        ret = rbus_unregDataElements(bus_handle_rbus, 1, dataElements_master);
                        gSlaveSubscribed = 0;

                }
         }
         else
         {
         	#if defined(CCSP_SUPPORT_ENABLED)
                ret = CcspBaseIf_UnRegister_Event(bus_handle, NULL, EventName);
                if (ret != CCSP_Message_Bus_OK) {
               		WbError(("%s unreg failed\n",EventName));
                    	return ;
                }else {
                    	WbInfo(("%s UnRegistration with CCSP Bus successful\n",EventName));
                }
              	#else
                WbError(("%s : CCSP_SUPPORT_NOT_ENABLED\n",__FUNCTION__ ));
              	#endif
          }

          WbInfo(("Exiting from %s and return value is %d\n", __FUNCTION__,ret));
    	return ;
}

/*************************************************************************************************************************************

    caller:    messageQueueProcessingMultiComp

    void UnregisterFromEvent
    (
      char* EventName,
    );

    description :
    subscribe to event
****************************************************************************************************************************************/

int subscribeToEvent(char* EventName)
{
    	int ret = RBUS_ERROR_SUCCESS ;
    	WbInfo(("%s : event name is %s\n", __FUNCTION__,EventName));
    	if ( gRbusEnabled == 1 )
    	{

        	char user_data[64] = {0};
        	if ( strncmp(EventName,SLAVE_COMP_SIGNAL_NAME,strlen(SLAVE_COMP_SIGNAL_NAME)) == 0 )
        	{
            		strncpy(user_data,"slaveSignal",sizeof(user_data)-1);
        	}
        	else if ( strncmp(EventName,MASTER_COMP_SIGNAL_NAME,strlen(MASTER_COMP_SIGNAL_NAME)) == 0 )
        	{       
            		strncpy(user_data,"masterSignal",sizeof(user_data)-1);
        	}
        	else if ( strncmp(EventName,BROADCASTSIGNAL_NAME,strlen(MASTER_COMP_SIGNAL_NAME)) == 0 )
        	{       
            		strncpy(user_data,"broadcastSignal",sizeof(user_data)-1);
        	}

        	ret = rbusEvent_Subscribe(bus_handle_rbus, EventName, multiComp_callbk_rbus, user_data, 60);
        	if(ret != RBUS_ERROR_SUCCESS) {
            		WbError(("Unable to subscribe to event %s with rbus error code : %d\n", EventName, ret));
                    return -1;
        	} 
        
        	WbInfo(("%s : subscribe to %s ret value is %d\n", __FUNCTION__,EventName,ret));
    	}
   	return 0;   
}

#endif 
/*************************************************************************************************************************************

    caller:    messageQueueProcessing, messageQueueProcessingMultiComp

    void sendWebConfigSignal
    (
      char* data,
    );

    description :
    API sends ACK/ACK to webconfig client
****************************************************************************************************************************************/

void sendWebConfigSignal(char* data)
{
	int ret = -1 ;
        if ( 1 == isWebCfgRbusEnabled() )
      	{
		#ifndef WEBCONFIG_BIN_SUPPORT
        	rbusMessage request, response;
          	rbusMessage_Init(&request);
          	rbusMessage_SetString(request,data);
          	WbInfo(("%s : rbus_publishEvent :: event_name : %s :: \n", __FUNCTION__, "webconfigSignal"));
          	if(( ret = rbus_invokeRemoteMethod("eRT.com.cisco.spvtg.ccsp.webpaagent", "webconfigSignal", request, 6000, &response)) != RBUSCORE_SUCCESS )
         	{
              		WbError(("%s rbus_invokeRemoteMethod for webconfigSignal failed & returns with Err: %d\n", __FUNCTION__, ret));
          	}
          	else
          	{
              		rbusMessage_Release(response);
          	}
		#else
		WbInfo(("%s : rbus_set :: event_name : %s :: \n", __FUNCTION__, "webconfigSignal"));
		bool isCommit = true;
		int sessionId = 0;
		rbusValue_t setVal;
		rbusValue_Init(&setVal);
		rbusValue_SetFromString(setVal, RBUS_STRING, data);

		rbusSetOptions_t opts = {isCommit,sessionId};
		ret = rbus_set(bus_handle_rbus, "webconfigSignal", setVal, &opts);
		if(RBUS_ERROR_SUCCESS != ret)
		{
			WbError(("%s rbus_set for webconfigSignal failed & returns with Err: %d\n", __FUNCTION__, ret));
		}
		rbusValue_Release(setVal);
		#endif
        }
        else
        {
            	#if defined(CCSP_SUPPORT_ENABLED)
                ret =  CcspBaseIf_WebConfigSignal(bus_handle, data);

                if ( ret != CCSP_SUCCESS )
                    WbError(("%s : CcspBaseIf_WebConfigSignal failed,  ret value is %d\n",__FUNCTION__,ret));
            	#else
                WbError(("%s : CCSP_SUPPORT_NOT_ENABLED\n",__FUNCTION__ ));
            	#endif
        }
    	return ;
}
