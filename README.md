# FSV

This repo is a fork of [fsv](http://fsv.sourceforge.net/), updated to current environments.
The original author is [Daniel Richard G.](http://fox.mit.edu/skunk/), a former student of Computer Science at the MIT.

## About fsv

> fsv (pronounced eff-ess-vee) is a file system visualizer in cyberspace. It lays out files and directories in three dimensions, geometrically representing the file system hierarchy to allow visual overview and analysis. fsv can visualize a modest home directory, a workstation's hard drive, or any arbitrarily large collection of files, limited only by the host computer's memory and graphics hardware.

Its ancestor, SGI's `fsn` (pronounced "fusion") originated on IRIX and was prominently featured in Jurassic Park: ["It's a Unix system!"](https://www.youtube.com/watch?v=3HjOjvu6oKA). 

[Screenshots](http://fsv.sourceforge.net/screenshots/) of the original clone are still available.

Useful info and screenshots of the original SGI IRIX implementation are available on [siliconbunny](http://www.siliconbunny.com/fsn-the-irix-3d-file-system-tool-from-jurassic-park/).

### Install

1. Clone the repository
2. Install dependencies (Ubuntu): `sudo apt-get install libgtkgl2.0-dev libgl1-mesa-dev libglu1-mesa-dev`
3. Run meson in repository root directory: `meson setup builddir`
    - Or set non-default options: `meson setup -Dbuildtype=release -Dprefix=~/.local builddir`
    - Check current options: `meson configure builddir`
    - Modify options on existing builddir: `meson configure -Doptimization=g builddir`
4. Compile: `ninja -C builddir`
5. Install: `sudo ninja -C builddir install`

## TODO

### Update to Gtk+3

1. DONE Update code, still using gtk+2, to build cleanly with
   `meson configure -Dc_args="-DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED" builddir`
2. DONE Update code, still using gtk+-2.0, to build cleanly with
   `meson configure -Dc_args="-DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGTK_DISABLE_SINGLE_INCLUDES -DGSEAL_ENABLE" builddir`
3. Update to 'modern' OpenGL 2.1. GtkGLArea in Gtk+3 supports only core
   profile, so modernize the OpenGL code as much as possible before switching to
   Gtk+3 and the OpenGL core profile.
    1. Switch from GL_QUADS to GL_TRIANGLES
4. Switch to Gtk+3

### Setup github actions for CI

### Use Gtk calendar

There exists commented out code for using gnome date edit widget
(gnome_date_edit_new etc.). Use gtk calendar widget instead.
https://developer-old.gnome.org/gtk2/stable/GtkCalendar.html

## Misc notes

### OpenGL versions and compatibility

Gtk+3 tries to create a OpenGL 3.2 core context, and if that fails it falls
back to whatever legacy context it manages to create. Thus one cannot assume
availability of any legacy pre-3.2 API's that are dropped in a core context. So
for maximum compatibility use the oldest API's still possible in 3.2.

For the shading language version, the latest [OpenGL core
specification](https://www.khronos.org/registry/OpenGL/specs/gl/glspec46.core.pdf)
says "The core profile of OpenGL 4.6 is also guaranteed to support all previous
versions of the OpenGL Shading Language back to version 1.40". Thus GLSL 1.40
is the minimum version. This corresponds to OpenGL 3.1. However, it also says
that "OpenGL 4.6 implementations are guaranteed to support versions 1.00, 3.00,
and 3.10 of the OpenGL ES Shading Language.". So that is also an alternative.
There's also WebGL 2.0 that roughly corresponds to OpenGL ES 3.0 & OpenGL 3.3
and also uses GLSL ES 3.0.

Fsv is a relatively simple OpenGL application and doesn't need very fancy
features. Thus, aim for OpenGL 3.1 and GLSL 1.40 in order to provide maximum
compatibility.
