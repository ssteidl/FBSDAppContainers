#include "cmdline.h"
#include <getopt.h>
#include <iostream>
#include <list>

using namespace appc;

namespace
{
    namespace command_strings
    {
        using command_name_list = std::list<std::string>;
        static const command_name_list command_names = {"run", "save", "publish"};
    }

    std::ostream& operator << (std::ostream& os, const command_strings::command_name_list& cmds)
    {
        bool first = true;
        for(auto& cmd : cmds)
        {
            if(!first)
            {
                os << ", ";
            }

            os << cmd;

            first = false;
        }

        return os;
    }

    constexpr struct option long_options[] = {
        {"name", required_argument, nullptr, 'n'},
        {"image", required_argument, nullptr, 'i'},
        {nullptr, 0, nullptr, 0}
    };
}


void commandline::usage()
{
    std::cerr << "USAGE: " << std::endl
              << "appc run --name=<container-name> --image=<image-name>" << std::endl
              << "appc save --name=<container-name>" << std::endl;
}

/**
 * Capture all arguments after '--' to use as a container
 * command.  Update argc so those arguments are not passed
 * to getopt.
 */
void commandline::parse_command_args(int argc, char** argv)
{
    for(int i = 0; i < argc; i++)
    {
        std::string arg = argv[i];
        if(arg == "--")
        {
            for(int j = i + 1; j < argc; j++)
            {
                container_cmd_args.push_back(argv[j]);
            }

            //Args needs to be null terminated to use with exec.
            container_cmd_args.push_back(nullptr);
            argc = i;
            break;
        }
    }
}

std::unique_ptr<commandline> commandline::parse(int argc, char** argv)
{

    std::unique_ptr<commandline> _this = std::make_unique<commandline>();

    /*Find and validate command*/
    if(argc >= 2)
    {
        std::string cmd_arg = argv[1];
        auto iter = std::find(std::begin(command_strings::command_names),
                              std::end(command_strings::command_names),
                              cmd_arg);

        if(iter != std::end(command_strings::command_names))
        {
            _this->cmd = *iter;
        }
        else
        {
            std::cerr << "Unknown command '" << cmd_arg << "'" << std::endl;
            return nullptr;
        }

        if(*iter == "run")
        {
            _this->parse_command_args(argc, argv);
        }
    }
    else
    {
        std::cerr << "No command found.  Second argument must be one of: "
                  << command_strings::command_names
                  << std::endl;

        return nullptr;
    }


    int ch = 0;
    while((ch = getopt_long(argc, argv, "n:i:", long_options, nullptr)) != -1)
    {
        switch(ch)
        {
        case 'n':
            _this->container = optarg;
            break;
        case 'i':
            _this->image = optarg;
            break;
        default:
            usage();
            return nullptr;
        }
    }

    return _this;
}
