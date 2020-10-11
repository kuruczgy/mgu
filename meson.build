project('mgu', 'c', default_options : [ 'c_std=c11', 'default_library=static' ])

em = host_machine.system() == 'emscripten'

protocols_code = []
protocols_client_header = []

if em
  wayland_client = []
  wayland_egl = []
  egl = []
  glesv2 = []
  pangocairo = []
else
  wayland_client = dependency('wayland-client')
  wayland_egl = dependency('wayland-egl')
  egl = dependency('egl')
  glesv2 = dependency('glesv2')
  pangocairo = dependency('pangocairo')

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

libtouch_proj = subproject('libtouch')
libtouch = libtouch_proj.get_variable('libtouch_dep')

ds_proj = subproject('ds')
ds_matrix_dep = ds_proj.get_variable('ds_matrix_dep')
ds_vec_dep = ds_proj.get_variable('ds_vec_dep')

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

if em
  mgu_win = library(
    'mgu-win',
    'src/win_em.c',
    dependencies : [ m, egl ],
    include_directories : incdir
  )
else
  mgu_win = library(
    'mgu-win',
    'src/wayland/window.c',
    'src/wayland/display.c',
    protocols_code,
    protocols_client_header,
    dependencies : [ m, egl, wayland_client, wayland_egl ],
    include_directories : incdir
  )
endif
mgu_win_dep = declare_dependency(
  sources : [ protocols_client_header ],
  link_with : mgu_win,
  include_directories : incdir
)

mgu_text = library(
  'mgu-text',
  'src/text.c',
  dependencies : [ pangocairo, mgu_gl_dep ],
  include_directories : incdir
)
mgu_text_dep = declare_dependency(link_with : mgu_text, include_directories : incdir)

mgu_sr_gl = library(
  'mgu-sr-gl',
  'src/sr.c',
  dependencies : [ glesv2, mgu_gl_dep, mgu_text_dep, ds_matrix_dep, ds_vec_dep ],
  include_directories : incdir
)
mgu_sr_gl_dep = declare_dependency(link_with : mgu_sr_gl, include_directories : incdir)

mgu_input = library(
  'mgu-input',
  'src/input.c',
  dependencies : [ m, libtouch, ds_vec_dep, ds_matrix_dep ],
  include_directories : incdir
)
mgu_input_dep = declare_dependency(link_with : mgu_input, include_directories : incdir)

# libmgu = static_library(
#   'mgu',
#   'src/wayland/display.c',
#   'src/wayland/window.c',
#   'src/render/render.c',
#   'src/render/shaders.c',
#   'src/render/texture.c',
#   'src/render/text.c',
#   'src/linalg.c',
#   protocols_code,
#   protocols_client_header,
#   dependencies : [ m, wayland_client, wayland_egl, egl, glesv2, pangocairo ],
#   include_directories: incdir
# )
# 
# libmgu_dep = declare_dependency(include_directories : incdir, link_with: libmgu,
#   sources : [ protocols_client_header ])

executable(
  'demo',
  'src/demo.c',
  include_directories: incdir,
  dependencies : [
    libtouch,
    mgu_sr_gl_dep,
    mgu_gl_dep,
    mgu_text_dep,
    mgu_win_dep,
    glesv2,
    ds_matrix_dep
  ]
)

if em
  conf_data = configuration_data()
  conf_data.set('exe_name', 'demo')
  configure_file(input : 'index.html.in',
		 output : 'index.html',
		 configuration : conf_data)
endif
