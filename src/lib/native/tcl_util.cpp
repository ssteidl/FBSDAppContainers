#include "tcl_util.h"

using namespace vessel;

void vessel::unref_tclobj(Tcl_Obj* obj)
{
    if(obj)
    {
        Tcl_DecrRefCount(obj);
    }
}

void vessel::tclobj_delete_proc(void* client_data, Tcl_Interp* interp)
{
    Tcl_DecrRefCount((Tcl_Obj*)client_data);
}

tclobj_ptr vessel::create_tclobj_ptr(Tcl_Obj* obj)
{
    return tclobj_ptr(obj, vessel::unref_tclobj);
}

int vessel::get_handle_from_channel(Tcl_Interp* interp, Tcl_Obj* chan_name, long& handle)
{
    /*Note: handle is of type long because that is the size of a void* which is what
     * is used to retrieve the handle.  If you pass it an int, bad things happen because
     * an int will get dereferenced as a wider void*
     */

    int mode = -1;

    Tcl_Channel chan = Tcl_GetChannel(interp, Tcl_GetString(chan_name), &mode);
    if(chan == nullptr) return TCL_ERROR;

    int tcl_error = Tcl_GetChannelHandle(chan, mode, (ClientData*)&handle);
    if(tcl_error)
    {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("Error getting handle from channel: %s",
                                               Tcl_GetString(chan_name)));
        return tcl_error;
    }

    return TCL_OK;
}

