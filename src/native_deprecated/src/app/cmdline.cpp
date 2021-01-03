#include "cmdline.h"
#include <getopt.h>
#include <iostream>

using namespace vessel;

namespace
{
    constexpr struct option long_options[] = {
        {"name", required_argument, nullptr, 'n'},
        {"image", required_argument, nullptr, 'i'},
        {"save", no_argument, nullptr, 's'},
        {"type", required_argument, nullptr, 't'},
        {nullptr, 0, nullptr, 0}
    };
}


void commandline::usage()
{
    /*TODO: implement commands instead of just flags
     * for example: vessel run OR vessel save
     */
    std::cerr << "USAGE: " << std::endl
              << "vessel --name=<container-name> --image=<image-name>" << std::endl
              << "OR" << std::endl
              << "vessel --save --type=<ufs|tgz>" << std::endl
              << std::endl;
}

std::unique_ptr<commandline> commandline::parse(int argc, char** argv)
{

    std::unique_ptr<commandline> _this = std::make_unique<commandline>();

    /*Capture all arguments after '--' to use as a container
     * command.  Update argc so those arguments are not passed
     * to getopt.
     */
    for(int i = 0; i < argc; i++)
    {
        std::string arg = argv[i];
        if(arg == "--")
        {
            for(int j = i + 1; j < argc; j++)
            {
                _this->container_cmd_args.push_back(argv[j]);
            }

            //Args needs to be null terminated to use with exec.
            _this->container_cmd_args.push_back(nullptr);
            argc = i;
            break;
        }
    }

    int ch = 0;
    while((ch = getopt_long(argc, argv, "n:i:st:", long_options, nullptr)) != -1)
    {
        switch(ch)
        {
        case 'n':
            _this->container = optarg;
            break;
        case 'i':
            _this->image = optarg;
            break;
        case 's':
            _this->do_save = true;
            _this->operation_count++;
            break;
        case 't':
            _this->image_type = optarg;
            break;
        default:
            usage();
        }
    }

    return _this;
}
