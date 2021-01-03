#include "app_functions.h"
#include "fs.h"
#include <iostream>
#include <image_archive.h>

using namespace vessel::funcs;

void vessel::funcs::auto_unmount(vessel::mountpoint* mnt)
{
    if(mnt == nullptr)
    {
        return;
    }

    int err = mnt->unmount(MNT_WAIT);
    if(err)
    {
        std::cerr << "Error unmounting: " << strerror(errno) << std::endl;
    }
}

auto_unmount_ptr
vessel::funcs::mount_container_image(commandline &cmdline, environment& env)
{
    if(cmdline.container.empty())
    {
        std::cerr << "ERROR: 'name' is required" << std::endl;
        commandline::usage();
        exit(1);
    }

    if(cmdline.image.empty())
    {
        std::cerr << "ERROR: 'image' is required" << std::endl;
        commandline::usage();
        exit(1);
    }

    fs_path image_path = env.find_image(cmdline.image);
    if(image_path)
    {
        std::cerr << "Image found: " << image_path.str() << std::endl;
    }
    else
    {
        std::cerr << "No image found" << std::endl;
    }

    std::cerr << "container: " << cmdline.container << std::endl;
    fs_path container_dir = env.find_container(cmdline.container);
    if(container_dir)
    {
        std::cerr << "Container dir exists: " << container_dir.str() << std::endl;
    }
    else
    {
        throw std::runtime_error("NYI: Create container directory if it doesn't exist");
    }

    auto_unmount_ptr mntpoint(create_nullfs_mount(image_path, container_dir, 0).release(),
                              auto_unmount);

    std::cerr << "Mounting" << std::endl;
    int error = mntpoint->mount();
    if(error)
    {
        std::cerr << "Mount failed: " << mntpoint->error_message() << std::endl
                  << "system error: " << strerror(errno) << std::endl;
        exit(1);
    }

    return mntpoint;
}

void vessel::funcs::save_container_image(commandline& cmdline, environment& env)
{
    if(!cmdline.do_save)
    {
        throw std::logic_error("do_save has not been requested");
    }

    if(cmdline.container.empty())
    {
        std::cerr << "--name required." << std::endl;
        commandline::usage();
        exit(1);
    }

    fs_path container_dir = env.find_container(cmdline.container);
    if(!container_dir)
    {
        std::cerr << "container not found: " << cmdline.container << std::endl;
        exit(1);
    }

    fs_path image_dest{env.image_dir()};
    image_dest += container_dir.basename();
    image_dest.append_extension("img");

    int ret = create_image(container_dir, image_dest);
    if(ret)
    {
        std::cerr << "Error creating image" << std::endl;
        exit(1);
    }

    int count = 0;
    archive_image(image_dest, env.archive_dir(),
                  [&count](const compression_progress& progress) {
        if((count % 1000) == 0) {
        std::cerr << "File size: " << progress.file_size << std::endl
                  << "Total read: " << progress.total_read << std::endl
                  << "Total written: " << progress.total_compressed_written << std::endl
                  << "Read: " << progress.bytes_read << std::endl
                  << "Written: " << progress.bytes_written << std::endl
                  << "% complete: " << ((float)progress.total_read / (float)progress.file_size) * 100 << std::endl
                  << "Count: " << count << std::endl;

        }
        ++count;
    });

    exit(ret);
}
