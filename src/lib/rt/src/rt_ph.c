/* 
 * Proview   Open Source Process Control.
 * Copyright (C) 2005-2017 SSAB EMEA AB.
 *
 * This file is part of Proview.
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
 * along with Proview. If not, see <http://www.gnu.org/licenses/>
 *
 * Linking Proview statically or dynamically with other modules is
 * making a combined work based on Proview. Thus, the terms and 
 * conditions of the GNU General Public License cover the whole 
 * combination.
 *
 * In addition, as a special exception, the copyright holders of
 * Proview give you permission to, from the build function in the
 * Proview Configurator, combine Proview with modules generated by the
 * Proview PLC Editor to a PLC program, regardless of the license
 * terms of these modules. You may copy and distribute the resulting
 * combined work under the terms of your choice, provided that every 
 * copy of the combined work is accompanied by a complete copy of 
 * the source code of Proview (the version used to produce the 
 * combined work), being distributed under the terms of the GNU 
 * General Public License plus this exception.
 */

/* rt_ph.c -- Runtime environment - Packet Handler */

#if 1
const char *ph_idstr ="rt_ph: NYI";
#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef pwr_h
#include "pwr.h"
#endif

#ifndef pwr_class_h
#include "pwr_class.h"
#endif

#ifndef pwr_baseclasses_h
#include "pwr_baseclasses.h"
#endif

#ifndef pwr_pams_h
#include "pwr_pams.h"
#endif

#ifndef rt_ph_h
#include "rt_ph.h"
#endif

#ifndef rt_gdh_h
#include "rt_gdh.h"
#endif








/* Type definitions */
typedef struct
    {
    PAMS_ADDRESS    Address;
    pwr_tObjid	    Que;
    pwr_tBoolean    Operator;
    pwr_tBoolean    Global;
    } _Ctx;


/* Local variables */

/* Local function prototypes */

static pwr_tStatus HandlePamsMsg (
  int	sts,
  char *MsgP,
  short Class,
  short Type,
  short Size,
  ph_uPack *PackP,
  pwr_tClassId *PackClass,
  void **ClassData
);

 

/*
* Name:    
*   ph_Connect
*
*
* Function:
*   Connects calling application to a queue-object.
*   The returned context is used as an identifier when calling Get and 
*   Disconnect
* Description:
*   
*/
pwr_tStatus ph_Connect
    (
    char	    *QueName,
    mq_uAddress     Address,
    ph_tCtx	    *Ctx
    )
{
    _Ctx	  *ctx;
    pwr_tObjid	  Que;
    pwr_tClassId  Class;
    pwr_tUInt32	  sts;
    char	  AttrName[sizeof(pwr_tFullName) + 35];

    *Ctx = NULL;

    sts = gdh_NameToObjid(QueName, &Que);
    if (EVEN(sts))
	return PH__NOSUCHQUE;

    sts = gdh_GetObjectClass(Que, &Class);
    if (EVEN(sts))
	return PH__NOSUCHQUE;

    ctx = (_Ctx *) calloc(1, sizeof(_Ctx));
    *Ctx = (ph_tCtx) ctx;

    if (Class == pwr_cClass_OpPlace)
	ctx->Operator = TRUE;
    else if (Class == pwr_cClass_Queue)
	ctx->Operator = FALSE;	
    else
	return PH__NOSUCHQUE;
	


    sprintf(AttrName, "%s.Address", QueName);
    sts = gdh_SetObjectInfo(AttrName, &Address, sizeof(Address));
    if (EVEN(sts))
	return PH__NOSUCHQUE;

    ctx->Address.all = Address.All;
    ctx->Que = Que;    
    
    return PH__SUCCESS;
} /* END ph_Connect */



/*
* Name:    
*   ph_Disconnect
*
*
* Function:
*   Disconnects calling application from a queue-object.
*   Frees the context 
* Description:
*   
*/
pwr_tStatus ph_Disconnect
    (
    ph_tCtx	    Ctx
    )
{
    _Ctx	*ctx;
    PAMS_ADDRESS Address;
    pwr_tUInt32 sts;
    char	AttrName[sizeof(pwr_tFullName) + 35];
    pwr_tFullName   Name;

    
    ctx = (_Ctx *) Ctx;

    sts = gdh_ObjidToName(ctx->Que, Name, sizeof(Name), cdh_mNName);
    if (EVEN(sts))
	return PH__NOSUCHQUE;

    if (!ctx->Global)
    {
	sprintf(AttrName, "%s.Address", Name);
	sts = gdh_GetObjectInfo(AttrName, &Address, sizeof(Address));    
	if (EVEN(sts))
	    return PH__NOSUCHQUE;

	if (Address.all == ctx->Address.all)
	{
	    Address.all = 0;
	    gdh_SetObjectInfo(AttrName, &Address, sizeof(Address));    
	}
    }

    free(ctx);

    return PH__SUCCESS;
    
}/* END ph_Disconnect */



/*
* Name:    
*   ph_FreeClassData
*
*
* Function:
*   Frees class data which where fetched with ph_GetPacketXXX
* Description:
*   
*/
pwr_tStatus ph_FreeClassData
    (
    void	    *ClassData
    )
{
    if (ClassData != NULL)
	free(ClassData);

    return PH__SUCCESS;

}/* END ph_FreeClassData  */



/*
* Name:    
*   ph_GetPacket
*
*
* Function:
*   Gets the next packet in the queue. Returns immeditely if the queue is 
*   empty
* Description:
*   
*/
pwr_tStatus ph_GetPacket
    (
    ph_tCtx	    Ctx,
    ph_uPack	    *Pack,
    pwr_tClassId    *PackClass,
    void	    **ClassData
    )
{
    pwr_tUInt32	sts;
static char     Msg[8192];        
    char	Prio = 0;
    short	Class;
    short	Type;
    short	MsgSize = sizeof Msg;
    short	Size;
    _Ctx *ctx = (_Ctx *) Ctx;


  sts = pams_get_msg ((char *)Msg, &Prio, &ctx->Address, &Class, &Type,
    &MsgSize, &Size, NULL, NULL, NULL, NULL, NULL, NULL, NULL);


  return HandlePamsMsg (
	    sts, (char *)&Msg, Class, Type, Size, Pack, PackClass, ClassData);


}/* END ph_GetPacket  */

/*
* Name:    
*   ph_GetWaitPacket
*
*
* Function:
*   Gets the next packet in the queue. Waits until a packet arrives or it is
*   timeouted
* Description:
*   
*/
pwr_tStatus ph_GetWaitPacket
    (
    ph_tCtx	    Ctx,
    ph_uPack	    *Pack,
    pwr_tClassId    *PackClass,
    void	    **ClassData,
    pwr_tUInt32	    Timeout
    )
{
static char     Msg[8192];        
    pwr_tUInt32	sts;
    char	Prio = 0;
    short	Class;
    short	Type;
    short	MsgSize = sizeof Msg;
    short	Size;
    char	*Data;
    ph_uPack	*PackP;
    _Ctx *ctx = (_Ctx *) Ctx;


  sts = pams_get_msgw ((char *)Msg, &Prio, &ctx->Address, &Class, &Type,
    &MsgSize, &Size, (int32 *)&Timeout, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL);

  return HandlePamsMsg (
	    sts, (char *)&Msg, Class, Type, Size, Pack, PackClass, ClassData);


}/* END ph_GetWaitPacket  */

/*
* Name:    
*   ph_SendPacket
*
*
* Function:
*   Sends a packet to a  queue or an operator.
* Description:
*   
*/
pwr_tStatus ph_SendPacket
    (
    char	    *QueName,
    ph_uPack	    *Pack,
    pwr_tClassId    PackClass,
    pwr_tUInt32	    ClassSize,
    void	    *ClassData
    )
{
    PAMS_ADDRESS Address;
    pwr_tUInt32	sts;
    char	Prio = 0;
    short	Class = ph_cMsgClass;
    short	Type = Pack->Type;
    short	Size;
    char	Delivery = 0;
    char	*MsgP;
    ph_uPack	*PackP;
    pwr_tObjid	Que;
    char	AttrName[sizeof(pwr_tFullName) + 35];
    

    sts = gdh_NameToObjid(QueName, &Que);
    if (EVEN(sts))
        return PH__NOSUCHQUE;

    sprintf(AttrName, "%s.Address", QueName);
    sts = gdh_GetObjectInfo(AttrName, &Address, sizeof(Address));
    if (EVEN(sts) || Address.all == 0)
	return PH__NOSUCHQUE;
    

    Size = sizeof(ph_uPack) + ClassSize;
    MsgP = malloc(Size);

    memcpy(MsgP, Pack, sizeof(ph_uPack));
    memcpy(MsgP + sizeof(ph_uPack), ClassData, ClassSize);

#ifdef OS_ELN
    sts = pams_put_msg (MsgP, &Prio, &Address, &Class, &Type, &Delivery, &Size,
      NULL, NULL, NULL, NULL, NULL);
#else
    sts = pams_put_msg (MsgP, &Prio, &Address, &Class, &Type, &Delivery, &Size,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL);
#endif
    free (MsgP);
    if (EVEN (sts))
	return sts;
    else
	return PH__SUCCESS;


} /* END ph_SendPacket  */

/*
* Name:    HandlePamsMsg
*   
*
*
* Function:
*   Handles a PAMS message
* Description:
*   
*/

static pwr_tStatus HandlePamsMsg (
    int	sts,
    char *MsgP,
    short Class,
    short Type,
    short Size,
    ph_uPack *Pack,
    pwr_tClassId *PackClass,
    void **ClassData
)
{
    char	*Data;
    ph_uPack	*PackP;

    if (sts == PAMS__TIMEOUT)
	return PH__TIMEOUT;
    else if (EVEN(sts))
	return PH__GETMSG;
    else if (sts == 1) /* SS$_NORMAL */
	;
    else
	return PH__QUEEMPTY;

    if (Class != ph_cMsgClass)
	return PH__QUEEMPTY;


    if (Pack != NULL)
	memcpy(Pack, MsgP, sizeof(ph_uPack));

    PackP = (ph_uPack *) MsgP;
    *PackClass = PackP->Standard.PackClass;
    
    /* Alloc and copy class data */
    Data = malloc(Size - sizeof(ph_uPack));
    memcpy(Data, MsgP + sizeof(ph_uPack), Size - sizeof(ph_uPack));
    *ClassData = Data;


    return PH__SUCCESS;

} /* END HandlePamsMsg */
#endif
