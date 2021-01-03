#ifndef URL_CMD_H
#define URL_CMD_H

#include <tcl.h>

int Vessel_ParseURL(void *clientData, Tcl_Interp *interp,
                  int objc, struct Tcl_Obj *const *objv);
#endif // URL_CMD_H
