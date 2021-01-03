#include <array>
#include <cerrno>
#include "exec.h"
#include <fcntl.h>
#include <iostream>
#include <list>
#include <set>
#include <sstream>
#include <string>
#include <termios.h>
#include <vector>
#include <unistd.h>
#include <sys/event.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "tcl_util.h"

/*TODO: Consistent naming conventions*/
/*TODO: Better logging support.  There is too much logging to cerr for an extension*/
/*TODO: There are actually 3 potentially separate modules in this file.  kqueue, process monitoring
 * and exec
 * NOTE: Now that we have added the signal handling and jail lookup, this isn't really a generic exec
 * module anymore.  It's now pretty specialized for the running a jail use case.*/
namespace
{
    /**
     * @brief The kqueue_state struct simply manages if there is an event ready to be
     * processed in the kqueue.
     */
    struct kqueue_state
    {
        int kqueue_fd;
        bool ready;

        kqueue_state(int kqueue_fd)
            : kqueue_fd(kqueue_fd),
              ready(false)
        {}

        ~kqueue_state()
        {
            if(kqueue_fd != -1)
            {
                close(kqueue_fd);
            }
        }
    };

    /**
     * Class for monitoring a heirarchy of processes.  This class provides the interface
     * that is called based on EVFILT_PROC events.
     */
    class monitored_process_group
    {
        pid_t m_child; /**< The process that was directly forked*/
        bool m_child_exited; /**< True if the child has exited.  Note that descendants
                               * can still be added if the child has exited.*/
        uint64_t m_child_exit_status = 0;

        std::set<pid_t> m_descendants; /**< Flattened set of descendant processes.*/

    public:
        monitored_process_group(pid_t child)
            : m_child(child),
              m_child_exited(false),
              m_descendants()
        {}

        void add_descendant(pid_t pid)
        {
            m_descendants.insert(pid);
        }

        /**
         * Remove pid from the monitor group.
         *
         * @returns true if all processes from the monitor group have exited.
         */
        bool exited(pid_t pid, uint64_t exit_status)
        {
            if(pid == m_child)
            {
                m_child_exited = true;
                m_child_exit_status = exit_status;

                std::cerr << "Waiting for child process: " << pid << std::endl;
                int status = 0;
                pid_t waitedpid = waitpid(pid, &status, 0);
                std::cerr << "Waited on: " << waitedpid << ": " << WIFEXITED(status) << ","
                          << WIFCONTINUED(status) << "," << WIFSIGNALED(status) << ","
                          << WIFSTOPPED(status)<< std::endl;
                std::cerr << "Signal: " << WTERMSIG(status) << std::endl;
                /*TODO: Have a background error here if waitpid fails.*/
            }
            else
            {
                size_t removed_count = m_descendants.erase(pid);
                if(removed_count == 0)
                {
                    throw std::runtime_error("Tried to remove a pid that does not exist in descendants");
                }
            }

            return (m_child_exited && m_descendants.empty());
        }

        std::set<pid_t> active_pids()
        {
            std::set<pid_t> pids;
            if(!m_child_exited)
            {
                pids.insert(m_child);
            }

            pids.insert(std::begin(m_descendants), std::end(m_descendants));
            return pids;
        }

        bool has_first_child_exited() const
        {
            return m_child_exited;
        }

        pid_t first_child_pid() const
        {
            return m_child;
        }
    };

    using monitored_process_group_list = std::list<std::shared_ptr<monitored_process_group>>;
    void KqueueEventCheckProc(void *clientData, int flags);
    void KqueueEventSetupProc(void *clientData, int flags);

    /**
     * @brief The vessel_exec_interp_state struct Contains the necessary per-interpreter state.  Basically
     * all of the data that is needed at the global level but hang it off of an interp for good practice.
     */
    struct vessel_exec_interp_state
    {
        kqueue_state state;
        Tcl_Interp* interp;
        monitored_process_group_list mpgs;
        vessel::tclobj_ptr signal_callback;

    private:
        vessel_exec_interp_state(Tcl_Interp* interp)
            : state(kqueue()),
              interp(interp),
              mpgs(),
              signal_callback(nullptr, vessel::unref_tclobj)
        {
            if(state.kqueue_fd != -1)
            {
                Tcl_CreateEventSource(KqueueEventSetupProc, KqueueEventCheckProc, this);
            }
        }
    public:
        ~vessel_exec_interp_state()
        {
            /*Tcl ignores the case when the event source doesn't exist.*/
            Tcl_DeleteEventSource(KqueueEventSetupProc, KqueueEventCheckProc, this);
        }

        static int create(Tcl_Interp* interp,
                          std::unique_ptr<vessel_exec_interp_state>& interp_state)
        {
            /*NOTE: Can't use make_unique with private constructor.*/
            std::unique_ptr<vessel_exec_interp_state> tmp_ptr(new vessel_exec_interp_state(interp));
            if(tmp_ptr->state.kqueue_fd == -1)
            {
                return vessel::syserror_result(interp, "APPC EXEC KQUEUE");
            }
            interp_state = std::move(tmp_ptr);
            return TCL_OK;
        }

        void set_signal_callback(Tcl_Obj* callback)
        {
            Tcl_IncrRefCount(callback);
            signal_callback = vessel::create_tclobj_ptr(callback);
        }
    };

    /**
     * @brief The process_group_tcl_event struct is the structure that is used as
     * a tcl event when the monitored process group has exited.  It is responsible
     * for calling the provided callback script when all processes have exited.
     */
    struct process_group_tcl_event : public Tcl_Event
    {
        Tcl_Interp* interp;
        Tcl_Obj* callback_script;
        std::shared_ptr<monitored_process_group> mpg;

        static int event_proc(Tcl_Event *ev, int flags)
        {
            (void)flags;
            process_group_tcl_event* _this = reinterpret_cast<process_group_tcl_event*>(ev);

            /*TODO: Add a "reason" parameter to the callback script.  In this case the reason would be
             * 'exit' but in other cases it might be 'signal' and have a jail id.*/
            int ret = Tcl_EvalObjEx(_this->interp, _this->callback_script, TCL_EVAL_GLOBAL);

            if(ret != TCL_OK)
            {
                Tcl_BackgroundError(_this->interp);
            }

            /*Explicitly call the destructor because TCL event loop owns the memory.  My understanding is
             * that this will explicitly call the destructors for member objects.*/
            _this->~process_group_tcl_event();

            return 1; /*Event has been processed*/
        }

        process_group_tcl_event(Tcl_Interp* interp, Tcl_Obj* callback_script,
                                std::shared_ptr<monitored_process_group>& mpg)
            : interp(interp),
              callback_script(callback_script),
              mpg(mpg)
        {
            this->proc = event_proc;
            this->nextPtr = nullptr;

            Tcl_IncrRefCount(callback_script);
        }

        ~process_group_tcl_event()
        {
            Tcl_DecrRefCount(callback_script);
        }
    };

    /**
     * @brief dup_fd Duplicates an fd into another specific fd and handles errors as needed.
     * @param old_fd
     * @param new_fd
     * @param fd_name
     * @param error_fd
     * @return
     */
    int dup_fd(int old_fd, int new_fd, const std::string& fd_name, int error_fd = 2)
    {
        new_fd = dup2(old_fd, new_fd);
        if(new_fd == -1)
        {
            std::ostringstream msg;
            msg << fd_name << " dup2: " << strerror(errno) << std::endl;
            ssize_t bytes_written = write(error_fd, msg.str().c_str(), msg.str().size());
            assert(bytes_written >= 0);
            return -1;
        }

        return 0;
    }

    /**
     * @brief KqueueEventReady The callback for the tcl file handler (fd notifier).
     * @param clientData
     * @param flags
     */
    void KqueueEventReady(void *clientData, int flags)
    {
        kqueue_state* state = reinterpret_cast<kqueue_state*>(clientData);
        state->ready = true;
    }

    /**
     * @brief KqueueEventSetupProc The setup proc for the tcl event loop integration.
     * @param clientData
     * @param flags
     */
    void KqueueEventSetupProc(void *clientData, int flags)
    {
        (void)flags;

        kqueue_state* state = (kqueue_state*)clientData;
        if(state == nullptr || state->kqueue_fd == -1)
        {
            return;
        }

        Tcl_CreateFileHandler(state->kqueue_fd, TCL_READABLE, KqueueEventReady, state);
    }

    /**
     * @brief handle_kqueue_proc_event Handles a kqueue EVFILT_PROC event.  It is responsible
     * for queuing the tcl event when the monitored process group has allof the processes exited.
     * @param event
     */
    void handle_kqueue_proc_event(struct kevent& event)
    {
        assert(event.udata);
        process_group_tcl_event* pge = reinterpret_cast<process_group_tcl_event*>(event.udata);

        if(event.fflags & NOTE_TRACKERR)
        {
            Tcl_SetObjResult(pge->interp,
                             Tcl_ObjPrintf("Error tracking child process of %lu", event.ident));
            Tcl_BackgroundError(pge->interp);
            return;
        }

        if(event.fflags & NOTE_CHILD)
        {
            pge->mpg->add_descendant((pid_t)event.ident);
        }
        else if(event.fflags & NOTE_EXIT)
        {
            bool is_process_group_empty = pge->mpg->exited(event.ident, event.data);
            if(is_process_group_empty)
            {
                /*Queues the process done callback.*/
                Tcl_QueueEvent(pge, TCL_QUEUE_TAIL);
            }
        }
    }

    /**
     * @brief The AppcExecSignalEvent class Encapsulates the event handling of signal events.
     */
    class vessel_exec_signal_event : public Tcl_Event
    {
        Tcl_Obj* m_eval_params;
        Tcl_Interp* m_interp;

        static int event_proc(Tcl_Event *evPtr, int flags)
        {
            vessel_exec_signal_event* _this = reinterpret_cast<vessel_exec_signal_event*>(evPtr);

            int error = Tcl_EvalObjEx(_this->m_interp, _this->m_eval_params, TCL_EVAL_GLOBAL);

            /*Memory will be deleted by the tcl event loop.*/
            _this->~vessel_exec_signal_event();

            if(error)
            {
                Tcl_BackgroundError(_this->m_interp);
            }

            return 1; /*Handled event.  */
        }

        vessel_exec_signal_event(Tcl_Obj* eval_params, Tcl_Interp* interp)
            : Tcl_Event(),
              m_eval_params(eval_params),
              m_interp(interp)
        {

            this->proc = event_proc;
            this->nextPtr = nullptr;
            Tcl_IncrRefCount(eval_params);
        }

        ~vessel_exec_signal_event()
        {
            Tcl_DecrRefCount(m_eval_params);
        }
    public:

        /**
         * @brief enqueue_signal_event Enqueue the signal callback into the event queue.
         * NOTE: This could/should probably be generalized.
         * @param interp
         * @param callback_eval_params
         */
        static void enqueue_signal_event(Tcl_Interp* interp, Tcl_Obj* callback_eval_params)
        {
            /*New placement because tcl event queue frees the memory*/
            vessel_exec_signal_event* event_buffer = reinterpret_cast<vessel_exec_signal_event*>(Tcl_Alloc(sizeof(vessel_exec_signal_event)));
            assert(event_buffer);
            new(event_buffer) vessel_exec_signal_event(callback_eval_params, interp);

            Tcl_QueueEvent(event_buffer, TCL_QUEUE_TAIL);
        }
    };

    /**
     * @brief handle_kqueue_signal_event Responsible for handling the signal event from kqueue and creating
     * the necessary tcl objects and tcl event so the tcl script signal callback will be invoked.
     * @param event
     */
    void handle_kqueue_signal_event(struct kevent& event)
    {
        assert(event.udata);
        vessel_exec_interp_state* interp_state = reinterpret_cast<vessel_exec_interp_state*>(event.udata);
        if(!interp_state->signal_callback)
        {
            return;
        }

        /*The list of mpg dicts that will be returned.*/
        vessel::tclobj_ptr top_level_list = vessel::create_tclobj_ptr(Tcl_NewListObj(0, nullptr));
        for(auto mpg : interp_state->mpgs)
        {
            /*Each mpg is represented by a dict:
             * "main_pid": empty if main pid has exited.  Pid if it is still active.
             * "active_pids": List of active pids*/

            /*Create the mpg dict and add it to the top level list to be returned.*/
            Tcl_Obj* mpg_dict = Tcl_NewDictObj();
            int error = Tcl_ListObjAppendElement(interp_state->interp, top_level_list.get(), mpg_dict);
            if(error)
            {
                Tcl_BackgroundError(interp_state->interp);
                return;
            }

            /*Add the value of the main pid.  Empty object if the main process has exited.*/
            vessel::tclobj_ptr main_pid_val = vessel::create_tclobj_ptr(Tcl_NewObj());
            if(!mpg->has_first_child_exited())
            {
                main_pid_val = vessel::create_tclobj_ptr(Tcl_NewWideIntObj(mpg->first_child_pid()));
            }
            error = Tcl_DictObjPut(interp_state->interp, mpg_dict, Tcl_NewStringObj("main_pid", -1), main_pid_val.release());
            if(error)
            {
                Tcl_BackgroundError(interp_state->interp);
                break;
            }

            /*Create the active pids list and add it to the mpg dict.*/
            Tcl_Obj* mpg_list = Tcl_NewListObj(0, nullptr);
            error = Tcl_DictObjPut(interp_state->interp, mpg_dict, Tcl_NewStringObj("active_pids", -1), mpg_list);
            if(error)
            {
                Tcl_BackgroundError(interp_state->interp);
                return;
            }

            /*The signal handler should take a list of lists each of which contain all active pids
             * for a process group.  The tcl code can search for jails and shut them down as necessary*/
            for(pid_t pid : mpg->active_pids())
            {
                error = Tcl_ListObjAppendElement(interp_state->interp, mpg_list, Tcl_NewWideIntObj(pid));
                if(error)
                {
                    Tcl_BackgroundError(interp_state->interp);
                    return;
                }
            }
        }

        /*Create an object with the callback elements provided as well as the top level list of mpgs*/

        int callback_length = 0;
        Tcl_Obj **callback_elements = nullptr;
        int error = Tcl_ListObjGetElements(interp_state->interp, interp_state->signal_callback.get(), &callback_length, &callback_elements);
        if(error)
        {
            Tcl_BackgroundError(interp_state->interp);
            return;
        }

        vessel::tclobj_ptr eval_params = vessel::create_tclobj_ptr(Tcl_NewListObj(callback_length, callback_elements));
        error = Tcl_ListObjAppendElement(interp_state->interp, eval_params.get(), Tcl_NewStringObj(sys_signame[event.ident], -1));
        error = error || Tcl_ListObjAppendElement(interp_state->interp, eval_params.get(), top_level_list.release());
        if(error)
        {
            Tcl_BackgroundError(interp_state->interp);
            return;
        }

        vessel_exec_signal_event::enqueue_signal_event(interp_state->interp, eval_params.release());
    }

    /**
     * @brief KqueueEventCheckProc Handles available kqueue events.  We know if
     * an event is ready based on the kqueue_state which is set by the file event
     * handler.
     * @param clientData
     * @param flags
     */
    void KqueueEventCheckProc(void *clientData, int flags)
    {
        (void)flags;

        kqueue_state* state = reinterpret_cast<kqueue_state*>(clientData);
        assert(state);

        if(!state->ready)
        {
            return;
        }
        state->ready = false;

        /*Kqueue is ready.  Get the available events and process them.*/
        std::array<struct kevent, 16> events = {};

        /*Use timespec instead of nullptr so kevent doesn't block*/
        timespec spec;
        spec.tv_sec = 0;
        spec.tv_nsec = 0;
        ssize_t event_count = kevent(state->kqueue_fd, nullptr, 0,
                                     events.data(), events.size(),
                                     &spec);

        if(event_count == -1)
        {
            /*TODO: We should use tcl background errors instead of c++ exceptions*/
            std::ostringstream msg;
            msg << "Error occurred while retrieving kevents: " << strerror(errno);
            std::cerr << msg.str() << std::endl;
            throw std::runtime_error(msg.str());
        }

        for(int i = 0; i < event_count; ++i)
        {
            switch(events[i].filter)
            {
            case EVFILT_PROC:
                handle_kqueue_proc_event(events[i]);
                break;
            case EVFILT_SIGNAL:
                handle_kqueue_signal_event(events[i]);
                break;
            default:
                throw std::runtime_error("Unexexpected filter type: " + std::to_string(events[i].filter));
                break;
            }
        }
    }

    void KqueueDeleteProc(void *clientData, Tcl_Interp *interp)
    {
        int kqueue_fd = -1;
        Tcl_Obj* fd_obj = (Tcl_Obj*)clientData;
        int tcl_error = Tcl_GetIntFromObj(interp, fd_obj, &kqueue_fd);

        if(!tcl_error)
        {
            Tcl_DecrRefCount(fd_obj);
            close(kqueue_fd);
        }
    }
}

int Appc_ExecInit(Tcl_Interp* interp)
{
    /*TODO: We should probably make our own kqueue module
     * for vessel::native extension
     */
    std::unique_ptr<vessel_exec_interp_state> interp_state;
    int tcl_error = vessel_exec_interp_state::create(interp, interp_state);
    if(tcl_error) return tcl_error;

    Tcl_SetAssocData(interp, "AppcExec", vessel::cpp_delete_with_interp<vessel_exec_interp_state>, interp_state.release());

    (void)Tcl_CreateObjCommand(interp, "vessel::exec_set_signal_handler", Appc_Exec_SetSignalHandler, nullptr, nullptr);
    (void)Tcl_CreateObjCommand(interp, "vessel::exec", Appc_Exec, nullptr, nullptr);

    return TCL_OK;
}

/**
 * @brief Appc_Exec_SetSignalHandler Setup kqueue to notify us of interesting signals.  Set the signal
 * handler that will be called when an interesting signal is given to us.
 * @param clientData
 * @param interp
 * @param objc
 * @param objv
 * @return
 */
int Appc_Exec_SetSignalHandler(void *clientData, Tcl_Interp *interp,
              int objc, struct Tcl_Obj *const *objv)
{

    /*NOTE: I wonder if I should be using a pty to pass along signals instead of doing this?
     * I don't think so because that wouldn't help with jail processes that run in the background.  Perhaps
     * that should be done in interactive mode.  See ticket #37*/
    if(objc != 2)
    {
        Tcl_WrongNumArgs(interp, objc, objv, "<callback_prefix>");
    }


    vessel_exec_interp_state* interp_state = reinterpret_cast<vessel_exec_interp_state*>(Tcl_GetAssocData(interp, "AppcExec", nullptr));

    signal(SIGHUP, SIG_IGN);

    struct kevent ev;
    std::vector<int> sigs = {SIGINT, SIGTERM, SIGHUP};
    for(int sig : sigs)
    {
        signal(sig, SIG_IGN);
        EV_SET(&ev, sig, EVFILT_SIGNAL, EV_ADD, 0, 0, (void*)interp_state);
        int error = kevent(interp_state->state.kqueue_fd, &ev, 1, nullptr, 0, nullptr);
        if(error)
        {
            return vessel::syserror_result(interp, "KQUEUE", "SIGNAL");
        }
    }

    interp_state->set_signal_callback(objv[1]);

    return TCL_OK;
}

int Appc_Exec(void *clientData, Tcl_Interp *interp,
              int objc, struct Tcl_Obj *const *objv)
{
    (void)objv;
    vessel_exec_interp_state* vessel_exec = reinterpret_cast<vessel_exec_interp_state*>(Tcl_GetAssocData(interp, "AppcExec", nullptr));
    assert(vessel_exec);
    kqueue_state* state = &vessel_exec->state;

    if(objc < 4)
    {
        Tcl_WrongNumArgs(interp, objc, objv, "<fd_dict> <callback_prefix> cmd...");
        return TCL_ERROR;
    }

    int stdin_fd = 0;
    int stderr_fd = 1;
    int stdout_fd = 2;

    Tcl_Obj* fd_dict = objv[1];
    Tcl_Obj* callback_prefix = objv[2];
    int tcl_error = TCL_OK;

    int async = 0;
    /*If the callback is not an empty string then we run the command
     * async*/
    (void)Tcl_GetStringFromObj(callback_prefix, &async);
    if(tcl_error) return tcl_error;

    Tcl_DictSearch search;
    Tcl_Obj* key = nullptr;
    Tcl_Obj* value = nullptr;
    int done;
    for(tcl_error = Tcl_DictObjFirst(interp, fd_dict, &search, &key, &value, &done);
        done == 0;
        Tcl_DictObjNext(&search, &key, &value, &done))
    {
        long handle = -1;
        if(std::string("stdin") == Tcl_GetString(key))
        {
            tcl_error = vessel::get_handle_from_channel(interp, value, handle);
            if(tcl_error) return tcl_error;
            stdin_fd = (int)handle;

        }
        else if(std::string("stdout") == Tcl_GetString(key))
        {
            tcl_error = vessel::get_handle_from_channel(interp, value, handle);
            if(tcl_error) return tcl_error;

            stdout_fd = (int)handle;
        }
        else if(std::string("stderr") == Tcl_GetString(key))
        {

            tcl_error = vessel::get_handle_from_channel(interp, value, handle);
            if(tcl_error) return tcl_error;

            stderr_fd = (int)handle;
        }
    }

    /*Now create the vector of arguments so that we can fork/exec*/
    std::vector<char*> args;
    for(int i = 3; i < objc; ++i)
    {
        args.push_back(Tcl_GetString(objv[i]));
    }
    args.push_back(nullptr);

    pid_t pid = fork();

    switch(pid)
    {
    case 0:
    {
        /*Child*/

        /*I don't think we want to touch the interp as even deleting it calls
         * deletion callbacks*/

        pid_t process_grpid = 0;
        /*Only start a new session if stdin is a pty and not stdin of the current process*/
        if(isatty(stdin_fd) && stdin_fd != 0)
        {
            process_grpid = setsid();

            if(process_grpid == (int)-1)
            {
                perror("setsid");
                exit(1);
            }

            int error = tcsetsid(stdin_fd, process_grpid);
            if(error == -1)
            {
                 perror("tcsetsid");
                 exit(1);
            }

            error = tcsetpgrp(stdin_fd, 0);

            /*shell child*/

            error = tcsetpgrp(stdin_fd, getpid());
            if(error == -1)
            {
                std::ostringstream msg;
                msg << "tcsetpgrp2: " << strerror(errno) << std::endl;
                exit(1);
            }

            struct termios tios;
            error = tcgetattr(stdin_fd, &tios);
            if(error == -1)
            {
                perror("tcgetattr");
                exit(1);
            }

            cfmakesane(&tios);

            error = tcsetattr(stdin_fd, TCSANOW, &tios);
            if(error == -1)
            {
                perror("tcsetattr");
                exit(1);
            }
        }

        /*dup2 handles the case where the fds are the same*/
        int error = dup_fd(stdin_fd, 0, "stdin");
        if(error) exit(1);

        error = dup_fd(stdout_fd, 1, "stdout");
        if(error) exit(1);

        error = dup_fd(stderr_fd, 2, "stderr");
        if(error) exit(1);

        closefrom(3);

        /*For now we keep the same environment or at least don't do anything special
         * to change the environment.  In the future we can use exect to setup environment
         * and do things with resource control, etc.*/

        error = execvp(args[0], args.data());
        if(error)
        {
            std::ostringstream msg;
            msg << "execvp error: " << strerror(errno) << std::endl;
            std::cerr << msg.str() << std::endl;
            exit(1);
        }
    }
    case -1:
        Tcl_SetResult(interp, (char*)Tcl_ErrnoMsg(errno), TCL_STATIC);
        return TCL_ERROR;
    default:
    {
        /*Parent*/
        if(async)
        {
            std::shared_ptr<monitored_process_group> mpg = std::make_shared<monitored_process_group>(pid);
            vessel_exec->mpgs.push_back(mpg);

            void* pge_buffer = Tcl_Alloc(sizeof(process_group_tcl_event));
            new(pge_buffer) process_group_tcl_event(interp, callback_prefix, mpg);

            struct kevent event;
            EV_SET(&event, pid, EVFILT_PROC, EV_ADD, NOTE_EXIT|NOTE_TRACK, 0, pge_buffer);

            int error = kevent(state->kqueue_fd, &event, 1, nullptr, 0, nullptr);
            if(error == -1)
            {
                return vessel::syserror_result(interp, "EXEC", "MONITOR", "KQUEUE");
            }
        }
        else
        {
            std::cerr << "Blocking wait for child process: " << pid << std::endl;
            int status = 0;
            pid_t waitedpid = waitpid(pid, &status, 0);
            std::cerr << "Waited on: " << waitedpid << ": " << WIFEXITED(status) << ","
                      << WIFCONTINUED(status) << "," << WIFSIGNALED(status) << ","
                      << WIFSTOPPED(status)<< std::endl;
            std::cerr << "Signal: " << WTERMSIG(status) << std::endl;
        }
        break;
    }
    }


    return TCL_OK;
}
