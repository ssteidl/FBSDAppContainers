#include <iostream>

#include "fs.h"

using namespace vessel;
int main(int argc, char** argv)
{
    Tcl_FindExecutable(argv[0]);

    fs_path path1 = std::string("/home/shane/");
    std::cerr << "path1: " << path1.str() << std::endl;

    {
        fs_path path2 = path1;
        path2 += "jack/box";
        std::cerr << "path2: " << path2.str() << std::endl;

        path2.append_extension("tgz");
        std::cerr << "path2: " << path2.str() << std::endl;
    }

    Tcl_Exit(0);
}
