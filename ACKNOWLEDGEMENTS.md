# Acknowledgements

ofxAbletonLinkAudio depends on, or interoperates with, software developed
by other parties. This file lists those dependencies and the terms
under which their work is used.

## Ableton Link / Link Audio

This addon is built on top of **Ableton Link Audio**, the audio
extension shipped with Ableton Link starting in Live 12.4. Link Audio
is open-source software developed by **Ableton AG** and released
under the MIT license.

This implementation is independent and is **not endorsed, certified,
or supported by Ableton**. "Ableton", "Live", "Link", and related
marks are trademarks of Ableton AG.

References:
- Ableton Link source:     https://github.com/Ableton/link
- Link concepts overview:  https://ableton.github.io/link/

The Link source is bundled inside this addon under `libs/link/`. Its
license terms are reproduced verbatim below, as required by the MIT
license.

```
Copyright 2016, Ableton AG, Berlin. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## ASIO standalone

Bundled with Ableton Link as a sub-submodule under
`libs/link/modules/asio-standalone/`. Used by Link for networking
primitives (it is the standalone Boost ASIO library, not the audio
ASIO). Boost Software License 1.0.

## Shared core

This addon ships a small shared C++ core under `libs/core/`
(`LinkAudioManager`, `AudioRingBuffer`) that is also used by the
TouchDesigner CHOPs, Max externals, VCV Rack modules, and VST3/AU
plugins distributed in the parent **VoidLinkAudio** R&D project:

- https://github.com/gluon/td-linkaudio  *(or wherever the parent repo lives)*

The core is released under the same MIT license as this addon.

## openFrameworks

The examples in this addon are openFrameworks applications and rely
on the openFrameworks runtime (see https://openframeworks.cc).
openFrameworks is licensed under the MIT license; consult the oF
distribution for its full terms.

---

ofxAbletonLinkAudio is part of Structure Void's open R&D.
https://structure-void.com  -  https://julienbayle.net
