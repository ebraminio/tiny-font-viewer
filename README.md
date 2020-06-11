Tiny Font Viewer
================

```
meson build && ninja -Cbuild && build/tiny-font-viewer
```

Cross compile to Windows,

```
meson winbuild --cross-file win32-cross-file.txt && ninja -Cwinbuild && i686-w64-mingw32-strip winbuild/tiny-font-viewer.exe && wine winbuild/tiny-font-viewer.exe
```
