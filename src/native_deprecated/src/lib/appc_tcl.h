#ifndef VESSEL_TCL_H
#define VESSEL_TCL_H

#include <tcl.h>
#include <memory>

namespace vessel
{

void delete_interp(Tcl_Interp* interp);
using interp_ptr = std::unique_ptr<Tcl_Interp, decltype(&delete_interp)>;
interp_ptr create_tcl_interp();

struct tcl_obj_raii
{
    Tcl_Obj* obj;

    tcl_obj_raii(Tcl_Obj* obj);
    tcl_obj_raii(const tcl_obj_raii& other);
    tcl_obj_raii(tcl_obj_raii&& other);
    tcl_obj_raii& operator=(const tcl_obj_raii& other);
    tcl_obj_raii& operator=(tcl_obj_raii&&);
    operator Tcl_Obj*() const;
    ~tcl_obj_raii();
};

}
#endif // VESSEL_TCL_H
