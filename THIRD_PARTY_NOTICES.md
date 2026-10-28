# Third-Party Notices

This repository redistributes the following third-party components. Their licenses apply to those files, not to the repository's original code.

## Puya PY32F0xx LL drivers and device support

Files:

- `firmware/vendor/py32f0xx_ll/`
- `firmware/vendor/cmsis/Device/PY32F0xx/`
- `firmware/ld/py32f002ax5.ld`

Copyright:

- Copyright (c) Puya Semiconductor Co.
- Portions Copyright (c) 2016 STMicroelectronics.

License: BSD 3-Clause

```text
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
```

## Arm CMSIS Core

Files:

- `firmware/vendor/cmsis/Core/Include/`

Copyright (c) 2009-2019 Arm Limited. All rights reserved.

License: Apache License 2.0

<https://www.apache.org/licenses/LICENSE-2.0>

## Puya device pack

File:

- `tools/Puya.PY32F0xx_DFP.1.1.7.pack`

Copyright (c) Puya Semiconductor Co. The pack is redistributed unchanged for PyOCD convenience. Its embedded license terms apply.

## ATmega-Soldering-Station

The firmware application is derived from:

- <https://github.com/wagiminator/ATmega-Soldering-Station>

Copyright (c) Stefan Wagner (wagiminator).

License: Creative Commons Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0).

<https://creativecommons.org/licenses/by-sa/3.0/>

Attribution and share-alike requirements apply to the derived firmware logic. See [CREDITS.md](CREDITS.md).

## PY32 template

The build layout was originally based on:

- <https://github.com/IOsetting/py32f0-template>

Refer to that project for its license terms.

## SEGGER J-Link device files

Files:

- `tools/jlink/JLinkDevices/`

These are SEGGER/Puya device support files. SEGGER J-Link software is proprietary; these files are included only to make optional J-Link flashing work. Review SEGGER's terms before redistribution.
