#include "app_functions.h"
#include "appc_tcl.h"
#include "cmdline.h"
#include "container.h"
#include "environment.h"
#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>

using namespace appc;

namespace {

/**
 * @brief The registry class is responsible for fetching the
 * image stack from some source.  That source may be filesystem,
 * ftp, https or some other source.
 *
 * NOTE: Remote zfs support would be a killer feature.  i.e. using zsend
 * and zrecv to retrieve images.
 */
class registry
{

};

/**
 * @brief The image_stack class represents a stack of images
 * that need to be mounted ontop of each other.
 */
class image_stack
{
    /* - stack of images that will be mounted
     * - mount(): mount the image stack using unionfs
     */
};

void validate_save_cmdline(commandline& cmdline)
{

}

int run_main(int argc, char** argv)
{
    Tcl_FindExecutable(argv[0]);

    /*Process commandline arguments
     * - name: name of the container
     * - image: Name of the filesystem image in the registry
     */
    std::unique_ptr<commandline> cmdline = commandline::parse(argc, argv);
    if(!cmdline)
    {
        exit(1);
    }

    environment env;

    appc::funcs::auto_unmount_ptr mountpoint = appc::funcs::mount_container_image(*cmdline, env);

    appc::jail the_jail(mountpoint->target(), cmdline->container);
    std::tuple<pid_t, int> child_ids = the_jail.fork_exec_jail(cmdline->container_cmd_args);

    pid_t child_pid = std::get<0>(child_ids);
    if(child_pid > 0)
    {
        int status = 0;
        pid_t wait_child_id = waitpid(child_pid, &status, 0);
        if(wait_child_id == -1)
        {
            std::cerr << "Error when waiting for child: " << strerror(errno) << std::endl;
            return 1;
        }
    }

    return 0;
}
}

int main(int argc, char** argv)
{
    int exit_code = 1;
    try
    {
        exit_code = run_main(argc, argv);
    }
    catch(std::exception& e)
    {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
    }
    catch(...)
    {
        std::cerr << "Fatal Error: " << "Unspecified" << std::endl;
    }

    exit(exit_code);
}
