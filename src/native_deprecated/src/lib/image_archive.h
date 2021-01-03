#ifndef IMAGE_ARCHIVE_H
#define IMAGE_ARCHIVE_H

#include <functional>
#include <sys/types.h>

namespace vessel
{

class fs_path;
/**
 * The term image_archive represents an image that can
 * be stored or retrieved from a registry.  image_archives are
 * somehow packaged so that they can be transported and stored
 * in an archive.  That package could theoretically be a
 * tarball, iso9660 image, compressed ufs image,
 * compressed zfs snapshot etc.
 */

struct compression_progress
{
    off_t file_size;
    ssize_t bytes_read;
    ssize_t bytes_written;
    size_t total_read;
    size_t total_compressed_written;
};

using compression_progress_callback =
    std::function<void(const compression_progress&)>;

int create_image(const fs_path& source, const fs_path& dest);

void archive_image(const fs_path& image, const fs_path& archive_dir,
                   const compression_progress_callback& progress_cb);

}
#endif // IMAGE_ARCHIVE_H
