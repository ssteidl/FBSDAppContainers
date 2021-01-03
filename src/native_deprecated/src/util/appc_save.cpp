#include "app_functions.h"
#include "vessel_tcl.h"
#include "cmdline.h"
#include "environment.h"
#include "image_archive.h"
#include <iostream>

using namespace vessel;

int run_main(int argc, char** argv)
{
    Tcl_FindExecutable(argv[0]);
    std::unique_ptr<commandline> cmdline = commandline::parse(argc, argv);

    environment env;

    vessel::funcs::save_container_image(*cmdline, env);
    return 0;
}

int main(int argc, char** argv)
{
    try
    {
        return run_main(argc, argv);
    }
    catch(std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
        exit(1);
    }
    catch(...)
    {
        std::cerr << "Uncaught exception" << std::endl;
        exit(1);
    }

    return 0;
}
