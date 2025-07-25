/*
	see copyright notice in squirrel.h
*/
#include "sqpcheader.h"
#include "sqvm.h"
#include "sqstring.h"
#include "sqtable.h"
#include "sqarray.h"
#include "sqfuncproto.h"
#include "sqclosure.h"
#include "squserdata.h"
#include "sqcompiler.h"
#include "sqfuncstate.h"
#include "sqclass.h"

template <Squirk Q>
bool sq_aux_gettypedarg(HSQUIRRELVM<Q> v,SQInteger idx,SQObjectType type,SQObjectPtr<Q> **o)
{
	*o = &stack_get(v,idx);
	if(obj_type(**o) != type){
		SQObjectPtr oval = v->PrintObjVal(**o);
		v->Raise_Error(_SC("wrong argument type, expected '%s' got '%.50s'"),IdType2Name(type),_stringval(oval));
		return false;
	}
	return true;
}

#define _GETSAFE_OBJ(v,idx,type,o) { if(!sq_aux_gettypedarg(v,idx,type,&o)) return SQ_ERROR; }

#define sq_aux_paramscheck(v,count) \
{ \
	if(sq_gettop(v) < count){ v->Raise_Error(_SC("not enough params in the stack")); return SQ_ERROR; }\
}		

template <Squirk Q>
SQInteger sq_aux_throwobject(HSQUIRRELVM<Q> v,SQObjectPtr<Q> &e)
{
	v->_lasterror = e;
	return SQ_ERROR;
}

template <Squirk Q>
SQInteger sq_aux_invalidtype(HSQUIRRELVM<Q> v,SQObjectType type)
{
	scsprintf(_ss(v)->GetScratchPad(100), _SC("unexpected type %s"), IdType2Name(type));
	return sq_throwerror(v, _ss(v)->GetScratchPad(-1));
}

template <Squirk Q>
HSQUIRRELVM<Q> sq_open(SQInteger initialstacksize)
{
	SQSharedState<Q> *ss;
	SQVM<Q> *v;
	sq_new(ss, SQSharedState<Q>);
	ss->Init();
	v = (SQVM<Q> *)SQ_MALLOC(sizeof(SQVM<Q>));
	new (v) SQVM<Q>(ss);
	ss->_root_vm = v;
	if(v->Init(NULL, initialstacksize)) {
		return v;
	} else {
		sq_delete(v, SQVM<Q>);
		return NULL;
	}
	return v;
}

template <Squirk Q>
HSQUIRRELVM<Q> sq_newthread(HSQUIRRELVM<Q> friendvm, SQInteger initialstacksize)
{
	SQSharedState<Q> *ss;
	SQVM<Q> *v;
	ss=_ss(friendvm);
	
	v= (SQVM<Q> *)SQ_MALLOC(sizeof(SQVM<Q>));
	new (v) SQVM<Q>(ss);
	
	if(v->Init(friendvm, initialstacksize)) {
		friendvm->Push(v);
		return v;
	} else {
		sq_delete(v, SQVM<Q>);
		return NULL;
	}
}

template <Squirk Q>
SQInteger sq_getvmstate(HSQUIRRELVM<Q> v)
{
	if(v->_suspended)
		return SQ_VMSTATE_SUSPENDED;
	else { 
		if(v->_callsstacksize != 0) return SQ_VMSTATE_RUNNING;
		else return SQ_VMSTATE_IDLE;
	}
}

template <Squirk Q>
void sq_seterrorhandler(HSQUIRRELVM<Q> v)
{
	SQObject<Q> o = stack_get(v, -1);
	if(sq_isclosure(o) || sq_isnativeclosure(o) || sq_isnull(o)) {
		v->_errorhandler = o;
		v->Pop();
	}
}

template <Squirk Q>
void sq_setdebughook(HSQUIRRELVM<Q> v)
{
	SQObject<Q> o = stack_get(v,-1);
	if(sq_isclosure(o) || sq_isnativeclosure(o) || sq_isnull(o)) {
		v->_debughook = o;
		v->Pop();
	}
}

template <Squirk Q>
void sq_close(HSQUIRRELVM<Q> v)
{
	SQSharedState<Q> *ss = _ss(v);
	_thread(ss->_root_vm)->Finalize();
	sq_delete(ss, SQSharedState<Q>);
}

template <Squirk Q>
SQRESULT sq_compile(HSQUIRRELVM<Q> v,SQLEXREADFUNC read,SQUserPointer p,const SQChar *sourcename,SQBool raiseerror)
{
	SQObjectPtr<Q> o;
	if(Compile(v, read, p, sourcename, o, raiseerror?true:false, _ss(v)->_debuginfo)) {
		v->Push(SQClosure<Q>::Create(_ss(v), _funcproto(o)));
		return SQ_OK;
	}
	return SQ_ERROR;
}

template <Squirk Q>
void sq_enabledebuginfo(HSQUIRRELVM<Q> v, SQBool enable)
{
	_ss(v)->_debuginfo = enable?true:false;
}

template <Squirk Q>
void sq_notifyallexceptions(HSQUIRRELVM<Q> v, SQBool enable)
{
	_ss(v)->_notifyallexceptions = enable?true:false;
}

template <Squirk Q>
void sq_addref(HSQUIRRELVM<Q> v,HSQOBJECT<Q> *po)
{
	if(!ISREFCOUNTED(obj_type(*po))) return;
#ifdef NO_GARBAGE_COLLECTOR
	__AddRef(po->_type,po->_unVal);
#else
	_ss(v)->_refs_table.AddRef(*po);
#endif
}

template <Squirk Q>
SQBool sq_release(HSQUIRRELVM<Q> v,HSQOBJECT<Q> *po)
{
	if(!ISREFCOUNTED(obj_type(*po))) return SQTrue;
#ifdef NO_GARBAGE_COLLECTOR
	__Release(po->_type,po->_unVal);
	return SQFalse; //the ret val doesn't work(and cannot be fixed)
#else
	return _ss(v)->_refs_table.Release(*po);
#endif
}

template <Squirk Q>
const SQChar *sq_objtostring(HSQOBJECT<Q> *o) 
{
	if(sq_type(*o) == OT_STRING) {
		return _stringval(*o);
	}
	return NULL;
}

template <Squirk Q>
SQInteger sq_objtointeger(HSQOBJECT<Q> *o) 
{
	if(sq_isnumeric(*o)) {
		return tointeger(*o);
	}
	return 0;
}

template <Squirk Q>
SQFloat sq_objtofloat(HSQOBJECT<Q> *o) 
{
	if(sq_isnumeric(*o)) {
		return tofloat(*o);
	}
	return 0;
}

template <Squirk Q>
SQBool sq_objtobool(HSQOBJECT<Q> *o) 
{
	if(sq_isbool(*o)) {
		return _integer(*o);
	}
	return SQFalse;
}

template <Squirk Q>
void sq_pushnull(HSQUIRRELVM<Q> v)
{
	v->Push(_null_<Q>);
}

template <Squirk Q>
void sq_pushstring(HSQUIRRELVM<Q> v,const SQChar *s,SQInteger len)
{
	if(s)
		v->Push(SQObjectPtr<Q>(SQString<Q>::Create(_ss(v), s, len)));
	else v->Push(_null_<Q>);
}

template <Squirk Q>
void sq_pushinteger(HSQUIRRELVM<Q> v,SQInteger n)
{
	v->Push(n);
}

template <Squirk Q>
void sq_pushbool(HSQUIRRELVM<Q> v,SQBool b)
{
	v->Push(b?true:false);
}

template <Squirk Q>
void sq_pushfloat(HSQUIRRELVM<Q> v,SQFloat n)
{
	v->Push(n);
}

template <Squirk Q>
void sq_pushuserpointer(HSQUIRRELVM<Q> v,SQUserPointer p)
{
	v->Push(p);
}

template <Squirk Q>
SQUserPointer sq_newuserdata(HSQUIRRELVM<Q> v,SQUnsignedInteger size)
{
	SQUserData<Q> *ud = SQUserData<Q>::Create(_ss(v), size);
	v->Push(ud);
	return ud->_val;
}

template <Squirk Q>
void sq_newtable(HSQUIRRELVM<Q> v)
{
	v->Push(SQTable<Q>::Create(_ss(v), 0));
}

template <Squirk Q>
void sq_newarray(HSQUIRRELVM<Q> v,SQInteger size)
{
	v->Push(SQArray<Q>::Create(_ss(v), size));
}

template <Squirk Q>
SQRESULT sq_newclass(HSQUIRRELVM<Q> v,SQBool hasbase)
{
	SQClass<Q> *baseclass = NULL;
	if(hasbase) {
		SQObjectPtr<Q> &base = stack_get(v,-1);
		if(obj_type(base) != OT_CLASS)
			return sq_throwerror(v,_SC("invalid base type"));
		baseclass = _class(base);
	}
	SQClass<Q> *newclass = SQClass<Q>::Create(_ss(v), baseclass);
	if(baseclass) v->Pop();
	v->Push(newclass);	
	return SQ_OK;
}

template <Squirk Q>
SQBool sq_instanceof(HSQUIRRELVM<Q> v)
{
	SQObjectPtr<Q> &inst = stack_get(v,-1);
	SQObjectPtr<Q> &cl = stack_get(v,-2);
	if(obj_type(inst) != OT_INSTANCE || obj_type(cl) != OT_CLASS)
		return sq_throwerror(v,_SC("invalid param type"));
	return _instance(inst)->InstanceOf(_class(cl))?SQTrue:SQFalse;
}

template <Squirk Q>
SQRESULT sq_arrayappend(HSQUIRRELVM<Q> v,SQInteger idx)
{
	sq_aux_paramscheck(v,2);
	SQObjectPtr<Q> *arr;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
	_array(*arr)->Append(v->GetUp(-1));
	v->Pop(1);
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_arraypop(HSQUIRRELVM<Q> v,SQInteger idx,SQBool pushval)
{
	sq_aux_paramscheck(v, 1);
	SQObjectPtr<Q> *arr;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
	if(_array(*arr)->Size() > 0) {
        if(pushval != 0){ v->Push(_array(*arr)->Top()); }
		_array(*arr)->Pop();
		return SQ_OK;
	}
	return sq_throwerror(v, _SC("empty array"));
}

template <Squirk Q>
SQRESULT sq_arrayresize(HSQUIRRELVM<Q> v,SQInteger idx,SQInteger newsize)
{
	sq_aux_paramscheck(v,1);
	SQObjectPtr<Q> *arr;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
	if(newsize >= 0) {
		_array(*arr)->Resize(newsize);
		return SQ_OK;
	}
	return sq_throwerror(v,_SC("negative size"));
}

template <Squirk Q>
SQRESULT sq_arrayreverse(HSQUIRRELVM<Q> v,SQInteger idx)
{
	sq_aux_paramscheck(v, 1);
	SQObjectPtr<Q> *o;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,o);
	SQArray<Q> *arr = _array(*o);
	if(arr->Size() > 0) {
		SQObjectPtr<Q> t;
		SQInteger size = arr->Size();
		SQInteger n = size >> 1; size -= 1;
		for(SQInteger i = 0; i < n; i++) {
			t = arr->_values[i];
			arr->_values[i] = arr->_values[size-i];
			arr->_values[size-i] = t;
		}
		return SQ_OK;
	}
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_arrayremove(HSQUIRRELVM<Q> v,SQInteger idx,SQInteger itemidx)
{
	sq_aux_paramscheck(v, 1); 
	SQObjectPtr<Q> *arr;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,arr); 
	return _array(*arr)->Remove(itemidx) ? SQ_OK : sq_throwerror(v,_SC("index out of range")); 
}

template <Squirk Q>
SQRESULT sq_arrayinsert(HSQUIRRELVM<Q> v,SQInteger idx,SQInteger destpos)
{
	sq_aux_paramscheck(v, 1); 
	SQObjectPtr<Q> *arr;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
	SQRESULT ret = _array(*arr)->Insert(destpos, v->GetUp(-1)) ? SQ_OK : sq_throwerror(v,_SC("index out of range"));
	v->Pop();
	return ret;
}

template <Squirk Q>
void sq_newclosure(HSQUIRRELVM<Q> v,SQFUNCTION<Q> func,SQUnsignedInteger nfreevars)
{
	SQNativeClosure<Q> *nc = SQNativeClosure<Q>::Create(_ss(v), func);
	nc->_nparamscheck = 0;
	for(SQUnsignedInteger i = 0; i < nfreevars; i++) {
		nc->_outervalues.push_back(v->Top());
		v->Pop();
	}
	v->Push(SQObjectPtr<Q>(nc));
}

template <Squirk Q>
SQRESULT sq_getclosureinfo(HSQUIRRELVM<Q> v,SQInteger idx,SQUnsignedInteger *nparams,SQUnsignedInteger *nfreevars)
{
	SQObject<Q> o = stack_get(v, idx);
	if(sq_isclosure(o)) {
		SQClosure<Q> *c = _closure(o);
		SQFunctionProto<Q> *proto = _funcproto(c->_function);
		*nparams = (SQUnsignedInteger)proto->_nparameters;
        *nfreevars = (SQUnsignedInteger)c->_outervalues.size();
		return SQ_OK;
	}
	return sq_throwerror(v,_SC("the object is not a closure"));
}

template <Squirk Q>
SQRESULT sq_setnativeclosurename(HSQUIRRELVM<Q> v,SQInteger idx,const SQChar *name)
{
	SQObject<Q> o = stack_get(v, idx);
	if(sq_isnativeclosure(o)) {
		SQNativeClosure<Q> *nc = _nativeclosure(o);
		nc->_name = SQString<Q>::Create(_ss(v),name);
		return SQ_OK;
	}
	return sq_throwerror(v,_SC("the object is not a nativeclosure"));
}

template <Squirk Q>
SQRESULT sq_setparamscheck(HSQUIRRELVM<Q> v,SQInteger nparamscheck,const SQChar *typemask)
{
	SQObject<Q> o = stack_get(v, -1);
	if(!sq_isnativeclosure(o))
		return sq_throwerror(v, _SC("native closure expected"));
	SQNativeClosure<Q> *nc = _nativeclosure(o);
	nc->_nparamscheck = nparamscheck;
	if(typemask) {
		SQIntVec res;
		if(!CompileTypemask(res, typemask))
			return sq_throwerror(v, _SC("invalid typemask"));
		nc->_typecheck.copy(res);
	}
	else {
		nc->_typecheck.resize(0);
	}
	if(nparamscheck == SQ_MATCHTYPEMASKSTRING) {
		nc->_nparamscheck = nc->_typecheck.size();
	}
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_bindenv(HSQUIRRELVM<Q> v,SQInteger idx)
{
	SQObjectPtr<Q> &o = stack_get(v,idx);
	if(!sq_isnativeclosure(o) &&
		!sq_isclosure(o))
		return sq_throwerror(v,_SC("the target is not a closure"));
    SQObjectPtr<Q> &env = stack_get(v,-1);
	if(!sq_istable(env) &&
		!sq_isclass(env) &&
		!sq_isinstance(env))
		return sq_throwerror(v,_SC("invalid environment"));
	SQObjectPtr<Q> w = _refcounted(env)->GetWeakRef(obj_type(env));
	SQObjectPtr<Q> ret;
	if(sq_isclosure(o)) {
		SQClosure<Q> *c = _closure(o)->Clone();
		c->_env = w;
		ret = c;
	}
	else { //then must be a native closure
		SQNativeClosure<Q> *c = _nativeclosure(o)->Clone();
		c->_env = w;
		ret = c;
	}
	v->Pop();
	v->Push(ret);
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_clear(HSQUIRRELVM<Q> v,SQInteger idx)
{
	SQObject<Q> &o=stack_get(v,idx);
	switch(obj_type(o)) {
		case OT_TABLE: _table(o)->Clear();	break;
		case OT_ARRAY: _array(o)->Resize(0); break;
		default:
			return sq_throwerror(v, _SC("clear only works on table and array"));
		break;

	}
	return SQ_OK;
}

template <Squirk Q>
void sq_pushroottable(HSQUIRRELVM<Q> v)
{
	v->Push(v->_roottable);
}

template <Squirk Q>
void sq_pushregistrytable(HSQUIRRELVM<Q> v)
{
	v->Push(_ss(v)->_registry);
}

template <Squirk Q>
void sq_pushconsttable(HSQUIRRELVM<Q> v)
{
	v->Push(_ss(v)->_consts);
}

template <Squirk Q>
SQRESULT sq_setroottable(HSQUIRRELVM<Q> v)
{
	SQObject<Q> o = stack_get(v, -1);
	if(sq_istable(o) || sq_isnull(o)) {
		v->_roottable = o;
		v->Pop();
		return SQ_OK;
	}
	return sq_throwerror(v, _SC("ivalid type"));
}

template <Squirk Q>
SQRESULT sq_setconsttable(HSQUIRRELVM<Q> v)
{
	SQObject<Q> o = stack_get(v, -1);
	if(sq_istable(o)) {
		_ss(v)->_consts = o;
		v->Pop();
		return SQ_OK;
	}
	return sq_throwerror(v, _SC("ivalid type, expected table"));
}

template <Squirk Q>
void sq_setforeignptr(HSQUIRRELVM<Q> v,SQUserPointer p)
{
	v->_foreignptr = p;
}

template <Squirk Q>
SQUserPointer sq_getforeignptr(HSQUIRRELVM<Q> v)
{
	return v->_foreignptr;
}

template <Squirk Q>
void sq_push(HSQUIRRELVM<Q> v,SQInteger idx)
{
	v->Push(stack_get(v, idx));
}

template <Squirk Q>
SQObjectType sq_gettype(HSQUIRRELVM<Q> v,SQInteger idx)
{
	return obj_type(stack_get(v, idx));
}

template <Squirk Q>
void sq_tostring(HSQUIRRELVM<Q> v,SQInteger idx)
{
	SQObjectPtr<Q> &o = stack_get(v, idx);
	SQObjectPtr<Q> res;
	v->ToString(o,res);
	v->Push(res);
}

template <Squirk Q>
void sq_tobool(HSQUIRRELVM<Q> v, SQInteger idx, SQBool *b)
{
	SQObjectPtr<Q> &o = stack_get(v, idx);
	*b = v->IsFalse(o)?SQFalse:SQTrue;
}

template <Squirk Q>
SQRESULT sq_getinteger(HSQUIRRELVM<Q> v,SQInteger idx,SQInteger *i)
{
	SQObjectPtr<Q> &o = stack_get(v, idx);
	if(sq_isnumeric(o)) {
		*i = tointeger(o);
		return SQ_OK;
	}
	return SQ_ERROR;
}

template <Squirk Q>
SQRESULT sq_getfloat(HSQUIRRELVM<Q> v,SQInteger idx,SQFloat *f)
{
	SQObjectPtr<Q> &o = stack_get(v, idx);
	if(sq_isnumeric(o)) {
		*f = tofloat(o);
		return SQ_OK;
	}
	return SQ_ERROR;
}

template <Squirk Q>
SQRESULT sq_getbool(HSQUIRRELVM<Q> v,SQInteger idx,SQBool *b)
{
	SQObjectPtr<Q> &o = stack_get(v, idx);
	if(sq_isbool(o)) {
		*b = _integer(o);
		return SQ_OK;
	}
	return SQ_ERROR;
}

template <Squirk Q>
SQRESULT sq_getstring(HSQUIRRELVM<Q> v,SQInteger idx,const SQChar **c)
{
	SQObjectPtr<Q> *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_STRING,o);
	*c = _stringval(*o);
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_getthread(HSQUIRRELVM<Q> v,SQInteger idx,HSQUIRRELVM<Q> *thread)
{
	SQObjectPtr<Q> *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_THREAD,o);
	*thread = _thread(*o);
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_clone(HSQUIRRELVM<Q> v,SQInteger idx)
{
	SQObjectPtr<Q> &o = stack_get(v,idx);
	v->Push(_null_<Q>);
	if(!v->Clone(o, stack_get(v, -1))){
		v->Pop();
		return sq_aux_invalidtype(v, obj_type(o));
	}
	return SQ_OK;
}

template <Squirk Q>
SQInteger sq_getsize(HSQUIRRELVM<Q> v, SQInteger idx)
{
	SQObjectPtr<Q> &o = stack_get(v, idx);
	SQObjectType type = obj_type(o);
	switch(type) {
	case OT_STRING:		return _string(o)->_len;
	case OT_TABLE:		return _table(o)->CountUsed();
	case OT_ARRAY:		return _array(o)->Size();
	case OT_USERDATA:	return _userdata(o)->_size;
	default:
		return sq_aux_invalidtype(v, type);
	}
}

template <Squirk Q>
SQRESULT sq_getuserdata(HSQUIRRELVM<Q> v,SQInteger idx,SQUserPointer *p,SQUserPointer *typetag)
{
	SQObjectPtr<Q> *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_USERDATA,o);
	(*p) = _userdataval(*o);
	if(typetag) *typetag = _userdata(*o)->_typetag;
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_settypetag(HSQUIRRELVM<Q> v,SQInteger idx,SQUserPointer typetag)
{
	SQObjectPtr<Q> &o = stack_get(v,idx);
	switch(obj_type(o)) {
		case OT_USERDATA:	_userdata(o)->_typetag = typetag;	break;
		case OT_CLASS:		_class(o)->_typetag = typetag;		break;
		default:			return sq_throwerror(v,_SC("invalid object type"));
	}
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_getobjtypetag(HSQOBJECT<Q> *o,SQUserPointer * typetag)
{
  switch(obj_type(*o)) {
    case OT_INSTANCE: *typetag = _instance(*o)->_class->_typetag; break;
    case OT_USERDATA: *typetag = _userdata(*o)->_typetag; break;
    case OT_CLASS:    *typetag = _class(*o)->_typetag; break;
    default: return SQ_ERROR;
  }
  return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_gettypetag(HSQUIRRELVM<Q> v,SQInteger idx,SQUserPointer *typetag)
{
	SQObjectPtr<Q> &o = stack_get(v,idx);
	if(SQ_FAILED(sq_getobjtypetag(&o,typetag)))
		return sq_throwerror(v,_SC("invalid object type"));
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_getuserpointer(HSQUIRRELVM<Q> v, SQInteger idx, SQUserPointer *p)
{
	SQObjectPtr<Q> *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_USERPOINTER,o);
	(*p) = _userpointer(*o);
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_setinstanceup(HSQUIRRELVM<Q> v, SQInteger idx, SQUserPointer p)
{
	SQObjectPtr<Q> &o = stack_get(v,idx);
	if(obj_type(o) != OT_INSTANCE) return sq_throwerror(v,_SC("the object is not a class instance"));
	_instance(o)->_userpointer = p;
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_setclassudsize(HSQUIRRELVM<Q> v, SQInteger idx, SQInteger udsize)
{
	SQObjectPtr<Q> &o = stack_get(v,idx);
	if(obj_type(o) != OT_CLASS) return sq_throwerror(v,_SC("the object is not a class"));
	if(_class(o)->_locked) return sq_throwerror(v,_SC("the class is locked"));
	_class(o)->_udsize = udsize;
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_getinstanceup(HSQUIRRELVM<Q> v, SQInteger idx, SQUserPointer *p,SQUserPointer typetag)
{
	SQObjectPtr<Q> &o = stack_get(v,idx);
	if(obj_type(o) != OT_INSTANCE) return sq_throwerror(v,_SC("the object is not a class instance"));
	(*p) = _instance(o)->_userpointer;
	if(typetag != 0) {
		SQClass<Q> *cl = _instance(o)->_class;
		do{
			if(cl->_typetag == typetag)
				return SQ_OK;
			cl = cl->_base;
		}while(cl != NULL);
		return sq_throwerror(v,_SC("invalid type tag"));
	}
	return SQ_OK;
}

template <Squirk Q>
SQInteger sq_gettop(HSQUIRRELVM<Q> v)
{
	return (v->_top) - v->_stackbase;
}

template <Squirk Q>
void sq_settop(HSQUIRRELVM<Q> v, SQInteger newtop)
{
	SQInteger top = sq_gettop(v);
	if(top > newtop)
		sq_pop(v, top - newtop);
	else
		while(top++ < newtop) sq_pushnull(v);
}

template <Squirk Q>
void sq_pop(HSQUIRRELVM<Q> v, SQInteger nelemstopop)
{
	assert(v->_top >= nelemstopop);
	v->Pop(nelemstopop);
}

template <Squirk Q>
void sq_poptop(HSQUIRRELVM<Q> v)
{
	assert(v->_top >= 1);
    v->Pop();
}

template <Squirk Q>
void sq_remove(HSQUIRRELVM<Q> v, SQInteger idx)
{
	v->Remove(idx);
}

template <Squirk Q>
SQInteger sq_cmp(HSQUIRRELVM<Q> v)
{
	SQInteger res;
	v->ObjCmp(stack_get(v, -1), stack_get(v, -2),res);
	return res;
}

template <Squirk Q>
SQRESULT sq_newslot(HSQUIRRELVM<Q> v, SQInteger idx, SQBool bstatic)
{
	sq_aux_paramscheck(v, 3);
	SQObjectPtr<Q> &self = stack_get(v, idx);
	if(obj_type(self) == OT_TABLE || obj_type(self) == OT_CLASS) {
		SQObjectPtr<Q> &key = v->GetUp(-2);
		if(obj_type(key) == OT_NULL) return sq_throwerror(v, _SC("null is not a valid key"));
		v->NewSlot(self, key, v->GetUp(-1),bstatic?true:false);
		v->Pop(2);
	}
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_deleteslot(HSQUIRRELVM<Q> v,SQInteger idx,SQBool pushval)
{
	sq_aux_paramscheck(v, 2);
	SQObjectPtr<Q> *self;
	_GETSAFE_OBJ(v, idx, OT_TABLE,self);
	SQObjectPtr<Q> &key = v->GetUp(-1);
	if(obj_type(key) == OT_NULL) return sq_throwerror(v, _SC("null is not a valid key"));
	SQObjectPtr<Q> res;
	if(!v->DeleteSlot(*self, key, res)){
		return SQ_ERROR;
	}
	if(pushval)	v->GetUp(-1) = res;
	else v->Pop(1);
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_set(HSQUIRRELVM<Q> v,SQInteger idx)
{
	SQObjectPtr<Q> &self = stack_get(v, idx);
	if(v->Set(self, v->GetUp(-2), v->GetUp(-1),false)) {
		v->Pop(2);
		return SQ_OK;
	}
	v->Raise_IdxError(v->GetUp(-2));return SQ_ERROR;
}

template <Squirk Q>
SQRESULT sq_rawset(HSQUIRRELVM<Q> v,SQInteger idx)
{
	SQObjectPtr<Q> &self = stack_get(v, idx);
	if(obj_type(v->GetUp(-2)) == OT_NULL) return sq_throwerror(v, _SC("null key"));
	switch(obj_type(self)) {
	case OT_TABLE:
		_table(self)->NewSlot(v->GetUp(-2), v->GetUp(-1));
		v->Pop(2);
		return SQ_OK;
	break;
	case OT_CLASS:
		_class(self)->NewSlot(_ss(v), v->GetUp(-2), v->GetUp(-1),false);
		v->Pop(2);
		return SQ_OK;
	break;
	case OT_INSTANCE:
		if(_instance(self)->Set(v->GetUp(-2), v->GetUp(-1))) {
			v->Pop(2);
			return SQ_OK;
		}
	break;
	case OT_ARRAY:
		if(v->Set(self, v->GetUp(-2), v->GetUp(-1),false)) {
			v->Pop(2);
			return SQ_OK;
		}
	break;
	default:
		v->Pop(2);
		return sq_throwerror(v, _SC("rawset works only on array/table/class and instance"));
	}
	v->Raise_IdxError(v->GetUp(-2));return SQ_ERROR;
}

template <Squirk Q>
SQRESULT sq_setdelegate(HSQUIRRELVM<Q> v,SQInteger idx)
{
	SQObjectPtr<Q> &self = stack_get(v, idx);
	SQObjectPtr<Q> &mt = v->GetUp(-1);
	SQObjectType type = obj_type(self);
	switch(type) {
	case OT_TABLE:
		if(obj_type(mt) == OT_TABLE) {
			if(!_table(self)->SetDelegate(_table(mt))) return sq_throwerror(v, _SC("delagate cycle")); v->Pop();}
		else if(obj_type(mt)==OT_NULL) {
			_table(self)->SetDelegate(NULL); v->Pop(); }
		else return sq_aux_invalidtype(v,type);
		break;
	case OT_USERDATA:
		if(obj_type(mt)==OT_TABLE) {
			_userdata(self)->SetDelegate(_table(mt)); v->Pop(); }
		else if(obj_type(mt)==OT_NULL) {
			_userdata(self)->SetDelegate(NULL); v->Pop(); }
		else return sq_aux_invalidtype(v, type);
		break;
	default:
			return sq_aux_invalidtype(v, type);
		break;
	}
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_rawdeleteslot(HSQUIRRELVM<Q> v,SQInteger idx,SQBool pushval)
{
	sq_aux_paramscheck(v, 2);
	SQObjectPtr<Q> *self;
	_GETSAFE_OBJ(v, idx, OT_TABLE,self);
	SQObjectPtr<Q> &key = v->GetUp(-1);
	SQObjectPtr<Q> t;
	if(_table(*self)->Get(key,t)) {
		_table(*self)->Remove(key);
	}
	if(pushval != 0)
		if(pushval)	v->GetUp(-1) = t;
	else
		v->Pop(1);
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_getdelegate(HSQUIRRELVM<Q> v,SQInteger idx)
{
	SQObjectPtr<Q> &self=stack_get(v,idx);
	switch(obj_type(self)){
	case OT_TABLE:
	case OT_USERDATA:
		if(!_delegable(self)->_delegate){
			v->Push(_null_<Q>);
			break;
		}
		v->Push(SQObjectPtr<Q>(_delegable(self)->_delegate));
		break;
	default: return sq_throwerror(v,_SC("wrong type")); break;
	}
	return SQ_OK;
	
}

template <Squirk Q>
SQRESULT sq_get(HSQUIRRELVM<Q> v,SQInteger idx)
{
	SQObjectPtr<Q> &self=stack_get(v,idx);
	if(v->Get(self,v->GetUp(-1),v->GetUp(-1),false,false))
		return SQ_OK;
	v->Pop(1);
	return sq_throwerror(v,_SC("the index doesn't exist"));
}

template <Squirk Q>
SQRESULT sq_rawget(HSQUIRRELVM<Q> v,SQInteger idx)
{
	SQObjectPtr<Q> &self=stack_get(v,idx);
	switch(obj_type(self)) {
	case OT_TABLE:
		if(_table(self)->Get(v->GetUp(-1),v->GetUp(-1)))
			return SQ_OK;
		break;
	case OT_CLASS:
		if(_class(self)->Get(v->GetUp(-1),v->GetUp(-1)))
			return SQ_OK;
		break;
	case OT_INSTANCE:
		if(_instance(self)->Get(v->GetUp(-1),v->GetUp(-1)))
			return SQ_OK;
		break;
	case OT_ARRAY:
		if(v->Get(self,v->GetUp(-1),v->GetUp(-1),false,false))
			return SQ_OK;
		break;
	default:
		v->Pop(1);
		return sq_throwerror(v,_SC("rawget works only on array/table/instance and class"));
	}	
	v->Pop(1);
	return sq_throwerror(v,_SC("the index doesn't exist"));
}

template <Squirk Q>
SQRESULT sq_getstackobj(HSQUIRRELVM<Q> v,SQInteger idx,HSQOBJECT<Q> *po)
{
	*po=stack_get(v,idx);
	return SQ_OK;
}

template <Squirk Q>
const SQChar *sq_getlocal(HSQUIRRELVM<Q> v,SQUnsignedInteger level,SQUnsignedInteger idx)
{
	SQUnsignedInteger cstksize=v->_callsstacksize;
	SQUnsignedInteger lvl=(cstksize-level)-1;
	SQInteger stackbase=v->_stackbase;
	if(lvl<cstksize){
		for(SQUnsignedInteger i=0;i<level;i++){
			auto &ci=v->_callsstack[(cstksize-i)-1];
			stackbase-=ci._prevstkbase;
		}
		auto &ci=v->_callsstack[lvl];
		if(obj_type(ci._closure)!=OT_CLOSURE)
			return NULL;
		SQClosure<Q> *c=_closure(ci._closure);
		SQFunctionProto<Q> *func=_funcproto(c->_function);
		if(func->_noutervalues > (SQInteger)idx) {
			v->Push(c->_outervalues[idx]);
			return _stringval(func->_outervalues[idx]._name);
		}
		idx -= func->_noutervalues;
		return func->GetLocal(v,stackbase,idx,(SQInteger)(ci._ip-func->_instructions)-1);
	}
	return NULL;
}

template <Squirk Q>
void sq_pushobject(HSQUIRRELVM<Q> v,HSQOBJECT<Q> obj)
{
	v->Push(SQObjectPtr(obj));
}

template <Squirk Q>
void sq_resetobject(HSQOBJECT<Q> *po)
{
	po->_unVal.pUserPointer=NULL;po->_type=OT_NULL;
}

template <Squirk Q>
SQRESULT sq_throwerror(HSQUIRRELVM<Q> v,const SQChar *err)
{
	v->_lasterror=SQString<Q>::Create(_ss(v),err);
	return -1;
}

template <Squirk Q>
void sq_reseterror(HSQUIRRELVM<Q> v)
{
	v->_lasterror = _null_<Q>;
}

template <Squirk Q>
void sq_getlasterror(HSQUIRRELVM<Q> v)
{
	v->Push(v->_lasterror);
}

template <Squirk Q>
void sq_reservestack(HSQUIRRELVM<Q> v,SQInteger nsize)
{
	if (((SQUnsignedInteger)v->_top + nsize) > v->_stack.size()) {
		v->_stack.resize(v->_stack.size() + ((v->_top + nsize) - v->_stack.size()));
	}
}

template <Squirk Q>
SQRESULT sq_resume(HSQUIRRELVM<Q> v,SQBool retval,SQBool raiseerror)
{
	if(obj_type(v->GetUp(-1))==OT_GENERATOR){
		v->Push(_null_<Q>); //retval
		if(!v->Execute(v->GetUp(-2),v->_top,0,v->_top,v->GetUp(-1),raiseerror,SQVM<Q>::ET_RESUME_GENERATOR))
		{v->Raise_Error(v->_lasterror); return SQ_ERROR;}
		if(!retval)
			v->Pop();
		return SQ_OK;
	}
	return sq_throwerror(v,_SC("only generators can be resumed"));
}

template <Squirk Q>
SQRESULT sq_call(HSQUIRRELVM<Q> v,SQInteger params,SQBool retval,SQBool raiseerror)
{
	SQObjectPtr<Q> res;
	if(v->Call(v->GetUp(-(params+1)),params,v->_top-params,res,raiseerror?true:false)){
		if(!v->_suspended) {
			v->Pop(params);//pop closure and args
		}
		if(retval){
			v->Push(res); return SQ_OK;
		}
		return SQ_OK;
	}
	else {
		v->Pop(params);
		return SQ_ERROR;
	}
	if(!v->_suspended)
		v->Pop(params);
	return sq_throwerror(v,_SC("call failed"));
}

template <Squirk Q>
SQRESULT sq_suspendvm(HSQUIRRELVM<Q> v)
{
	return v->Suspend();
}

template <Squirk Q>
SQRESULT sq_wakeupvm(HSQUIRRELVM<Q> v,SQBool wakeupret,SQBool retval,SQBool raiseerror,SQBool throwerror)
{
	SQObjectPtr<Q> ret;
	if(!v->_suspended)
		return sq_throwerror(v,_SC("cannot resume a vm that is not running any code"));
	if(wakeupret) {
		v->GetAt(v->_stackbase+v->_suspended_target)=v->GetUp(-1); //retval
		v->Pop();
	} else v->GetAt(v->_stackbase+v->_suspended_target)=_null_<Q>;
	if(!v->Execute(_null_<Q>,v->_top,-1,-1,ret,raiseerror,throwerror?SQVM<Q>::ET_RESUME_THROW_VM : SQVM<Q>::ET_RESUME_VM))
		return SQ_ERROR;
	if(sq_getvmstate(v) == SQ_VMSTATE_IDLE) {
		while (v->_top > 1) v->_stack[--v->_top] = _null_<Q>;
	}
	if(retval)
		v->Push(ret);
	return SQ_OK;
}

template <Squirk Q>
void sq_setreleasehook(HSQUIRRELVM<Q> v,SQInteger idx,SQRELEASEHOOK hook)
{
	if(sq_gettop(v) >= 1){
		SQObjectPtr<Q> &ud=stack_get(v,idx);
		switch( obj_type(ud) ) {
		case OT_USERDATA:	_userdata(ud)->_hook = hook;	break;
		case OT_INSTANCE:	_instance(ud)->_hook = hook;	break;
		case OT_CLASS:		_class(ud)->_hook = hook;		break;
		default: break; //shutup compiler
		}
	}
}

template <Squirk Q>
void sq_setcompilererrorhandler(HSQUIRRELVM<Q> v,SQCOMPILERERROR<Q> f)
{
	_ss(v)->_compilererrorhandler = f;
}

template <Squirk Q>
SQRESULT sq_writeclosure(HSQUIRRELVM<Q> v,SQWRITEFUNC w,SQUserPointer up)
{
	SQObjectPtr<Q> *o = NULL;
	_GETSAFE_OBJ(v, -1, OT_CLOSURE,o);
	unsigned short tag = SQ_BYTECODE_STREAM_TAG;
	if(w(up,&tag,2) != 2)
		return sq_throwerror(v,_SC("io error"));
	if(!_closure(*o)->Save(v,up,w))
		return SQ_ERROR;
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_readclosure(HSQUIRRELVM<Q> v,SQREADFUNC r,SQUserPointer up)
{
	SQObjectPtr<Q> closure;
	
	unsigned short tag;
	if(r(up,&tag,2) != 2)
		return sq_throwerror(v,_SC("io error"));
	if(tag != SQ_BYTECODE_STREAM_TAG)
		return sq_throwerror(v,_SC("invalid stream"));
	if(!SQClosure<Q>::Load(v,up,r,closure))
		return SQ_ERROR;
	v->Push(closure);
	return SQ_OK;
}

template <Squirk Q>
SQChar *sq_getscratchpad(HSQUIRRELVM<Q> v,SQInteger minsize)
{
	return _ss(v)->GetScratchPad(minsize);
}

template <Squirk Q>
SQInteger sq_collectgarbage(HSQUIRRELVM<Q> v)
{
#ifndef NO_GARBAGE_COLLECTOR
	return _ss(v)->CollectGarbage(v);
#else
	return -1;
#endif
}

template <Squirk Q>
const SQChar *sq_getfreevariable(HSQUIRRELVM<Q> v,SQInteger idx,SQUnsignedInteger nval)
{
	SQObjectPtr<Q> &self = stack_get(v,idx);
	const SQChar *name = NULL;
	if(obj_type(self) == OT_CLOSURE) {
		if(_closure(self)->_outervalues.size()>nval) {
			v->Push(_closure(self)->_outervalues[nval]);
			SQFunctionProto<Q> *fp = _funcproto(_closure(self)->_function);
			SQOuterVar<Q> &ov = fp->_outervalues[nval];
			name = _stringval(ov._name);
		}
	}
	return name;
}

template <Squirk Q>
SQRESULT sq_setfreevariable(HSQUIRRELVM<Q> v,SQInteger idx,SQUnsignedInteger nval)
{
	SQObjectPtr<Q> &self=stack_get(v,idx);
	switch(obj_type(self))
	{
	case OT_CLOSURE:
		if(_closure(self)->_outervalues.size()>nval){
			_closure(self)->_outervalues[nval]=stack_get(v,-1);
		}
		else return sq_throwerror(v,_SC("invalid free var index"));
		break;
	case OT_NATIVECLOSURE:
		if(_nativeclosure(self)->_outervalues.size()>nval){
			_nativeclosure(self)->_outervalues[nval]=stack_get(v,-1);
		}
		else return sq_throwerror(v,_SC("invalid free var index"));
		break;
	default:
		return sq_aux_invalidtype(v,obj_type(self));
	}
	v->Pop(1);
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_setattributes(HSQUIRRELVM<Q> v,SQInteger idx)
{
	SQObjectPtr<Q> *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_CLASS,o);
	SQObjectPtr<Q> &key = stack_get(v,-2);
	SQObjectPtr<Q> &val = stack_get(v,-1);
	SQObjectPtr<Q> attrs;
	if(obj_type(key) == OT_NULL) {
		attrs = _class(*o)->_attributes;
		_class(*o)->_attributes = val;
		v->Pop(2);
		v->Push(attrs);
		return SQ_OK;
	}else if(_class(*o)->GetAttributes(key,attrs)) {
		_class(*o)->SetAttributes(key,val);
		v->Pop(2);
		v->Push(attrs);
		return SQ_OK;
	}
	return sq_throwerror(v,_SC("wrong index"));
}

template <Squirk Q>
SQRESULT sq_getattributes(HSQUIRRELVM<Q> v,SQInteger idx)
{
	SQObjectPtr<Q> *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_CLASS,o);
	SQObjectPtr<Q> &key = stack_get(v,-1);
	SQObjectPtr<Q> attrs;
	if(obj_type(key) == OT_NULL) {
		attrs = _class(*o)->_attributes;
		v->Pop();
		v->Push(attrs); 
		return SQ_OK;
	}
	else if(_class(*o)->GetAttributes(key,attrs)) {
		v->Pop();
		v->Push(attrs);
		return SQ_OK;
	}
	return sq_throwerror(v,_SC("wrong index"));
}

template <Squirk Q>
SQRESULT sq_getmemberhandle(HSQUIRRELVM<Q> v, SQInteger idx, HSQMEMBERHANDLE *handle)
{
	SQObjectPtr<Q> *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_CLASS, o);
	SQObjectPtr<Q> &key = stack_get(v, -1);
	SQTable<Q> *m = _class(*o)->_members;
	SQObjectPtr<Q> val;
	if (m->Get(key, val)) {
		handle->_static = _isfield(val) ? SQFalse : SQTrue;
		handle->_index = _member_idx(val);
		v->Pop();
		return SQ_OK;
	}
	return sq_throwerror(v, _SC("wrong index"));
}

template <Squirk Q>
SQRESULT _getmemberbyhandle(HSQUIRRELVM<Q> v, SQObjectPtr<Q> &self, const HSQMEMBERHANDLE *handle, SQObjectPtr<Q> *&val)
{
	switch (sq_type(self)) {
	case OT_INSTANCE: {
		SQInstance<Q> *i = _instance(self);
		if (handle->_static) {
			SQClass<Q> *c = i->_class;
			val = &c->_methods[handle->_index].val;
		}
		else {
			val = &i->_values[handle->_index];

		}
	}
					break;
	case OT_CLASS: {
		SQClass<Q> *c = _class(self);
		if (handle->_static) {
			val = &c->_methods[handle->_index].val;
		}
		else {
			val = &c->_defaultvalues[handle->_index].val;
		}
	}
				 break;
	default:
		return sq_throwerror(v, _SC("wrong obj_type(expected class or instance)"));
	}
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_getbyhandle(HSQUIRRELVM<Q> v, SQInteger idx, const HSQMEMBERHANDLE *handle)
{
	SQObjectPtr<Q> &self = stack_get(v, idx);
	SQObjectPtr<Q> *val = NULL;
	if (SQ_FAILED(_getmemberbyhandle(v, self, handle, val))) {
		return SQ_ERROR;
	}
	v->Push(_realval(*val));
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_setbyhandle(HSQUIRRELVM<Q> v, SQInteger idx, const HSQMEMBERHANDLE *handle)
{
	SQObjectPtr<Q> &self = stack_get(v, idx);
	SQObjectPtr<Q> &newval = stack_get(v, -1);
	SQObjectPtr<Q> *val = NULL;
	if (SQ_FAILED(_getmemberbyhandle(v, self, handle, val))) {
		return SQ_ERROR;
	}
	*val = newval;
	v->Pop();
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_getbase(HSQUIRRELVM<Q> v,SQInteger idx)
{
	SQObjectPtr<Q> *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_CLASS,o);
	if(_class(*o)->_base)
		v->Push(SQObjectPtr<Q>(_class(*o)->_base));
	else
		v->Push(_null_<Q>);
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_getclass(HSQUIRRELVM<Q> v,SQInteger idx)
{
	SQObjectPtr<Q> *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_INSTANCE,o);
	v->Push(SQObjectPtr<Q>(_instance(*o)->_class));
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_createinstance(HSQUIRRELVM<Q> v,SQInteger idx)
{
	SQObjectPtr<Q> *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_CLASS,o);
	v->Push(_class(*o)->CreateInstance());
	return SQ_OK;
}

template <Squirk Q>
void sq_weakref(HSQUIRRELVM<Q> v,SQInteger idx)
{
	SQObject<Q> &o=stack_get(v,idx);
	if(ISREFCOUNTED(obj_type(o))) {
		v->Push(_refcounted(o)->GetWeakRef(obj_type(o)));
		return;
	}
	v->Push(o);
}

template <Squirk Q>
SQRESULT sq_getweakrefval(HSQUIRRELVM<Q> v,SQInteger idx)
{
	SQObjectPtr<Q> &o = stack_get(v,idx);
	if(obj_type(o) != OT_WEAKREF) {
		return sq_throwerror(v,_SC("the object must be a weakref"));
	}
	v->Push(_weakref(o)->_obj);
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_getdefaultdelegate(HSQUIRRELVM<Q> v,SQObjectType t)
{
	SQSharedState<Q> *ss = _ss(v);
	switch(t) {
	case OT_TABLE: v->Push(ss->_table_default_delegate); break;
	case OT_ARRAY: v->Push(ss->_array_default_delegate); break;
	case OT_STRING: v->Push(ss->_string_default_delegate); break;
	case OT_INTEGER: case OT_FLOAT: v->Push(ss->_number_default_delegate); break;
	case OT_GENERATOR: v->Push(ss->_generator_default_delegate); break;
	case OT_CLOSURE: case OT_NATIVECLOSURE: v->Push(ss->_closure_default_delegate); break;
	case OT_THREAD: v->Push(ss->_thread_default_delegate); break;
	case OT_CLASS: v->Push(ss->_class_default_delegate); break;
	case OT_INSTANCE: v->Push(ss->_instance_default_delegate); break;
	case OT_WEAKREF: v->Push(ss->_weakref_default_delegate); break;
	default: return sq_throwerror(v,_SC("the type doesn't have a default delegate"));
	}
	return SQ_OK;
}

template <Squirk Q>
SQRESULT sq_next(HSQUIRRELVM<Q> v,SQInteger idx)
{
	SQObjectPtr<Q> o=stack_get(v,idx),&refpos = stack_get(v,-1),realkey,val;
	if(obj_type(o) == OT_GENERATOR) {
		return sq_throwerror(v,_SC("cannot iterate a generator"));
	}
	int faketojump;
	if(!v->FOREACH_OP(o,realkey,val,refpos,0,666,faketojump))
		return SQ_ERROR;
	if(faketojump != 666) {
		v->Push(realkey);
		v->Push(val);
		return SQ_OK;
	}
	return SQ_ERROR;
}

struct BufState{
	const SQChar *buf;
	SQInteger ptr;
	SQInteger size;
};

SQInteger buf_lexfeed(SQUserPointer file)
{
	BufState *buf=(BufState*)file;
	if(buf->size<(buf->ptr+1))
		return 0;
	return buf->buf[buf->ptr++];
}

template <Squirk Q>
SQRESULT sq_compilebuffer(HSQUIRRELVM<Q> v,const SQChar *s,SQInteger size,const SQChar *sourcename,SQBool raiseerror) {
	BufState buf;
	buf.buf = s;
	buf.size = size;
	buf.ptr = 0;
	return sq_compile(v, buf_lexfeed, &buf, sourcename, raiseerror);
}

template <Squirk Q>
void sq_move(HSQUIRRELVM<Q> dest,HSQUIRRELVM<Q> src,SQInteger idx)
{
	dest->Push(stack_get(src,idx));
}

template <Squirk Q>
void sq_setprintfunc(HSQUIRRELVM<Q> v, SQPRINTFUNCTION<Q> printfunc)
{
	_ss(v)->_printfunc = printfunc;
}

template <Squirk Q>
SQPRINTFUNCTION<Q> sq_getprintfunc(HSQUIRRELVM<Q> v)
{
	return _ss(v)->_printfunc;
}

void *sq_malloc(SQUnsignedInteger size)
{
	return SQ_MALLOC(size);
}

void *sq_realloc(void* p,SQUnsignedInteger oldsize,SQUnsignedInteger newsize)
{
	return SQ_REALLOC(p,oldsize,newsize);
}

void sq_free(void *p,SQUnsignedInteger size)
{
	SQ_FREE(p,size);
}

/*vm*/
template HSQUIRRELVM<Squirk::Standard> sq_open<Squirk::Standard>(SQInteger initialstacksize);
template HSQUIRRELVM<Squirk::Standard> sq_newthread<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> friendvm, SQInteger initialstacksize);
template void sq_seterrorhandler<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);
template void sq_close<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);
template void sq_setforeignptr<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQUserPointer p);
template SQUserPointer sq_getforeignptr<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);
template void sq_setprintfunc<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQPRINTFUNCTION<Squirk::Standard> printfunc);
template SQPRINTFUNCTION<Squirk::Standard> sq_getprintfunc<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);
template SQRESULT sq_suspendvm<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);
template SQRESULT sq_wakeupvm<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQBool resumedret, SQBool retval, SQBool raiseerror, SQBool throwerror);
template SQInteger sq_getvmstate<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);

/*compiler*/
template SQRESULT sq_compile<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQLEXREADFUNC read, SQUserPointer p, const SQChar *sourcename, SQBool raiseerror);
template SQRESULT sq_compilebuffer<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, const SQChar *s, SQInteger size, const SQChar *sourcename, SQBool raiseerror);
template void sq_enabledebuginfo<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQBool enable);
template void sq_notifyallexceptions<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQBool enable);
template void sq_setcompilererrorhandler<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQCOMPILERERROR<Squirk::Standard> f);

/*stack operations*/
template void sq_push<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template void sq_pop<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger nelemstopop);
template void sq_poptop<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);
template void sq_remove<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQInteger sq_gettop<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);
template void sq_settop<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger newtop);
template void sq_reservestack<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger nsize);
template SQInteger sq_cmp<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);
template void sq_move<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> dest, HSQUIRRELVM<Squirk::Standard> src, SQInteger idx);

/*object creation handling*/
template SQUserPointer sq_newuserdata<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQUnsignedInteger size);
template void sq_newtable<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);
template void sq_newarray<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger size);
template void sq_newclosure<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQFUNCTION<Squirk::Standard> func, SQUnsignedInteger nfreevars);
template SQRESULT sq_setparamscheck<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger nparamscheck, const SQChar *typemask);
template SQRESULT sq_bindenv<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template void sq_pushstring<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, const SQChar *s, SQInteger len);
template void sq_pushfloat<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQFloat f);
template void sq_pushinteger<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger n);
template void sq_pushbool<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQBool b);
template void sq_pushuserpointer<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQUserPointer p);
template void sq_pushnull<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);
template SQObjectType sq_gettype<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQInteger sq_getsize<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQRESULT sq_getbase<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQBool sq_instanceof<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);
template void sq_tostring<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template void sq_tobool<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQBool *b);
template SQRESULT sq_getstring<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, const SQChar **c);
template SQRESULT sq_getinteger<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQInteger *i);
template SQRESULT sq_getfloat<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQFloat *f);
template SQRESULT sq_getbool<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQBool *b);
template SQRESULT sq_getthread<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, HSQUIRRELVM<Squirk::Standard> *thread);
template SQRESULT sq_getuserpointer<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQUserPointer *p);
template SQRESULT sq_getuserdata<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQUserPointer *p, SQUserPointer *typetag);
template SQRESULT sq_settypetag<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQUserPointer typetag);
template SQRESULT sq_gettypetag<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQUserPointer *typetag);
template void sq_setreleasehook<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQRELEASEHOOK hook);
template SQChar *sq_getscratchpad<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger minsize);
template SQRESULT sq_getclosureinfo<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQUnsignedInteger *nparams, SQUnsignedInteger *nfreevars);
template SQRESULT sq_setnativeclosurename<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, const SQChar *name);
template SQRESULT sq_setinstanceup<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQUserPointer p);
template SQRESULT sq_getinstanceup<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQUserPointer *p, SQUserPointer typetag);
template SQRESULT sq_setclassudsize<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQInteger udsize);
template SQRESULT sq_newclass<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQBool hasbase);
template SQRESULT sq_createinstance<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQRESULT sq_setattributes<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQRESULT sq_getattributes<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQRESULT sq_getclass<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template void sq_weakref<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQRESULT sq_getdefaultdelegate<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQObjectType t);
template SQRESULT sq_getmemberhandle<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, HSQMEMBERHANDLE *handle);
template SQRESULT sq_getbyhandle<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, const HSQMEMBERHANDLE *handle);
template SQRESULT sq_setbyhandle<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, const HSQMEMBERHANDLE *handle);

/*object manipulation*/
template void sq_pushroottable<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);
template void sq_pushregistrytable<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);
template void sq_pushconsttable<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);
template SQRESULT sq_setroottable<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);
template SQRESULT sq_setconsttable<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);
template SQRESULT sq_newslot<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQBool bstatic);
template SQRESULT sq_deleteslot<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQBool pushval);
template SQRESULT sq_set<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQRESULT sq_get<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQRESULT sq_rawget<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQRESULT sq_rawset<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQRESULT sq_rawdeleteslot<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQBool pushval);
template SQRESULT sq_arrayappend<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQRESULT sq_arraypop<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQBool pushval);
template SQRESULT sq_arrayresize<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQInteger newsize);
template SQRESULT sq_arrayreverse<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQRESULT sq_arrayremove<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQInteger itemidx);
template SQRESULT sq_arrayinsert<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQInteger destpos);
template SQRESULT sq_setdelegate<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQRESULT sq_getdelegate<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQRESULT sq_clone<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQRESULT sq_setfreevariable<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQUnsignedInteger nval);
template SQRESULT sq_next<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQRESULT sq_getweakrefval<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);
template SQRESULT sq_clear<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx);

/*calls*/
template SQRESULT sq_call<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger params, SQBool retval, SQBool raiseerror);
template SQRESULT sq_resume<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQBool retval, SQBool raiseerror);
template const SQChar *sq_getlocal<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQUnsignedInteger level, SQUnsignedInteger idx);
template const SQChar *sq_getfreevariable<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, SQUnsignedInteger nval);
template SQRESULT sq_throwerror<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, const SQChar *err);
template void sq_reseterror<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);
template void sq_getlasterror<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);

/*raw object handling*/
template SQRESULT sq_getstackobj<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, SQInteger idx, HSQOBJECT<Squirk::Standard> *po);
template void sq_pushobject<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, HSQOBJECT<Squirk::Standard> obj);
template void sq_addref<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, HSQOBJECT<Squirk::Standard> *po);
template SQBool sq_release<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v, HSQOBJECT<Squirk::Standard> *po);
template void sq_resetobject<Squirk::Standard>(HSQOBJECT<Squirk::Standard> *po);
template const SQChar *sq_objtostring<Squirk::Standard>(HSQOBJECT<Squirk::Standard> *o);
template SQBool sq_objtobool<Squirk::Standard>(HSQOBJECT<Squirk::Standard> *o);
template SQInteger sq_objtointeger<Squirk::Standard>(HSQOBJECT<Squirk::Standard> *o);
template SQFloat sq_objtofloat<Squirk::Standard>(HSQOBJECT<Squirk::Standard> *o);
template SQRESULT sq_getobjtypetag<Squirk::Standard>(HSQOBJECT<Squirk::Standard> *o, SQUserPointer * typetag);

/*GC*/
template SQInteger sq_collectgarbage<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);

/*serialization*/
template SQRESULT sq_writeclosure<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> vm, SQWRITEFUNC writef, SQUserPointer up);
template SQRESULT sq_readclosure<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> vm, SQREADFUNC readf, SQUserPointer up);

/*debug*/
template void sq_setdebughook<Squirk::Standard>(HSQUIRRELVM<Squirk::Standard> v);

/*vm*/
template HSQUIRRELVM<Squirk::AlignObject> sq_open<Squirk::AlignObject>(SQInteger initialstacksize);
template HSQUIRRELVM<Squirk::AlignObject> sq_newthread<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> friendvm, SQInteger initialstacksize);
template void sq_seterrorhandler<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);
template void sq_close<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);
template void sq_setforeignptr<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQUserPointer p);
template SQUserPointer sq_getforeignptr<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);
template void sq_setprintfunc<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQPRINTFUNCTION<Squirk::AlignObject> printfunc);
template SQPRINTFUNCTION<Squirk::AlignObject> sq_getprintfunc<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);
template SQRESULT sq_suspendvm<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);
template SQRESULT sq_wakeupvm<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQBool resumedret, SQBool retval, SQBool raiseerror, SQBool throwerror);
template SQInteger sq_getvmstate<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);

/*compiler*/
template SQRESULT sq_compile<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQLEXREADFUNC read, SQUserPointer p, const SQChar *sourcename, SQBool raiseerror);
template SQRESULT sq_compilebuffer<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, const SQChar *s, SQInteger size, const SQChar *sourcename, SQBool raiseerror);
template void sq_enabledebuginfo<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQBool enable);
template void sq_notifyallexceptions<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQBool enable);
template void sq_setcompilererrorhandler<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQCOMPILERERROR<Squirk::AlignObject> f);

/*stack operations*/
template void sq_push<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template void sq_pop<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger nelemstopop);
template void sq_poptop<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);
template void sq_remove<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQInteger sq_gettop<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);
template void sq_settop<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger newtop);
template void sq_reservestack<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger nsize);
template SQInteger sq_cmp<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);
template void sq_move<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> dest, HSQUIRRELVM<Squirk::AlignObject> src, SQInteger idx);

/*object creation handling*/
template SQUserPointer sq_newuserdata<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQUnsignedInteger size);
template void sq_newtable<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);
template void sq_newarray<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger size);
template void sq_newclosure<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQFUNCTION<Squirk::AlignObject> func, SQUnsignedInteger nfreevars);
template SQRESULT sq_setparamscheck<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger nparamscheck, const SQChar *typemask);
template SQRESULT sq_bindenv<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template void sq_pushstring<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, const SQChar *s, SQInteger len);
template void sq_pushfloat<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQFloat f);
template void sq_pushinteger<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger n);
template void sq_pushbool<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQBool b);
template void sq_pushuserpointer<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQUserPointer p);
template void sq_pushnull<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);
template SQObjectType sq_gettype<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQInteger sq_getsize<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQRESULT sq_getbase<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQBool sq_instanceof<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);
template void sq_tostring<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template void sq_tobool<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQBool *b);
template SQRESULT sq_getstring<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, const SQChar **c);
template SQRESULT sq_getinteger<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQInteger *i);
template SQRESULT sq_getfloat<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQFloat *f);
template SQRESULT sq_getbool<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQBool *b);
template SQRESULT sq_getthread<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, HSQUIRRELVM<Squirk::AlignObject> *thread);
template SQRESULT sq_getuserpointer<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQUserPointer *p);
template SQRESULT sq_getuserdata<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQUserPointer *p, SQUserPointer *typetag);
template SQRESULT sq_settypetag<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQUserPointer typetag);
template SQRESULT sq_gettypetag<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQUserPointer *typetag);
template void sq_setreleasehook<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQRELEASEHOOK hook);
template SQChar *sq_getscratchpad<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger minsize);
template SQRESULT sq_getclosureinfo<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQUnsignedInteger *nparams, SQUnsignedInteger *nfreevars);
template SQRESULT sq_setnativeclosurename<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, const SQChar *name);
template SQRESULT sq_setinstanceup<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQUserPointer p);
template SQRESULT sq_getinstanceup<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQUserPointer *p, SQUserPointer typetag);
template SQRESULT sq_setclassudsize<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQInteger udsize);
template SQRESULT sq_newclass<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQBool hasbase);
template SQRESULT sq_createinstance<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQRESULT sq_setattributes<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQRESULT sq_getattributes<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQRESULT sq_getclass<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template void sq_weakref<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQRESULT sq_getdefaultdelegate<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQObjectType t);
template SQRESULT sq_getmemberhandle<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, HSQMEMBERHANDLE *handle);
template SQRESULT sq_getbyhandle<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, const HSQMEMBERHANDLE *handle);
template SQRESULT sq_setbyhandle<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, const HSQMEMBERHANDLE *handle);

/*object manipulation*/
template void sq_pushroottable<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);
template void sq_pushregistrytable<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);
template void sq_pushconsttable<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);
template SQRESULT sq_setroottable<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);
template SQRESULT sq_setconsttable<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);
template SQRESULT sq_newslot<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQBool bstatic);
template SQRESULT sq_deleteslot<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQBool pushval);
template SQRESULT sq_set<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQRESULT sq_get<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQRESULT sq_rawget<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQRESULT sq_rawset<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQRESULT sq_rawdeleteslot<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQBool pushval);
template SQRESULT sq_arrayappend<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQRESULT sq_arraypop<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQBool pushval);
template SQRESULT sq_arrayresize<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQInteger newsize);
template SQRESULT sq_arrayreverse<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQRESULT sq_arrayremove<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQInteger itemidx);
template SQRESULT sq_arrayinsert<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQInteger destpos);
template SQRESULT sq_setdelegate<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQRESULT sq_getdelegate<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQRESULT sq_clone<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQRESULT sq_setfreevariable<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQUnsignedInteger nval);
template SQRESULT sq_next<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQRESULT sq_getweakrefval<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);
template SQRESULT sq_clear<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx);

/*calls*/
template SQRESULT sq_call<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger params, SQBool retval, SQBool raiseerror);
template SQRESULT sq_resume<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQBool retval, SQBool raiseerror);
template const SQChar *sq_getlocal<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQUnsignedInteger level, SQUnsignedInteger idx);
template const SQChar *sq_getfreevariable<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, SQUnsignedInteger nval);
template SQRESULT sq_throwerror<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, const SQChar *err);
template void sq_reseterror<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);
template void sq_getlasterror<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);

/*raw object handling*/
template SQRESULT sq_getstackobj<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, SQInteger idx, HSQOBJECT<Squirk::AlignObject> *po);
template void sq_pushobject<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, HSQOBJECT<Squirk::AlignObject> obj);
template void sq_addref<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, HSQOBJECT<Squirk::AlignObject> *po);
template SQBool sq_release<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v, HSQOBJECT<Squirk::AlignObject> *po);
template void sq_resetobject<Squirk::AlignObject>(HSQOBJECT<Squirk::AlignObject> *po);
template const SQChar *sq_objtostring<Squirk::AlignObject>(HSQOBJECT<Squirk::AlignObject> *o);
template SQBool sq_objtobool<Squirk::AlignObject>(HSQOBJECT<Squirk::AlignObject> *o);
template SQInteger sq_objtointeger<Squirk::AlignObject>(HSQOBJECT<Squirk::AlignObject> *o);
template SQFloat sq_objtofloat<Squirk::AlignObject>(HSQOBJECT<Squirk::AlignObject> *o);
template SQRESULT sq_getobjtypetag<Squirk::AlignObject>(HSQOBJECT<Squirk::AlignObject> *o, SQUserPointer * typetag);

/*GC*/
template SQInteger sq_collectgarbage<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);

/*serialization*/
template SQRESULT sq_writeclosure<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> vm, SQWRITEFUNC writef, SQUserPointer up);
template SQRESULT sq_readclosure<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> vm, SQREADFUNC readf, SQUserPointer up);

/*debug*/
template void sq_setdebughook<Squirk::AlignObject>(HSQUIRRELVM<Squirk::AlignObject> v);

/*vm*/
template HSQUIRRELVM<Squirk::StandardShared> sq_open<Squirk::StandardShared>(SQInteger initialstacksize);
template HSQUIRRELVM<Squirk::StandardShared> sq_newthread<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> friendvm, SQInteger initialstacksize);
template void sq_seterrorhandler<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);
template void sq_close<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);
template void sq_setforeignptr<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQUserPointer p);
template SQUserPointer sq_getforeignptr<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);
template void sq_setprintfunc<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQPRINTFUNCTION<Squirk::StandardShared> printfunc);
template SQPRINTFUNCTION<Squirk::StandardShared> sq_getprintfunc<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);
template SQRESULT sq_suspendvm<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);
template SQRESULT sq_wakeupvm<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQBool resumedret, SQBool retval, SQBool raiseerror, SQBool throwerror);
template SQInteger sq_getvmstate<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);

/*compiler*/
template SQRESULT sq_compile<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQLEXREADFUNC read, SQUserPointer p, const SQChar *sourcename, SQBool raiseerror);
template SQRESULT sq_compilebuffer<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, const SQChar *s, SQInteger size, const SQChar *sourcename, SQBool raiseerror);
template void sq_enabledebuginfo<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQBool enable);
template void sq_notifyallexceptions<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQBool enable);
template void sq_setcompilererrorhandler<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQCOMPILERERROR<Squirk::StandardShared> f);

/*stack operations*/
template void sq_push<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template void sq_pop<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger nelemstopop);
template void sq_poptop<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);
template void sq_remove<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQInteger sq_gettop<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);
template void sq_settop<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger newtop);
template void sq_reservestack<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger nsize);
template SQInteger sq_cmp<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);
template void sq_move<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> dest, HSQUIRRELVM<Squirk::StandardShared> src, SQInteger idx);

/*object creation handling*/
template SQUserPointer sq_newuserdata<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQUnsignedInteger size);
template void sq_newtable<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);
template void sq_newarray<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger size);
template void sq_newclosure<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQFUNCTION<Squirk::StandardShared> func, SQUnsignedInteger nfreevars);
template SQRESULT sq_setparamscheck<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger nparamscheck, const SQChar *typemask);
template SQRESULT sq_bindenv<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template void sq_pushstring<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, const SQChar *s, SQInteger len);
template void sq_pushfloat<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQFloat f);
template void sq_pushinteger<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger n);
template void sq_pushbool<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQBool b);
template void sq_pushuserpointer<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQUserPointer p);
template void sq_pushnull<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);
template SQObjectType sq_gettype<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQInteger sq_getsize<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQRESULT sq_getbase<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQBool sq_instanceof<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);
template void sq_tostring<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template void sq_tobool<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQBool *b);
template SQRESULT sq_getstring<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, const SQChar **c);
template SQRESULT sq_getinteger<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQInteger *i);
template SQRESULT sq_getfloat<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQFloat *f);
template SQRESULT sq_getbool<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQBool *b);
template SQRESULT sq_getthread<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, HSQUIRRELVM<Squirk::StandardShared> *thread);
template SQRESULT sq_getuserpointer<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQUserPointer *p);
template SQRESULT sq_getuserdata<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQUserPointer *p, SQUserPointer *typetag);
template SQRESULT sq_settypetag<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQUserPointer typetag);
template SQRESULT sq_gettypetag<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQUserPointer *typetag);
template void sq_setreleasehook<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQRELEASEHOOK hook);
template SQChar *sq_getscratchpad<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger minsize);
template SQRESULT sq_getclosureinfo<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQUnsignedInteger *nparams, SQUnsignedInteger *nfreevars);
template SQRESULT sq_setnativeclosurename<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, const SQChar *name);
template SQRESULT sq_setinstanceup<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQUserPointer p);
template SQRESULT sq_getinstanceup<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQUserPointer *p, SQUserPointer typetag);
template SQRESULT sq_setclassudsize<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQInteger udsize);
template SQRESULT sq_newclass<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQBool hasbase);
template SQRESULT sq_createinstance<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQRESULT sq_setattributes<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQRESULT sq_getattributes<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQRESULT sq_getclass<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template void sq_weakref<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQRESULT sq_getdefaultdelegate<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQObjectType t);
template SQRESULT sq_getmemberhandle<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, HSQMEMBERHANDLE *handle);
template SQRESULT sq_getbyhandle<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, const HSQMEMBERHANDLE *handle);
template SQRESULT sq_setbyhandle<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, const HSQMEMBERHANDLE *handle);

/*object manipulation*/
template void sq_pushroottable<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);
template void sq_pushregistrytable<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);
template void sq_pushconsttable<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);
template SQRESULT sq_setroottable<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);
template SQRESULT sq_setconsttable<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);
template SQRESULT sq_newslot<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQBool bstatic);
template SQRESULT sq_deleteslot<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQBool pushval);
template SQRESULT sq_set<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQRESULT sq_get<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQRESULT sq_rawget<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQRESULT sq_rawset<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQRESULT sq_rawdeleteslot<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQBool pushval);
template SQRESULT sq_arrayappend<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQRESULT sq_arraypop<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQBool pushval);
template SQRESULT sq_arrayresize<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQInteger newsize);
template SQRESULT sq_arrayreverse<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQRESULT sq_arrayremove<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQInteger itemidx);
template SQRESULT sq_arrayinsert<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQInteger destpos);
template SQRESULT sq_setdelegate<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQRESULT sq_getdelegate<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQRESULT sq_clone<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQRESULT sq_setfreevariable<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQUnsignedInteger nval);
template SQRESULT sq_next<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQRESULT sq_getweakrefval<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);
template SQRESULT sq_clear<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx);

/*calls*/
template SQRESULT sq_call<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger params, SQBool retval, SQBool raiseerror);
template SQRESULT sq_resume<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQBool retval, SQBool raiseerror);
template const SQChar *sq_getlocal<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQUnsignedInteger level, SQUnsignedInteger idx);
template const SQChar *sq_getfreevariable<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, SQUnsignedInteger nval);
template SQRESULT sq_throwerror<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, const SQChar *err);
template void sq_reseterror<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);
template void sq_getlasterror<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);

/*raw object handling*/
template SQRESULT sq_getstackobj<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, SQInteger idx, HSQOBJECT<Squirk::StandardShared> *po);
template void sq_pushobject<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, HSQOBJECT<Squirk::StandardShared> obj);
template void sq_addref<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, HSQOBJECT<Squirk::StandardShared> *po);
template SQBool sq_release<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v, HSQOBJECT<Squirk::StandardShared> *po);
template void sq_resetobject<Squirk::StandardShared>(HSQOBJECT<Squirk::StandardShared> *po);
template const SQChar *sq_objtostring<Squirk::StandardShared>(HSQOBJECT<Squirk::StandardShared> *o);
template SQBool sq_objtobool<Squirk::StandardShared>(HSQOBJECT<Squirk::StandardShared> *o);
template SQInteger sq_objtointeger<Squirk::StandardShared>(HSQOBJECT<Squirk::StandardShared> *o);
template SQFloat sq_objtofloat<Squirk::StandardShared>(HSQOBJECT<Squirk::StandardShared> *o);
template SQRESULT sq_getobjtypetag<Squirk::StandardShared>(HSQOBJECT<Squirk::StandardShared> *o, SQUserPointer * typetag);

/*GC*/
template SQInteger sq_collectgarbage<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);

/*serialization*/
template SQRESULT sq_writeclosure<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> vm, SQWRITEFUNC writef, SQUserPointer up);
template SQRESULT sq_readclosure<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> vm, SQREADFUNC readf, SQUserPointer up);

/*debug*/
template void sq_setdebughook<Squirk::StandardShared>(HSQUIRRELVM<Squirk::StandardShared> v);

/*vm*/
template HSQUIRRELVM<Squirk::AlignObjectShared> sq_open<Squirk::AlignObjectShared>(SQInteger initialstacksize);
template HSQUIRRELVM<Squirk::AlignObjectShared> sq_newthread<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> friendvm, SQInteger initialstacksize);
template void sq_seterrorhandler<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);
template void sq_close<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);
template void sq_setforeignptr<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQUserPointer p);
template SQUserPointer sq_getforeignptr<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);
template void sq_setprintfunc<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQPRINTFUNCTION<Squirk::AlignObjectShared> printfunc);
template SQPRINTFUNCTION<Squirk::AlignObjectShared> sq_getprintfunc<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);
template SQRESULT sq_suspendvm<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);
template SQRESULT sq_wakeupvm<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQBool resumedret, SQBool retval, SQBool raiseerror, SQBool throwerror);
template SQInteger sq_getvmstate<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);

/*compiler*/
template SQRESULT sq_compile<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQLEXREADFUNC read, SQUserPointer p, const SQChar *sourcename, SQBool raiseerror);
template SQRESULT sq_compilebuffer<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, const SQChar *s, SQInteger size, const SQChar *sourcename, SQBool raiseerror);
template void sq_enabledebuginfo<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQBool enable);
template void sq_notifyallexceptions<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQBool enable);
template void sq_setcompilererrorhandler<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQCOMPILERERROR<Squirk::AlignObjectShared> f);

/*stack operations*/
template void sq_push<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template void sq_pop<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger nelemstopop);
template void sq_poptop<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);
template void sq_remove<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQInteger sq_gettop<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);
template void sq_settop<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger newtop);
template void sq_reservestack<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger nsize);
template SQInteger sq_cmp<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);
template void sq_move<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> dest, HSQUIRRELVM<Squirk::AlignObjectShared> src, SQInteger idx);

/*object creation handling*/
template SQUserPointer sq_newuserdata<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQUnsignedInteger size);
template void sq_newtable<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);
template void sq_newarray<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger size);
template void sq_newclosure<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQFUNCTION<Squirk::AlignObjectShared> func, SQUnsignedInteger nfreevars);
template SQRESULT sq_setparamscheck<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger nparamscheck, const SQChar *typemask);
template SQRESULT sq_bindenv<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template void sq_pushstring<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, const SQChar *s, SQInteger len);
template void sq_pushfloat<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQFloat f);
template void sq_pushinteger<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger n);
template void sq_pushbool<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQBool b);
template void sq_pushuserpointer<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQUserPointer p);
template void sq_pushnull<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);
template SQObjectType sq_gettype<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQInteger sq_getsize<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQRESULT sq_getbase<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQBool sq_instanceof<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);
template void sq_tostring<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template void sq_tobool<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQBool *b);
template SQRESULT sq_getstring<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, const SQChar **c);
template SQRESULT sq_getinteger<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQInteger *i);
template SQRESULT sq_getfloat<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQFloat *f);
template SQRESULT sq_getbool<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQBool *b);
template SQRESULT sq_getthread<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, HSQUIRRELVM<Squirk::AlignObjectShared> *thread);
template SQRESULT sq_getuserpointer<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQUserPointer *p);
template SQRESULT sq_getuserdata<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQUserPointer *p, SQUserPointer *typetag);
template SQRESULT sq_settypetag<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQUserPointer typetag);
template SQRESULT sq_gettypetag<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQUserPointer *typetag);
template void sq_setreleasehook<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQRELEASEHOOK hook);
template SQChar *sq_getscratchpad<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger minsize);
template SQRESULT sq_getclosureinfo<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQUnsignedInteger *nparams, SQUnsignedInteger *nfreevars);
template SQRESULT sq_setnativeclosurename<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, const SQChar *name);
template SQRESULT sq_setinstanceup<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQUserPointer p);
template SQRESULT sq_getinstanceup<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQUserPointer *p, SQUserPointer typetag);
template SQRESULT sq_setclassudsize<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQInteger udsize);
template SQRESULT sq_newclass<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQBool hasbase);
template SQRESULT sq_createinstance<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQRESULT sq_setattributes<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQRESULT sq_getattributes<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQRESULT sq_getclass<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template void sq_weakref<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQRESULT sq_getdefaultdelegate<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQObjectType t);
template SQRESULT sq_getmemberhandle<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, HSQMEMBERHANDLE *handle);
template SQRESULT sq_getbyhandle<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, const HSQMEMBERHANDLE *handle);
template SQRESULT sq_setbyhandle<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, const HSQMEMBERHANDLE *handle);

/*object manipulation*/
template void sq_pushroottable<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);
template void sq_pushregistrytable<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);
template void sq_pushconsttable<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);
template SQRESULT sq_setroottable<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);
template SQRESULT sq_setconsttable<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);
template SQRESULT sq_newslot<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQBool bstatic);
template SQRESULT sq_deleteslot<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQBool pushval);
template SQRESULT sq_set<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQRESULT sq_get<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQRESULT sq_rawget<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQRESULT sq_rawset<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQRESULT sq_rawdeleteslot<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQBool pushval);
template SQRESULT sq_arrayappend<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQRESULT sq_arraypop<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQBool pushval);
template SQRESULT sq_arrayresize<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQInteger newsize);
template SQRESULT sq_arrayreverse<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQRESULT sq_arrayremove<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQInteger itemidx);
template SQRESULT sq_arrayinsert<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQInteger destpos);
template SQRESULT sq_setdelegate<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQRESULT sq_getdelegate<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQRESULT sq_clone<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQRESULT sq_setfreevariable<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQUnsignedInteger nval);
template SQRESULT sq_next<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQRESULT sq_getweakrefval<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);
template SQRESULT sq_clear<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx);

/*calls*/
template SQRESULT sq_call<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger params, SQBool retval, SQBool raiseerror);
template SQRESULT sq_resume<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQBool retval, SQBool raiseerror);
template const SQChar *sq_getlocal<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQUnsignedInteger level, SQUnsignedInteger idx);
template const SQChar *sq_getfreevariable<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, SQUnsignedInteger nval);
template SQRESULT sq_throwerror<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, const SQChar *err);
template void sq_reseterror<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);
template void sq_getlasterror<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);

/*raw object handling*/
template SQRESULT sq_getstackobj<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, SQInteger idx, HSQOBJECT<Squirk::AlignObjectShared> *po);
template void sq_pushobject<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, HSQOBJECT<Squirk::AlignObjectShared> obj);
template void sq_addref<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, HSQOBJECT<Squirk::AlignObjectShared> *po);
template SQBool sq_release<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v, HSQOBJECT<Squirk::AlignObjectShared> *po);
template void sq_resetobject<Squirk::AlignObjectShared>(HSQOBJECT<Squirk::AlignObjectShared> *po);
template const SQChar *sq_objtostring<Squirk::AlignObjectShared>(HSQOBJECT<Squirk::AlignObjectShared> *o);
template SQBool sq_objtobool<Squirk::AlignObjectShared>(HSQOBJECT<Squirk::AlignObjectShared> *o);
template SQInteger sq_objtointeger<Squirk::AlignObjectShared>(HSQOBJECT<Squirk::AlignObjectShared> *o);
template SQFloat sq_objtofloat<Squirk::AlignObjectShared>(HSQOBJECT<Squirk::AlignObjectShared> *o);
template SQRESULT sq_getobjtypetag<Squirk::AlignObjectShared>(HSQOBJECT<Squirk::AlignObjectShared> *o, SQUserPointer * typetag);

/*GC*/
template SQInteger sq_collectgarbage<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);

/*serialization*/
template SQRESULT sq_writeclosure<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> vm, SQWRITEFUNC writef, SQUserPointer up);
template SQRESULT sq_readclosure<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> vm, SQREADFUNC readf, SQUserPointer up);

/*debug*/
template void sq_setdebughook<Squirk::AlignObjectShared>(HSQUIRRELVM<Squirk::AlignObjectShared> v);
