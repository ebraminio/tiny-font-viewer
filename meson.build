project('tiny-font-viewer', 'c',
  version : '0.1',
  default_options : [
    'warning_level=3',
    'b_pch=false',
    'c_std=c99',
    'cpp_std=c++11',
    'default_library=static',
  ])

harfbuzz = subproject('harfbuzz', default_options : ['experimental_api=true',
  'glib=disabled', 'gobject=disabled', 'cairo=disabled',
  'fontconfig=disabled', 'icu=disabled', 'freetype=disabled',
  'introspection=disabled', 'gtk_doc=disabled', 'tests=disabled'])
harfbuzz_dep = harfbuzz.get_variable('libharfbuzz_dep')
harfbuzz_incsrc = harfbuzz.get_variable('incsrc')

libui = subproject('libui')
libui_deps = libui.get_variable('libui_deps')
libui_libui = libui.get_variable('libui_libui')

exe = executable('tiny-font-viewer', 'tiny-font-viewer.c',
  dependencies : [harfbuzz_dep, libui_deps],
  include_directories : [harfbuzz_incsrc, 'subprojects/libui'],
  link_with : libui_libui,
  install : true)

#test('basic', exe)
