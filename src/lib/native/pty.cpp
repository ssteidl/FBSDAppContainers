/*
 * Pseudo TTY library
 *
 * Copyright (C) 2015 Lawrence Woodman <lwoodman@vlifesystems.com>
 *
 * Licensed under an MIT licence.  Please see LICENCE.md for details.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <tcl.h>
#include <termios.h>
#include "tcl_util.h"
#include <sys/ioctl.h>

namespace {

int
Pty_Open(ClientData clientData,
         Tcl_Interp *interp,
         int objc,
         Tcl_Obj *CONST objv[])
{
  intptr_t masterfd;
  char* slaveName;
  Tcl_Channel masterChan;
  char *argStr;
  int lengthArgStr;
  int argNum;
  int nogrant = 0;
  int numArgsLeft;
  Tcl_Obj *result;

  for (argNum = 1; argNum < objc; argNum++) {
    argStr = Tcl_GetStringFromObj (objv[argNum], &lengthArgStr);
    if (argStr[0] != '-') {
      break;
    }

    if (strncmp(argStr, "-nogrant", lengthArgStr) == 0) {
      nogrant = 1;
    } else {
      Tcl_AppendResult(interp,
                       "bad option \"", argStr,
                       "\": must be -nogrant",
                       (char *) NULL);
      return TCL_ERROR;
    }
  }

  numArgsLeft = objc - argNum;

  if (numArgsLeft != 0) {
    Tcl_WrongNumArgs(interp, 1, objv, "?options?");
    return TCL_ERROR;
  }

  masterfd = posix_openpt(O_RDWR | O_NOCTTY);

  if (masterfd == -1) {
    Tcl_AppendResult(interp,
                     "posix_openpt failed \"",
                     Tcl_ErrnoMsg(Tcl_GetErrno()), "\"",
                     (char *) NULL);
    return TCL_ERROR;
  }

  if (!nogrant) {
    if (grantpt(masterfd) != 0) {
      Tcl_AppendResult(interp,
                       "grantpt failed \"",
                       Tcl_ErrnoMsg(Tcl_GetErrno()), "\"",
                       (char *) NULL);
      return TCL_ERROR;
    }
  }

  if (unlockpt(masterfd) != 0) {
    Tcl_AppendResult(interp,
                     "unlockpt failed \"",
                     Tcl_ErrnoMsg(Tcl_GetErrno()), "\"",
                     (char *) NULL);
    return TCL_ERROR;
  }

  slaveName = ptsname(masterfd);
  if (slaveName == NULL) {
      Tcl_AppendResult(interp,
                       "ptsname failed \"",
                       Tcl_ErrnoMsg(Tcl_GetErrno()), "\"",
                       (char *) NULL);
      return TCL_ERROR;
  }
  Tcl_Obj* slaveNameObj = Tcl_NewStringObj(slaveName, -1);
  masterChan =
    Tcl_MakeFileChannel((ClientData)masterfd, TCL_READABLE | TCL_WRITABLE);

  if (masterChan == (Tcl_Channel)NULL) {
      return TCL_ERROR;
  }

  Tcl_RegisterChannel(interp, masterChan);

  result = Tcl_NewListObj(0, NULL);

  Tcl_ListObjAppendElement(interp,
                           result,
                           Tcl_NewStringObj(Tcl_GetChannelName(masterChan),
                                            -1));
  Tcl_ListObjAppendElement(interp, result, slaveNameObj);
  Tcl_SetObjResult(interp, result);

  return TCL_OK;
}


int
Pty_MakeRaw(ClientData clientData,
         Tcl_Interp *interp,
         int objc,
         Tcl_Obj *CONST objv[])
{
    if(objc != 2)
    {
        Tcl_WrongNumArgs(interp, objc, objv, "channel");
        return TCL_ERROR;
    }

    Tcl_Obj* chan_name = objv[1];

    long handle = -1;
    int tcl_error = vessel::get_handle_from_channel(interp, chan_name, handle);
    if(tcl_error) return tcl_error;

    termios tios;
    int error = tcgetattr((int)handle, &tios);
    if(error == -1)
    {
        return vessel::syserror_result(interp, "PTY", "TCGETATTR");
    }

    /*Create byte array that can be used for restoring settings*/
    vessel::tclobj_ptr old_tios = vessel::create_tclobj_ptr(Tcl_NewByteArrayObj((unsigned char*)&tios, sizeof(tios)));

    cfmakeraw(&tios);
    error = tcsetattr((int)handle, TCSANOW, &tios);
    if(error == -1)
    {
        return vessel::syserror_result(interp, "PTY", "TCSETATTR");
    }

    Tcl_IncrRefCount(old_tios.get());
    Tcl_SetObjResult(interp, old_tios.release());
    return TCL_OK;
}

int
Pty_CopyWindowSize(ClientData clientData,
         Tcl_Interp *interp,
         int objc,
         Tcl_Obj *CONST objv[])
{
    if(objc != 3)
    {
        Tcl_WrongNumArgs(interp, objc, objv, "src_chan dest_chan");
        return TCL_ERROR;
    }

    Tcl_Obj* src_chan_name = objv[1];
    long src_handle = -1;
    int tcl_error = vessel::get_handle_from_channel(interp, src_chan_name, src_handle);
    if(tcl_error) return tcl_error;

    Tcl_Obj* dest_chan_name = objv[2];
    long dest_handle = -1;
    tcl_error = vessel::get_handle_from_channel(interp, dest_chan_name, dest_handle);
    if(tcl_error) return tcl_error;

    winsize ws;
    memset(&ws, 0, sizeof(winsize));
    int error = ioctl((int)src_handle, TIOCGWINSZ, &ws);
    if(error == -1)
    {
        return vessel::syserror_result(interp, "PTY", "WINSZ", "TIOCGWINSZ");
    }

    error = ioctl((int)dest_handle, TIOCSWINSZ, &ws);
    if(error == -1)
    {
        return vessel::syserror_result(interp, "PTY", "WINSZ", "TIOCSWINSZ");
    }
    return TCL_OK;
}

int
Pty_TermiosRestore(ClientData clientData,
         Tcl_Interp *interp,
         int objc,
         Tcl_Obj *CONST objv[])
{
    if(objc != 3)
    {
        Tcl_WrongNumArgs(interp, objc, objv, "channel termios_binary");
        return TCL_ERROR;
    }

    long handle = 0;
    int tcl_error = vessel::get_handle_from_channel(interp, objv[1], handle);
    if(tcl_error) return tcl_error;

    int length = 0;
    unsigned char* termios_bytes = Tcl_GetByteArrayFromObj(objv[2], &length);
    int error = tcsetattr((int)handle, TCSANOW, (termios*)termios_bytes);
    if(error == -1)
    {
        return vessel::syserror_result(interp, "PTY", "TCSETATTR");
    }

    return TCL_OK;
}
}

extern "C"
{
    int
    Pty_Init(Tcl_Interp *interp)
    {
      Tcl_CreateObjCommand(interp,
                           "pty::open",
                           Pty_Open,
                           (ClientData) NULL,
                           (Tcl_CmdDeleteProc *) NULL);

      Tcl_CreateObjCommand(interp,
                           "pty::makeraw",
                           Pty_MakeRaw,
                           (ClientData) NULL,
                           (Tcl_CmdDeleteProc *) NULL);

      Tcl_CreateObjCommand(interp,
                           "pty::copy_winsz",
                           Pty_CopyWindowSize,
                           (ClientData) NULL,
                           (Tcl_CmdDeleteProc *) NULL);

      Tcl_CreateObjCommand(interp,
                           "pty::restore",
                           Pty_TermiosRestore,
                           (ClientData) NULL,
                           (Tcl_CmdDeleteProc *) NULL);

      if ( Tcl_PkgProvide(interp, "pty", "0.1") != TCL_OK ) {
        return TCL_ERROR;
      }

      return TCL_OK;
    }
}
