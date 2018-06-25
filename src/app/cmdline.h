#ifndef CMDLINE_H
#define CMDLINE_H

#include <string>
#include <list>
#include <vector>

namespace appc
{

struct commandline
{
public:

    std::string image;
    std::string container;
    std::vector<char*> container_cmd_args;

    std::string cmd; /**< Which command is being run*/

private:

    void parse_command_args(int argc, char** argv);

public:

    static void usage();

    static std::unique_ptr<commandline> parse(int argc, char** argv);
};
}
#endif // CMDLINE_H
