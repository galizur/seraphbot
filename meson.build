project(
    'seraphbot',
    'cpp',
    version: '0.1.0',
    default_options: ['warning_level=3', 'werror=false', 'cpp_std=c++23'],
)

cmake = import('cmake')

openssl_dep = dependency('openssl', required: true)
boost_dep = dependency('boost', modules: ['process'], required: true)
nlohmann_dep = dependency('nlohmann_json', required: true)
thread_dep = dependency('threads', required: true)
glfw_dep = dependency('glfw3', required: true)
gl_dep = dependency('gl', required: true)

imgui_proj = subproject('imgui')
imgui_dep = imgui_proj.get_variable('imgui_dep')

spdlog_proj = cmake.subproject('spdlog')
spdlog_dep = spdlog_proj.get_variable('spdlog_dep')

inc = include_directories('include')

subdir('src')

executable(
    'seraphbot',
    [sources, tw_sources, ui_sources],
    include_directories: inc,
    dependencies: [
        openssl_dep,
        boost_dep,
        nlohmann_dep,
        thread_dep,
        imgui_dep,
        glfw_dep,
        gl_dep,
        spdlog_dep,
    ],
    cpp_args: ['-DBOOST_PROCESS_HEADER_ONLY']
)
