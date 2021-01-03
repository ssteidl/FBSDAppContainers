#ifndef APPC_FS_H
#define APPC_FS_H

#include "vessel_tcl.h"
#include <list>
#include <string>
#include <sys/stat.h>

namespace vessel
{

struct path_stat
{
    struct stat stat_buf;
    bool is_dir() const;

private:
    friend class fs_path;

    path_stat(const std::string& path);
    path_stat(const path_stat& other) = delete;
    path_stat(path_stat&& other) = delete;
    path_stat& operator=(const path_stat& other) = delete;
    path_stat& operator=(path_stat&& other) = delete;

};

class resource_fd
{
    int m_fd;

    void do_close();
    static int validate_fd(int fd);

    resource_fd(const resource_fd& other) = delete;
    resource_fd& operator=(const resource_fd& other) = delete;
public:

    resource_fd(int fd);
    resource_fd(resource_fd&& other);
    resource_fd& operator=(int fd);
    resource_fd& operator=(resource_fd&& other);
    operator int();
    ~resource_fd();
};

class fs_path
{
private:
    tcl_obj_raii m_path;

public:

    fs_path();
    fs_path(const std::string& path);
    fs_path(const fs_path& other);
    fs_path(fs_path&& other);
    fs_path& operator=(const fs_path& rhs);
    fs_path& operator=(fs_path&& rhs);


    bool exists() const;

    bool is_dir() const;

    bool is_readable() const;

    bool is_writable() const;

    bool is_executable() const;

    std::unique_ptr<path_stat> stat() const;

    bool operator==(const fs_path& rhs) const;

    fs_path& operator+=(const std::string& path_component);

    std::string basename() const;

    void append_extension(const std::string& extension);

    explicit operator bool() const;

    std::string str() const;

    fs_path find_dir(const std::string& dir_name) const;

    ~fs_path();
};

/**
 * @brief validate_directory validates that the path exists and is
 * a directory.  Throws exception if either test failes.
 */
void validate_directory(const fs_path& path);

}
#endif // APPC_FS_H
