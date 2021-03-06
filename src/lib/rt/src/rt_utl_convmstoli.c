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

/* rt_utl_convmstoli.c 

   Given an input in milliseconds ConvMSToLI converts from
   the integer millisecond value to the ELN internal delta
   time format of dddd hh:mm:ss.cc. Finally ConvMSToLI converts
   the delta time to a LARGE_INTEGER. Since the smallest time
   unit that can be stored is 1/100 sec, any single ms is truncated.
   ie. 45 ms would be converted to 0000 00:00:00.04 which is 40 ms.

   The largest ms-value that can be converted by ConvMSToLI is 
   999 days, a value which is hardly likely to be entered.

   Arguments:
      Time        Time in ms that is to be converted.
      TimeLI      pointer to a LARGE_INTEGER in which the converted
                  ms-time is stored.  */
#if defined OS_POSIX
  const char *rt_utl_convmstoli_str = "rt_utl_convmstoli, Not needed for LYNX and LINUX";
#else

#ifdef	OS_ELN
#include descrip
#endif

#ifdef	OS_VMS
#include <descrip.h>
#include <lib$routines.h>
#endif

#include "pwr.h"
#include "pwr_class.h"


#if 1 
void plc_ConvMSToLI(pwr_tUInt32 Time, pwr_tVaxTime *TimeLI)
{
    int Multiplier = -100000; /* Used to convert 1/10 ms to 100 ns, delta time*/
    int Addend = 0;

    /* Truncate to 1/100 sec */
    Time /= 10; 

    lib$emul (&Multiplier, &Time, &Addend, TimeLI);

} /* END plc_ConvMSToLI */


#else  /* Old code */

   

#include $vaxelnc

void 
plc_ConvMSToLI(Time, TimeLI)

pwr_tUInt32	 	Time;	/* Time in ms that is to be converted	*/
LARGE_INTEGER 	*TimeLI;/* Pointer to LARGE_INTEGER that will contain result */

{
$DESCRIPTOR(TimeDescr, ""); /* Create empty string descr. to hold delta time */
char	DeltaString[16]; /* To store the delta time with separators */

char	TimeCell[12] = {'0'}; /* Init the thousand day field to 0 */

pwr_tUInt32	DivCell[12] = {	 	 0,	/* No times > 999 days */
				 860400000,	/* 100 Days 	*/
				  86400000,	/* 10 Days	*/
				   8640000,	/* 1 Day	*/
				   3600000,	/* 10 Hours	*/
				    360000,	/* 1 Hour	*/
				     60000,	/* 10 min	*/
				      6000,	/* 1 min	*/
				      1000,	/* 10 sec	*/
				       100,	/* 1 sec	*/
				        10,	/* 10th sec	*/
					 1};	/* 100th sec	*/
pwr_tInt32	i;

    /* 	First, divide ms by 10 to get 100th of seconds which is the
     *  smallest unit in the time string. This means that any single
     *  ms will be truncated. ie. 49 ms will be truncated to 4 1/100
     *  sec, 40 ms.
     */

    Time /= 10;
    
    /* 	Now, work our way through the different time units, until we
     *  reach 1/100 sec. Convert each time to an ASCII number by
     *  adding 48 to the variable.
     */

    for (i = 1; i < 12; i++)
    {
	TimeCell[i] = (Time / DivCell[i]) + 48;
	Time %= DivCell[i];
    }
	
    /* Fill the string with delta time information, and add the separators */
    DeltaString[0]	= TimeCell[0];	/********************************/
    DeltaString[1]	= TimeCell[1];  /*	Number of days	     	*/
    DeltaString[2]	= TimeCell[2];	/* 				*/
    DeltaString[3]	= TimeCell[3];	/********************************/
    DeltaString[4]	= ' ';
    DeltaString[5]	= TimeCell[4];	/*  Number of hours		*/
    DeltaString[6]	= TimeCell[5];
    DeltaString[7]	= ':';
    DeltaString[8]	= TimeCell[6];	/*  Number of minutes		*/
    DeltaString[9]	= TimeCell[7];
    DeltaString[10]	= ':';
    DeltaString[11]	= TimeCell[8];	/*  Number of seconds		*/
    DeltaString[12]	= TimeCell[9];
    DeltaString[13]	= '.';
    DeltaString[14]	= TimeCell[10];	/*  Number of 1/100 seconds	*/
    DeltaString[15]	= TimeCell[11];

    /* Fill in the descriptor */
    TimeDescr.dsc$a_pointer	= &DeltaString;
    TimeDescr.dsc$w_length	= 16;
    
    /* Convert from string to LARGE_INTEGER */

    *TimeLI = eln$time_value(&TimeDescr);    

} /* END plc_ConvMSToLI */
#endif

#endif /* Not OS_LYNX || OS_LINUX || defined OS_MACOS || defined OS_FREEBSD */


