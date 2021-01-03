#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "fs.h"

namespace vessel
{
    class environment
    {
        fs_path m_archive_dir;
        fs_path m_image_dir;
        fs_path m_container_dir;

        environment(environment&& other) = delete;
        environment& operator=(environment&& other) = delete;
    public:

        environment();

        fs_path find_image(const std::string& image_name) const;
        fs_path find_container(const std::string& container_name) const;
        fs_path archive_dir() const;
        fs_path image_dir() const;
    };

}
#endif // ENVIRONMENT_H
