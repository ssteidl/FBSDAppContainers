#include "mountpoint.h"

#include "fs.h"
#include <iostream>
#include <map>
#include <string>

#include <sys/uio.h>
#include <vector>

using namespace vessel;

namespace
{
    class base_mountpoint : public mountpoint
    {
        fs_path m_from;
        fs_path m_target;

        using options = std::map<std::string, std::string>;
        options m_opts;

        int m_flags;

        std::string m_last_mount_err;

    protected:

        void add_option(const std::string& option, const std::string& value)
        {
            m_opts[option] = value;
        }

    public:

        std::string fspath() {

            return m_opts["fspath"];
        }

        /** NOTE: I don't yet handle ufs which may require "from"
          * instead of "target"
          */
        base_mountpoint(const fs_path& from, const fs_path& target,
                        const std::string fs_type, int flags)
            : m_from(from),
              m_target(target),
              m_opts({{"target", m_from.str()},
                      {"fspath", m_target.str()},
                      {"fstype", fs_type}}),
              m_flags(flags),
              m_last_mount_err()
        {}

        fs_path from() override
        {
            return m_from;
        }

        fs_path target() override
        {
            return m_target;
        }

        int mount() override
        {
            char errmsg[255];
            memset(errmsg, 0, sizeof(errmsg));
            std::vector<struct iovec> iovs;

            struct iovec iov;
            for(auto& option : m_opts) {

                iov.iov_base = (void*)option.first.c_str();
                iov.iov_len = option.first.length() + 1;
                iovs.push_back(iov);

                iov.iov_base = (void*)option.second.c_str();
                iov.iov_len = option.second.length() + 1;
                iovs.push_back(iov);

                std::cerr << option.first << "=" << option.second << std::endl;
            }

            iov.iov_base = (void*)"errmsg";
            iov.iov_len = sizeof("errmsg");
            iovs.push_back(iov);

            iov.iov_base = errmsg;
            iov.iov_len = sizeof(errmsg);
            iovs.push_back(iov);

            int err = ::nmount(iovs.data(), iovs.size(), m_flags);
            if(err == -1 && errmsg[0]) {
                m_last_mount_err = errmsg;
            }

            return err;
        }

        int unmount(int flags) override
        {
            return ::unmount(m_target.str().c_str(), flags);
        }

        std::string error_message() override
        {
            return m_last_mount_err;
        }
    };
}

mountpoint::~mountpoint()
{}

std::unique_ptr<mountpoint> vessel::create_nullfs_mount(fs_path from,
                                                      fs_path target,
                                                      int flags)
{
    return std::make_unique<base_mountpoint>(from, target, "nullfs", flags);
}
