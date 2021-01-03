#ifndef CONTAINER_H
#define CONTAINER_H

#include "fs.h"

#include <tuple>

#include <sys/param.h>
extern "C"
{
#include <sys/jail.h>
}

#include <vector>

using sys_jail = struct jail;
namespace vessel
{
    class jail
    {
        sys_jail m_jail;

        jail(const jail& other) = delete;
        jail(jail&& other) = delete;
        jail& operator=(const jail& other) = delete;
        jail& operator=(jail&& other) = delete;
    public:
        jail(fs_path path, const std::string& hostname);

//        std::tuple<pid_t, int> fork_jail();

        std::tuple<pid_t, int> fork_exec_jail(const std::vector<char*>& args);

        ~jail();
    };
}

#endif // CONTAINER_H
