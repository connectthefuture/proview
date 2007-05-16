/* 
 * Proview   $Id: rt_statussrv.cpp,v 1.2 2007-05-16 12:32:55 claes Exp $
 * Copyright (C) 2005 SSAB Oxel�sund AB.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation, either version 2 of 
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with the program, if not, write to the Free Software 
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <map.h>
#include <iostream.h>
#include <fstream.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <pthread.h>

#include "pwr.h"
#include "pwr_version.h"
#include "pwr_baseclasses.h"
#include "co_cdh.h"
#include "co_time.h"
#include "co_msg.h"
#include "co_dcli.h"
#include "rt_gdh.h"
#include "rt_errh.h"
#include "rt_qcom.h"
#include "rt_ini_event.h"
#include "rt_aproc.h"
#include "rt_pwr_msg.h"
#include "rt_qcom_msg.h"
#include "statussrv_Stub.h"
#include "Service.nsmap"

// Application offset and size in nodeobject
#define APPL_OFFSET 20
#define APPL_SIZE 20

class status_server {
 public:
  status_server() : m_grant_all(true), m_config(0), m_node(0), qid(qcom_cNQid) {}

  bool m_grant_all;
  pwr_sNode *m_config;
  pwr_sNode *m_node;
  qcom_sQid qid;
  char version[20];
  pwr_tObjName m_appl[APPL_SIZE];
};


static status_server *statussrv;
static void *statussrv_cyclic( void *arg);


int main(  int argc, char *argv[])
{
  struct soap soap;
  int m,s;   // Master and slave sockets
  pwr_tStatus sts;
  pwr_tOid node_oid;
  qcom_sQid qini;
  qcom_sQattr qAttr;
  qcom_sQid qid = qcom_cNQid;
  int restarts = 10;
  int ignore_config = 0;
  
  for ( int i = 1; i < argc; i++) {
    if ( strcmp( argv[i], "-i") == 0)
      ignore_config = 1;
  }

  sts = gdh_Init("status_server");
  if ( EVEN(sts)) {
    exit(sts);
  }

  errh_Init("status_server", errh_eAnix_statussrv);
  errh_SetStatus( PWR__SRVSTARTUP);

  if (!qcom_Init(&sts, 0, "status_server")) {
    errh_Fatal("qcom_Init, %m", sts); 
    errh_SetStatus( PWR__SRVTERM);
   exit(sts);
  } 

  qAttr.type = qcom_eQtype_private;
  qAttr.quota = 100;
  if (!qcom_CreateQ(&sts, &qid, &qAttr, "events")) {
    errh_Fatal("qcom_CreateQ, %m", sts);
    errh_SetStatus( PWR__SRVTERM);
    exit(sts);
  } 

  qini = qcom_cQini;
  if (!qcom_Bind(&sts, &qid, &qini)) {
    errh_Fatal("qcom_Bind(Qini), %m", sts);
    errh_SetStatus( PWR__SRVTERM);
    exit(-1);
  }


  statussrv = new status_server();
  statussrv->qid = qid;

  // Link to $Node object
  sts = gdh_GetNodeObject( 0, &node_oid);
  if ( EVEN(sts)) {
    errh_SetStatus( sts);
    exit(-1);
  }

  sts = gdh_ObjidToPointer( node_oid, (void **)&statussrv->m_node);
  if ( EVEN(sts)) {
    errh_SetStatus( sts);
    exit(-1);
  }

  // Get application names
  for ( int i = 0; i < APPL_SIZE; i++) {
    if ( cdh_ObjidIsNotNull( statussrv->m_node->ProcObject[i+APPL_OFFSET])) {
      sts = gdh_ObjidToName( statussrv->m_node->ProcObject[i+APPL_OFFSET], 
			     statussrv->m_appl[i], 
			     sizeof( statussrv->m_appl[0]), cdh_mName_object);
      if ( EVEN(sts))
	strcpy( statussrv->m_appl[i], "");
    }
    else
      strcpy( statussrv->m_appl[i], "");
  }


  if ( !ignore_config) {
    // Get StatusServerConfig object
    pwr_tOid config_oid;
    sts = gdh_GetClassList( pwr_cClass_StatusServerConfig, &config_oid);
    if ( EVEN(sts)) {
      // Not configured
      errh_SetStatus( 0);
      exit(sts);
    }
    
    sts = gdh_ObjidToPointer( config_oid, (void **)&statussrv->m_config);
    if ( EVEN(sts)) {
      errh_SetStatus( sts);
      exit(sts);
    }
    
    aproc_RegisterObject( config_oid);
  }

  // Read version file

  char		buff[100];
  pwr_tFileName fname;

  dcli_translate_filename( fname, "$pwr_exe/rt_version.dat");

  ifstream	fp( fname);

  if ( fp) {
    fp.getline( buff, sizeof(buff));
    strcpy( statussrv->version, "V");
    strcat( statussrv->version, &buff[9]);
    fp.close();
  }
  else
    strcpy( statussrv->version, "");

  // Create a cyclic tread to receive swap and terminate events
  pthread_t 	thread;
  sts = pthread_create( &thread, NULL, statussrv_cyclic, NULL);

  errh_SetStatus( PWR__SRUN);

  soap_init( &soap);
  for ( int i = 0; i < restarts + 1; i++) {
    m = soap_bind( &soap, NULL, 18084, 100);
    if ( m < 0) {
      if ( i == restarts) {
	soap_print_fault( &soap, stderr);
	break;
      }
      printf( "Soap bind failed, retrying...\n");
      sleep( 10);
    }
    else {
      // fprintf( stderr, "Socket connection successfull: master socket = %d\n", m);

      for ( int i = 1;; i++) {
	s = soap_accept( &soap);
	if ( s < 0) {
	  soap_print_fault( &soap, stderr);
	  break;
	}

	// fprintf( stderr, "%d: request from IP=%lu.%lu.%lu.%lu socket=%d\n", i,
	//	  (soap.ip>>24)&0xFF,(soap.ip>>16)&0xFF,(soap.ip>>8)&0xFF,soap.ip&0xFF, s);

	if ( soap_serve( &soap) != SOAP_OK)         // Process RPC request
	  soap_print_fault( &soap, stderr);
	soap_destroy( &soap);   // Clean up class instances
	soap_end( &soap);       // Clean up everything and close socket
      }
    }
  }

  soap_done( &soap);     // Close master socket and detach environment
  return SOAP_OK;
}

static void *statussrv_cyclic( void *arg)
{
  pwr_tTime current_time;
  int tmo = 2000;
  char mp[2000];
  qcom_sGet get;
  pwr_tStatus sts;

  for (;;) {

    clock_gettime( CLOCK_REALTIME, &current_time);
    aproc_TimeStamp();

    get.maxSize = sizeof(mp);
    get.data = mp;
    qcom_Get( &sts, &statussrv->qid, &get, tmo);
    if (sts == QCOM__TMO || sts == QCOM__QEMPTY) {
      // Do nothing...
    }
    else {
      ini_mEvent  new_event;
      qcom_sEvent *ep = (qcom_sEvent*) get.data;

      new_event.m  = ep->mask;
      if (new_event.b.oldPlcStop) {
	errh_SetStatus( PWR__SRVRESTART);
      } else if (new_event.b.swapDone) {
	errh_SetStatus( PWR__SRUN);
      } else if (new_event.b.terminate) {
	exit(0);
      }
    }
  }
}

void statussrv_GetText( pwr_tStatus sts, char *buf, int size)
{
  if ( sts == 0)
    strcpy( buf, "-");
  else
    msg_GetText( sts, buf, size);
}

SOAP_FMAC5 int SOAP_FMAC6 __s0__GetStatus(struct soap *soap, 
					  _s0__GetStatus *s0__GetStatus, 
					  _s0__GetStatusResponse *s0__GetStatusResponse)
{
  pwr_tTime current_time;
  char msg[200];
  char timstr[40];

  clock_gettime( CLOCK_REALTIME, &current_time);

  if ( s0__GetStatus->ClientRequestHandle)
    s0__GetStatusResponse->ClientRequestHandle = 
      new std::string( *s0__GetStatus->ClientRequestHandle);

  if ( strcmp( statussrv->version, "") != 0)
    s0__GetStatusResponse->Version = std::string( statussrv->version);
  else
    s0__GetStatusResponse->Version = std::string( pwrv_cPwrVersionStr);
  s0__GetStatusResponse->SystemStatus = statussrv->m_node->SystemStatus;
  statussrv_GetText( statussrv->m_node->SystemStatus, msg, sizeof(msg));
  s0__GetStatusResponse->SystemStatusStr = std::string( msg);
  s0__GetStatusResponse->Description = new std::string( statussrv->m_node->Description);
  time_AtoAscii( &statussrv->m_node->SystemTime, time_eFormat_DateAndTime, timstr, sizeof(timstr));
  s0__GetStatusResponse->SystemTime = new std::string( timstr);
  time_AtoAscii( &statussrv->m_node->BootTime, time_eFormat_DateAndTime, timstr, sizeof(timstr));
  s0__GetStatusResponse->BootTime = new std::string( timstr);
  time_AtoAscii( &statussrv->m_node->RestartTime, time_eFormat_DateAndTime, timstr, sizeof(timstr));
  s0__GetStatusResponse->RestartTime = new std::string( timstr);
  s0__GetStatusResponse->Restarts = (int *) malloc( sizeof(s0__GetStatusResponse->Restarts));
  *s0__GetStatusResponse->Restarts = statussrv->m_node->Restarts;
  return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6 __s0__GetExtStatus(struct soap *soap, 
					     _s0__GetExtStatus *s0__GetExtStatus, 
					     _s0__GetExtStatusResponse *s0__GetExtStatusResponse)
{
  char msg[200];


  if ( s0__GetExtStatus->ClientRequestHandle)
    s0__GetExtStatusResponse->ClientRequestHandle = 
      new std::string( *s0__GetExtStatus->ClientRequestHandle);

  s0__GetExtStatusResponse->ServerSts1 = statussrv->m_node->ProcStatus[0];
  statussrv_GetText( statussrv->m_node->ProcStatus[0], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts1Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts1Name = std::string( "rt_ini");

  s0__GetExtStatusResponse->ServerSts2 = statussrv->m_node->ProcStatus[1];
  statussrv_GetText( statussrv->m_node->ProcStatus[1], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts2Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts2Name = std::string( "rt_qmon");

  s0__GetExtStatusResponse->ServerSts3 = statussrv->m_node->ProcStatus[2];
  statussrv_GetText( statussrv->m_node->ProcStatus[2], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts3Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts3Name = std::string( "rt_neth");

  s0__GetExtStatusResponse->ServerSts4 = statussrv->m_node->ProcStatus[3];
  statussrv_GetText( statussrv->m_node->ProcStatus[3], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts4Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts4Name = std::string( "rt_neth_acp");

  s0__GetExtStatusResponse->ServerSts5 = statussrv->m_node->ProcStatus[4];
  statussrv_GetText( statussrv->m_node->ProcStatus[4], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts5Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts5Name = std::string( "rt_io");

  s0__GetExtStatusResponse->ServerSts6 = statussrv->m_node->ProcStatus[5];
  statussrv_GetText( statussrv->m_node->ProcStatus[5], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts6Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts6Name = std::string( "rt_tmon");

  s0__GetExtStatusResponse->ServerSts7 = statussrv->m_node->ProcStatus[6];
  statussrv_GetText( statussrv->m_node->ProcStatus[6], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts7Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts7Name = std::string( "rt_emon");

  s0__GetExtStatusResponse->ServerSts8 = statussrv->m_node->ProcStatus[7];
  statussrv_GetText( statussrv->m_node->ProcStatus[7], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts8Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts8Name = std::string( "rt_alim");

  s0__GetExtStatusResponse->ServerSts9 = statussrv->m_node->ProcStatus[8];
  statussrv_GetText( statussrv->m_node->ProcStatus[8], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts9Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts9Name = std::string( "rt_bck");

  s0__GetExtStatusResponse->ServerSts10 = statussrv->m_node->ProcStatus[9];
  statussrv_GetText( statussrv->m_node->ProcStatus[9], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts10Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts10Name = std::string( "rt_linksup");

  s0__GetExtStatusResponse->ServerSts11 = statussrv->m_node->ProcStatus[10];
  statussrv_GetText( statussrv->m_node->ProcStatus[10], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts11Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts11Name = std::string( "rt_trend");

  s0__GetExtStatusResponse->ServerSts12 = statussrv->m_node->ProcStatus[11];
  statussrv_GetText( statussrv->m_node->ProcStatus[11], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts12Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts12Name = std::string( "rt_fast");

  s0__GetExtStatusResponse->ServerSts13 = statussrv->m_node->ProcStatus[12];
  statussrv_GetText( statussrv->m_node->ProcStatus[12], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts13Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts13Name = std::string( "rt_elog");

  s0__GetExtStatusResponse->ServerSts14 = statussrv->m_node->ProcStatus[13];
  statussrv_GetText( statussrv->m_node->ProcStatus[13], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts14Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts14Name = std::string( "rt_webmon");

  s0__GetExtStatusResponse->ServerSts15 = statussrv->m_node->ProcStatus[14];
  statussrv_GetText( statussrv->m_node->ProcStatus[14], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts15Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts15Name = std::string( "rt_webmonmh");

  s0__GetExtStatusResponse->ServerSts16 = statussrv->m_node->ProcStatus[15];
  statussrv_GetText( statussrv->m_node->ProcStatus[15], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts16Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts16Name = std::string( "sysmon");

  s0__GetExtStatusResponse->ServerSts17 = statussrv->m_node->ProcStatus[16];
  statussrv_GetText( statussrv->m_node->ProcStatus[16], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts17Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts17Name = std::string( "plc");

  s0__GetExtStatusResponse->ServerSts18 = statussrv->m_node->ProcStatus[17];
  statussrv_GetText( statussrv->m_node->ProcStatus[17], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts18Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts18Name = std::string( "rs_remote");

  s0__GetExtStatusResponse->ServerSts19 = statussrv->m_node->ProcStatus[18];
  statussrv_GetText( statussrv->m_node->ProcStatus[18], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts19Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts19Name = std::string( "opc_server");

  s0__GetExtStatusResponse->ServerSts20 = statussrv->m_node->ProcStatus[19];
  statussrv_GetText( statussrv->m_node->ProcStatus[19], msg, sizeof(msg));
  s0__GetExtStatusResponse->ServerSts20Str = std::string( msg);
  s0__GetExtStatusResponse->ServerSts20Name = std::string( "rt_statussrv");

  s0__GetExtStatusResponse->ApplSts1 = statussrv->m_node->ProcStatus[20];
  statussrv_GetText( statussrv->m_node->ProcStatus[20], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts1Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts1Name = std::string( statussrv->m_appl[0]);

  s0__GetExtStatusResponse->ApplSts2 = statussrv->m_node->ProcStatus[21];
  statussrv_GetText( statussrv->m_node->ProcStatus[21], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts2Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts2Name = std::string( statussrv->m_appl[1]);

  s0__GetExtStatusResponse->ApplSts3 = statussrv->m_node->ProcStatus[22];
  statussrv_GetText( statussrv->m_node->ProcStatus[22], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts3Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts3Name = std::string( statussrv->m_appl[2]);

  s0__GetExtStatusResponse->ApplSts4 = statussrv->m_node->ProcStatus[23];
  statussrv_GetText( statussrv->m_node->ProcStatus[23], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts4Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts4Name = std::string( statussrv->m_appl[3]);

  s0__GetExtStatusResponse->ApplSts5 = statussrv->m_node->ProcStatus[24];
  statussrv_GetText( statussrv->m_node->ProcStatus[24], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts5Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts5Name = std::string( statussrv->m_appl[4]);

  s0__GetExtStatusResponse->ApplSts6 = statussrv->m_node->ProcStatus[25];
  statussrv_GetText( statussrv->m_node->ProcStatus[25], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts6Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts6Name = std::string( statussrv->m_appl[5]);

  s0__GetExtStatusResponse->ApplSts7 = statussrv->m_node->ProcStatus[26];
  statussrv_GetText( statussrv->m_node->ProcStatus[26], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts7Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts7Name = std::string( statussrv->m_appl[6]);

  s0__GetExtStatusResponse->ApplSts8 = statussrv->m_node->ProcStatus[27];
  statussrv_GetText( statussrv->m_node->ProcStatus[27], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts8Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts8Name = std::string( statussrv->m_appl[7]);

  s0__GetExtStatusResponse->ApplSts9 = statussrv->m_node->ProcStatus[28];
  statussrv_GetText( statussrv->m_node->ProcStatus[28], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts9Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts9Name = std::string( statussrv->m_appl[8]);

  s0__GetExtStatusResponse->ApplSts10 = statussrv->m_node->ProcStatus[29];
  statussrv_GetText( statussrv->m_node->ProcStatus[29], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts10Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts10Name = std::string( statussrv->m_appl[9]);

  s0__GetExtStatusResponse->ApplSts11 = statussrv->m_node->ProcStatus[30];
  statussrv_GetText( statussrv->m_node->ProcStatus[30], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts11Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts11Name = std::string( statussrv->m_appl[10]);

  s0__GetExtStatusResponse->ApplSts12 = statussrv->m_node->ProcStatus[31];
  statussrv_GetText( statussrv->m_node->ProcStatus[31], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts12Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts12Name = std::string( statussrv->m_appl[11]);

  s0__GetExtStatusResponse->ApplSts13 = statussrv->m_node->ProcStatus[32];
  statussrv_GetText( statussrv->m_node->ProcStatus[32], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts13Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts13Name = std::string( statussrv->m_appl[12]);

  s0__GetExtStatusResponse->ApplSts14 = statussrv->m_node->ProcStatus[33];
  statussrv_GetText( statussrv->m_node->ProcStatus[33], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts14Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts14Name = std::string( statussrv->m_appl[13]);

  s0__GetExtStatusResponse->ApplSts15 = statussrv->m_node->ProcStatus[34];
  statussrv_GetText( statussrv->m_node->ProcStatus[34], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts15Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts15Name = std::string( statussrv->m_appl[14]);

  s0__GetExtStatusResponse->ApplSts16 = statussrv->m_node->ProcStatus[35];
  statussrv_GetText( statussrv->m_node->ProcStatus[35], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts16Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts16Name = std::string( statussrv->m_appl[15]);

  s0__GetExtStatusResponse->ApplSts17 = statussrv->m_node->ProcStatus[36];
  statussrv_GetText( statussrv->m_node->ProcStatus[36], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts17Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts17Name = std::string( statussrv->m_appl[16]);

  s0__GetExtStatusResponse->ApplSts18 = statussrv->m_node->ProcStatus[37];
  statussrv_GetText( statussrv->m_node->ProcStatus[37], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts18Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts18Name = std::string( statussrv->m_appl[17]);

  s0__GetExtStatusResponse->ApplSts19 = statussrv->m_node->ProcStatus[38];
  statussrv_GetText( statussrv->m_node->ProcStatus[38], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts19Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts19Name = std::string( statussrv->m_appl[18]);

  s0__GetExtStatusResponse->ApplSts20 = statussrv->m_node->ProcStatus[39];
  statussrv_GetText( statussrv->m_node->ProcStatus[39], msg, sizeof(msg));
  s0__GetExtStatusResponse->ApplSts20Str = std::string( msg);
  s0__GetExtStatusResponse->ApplSts20Name = std::string( statussrv->m_appl[19]);

  return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6 __s0__Restart(struct soap *soap, 
					_s0__Restart *s0__Restart, 
					_s0__RestartResponse *s0__RestartResponse)
{
  if ( s0__Restart->ClientRequestHandle)
    s0__RestartResponse->ClientRequestHandle = 
      new std::string( *s0__Restart->ClientRequestHandle);
  return SOAP_OK;
}
