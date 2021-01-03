#ifndef MOUNTPOINT_H
#define MOUNTPOINT_H

#include "fs.h"
#include <memory>
#include <string>

/*Include these so that the MNT_* values
 * are automatically available to clients.
 */
#include <sys/param.h>
#include <sys/mount.h>

namespace vessel
{
    class mountpoint
    {
    public:

        virtual fs_path from() = 0;
        virtual fs_path target() = 0;

        virtual int mount() = 0;
        virtual int unmount(int flags) = 0;

        /**
         * Error message string from the last mount attempt.
         * this may be different then the error message associated
         * with errno
         */
        virtual std::string error_message() = 0;

        virtual ~mountpoint();
    };

    std::unique_ptr<mountpoint> create_nullfs_mount(fs_path from,
                                                    fs_path target,
                                                    int mount_flags);
}
#endif // MOUNTPOINT_H
