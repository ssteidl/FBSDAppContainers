#ifndef TCL_UTIL_H
#define TCL_UTIL_H

#include <cerrno>
#include <memory>
#include <tcl.h>


namespace vessel
{

void unref_tclobj(Tcl_Obj* obj);

template<class T>
void tclalloc_free(T* arg);

using tclobj_ptr = std::unique_ptr<Tcl_Obj, decltype(&unref_tclobj)>;

template<class T>
using tclalloc_ptr = std::unique_ptr<T, decltype(&tclalloc_free<T>)>;

void tclobj_delete_proc(void* client_data, Tcl_Interp* interp);

tclobj_ptr create_tclobj_ptr(Tcl_Obj* obj);

template<typename... CODES>
int syserror_result(Tcl_Interp* interp, CODES... error_codes)
{

    Tcl_SetResult(interp, (char*)Tcl_ErrnoMsg(errno), TCL_STATIC);
    Tcl_SetErrorCode(interp, &error_codes..., Tcl_ErrnoId(), nullptr);
    return TCL_ERROR;
}

int get_handle_from_channel(Tcl_Interp* interp, Tcl_Obj* chan_name, long& handle);

template <typename T>
void cpp_delete(void* client_data)
{
    delete((T*)client_data);
}

template <typename T>
void cpp_delete_with_interp(void* client_data, Tcl_Interp* interp)
{
    delete((T*)client_data);
}

struct fd_guard
{
    int fd;

    fd_guard(int fd);
    int release();
    ~fd_guard();
};
}

template<class T>
void vessel::tclalloc_free(T* arg)
{
    Tcl_Free(reinterpret_cast<char*>(arg));
}

#endif // TCL_UTIL_H
