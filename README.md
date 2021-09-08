# FSV

[![travis ci](https://travis-ci.org/mcuelenaere/fsv.svg?branch=master)](https://travis-ci.org/mcuelenaere/fsv)

NOTE: this project is **not** actively maintained.

This repo is a fork of [fsv](http://fsv.sourceforge.net/), with some patches on top to align it more to the current state of affairs.
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

1. Update code, still using gtk+2, to build cleanly with
   `meson configure -Dc_args="-DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED" builddir`
2. Switch to Gtk+3

### Setup github actions for CI
