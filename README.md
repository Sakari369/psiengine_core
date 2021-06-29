PSITriangle 3D Engine core libraries
=====================

These are the core parts of my custom 3D engine.
See psiengine_public/README.MD for more information.

Software needed for building.
================================

PSIEngine uses the following open source libraries and software:

Needed to compile:

 - https://cmake.org/ :: CMake for building.
 - https://conan.io/ :: C++ package manager. Needed to build and install library dependencies.

Library dependencies installed via Conan:

 - https://glm.g-truc.net/0.9.9/index.html :: GLSL semantic compatible math lib. Vectors and matrixes etc
 - http://glew.sourceforge.net/ :: OpenGL Extension Wrapper. Makes it easy to use available OpenGL extensions.
 - https://www.freetype.org/ :: Freetype for text rendering support.

Compiling & Running
===================

Clone the repository, run:

`git submodule update --init --recursive`
To pull in the freetype-gl headers.

Then compile the core engine library:

```bash
mkdir build && cd build
conan install ../
cmake -G Ninja ../ (or cmake -g "Unix Makefiles" ../)
ninja (or make)
```

This will build the core library under build/lib.
