# MGU - Minimal GUI Utilities
A minimal set of utilities for building simple GUIs. Built on `wayland`, and
`glesv2`. `pangocairo` is used for text rendering. (Can also be compiled with
`emscripten`, but this is very experimental.)

## Libraries
- `mgu-win`: Window surface management.
- `mgu-gl`: Small utilities to make working with OpenGL easier.
- `mgu-text`: Small wrapper around `pangocairo` to render text to OpenGL
  surfaces.
- `mgu-sr`: Small 2D rendering library using OpenGL. (Currently can draw
  rectangles, and text.)
