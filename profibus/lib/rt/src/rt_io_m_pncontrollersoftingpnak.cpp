/* 
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

/* rt_io_m_pncontrollersoftingpnak.cpp -- io methods for the profinet master object
   The PnControllerSoftingPNAK object serves as agent for one Profinet network
   The profinet stack we use is a Profinet stack from Softing
*/

#include <vector>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pwr.h"
#include "co_cdh.h"
#include "pwr_baseclasses.h"
#include "pwr_profibusclasses.h"
#include "rt_io_base.h"
#include "rt_io_bus.h"
#include "rt_io_msg.h"
#include "rt_errh.h"
#include "rt_io_agent_init.h"
#include "rt_pb_msg.h"

#include "profinet.h"
#include "pnak.h"
#include "co_dcli.h"
#include "rt_pn_gsdml_data.h"
#include "rt_io_pn_locals.h"
#include "rt_pn_iface.h"

#include "sys/socket.h"
#include "sys/ioctl.h"
#include "net/if.h"
#include "netinet/in.h"
#include "arpa/inet.h"



static int count;

static pwr_tStatus IoAgentInit (
  io_tCtx	ctx,
  io_sAgent	*ap
);
static pwr_tStatus IoAgentRead (
  io_tCtx	ctx,
  io_sAgent	*ap
);
static pwr_tStatus IoAgentWrite (
  io_tCtx	ctx,
  io_sAgent	*ap
);
static pwr_tStatus IoAgentClose (
  io_tCtx	ctx,
  io_sAgent	*ap
);


/*----------------------------------------------------------------------------*\
   Init method for the Pb_profiboard agent  
\*----------------------------------------------------------------------------*/
static pwr_tStatus IoAgentInit (
  io_tCtx	ctx,
  io_sAgent	*ap
) 
{
  pwr_sClass_PnControllerSoftingPNAK *op;
  pwr_tUInt16 sts;
  io_sAgentLocal *local;
  io_sRackLocal *r_local;

  char fname[196];
  char hname[40];
  char *env;

  vector<GsdmlDeviceData *>  device_vect;
  GsdmlDeviceData  *dev_data;
  PnDeviceData     *pn_dev_data;
  PnIOCRData       *pn_iocr_data;
  PnModuleData     *pn_slot_data;
  PnSubmoduleData  *pn_subslot_data;
  unsigned short    ii, jj, kk, ll, offset_inputs, offset_outputs, type;
  unsigned short    num_modules = 0;
  int               s;

  struct ifreq ifr = {};

  io_sRack *slave_list;

  count=0;

  /* Allocate area for local data structure */
  ap->Local = (io_sAgentLocal *) new io_sAgentLocal;
  if (!ap->Local) {
    //    errh_Error( "ERROR config Profibus DP Master %s - %s", ap->Name, "calloc");
    return IO__ERRINIDEVICE;
  }

  local = (io_sAgentLocal *) ap->Local;
    
  op = (pwr_sClass_PnControllerSoftingPNAK *) ap->op;

  op->Status = PB__NOTINIT;

  /* Initialize interface */
  
  errh_Info( "Initializing interface for Profinet IO Master %s", ap->Name);

  /* Add master as a device */

  dev_data = new GsdmlDeviceData;

  /* Get configs for device */

  gethostname(hname, 40);

  s = socket(AF_INET, SOCK_DGRAM, 0);
  strncpy(ifr.ifr_name, "eth1", sizeof(ifr.ifr_name));
  if (ioctl(s, SIOCGIFADDR, &ifr) >= 0) {
    strcpy(dev_data->ip_address, inet_ntoa(((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr));
  }
  if (ioctl(s, SIOCGIFNETMASK, &ifr) >= 0) {
    strcpy(dev_data->subnet_mask, inet_ntoa(((struct sockaddr_in *) &ifr.ifr_netmask)->sin_addr));
  }

  sscanf(dev_data->ip_address, "%hhu.%hhu.%hhu.%hhu", &local->ipaddress[3], &local->ipaddress[2], &local->ipaddress[1], &local->ipaddress[0]) ;
  sscanf(dev_data->subnet_mask, "%hhu.%hhu.%hhu.%hhu", &local->subnetmask[3], &local->subnetmask[2], &local->subnetmask[1], &local->subnetmask[0]) ;

  strcpy(dev_data->device_name, hname);
  dev_data->device_num = PN_DEVICE_REFERENCE_THIS_STATION;
  strcpy(dev_data->device_text, "controller");
  dev_data->vendor_id = 279; // Softing vendor id
  dev_data->device_id = 0;
  strcpy(dev_data->version, "1.0");
  dev_data->byte_order = 0;

  device_vect.push_back(dev_data);

  pn_dev_data = new PnDeviceData;

  pn_dev_data->device_ref = PN_DEVICE_REFERENCE_THIS_STATION;
  local->device_data.push_back(pn_dev_data);

  env = getenv("pwrp_load");

  /* Iterate over the slaves.  */

  for (slave_list = ap->racklist, ii = 0; slave_list != NULL;
       slave_list = slave_list->next, ii++) {

    dev_data = new GsdmlDeviceData;
    pn_dev_data = new PnDeviceData;

    sprintf(fname, "%s/pwr_pn_%s.xml", env, cdh_ObjidToFnString(NULL, slave_list->Objid ));

    dev_data->read(fname);
    device_vect.push_back(dev_data);
    
    pn_dev_data->device_ref = ii + 1;
    
    for (jj = 0; jj < dev_data->iocr_data.size(); jj++) {
      pn_iocr_data = new PnIOCRData;
      pn_iocr_data->type = dev_data->iocr_data[jj]->type;
      pn_dev_data->iocr_data.push_back(pn_iocr_data);
    }

    num_modules = 0;
    for (jj = 0; jj < dev_data->slot_data.size(); jj++) {
      if ((dev_data->slot_data[jj]->module_enum_number != 0) ||
	  (jj == 0))
	num_modules++;
      else break;
    }

    for (jj = 0; jj < num_modules; jj++) {
      pn_slot_data = new PnModuleData;
      pn_slot_data->slot_number = dev_data->slot_data[jj]->slot_number;
      pn_slot_data->ident_number = dev_data->slot_data[jj]->module_ident_number;
      pn_dev_data->module_data.push_back(pn_slot_data);

      for (kk = 0; kk < dev_data->slot_data[jj]->subslot_data.size(); kk++) {
	pn_subslot_data = new PnSubmoduleData;
        pn_subslot_data->subslot_number = dev_data->slot_data[jj]->subslot_data[kk]->subslot_number;
	pn_subslot_data->ident_number = dev_data->slot_data[jj]->subslot_data[kk]->submodule_ident_number;
        if (dev_data->slot_data[jj]->subslot_data[kk]->io_input_length > 0) {
	  pn_subslot_data->io_in_data_length = dev_data->slot_data[jj]->subslot_data[kk]->io_input_length;
	  pn_subslot_data->type = PROFINET_IO_SUBMODULE_TYPE_INPUT ;
	} 
        if (dev_data->slot_data[jj]->subslot_data[kk]->io_output_length > 0) {
	  pn_subslot_data->io_out_data_length = dev_data->slot_data[jj]->subslot_data[kk]->io_output_length;
	  pn_subslot_data->type |= PROFINET_IO_SUBMODULE_TYPE_OUTPUT;
	}
        if ((dev_data->slot_data[jj]->subslot_data[kk]->io_output_length > 0) &&
	    (dev_data->slot_data[jj]->subslot_data[kk]->io_input_length > 0)) {
	  pn_subslot_data->type |= PROFINET_IO_SUBMODULE_TYPE_INPUT_AND_OUTPUT;
	}
	
	pn_dev_data->module_data[jj]->submodule_data.push_back(pn_subslot_data);
      }
    }
    local->device_data.push_back(pn_dev_data);
  }

  /* Start profistack */

  pnak_init();

  sts = pnak_start_profistack(0, PNAK_CONTROLLER_MODE);
  
  if (sts != PNAK_OK) {
    op->Status = PB__INITFAIL;
    errh_Error( "Starting profistack returned with error code: %d", sts);
    return IO__ERRINIDEVICE;
  }
  

  /* Download configuration for all devices */
  
  for (ii = 0; ii < device_vect.size(); ii++) {
    //  for (ii = 0; ii < 1; ii++) {
    pack_download_req(&local->service_req_res, device_vect[ii], local->device_data[ii]->device_ref);
    
    sts = pnak_send_service_req_res(0, &local->service_req_res);
    
    if (sts == PNAK_OK) {
      sts = handle_service_con(local, ap);
      
      if (sts == PNAK_OK) { 
	/* Loop through devices and calculate offset for io */
	
	
	for (jj = 0; jj <  local->device_data[ii]->iocr_data.size(); jj++) {
	  offset_inputs = 0;
	  offset_outputs = 0;
	  type = local->device_data[ii]->iocr_data[jj]->type;
	  for (kk = 0; kk < local->device_data[ii]->module_data.size(); kk++) {
	    for (ll = 0; ll < local->device_data[ii]->module_data[kk]->submodule_data.size(); ll++) {
	      PnSubmoduleData *submodule;
	      submodule = local->device_data[ii]->module_data[kk]->submodule_data[ll];
	      if (submodule->type & type) {
		if ((submodule->type == PROFINET_IO_SUBMODULE_TYPE_INPUT) ||
		    (submodule->type == PROFINET_IO_SUBMODULE_TYPE_INPUT_AND_OUTPUT)) {
		  submodule->offset_clean_io_in = offset_inputs;
		  offset_inputs += submodule->io_in_data_length;
		}
		if ((submodule->type == PROFINET_IO_SUBMODULE_TYPE_OUTPUT) ||
		    (submodule->type == PROFINET_IO_SUBMODULE_TYPE_INPUT_AND_OUTPUT)) {
		  submodule->offset_clean_io_out = offset_outputs;
		  offset_outputs += submodule->io_out_data_length;
		}
	      }
	    }
	  }
	  local->device_data[ii]->iocr_data[jj]->clean_io_data = (unsigned char *) calloc(1, offset_inputs + offset_outputs);
	  local->device_data[ii]->iocr_data[jj]->clean_io_data_length = offset_inputs + offset_outputs;
	}
      }
    }
  }
  
  /* Loop trough devices and set up i/o */

  for (slave_list = ap->racklist, ii = 0; slave_list != NULL;
       slave_list = slave_list->next, ii++) {
    slave_list->Local = (unsigned char *) calloc(1, sizeof(io_sRackLocal));
    r_local = (io_sRackLocal *) slave_list->Local;
    
    for (jj = 0; jj <  local->device_data[ii + 1]->iocr_data.size(); jj++) {
      
      if (local->device_data[ii + 1]->iocr_data[jj]->type == PROFINET_IO_CR_TYPE_INPUT) {
	r_local->bytes_of_input = local->device_data[ii + 1]->iocr_data[jj]->clean_io_data_length;
	r_local->inputs = local->device_data[ii + 1]->iocr_data[jj]->clean_io_data;
      }
      else if (local->device_data[ii + 1]->iocr_data[jj]->type == PROFINET_IO_CR_TYPE_OUTPUT) {
	r_local->bytes_of_output = local->device_data[ii + 1]->iocr_data[jj]->clean_io_data_length;
	r_local->outputs = local->device_data[ii + 1]->iocr_data[jj]->clean_io_data;
      }
    }
  }
  

  /* Set identification */

  pack_set_identification_req(&local->service_req_res);

  sts = pnak_send_service_req_res(0, &local->service_req_res);

  if (sts == PNAK_OK) {
    sts = handle_service_con(local, ap);
  }

  /* Set mode online */
  
  T_PNAK_EVENT_SET_MODE        pMode;
  
  pMode.Mode = PNAK_MODE_ONLINE;
  
  sts = pnak_set_mode(0, &pMode);
  
  if (sts != PNAK_OK) {
    op->Status = PB__INITFAIL;
    errh_Error( "Profistack unable to go online, errcode: %d", sts);
    return IO__ERRINIDEVICE;
  }
  

  T_PNAK_WAIT_OBJECT           wait_object;
  
  wait_object = PNAK_WAIT_OBJECT_STATE_CHANGED;
  
  sts = pnak_wait_for_multiple_objects(0, &wait_object, PNAK_INFINITE_TIMEOUT);
  
  if (sts == PNAK_OK) {
    T_PNAK_EVENT_STATE           pState;
    
    sts = pnak_get_state(0, &pState);
    
    if (pState.Mode != PNAK_MODE_ONLINE) {
      
      if (sts != PNAK_OK) {
	op->Status = PB__INITFAIL;
	errh_Error( "Profistack unable to set state online, errcode: %d", sts);
	return IO__ERRINIDEVICE;
      }
    }
  }

  /* Activate the devices */
  
  T_PNAK_EVENT_SET_DEVICE_STATE  set_dev_state;
  
  memset(&set_dev_state, 0, sizeof(set_dev_state));
  
  for (ii = 0; ii < local->device_data.size(); ii++) {
    set_dev_state.ActivateDeviceReference[ii] = 1;
  }
  
  sts = pnak_set_device_state(0, &set_dev_state);
  
  if (sts != PNAK_OK) {
    op->Status = PB__INITFAIL;
    errh_Error( "Profistack unable to activate devices, errcode: %d", sts);
    return IO__ERRINIDEVICE;
  }

  /* Check state for all devices */

  for (ii = 1; ii < device_vect.size(); ii++) {
    //  for (ii = 0; ii < 1; ii++) {
    pack_get_device_state_req(&local->service_req_res, local->device_data[ii]->device_ref);

    sts = pnak_send_service_req_res(0, &local->service_req_res);

    if (sts == PNAK_OK) {
      sts = handle_service_con(local, ap);
    }
  }

  
  /* Active supervision thread */
  
  pthread_attr_t attr;
  
  local->args.local = local;
  local->args.ap = ap;
  
  pthread_attr_init(&attr);
  pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
  pthread_create(&local->handle_events, &attr, handle_events, &local->args);
  
  op->Status = PB__NORMAL;
  
  return IO__SUCCESS;
}

/*----------------------------------------------------------------------------*\
   Read method for the Pb_Profiboard agent  
\*----------------------------------------------------------------------------*/
static pwr_tStatus IoAgentRead (
  io_tCtx	ctx,
  io_sAgent	*ap
)
{
  io_sAgentLocal *local;
  PnIOCRData       *pn_iocr_data;
  pwr_tUInt16 sts;
  unsigned char *io_datap;
  unsigned char *clean_io_datap;
  unsigned char status_data = 0;
  PnSubmoduleData *submodule;


  unsigned short data_length, ii, jj, kk, ll;

  local = (io_sAgentLocal *) ap->Local;

  //  handle_events(&local->args);
  /* Read i/o for all devices and move it to clean io data area */

  for (ii = 1; ii < local->device_data.size(); ii++) {
    if (local->device_data[ii]->device_state == PNAK_DEVICE_STATE_CONNECTED) {
      for (jj = 0; jj < local->device_data[ii]->iocr_data.size(); jj++) {
	
	pn_iocr_data = local->device_data[ii]->iocr_data[jj];
      
	if (pn_iocr_data->type == PROFINET_IO_CR_TYPE_INPUT) {
	  data_length = pn_iocr_data->io_data_length;
	  
	  sts = pnak_get_iocr_data(0, pn_iocr_data->identifier, pn_iocr_data->io_data, &data_length, &status_data);
	  
	  if (sts == PNAK_OK) {
	    for (kk = 0; kk < local->device_data[ii]->module_data.size(); kk++) {
	      for (ll = 0; ll < local->device_data[ii]->module_data[kk]->submodule_data.size(); ll++) {
		submodule = local->device_data[ii]->module_data[kk]->submodule_data[ll];
		if ((submodule->type == PROFINET_IO_SUBMODULE_TYPE_INPUT) ||
		    (submodule->type == PROFINET_IO_SUBMODULE_TYPE_INPUT_AND_OUTPUT)) {
		  io_datap = (pn_iocr_data->io_data + submodule->offset_io_in);
		  clean_io_datap = (pn_iocr_data->clean_io_data + submodule->offset_clean_io_in);
		  memcpy(clean_io_datap, io_datap, submodule->io_in_data_length);	  
		}
	      }
	    }
	  }
	}
      }
    }
  }
  return IO__SUCCESS;
}


/*----------------------------------------------------------------------------*\
   Write method for the Pb_Profiboard agent  
\*----------------------------------------------------------------------------*/
static pwr_tStatus IoAgentWrite (
  io_tCtx	ctx,
  io_sAgent	*ap
) 
{
  io_sAgentLocal *local;
  PnIOCRData       *pn_iocr_data;
  pwr_tUInt16 sts;
  unsigned char *io_datap;
  unsigned char *clean_io_datap;
  PnSubmoduleData *submodule;

  unsigned short data_length, ii, jj, kk, ll;
  unsigned char *status_datap;

  local = (io_sAgentLocal *) ap->Local;

  /* Write i/o for all devices fetch it first from clean io data area */

  for (ii = 1; ii < local->device_data.size(); ii++) {

    if (local->device_data[ii]->device_state == PNAK_DEVICE_STATE_CONNECTED) {
      for (jj = 0; jj < local->device_data[ii]->iocr_data.size(); jj++) {
	
	pn_iocr_data = local->device_data[ii]->iocr_data[jj];
	
	if (pn_iocr_data->type == PROFINET_IO_CR_TYPE_OUTPUT) {
	  data_length = pn_iocr_data->io_data_length;	  
	  
	  for (kk = 0; kk < local->device_data[ii]->module_data.size(); kk++) {
	    for (ll = 0; ll < local->device_data[ii]->module_data[kk]->submodule_data.size(); ll++) {
	      submodule = local->device_data[ii]->module_data[kk]->submodule_data[ll];
	      if ((submodule->type == PROFINET_IO_SUBMODULE_TYPE_OUTPUT) ||
		  (submodule->type == PROFINET_IO_SUBMODULE_TYPE_INPUT_AND_OUTPUT)) {
		io_datap = (pn_iocr_data->io_data + submodule->offset_io_out);
		clean_io_datap = (pn_iocr_data->clean_io_data + submodule->offset_clean_io_out);
		memcpy(io_datap, clean_io_datap, submodule->io_out_data_length);
		
		status_datap = io_datap + submodule->io_out_data_length;
		*status_datap = PNAK_IOXS_STATUS_NO_EXTENTION_FOLLOWS | PNAK_IOXS_STATUS_DATA_GOOD;
		
	      }
	    }
	  }
	  sts = pnak_set_iocr_data(0, pn_iocr_data->identifier, pn_iocr_data->io_data, pn_iocr_data->io_data_length);
	}
      }
    }
  }
   

  return IO__SUCCESS;
}


/*----------------------------------------------------------------------------*\
  
\*----------------------------------------------------------------------------*/
static pwr_tStatus IoAgentClose (
  io_tCtx	ctx,
  io_sAgent	*ap
) 
{
  io_sAgentLocal 	*local;
  pwr_tStatus            sts = PB__NOTINIT;

  local = (io_sAgentLocal *) ap->Local;

  /* Stop profistack */

  pnak_init();

  sts = pnak_stop_profistack(0);

  pnak_term();

  /* Clean data areas .... */

  return sts;
}


/*----------------------------------------------------------------------------*\
  Every method to be exported to the workbench should be registred here.
\*----------------------------------------------------------------------------*/

pwr_dExport pwr_BindIoMethods(PnControllerSoftingPNAK) = {
  pwr_BindIoMethod(IoAgentInit),
  pwr_BindIoMethod(IoAgentRead),
  pwr_BindIoMethod(IoAgentWrite),
  pwr_BindIoMethod(IoAgentClose),
  pwr_NullMethod
};