This is a simple example that shows how to get GPU temperature for NVIDIA and AMD GPUs.

The code lies in ``entry_point.c``.
It makes use of two APIs:
- NvAPI for NVIDIA GPUs (check https://developer.nvidia.com/rtx/path-tracing/nvapi/get-started)
- ADL for AMD GPUs (check https://gpuopen.com/adl)

To compile this simply give ``entry_point.c`` to your compiler of choice.

MSVC example: ``cl /O2 /TC entry_point.c /Fegpu_temperature.exe``

``entry_point.c`` uses the MIT license.
API header files use their own respective licenses.
