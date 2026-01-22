#pragma once

// On Windows: set DLL search dir and default PROJ/GDAL data paths relative to the exe.
// On non-Windows: no-op.
void win_prepare_runtime();
