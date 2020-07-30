project('micro_gui_utils', 'c', default_options : [ 'c_std=c11' ])

cc = meson.get_compiler('c')
m = cc.find_library('m', required : false)

wayland_client = dependency('wayland-client')
wayland_egl = dependency('wayland-egl')
egl = dependency('egl')
glesv2 = dependency('glesv2')

libtouch_proj = subproject('libtouch')
libtouch = libtouch_proj.get_variable('libtouch_dep')

incdir = include_directories('include')

# XDG Shell boilerplate
dep_scanner = dependency('wayland-scanner')
prog_scanner = find_program(dep_scanner
  .get_pkgconfig_variable('wayland_scanner'))
dep_wp = dependency('wayland-protocols', version: '>= 1.18')
dir_wp_base = dep_wp.get_pkgconfig_variable('pkgdatadir')
base_file = 'xdg-shell'
xml_path = '@0@/stable/@1@/@1@.xml'.format(dir_wp_base, base_file)
xdg_shell_header_target = custom_target(
  'xdg_shell_header_target',
  command: [ prog_scanner, 'client-header', '@INPUT@', '@OUTPUT@' ],
  input: xml_path,
  output: '@0@-client-protocol.h'.format(base_file),
)
xdg_shell_private_target = custom_target(
  'xdg_shell_private_target',
  command: [ prog_scanner, 'private-code', '@INPUT@', '@OUTPUT@' ],
  input: xml_path,
  output: '@0@-client-protocol.c'.format(base_file),
)

libmgu = static_library(
  'mgu',
  'src/wayland/display.c',
  'src/wayland/window.c',
  'src/render/render.c',
  'src/render/shaders.c',
  'src/linalg.c',
  xdg_shell_header_target,
  xdg_shell_private_target,
  dependencies : [ m, wayland_client, wayland_egl, egl, glesv2 ],
  include_directories: incdir
)

executable(
  'demo',
  'src/demo.c',
  include_directories: incdir,
  link_with: libmgu,
  dependencies : [ libtouch ]
)
