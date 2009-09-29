
#ifndef SCM_IO_FILE_H_INCLUDED
#define SCM_IO_FILE_H_INCLUDED

#include <string>

#include <boost/noncopyable.hpp>

#include <scm/core/numeric_types.h>
#include <scm/core/pointer_types.h>

#include <scm/core/platform/platform.h>
#include <scm/core/utilities/platform_warning_disable.h>

namespace scm {
namespace io {
namespace detail {

const scm::uint32   default_io_block_size           = 32768u;
const scm::uint32   default_asynchronous_requests   = 8u;

} // namespace detail

class file_core;

class __scm_export(core) file : boost::noncopyable
{
public:
    typedef char                char_type;
    typedef scm::int64          size_type;

public:
    file();
    virtual ~file();

    void                        swap(file& rhs);

    // functionality depending on file_core
    bool                        open(const std::string&       file_path,
                                     std::ios_base::openmode  open_mode                 = std::ios_base::in | std::ios_base::out,
                                     bool                     disable_system_cache      = true,
                                     scm::uint32              io_block_size             = detail::default_io_block_size,
                                     scm::uint32              async_io_requests         = detail::default_asynchronous_requests);
    bool                        is_open() const;
    void                        close();
    size_type                   read(char_type*const output_buffer,
                                     size_type       num_bytes_to_read);
    size_type                   write(const char_type*const input_buffer,
                                      size_type             num_bytes_to_write);
    size_type                   seek(size_type                  off,
                                     std::ios_base::seek_dir    way);
	size_type			        set_end_of_file();

    size_type                   optimal_buffer_size() const;

    size_type                   size() const;
    const std::string&          file_path() const;

private:
    scm::scoped_ptr<file_core>  _file_core;

}; // class file

} // namespace io
} // namespace scm

namespace std {

inline void
swap(scm::io::file& lhs,
    scm::io::file& rhs)
{
    lhs.swap(rhs);
}

} // namespace std

#include <scm/core/utilities/platform_warning_disable.h>

#endif // SCM_IO_FILE_H_INCLUDED