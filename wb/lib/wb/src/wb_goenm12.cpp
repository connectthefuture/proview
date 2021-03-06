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

#include <stdio.h>

#include "pwr.h"
#include "flow_ctx.h"
#include "flow_api.h"
#include "co_dcli.h"
#include "wb_ldh.h"
#include "wb_foe_msg.h"
#include "wb_vldh.h"
#include "wb_goen.h"
#include "wb_gre.h"
#include "wb_goenm12.h"

static	float	f_strlength  = GOEN_F_OBJNAME_STRWIDTH;
static	float	f_strheight  = GOEN_F_OBJNAME_STRHEIGHT;


/*_Methods defined for this module_______________________________________*/

/*************************************************************************
*
* Name:		goen_create_nodetype_m12()
*
* Type		
*
* Type		Parameter	IOGF	Description
*    pwr_sGraphPlcNode	*graphbody	Pointer to objecttype data
*    Widget	        widget			Neted widget
*    unsigned long 	*mask			Mask for drawing inputs/outputs
*    int		color			Highlight color
*    Cursor		cursor			Hot cursor
*    unsigned long      *node_type_id		Nodetypeid for created nodetype
*
* Description:
*	Create a nodetype
*
**************************************************************************/


int goen_create_nodetype_m12( pwr_sGraphPlcNode	*graphbody,
			      pwr_tClassId	cid,
			      ldh_tSesContext	ldhses,
			      flow_tCtx		ctx,
			      unsigned int 	*mask,
			      unsigned long	subwindowmark,
			      unsigned long	node_width,
			      flow_tNodeClass	*node_class,
			      vldh_t_node		node)
{
  int		graph_index;
  int		annot_count;
  int		sts;
  char		annot_str[3][80];
  int		annot_nr[3];
  char		name[80];
  int		size;
  flow_tNodeClass	nc;
  pwr_tFileName fname;
	
  sts = ldh_ClassIdToName(ldhses, cid, name, sizeof(name), &size);
  if ( EVEN(sts) ) return sts;

  /* Get graph index for this class */
  graph_index = graphbody->graphindex;	

  /* Get number of annotations and the width of the annotations */
  sts = WGre::get_annotations( node,
		(char *)annot_str, annot_nr, &annot_count,
		sizeof( annot_str)/sizeof(annot_str[0]), sizeof( annot_str[0]));
  if ( EVEN(sts)) return sts;

  bool loaded = false;
  if ( subwindowmark != 0) {
    sprintf( fname, "$pwrp_exe/%s_sw.flwn", name);
    cdh_ToLower( fname, fname);
    dcli_translate_filename( fname, fname);

    sts = flow_LoadNodeClass( ctx, fname, &nc);
    if ( ODD(sts))
      loaded = true;
    else {
      // Try base
      sprintf( fname, "$pwr_exe/pwr_c_%s_sw.flwn", name);
      cdh_ToLower( fname, fname);
      dcli_translate_filename( fname, fname);

      sts = flow_LoadNodeClass( ctx, fname, &nc);
      if ( ODD(sts))
	loaded = true;
    }
  }
  if ( !loaded) {
    sprintf( fname, "$pwrp_exe/%s.flwn", name);
    cdh_ToLower( fname, fname);
    dcli_translate_filename( fname, fname);

    sts = flow_LoadNodeClass( ctx, fname, &nc);
    if ( EVEN(sts)) {
      // Try base
      sprintf( fname, "$pwr_exe/pwr_c_%s.flwn", name);
      cdh_ToLower( fname, fname);
      dcli_translate_filename( fname, fname);

      sts = flow_LoadNodeClass( ctx, fname, &nc);
      if ( EVEN(sts)) return sts;
    }
  }

  /* Add execute order display */
  double x =  0;
  double y  = 0;
  flow_AddFilledRect( nc, x, y, 
		      GOEN_DISPLAYNODEWIDTH, GOEN_DISPLAYNODEHEIGHT, 
		      flow_eDrawType_LineErase, flow_mDisplayLevel_2);
  flow_AddRect( nc, x, y, 
		GOEN_DISPLAYNODEWIDTH, GOEN_DISPLAYNODEHEIGHT, 
		flow_eDrawType_LineRed, 1, flow_mDisplayLevel_2);
  flow_AddAnnot( nc, 
		 x + f_strlength,
		 y + (GOEN_DISPLAYNODEHEIGHT + f_strheight)/2.0,
		 GOEN_DISPLAYNODE_ANNOT, flow_eDrawType_TextHelvetica, GOEN_F_TEXTSIZE, 
		 flow_eAnnotType_OneLine, flow_mDisplayLevel_2);

  *node_class = nc;
  return GOEN__SUCCESS;
}



/*************************************************************************
*
* Name:		goen_get_point_info()
*
* Type		
*
* Type		Parameter	IOGF	Description
*    pwr_sGraphPlcNode	*graphbody	Pointer to objecttype data
*    unsigned long	point			Connection point nr
*    unsigned long 	*mask			Mask for drawing inputs/outputs
*    goen_conpoint_type	*info_pointer		Pointer to calculated data
*
* Description:
*	Calculates relativ koordinates for a connectionpoint and investigates
*	the connectionpoint type.
*
**************************************************************************/
int goen_get_point_info_m12( WGre *grectx, pwr_sGraphPlcNode *graphbody, 
			     unsigned long point, unsigned int *mask, 
			     unsigned long node_width, goen_conpoint_type *info_pointer, 
			     vldh_t_node node)
{
  info_pointer->x = 0;
  info_pointer->y = 0;
  info_pointer->type = CON_RIGHT;
  return GOEN__SUCCESS;
}



/*************************************************************************
*
* Name:		goen_get_parameter_m12()
*
* Type		
*
* Type		Parameter	IOGF	Description
*    pwr_sGraphPlcNode	*graphbody	Pointer to objecttype data
*    unsigned long	point			Connection point nr
*    unsigned long 	*mask			Mask for drawing inputs/outputs
*    unsigned long	*par_type		Input or output parameter	
*
* Description:
*	Gets pointer to parameterdata for connectionpoint.
*
**************************************************************************/
int	goen_get_parameter_m12( pwr_sGraphPlcNode *graphbody, pwr_tClassId cid, 
			       ldh_tSesContext ldhses, unsigned long con_point, 
			       unsigned int *mask, unsigned long *par_type, 
			       unsigned long *par_inverted, unsigned long *par_index)
{
  ldh_sParDef 	*bodydef;
  int 		rows;
  unsigned long	inputs,interns,outputs;
  int		i, input_found, output_found;
  int		sts;

  /* Get the runtime paramters for this class */
  sts = ldh_GetObjectBodyDef(ldhses, cid, "RtBody", 1, 
			     &bodydef, &rows);
  if ( EVEN(sts)) {
    /* This object contains only a devbody */
    sts = ldh_GetObjectBodyDef(ldhses, cid, "DevBody", 1, 
			       &bodydef, &rows);
    if ( EVEN(sts) ) return GOEN__NOPOINT;
  }

  inputs = graphbody->parameters[PAR_INPUT];
  interns = graphbody->parameters[PAR_INTERN];
  outputs = graphbody->parameters[PAR_OUTPUT];

  input_found = 0;
  output_found = 0;
  for ( i = 0; i < (int)inputs; i++) {
    if (bodydef[i].Par->Input.Graph.ConPointNr == con_point ) {
      *par_type = PAR_INPUT;
      *par_index = i;
      *par_inverted = GOEN_NOT_INVERTED;
      input_found = 1;
      break;
    }
  }
  if ( input_found == 0 ) {
    for ( i = inputs + interns; i < int(inputs + interns + outputs); i++) {
      if (bodydef[i].Par->Output.Graph.ConPointNr == con_point ) {
	*par_type = PAR_OUTPUT;
	*par_index = i;
	*par_inverted = GOEN_NOT_INVERTED;
	output_found = 1;
	break;
      }
    }
  }
  
  free((char *) bodydef);
  if ( input_found || output_found ) return GOEN__SUCCESS;
  else return GOEN__NOPOINT;
}



/*************************************************************************
*
* Name:		goen_get_location_point_m12()
*
* Type		
*
* Type		Parameter	IOGF	Description
*    pwr_sGraphPlcNode	*graphbody	Pointer to objecttype data
*    goen_point_type	*info_pointer		Locationpoint
*
* Description:
*	Calculates kooridates for locationpoint relativ geomtrical center.
*
**************************************************************************/
int goen_get_location_point_m12( WGre *grectx, pwr_sGraphPlcNode *graphbody, 
				 unsigned int *mask, unsigned long node_width, 
				 goen_point_type *info_pointer, vldh_t_node node)
{
	return GOEN__SUCCESS;
}
