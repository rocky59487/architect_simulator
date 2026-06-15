#pragma once
//
// The SINGLE include point for the self-built supernodal Cholesky (sn_chol.h). sn_chol.h pulls in
// <metis.h> + <cblas.h> and declares LAPACKE_dpotrf / openblas_set_num_threads / openblas_get_num_threads
// via extern "C". Every FrameCore TU that needs the supernodal solver includes THIS header, never
// "sn_chol.h" directly (mirrors the FrameEigen.h choke-point pattern).
//
//   * Standalone build : FRAMECORE_UE undefined -> plain include; FRAMECORE_SUPERNODAL defaults 1.
//   * UE module build  : FRAMECORE_UE=1 -> headers wrapped in UE's third-party guard macros.
//     FRAMECORE_SUPERNODAL stays 0 until MSVC-clean OpenBLAS is wired in FrameCore.Build.cs
//     (the conda MinGW OpenBLAS is GNU-ar / MinGW-ABI and must not enter the UE build).
//
// FRAMECORE_SUPERNODAL gates the whole lane: when 0, sn_chol.h is NOT included and SnSolver.cpp's
// supernodal path compiles out (it falls back to LDLT). Callers may still call solveLoadSupernodal;
// it just routes to the LDLT oracle.
//
#if defined(FRAMECORE_UE)
  #ifndef FRAMECORE_SUPERNODAL
  #  define FRAMECORE_SUPERNODAL 0      // UE: off until MSVC-clean OpenBLAS (see FrameCore.Build.cs)
  #endif
  #if FRAMECORE_SUPERNODAL
    #include "CoreMinimal.h"            // THIRD_PARTY_INCLUDES_* / PRAGMA_* (UE branch only)
    PRAGMA_DEFAULT_VISIBILITY_START
    THIRD_PARTY_INCLUDES_START
    #include "sn_chol.h"                // pulls in <metis.h> + <cblas.h>
    THIRD_PARTY_INCLUDES_END
    PRAGMA_DEFAULT_VISIBILITY_END
  #endif
#else
  #ifndef FRAMECORE_SUPERNODAL
  #  define FRAMECORE_SUPERNODAL 1      // standalone: on (build.bat links conda OpenBLAS/METIS)
  #endif
  #if FRAMECORE_SUPERNODAL
    #include "sn_chol.h"
  #endif
#endif
