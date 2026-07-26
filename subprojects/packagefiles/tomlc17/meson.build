project('tomlc17', 'c', version: '1.0.0')

tomlc17_lib = static_library('tomlc17', 'src/tomlc17.c', c_args: '-std=c17')
tomlc17_dep = declare_dependency(
  include_directories: include_directories('src'),
  link_with: tomlc17_lib
)
