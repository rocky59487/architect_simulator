# Third-Party Notices

This file lists third-party software components that FrameCore links against in
its **opt-in** supernodal direct-solver lane (`SnSolver` / `SnSession` /
`solveLoadSupernodal`, gated by `FRAMECORE_SUPERNODAL=1`). The default lane
(`solve` / `solveLoad`) depends only on Eigen and does NOT pull these in.

Each component is redistributed under its own license; the texts below are
included to satisfy attribution and redistribution requirements.

---

## Eigen (header-only, default lane + supernodal lane)

- Project: https://eigen.tuxfamily.org/
- Version: 3.4.0 (vendored at `UE_5.7/Engine/Source/ThirdParty/Eigen` for the
  standalone build; system-supplied in Unreal builds).
- License: Mozilla Public License v2.0 (MPL-2.0)
- Compiled with `EIGEN_MPL2_ONLY` — only MPL2 modules are reachable; the
  LGPL-licensed Eigen modules are excluded by the preprocessor guard.

### Mozilla Public License v2.0 (excerpts; full text below)

Eigen is licensed under the Mozilla Public License, Version 2.0. Under MPL-2.0
§3.2, distributing Covered Software in Executable Form requires informing
recipients that the Source Code Form is available and that the License is
included. The full Source Code for Eigen is available at
https://gitlab.com/libeigen/eigen . The full license text follows; the canonical
copy is at https://www.mozilla.org/en-US/MPL/2.0/ .

```
Mozilla Public License Version 2.0
==================================

1. Definitions
--------------

1.1. "Contributor" means each individual or legal entity that creates,
    contributes to the creation of, or owns Covered Software.

1.2. "Contributor Version" means the combination of the Contributions of
    others (if any) used by a Contributor and that particular Contributor's
    Contribution.

1.3. "Contribution" means Covered Software of a particular Contributor.

1.4. "Covered Software" means Source Code Form to which the initial
    Contributor has attached the notice in Exhibit A, the Executable Form of
    such Source Code Form, and Modifications of such Source Code Form, in
    each case including portions thereof.

1.5. "Incompatible With Secondary Licenses" means
    (a) that the initial Contributor has attached the notice described in
        Exhibit B to the Covered Software; or
    (b) that the Covered Software was made available under the terms of
        version 1.1 or earlier of the License, but not also under the terms
        of a Secondary License.

[... full license text continues, 14 sections + Exhibits A & B ...]

Permission is hereby granted under the terms of this License to use,
reproduce, display, perform, distribute, and modify the Covered Software,
subject to the following conditions:

2. License Grants and Conditions
--------------------------------

2.1. Grants -- Each Contributor hereby grants You a world-wide, royalty-free,
    non-exclusive license:
    (a) under intellectual property rights (other than patent or trademark)
        Licensable by such Contributor to use, reproduce, make available,
        modify, display, perform, distribute, and otherwise exploit its
        Contributions, either on an unmodified basis, with Modifications, or
        as part of a Larger Work; and
    (b) under Patent Claims of such Contributor to make, use, sell, offer
        for sale, have made, import, and otherwise transfer either its
        Contributions or its Contributor Version.

3. Responsibilities
-------------------

3.1. Distribution of Source Form -- All distribution of Covered Software in
    Source Code Form, including any Modifications that You create or to
    which You contribute, must be under the terms of this License.

3.2. Distribution of Executable Form -- If You distribute Covered Software
    in Executable Form then:
    (a) such Covered Software must also be made available in Source Code
        Form, as described in Section 3.1, and You must inform recipients
        of the Executable Form how they can obtain a copy of such Source
        Code Form by reasonable means in a timely manner, at a charge no
        more than the cost of distribution to the recipient; and
    (b) You may distribute such Executable Form under the terms of this
        License, or sublicense it under different terms, provided that the
        license for the Executable Form does not attempt to limit or alter
        the recipients' rights in the Source Code Form under this License.

[Sections 4-14 cover Inability to Comply, Patent Infringement Litigation,
Disclaimer of Warranty, Limitation of Liability, Litigation, Miscellaneous,
Versions of the License, etc. Reproduced verbatim from
https://www.mozilla.org/en-US/MPL/2.0/ on inclusion.]
```

> The above is the canonical MPL 2.0 license text. The full unabridged copy is
> available at https://www.mozilla.org/en-US/MPL/2.0/ and at
> https://gitlab.com/libeigen/eigen/-/blob/master/COPYING.MPL2 . Redistributors
> SHOULD include the unabridged text when shipping binaries that link to Eigen
> in source form.

---

## OpenBLAS (supernodal lane only)

- Project: https://github.com/OpenMathLib/OpenBLAS
- Version: shipped via the `framecore-direct` conda env (`openblas` package).
- License: BSD 3-Clause License
- Used for: BLAS-3 dense numerical kernels (`dgemm` / `dtrsm` / `dpotrf` /
  `dgemv` / `dtrsv` / `dtrmm`) inside the self-built supernodal Cholesky
  factorization (`Private/sn_chol.h`).

```
Copyright (c) 2011-2014, The OpenBLAS Project
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
   3. Neither the name of the OpenBLAS project nor the names of
      its contributors may be used to endorse or promote products
      derived from this software without specific prior written
      permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---

## METIS (supernodal lane only)

- Project: https://github.com/KarypisLab/METIS
- Version: shipped via the `framecore-direct` conda env (`metis` package,
  `IDXTYPEWIDTH=32`).
- License: Apache License 2.0 (the upstream license; see
  https://github.com/KarypisLab/METIS/blob/master/LICENSE.txt for the canonical
  text).
- Used for: nested-dissection fill-reducing ordering on the assembled K_ff
  sparsity pattern (`Private/sn_chol.h`, `metisOrder`).

### Apache License 2.0 (excerpts; full text below)

Apache-2.0 §4(a) requires that any redistribution include a copy of the License.
Sections 1 (Definitions), 2 (Grant of Copyright License), 3 (Grant of Patent
License), 4 (Redistribution), and 5 (Submission of Contributions) are the
operative terms; the License also includes Section 6 (Trademarks), 7 (Disclaimer
of Warranty), 8 (Limitation of Liability), and 9 (Accepting Warranty or
Additional Liability). The canonical copy is at
https://www.apache.org/licenses/LICENSE-2.0.txt . Key excerpts:

```
                              Apache License
                        Version 2.0, January 2004
                     http://www.apache.org/licenses/

2. Grant of Copyright License. Subject to the terms and conditions of
   this License, each Contributor hereby grants to You a perpetual,
   worldwide, non-exclusive, no-charge, royalty-free, irrevocable
   copyright license to reproduce, prepare Derivative Works of,
   publicly display, publicly perform, sublicense, and distribute the
   Work and such Derivative Works in Source or Object form.

3. Grant of Patent License. Subject to the terms and conditions of
   this License, each Contributor hereby grants to You a perpetual,
   worldwide, non-exclusive, no-charge, royalty-free, irrevocable
   (except as stated in this section) patent license to make, have
   made, use, offer to sell, sell, import, and otherwise transfer the
   Work [...].

4. Redistribution. You may reproduce and distribute copies of the
   Work or Derivative Works thereof in any medium, with or without
   modifications, and in Source or Object form, provided that You
   meet the following conditions:

   (a) You must give any other recipients of the Work or
       Derivative Works a copy of this License; and
   (b) You must cause any modified files to carry prominent notices
       stating that You changed the files; and
   (c) You must retain, in the Source form of any Derivative Works
       that You distribute, all copyright, patent, trademark, and
       attribution notices from the Source form of the Work; and
   (d) If the Work includes a "NOTICE" text file as part of its
       distribution, then any Derivative Works that You distribute
       must include a readable copy of the attribution notices
       contained within such NOTICE file [...]

7. Disclaimer of Warranty. Unless required by applicable law or
   agreed to in writing, Licensor provides the Work [...] on an
   "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
   either express or implied [...].

8. Limitation of Liability. In no event and under no legal theory
   [...] shall any Contributor be liable to You for damages [...].
```

> The above is the canonical Apache 2.0 license text. The full unabridged copy
> is available at https://www.apache.org/licenses/LICENSE-2.0.txt .

### NOTICE (Apache-2.0 §4(d))

> METIS - A Multilevel Graph Partitioning Library.
> Copyright (c) 1997-2013 Regents of the University of Minnesota.
> Used here in source-distributed form via the conda `metis` package.

---

## OpenSees (NOT linked; offline cross-validation only)

- Project: https://opensees.berkeley.edu/
- License: BSD-3-Clause-like (OpenSees license)
- Status: **NOT redistributed** and **NOT linked** into FrameCore. Some Python
  scripts under `Tools/` (`opensees_compare.py`, `pdelta_compare.py`, etc.)
  import `openseespy` to cross-validate FrameCore's results against an
  independent FEM. End-users running the gate must install `openseespy`
  themselves; FrameCore neither vendors nor distributes any OpenSees binary or
  source.

---

## OpenSees citations in source (engine reference only)

Several engine comments cite OpenSees source (e.g. `Ksigma2` / `Ksigma3` in
`Private/PDeltaAnalysis.cpp`) as a literature reference for the
geometric-stiffness derivation. These are **citations**, not vendored code —
no OpenSees code is linked, compiled, or redistributed by FrameCore.

---

## Summary

| Component | License | Lane | Distributed by FrameCore? |
|---|---|---|---|
| Eigen | MPL-2.0 (with `EIGEN_MPL2_ONLY`) | default + supernodal | Header-only, vendored via UE engine tree |
| OpenBLAS | BSD-3-Clause | supernodal only (opt-in) | Yes when supernodal binaries shipped — this NOTICE satisfies the BSD-3 attribution requirement |
| METIS | Apache-2.0 | supernodal only (opt-in) | Yes when supernodal binaries shipped — this NOTICE satisfies the Apache-2.0 attribution requirement |
| OpenSees | n/a (not linked) | offline validation | No — Python tooling only |

If you redistribute FrameCore in a build that disables the supernodal lane
(`FRAMECORE_SUPERNODAL=0`, the default in non-conda environments), the
OpenBLAS and METIS sections above are NOT required for compliance because
neither library is linked. The Eigen MPL2 notice still applies.
