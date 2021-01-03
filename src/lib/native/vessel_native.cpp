#include <array>
#include <cassert>
#include <cerrno>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <tcl.h>
#include <unistd.h>
#include <vector>

#include <getopt.h>

#include "../../dns/embdns.h"
#include "exec.h"
#include "tcl_util.h"
#include "url_cmd.h"

namespace
{
    std::vector<const char*> argv_vector_from_command_args(int argc, Tcl_Obj** args)
    {
        std::vector<const char*> argv{};

        for(int i = 0; i < argc; ++i)
        {
            argv.push_back(Tcl_GetString(args[i]));
        }

        return argv;
    }

    std::string build_options_help()
    {
        std::ostringstream msg;
        /*Probably not great design to output help from the library but
         * it's simple and good enough for now*/
        msg << "vesseld build <--file=vesseld_file> <--name=image-name> <--tag=tag>" << std::endl
            << "--file    Path to the vessel file used to build the image" << std::endl
            << "--name    Name of the image" << std::endl
            << "--tag     Tag for the image.  Only one tag is accepted.  Default is 'latest'" << std::endl;

        return msg.str();
    }

    int parse_build_options(Tcl_Interp* interp, int argc, Tcl_Obj** args, Tcl_Obj* options_dict)
    {
        assert(argc > 0);

        static const struct option long_opts[] = {
            {"file", required_argument, nullptr, 'f'},
            {"help", no_argument, nullptr, 'h'},
            {"name", required_argument, nullptr, 'n'},
            {"tag", required_argument, nullptr, 't'},
            {nullptr, 0, nullptr, 0}
        };

        std::vector<const char*> argv = argv_vector_from_command_args(argc, args);

        int ch = -1;

        vessel::tclobj_ptr args_dict(Tcl_NewDictObj(), vessel::unref_tclobj);
        int tcl_error = TCL_OK;
        while((ch = getopt_long(argc, (char* const *)argv.data(), "f:hn:t:", long_opts, nullptr)) != -1)
        {
            switch(ch)
            {
            case 'f':
                tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                           Tcl_NewStringObj("file", -1),
                                           Tcl_NewStringObj(optarg, -1));
                if(tcl_error) return tcl_error;
                break;
            case 'h':
            {
                std::string help_msg = build_options_help();
                tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                           Tcl_NewStringObj("help", -1),
                                           Tcl_NewStringObj(help_msg.c_str(), help_msg.size()));
                break;
            }
            case 'n':
                tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                           Tcl_NewStringObj("name", -1),
                                           Tcl_NewStringObj(optarg, -1));
                if(tcl_error) return tcl_error;
                break;
            case 't':
                tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                           Tcl_NewStringObj("tag", -1),
                                           Tcl_NewStringObj(optarg, -1));
                if(tcl_error) return tcl_error;
                break;
            case ':':
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("Missing argument for optind: %d", optind));
                return TCL_ERROR;
            case '?':
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("Unknown option for optind: %d", optind));
                return TCL_ERROR;
            default:
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("Error parsing options.  optind: %d", optind));
                return TCL_ERROR;
            }
        }

        tcl_error = Tcl_DictObjPut(interp, options_dict,Tcl_NewStringObj("args", -1), args_dict.release());
        return tcl_error;
    }

    int parse_create_network_options(Tcl_Interp* interp, int argc, Tcl_Obj** args, Tcl_Obj* options_dict)
    {
        assert(argc > 0);

        static const struct option long_opts[] = {
            {"name", required_argument, nullptr, 'n'},
            {"dns", required_argument, nullptr, 'd'},
            {"ip", required_argument, nullptr, 'i'},
            {nullptr, 0, nullptr, 0}
        };

        /*dns options are of the form "name:ip"*/

        std::vector<const char*> argv = argv_vector_from_command_args(argc, args);

        int ch = -1;

        vessel::tclobj_ptr args_dict(Tcl_NewDictObj(), vessel::unref_tclobj);
        vessel::tclobj_ptr dns_list(Tcl_NewListObj(0, nullptr), vessel::unref_tclobj);
        int tcl_error = TCL_OK;
        while((ch = getopt_long(argc, (char* const *)argv.data(), "n:d:i:", long_opts, nullptr)) != -1)
        {
            switch(ch)
            {
            case 'n':
                tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                           Tcl_NewStringObj("name", -1),
                                           Tcl_NewStringObj(optarg, -1));
                if(tcl_error) return tcl_error;
                break;
            case 'd':
                tcl_error = Tcl_ListObjAppendElement(interp, dns_list.get(),
                                                     Tcl_NewStringObj(optarg, -1));
                if(tcl_error) return tcl_error;
                break;
            case 'i':
                tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                           Tcl_NewStringObj("ip", -1),
                                           Tcl_NewStringObj(optarg, -1));
                if(tcl_error) return tcl_error;
                break;
            default:
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("Error parsing options.  optind: %d", optind));
                return TCL_ERROR;
            }
        }

        tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                   Tcl_NewStringObj("dns", -1),
                                   dns_list.release());

        tcl_error = Tcl_DictObjPut(interp, options_dict,Tcl_NewStringObj("args", -1), args_dict.release());
        return tcl_error;
    }

    std::string run_options_help()
    {
        std::ostringstream msg;
        msg << "vessel run <--name=container_name> {--interactive} {--rm} {--volume=/path/to/hostdir:/path/to/mountdir} \n{--dataset=zfs/dataset:container_mountpoint} image{:tag} {command...}" << std::endl << std::endl
            << "--name        Name of the new container" << std::endl
            << "--interactive Start an interactive shell via pty" << std::endl
            << "--rm          Remove the image after it exits" << std::endl
            << "--dataset     Use the specified zfs dataset at the mountpoint.  Creates the dataset if needed." << std::endl
            << "--volume      Path to directory to nullfs mount into the container" << std::endl;

        return msg.str();
    }

    int parse_run_options(Tcl_Interp* interp, int argc, Tcl_Obj** args, Tcl_Obj* options_dict)
    {
        /*At some point we will have a server and need a -it flag*/
        /*vessel run <layer> */

        assert(argc > 0);

        static const struct option long_opts[] = {
            {"volume", required_argument, nullptr, 'v'},
            {"rm", no_argument, nullptr, 'r'},
            {"help", no_argument, nullptr, 'h'},
            {"interactive", no_argument, nullptr, 'i'},
            {"name", required_argument, nullptr, 'n'},
            {"dataset", required_argument, nullptr, 'd'},
            {"network", required_argument, nullptr, 'u'},
            {nullptr, 0, nullptr, 0}
        };

        std::vector<const char*> argv = argv_vector_from_command_args(argc, args);

        int ch = -1;

        bool remove_container = false;
        vessel::tclobj_ptr args_dict(Tcl_NewDictObj(), vessel::unref_tclobj);
        int tcl_error = TCL_OK;
        bool interactive = false;
        std::string network("inherit");
        vessel::tclobj_ptr dataset_list(Tcl_NewListObj(0, nullptr), vessel::unref_tclobj);
        vessel::tclobj_ptr volume_list(Tcl_NewListObj(0, nullptr), vessel::unref_tclobj);
        while((ch = getopt_long(argc, (char* const *)argv.data(), "d:iv:hn:u:", long_opts, nullptr)) != -1)
        {
            switch(ch)
            {
            case 'd':
                tcl_error = Tcl_ListObjAppendElement(interp, dataset_list.get(), Tcl_NewStringObj(optarg, -1));
                if(tcl_error) return tcl_error;
                break;
            case 'v':
                tcl_error = Tcl_ListObjAppendElement(interp, volume_list.get(), Tcl_NewStringObj(optarg, -1));
                if(tcl_error) return tcl_error;
                break;
            case 'r':
                remove_container = true;
                break;
            case 'h':
            {
                std::string help_msg = run_options_help();
                tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                           Tcl_NewStringObj("help", -1),
                                           Tcl_NewStringObj(help_msg.c_str(), help_msg.size()));
                if(tcl_error) return tcl_error;

                /*Short circuit for help flag*/
                tcl_error = Tcl_DictObjPut(interp, options_dict,
                                           Tcl_NewStringObj("args", -1),
                                           args_dict.release());
                if(tcl_error) return tcl_error;

                return TCL_OK;
            }
            case 'i':
                interactive = true;
                break;
            case 'n':
                tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                           Tcl_NewStringObj("name", -1),
                                           Tcl_NewStringObj(optarg, -1));
                if(tcl_error) return tcl_error;
                break;
            case 'u':
                network = optarg;
                break;
            case ':':
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("Missing argument for optind: %d", optind));
                return TCL_ERROR;
            case '?':
                /*Ignore unknown options case because there could be options in the command arguments.*/
                break;
            default:
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("Error parsing options.  optind: %d", optind));
                return TCL_ERROR;
            }
        }

        tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                   Tcl_NewStringObj("datasets", -1),
                                   dataset_list.release());
        if(tcl_error) return tcl_error;

        tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                   Tcl_NewStringObj("volumes", -1),
                                   volume_list.release());
        if(tcl_error) return tcl_error;

        tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                   Tcl_NewStringObj("remove", -1),
                                   Tcl_NewBooleanObj(remove_container));
        if(tcl_error) return tcl_error;

        tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                   Tcl_NewStringObj("interactive", -1),
                                   Tcl_NewBooleanObj(interactive));
        if(tcl_error) return tcl_error;

        tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                   Tcl_NewStringObj("network", -1),
                                   Tcl_NewStringObj(network.c_str(), network.size()));
        if(tcl_error) return tcl_error;

        if(optind == argc)
        {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("Missing image name in run command", -1));
            return TCL_ERROR;
        }

        tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                   Tcl_NewStringObj("image", -1),
                                   Tcl_NewStringObj(argv[optind], -1));
        if(tcl_error) return tcl_error;

        optind++;

        if (optind < argc)
        {
            vessel::tclobj_ptr command_list(Tcl_NewListObj(0, nullptr), vessel::unref_tclobj);

            for(; optind < argc; ++optind)
            {
                tcl_error = Tcl_ListObjAppendElement(interp, command_list.get(),
                                                     Tcl_NewStringObj(argv[optind], -1));
                if(tcl_error) return tcl_error;
            }
            tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                       Tcl_NewStringObj("command", -1),
                                       command_list.release());
            if(tcl_error) return tcl_error;
        }

        tcl_error = Tcl_DictObjPut(interp, options_dict,
                                   Tcl_NewStringObj("args", -1),
                                   args_dict.release());
        return TCL_OK;
    }

    std::string publish_pull_options_help()
    {
        std::ostringstream msg;
        msg << "push|pull <--tag=val> image" << std::endl
            << "--tag    The tag value" << std::endl
            << "--help   Print this help message"
            << "image    The image to pull or publish" << std::endl;
        return msg.str();
    }

    int parse_publish_pull_options(Tcl_Interp* interp, int argc,
                                   Tcl_Obj** args, Tcl_Obj* options_dict)
    {
        assert(argc > 0);

        static const struct option long_opts[] = {
            {"help", no_argument, nullptr, 'h'},
            {"tag", required_argument, nullptr, 't'},
            {nullptr, 0, nullptr, 0}
        };

        if(argc < 2)
        {
            Tcl_WrongNumArgs(interp, argc, args, "publish <image> [--tag]");
            return TCL_ERROR;
        }

        std::vector<const char*> argv = argv_vector_from_command_args(argc, args);

        int ch = -1;

        std::string image{};
        std::string tag{};
        vessel::tclobj_ptr args_dict(Tcl_NewDictObj(), vessel::unref_tclobj);
        int tcl_error = TCL_OK;
        while((ch = getopt_long(argc, (char* const *)argv.data(), "ht:", long_opts, nullptr)) != -1)
        {
            switch(ch)
            {
            case 'h':
            {
                std::string help_msg = publish_pull_options_help();
                tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                           Tcl_NewStringObj("help", -1),
                                           Tcl_NewStringObj(help_msg.c_str(), help_msg.size()));
                if(tcl_error) return tcl_error;

                /*Short circuit for help flag*/
                tcl_error = Tcl_DictObjPut(interp, options_dict,
                                           Tcl_NewStringObj("args", -1),
                                           args_dict.release());
                if(tcl_error) return tcl_error;

                return TCL_OK;

            }
            case 't':
                tag = optarg;
                break;
            case ':':
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("Missing argument for optind: %d", optind));
                return TCL_ERROR;
            case '?':
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("Unknown argument for optind: %d", optind));
                return TCL_ERROR;
            default:
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("Unknown error for optind: %d", optind));
                return TCL_ERROR;
            }
        }

        if((argc - optind) <= 0)
        {
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("image name not provided"));
            return TCL_ERROR;
        }

        image = argv[optind];

        tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                       Tcl_NewStringObj("tag", -1),
                       Tcl_NewStringObj(tag.c_str(), tag.size()));
        if(tcl_error) return tcl_error;

        tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                       Tcl_NewStringObj("image", -1),
                       Tcl_NewStringObj(image.c_str(), image.size()));
        if(tcl_error) return tcl_error;

        tcl_error = Tcl_DictObjPut(interp, options_dict,
                       Tcl_NewStringObj("args", -1),
                       args_dict.release());
        if(tcl_error) return tcl_error;

        return TCL_OK;
    }

    std::string image_options_help()
    {
        std::ostringstream msg;
        msg << "vesseld image <options>" << std::endl
            << "--list    List all images available in the vessel dataset" << std::endl
            << "--help    Print this help message" << std::endl;

        return msg.str();
    }

    int parse_image_options(Tcl_Interp* interp, int argc,
                            Tcl_Obj** args, Tcl_Obj* options_dict)
    {
        assert(argc > 0);

        static const struct option long_opts[] = {
            {"list", no_argument, nullptr, 'l'},
            {"help", no_argument, nullptr, 'h'},
            {nullptr, 0, nullptr, 0}
        };

        if(argc < 2)
        {
            Tcl_WrongNumArgs(interp, argc, args, "image <options>");
            return TCL_ERROR;
        }

        std::vector<const char*> argv = argv_vector_from_command_args(argc, args);

        int ch = -1;

        bool list = false;
        vessel::tclobj_ptr args_dict(Tcl_NewDictObj(), vessel::unref_tclobj);
        int tcl_error = TCL_OK;
        while((ch = getopt_long(argc, (char* const *)argv.data(), "hl", long_opts, nullptr)) != -1)
        {
            switch(ch)
            {
            case 'h':
            {
                std::string help_msg = image_options_help();
                tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                           Tcl_NewStringObj("help", -1),
                                           Tcl_NewStringObj(help_msg.c_str(), help_msg.size()));
                if(tcl_error) return tcl_error;

                /*Short circuit for help flag*/
                tcl_error = Tcl_DictObjPut(interp, options_dict,
                                           Tcl_NewStringObj("args", -1),
                                           args_dict.release());
                if(tcl_error) return tcl_error;

                return TCL_OK;

            }
            case 'l':
                list = true;
                break;
            case ':':
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("Missing argument for optind: %d", optind));
                return TCL_ERROR;
            case '?':
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("Unknown argument for optind: %d", optind));
                return TCL_ERROR;
            default:
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("Unknown error for optind: %d", optind));
                return TCL_ERROR;
            }
        }

        tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                       Tcl_NewStringObj("list", -1),
                       Tcl_NewBooleanObj(list));
        if(tcl_error) return tcl_error;

        tcl_error = Tcl_DictObjPut(interp, options_dict,
                       Tcl_NewStringObj("args", -1),
                       args_dict.release());
        if(tcl_error) return tcl_error;

        return TCL_OK;
    }

    std::string export_options_help()
    {
        std::ostringstream msg;
        msg << "vessel export {--dir} image:tag" << std::endl
            << "--dir     Directory to output the image" << std::endl
            << "--help    Print this help message" << std::endl;

        return msg.str();
    }

    int parse_export_options(Tcl_Interp* interp, int argc,
                            Tcl_Obj** args, Tcl_Obj* options_dict)
    {
        assert(argc > 0);

        static const struct option long_opts[] = {
            {"dir", required_argument, nullptr, 'd'},
            {"help", no_argument, nullptr, 'h'},
            {nullptr, 0, nullptr, 0}
        };

        if(argc < 2)
        {
            Tcl_WrongNumArgs(interp, argc, args, "image <options>");
            return TCL_ERROR;
        }

        std::vector<const char*> argv = argv_vector_from_command_args(argc, args);

        int ch = -1;

        char pwd_buf[MAXPATHLEN+1];
        memset(pwd_buf, 0, sizeof(pwd_buf));
        vessel::tclobj_ptr pwd_obj(Tcl_NewStringObj(getwd(pwd_buf), -1), vessel::unref_tclobj);
        vessel::tclobj_ptr args_dict(Tcl_NewDictObj(), vessel::unref_tclobj);
        int tcl_error = TCL_OK;
        while((ch = getopt_long(argc, (char* const *)argv.data(), "d:h", long_opts, nullptr)) != -1)
        {
            switch(ch)
            {
            case 'd':
                pwd_obj = vessel::tclobj_ptr(Tcl_NewStringObj(optarg, -1), vessel::unref_tclobj);
                break;
            case 'h':
            {
                std::string help_msg = export_options_help();
                tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                                           Tcl_NewStringObj("help", -1),
                                           Tcl_NewStringObj(help_msg.c_str(), help_msg.size()));
                if(tcl_error) return tcl_error;

                /*Short circuit for help flag*/
                tcl_error = Tcl_DictObjPut(interp, options_dict,
                                           Tcl_NewStringObj("args", -1),
                                           args_dict.release());
                if(tcl_error) return tcl_error;

                return TCL_OK;

            }
            case ':':
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("Missing argument for optind: %d", optind));
                return TCL_ERROR;
            case '?':
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("Unknown argument for optind: %d", optind));
                return TCL_ERROR;
            default:
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("Unknown error for optind: %d", optind));
                return TCL_ERROR;
            }
        }


        if(optind == argc)
        {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("Missing <image:tag> in export command", -1));
            return TCL_ERROR;
        }

        tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                       Tcl_NewStringObj("dir", -1),
                       pwd_obj.release());
        if(tcl_error) return tcl_error;

        vessel::tclobj_ptr image_obj(Tcl_NewStringObj(argv[optind], -1), vessel::unref_tclobj);
        tcl_error = Tcl_DictObjPut(interp, args_dict.get(),
                       Tcl_NewStringObj("image", -1),
                       image_obj.release());
        if(tcl_error) return tcl_error;

        tcl_error = Tcl_DictObjPut(interp, options_dict,
                       Tcl_NewStringObj("args", -1),
                       args_dict.release());
        if(tcl_error) return tcl_error;

        return TCL_OK;
    }

    /**
     * vessel::parse_options $argc $argv
     */
    int Vessel_ParseOptions(void *clientData, Tcl_Interp *interp,
                          int objc, struct Tcl_Obj *const *objv)
    {
        (void)clientData;
        if(objc < 2)
        {
            Tcl_WrongNumArgs(interp, objc, objv, "Only one arg expected. <argv>");
            return TCL_ERROR;
        }

        int arg_count = 0;
        Tcl_Obj** argument_objs = nullptr;
        int tcl_error = Tcl_ListObjGetElements(interp, objv[1], &arg_count, &argument_objs);
        if(tcl_error)
        {
            return tcl_error;
        }

        if(arg_count == 0)
        {
            Tcl_SetResult(interp, (char*)"subcommand not provided", TCL_STATIC);
            Tcl_SetErrorCode(interp, "ARGS");
            return TCL_ERROR;
        }

        vessel::tclobj_ptr command_options(Tcl_NewDictObj(), vessel::unref_tclobj);
        Tcl_Obj* pre_command_flags = Tcl_NewDictObj();
        tcl_error = Tcl_DictObjPut(interp, command_options.get(),
                                   Tcl_NewStringObj("pre_command_flags", -1),
                                   pre_command_flags);
        if(tcl_error) return tcl_error;

        tcl_error = Tcl_DictObjPut(interp, pre_command_flags,
                                   Tcl_NewStringObj("local", -1),
                                   Tcl_NewBooleanObj(0));
        if(tcl_error) return tcl_error;

        /*Parse pre command options like --local*/
        int command_index = 0; /*Index of the command in the options list*/
        for (int i=0; i < arg_count; i++)
        {
            std::string pre_command_option(Tcl_GetStringFromObj(argument_objs[i], nullptr));
            if(pre_command_option == "--local")
            {
                tcl_error = Tcl_DictObjPut(interp, pre_command_flags,
                                           Tcl_NewStringObj("local", -1),
                                           Tcl_NewBooleanObj(1));
            }
            else
            {
                break;
            }

            command_index++;
        }
        arg_count -= command_index;
        argument_objs += command_index;
        std::string command(Tcl_GetString(argument_objs[0]));

        /*TODO: Make command lookup table for commands.*/
        tcl_error = Tcl_DictObjPut(interp, command_options.get(), Tcl_NewStringObj("args", -1),
                                   Tcl_NewStringObj("", 0));
        if(tcl_error) return tcl_error;

        if(command == "build")
        {
            //parse args if any remaining.  shift to first non-command argument
            tcl_error = parse_build_options(interp, arg_count, argument_objs, command_options.get());
            if(tcl_error) return tcl_error;
        }
        else if(command == "run")
        {
            tcl_error = parse_run_options(interp, arg_count, argument_objs, command_options.get());
            if(tcl_error) return tcl_error;
        }
        else if(command == "publish")
        {
            tcl_error = parse_publish_pull_options(interp, arg_count,
                                                   argument_objs, command_options.get());
            if(tcl_error) return tcl_error;
        }
        else if(command == "pull")
        {
            tcl_error = parse_publish_pull_options(interp, arg_count,
                                                   argument_objs, command_options.get());
            if(tcl_error) return tcl_error;
        }
        else if(command == "create-network")
        {
            tcl_error = parse_create_network_options(interp, arg_count,
                                                     argument_objs, command_options.get());
            if(tcl_error) return tcl_error;
        }
        else if(command == "image")
        {
            tcl_error = parse_image_options(interp, arg_count, argument_objs, command_options.get());
            if(tcl_error) return tcl_error;
        }
        else if(command == "export")
        {
            tcl_error = parse_export_options(interp, arg_count, argument_objs, command_options.get());
            if(tcl_error) return tcl_error;
        }
        else
        {
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("Unknown command given: %s", command.c_str()));
            return TCL_ERROR;
        }

        tcl_error = Tcl_DictObjPut(interp, command_options.get(),
                                   Tcl_NewStringObj("command", -1),
                                   argument_objs[0]);
        if(tcl_error) return tcl_error;
        Tcl_SetObjResult(interp, command_options.release());
        return TCL_OK;
    }

    int Vessel_DNSParseQuery(void *clientData, Tcl_Interp *interp,
                           int objc, struct Tcl_Obj *const *objv)
    {
        /*vessel::dns::parse_query <binary obj>
         * @returns array with the following keys:
         *     qname, type, class
         */

        if(objc != 2)
        {
            Tcl_WrongNumArgs(interp, objc, objv, "binary_buffer");
            return TCL_ERROR;
        }

        int buf_len = 0;
        unsigned char* query_buf = Tcl_GetByteArrayFromObj(objv[1], &buf_len);

        embdns::dns_query query = embdns::parse_packet(query_buf, (size_t)buf_len);

        Tcl_Obj* query_dict = Tcl_NewDictObj();
        Tcl_DictObjPut(interp, query_dict,
                       Tcl_NewStringObj("qname", -1),
                       Tcl_NewStringObj(query.qname.c_str(), query.qname.size()));

        Tcl_DictObjPut(interp, query_dict,
                       Tcl_NewStringObj("type", -1),
                       Tcl_NewIntObj(query.qtype) /*TODO: map int to string*/);

        Tcl_DictObjPut(interp, query_dict,
                       Tcl_NewStringObj("class", -1),
                       Tcl_NewIntObj(query.qclass) /*TODO: map int to string*/);

        Tcl_DictObjPut(interp, query_dict,
                       Tcl_NewStringObj("raw_query", -1),
                       Tcl_NewByteArrayObj(query.raw().data(), query.raw().size()));

        Tcl_SetObjResult(interp, query_dict);
        return TCL_OK;
    }


    int Vessel_DNSGenerateAResponse(void *clientData, Tcl_Interp *interp,
                                 int objc, struct Tcl_Obj *const *objv)
    {

        /*generate_A_response addr ttl raw_query*/
        if(objc != 4)
        {
            Tcl_WrongNumArgs(interp, objc, objv, "addr ttl raw_query");
            return TCL_ERROR;
        }

        std::string addr(Tcl_GetString(objv[1]));
        uint32_t ttl = 0;
        int tcl_error = Tcl_GetIntFromObj(interp, objv[2], (int*)&ttl);
        if(tcl_error) return tcl_error;

        int query_size = 0;
        unsigned char* raw_query = Tcl_GetByteArrayFromObj(objv[3], &query_size);

        embdns::dns_query query = embdns::parse_packet(raw_query, (size_t)query_size);
        if(query.qtype != 1)
        {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("Attempted to generate dns A record response for non A record query", -1));
            return TCL_ERROR;
        }

        std::array<uint8_t, embdns::dns_message::MAX_SIZE> pkt_buf;
        size_t response_size = 0;

        if(!addr.empty())
        {
            response_size = embdns::generate_response(pkt_buf, query, addr, ttl);
        }
        else
        {
            response_size = embdns::generate_response(pkt_buf, query);
        }
        Tcl_Obj* response_buf_obj = Tcl_NewByteArrayObj(pkt_buf.data(), (int)response_size);
        Tcl_SetObjResult(interp, response_buf_obj);
        return TCL_OK;
    }

    void init_dns(Tcl_Interp* interp)
    {
        (void)Tcl_CreateObjCommand(interp, "vessel::dns::parse_query", Vessel_DNSParseQuery, nullptr, nullptr);
        (void)Tcl_CreateObjCommand(interp, "vessel::dns::generate_A_response", Vessel_DNSGenerateAResponse,
                                   nullptr, nullptr);
    }

    void init_url(Tcl_Interp* interp)
    {
        (void)Tcl_CreateObjCommand(interp, "vessel::url::parse", Vessel_ParseURL, nullptr, nullptr);
    }

    void init_exec(Tcl_Interp* interp)
    {
        (void)Vessel_ExecInit(interp);
    }
}

extern "C" {

/*Forward declare PTY_Init from pty.c module.*/
int
Pty_Init(Tcl_Interp *interp);

int
Udp_Init(Tcl_Interp *interp);

extern int Vesseltcl_Init(Tcl_Interp* interp)
{
    if(Tcl_InitStubs(interp, "8.6", 0) == nullptr)
    {
        return TCL_ERROR;
    }

    (void)Tcl_CreateObjCommand(interp, "vessel::parse_options", Vessel_ParseOptions, nullptr, nullptr);

    init_dns(interp);
    init_url(interp);
    init_exec(interp);
    Pty_Init(interp);
    Udp_Init(interp);
    Tcl_PkgProvide(interp, "vessel::native", "1.0.0");

    return TCL_OK;
}
}
