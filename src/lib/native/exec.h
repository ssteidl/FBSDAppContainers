#ifndef EXEC_H
#define EXEC_H
#include <tcl.h>

int Vessel_ExecInit(Tcl_Interp* interp);

int Vessel_Exec_SetSignalHandler(void *clientData, Tcl_Interp *interp,
              int objc, struct Tcl_Obj *const *objv);

int Vessel_Exec(void *clientData, Tcl_Interp *interp,
              int objc, struct Tcl_Obj *const *objv);

#endif // EXEC_H
