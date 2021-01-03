#include "image_archive.h"

#include <archive.h>
#include <archive_entry.h>
#include <cassert>
#include <fcntl.h>
#include "fs.h"
#include <iostream>
#include <memory>
#include <paths.h>
#include <signal.h>
#include <sstream>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
using namespace vessel;

namespace
{
    void _free_write_archive(archive* ptr)
    {
        if(ptr)
        {
            archive_write_free(ptr);
        }
    }

    using write_archive_ptr = std::unique_ptr<archive, decltype(&_free_write_archive)>;
    write_archive_ptr create_write_archive()
    {
        return write_archive_ptr(archive_write_new(), _free_write_archive);
    }

    void _free_archive_entry(archive_entry* entry)
    {
        if(entry)
        {
            archive_entry_free(entry);
        }
    }

    using archive_entry_ptr = std::unique_ptr<archive_entry, decltype(&_free_archive_entry)>;
    archive_entry_ptr create_archive_entry(archive* a)
    {
        return archive_entry_ptr(archive_entry_new2(a), _free_archive_entry);
    }

    void xz_compress_image(fs_path image,
                           fs_path output_file,
                           const compression_progress_callback& progress_cb)
    {
        write_archive_ptr image_archive = create_write_archive();

        (void)archive_write_set_format_pax(image_archive.get());
        (void)archive_write_add_filter_xz(image_archive.get());
        archive_write_set_bytes_per_block(image_archive.get(), 10);

        int archive_error = archive_write_open_filename(image_archive.get(),
                                                        output_file.str().c_str());
        if(archive_error)
        {
            std::ostringstream msg;
            msg << "Error opening file for write: "
                << archive_error_string(image_archive.get())
                << std::endl;

            throw std::runtime_error(msg.str());
        }

        resource_fd fd = open(image.str().c_str(), O_RDONLY);
        if(fd == -1)
        {
            std::ostringstream msg;
            msg << "Error opening image file for compression: '" << image.str()
                << "' " << strerror(errno);
            throw std::runtime_error(msg.str());
        }

        archive_entry_ptr entry = create_archive_entry(image_archive.get());
        std::unique_ptr<path_stat> sb = image.stat();

        archive_entry_copy_stat(entry.get(), &sb->stat_buf);
        archive_entry_set_pathname(entry.get(), image.str().c_str());
        //NOTE: Need to set fflags if create image with all files instead of using
        //makefs

        int header_error = archive_write_header(image_archive.get(), entry.get());
        assert(header_error == ARCHIVE_OK);

        char buffer[BUFSIZ];
        memset(buffer, 0, sizeof(buffer));

        size_t total_bytes_read = 0;
        size_t total_bytes_written = 0;
        while(true)
        {
            ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
            if(bytes_read == -1)
            {
                std::ostringstream msg;
                msg << "Error reading from image: '" << image.str()
                    << "': " << strerror(errno);
                throw std::runtime_error(msg.str());
            }
            else if(bytes_read == 0)
            {
                //EOF
                break;
            }
            total_bytes_read += bytes_read;

            ssize_t bytes_written = archive_write_data(image_archive.get(), buffer, bytes_read);
            if(bytes_written == -1)
            {
                std::ostringstream msg;
                msg << "Error compressing image '" << image.str()
                    << "': " << archive_error_string(image_archive.get());
                throw std::runtime_error(msg.str());
            }
            total_bytes_written += bytes_written;

            /*TODO: Some sort of progress callback
             * (total_size, bytes_read, compressed_bytes_written,
             *  total_bytes_read, total_compressed_bytes_written)
             */
            if(progress_cb)
            {
                compression_progress progress;
                progress = {sb->stat_buf.st_size, bytes_read, bytes_written,
                            total_bytes_read, total_bytes_written};
                progress_cb(progress);
            }
        }
    }

    /**
     * @brief run_makefs Runs the "makefs" utility to create a ufs image.
     * @param source Directory which will be made into an image
     * @param dest Path the the image file which will be created.
     * @return Returns the exit code from makefs.
     */
    int run_makefs(fs_path source,
                   fs_path dest)
    {
        //TODO: mask sigchild and other necessary signals. Perhaps just
        //use SA_RESTART for now.
        pid_t child_pid = fork();

        //TODO: Implement details.
        switch(child_pid)
        {
        case -1:
        {
            std::ostringstream msg;
            msg << "Error forking makefs subprocess: " << strerror(errno);
            throw std::runtime_error(msg.str());
            break;
        }
        case 0:
        {
            //child
            std::vector<char const*> args;
            std::string src_str = source.str();
            std::string dest_str = dest.str();
            args.push_back("makefs");
            args.push_back("-t");
            args.push_back("ffs");
            args.push_back(dest_str.c_str());
            args.push_back(src_str.c_str());
            args.push_back(nullptr);

            for(auto arg : args)
            {
                if(arg == nullptr)
                {
                    break;
                }
                std::cerr << arg << " ";
            }
            std::cerr << std::endl;
            int error = execvp("makefs", (char* const*)args.data());
            if(error)
            {
                std::ostringstream msg;
                msg << "Error exec'ing makefs: " << strerror(errno);
                throw std::runtime_error(msg.str());
            }
        }
            break;
        default:
        {

            int status = 0;
            __wrusage resources;
            siginfo_t siginfo;
            pid_t pid = wait6(P_PID, child_pid, &status, WEXITED,
                              &resources, &siginfo);

            std::cerr << "Wait6 return: " << pid << std::endl;
            //TODO: Do something with the resources and siginfo.  We should
            //return an object with easy access to all the info returned from
            //the wait6 call.  It should have a stream operator function so
            //we can easily output the info.
            if(pid == -1)
            {
                //TODO: Need timeout.  This can be done when we
                //switch to async/kqueue
                std::ostringstream msg;
                msg << "Error waiting for makefs: " << strerror(errno);
                throw std::runtime_error(msg.str());
            }

            if(pid > 0)
            {
                return WEXITSTATUS(status);
            }
            break;
        }
        }

        //Shouldn't get here.
        throw std::logic_error("Unhandled case in makefs");
    }
}

int vessel::create_image(const fs_path &source, const fs_path &dest)
{
    if(!source.exists())
    {
        std::ostringstream msg;
        msg << "Image source does not exist: " << source.str();
        throw std::runtime_error(msg.str());
    }

    if(!source.is_dir() || !source.is_readable())
    {
        std::ostringstream msg;
        msg << "Source of image archive is not a readable directory: " << source.str();
        throw std::runtime_error(msg.str());
    }

    if(dest.exists())
    {
        std::ostringstream msg;
        msg << "Destination for archive already exists: " << dest.str();
        throw std::runtime_error(msg.str());
    }

    std::cerr << "dest: " << dest.str() << std::endl;
    int exit_code = run_makefs(source, dest);
    return exit_code;
}

void vessel::archive_image(const fs_path& image, const fs_path& archive_dir,
                         const compression_progress_callback& progress_cb)
{
    fs_path output_file{archive_dir};
    output_file += image.basename();
    output_file.append_extension("xz");
    xz_compress_image(image, output_file, progress_cb);

    return;
}
