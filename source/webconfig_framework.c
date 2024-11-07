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

#include <fcntl.h>

#include "webconfig_framework.h"
#include "webconfig_bus_interface.h"
#include "webconfig_logging.h"
#include "webconfig_err.h"
#define MAX_DEBUG_ITER           100

char mqEventName[64] = {0};


char process_name[64]={0};

int gNumOfSubdocs = 0;

pthread_mutex_t webconfig_exec = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t webconfig_exec_completed = PTHREAD_COND_INITIALIZER;

pthread_mutex_t queue_access = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t reg_subdoc = PTHREAD_MUTEX_INITIALIZER;

pthread_condattr_t webconfig_attr;


PblobRegInfo blobData;

queueInfo  queueData;

#ifdef WBCFG_MULTI_COMP_SUPPORT

char multiCompMqEventName[64] = {0} , mCompMqSlaveName[64] = {0};

extern queueInfo  mCompQueueData;

extern pthread_mutex_t mCompQueue_access ;

extern int checkIfVersionExecInMultiCompQueue(uint32_t version,int *queueIndex);
extern void* messageQueueProcessingMultiComp();
extern void* messageQueueProcessingMultiCompSlave();
extern int isMultiCompQueueEmpty();
extern void initMultiComp();
#endif
setVersion updateVersion;
pErr execReturn = NULL;


/*************************************************************************************************************************************

    caller:     register_sub_docs

    prototype:

        void*
        display_subDocs
            (
            );

    description:

        Function/thread  for debugging purpuse. When FRAMEWORK_DEBUG is created thread logs all the registered sub docs and version 
 		and also the requests in queue. It runs is infinite loop, when FRAMEWORK_DEBUG file exists thread it logs the queue info 
 		in DEFAULT_DEBUG_INTERVAL for DEFAULT_DEBUG_ITER times

**************************************************************************************************************************************/
void* display_subDocs()
{

	WbInfo(("Inside FUNC %s LINE %d\n",__FUNCTION__,__LINE__));
	PblobRegInfo blobDisplayData;
	FILE *fp, *fd;
	fd=NULL;
	int i;
	int n;
    	int interval =DEFAULT_DEBUG_INTERVAL , num_of_dbg_iter=DEFAULT_DEBUG_ITER;

	pthread_detach(pthread_self());
	while (1)
	{

		sleep(DEFAULT_DEBUG_INTERVAL);
		fp = fopen (FRAMEWORK_DEBUG, "r");
	    	if( fp )
	    	{ 
			n = fscanf(fp, "%d %d", &interval,&num_of_dbg_iter);//CID 144038: Unchecked return value from library (CHECKED_RETURN)
			if (n != 2) {
   				interval = DEFAULT_DEBUG_INTERVAL;
   				num_of_dbg_iter = DEFAULT_DEBUG_ITER;
                                    }

			if (num_of_dbg_iter > MAX_DEBUG_ITER) { //CID 144041: Untrusted loop bound (TAINTED_SCALAR)
                                 num_of_dbg_iter = MAX_DEBUG_ITER; 
                                }

			WbInfo(("interval is %d, num_of_dbg_iter is %d \n",interval,num_of_dbg_iter));

			do
			{
				if(fd)
				{
				    fclose(fd);
				    fd = NULL;
				}
				pthread_mutex_lock(&reg_subdoc);

				blobDisplayData = blobData;

				for (i=0 ; i < gNumOfSubdocs ; i++)
				{	
					WbInfo(("SUBDOC_NAME is %s , current version is %u \n",blobDisplayData->subdoc_name, blobDisplayData->version));
    					blobDisplayData ++;

				}

				pthread_mutex_unlock(&reg_subdoc);

				i = 0;
				
				pthread_mutex_lock(&queue_access);


				if (isQueueEmpty())
			  	{		
					WbInfo(("%s : Queue Empty\n",__FUNCTION__));
			  	}

			  	else
			  	{
			  		for( i = queueData.front; i!=queueData.rear; i=(i+1)%QUEUE_SIZE) 
				    {
				  		  WbInfo(("TXID %hu version %u timeout %zu state %d \n",queueData.txid_queue[i],queueData.version_queue[i],queueData.timeout_queue[i],queueData.blob_state_queue[i]));

				    }
				    WbInfo(("TXID %hu version %u timeout %zu state %d \n",queueData.txid_queue[i],queueData.version_queue[i],queueData.timeout_queue[i],queueData.blob_state_queue[i]));


			  	}

			  	pthread_mutex_unlock(&queue_access);

			  	#ifdef WBCFG_MULTI_COMP_SUPPORT

				pthread_mutex_lock(&mCompQueue_access);


				if (isMultiCompQueueEmpty())
			  	{		
					WbInfo(("%s : MultiCompQueue Empty\n",__FUNCTION__));
			  	}

			  	else
			  	{
			  		for( i = mCompQueueData.front; i!=mCompQueueData.rear; i=(i+1)%QUEUE_SIZE) 
				    {
				  		  WbInfo(("MultiComp TXID %hu version %u timeout %zu state %d \n",mCompQueueData.txid_queue[i],mCompQueueData.version_queue[i],mCompQueueData.timeout_queue[i],mCompQueueData.blob_state_queue[i]));

				    }
				    WbInfo(("MultiComp %hu version %u timeout %zu state %d \n",mCompQueueData.txid_queue[i],mCompQueueData.version_queue[i],mCompQueueData.timeout_queue[i],mCompQueueData.blob_state_queue[i]));


			  	}

			  	pthread_mutex_unlock(&mCompQueue_access);

			  	#endif

				num_of_dbg_iter--;
				sleep(interval);

			} while((num_of_dbg_iter>0) && ((fd=fopen (FRAMEWORK_DEBUG, "r")) != 0));//CID 144042: Time of check time of use (TOCTOU)

			fclose(fp);
                        fp=NULL;

			unlink(FRAMEWORK_DEBUG);
			interval = DEFAULT_DEBUG_INTERVAL;
			num_of_dbg_iter = DEFAULT_DEBUG_ITER;
		}	 
	}
	return NULL;
}

/*************************************************************************************************************************************

    caller:     Component during initialization

    prototype:

        void
        register_sub_docs
            (
            	blobRegInfo *bInfo,
            	int NumOfSubdocs,
            	getVersion getv,
            	setVersion setv
            );

    description:
       Component needs to register all the subdoc’s it supports during its initialization. 

    argument:   
    	blobRegInfo		bInfo,
	This is reg data , maintains subdoc name and version

        int                    	NumOfSubdocs,
	This argument expects number of subdocs registered by the component.
			
        getVersion              getv,
        This function pointer expects a handler from component to return the current applied version for subdoc name passed as arg. 


        setVersion              setv
        This function pointer expects a handler from component to set the current version for subdoc name passed as arg. 



***************************************************************************************************************************************/

void register_sub_docs(blobRegInfo *bInfo,int NumOfSubdocs, getVersion getv , setVersion setv )
{
	WbInfo(("Inside FUNC %s LINE %d\n",__FUNCTION__,__LINE__));

	gNumOfSubdocs=NumOfSubdocs;

	blobData= bInfo ;


	memset(&queueData, 0, sizeof(queueData));


	queueData.front = -1;
	queueData.rear = -1;

#ifdef WBCFG_MULTI_COMP_SUPPORT
	
	memset(&mCompQueueData, 0, sizeof(mCompQueueData));

	mCompQueueData.front = -1;
	mCompQueueData.rear = -1;
#endif
	int i ;

	if ( getv != NULL )
	{
	 	for (i=0 ; i < gNumOfSubdocs ; i++)
		{
			 bInfo->version = getv(bInfo->subdoc_name);
			 bInfo++;
		}
	}

	if ( setv != NULL )
			updateVersion = setv;

	// Intialize message queue
	initMessageQueue();
	
	pthread_t tid, tid_sub;

	int ret = pthread_create(&tid, NULL, &display_subDocs, NULL); 

	if ( ret != 0 )
		WbError(("%s: display_subDocs pthread_create failed , ERROR : %s \n", __FUNCTION__,strerror(errno)));

	//Thread to subscribe WEBCFG_SUBDOC_FORCE_RESET_EVENT 
	ret = pthread_create(&tid_sub, NULL, &subscribeSubdocForceReset, NULL); 

	if ( ret != 0 )
		WbError(("%s: subscribeSubdocForceReset pthread_create failed , ERROR : %s \n", __FUNCTION__,strerror(errno)));
		
}

/*************************************************************************************************************************************

    caller:    check_component_crash

    prototype:

        void
        notifyVersion_to_Webconfig
            (
            	char* subdoc_name,
 				uint32_t version
            );

    description :
	 Function to notify subdoc version to webconfig client 

    argument:   
		char* subdoc_name,	
		Name of subdoc

 		uint32_t version
 		Version number of of a subdoc

***************************************************************************************************************************************/

void notifyVersion_to_Webconfig(char* subdoc_name, uint32_t version,int process_crashed)
{

	WbInfo(("%s : doc name %s , doc version %u\n",__FUNCTION__,subdoc_name,version));

	char data[128]= {0};
    
    	if ( process_crashed == 1 )
    	{
   	   	 snprintf(data,sizeof(data),"%s,0,%u,%s",subdoc_name,version,COMPONENT_CRASH_EVENT);
    	}
    	else
    	{
        	snprintf(data,sizeof(data),"%s,0,%u,%s",subdoc_name,version,COMPONENT_INIT_EVENT);
    	}

        sendWebConfigSignal(data);
}

/*************************************************************************************************************************************
    caller:    Component calls this func during it's initilization

    prototype:

        void
        check_component_crash
            (
				char* init_file
            );

    description :
		Component to call this API, framework will notify subdoc versions to WebConfig client if component is coming after crash
    
    argument:   
		char* init_file
		Init file is the file which RDKB component creates after initialization. 

***************************************************************************************************************************************/

void check_component_crash(char* init_file)
{

	WbInfo(("Inside FUNC %s LINE %d, init file is %s \n",__FUNCTION__,__LINE__,init_file));

	/**
	 * Components based purely on RBUS will not invoke CCSP_Message_Bus_Init.
	 * Check and update rbus_enabled state .
	 */

    	int comp_crashed = 0 ;
	int fd = access(init_file, F_OK); 
    	if(fd == 0)
    	{ 
		WbInfo(("%s file present, component is coming after crash. Need to notify webconfig \n",init_file )); 
        	comp_crashed = 1 ;
        }
        else
        {
                WbInfo(("%s file not present, need to send component init event to webconfig \n",init_file )); 

        }
    	pthread_mutex_lock(&reg_subdoc);
    	PblobRegInfo blobNotify;

    	blobNotify = blobData;

	int i ;
	for (i=0 ; i < gNumOfSubdocs ; i++)
	{
		notifyVersion_to_Webconfig (blobNotify->subdoc_name,blobNotify->version,comp_crashed);
	        blobNotify++;
	}

	pthread_mutex_unlock(&reg_subdoc);
}

/*************************************************************************************************************************************

    caller:    PushBlobRequest

    prototype:

        size_t
        defFunc_calculateTimeout
            (
				size_t numOfEntries
            );

    description :
		
		Function available to calculate the timeout if component doesn't provide the timeout function    
    argument:   
		size_t numOfEntries
		number of entries in a request.
		
	return : timeout value

****************************************************************************************************************************************/

size_t defFunc_calculateTimeout(size_t numOfEntries)
{
	// value in seconds
	return  (DEFAULT_TIMEOUT + (numOfEntries * DEFAULT_TIMEOUT_PER_ENTRY)) ;
}

/*************************************************************************************************************************************

    caller:    PushBlobRequest, messageQueueProcessing

    prototype:

        void
        send_NACK
            (
		char *subdoc_name, 
		uint16_t txid, 
		uint32_t version, 
		uint16_t ErrCode,
		char *failureReason
            );

    description :
		
		Function sends NACK to notify webconfig client that  BLOB execution failed.

    argument:   
		char *subdoc_name --> Name of subdoc

		uint16_t txid    -->  transaction id of the request
		uint32_t version --> version of the request
		uint16_t ErrCode --> Err code why execution failed
		char *failureReason --> Error message why execution failed
		

****************************************************************************************************************************************/

void send_NACK (char *subdoc_name, uint16_t txid, uint32_t version, uint16_t ErrCode,char *failureReason)  
{
		
	WbInfo(("%s : doc name %s , doc version %u, txid is %hu, ErrCode is %hu, Failure reason is %s\n",__FUNCTION__,subdoc_name,version,txid,ErrCode,failureReason));

	char data[256]= {0};
    snprintf(data,sizeof(data),"%s,%hu,%u,NACK,0,%s,%hu,%s",subdoc_name,txid,version,process_name,ErrCode,failureReason);

    sendWebConfigSignal(data);

}

/*************************************************************************************************************************************

    caller:    PushBlobRequest, messageQueueProcessing

    prototype:

        void
        send_ACK
            (
		char *subdoc_name, 
		uint16_t txid, 
		uint32_t version, 
		unsigned long timeout,
            );

    description :
		
		Function sends ACK to notify webconfig client that BLOB execution is success or while notifying timeout value.

    argument:   
		char *subdoc_name --> Name of subdoc

		uint16_t txid    -->  transaction id of the request
		uint32_t version --> version of the request
		unsigned long timeout --> Timeout value needed for blob execution
		
****************************************************************************************************************************************/
void send_ACK (char *subdoc_name, uint16_t txid, uint32_t version, unsigned long timeout,char *msg )
{
	WbInfo(("%s : doc name %s , doc version %u, txid is %hu  timeout is %lu\n",__FUNCTION__,subdoc_name,version,txid,timeout));

	char data[256]= {0};

	if ( msg[0] == '\0' || msg[0] == '0' )
	{
    		snprintf(data,sizeof(data),"%s,%hu,%u,ACK,%lu",subdoc_name,txid,version,timeout);
	}

    	else
    	{
       		snprintf(data,sizeof(data),"%s,%hu,%u,ACK;%s,%lu",subdoc_name,txid,version,msg,timeout);
	
	}

    sendWebConfigSignal(data);

}

/*************************************************************************************************************************************

    caller:    messageQueueProcessing

    prototype:

        void
        updateVersionAndState
            (
		uint32_t version,
		int blob_execution_retValue,
		PblobRegInfo blobUpdate
            );

    description :
		
		This function update subdoc version and state after completion of blob request 

    argument:   
		uint32_t version --> Version to be updated
		int blob_execution_retValue --> if value is BLOB_EXEC_SUCCESS means blob execution is success
		PblobRegInfo blobUpdate  --> Pointer where version value needs to be updated
		
****************************************************************************************************************************************/

void updateVersionAndState(uint32_t version, int blob_execution_retValue,PblobRegInfo blobUpdate)
{
	pthread_mutex_lock(&queue_access);

	if ( blob_execution_retValue == BLOB_EXEC_SUCCESS )
	{
		
		pthread_mutex_lock(&reg_subdoc);

		if (updateVersion != NULL )
	   		 updateVersion(blobUpdate->subdoc_name,version);

	   	blobUpdate->version = version;

	   	 pthread_mutex_unlock(&reg_subdoc);

	    	queueData.blob_state_queue[queueData.front] = COMPLETED;
	}
	else
	{
	    	queueData.blob_state_queue[queueData.front]  = FAILED;
	}

	pthread_mutex_unlock(&queue_access);
}


/*************************************************************************************************************************************

    caller:    messageQueueProcessing

    prototype:

        blobRegInfo
        getAddress
            (
		char* subdoc_name
            );

    description :
		
		Function to get pointer address , required to update the version after successfull completion of blob execution 

    argument:   
		char* subdoc_name -- Name of subdoc 
		
    return :
		returns pointer address of the subdoc name , used to update newer version of subdoc

****************************************************************************************************************************************/
blobRegInfo* getAddress(char* subdoc_name)
{
	pthread_mutex_lock(&reg_subdoc);

	PblobRegInfo blobReturnPointer;
    	blobReturnPointer = blobData;
    	int i;
	for(i = 0 ; i <gNumOfSubdocs ; i++)
	{
		if( strcmp(blobReturnPointer->subdoc_name,subdoc_name) == 0 )
		{
			pthread_mutex_unlock(&reg_subdoc);
	    
			return blobReturnPointer;
		}
		blobReturnPointer++;
	}
	pthread_mutex_unlock(&reg_subdoc);

	return NULL;
}


/*************************************************************************************************************************************

    caller:    addEntryToQueue

    prototype:

        int
        isQueueFull
            (
            );

    description :
		
		Function to check if Queue is full
		
    return :
		returns 1 if queue is full , otherwise returns 0

****************************************************************************************************************************************/

int isQueueFull() 
{  
	if( (queueData.front == queueData.rear + 1) || (queueData.front == 0 && queueData.rear == QUEUE_SIZE-1)) 
	{
		return 1;
	}	

	return 0;
} 

/*************************************************************************************************************************************

    caller:    PushBlobRequest

    prototype:

        int
        addEntryToQueue
            (
		uint32_t version,
		uint16_t txid, 
		unsigned long timeout,
		void *exec_data
            );

    description :
		
		To Add entry to queue which maintains the txid, version and timeout of blob requests

    argument:   
		uint32_t version --> Version number of the new request
		uint16_t txid   -->  transaction id of new request
		unsigned long timeout --> timeout value of new request
		void *exec_data		 --> exec_data pointer address , which will be used to free the memory after blob exec
    return :
		returns 1 if adding entry to queue is success , otherwise returns 0

****************************************************************************************************************************************/

int addEntryToQueue(uint32_t version,uint16_t txid, unsigned long timeout,void *exec_data) 
{ 

	pthread_mutex_lock(&queue_access);

	if (isQueueFull() )
   	{	
   		WbError(("Queue full\n"));
   		pthread_mutex_unlock(&queue_access);

        	return 1; 
   	}

   	if(queueData.front == -1) 
   		queueData.front = 0;

    
   	queueData.rear = (queueData.rear + 1) % QUEUE_SIZE;

   	queueData.txid_queue[queueData.rear] = txid; 
   	queueData.version_queue[queueData.rear] = version ;
   	queueData.timeout_queue[queueData.rear] = timeout;
   	queueData.blob_state_queue[queueData.rear] = PENDING;

	queueData.execDataPointerAddr[queueData.rear] = (execData*) exec_data;

	pthread_mutex_unlock(&queue_access);

	return 0;

} 
  

/*************************************************************************************************************************************

    caller:    checkIfVersionExecInQueue,removeEntryFromQueue

    prototype:

        int
        isQueueEmpty
            (
            );

    description :
		
  		Function to check if queue is empty. 
		
    return :
		returns 1 if queue is empty , otherwise returns 0

****************************************************************************************************************************************/
int isQueueEmpty() 
{  
	if(queueData.front == -1) 
	{	
		return 1;
	}

	return 0;

}
 
/*************************************************************************************************************************************
    caller:    PushBlobRequest

    prototype:

        void
        removeEntryfromRearEnd
            (
            );

    description :
		
		 Function removes entry from rear , required when mq_send call fails 

****************************************************************************************************************************************/
void removeEntryfromRearEnd()
{
	pthread_mutex_lock(&queue_access);

	queueData.txid_queue[queueData.rear] = 0;
	queueData.version_queue[queueData.rear] = 0;
        queueData.timeout_queue[queueData.rear] = 0;
        queueData.blob_state_queue[queueData.rear] = NOT_STARTED;
	queueData.execDataPointerAddr[queueData.rear] = NULL ;

	if (queueData.front == queueData.rear)
    	{
		queueData.front = -1;
       		queueData.rear = -1;
   	}
  	else
  	{
		queueData.rear = (queueData.rear -1) % QUEUE_SIZE;     

       		if (queueData.rear < 0) 
        	{
            		queueData.rear += QUEUE_SIZE;   
        	}
  	}
   	pthread_mutex_unlock(&queue_access);
}

/*************************************************************************************************************************************

    caller:    messageQueueProcessing

    prototype:

        void
        removeEntryFromQueue
            (
            );

    description :
		
		 Function removes entry from queue after blob execution is complete

****************************************************************************************************************************************/

void removeEntryFromQueue() 
{ 
	pthread_mutex_lock(&queue_access);

  	if (isQueueEmpty())
  	{
  	  	WbInfo(("%s queue empty\n",__FUNCTION__));
  	  	pthread_mutex_unlock(&queue_access);

  	  	return ; 

  	}


    	queueData.txid_queue[queueData.front] = 0;
    	queueData.version_queue[queueData.front] = 0;
    	queueData.timeout_queue[queueData.front] = 0;
       	queueData.blob_state_queue[queueData.front] = NOT_STARTED;
       	queueData.execDataPointerAddr[queueData.front] = NULL ;

       	if (queueData.front == queueData.rear)
		{
    	        queueData.front = -1;
                queueData.rear = -1;
        }
  	else
  	{
  		queueData.front = (queueData.front + 1) % QUEUE_SIZE;
  	}

   	pthread_mutex_unlock(&queue_access);

} 

/*************************************************************************************************************************************

    caller:    messageQueueProcessing

    prototype:

        void*
        execute_request
            (
		void *arg            
	    );

    description :
		
		timed thread to execute the blob request

    argument:   
		void *arg  --> The structure which is parsed from the blob. Contains parameters based on feature.

		
    return :
		Value returned from blob execution handler in Err struct format

****************************************************************************************************************************************/

void* execute_request(void *arg)
{

	WbInfo(("Inside FUNC %s LINE %d \n",__FUNCTION__,__LINE__));

	int oldtype;

    	/* allow the thread to be killed at any time */
   	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype);
   	pthread_detach(pthread_self());


    	execData *exec_data  = (execData*) arg;

	execReturn = exec_data->executeBlobRequest(exec_data->user_data);

   	/* wake up the caller if execution is completed in time */
    	pthread_cond_signal(&webconfig_exec_completed);
   	return NULL;
}

/*************************************************************************************************************************************

    caller:    initMessageQueue

    prototype:

        void*
        messageQueueProcessing
            (
            );

    description :
		
		Thread to monitor all blob execution requests (message queue).
		After receiving the events this thread will create timed thread for blob execution. 
		Timeout value is MAX_FUNC_EXEC_TIMEOUT * timeout value returned by calcTimeout. 
		
		If execution is completed within the timeout value specified for thread, 
		messageQueueProcessing will update the version and then call send the ACK with timeout as 0. 
		If execution returns failure within timeout value, then NACK is sent.
		If execution fails to complete within timeout, then thread execution is cancelled, and NACK is sent.

		If thread creation for blob execution fails, execution handler is called directly
****************************************************************************************************************************************/

void* messageQueueProcessing()
{
	WbInfo(("Inside FUNC %s LINE %d \n",__FUNCTION__,__LINE__));

	blobRegInfo *blobDataProcessing;

	//char *queue_name = (char*) arg;
	pthread_detach(pthread_self());

    	struct timespec abs_time;

    	pthread_t tid;
    	int err, pthreadRetValue,rollbkRet;

	mqd_t mq;
	struct mq_attr attr;
    	char buffer[MAX_SIZE + 1];

    	/* initialize the queue attributes */
   	 attr.mq_flags = 0;
    	attr.mq_maxmsg = MAX_EVENTS_IN_MQUEUE;
    	attr.mq_msgsize = MAX_SIZE;
    	attr.mq_curmsgs = 0;
    	errno = 0;
    	/* create the message queue */
    	mq_unlink(mqEventName);
    	mq = mq_open(mqEventName, O_CREAT | O_RDONLY, 0644, &attr);
    	if ( (mqd_t)-1 == mq )
    	{
    		WbError(("message queue open failed , ERROR : %s. Returning from %s\n",strerror(errno),__FUNCTION__));
    		return NULL;

    	}

 	pthread_condattr_init(&webconfig_attr);
	pthread_condattr_setclock(&webconfig_attr, CLOCK_MONOTONIC);
	pthread_cond_init(&webconfig_exec_completed, &webconfig_attr);

    	while(1)
    	{

    		memset(&abs_time, 0, sizeof(abs_time));
    		memset(buffer,0,sizeof(buffer));

    		blobDataProcessing = NULL;
    		err = 0 ; pthreadRetValue = 0; rollbkRet = 0 ;
			errno = 0;

    		ssize_t bytes_read;

    		/* receive the message */
     		bytes_read = mq_receive(mq, buffer, MAX_SIZE, NULL);

     		if ( bytes_read >= 0 ) 
     		{
	      		
			buffer[bytes_read] = '\0';
	        	// Processing pending
	       		WbInfo(("Received event\n"));

	        	pthread_mutex_lock(&webconfig_exec);

		       	execData *exec_data  = (execData*) buffer;

		       	blobDataProcessing=getAddress(exec_data->subdoc_name);

			pthread_mutex_lock(&queue_access);

			queueData.blob_state_queue[queueData.front] = IN_PROGRESS;

			pthread_mutex_unlock(&queue_access);

		        if ( exec_data->executeBlobRequest )
			{
			 
				pthreadRetValue=pthread_create(&tid, NULL, execute_request,(void*)buffer);

			        if ( 0 == pthreadRetValue )
			        {

				       	clock_gettime(CLOCK_MONOTONIC, &abs_time);

				       	abs_time.tv_sec += MAX_FUNC_EXEC_TIMEOUT * queueData.timeout_queue[queueData.front] ;
				       	abs_time.tv_nsec += 0;

		        		err = pthread_cond_timedwait(&webconfig_exec_completed, &webconfig_exec, &abs_time);

		        		if (err == ETIMEDOUT)
		        		{
		        			pthread_cancel(tid);

		           			WbError(("%s: subdoc %s , txid %hu , version %u execution timedout\n", __FUNCTION__,exec_data->subdoc_name,queueData.txid_queue[queueData.front],exec_data->version));

						updateVersionAndState(exec_data->version,0,blobDataProcessing);
                                                if(exec_data->disableWebCfgNotification != 1)
                                                {
			            		        send_NACK(exec_data->subdoc_name,queueData.txid_queue[queueData.front],exec_data->version,BLOB_EXECUTION_TIMEDOUT,"Blob Execution Timedout");
                                                }
			            		if ( exec_data->rollbackFunc )
						{
							rollbkRet = exec_data->rollbackFunc();

							if (rollbkRet != ROLLBACK_SUCCESS )
			            				WbError(("%s : Rolling back of data failed\n",__FUNCTION__));
			            		
						}

				        }
				        	
				        else
				        {
	
			        		if ( BLOB_EXEC_SUCCESS == execReturn->ErrorCode )
			        		{
							WbInfo(("%s : Execution success , sending completed ACK\n",__FUNCTION__));
							updateVersionAndState(exec_data->version,execReturn->ErrorCode,blobDataProcessing);
                                                        if(exec_data->disableWebCfgNotification != 1)
                                                        {
							        send_ACK(exec_data->subdoc_name,queueData.txid_queue[queueData.front],exec_data->version,0,execReturn->ErrorMsg);
                                                        }
				        	}
				        	else
						{
							WbError(("%s : Execution failed Error Code :%hu Reason: %s \n",__FUNCTION__,execReturn->ErrorCode,execReturn->ErrorMsg));
							updateVersionAndState(exec_data->version,execReturn->ErrorCode,blobDataProcessing);
                                                        if(exec_data->disableWebCfgNotification != 1)
                                                        {
							        send_NACK(exec_data->subdoc_name,queueData.txid_queue[queueData.front],exec_data->version,execReturn->ErrorCode,execReturn->ErrorMsg);
                                                        }
			                                if ( (exec_data->rollbackFunc) && ( VALIDATION_FALIED != execReturn->ErrorCode ) )
                                               		{	
								rollbkRet = exec_data->rollbackFunc();

								if (rollbkRet != ROLLBACK_SUCCESS )
			            				WbError(("%s : Rolling back of data failed\n",__FUNCTION__));
							}

						}

		        		}
		        	}	

			        else
			        {
			        	WbError(("%s: execute_request pthread_create failed , ERROR : %s \n", __FUNCTION__,strerror(errno)));

				        execReturn = exec_data->executeBlobRequest(exec_data->user_data);

				        if ( BLOB_EXEC_SUCCESS == execReturn->ErrorCode )
				        {
						WbInfo(("%s : Execution success , sending completed ACK\n",__FUNCTION__));
						updateVersionAndState(exec_data->version,execReturn->ErrorCode,blobDataProcessing);
                                                if(exec_data->disableWebCfgNotification != 1)
                                                {
						        send_ACK(exec_data->subdoc_name,queueData.txid_queue[queueData.front],exec_data->version,0,execReturn->ErrorMsg);
                                                }
				        }
				        else
					{

						WbError(("%s : Execution failed Error Code :%hu Reason: %s \n",__FUNCTION__,execReturn->ErrorCode,execReturn->ErrorMsg));
						updateVersionAndState(exec_data->version,execReturn->ErrorCode,blobDataProcessing);
                                                if(exec_data->disableWebCfgNotification != 1)
                                                {
						        send_NACK(exec_data->subdoc_name,queueData.txid_queue[queueData.front],exec_data->version,execReturn->ErrorCode,execReturn->ErrorMsg);
                                                }
                                                if ( (exec_data->rollbackFunc) && ( VALIDATION_FALIED != execReturn->ErrorCode ) )
                                       		{
							rollbkRet = exec_data->rollbackFunc();

							if (rollbkRet != ROLLBACK_SUCCESS )
			            				WbError(("%s : Rolling back of data failed\n",__FUNCTION__));
						}
					}
				}

			}
		    	else
			{	
				WbError(("%s executeBlobRequest function pointer is NULL , Send NACK\n",__FUNCTION__));
				updateVersionAndState(exec_data->version,execReturn->ErrorCode,blobDataProcessing);
                                if(exec_data->disableWebCfgNotification != 1)
                                {
				        send_NACK(exec_data->subdoc_name,queueData.txid_queue[queueData.front],exec_data->version,NULL_BLOB_EXEC_POINTER,"Null Execution Pointer passed");
                                }
			}


				if ( exec_data->freeResources ) 
					exec_data->freeResources(queueData.execDataPointerAddr[queueData.front]);

				if ( execReturn != NULL)
				{
					free(execReturn);
					execReturn = NULL;
				}

				removeEntryFromQueue();

		    	        pthread_mutex_unlock(&webconfig_exec);
	    	}    	
   	}
	return NULL;
}

/*************************************************************************************************************************************

    caller:    register_sub_docs

    prototype:

        void
        initMessageQueue
            (
            );

    description :
		
		Function creates 1 thread to monitor webconfig requests and in case of multi comp support this function creeats 2 other 
		message queue monitor threads for master and slave execution


****************************************************************************************************************************************/

void initMessageQueue()

{
	WbInfo(("Inside FUNC %s LINE %d \n",__FUNCTION__,__LINE__));

		pthread_t tid;

    	FILE  *pFile      = NULL;

    	char command[32]= {0};
    	snprintf(command,sizeof(command),"cat /proc/%d/comm",getpid());

    	pFile = popen(command, "r");
    	if(pFile)
    	{
        	fgets(process_name, sizeof(process_name), pFile);
        	pclose(pFile);
        	pFile = NULL ;
        	char* pos;
        	if(process_name[0] != '\0')
        	{
           		if ( ( pos = strchr( process_name, '\n' ) ) != NULL ) {
               	 	*pos = '\0';
              		}
        	}

     	}

     	int ret;

     	snprintf(mqEventName,sizeof(mqEventName), "%s-%s",WEBCONFIG_QUEUE_NAME,process_name);
  
		ret = pthread_create(&tid, NULL, &messageQueueProcessing, NULL); 

		if ( ret != 0 )
			WbError(("%s: messageQueueProcessing pthread_create failed , ERROR : %s \n", __FUNCTION__,strerror(errno)));    

}

/*************************************************************************************************************************************

    caller:    checkNewVersionUpdateRequired

    prototype:

        int
        checkIfVersionExecInQueue
            (
		uint32_t version,
		int *queueIndex
            );

    description :
		
		Function returns if version execution is in queue , if version in queue already then just need to update TX ID 

    argument:   
		uint32_t version --> version number of new request
		int *queueIndex	 --> function need to fill this data and return to caller, caller will use this index to update the 
		new transaction id info

    return :
		returns VERSION_UPDATE_REQUIRED is subdoc has newer version , returns EXECUTION_IN_QUEUE is exec is in queue already

****************************************************************************************************************************************/

int checkIfVersionExecInQueue(uint32_t version,int *queueIndex)
{
	pthread_mutex_lock(&queue_access);

  	if (isQueueEmpty())
  	{
  		WbInfo(("%s : Queue Empty\n",__FUNCTION__));
		pthread_mutex_unlock(&queue_access);

  	  	return VERSION_UPDATE_REQUIRED ; 

  	}

	int i ;

    	for( i = queueData.front; i!=queueData.rear; i=(i+1)%QUEUE_SIZE) 
    	{

		if ( version == queueData.version_queue[i] )
		{
	        	*queueIndex = i;
	        	pthread_mutex_unlock(&queue_access);

	        	return EXECUTION_IN_QUEUE ;
	   	 }	
    	}


	if ( version == queueData.version_queue[i] )
	{
	    	*queueIndex = i;
	     	pthread_mutex_unlock(&queue_access);

		return EXECUTION_IN_QUEUE ;
	}

	pthread_mutex_unlock(&queue_access);

    return VERSION_UPDATE_REQUIRED; 
}


/*************************************************************************************************************************************

    caller:    PushBlobRequest

    prototype:

        int
        checkNewVersionUpdateRequired
            (
		execData *exec_data,
		int *queueIndex
            );

    description :
		
		Function to check if version update is required, if version is already exist or request in queue then no need to push the request to message queue 


    argument:   
		execData *exec_data 	--> which struct has version and transaction id details
		int *queueIndex		--> checkIfVersionExecInQueue function need to fill this data and return to caller, caller will use this index to update the 
					new transaction id info
    return :
		returns VERSION_UPDATE_REQUIRED is subdoc has newer version , returns VERSION_ALREADY_EXIST if exec already exists
		also returns SUBDOC_NOT_SUPPORTED if subdoc is not supported

****************************************************************************************************************************************/

int checkNewVersionUpdateRequired(execData *exec_data,int *queueIndex)
{
	WbInfo(("Inside FUNC %s LINE %d \n",__FUNCTION__,__LINE__));
	int i =0 ;
        FILE *fp;

    	pthread_mutex_lock(&reg_subdoc);

	PblobRegInfo blobCheckVersion;;

	blobCheckVersion = blobData;

	for (i=0 ; i < gNumOfSubdocs ; i++)
	{
	        if ( strcmp(blobCheckVersion->subdoc_name,exec_data->subdoc_name) == 0)
	        {
			if ( (uint32_t)blobCheckVersion->version == exec_data->version )
	        	{
                    pthread_mutex_unlock(&reg_subdoc);
                    if ( ( strcmp (exec_data->subdoc_name,"hotspot") == 0 ) && ((fp=fopen (HOTSPOT_VERSION_IGNORE, "r")) != 0))//CID 172860: Time of check time of use (TOCTOU)
                    {
                            unlink(HOTSPOT_VERSION_IGNORE);
                            fclose(fp);
                            return VERSION_UPDATE_REQUIRED ; 
                    }
	        		return VERSION_ALREADY_EXIST;
	        	}

	        	else 
	        	{	 
	        		pthread_mutex_unlock(&reg_subdoc);
	#ifdef WBCFG_MULTI_COMP_SUPPORT

	        		if (exec_data->multiCompRequest == 0 )

	        			return (checkIfVersionExecInQueue(exec_data->version,queueIndex) );
	        		else
	        			return (checkIfVersionExecInMultiCompQueue(exec_data->version,queueIndex) );
	#else
	        		
	        		return (checkIfVersionExecInQueue(exec_data->version,queueIndex) );
	
	#endif
	        	}

	        }

	     blobCheckVersion++;   
	}

	pthread_mutex_unlock(&reg_subdoc);

	return SUBDOC_NOT_SUPPORTED;
}


/*************************************************************************************************************************************

    caller:    PushBlobRequest

    prototype:

        size_t
        getPendingQueueTimeout
            (
		uint16_t txid
            );

    description :
		
		Function to calculate queue timeout , need to add all queue timeout to subdoc execution time when requests are in queue 

    argument:   
		uint16_t txid -->transaction id number
		
    return :
		returns pending timeout value 
****************************************************************************************************************************************/

size_t getPendingQueueTimeout(uint16_t txid)
{
	int queueTimeout = 0;

	pthread_mutex_lock(&queue_access);

  	if (isQueueEmpty())
  	{
  		WbInfo(("%s : Queue Empty\n",__FUNCTION__));
		pthread_mutex_unlock(&queue_access);

  	  	return queueTimeout ; 

  	}
	int i ;
	for( i = queueData.front; i!=queueData.rear; i=(i+1)%QUEUE_SIZE) 
   	{

    		if ( queueData.txid_queue[i] == txid )
    		{
			pthread_mutex_unlock(&queue_access);
    			return queueTimeout;
    		} 

        	queueTimeout += queueData.timeout_queue[i];
    	}

    	if ( queueData.txid_queue[i] == txid )
    	{
		pthread_mutex_unlock(&queue_access);
    		return queueTimeout;
    	}
	queueTimeout += queueData.timeout_queue[i];

	pthread_mutex_unlock(&queue_access);

    return queueTimeout; 
}



/*************************************************************************************************************************************

    caller:    componenent calls this function after receivng the blob request(single component)

    prototype:

        void
        PushBlobRequest
            (
		execData *exec_data
            );

    description :
		
		Function to decide and push the blob execution request to message queue 

    argument:   
		execData *exec_data	--> Component fills this structure , which has all the info needed to execute the blob request
    return :
		returns pointer address of the subdoc name , used to update newer version of subdoc

****************************************************************************************************************************************/

void PushBlobRequest (execData *exec_data )  
{

	WbInfo(("%s : subdoc_name %s , txid %hu, version %u , entries %zu\n",__FUNCTION__,exec_data->subdoc_name,exec_data->txid,exec_data->version,exec_data->numOfEntries));

	unsigned long timeout = 0;
	unsigned long timeout_to_webconfig = 0 ;
    	mqd_t mq;


    	/* open the message queue */


    	int queueIndex=0;

	int retVal =  checkNewVersionUpdateRequired(exec_data,&queueIndex);

	if ( VERSION_UPDATE_REQUIRED == retVal ) 
	{
		WbInfo(("New version available , prcessing new Blob request\n"));
    		mq = mq_open(mqEventName, O_WRONLY);
    		//CHECK((mqd_t)-1 != mq);
        	if ( (mqd_t)-1 == mq )
    		{
    			WbError(("%s message queue open failed , ERROR : %s\n",__FUNCTION__,strerror(errno)));
                        if(exec_data->disableWebCfgNotification != 1)
                        {
			        send_NACK(exec_data->subdoc_name,exec_data->txid,exec_data->version,MQUEUE_OPEN_FAILED,"MQ OPEN FAILED");
                        }
	  		
                        goto EXIT;
    		}

		if (!exec_data->calcTimeout)
		{
			WbInfo(("calculateTimeout is NULL , using default timeout routine\n"));
			exec_data->calcTimeout = defFunc_calculateTimeout;
		}
		
			timeout = exec_data->calcTimeout(exec_data->numOfEntries);
			WbInfo(("%s timeout received from calcTimeout is %lu\n",__FUNCTION__,timeout));

			timeout_to_webconfig = timeout + (getPendingQueueTimeout(exec_data->txid));

		if (! (addEntryToQueue(exec_data->version,exec_data->txid,timeout,exec_data)) )
		{

			#if 0
			if (mq_getattr(mq, &attr) == 0)
		        {

				if ( attr.mq_curmsgs > 0 )
				timeout = (unsigned int) (timeout * attr.mq_curmsgs) ;

				WbInfo(("Num of events in queue are %lu\n",attr.mq_curmsgs));
	     		}

			#endif
                        if(exec_data->disableWebCfgNotification != 1)
                        {	
	
			        WbInfo(("%s : Send received request ACK , timeout is %lu\n",__FUNCTION__,timeout_to_webconfig));

			        send_ACK(exec_data->subdoc_name,exec_data->txid,exec_data->version,timeout_to_webconfig,"");
                        }
			
            		if ( 0 != mq_send(mq, (char*) exec_data, sizeof(*exec_data), 0))
                        {
                 		WbError(("%s message queue send failed , ERROR is : %s\n",__FUNCTION__,strerror(errno)));
                                if(exec_data->disableWebCfgNotification != 1)
                                {
                		        send_NACK(exec_data->subdoc_name,exec_data->txid,exec_data->version,MQUEUE_SEND_FAILED,"MQ SEND FAILED");
                                }
                 		removeEntryfromRearEnd();
                 		if ((0 != mq_close(mq)))
            				WbError(("%s message queue close failed , ERROR is : %s\n",__FUNCTION__,strerror(errno)));
                 		goto EXIT;
					}


			if ((0 != mq_close(mq)))
            			WbError(("%s message queue close failed , ERROR is : %s\n",__FUNCTION__,strerror(errno)));

            		return ;

		}

		else
		{
			WbError(("%s QUEUE FULL\n",__FUNCTION__));
                        if(exec_data->disableWebCfgNotification != 1)
                        {
			        send_NACK(exec_data->subdoc_name,exec_data->txid,exec_data->version,QUEUE_PUSH_FAILED,"Queue is Full");
                        }
		}

		if ((0 != mq_close(mq)))
            		WbError(("%s message queue close failed , ERROR is : %s\n",__FUNCTION__,strerror(errno)));


	}
	else if ( EXECUTION_IN_QUEUE == retVal )
	{
		WbInfo((" %s : Execution is in progress, updating transaction id\n",__FUNCTION__));
		pthread_mutex_lock(&queue_access);

		queueData.txid_queue[queueIndex] = exec_data->txid;
		pthread_mutex_unlock(&queue_access);

		
		if (!exec_data->calcTimeout)
		{
			WbInfo(("calculateTimeout is NULL , using default timeout routine\n"));
			exec_data->calcTimeout = defFunc_calculateTimeout;
		}
		
		timeout = exec_data->calcTimeout(exec_data->numOfEntries);

		WbInfo(("%s timeout received from calcTimeout is %lu\n",__FUNCTION__,timeout));

	   	timeout_to_webconfig = timeout + (getPendingQueueTimeout(exec_data->txid));
                if(exec_data->disableWebCfgNotification != 1)
                {
		        WbInfo(("%s : Send received request ACK , timeout is %lu\n",__FUNCTION__,timeout_to_webconfig));

		        send_ACK(exec_data->subdoc_name,exec_data->txid,exec_data->version,timeout_to_webconfig,"");
                }

	}
	else if ( VERSION_ALREADY_EXIST == retVal )
	{
		WbInfo(("Already having updated version, no need to prcess Blob request\n"));
                if(exec_data->disableWebCfgNotification != 1)
                {
		        send_ACK(exec_data->subdoc_name,exec_data->txid,exec_data->version,0,"");
                }
	}

	else if ( SUBDOC_NOT_SUPPORTED == retVal )
	{
		WbError(("Subdoc not registered , support not available . sending NACK\n"));
                if(exec_data->disableWebCfgNotification != 1)
                {
		        send_NACK(exec_data->subdoc_name,exec_data->txid,exec_data->version,SUBDOC_NOT_SUPPORTED,"INVALID SUBDOC");
                }
	}

EXIT : 
	if ( exec_data->freeResources ) 
		exec_data->freeResources(exec_data);
}

int resetSubdocVersion(char* subdoc_name) {

	pthread_mutex_lock(&reg_subdoc);
	PblobRegInfo blobResetVersion;;
	blobResetVersion = blobData;
	int i;

	for (i = 0; i < gNumOfSubdocs; i++)
	{
		if (0 == strncmp(subdoc_name, blobResetVersion->subdoc_name, strlen(blobResetVersion->subdoc_name))) {
			WbInfo(("%s : Resetting subdoc version for '%s'\n",__FUNCTION__, subdoc_name));
			if (updateVersion != NULL ) {
	   			updateVersion(blobResetVersion->subdoc_name,0);
				blobResetVersion->version = 0;
				pthread_mutex_unlock(&reg_subdoc);
				WbInfo(("%s : Subdoc version reset success for '%s'\n",__FUNCTION__, subdoc_name));
				return 0;
			}
		}
		blobResetVersion++;
	}
	pthread_mutex_unlock(&reg_subdoc);
	return 1;
}
