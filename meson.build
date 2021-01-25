project('mgu', 'c', default_options : [ 'c_std=c11', 'default_library=static' ])

system = host_machine.system()

protocols_code = []
protocols_client_header = []

egl = dependency('egl')
glesv2 = dependency('glesv2')

deps_win = []
deps_text = []

if system == 'emscripten'
elif system == 'linux-android'
else
  deps_win += [ dependency('wayland-client'), dependency('wayland-egl') ]
  deps_text += dependency('pangocairo')

  # Wayland scanner stuff. Taken from wlroots (protocols/meson.build)
  dep_scanner = dependency('wayland-scanner')
  wayland_scanner = find_program(dep_scanner
    .get_pkgconfig_variable('wayland_scanner'))
  dep_wp = dependency('wayland-protocols', version: '>= 1.18')
  wl_protocol_dir = dep_wp.get_pkgconfig_variable('pkgdatadir')
  protocols = {
    'xdg-shell' : wl_protocol_dir / 'stable/xdg-shell/xdg-shell.xml',
    'wlr-layer-shell-unstable-v1': 'protocol/wlr-layer-shell-unstable-v1.xml',
  }
  foreach name, path : protocols
    code = custom_target(
      name.underscorify() + '_c',
      input: path,
      output: '@BASENAME@-protocol.c',
      command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
    )

    client_header = custom_target(
      name.underscorify() + '_client_h',
      input: path,
      output: '@BASENAME@-client-protocol.h',
      command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
    )

    protocols_code += [ code ]
    protocols_client_header += [ client_header ]
  endforeach
endif

cc = meson.get_compiler('c')
m = cc.find_library('m', required : false)

pu_proj = subproject('platform_utils')
pu_main_dep = pu_proj.get_variable('pu_main_dep')
pu_log_dep = pu_proj.get_variable('pu_log_dep')
pu_event_loop_dep = pu_proj.get_variable('pu_event_loop_dep')

libtouch_proj = subproject('libtouch')
libtouch = libtouch_proj.get_variable('libtouch_dep')

ds_proj = subproject('ds')
ds_matrix_dep = ds_proj.get_variable('ds_matrix_dep')
ds_vec_dep = ds_proj.get_variable('ds_vec_dep')
ds_iter_dep = ds_proj.get_variable('ds_iter_dep')
ds_hashmap_dep = ds_proj.get_variable('ds_hashmap_dep')

incdir = include_directories('include')

mgu_gl = library(
  'mgu-gl',
  'src/gl/render.c',
  'src/gl/shaders.c',
  'src/gl/texture.c',
  dependencies : glesv2,
  include_directories : incdir
)
mgu_gl_dep = declare_dependency(link_with : mgu_gl, include_directories : incdir)

mgu_win = library(
  'mgu-win',
  'src/win.c',
  protocols_code,
  protocols_client_header,
  dependencies : [ m, egl, ds_vec_dep, pu_event_loop_dep, deps_win ],
  include_directories : incdir
)
mgu_win_dep = declare_dependency(
  sources : [ protocols_client_header ],
  link_with : mgu_win,
  include_directories : incdir
)

mgu_text = library(
  'mgu-text',
  'src/text.c',
  dependencies : [ mgu_gl_dep, pu_main_dep, ds_vec_dep, ds_iter_dep, deps_text ],
  include_directories : incdir
)
mgu_text_dep = declare_dependency(link_with : mgu_text, include_directories : incdir)

mgu_sr = library(
  'mgu-sr-gl',
  'src/sr.c',
  dependencies : [ glesv2, mgu_gl_dep, mgu_text_dep, ds_matrix_dep, ds_vec_dep, pu_main_dep ],
  include_directories : incdir
)
mgu_sr_dep = declare_dependency(link_with : mgu_sr, include_directories : incdir)

mgu_input = library(
  'mgu-input',
  'src/input.c',
  dependencies : [ m, libtouch, ds_vec_dep, ds_matrix_dep ],
  include_directories : incdir
)
mgu_input_dep = declare_dependency(link_with : mgu_input, include_directories : incdir)

mgu_demo = library(
  'demo',
  'src/demo.c',
  include_directories: incdir,
  dependencies : [
    libtouch,
    mgu_sr_dep,
    mgu_gl_dep,
    mgu_text_dep,
    mgu_win_dep,
    glesv2,
    ds_matrix_dep,
    ds_hashmap_dep,
    pu_event_loop_dep,
    pu_main_dep,
    pu_log_dep,
  ]
)
mgu_dep = declare_dependency(link_with: mgu_demo)

if not meson.is_subproject()
  executable('demo', dependencies: mgu_dep)
endif
