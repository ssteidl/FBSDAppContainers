#include "container.h"
#include <sstream>
#include <unistd.h>

using namespace vessel;

vessel::jail::jail(fs_path path, const std::string& hostname)
    : m_jail()
{
    if(!path || !path.is_dir())
    {
        std::ostringstream msg;
        msg << "Jail path is not valid: " << path.str();
        throw std::logic_error(msg.str());
    }

    if(hostname.empty())
    {
        throw std::logic_error("Jail hostname cannot be empty");
    }

    memset(&m_jail, 0, sizeof(m_jail));

    m_jail.hostname = strdup(hostname.c_str());
    m_jail.path = strdup(path.str().c_str());
    m_jail.jailname = strdup(hostname.c_str());
}


//pid_t vessel::jail::fork_jail()
//{
//    pid_t child_id = fork();

//    switch(child_id)
//    {
//    case -1:
//        return ret;
//    case 0:
//        //Child
//        ::jail(&m_jail);
//        return 0;
//    default:
//        //Parent
//        ret

//    }
//}

std::tuple<pid_t, int> vessel::jail::fork_exec_jail(const std::vector<char*>& args)
{
    if(args.back() != nullptr)
    {
        throw std::logic_error("jail args must be null terminated");
    }

    pid_t child_id = fork();

    switch(child_id)
    {
    case -1:
        return {-1, -1};

    case 0:
        //Do jail and exec below
        break;

    default:
        /*TODO: Should we send the jail id via a pipe so we have
         * it in the parent*/
        return {child_id, 0};

    }

    int jail_id = ::jail(&m_jail);

    if(jail_id == -1)
    {
        std::ostringstream msg;
        msg << "Error jailing child process: " << strerror(errno);
        throw std::runtime_error(msg.str());
    }

    (void)execvp(args[0], args.data());

    //Should never get here.
    {
        std::ostringstream msg;
        msg << "Exec of child process failed: " << strerror(errno);
        throw std::runtime_error(msg.str());
    }
}

vessel::jail::~jail()
{
    if(m_jail.hostname) free(m_jail.hostname);
    if(m_jail.path) free(m_jail.path);
    if(m_jail.jailname) free(m_jail.jailname);
}
