
#ifndef SCM_GL_UTIL_VOLUME_READER_BLOCKED_H_INCLUDED
#define SCM_GL_UTIL_VOLUME_READER_BLOCKED_H_INCLUDED

#include <scm/gl_util/volume/volume_reader.h>

#include <scm/core/platform/platform.h>
#include <scm/core/utilities/platform_warning_disable.h>

namespace scm {
namespace gl {

class __scm_export(gl_util) volume_reader_blocked : public volume_reader
{
public:

public:
    volume_reader_blocked(const std::string& file_path,
                                bool         file_unbuffered = false);
    virtual ~volume_reader_blocked();

    bool                read(const scm::math::vec3ui& o,
                             const scm::math::vec3ui& s,
                                   void*              d);

protected:
    int64               _data_start_offset;

}; // struct volume_reader_blocked

} // namespace gl
} // namespace scm

#include <scm/core/utilities/platform_warning_enable.h>

#endif // #define SCM_GL_UTIL_VOLUME_READER_BLOCKED_H_INCLUDED

