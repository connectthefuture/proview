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
 **/

/* wb_c_planthier.c -- work bench methods of the PlantHier class. */

#include <string.h>
#include "wb_pwrs.h"
#include "wb_pwrs_msg.h"
#include "wb_pwrb_msg.h"
#include "wb_foe_msg.h"
#include "wb_ldh.h"
#include "wb_wsx.h"
#include "pwr_baseclasses.h"
#include "co_dcli.h"
#include "flow.h"
#include "flow_browctx.h"
#include "flow_browapi.h"
#include "wb_wnav.h"
#include "wb_build.h"
#include "wb_wsx.h"
#include "cow_msgwindow.h"

/*----------------------------------------------------------------------------*\
  Build volume.
\*----------------------------------------------------------------------------*/
static pwr_tStatus Build (
  ldh_sMenuCall *ip
)
{
  wb_build build( *(wb_session *)ip->PointedSession, ip->wnav);

  build.opt = ip->wnav->gbl.build;
  build.planthier( ip->Pointed.Objid);

  if ( build.sts() == PWRB__NOBUILT)
    ip->wnav->message( 'I', "Nothing to build");
  return build.sts();
}

//
// Syntax Check
//
static pwr_tStatus SyntaxCheck( ldh_tSesContext Session,
				pwr_tAttrRef Object,
				int *ErrorCount,
				int *WarningCount) 
{
  pwr_tStatus sts;
  pwr_tCid defgraph_class[] = { pwr_cClass_XttGraph, 0};
  pwr_tCid deftrend_class[] = { pwr_cClass_DsTrend, pwr_cClass_DsFast, pwr_cClass_DsFastCurve,
				pwr_cClass_PlotGroup, 0};

  sts = wsx_CheckAttrRef( Session, Object, "DefGraph", defgraph_class, 1, ErrorCount, WarningCount);
  if ( EVEN(sts)) return sts;
  
  sts = wsx_CheckAttrRef( Session, Object, "DefTrend", deftrend_class, 1, ErrorCount, WarningCount);
  if ( EVEN(sts)) return sts;

  return PWRS__SUCCESS;
}  

/*----------------------------------------------------------------------------*\
  Every method to be exported to the workbench should be registred here.
\*----------------------------------------------------------------------------*/

pwr_dExport pwr_BindMethods($PlantHier) = {
  pwr_BindMethod(Build),
  pwr_BindMethod(SyntaxCheck),
  pwr_NullMethod
};

