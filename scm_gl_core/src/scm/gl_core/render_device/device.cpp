
#include "device.h"

#include <exception>
#include <stdexcept>
#include <sstream>

#include <boost/bind.hpp>
#include <boost/filesystem.hpp>

#include <scm/core/io/tools.h>
#include <scm/core/utilities/foreach.h>

#include <scm/gl_core/config.h>
#include <scm/gl_core/log.h>
#include <scm/gl_core/frame_buffer_objects.h>
#include <scm/gl_core/query_objects.h>
#include <scm/gl_core/render_device/context.h>
#include <scm/gl_core/render_device/opengl/gl3_core.h>
#include <scm/gl_core/render_device/opengl/util/assert.h>
#include <scm/gl_core/render_device/opengl/util/error_helper.h>
#include <scm/gl_core/shader_objects/program.h>
#include <scm/gl_core/shader_objects/shader.h>
#include <scm/gl_core/buffer_objects/vertex_array.h>
#include <scm/gl_core/buffer_objects/vertex_format.h>
#include <scm/gl_core/state_objects/depth_stencil_state.h>
#include <scm/gl_core/state_objects/rasterizer_state.h>
#include <scm/gl_core/state_objects/sampler_state.h>
#include <scm/gl_core/texture_objects.h>

namespace scm {
namespace gl {

render_device::render_device()
{
    _opengl3_api_core.reset(new opengl::gl3_core());

    if (!_opengl3_api_core->initialize()) {
        std::ostringstream s;
        s << "render_device::render_device(): error initializing gl core.";
        glerr() << log::fatal << s.str() << log::end;
        throw(std::runtime_error(s.str()));
    }
    unsigned req_version_major = SCM_GL_CORE_BASE_OPENGL_VERSION / 100;
    unsigned req_version_minor = (SCM_GL_CORE_BASE_OPENGL_VERSION - req_version_major * 100) / 10;

    if (!_opengl3_api_core->version_supported(req_version_major, req_version_minor)) {
        std::ostringstream s;
        s << "render_device::render_device(): error initializing gl core "
          << "(at least OpenGL "
          << req_version_major << "." << req_version_minor
          << " requiered, encountered version "
          << _opengl3_api_core->context_information()._version_major << "."
          << _opengl3_api_core->context_information()._version_minor << ").";
        glerr() << log::fatal << s.str() << log::end;
        throw(std::runtime_error(s.str()));
    }
    else {
        glout() << log::info << "render_device::render_device(): "
                << "scm_gl_core OpenGL "
                << req_version_major << "." << req_version_minor
                << " support enabled on "
                << _opengl3_api_core->context_information()._version_major << "."
                << _opengl3_api_core->context_information()._version_minor
                << " context." << log::end;
    }

#ifdef SCM_GL_CORE_USE_DIRECT_STATE_ACCESS
    if (!_opengl3_api_core->is_supported("GL_EXT_direct_state_access")) {
        std::ostringstream s;
        s << "render_device::render_device(): error initializing gl core "
          << "(missing requiered extension GL_EXT_direct_state_access).";
        glerr() << log::fatal << s.str() << log::end;
        throw(std::runtime_error(s.str()));
    }
#endif

    init_capabilities();

    // setup main rendering context
    try {
        _main_context.reset(new render_context(*this));
    }
    catch (const std::exception& e) {
        std::ostringstream s;
        s << "render_device::render_device(): error creating main_context (evoking error: "
          << e.what()
          << ").";
        glerr() << log::fatal << s.str() << log::end;
        throw(std::runtime_error(s.str()));
    }
}

render_device::~render_device()
{
    _main_context.reset();

    assert(0 == _registered_resources.size());
}

const opengl::gl3_core&
render_device::opengl3_api() const
{
    return (*_opengl3_api_core);
}

render_context_ptr
render_device::main_context() const
{
    return (_main_context);
}

render_context_ptr
render_device::create_context()
{
    return (render_context_ptr(new render_context(*this)));
}

const render_device::device_capabilities&
render_device::capabilities() const
{
    return (_capabilities);
}

void
render_device::init_capabilities()
{
    const opengl::gl3_core& glcore = opengl3_api();

    glcore.glGetIntegerv(GL_MAX_VERTEX_ATTRIBS,           &_capabilities._max_vertex_attributes);
    glcore.glGetIntegerv(GL_MAX_DRAW_BUFFERS,             &_capabilities._max_draw_buffers);
    glcore.glGetIntegerv(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS, &_capabilities._max_dual_source_draw_buffers);

    assert(_capabilities._max_vertex_attributes > 0);
    assert(_capabilities._max_draw_buffers > 0);
    assert(_capabilities._max_dual_source_draw_buffers > 0);

    glcore.glGetIntegerv(GL_MAX_TEXTURE_SIZE,             &_capabilities._max_texture_size);
    glcore.glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE,          &_capabilities._max_texture_3d_size);
    glcore.glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS,     &_capabilities._max_array_texture_layers);
    glcore.glGetIntegerv(GL_MAX_SAMPLES,                  &_capabilities._max_samples);
    glcore.glGetIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES,    &_capabilities._max_depth_texture_samples);
    glcore.glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES,    &_capabilities._max_color_texture_samples);
    glcore.glGetIntegerv(GL_MAX_INTEGER_SAMPLES,          &_capabilities._max_integer_samples);
    glcore.glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,      &_capabilities._max_texture_image_units);
    glcore.glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE,      &_capabilities._max_texture_buffer_size);
    glcore.glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS,        &_capabilities._max_frame_buffer_color_attachments);

    assert(_capabilities._max_texture_size > 0);
    assert(_capabilities._max_texture_3d_size > 0);
    assert(_capabilities._max_array_texture_layers > 0);
    assert(_capabilities._max_samples > 0);
    assert(_capabilities._max_depth_texture_samples > 0);
    assert(_capabilities._max_color_texture_samples > 0);
    assert(_capabilities._max_integer_samples > 0);
    assert(_capabilities._max_texture_image_units > 0);
    assert(_capabilities._max_texture_buffer_size > 0);
    assert(_capabilities._max_frame_buffer_color_attachments > 0);

    glcore.glGetIntegerv(GL_MAX_VERTEX_UNIFORM_BLOCKS,        &_capabilities._max_vertex_uniform_blocks);
    glcore.glGetIntegerv(GL_MAX_GEOMETRY_UNIFORM_BLOCKS,      &_capabilities._max_geometry_uniform_blocks);
    glcore.glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS,      &_capabilities._max_fragment_uniform_blocks);
    glcore.glGetIntegerv(GL_MAX_COMBINED_UNIFORM_BLOCKS,      &_capabilities._max_combined_uniform_blocks);
    glcore.glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS,      &_capabilities._max_uniform_buffer_bindings);
    glcore.glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,  &_capabilities._uniform_buffer_offset_alignment);

    assert(_capabilities._max_vertex_uniform_blocks > 0);
    assert(_capabilities._max_geometry_uniform_blocks > 0);
    assert(_capabilities._max_fragment_uniform_blocks > 0);
    assert(_capabilities._max_combined_uniform_blocks > 0);
    assert(_capabilities._max_uniform_buffer_bindings > 0);
    assert(_capabilities._uniform_buffer_offset_alignment > 0);

    if (SCM_GL_CORE_BASE_OPENGL_VERSION >= SCM_GL_CORE_OPENGL_VERSION_410) {
        glcore.glGetIntegerv(GL_MAX_VIEWPORTS,                    &_capabilities._max_viewports);
    }
    else {
        _capabilities._max_viewports = 1;
    }

    assert(_capabilities._max_viewports > 0);
}

// buffer api /////////////////////////////////////////////////////////////////////////////////////
buffer_ptr
render_device::create_buffer(const buffer_desc& in_buffer_desc,
                             const void*        in_initial_data)
{
    buffer_ptr new_buffer(new buffer(*this, in_buffer_desc, in_initial_data),
                          boost::bind(&render_device::release_resource, this, _1));
    if (new_buffer->fail()) {
        if (new_buffer->bad()) {
            glerr() << log::error << "render_device::create_buffer(): unable to create buffer object ("
                    << new_buffer->state().state_string() << ")." << log::end;
        }
        else {
            glerr() << log::error << "render_device::create_buffer(): unable to allocate buffer ("
                    << new_buffer->state().state_string() << ")." << log::end;
        }
        return (buffer_ptr());
    }
    else {
        register_resource(new_buffer.get());
        return (new_buffer);
    }
}

buffer_ptr
render_device::create_buffer(buffer_binding in_binding,
                             buffer_usage   in_usage,
                             scm::size_t    in_size,
                             const void*    in_initial_data)
{
    return (create_buffer(buffer_desc(in_binding, in_usage, in_size), in_initial_data));
}

bool
render_device::resize_buffer(const buffer_ptr& in_buffer, scm::size_t in_size)
{
    buffer_desc desc = in_buffer->descriptor();
    desc._size = in_size;
    if (!in_buffer->buffer_data(*this, desc, 0)) {
        glerr() << log::error << "render_device::resize_buffer(): unable to reallocate buffer ("
                << in_buffer->state().state_string() << ")." << log::end;
        return (false);
    }
    else {
        return (true);
    }
}

vertex_array_ptr
render_device::create_vertex_array(const vertex_format& in_vert_fmt,
                                   const buffer_array&  in_attrib_buffers,
                                   const program_ptr&   in_program)
{
    vertex_array_ptr new_array(new vertex_array(*this, in_vert_fmt, in_attrib_buffers, in_program));
    if (new_array->fail()) {
        if (new_array->bad()) {
            glerr() << log::error << "render_device::create_vertex_array(): unable to create vertex array object ("
                    << new_array->state().state_string() << ")." << log::end;
        }
        else {
            glerr() << log::error << "render_device::create_vertex_array(): unable to initialize vertex array object ("
                    << new_array->state().state_string() << ")." << log::end;
        }
        return (vertex_array_ptr());
    }
    return (new_array);
}

// shader api /////////////////////////////////////////////////////////////////////////////////////
void
render_device::add_include_path(const std::string& in_path,
                                const std::string& in_file_extensions,
                                bool               in_scan_subdirectories)
{
}

void
render_device::add_include_paths(const std::vector<std::string>& in_paths,
                                 const std::string&              in_file_extensions,
                                 bool                            in_scan_subdirectories)
{
}

bool
render_device::add_include_string(const std::string& in_path,
                                  const std::string& in_source_string)
{
    const opengl::gl3_core& glcore = opengl3_api();
    util::gl_error          glerror(glcore);

    if (!glcore.extension_ARB_shading_language_include) {
        glout() << log::warning << "render_device::add_include_string(): "
                << "shader includes not supported (GL_ARB_shading_language_include unsupported), ignoring include string." << log::end;
        return false;
    }

    if (in_path[0] != '/') {
        glerr() << log::error << "render_device::add_include_string(): "
                << "<error> path not starting with '/'." << log::end;
        return false;
    }

    glcore.glNamedStringARB(GL_SHADER_INCLUDE_ARB,
                            static_cast<int>(in_path.length()),          in_path.c_str(),
                            static_cast<int>(in_source_string.length()), in_source_string.c_str());

    if (glerror) {
        switch (glerror.to_object_state()) {
        case object_state::OS_ERROR_INVALID_VALUE:
            glerr() << log::error << "render_device::add_include_string(): "
                    << "error creating named include string (path or source string empty or path not starting with '/'." << log::end;
            return false;
            break;
        default:
            glerr() << log::error << "render_device::add_include_string(): "
                    << "error creating named include string (an unknown error occured)" << log::end;
            return false;
        }
    }

    size_t      parent_path_end = in_path.find_last_of('/');
    std::string parent_path     = in_path.substr(0, parent_path_end);

    if (!parent_path.empty()) {
        _default_include_paths.insert(parent_path);
    }

    gl_assert(glcore, leaving render_device::add_include_string());

    return true;
}

void
render_device::add_macro_define(const std::string& in_name,
                                const std::string& in_value)
{
    _default_macro_defines[in_name] = shader_macro(in_name, in_value);
}

void
render_device::add_macro_define(const shader_macro& in_macro)
{
    _default_macro_defines[in_macro._name] = in_macro;
}

void
render_device::add_macro_defines(const shader_macro_array& in_macros)
{
    foreach(const shader_macro& m, in_macros.macros()) {
        add_macro_define(m);
    }
}

shader_ptr
render_device::create_shader(shader_stage       in_stage,
                             const std::string& in_source,
                             const std::string& in_source_name)
{
    return create_shader(in_stage, in_source, shader_macro_array(), shader_include_path_list(), in_source_name);
}

shader_ptr
render_device::create_shader(shader_stage              in_stage,
                             const std::string&        in_source,
                             const shader_macro_array& in_macros,
                             const std::string&        in_source_name)
{
    return create_shader(in_stage, in_source, in_macros, shader_include_path_list(), in_source_name);
}

shader_ptr
render_device::create_shader(shader_stage                    in_stage,
                             const std::string&              in_source,
                             const shader_include_path_list& in_inc_paths,
                             const std::string&              in_source_name)
{
    return create_shader(in_stage, in_source, shader_macro_array(), in_inc_paths, in_source_name);
}

shader_ptr
render_device::create_shader(shader_stage                    in_stage,
                             const std::string&              in_source,
                             const shader_macro_array&       in_macros,
                             const shader_include_path_list& in_inc_paths,
                             const std::string&              in_source_name)
{
    // combine macro definitions
    shader_macro_array  macro_array(in_macros);

    shader_macro_map::const_iterator mb = _default_macro_defines.begin();
    shader_macro_map::const_iterator me = _default_macro_defines.end();

    for(; mb != me; ++mb) {
        macro_array(mb->second._name, mb->second._value);
    }

    // combine shader include paths
    shader_include_path_list   include_paths(in_inc_paths);

    shader_include_path_set::const_iterator ipb = _default_include_paths.begin();
    shader_include_path_set::const_iterator ipe = _default_include_paths.end();

    for(; ipb != ipe; ++ipb) {
        include_paths.push_back(*ipb);
    }

    shader_ptr new_shader(new shader(*this,
                                     in_stage,
                                     in_source,
                                     in_source_name,
                                     macro_array,
                                     include_paths));
    if (new_shader->fail()) {
        if (new_shader->bad()) {
            glerr() << "render_device::create_shader(): unable to create shader object ("
                    << "name: " << in_source_name << ", "
                    << "stage: " << shader_stage_string(in_stage) << ", "
                    << new_shader->state().state_string() << ")." << log::end;
        }
        else {
            glerr() << "render_device::create_shader(): unable to compile shader ("
                    << "name: " << in_source_name << ", "
                    << "stage: " << shader_stage_string(in_stage) << ", "
                    << new_shader->state().state_string() << "):" << log::nline
                    << new_shader->info_log() << log::end;
        }
        return (shader_ptr());
    }
    else {
        if (!new_shader->info_log().empty()) {
            glout() << log::info << "render_device::create_shader(): compiler info ("
                    << "name: " << in_source_name << ", "
                    << "stage: " << shader_stage_string(in_stage)
                    << ")" << log::nline
                    << new_shader->info_log() << log::end;
        }
        return (new_shader);
    }
}

shader_ptr
render_device::create_shader_from_file(shader_stage       in_stage,
                                       const std::string& in_file_name)
{
    return create_shader_from_file(in_stage, in_file_name, shader_macro_array(), shader_include_path_list());
}

shader_ptr
render_device::create_shader_from_file(shader_stage              in_stage,
                                       const std::string&        in_file_name,
                                       const shader_macro_array& in_macros)
{
    return create_shader_from_file(in_stage, in_file_name, in_macros, shader_include_path_list());
}

shader_ptr
render_device::create_shader_from_file(shader_stage                    in_stage,
                                       const std::string&              in_file_name,
                                       const shader_include_path_list& in_inc_paths)
{
    return create_shader_from_file(in_stage, in_file_name, shader_macro_array(), in_inc_paths);
}

shader_ptr
render_device::create_shader_from_file(shader_stage                    in_stage,
                                       const std::string&              in_file_name,
                                       const shader_macro_array&       in_macros,
                                       const shader_include_path_list& in_inc_paths)
{
    namespace bfs = boost::filesystem;
    bfs::path       file_path(in_file_name, bfs::native);
    std::string     source_string;

    if (   !io::read_text_file(in_file_name, source_string)) {
        glerr() << "render_device::create_shader_from_file(): error reading shader file " << in_file_name << log::end;
        return (shader_ptr());
    }

    return create_shader(in_stage, source_string, in_macros, in_inc_paths, file_path.filename());
}

program_ptr
render_device::create_program(const shader_list& in_shaders)
{
    program_ptr new_program(new program(*this, in_shaders));
    if (new_program->fail()) {
        if (new_program->bad()) {
            glerr() << "render_device::create_program(): unable to create shader object ("
                    << new_program->state().state_string() << ")." << log::end;
        }
        else {
            glerr() << "render_device::create_program(): error during link operation ("
                    << new_program->state().state_string() << "):" << log::nline
                    << new_program->info_log() << log::end;
        }
        return (program_ptr());
    }
    else {
        if (!new_program->info_log().empty()) {
            glout() << log::info << "render_device::create_program(): linker info" << log::nline
                    << new_program->info_log() << log::end;
        }
        return (new_program);
    }
}

// texture api ////////////////////////////////////////////////////////////////////////////////////
texture_1d_ptr
render_device::create_texture_1d(const texture_1d_desc&   in_desc)
{
    texture_1d_ptr  new_tex(new texture_1d(*this, in_desc));
    if (new_tex->fail()) {
        if (new_tex->bad()) {
            glerr() << log::error << "render_device::create_texture_1d(): unable to create texture object ("
                    << new_tex->state().state_string() << ")." << log::end;
        }
        else {
            glerr() << log::error << "render_device::create_texture_1d(): unable to allocate texture image data ("
                    << new_tex->state().state_string() << ")." << log::end;
        }
        return (texture_1d_ptr());
    }
    else {
        return (new_tex);
    }
}

texture_1d_ptr
render_device::create_texture_1d(const texture_1d_desc&    in_desc,
                                 const data_format         in_initial_data_format,
                                 const std::vector<void*>& in_initial_mip_level_data)
{
    texture_1d_ptr  new_tex(new texture_1d(*this, in_desc, in_initial_data_format, in_initial_mip_level_data));
    if (new_tex->fail()) {
        if (new_tex->bad()) {
            glerr() << log::error << "render_device::create_texture_1d(): unable to create texture object ("
                    << new_tex->state().state_string() << ")." << log::end;
        }
        else {
            glerr() << log::error << "render_device::create_texture_1d(): unable to allocate texture image data ("
                    << new_tex->state().state_string() << ")." << log::end;
        }
        return (texture_1d_ptr());
    }
    else {
        return (new_tex);
    }
}

texture_1d_ptr
render_device::create_texture_1d(const unsigned      in_size,
                                 const data_format   in_format,
                                 const unsigned      in_mip_levels,
                                 const unsigned      in_array_layers)
{
    return (create_texture_1d(texture_1d_desc(in_size, in_format, in_mip_levels, in_array_layers)));
}

texture_1d_ptr
render_device::create_texture_1d(const unsigned            in_size,
                                 const data_format         in_format,
                                 const unsigned            in_mip_levels,
                                 const unsigned            in_array_layers,
                                 const data_format         in_initial_data_format,
                                 const std::vector<void*>& in_initial_mip_level_data)
{
    return (create_texture_1d(texture_1d_desc(in_size, in_format, in_mip_levels, in_array_layers),
                              in_initial_data_format,
                              in_initial_mip_level_data));
}

texture_2d_ptr
render_device::create_texture_2d(const texture_2d_desc&   in_desc)
{
    texture_2d_ptr  new_tex(new texture_2d(*this, in_desc));
    if (new_tex->fail()) {
        if (new_tex->bad()) {
            glerr() << log::error << "render_device::create_texture_2d(): unable to create texture object ("
                    << new_tex->state().state_string() << ")." << log::end;
        }
        else {
            glerr() << log::error << "render_device::create_texture_2d(): unable to allocate texture image data ("
                    << new_tex->state().state_string() << ")." << log::end;
        }
        return (texture_2d_ptr());
    }
    else {
        return (new_tex);
    }
}

texture_2d_ptr
render_device::create_texture_2d(const texture_2d_desc&    in_desc,
                                 const data_format         in_initial_data_format,
                                 const std::vector<void*>& in_initial_mip_level_data)
{
    texture_2d_ptr  new_tex(new texture_2d(*this, in_desc, in_initial_data_format, in_initial_mip_level_data));
    if (new_tex->fail()) {
        if (new_tex->bad()) {
            glerr() << log::error << "render_device::create_texture_2d(): unable to create texture object ("
                    << new_tex->state().state_string() << ")." << log::end;
        }
        else {
            glerr() << log::error << "render_device::create_texture_2d(): unable to allocate texture image data ("
                    << new_tex->state().state_string() << ")." << log::end;
        }
        return (texture_2d_ptr());
    }
    else {
        return (new_tex);
    }
}

texture_2d_ptr
render_device::create_texture_2d(const math::vec2ui& in_size,
                                 const data_format   in_format,
                                 const unsigned      in_mip_levels,
                                 const unsigned      in_array_layers,
                                 const unsigned      in_samples)
{
    return (create_texture_2d(texture_2d_desc(in_size, in_format, in_mip_levels, in_array_layers, in_samples)));
}

texture_2d_ptr
render_device::create_texture_2d(const math::vec2ui&       in_size,
                                 const data_format         in_format,
                                 const unsigned            in_mip_levels,
                                 const unsigned            in_array_layers,
                                 const unsigned            in_samples,
                                 const data_format         in_initial_data_format,
                                 const std::vector<void*>& in_initial_mip_level_data)
{
    return (create_texture_2d(texture_2d_desc(in_size, in_format, in_mip_levels, in_array_layers, in_samples),
                              in_initial_data_format,
                              in_initial_mip_level_data));
}

texture_3d_ptr
render_device::create_texture_3d(const texture_3d_desc&   in_desc)
{
    texture_3d_ptr  new_tex(new texture_3d(*this, in_desc));
    if (new_tex->fail()) {
        if (new_tex->bad()) {
            glerr() << log::error << "render_device::create_texture_3d(): unable to create texture object ("
                    << new_tex->state().state_string() << ")." << log::end;
        }
        else {
            glerr() << log::error << "render_device::create_texture_3d(): unable to allocate texture image data ("
                    << new_tex->state().state_string() << ")." << log::end;
        }
        return (texture_3d_ptr());
    }
    else {
        return (new_tex);
    }
}

texture_3d_ptr
render_device::create_texture_3d(const texture_3d_desc&    in_desc,
                                 const data_format         in_initial_data_format,
                                 const std::vector<void*>& in_initial_mip_level_data)
{
    texture_3d_ptr  new_tex(new texture_3d(*this, in_desc, in_initial_data_format, in_initial_mip_level_data));
    if (new_tex->fail()) {
        if (new_tex->bad()) {
            glerr() << log::error << "render_device::create_texture_3d(): unable to create texture object ("
                    << new_tex->state().state_string() << ")." << log::end;
        }
        else {
            glerr() << log::error << "render_device::create_texture_3d(): unable to allocate texture image data ("
                    << new_tex->state().state_string() << ")." << log::end;
        }
        return (texture_3d_ptr());
    }
    else {
        return (new_tex);
    }
}

texture_3d_ptr
render_device::create_texture_3d(const math::vec3ui& in_size,
                                 const data_format   in_format,
                                 const unsigned      in_mip_levels)
{
    return (create_texture_3d(texture_3d_desc(in_size, in_format, in_mip_levels)));
}

texture_3d_ptr
render_device::create_texture_3d(const math::vec3ui&       in_size,
                                 const data_format         in_format,
                                 const unsigned            in_mip_levels,
                                 const data_format         in_initial_data_format,
                                 const std::vector<void*>& in_initial_mip_level_data)
{
    return (create_texture_3d(texture_3d_desc(in_size, in_format, in_mip_levels),
                              in_initial_data_format,
                              in_initial_mip_level_data));
}

texture_buffer_ptr
render_device::create_texture_buffer(const texture_buffer_desc& in_desc)
{
    texture_buffer_ptr  new_tex(new texture_buffer(*this, in_desc));
    if (new_tex->fail()) {
        if (new_tex->bad()) {
            glerr() << log::error << "render_device::create_texture_buffer(): unable to create texture buffer object ("
                    << new_tex->state().state_string() << ")." << log::end;
        }
        else {
            glerr() << log::error << "render_device::create_texture_buffer(): unable to allocate or attach texture buffer data ("
                    << new_tex->state().state_string() << ")." << log::end;
        }
        return texture_buffer_ptr();
    }
    else {
        return new_tex;
    }
}

texture_buffer_ptr
render_device::create_texture_buffer(const data_format   in_format,
                                     const buffer_ptr&   in_buffer)
{
    return create_texture_buffer(texture_buffer_desc(in_format, in_buffer));
}

texture_buffer_ptr
render_device::create_texture_buffer(const data_format   in_format,
                                     buffer_usage        in_buffer_usage,
                                     scm::size_t         in_buffer_size,
                                     const void*         in_buffer_initial_data)
{
    buffer_ptr  tex_buffer = create_buffer(BIND_TEXTURE_BUFFER, in_buffer_usage, in_buffer_size, in_buffer_initial_data);
    if (!tex_buffer) {
        glerr() << log::error << "render_device::create_texture_buffer(): unable to create texture buffer data buffer." << log::end;
        return texture_buffer_ptr();
    }
    return create_texture_buffer(in_format, tex_buffer);
}

sampler_state_ptr
render_device::create_sampler_state(const sampler_state_desc& in_desc)
{
    sampler_state_ptr  new_sstate(new sampler_state(*this, in_desc));
    if (new_sstate->fail()) {
        if (new_sstate->bad()) {
            glerr() << log::error << "render_device::create_sampler_state(): unable to create sampler state object ("
                    << new_sstate->state().state_string() << ")." << log::end;
        }
        return (sampler_state_ptr());
    }
    else {
        return (new_sstate);
    }
}

sampler_state_ptr
render_device::create_sampler_state(texture_filter_mode  in_filter,
                                    texture_wrap_mode    in_wrap,
                                    unsigned             in_max_anisotropy,
                                    float                in_min_lod,
                                    float                in_max_lod,
                                    float                in_lod_bias,
                                    compare_func         in_compare_func,
                                    texture_compare_mode in_compare_mode)
{
    return (create_sampler_state(sampler_state_desc(in_filter, in_wrap, in_wrap, in_wrap,
        in_max_anisotropy, in_min_lod, in_max_lod, in_lod_bias, in_compare_func, in_compare_mode)));
}

sampler_state_ptr
render_device::create_sampler_state(texture_filter_mode  in_filter,
                                    texture_wrap_mode    in_wrap_s,
                                    texture_wrap_mode    in_wrap_t,
                                    texture_wrap_mode    in_wrap_r,
                                    unsigned             in_max_anisotropy,
                                    float                in_min_lod,
                                    float                in_max_lod,
                                    float                in_lod_bias,
                                    compare_func         in_compare_func,
                                    texture_compare_mode in_compare_mode)
{
    return (create_sampler_state(sampler_state_desc(in_filter, in_wrap_s, in_wrap_t, in_wrap_r,
        in_max_anisotropy, in_min_lod, in_max_lod, in_lod_bias, in_compare_func, in_compare_mode)));
}

// frame buffer api ///////////////////////////////////////////////////////////////////////////////
render_buffer_ptr
render_device::create_render_buffer(const render_buffer_desc& in_desc)
{
    render_buffer_ptr  new_rb(new render_buffer(*this, in_desc));
    if (new_rb->fail()) {
        if (new_rb->bad()) {
            glerr() << log::error << "render_device::create_render_buffer(): unable to create render buffer object ("
                    << new_rb->state().state_string() << ")." << log::end;
        }
        return (render_buffer_ptr());
    }
    else {
        return (new_rb);
    }
}

render_buffer_ptr
render_device::create_render_buffer(const math::vec2ui& in_size,
                                    const data_format   in_format,
                                    const unsigned      in_samples)
{
    return (create_render_buffer(render_buffer_desc(in_size, in_format, in_samples)));
}

frame_buffer_ptr
render_device::create_frame_buffer()
{
    frame_buffer_ptr  new_rb(new frame_buffer(*this));
    if (new_rb->fail()) {
        if (new_rb->bad()) {
            glerr() << log::error << "render_device::create_render_buffer(): unable to create frame buffer object ("
                    << new_rb->state().state_string() << ")." << log::end;
        }
        return (frame_buffer_ptr());
    }
    else {
        return (new_rb);
    }
}

depth_stencil_state_ptr
render_device::create_depth_stencil_state(const depth_stencil_state_desc& in_desc)
{
    depth_stencil_state_ptr new_ds_state(new depth_stencil_state(*this, in_desc));
    return (new_ds_state);
}

depth_stencil_state_ptr
render_device::create_depth_stencil_state(bool in_depth_test, bool in_depth_mask, compare_func in_depth_func,
                                          bool in_stencil_test, unsigned in_stencil_rmask, unsigned in_stencil_wmask,
                                          stencil_ops in_stencil_ops)
{
    return (create_depth_stencil_state(depth_stencil_state_desc(in_depth_test, in_depth_mask, in_depth_func,
                                                                in_stencil_test, in_stencil_rmask, in_stencil_wmask,
                                                                in_stencil_ops)));
}

depth_stencil_state_ptr
render_device::create_depth_stencil_state(bool in_depth_test, bool in_depth_mask, compare_func in_depth_func,
                                          bool in_stencil_test, unsigned in_stencil_rmask, unsigned in_stencil_wmask,
                                          stencil_ops in_stencil_front_ops, stencil_ops in_stencil_back_ops)
{
    return (create_depth_stencil_state(depth_stencil_state_desc(in_depth_test, in_depth_mask, in_depth_func,
                                                                in_stencil_test, in_stencil_rmask, in_stencil_wmask,
                                                                in_stencil_front_ops, in_stencil_front_ops)));
}

rasterizer_state_ptr
render_device::create_rasterizer_state(const rasterizer_state_desc& in_desc)
{
    rasterizer_state_ptr new_r_state(new rasterizer_state(*this, in_desc));
    return (new_r_state);
}

rasterizer_state_ptr
render_device::create_rasterizer_state(fill_mode in_fmode, cull_mode in_cmode, polygon_orientation in_fface,
                                       bool in_msample, bool in_sctest, bool in_smlines, const point_raster_state& in_point_state)
{
    return (create_rasterizer_state(rasterizer_state_desc(in_fmode, in_cmode, in_fface,
                                                          in_msample, in_sctest, in_smlines, in_point_state)));
}

blend_state_ptr
render_device::create_blend_state(const blend_state_desc& in_desc)
{
    blend_state_ptr new_bl_state(new blend_state(*this, in_desc));
    return (new_bl_state);
}

blend_state_ptr
render_device::create_blend_state(bool in_enabled,
                                  blend_func in_src_rgb_func,   blend_func in_dst_rgb_func,
                                  blend_func in_src_alpha_func, blend_func in_dst_alpha_func,
                                  blend_equation  in_rgb_equation, blend_equation in_alpha_equation,
                                  unsigned in_write_mask, bool in_alpha_to_coverage)
{
    return (create_blend_state(blend_state_desc(blend_ops(in_enabled,
                                                          in_src_rgb_func,   in_dst_rgb_func,
                                                          in_src_alpha_func, in_dst_alpha_func,
                                                          in_rgb_equation,   in_alpha_equation, in_write_mask),
                                                in_alpha_to_coverage)));
}

blend_state_ptr
render_device::create_blend_state(const blend_ops_array& in_blend_ops, bool in_alpha_to_coverage)
{
    return (create_blend_state(blend_state_desc(in_blend_ops, in_alpha_to_coverage)));
}

// query api //////////////////////////////////////////////////////////////////////////////////////
timer_query_ptr
render_device::create_timer_query()
{
    timer_query_ptr  new_tq(new timer_query(*this));
    if (new_tq->fail()) {
        if (new_tq->bad()) {
            glerr() << log::error << "render_device::create_timer_query(): unable to create timer query object ("
                    << new_tq->state().state_string() << ")." << log::end;
        }
        return (timer_query_ptr());
    }
    else {
        return (new_tq);
    }
}

void
render_device::print_device_informations(std::ostream& os) const
{
    os << "OpenGL render device" << std::endl;
    os << *_opengl3_api_core;
}
const std::string
render_device::device_vendor() const
{
    return _opengl3_api_core->context_information()._vendor;
}

const std::string
render_device::device_renderer() const
{
    return _opengl3_api_core->context_information()._renderer;
}

const std::string
render_device::device_shader_compiler() const
{
    return _opengl3_api_core->context_information()._glsl_version_info;
}

const std::string
render_device::device_context_version() const
{
    std::stringstream s;
    s << _opengl3_api_core->context_information()._version_major << "." 
      << _opengl3_api_core->context_information()._version_minor << "." 
      << _opengl3_api_core->context_information()._version_release;
    if (!_opengl3_api_core->context_information()._version_info.empty())
         s << " " << _opengl3_api_core->context_information()._version_info;
    if (!_opengl3_api_core->context_information()._profile_string.empty())
         s << " " << _opengl3_api_core->context_information()._profile_string;

    return s.str();
}

void
render_device::register_resource(render_device_resource* res_ptr)
{
    _registered_resources.insert(res_ptr);
}

void
render_device::release_resource(render_device_resource* res_ptr)
{
    resource_ptr_set::iterator res_iter = _registered_resources.find(res_ptr);
    if (res_iter != _registered_resources.end()) {
        _registered_resources.erase(res_iter);
    }

    delete res_ptr;
}

std::ostream& operator<<(std::ostream& os, const render_device& ren_dev)
{
    ren_dev.print_device_informations(os);
    return (os);
}

} // namespace gl
} // namespace scm
