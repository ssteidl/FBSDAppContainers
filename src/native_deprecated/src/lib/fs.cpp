#include <vessel_tcl.h>
#include <cassert>
#include <cstdlib>
#include "fs.h"
#include <iostream>
#include <libgen.h>
#include <sys/stat.h>
#include <sstream>
#include <unistd.h>
#include <vector>

using namespace vessel;

/**************path_stat****************************/


path_stat::path_stat(const std::string& path)
    : stat_buf()
{
    int error = ::stat(path.c_str(), &stat_buf);
    if(error)
    {
        std::ostringstream msg;
        msg << "Error stat'ing path '" << path << "': " << strerror(errno);
        throw std::runtime_error(msg.str());
    }
}

bool path_stat::is_dir() const
{
    return S_ISDIR(stat_buf.st_mode);
}

/**************resource-fd***************************/
void resource_fd::do_close()
{
    if(m_fd != -1)
    {
        (void)::close(m_fd);
    }
}

int resource_fd::validate_fd(int fd)
{
    if(fd < -1)
    {
        throw std::invalid_argument("Illegal fd provided to resource_fd");
    }

    return fd;
}

resource_fd::resource_fd(int fd)
    : m_fd(validate_fd(fd))
{}

resource_fd::resource_fd(resource_fd&& other)
{
    do_close();

    m_fd = validate_fd(other.m_fd);
    other.m_fd = -1;
}

resource_fd& resource_fd::operator=(int fd)
{
    do_close();

    m_fd = validate_fd(fd);

    return *this;
}

resource_fd& resource_fd::operator=(resource_fd&& other)
{
    do_close();

    m_fd = validate_fd(other.m_fd);

    return *this;
}

resource_fd::operator int()
{
    return m_fd;
}

resource_fd::~resource_fd()
{
    do_close();
}

/**************fs_path******************************/

namespace
{
    tcl_obj_raii create_path_obj(const std::string& path)
    {
        tcl_obj_raii obj = Tcl_NewStringObj(path.c_str(), path.size());
        interp_ptr interp = create_tcl_interp();
        int tcl_error = Tcl_FSConvertToPathType(interp.get(), obj);
        if(tcl_error)
        {
            std::ostringstream msg;
            msg << "Internal error creating path '" << path << "': " << Tcl_GetStringResult(interp.get());
            throw std::invalid_argument(msg.str());
        }

        return obj;
    }
}

fs_path::fs_path(const std::string& path)
    : m_path(create_path_obj(path))
{}

fs_path::fs_path()
    : fs_path(std::string())
{}

/*Copies the path.*/
fs_path::fs_path(const fs_path& other)
    : fs_path(Tcl_GetStringFromObj(other.m_path, nullptr))
{}

fs_path::fs_path(fs_path&& other)
    : m_path(std::move(other.m_path))
{}

fs_path& fs_path::operator=(const fs_path& other)
{
    if(this != &other)
    {
        m_path = other.m_path;
    }

    return *this;
}

fs_path& fs_path::operator=(fs_path&& other)
{
    if(this != &other)
    {
        m_path = std::move(other.m_path);
    }

    return *this;
}

bool fs_path::exists() const
{
    int result = ::access(str().c_str(), F_OK);
    return (result == 0);
}

bool fs_path::is_readable() const
{
    int result = ::access(str().c_str(), R_OK);
    return (result == 0);
}

bool fs_path::is_writable() const
{
    int result = ::access(str().c_str(), W_OK);
    return (result == 0);
}

bool fs_path::is_executable() const
{
    int result = ::access(str().c_str(), X_OK);
    return (result == 0);
}

std::unique_ptr<path_stat> fs_path::stat() const
{
    return std::unique_ptr<path_stat>(new path_stat(str()));
}

void fs_path::append_extension(const std::string& extension)
{
    int num_components = 0;
    tcl_obj_raii split_list = Tcl_FSSplitPath(m_path, &num_components);
    assert(num_components);

    std::string base = ::basename(str().c_str());

    tcl_obj_raii base_with_extension =
            Tcl_ObjPrintf("%s.%s", base.c_str(), extension.c_str());

    interp_ptr interp = create_tcl_interp();
    int tcl_error = Tcl_ListObjReplace(interp.get(), split_list, num_components - 1, 1, 1,
                                       &base_with_extension.obj);

    if(tcl_error)
    {
        std::ostringstream msg;
        msg << "Error adding extenstion '" << extension
            << "' to path '" << str() << ": "
            << Tcl_GetStringResult(interp.get());

        throw std::runtime_error(msg.str());
    }

    m_path = Tcl_FSJoinPath(split_list, -1);
}

bool fs_path::is_dir() const
{
    return path_stat(str()).is_dir();
}

bool fs_path::operator==(const fs_path& rhs) const
{
    if(this == &rhs)
    {
        return true;
    }

    return Tcl_FSEqualPaths(m_path, rhs.m_path);
}

fs_path& fs_path::operator+=(const std::string& path_component)
{
    tcl_obj_raii component_obj = Tcl_NewStringObj(path_component.c_str(),
                                                  path_component.size());

    m_path = Tcl_FSJoinToPath(m_path, 1, &component_obj.obj);
    return *this;
}

fs_path::operator bool() const
{
    int length = 0;
    (void)Tcl_GetStringFromObj(m_path, &length);
    return (length > 0);
}

std::string fs_path::basename() const
{
    return ::basename(str().c_str());
}

std::string fs_path::str() const
{
    return Tcl_GetStringFromObj(m_path, nullptr);
}

fs_path fs_path::find_dir(const std::string& dir_name) const
{
    if(!is_dir())
    {
        std::ostringstream msg;
        msg << "Attempted to find directory in path which is not a directory: search="
            << dir_name << "directory: " << str();
        throw std::logic_error(msg.str());
    }

    std::unique_ptr<Tcl_Interp, decltype(&Tcl_DeleteInterp)>
                    interp(Tcl_CreateInterp(), Tcl_DeleteInterp);

    tcl_obj_raii result_list = Tcl_NewListObj(0, nullptr);

    Tcl_GlobTypeData globtype;
    memset(&globtype, 0, sizeof(globtype));

    globtype.type = TCL_GLOB_TYPE_DIR;

    //Search for a folder with the image name in the vessel_image_dir
    int tcl_error = Tcl_FSMatchInDirectory(interp.get(), result_list, m_path,
                                            dir_name.c_str(), &globtype);

    if(tcl_error)
    {
        //log debug
        const char* result_str = Tcl_GetStringResult(interp.get());
        std::cerr << "Failed to find image '" << dir_name << "': " << result_str << std::endl;
        return fs_path();
    }

    int length = 0;
    tcl_error = Tcl_ListObjLength(interp.get(), result_list, &length);
    if(!tcl_error && length > 1)
    {
        std::ostringstream msg;
        msg << "Multiple images found for name: " << dir_name;
        throw std::runtime_error(msg.str());
    }

    if(length == 0)
    {
        return fs_path();
    }

    Tcl_Obj* image_path = nullptr;
    tcl_error = Tcl_ListObjIndex(interp.get(), result_list, 0, &image_path);
    assert(!tcl_error);
    assert(image_path);

    if(image_path)
    {
        return fs_path(Tcl_GetStringFromObj(image_path, nullptr));
    }
    else
    {
        return fs_path();
    }
}

fs_path::~fs_path()
{}

/*****************functions************************************/
void vessel::validate_directory(const fs_path& path)
{
    if(!path.exists())
    {
        std::cerr << path.str() << " does not exist" << std::endl;
        exit(1);
    }

    if(!path.is_dir())
    {
        std::cerr << path.str() << " is not a directory" << std::endl;
        exit(1);
    }
}
