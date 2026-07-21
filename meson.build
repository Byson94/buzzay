project('buzzay', 'c',
    version: '0.1',
    default_options: [
        'c_std=c11',
        'warning_level=2',
    ]
)

cc = meson.get_compiler('c')

add_project_arguments([
    '-D_DEFAULT_SOURCE',
    '-DWLR_USE_UNSTABLE',
    '-fvisibility=hidden',
], language: 'c')

add_project_link_arguments([
    '-rdynamic',
], language: 'c')

# Dependencies
wlroots_dep = dependency('wlroots-0.20')
wayland_server_dep = dependency('wayland-server')
xkbcommon_dep = dependency('xkbcommon')
scenefx_proj = subproject('scenefx')
scenefx_dep = dependency('scenefx-0.5')

# Wayland protocol generation
wayland_scanner_dep = dependency('wayland-scanner', native: true)
wayland_scanner = find_program(wayland_scanner_dep.get_variable(pkgconfig: 'wayland_scanner'), native: true)

wlr_protos_dep = dependency('wlr-protocols', native: true)
proto_dir = wlr_protos_dep.get_variable(pkgconfig: 'pkgdatadir') + '/unstable/'

layer_shell_xml = proto_dir + '/wlr-layer-shell-unstable-v1.xml'

layer_shell_header = custom_target(
    'wlr-layer-shell-unstable-v1-protocol.h',
    output: 'wlr-layer-shell-unstable-v1-protocol.h',
    input: layer_shell_xml,
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@']
)

# Get all code
src_files = run_command('find', 'src', '-name', '*.c', check: true).stdout().strip().split('\n')
sources = files(src_files)

inc_dirs = include_directories('include', '.')

# build
executable(
    'buzzay',
    sources,
    layer_shell_header,
    include_directories: inc_dirs,
    dependencies: [
        wlroots_dep,
        wayland_server_dep,
        xkbcommon_dep,
        # scenefx_dep,
    ],
    install: true
)
