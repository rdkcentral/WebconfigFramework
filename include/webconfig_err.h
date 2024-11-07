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


#ifndef  _WEBCONFIG_ERROR_API_H
#define  _WEBCONFIG_ERROR_API_H

#define ROLLBACK_SUCCESS 	       		  0
#define VALID_IP 		      		  0
#define BLOB_EXEC_SUCCESS              		300
#define BLOB_EXECUTION_TIMEDOUT	       		301
#define NULL_BLOB_EXEC_POINTER 	       		302
#define SUBDOC_NOT_SUPPORTED 	       		303
#define MQUEUE_OPEN_FAILED	      	   	304
#define MQUEUE_SEND_FAILED	     	   	305
#define QUEUE_PUSH_FAILED	           	306
#define VALIDATION_FALIED	    	   	307   // Return this error if rollback is not required

#define INVALID_IP 		     		308
#define INVALID_PORT 		       	   	309
#define INVALID_PROTOCOL 	      	   	310
#define INVALID_MAC      	           	311
#define STATIC_IP_NOT_IN_LANRANGE  		312
#define LAN_PARAM_IP_RANGE_INVALID 		313
#define LAN_PARAM_NETMASK_INVALID  		314
#define COSA_SYNCHRONIZE_FAILED    		315
#define FILE_OPEN_ERROR                		316
#define LAN_PARAM_LEASETIME_INVALID    		317
#define LAN_PARAM_GATEWAY_IP_INVALID    	318
#define TUNNEL_WIFI_VAP_COUNT_MISMATCH      	319
#define INVALID_VLANID_RANGE                    320
#define BLOB_EXEC_FAILURE                       321

#if defined (SPEED_BOOST_SUPPORTED)
#define OVERLAPPING_SPEEDBOOST_PORT             322
#endif

#define SYSCFG_FAILURE            	    	601
#define SYSEVENT_FAILURE            		602
#define MSG_QUEUE_POST_FAILURE          	603

#define WIFI_HAL_FAILURE                	651
#define WIFI_AP_ENABLE_HAL_FAILURE      	652

#define ADVSEC_FAILURE                  	701
#define MOCA_HAL_FAILURE                	720

#define TELE_BLOB_PROCESSES_FAILURE     	750
#define TELE_BLOB_SAVE_FAILURE          	751

#define INVALID_DMZ_WAN_IP              	790

#define MB_ERROR_PRECONDITION_FAILED            801
#define MB_ERROR_BRIDGE_MODE_ENABLED            802
#define MB_ERROR_RADIO_OFF                      803
#define MB_ERROR_BANDSTEERING_ENABLED           804
#define MB_ERROR_MESH_SERVICE_START_FAIL        805
#define MB_ERROR_MESH_SERVICE_STOP_FAIL         806


// multi comp error codes
#ifdef WBCFG_MULTI_COMP_SUPPORT

#define SLAVE_RESPONSE_TIME_OUT 	    	850
#define SLAVE_EXEC_TIME_OUT			851

#define SLAVE_EXEC_FAILED			852
#define SLAVE_NOT_READY          853

#define POST_EXEC_SUCCESS              0
#endif

#define TR104_HAL_FAILURE                       950

#endif /* _WEBCONFIG_ERROR_API_H */
