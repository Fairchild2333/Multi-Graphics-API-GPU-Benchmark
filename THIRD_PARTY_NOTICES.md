# Third-party notices

This file covers third-party components copied into the release payload. The
project itself is licensed under MIT; see the repository root `LICENSE` (also
staged as `licenses/LICENSE`).

## GLAD

- Project: https://github.com/Dav1dde/glad
- Generated with GLAD 2.0.8 for OpenGL Core 4.3.
- License: MIT for GLAD; generated declarations also derive from Khronos
  specifications under the terms reproduced in `third_party/glad/LICENSE`.

## GLFW

- Project: https://www.glfw.org/
- License: zlib/libpng-style license.

Copyright (c) 2002-2006 Marcus Geelnard

Copyright (c) 2006-2019 Camilla Löwy

This software is provided 'as-is', without any express or implied warranty. In
no event will the authors be held liable for any damages arising from the use of
this software.

Permission is granted to anyone to use this software for any purpose, including
commercial applications, and to alter it and redistribute it freely, subject to
the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a product,
   an acknowledgment in the product documentation would be appreciated but is
   not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

## Microsoft Windows App SDK and Visual C++ runtime

The optional WinUI self-contained payload and Visual C++ runtime files are
Microsoft redistributables. Distribution remains subject to the applicable
Microsoft license and REDIST terms for the non-preview toolchain used to build
the release.

## RenderDoc

RenderDoc is bundled only when a complete portable payload is supplied. Its MIT
license and bundled third-party acknowledgements
must remain intact under `tools/RenderDoc`. Release staging accepts the official
x64 portable ZIP as an offline input or a pinned HTTPS URL plus SHA-256, and
records that archive provenance in `tools/RenderDoc/BUNDLE_SOURCE.json`.

## Cinematic Liquid research references (not vendored)

The Cinematic Liquid shaders and Vulkan integration in this repository are an
independent implementation. Their solver and presentation design were informed
by the following public research and open-source projects; no source files or
assets from these repositories are bundled here:

- MLS-MPM/APIC paper: https://yuanming.taichi.graphics/publication/2018-mlsmpm/
- jeantimex/fluid (MIT): https://github.com/jeantimex/fluid
- matsuoka-601/Splash (MIT): https://github.com/matsuoka-601/Splash
- luihabl/VkFluidSim (CC0-1.0): https://github.com/luihabl/VkFluidSim
- Wumpf/blub (MIT): https://github.com/Wumpf/blub
