
#ifndef SCM_GL_CORE_BUFFER_OBJECTS_FWD_H_INCLUDED
#define SCM_GL_CORE_BUFFER_OBJECTS_FWD_H_INCLUDED

#include <scm/core/pointer_types.h>

namespace scm {
namespace gl {

class buffer;
class vertex_format;
class vertex_array;

typedef shared_ptr<buffer>              buffer_ptr;
typedef shared_ptr<const buffer>        buffer_cptr;
typedef shared_ptr<vertex_format>       vertex_format_ptr;
typedef shared_ptr<const vertex_format> vertex_format_cptr;
typedef shared_ptr<vertex_array>        vertex_array_ptr;
typedef shared_ptr<const vertex_array>  vertex_array_cptr;

} // namespace gl
} // namespace scm

#endif // SCM_GL_CORE_BUFFER_OBJECTS_FWD_H_INCLUDED
