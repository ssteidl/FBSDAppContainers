#include "app_functions.h"
#include "vessel_tcl.h"
#include "cmdline.h"
#include "environment.h"
#include <iostream>
#include <limits.h>
#include "mountpoint.h"
#include <unistd.h>

using namespace vessel;
using namespace vessel::funcs;

int run_main(int argc, char** argv)
{
    Tcl_FindExecutable(argv[0]);

    environment env;
    std::unique_ptr<commandline> cmdline = commandline::parse(argc, argv);

    if(!cmdline)
    {
        return 1;
    }

    auto_unmount_ptr mntpoint = mount_container_image(*cmdline, env);

    return 0;
}

int main(int argc, char** argv)
{

    /*
     * Arguments:
     * --image: Name of the image.  vessel_mount will look in APPC_IMAGE_DIR
     * to find the image, then nullfs mount the image to --name folder
     * in APPC_CONTAINER_DIR.
     */

    int exit_code = 1;
    try
    {
        exit_code = run_main(argc, argv);
    }
    catch(std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
    }
    catch(...)
    {
        std::cerr << "Unknown error occurred" << std::endl;
    }

    exit(exit_code);
}
