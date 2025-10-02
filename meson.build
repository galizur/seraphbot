project(
    'seraphbot',
    'cpp',
    version: '0.1.0',
    default_options: ['warning_level=3', 'werror=false', 'cpp_std=c++23,c++latest,c++20'],
)

add_project_arguments('-DIMGUI_IMPL_OPENGL_LOADER_CUSTOM', language: 'cpp')

if host_machine.system() == 'linux'
  add_project_arguments('-DGLFW_USE_WAYLAND=1', language: 'cpp')
endif

# Libc++ detection
cpp_args = get_option('cpp_args')
cpp_link_args = get_option('cpp_link_args')
using_libcxx = false
foreach arg : cpp_args
  if arg.contains('-stdlib=libc++')
    using_libcxx = true
    break
  endif
endforeach
if not using_libcxx
  foreach arg : cpp_link_args
    if arg.contains('-stdlib=libc++')
      using_libcxx = true
      break
    endif
  endforeach
endif

# System libraries
dl_dep = dependency('dl', required: false)
boost_head_dep = dependency('boost', version: '=1.88.0', required: true)
boost_dep = dependency('boost', modules: ['process'], version: '=1.88.0', required: false)
openssl_dep = dependency('openssl', version: '>=3.0.8', required: true, fallback: ['openssl', 'openssl_dep'])
spdlog_dep = dependency('spdlog', version: '>=1.15.3', required: not using_libcxx)
nlohmann_dep = dependency('nlohmann_json', version: '>=3.12', required: true)
thread_dep = dependency('threads', required: true)
glfw_dep = dependency('glfw3', version: '>=3.4', required: true)
gl_dep = dependency('gl', required: true)
lua_dep = dependency('lua', fallback: ['lua', 'lua_dep'], required: true)
sol2_dep = dependency('sol2', fallback: ['sol2', 'sol2_dep'], required: true)
miniaudio_dep = dependency('miniaudio', required: true)

# Subprojects
imgui_proj = subproject('imgui', default_options: 'default_library=static')
imgui_dep = imgui_proj.get_variable('imgui_dep')

cmake = import('cmake')
opt_var = cmake.subproject_options()
opt_var.set_override_option('default_library', 'static')
if not boost_dep.found()
  message('Building boost from source')
  boost_proj = cmake.subproject('boost', options: opt_var)
  boost_dep = boost_proj.get_variable('boost_dep')
endif

if not spdlog_dep.found() or using_libcxx
  message('Building spdlog from source (libc++ compatibility or system version not found)')
  opt_var.add_cmake_defines({'SPDLOG_BUILD_SHARED': false})
  if using_libcxx
    opt_var.add_cmake_defines({'CMAKE_CXX_FLAGS': '-stdlib=libc++'})
  endif
  spdlog_proj = cmake.subproject('spdlog', options: opt_var)
  spdlog_dep = spdlog_proj.get_variable('spdlog_dep')
endif

inc = include_directories('include')

subdir('src')

executable(
    'seraphbot',
    [sources, core_sources, tw_sources, ui_sources],
    include_directories: inc,
    dependencies: [
        openssl_dep,
        boost_head_dep,
        boost_dep,
        nlohmann_dep,
        thread_dep,
        glfw_dep,
        imgui_dep,
        gl_dep,
        spdlog_dep,
        lua_dep,
        sol2_dep,
        miniaudio_dep
    ],
)
