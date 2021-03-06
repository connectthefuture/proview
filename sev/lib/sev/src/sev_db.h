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

#ifndef sev_db_h
#define sev_db_h

#include <vector>

#include "pwr.h"
#include "pwr_class.h"
#include "pwr_baseclasses.h"
#include "rt_mh_net.h"
#include "rt_sev_net.h"
#include "sev_valuecache.h"

using namespace std;

#define sev_cVersion 3

typedef enum {
  sev_eDbType_,
  sev_eDbType_Mysql,
  sev_eDbType_Sqlite,
  sev_eDbType_HDF5
} sev_eDbType;

typedef struct {
  float current_load;
  float medium_load;
  float storage_rate;
  float medium_storage_rate;
  unsigned int datastore_msg_cnt;
  unsigned int dataget_msg_cnt;
  unsigned int items_msg_cnt;
  unsigned int eventstore_msg_cnt;
} sev_sStat;

typedef struct {
  pwr_tTime time;
  pwr_tFloat32 value;
  int stored;
} sev_StoredFloat32;

typedef struct {
  pwr_tTime time;
  pwr_tInt32 value;
  int stored;
} sev_StoredInt32;

typedef struct {
  int size;
  int first;
  int last;
  void *values;
} sev_sStoredValues;

class sev_attr {
 public:
  sev_attr() : type(pwr_eType_), size(0), elem(0) {
    strcpy( aname, ""); strcpy( unit, "");
  }
  sev_attr( const sev_attr& x) : type(x.type), size(x.size), elem(x.elem) {
    strncpy( aname, x.aname, sizeof(aname)); strncpy( unit, x.unit, sizeof(unit));
  }
  pwr_tOName	aname;
  pwr_eType	type;
  unsigned int	size;
  unsigned int  elem;
  pwr_tString16 unit;
};

class sev_event {
 public:
  unsigned int 	       	type;
  unsigned int	 	eventprio;
  mh_sEventId 		eventid;
  pwr_tTime 		time;
  char 			eventtext[80];
  char 			eventname[80];
};

class sev_item {
 public:
  sev_item() : deadband_active(0), last_id(0), value_size(0), old_value(0), first_storage(1), status(0), logged_status(0), 
    cache(0), idx(0), deleted(0) { 
    /*memset( old_value, 0, sizeof(old_value));*/
  }
  sev_item( const sev_item& x) : id(x.id), oid(x.oid), creatime(x.creatime), modtime(x.modtime), 
    storagetime(x.storagetime), sevid(x.sevid), scantime(x.scantime), deadband(x.deadband), options(x.options),
    deadband_active(x.deadband_active), last_id(x.last_id), value_size(x.value_size), old_value(x.old_value),
    first_storage(x.first_storage),
    attrnum(x.attrnum), attr(x.attr), status(x.status), logged_status(x.logged_status), cache(0),
    idx(x.idx), deleted(x.deleted) {
    strncpy( tablename, x.tablename, sizeof(tablename)); 
    strncpy( oname, x.oname, sizeof(oname)); 
    strncpy( description, x.description, sizeof(description));
    if ( x.cache)
      switch ( attr[0].type) {
      case pwr_eType_Float32:
      case pwr_eType_Float64:
      case pwr_eType_Int32:
	cache = new sev_valuecache_double(*(sev_valuecache_double *)x.cache);
	break;
      case pwr_eType_Boolean:
	cache = new sev_valuecache_bool(*(sev_valuecache_bool *)x.cache);
	break;
      default: ;
      }
  }
  ~sev_item() {
    if ( cache)
      delete cache;
  }
  unsigned int 	id;
  char		tablename[256];
  pwr_tOid	oid;
  pwr_tOName	oname;
  pwr_tTime	creatime;
  pwr_tTime	modtime;
  pwr_tDeltaTime storagetime;
  pwr_tRefId    sevid;
  pwr_tString80 description;
  pwr_tFloat32  scantime;
  pwr_tFloat32  deadband;
  pwr_tMask	options;
  int		deadband_active;
  unsigned int	last_id;
  //char		old_value[8];
  unsigned int value_size;
  void *old_value;
  int 		first_storage;
  unsigned int  attrnum;
  vector<sev_attr>	attr;
  pwr_tStatus	status;
  pwr_tStatus   logged_status;
  sev_valuecache *cache;
  unsigned int	idx;
  int deleted;
};


class sev_db {
 public:
  vector<sev_item> m_items;

  sev_db() {}
  virtual ~sev_db() {}
  virtual int check_item( pwr_tStatus *sts, pwr_tOid oid, char *oname, char *aname, 
			  pwr_tDeltaTime storatetime, pwr_eType type, unsigned int size, 
			  char *description, char *unit, pwr_tFloat32 scantime, 
			  pwr_tFloat32 deadband, pwr_tMask options, unsigned int *idx) 
  { *sts = 0; return 0;}
  virtual int add_item( pwr_tStatus *sts, pwr_tOid oid, char *oname, char *aname, 
			pwr_tDeltaTime storagetime, pwr_eType type, unsigned int size, 
			char *description, char *unit, pwr_tFloat32 scantime, 
			pwr_tFloat32 deadband, pwr_tMask options, unsigned int *idx) 
    { *sts = 0; return 0;}
  virtual int delete_item( pwr_tStatus *sts, pwr_tOid oid, char *aname) { *sts = 0; return 0;}
  virtual int store_value( pwr_tStatus *sts, int item_idx, int attr_idx,
			   pwr_tTime time, void *buf, unsigned int size) { *sts = 0; return 0;}
  virtual int get_values( pwr_tStatus *sts, pwr_tOid oid, pwr_tMask options, float deadband, 
			  char *aname, pwr_eType type, 
			  unsigned int size, pwr_tFloat32 scantime, pwr_tTime *creatime, pwr_tTime *starttime, 
			  pwr_tTime *endtime, int maxsize, pwr_tTime **tbuf, void **vbuf, 
			  unsigned int *bsize) { *sts = 0; return 0;}
  virtual int get_items( pwr_tStatus *sts) { *sts = 0; return 0;}
  virtual int delete_old_data( pwr_tStatus *sts, char *tablename, 
			       pwr_tMask options, pwr_tTime limit, pwr_tFloat32 scantime, pwr_tFloat32 garbagecycle) 
  { *sts = 0; return 0;}

  virtual int check_objectitem( pwr_tStatus *sts, char *tablename, pwr_tOid oid, char *oname, char *aname, 
				pwr_tDeltaTime storagetime, 
				char *description, pwr_tFloat32 scantime, 
				pwr_tFloat32 deadband, pwr_tMask options, unsigned int attrnum,
				sev_sHistAttr *attr, unsigned int *idx) { *sts = 0; return 0;}
  virtual int add_objectitem( pwr_tStatus *sts, char *tablename, pwr_tOid oid, char *oname, char *aname, 
			      pwr_tDeltaTime storagetime,
			      char *description, pwr_tFloat32 scantime, 
			      pwr_tFloat32 deadband, pwr_tMask options, unsigned int attrnum,
			      sev_sHistAttr *attr, unsigned int *idx) { *sts = 0; return 0;} 
  virtual int store_objectitem( pwr_tStatus *sts, char *tablename, pwr_tOid oid, char *oname, char *aname, 
                                pwr_tDeltaTime storagetime, char *description, pwr_tFloat32 scantime, pwr_tFloat32 deadband, pwr_tMask options) { return 0;}
  virtual int store_event( pwr_tStatus *sts, int item_idx, sev_event *ep) { *sts = 0; return 0;}
  virtual int get_item( pwr_tStatus *sts, sev_item *item, pwr_tOid oid, char *attributename) { *sts = 0; return 0;}
  virtual int get_objectitem( pwr_tStatus *sts, sev_item *item, pwr_tOid oid, char *attributename) { *sts = 0; return 0;}
  virtual int get_objectitems( pwr_tStatus *sts) { *sts = 0; return 0;}
  virtual int check_objectitemattr( pwr_tStatus *sts, char *tablename, pwr_tOid oid, char *aname, char *oname, 
																	  pwr_eType type, unsigned int size, unsigned int *idx) { *sts = 0; return 0;}
  virtual int delete_old_objectdata( pwr_tStatus *sts, char *tablename, 
                                     pwr_tMask options, pwr_tTime limit, pwr_tFloat32 scantime, pwr_tFloat32 garbagecycle) { *sts = 0; return 0;}
  virtual int get_objectvalues( pwr_tStatus *sts, sev_item *item,
                                unsigned int size, pwr_tTime *starttime, pwr_tTime *endtime, 
                                int maxsize, pwr_tTime **tbuf, void **vbuf, unsigned int *bsize) { *sts = 0; return 0;}
  virtual int handle_objectchange(pwr_tStatus *sts, char *tablename, unsigned int item_idx, bool newObject) { *sts = 0; return 0;}
  virtual int repair_table( pwr_tStatus *sts, char *tablename) { *sts = 0; return 0;}
  virtual int alter_engine( pwr_tStatus *sts, char *tablename) { *sts = 0; return 0;}
  virtual int optimize( pwr_tStatus *sts, char *tablename) { *sts = 0; return 0;}
  virtual int store_stat( sev_sStat *stat) { return 0;}
  virtual int begin_transaction() { return 0;}
  virtual int commit_transaction() { return 0;}
  virtual char *dbName() { return 0;}
  
  static sev_db *open_database( sev_eDbType type);
  static int get_systemname( char *name);
};
#endif
