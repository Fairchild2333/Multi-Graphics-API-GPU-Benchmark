# Package limitations

This package is a staged Windows x64 engineering release. It is not yet a
fully self-contained public installer.

- Windows binaries delay-load `vulkan-1.dll` and probe it before any Vulkan API
  call, so a machine without a Vulkan loader can fall back to DirectX/WARP. The
  Vulkan SDK and system/driver loader are not bundled. Binary import auditing is
  part of staging, but startup still needs a clean-machine test with
  `vulkan-1.dll` absent before this fallback is release-qualified.
- Python report sources are included for traceability, but no target-machine
  Python dependency is supported. Reporting is portable only when the manifest
  says a frozen `report_worker` is bundled and application integration uses it.
- RenderDoc is optional and is not copied unless a complete portable directory
  is supplied at package time. The application checks staged
  `tools/RenderDoc`, and for Vulkan configures that directory as a process-local
  implicit-layer path before GPU probing. The staged fifth-second capture works
  on the build machine, but still needs a clean-machine validation before the
  workflow is release-qualified.
- A WinUI payload is included only when a prebuilt x64 self-contained output is
  supplied. Presence in the ZIP does not replace a clean-machine launch test.
- CMake attempts to bundle the release MSVC runtime DLLs. Redistribution is
  subject to the Visual Studio license, and the package verifier must confirm
  the required DLLs are present.
- The stage includes a complete `files.sha256` inventory; the ZIP and MSI get
  separate SHA-256 release-asset hashes. These detect accidental corruption but
  do not replace Authenticode signing.
- The project is distributed under the MIT license (`LICENSE` at the repository
  root, also staged under `licenses/`). Third-party notices cover GLAD, GLFW,
  RenderDoc (when bundled), and the Microsoft redistributable caveat. Unsigned
  public builds and clean-machine install/capture validation remain release
  gates.

The ZIP deliberately contains no user results, captures, reports, logs,
developer SDKs, vcpkg tree, shader compiler, Python installation, PDB files, or
source-tree build libraries.
