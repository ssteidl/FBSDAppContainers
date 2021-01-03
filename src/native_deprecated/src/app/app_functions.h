#ifndef APP_FUNCTIONS_H
#define APP_FUNCTIONS_H

#include "cmdline.h"
#include "environment.h"
#include "mountpoint.h"

/*
 * The app functions are building blocks for
 * the different applications and utilities.
 * vessel will use all of them and the utilities
 * will generally use some subset.
 *
 * For example, vessel_mount will use
 * mount_container_image().
 */
namespace vessel { namespace funcs {
    void auto_unmount(vessel::mountpoint* mnt);
    using auto_unmount_ptr = std::unique_ptr<mountpoint, decltype(&auto_unmount)>;

    auto_unmount_ptr mount_container_image(commandline& cmdline, environment& env);
    
    void save_container_image(commandline& cmdline, environment& env);
}}

#endif // APP_FUNCTIONS_H
