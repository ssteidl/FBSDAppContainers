#ifndef CMDLINE_H
#define CMDLINE_H

#include <string>
#include <list>
#include <vector>

namespace vessel
{

struct commandline
{
public:
    std::string image;
    std::string container;
    std::vector<char*> container_cmd_args;
    std::string image_type;
    int operation_count; /*Used for validation that only one
                          * operation is given.*/
    bool do_save;

    static void usage();

    static std::unique_ptr<commandline> parse(int argc, char** argv);
};
}
#endif // CMDLINE_H
