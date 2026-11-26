# Isolated Skia evaluation

These probes exercise graphics and text in isolation. The adopted backend and production build are documented in [Windows Skia runtime](../../../docs/skia-runtime.md).
Build serially (`-j 1`) on this machine. Runtime performance acceptance is deferred.

## Fixed candidate

- Skia: `e26a2f19555c0cc7cfe5f457d6555bcbf22aaca1`, official `https://skia.googlesource.com/skia.git`.
- GN: `b2afae122eeb6ce09c52d63f67dc53fc517dbdc8`, fetched by the candidate’s `bin/fetch-gn`.
- Persistent ignored source location: `.build-support/skia-evaluation/skia`.
- Generated probes, installed evaluation package and evidence: `out/skia-evaluation`.
- Dependencies: `fetch-dependencies.py SKIA_ROOT` reads exact revisions from DEPS;
  its manifest is written next to the checkout. It never changes global Git configuration.

Verify the checkout revision before building. The GN files record this evaluation
machine’s paths; adjust the Visual Studio/Emscripten paths for another machine.
Use an x64 MSVC developer prompt, Release, and the DLL CRT (`/MD`).
GN uses the explicit Python executable for generation, but some generated copy/archive
rules call `python3`. Ensure that command resolves to real Python, not a Windows
application-execution alias. A process-local PATH shim is sufficient; global changes
are unnecessary.

## Windows CPU, GPU and text

Copy `gpu-text.gn` to `SKIA_ROOT/out/sunfleck-gpu-text/args.gn`, then run in that checkout:

```powershell
bin/gn.exe gen out/sunfleck-gpu-text --script-executable=C:/path/to/python.exe
ninja -C out/sunfleck-gpu-text -j 1 skia skparagraph
```

From Sunfleck, with `SKIA_ROOT` and `ANGLE_DIRECTORY` replaced by actual paths:

```powershell
cmake -S tools/development/skia-spike -B out/skia-evaluation/probe -G Ninja -DCMAKE_BUILD_TYPE=Release -DSKIA_ROOT=SKIA_ROOT -DSKIA_LIBRARY=SKIA_ROOT/out/sunfleck-gpu-text/skia.lib -DSKIA_EVALUATE_TEXT=ON -DSKIA_EVALUATE_GPU=ON -DANGLE_DIRECTORY=ANGLE_DIRECTORY
cmake --build out/skia-evaluation/probe -j 1
ctest --test-dir out/skia-evaluation/probe --output-on-failure
```

`ANGLE_DIRECTORY` must contain matching `libEGL.dll` and `libGLESv2.dll`. The GPU probe
loads only that explicit directory, requests ANGLE D3D11, and verifies GL_RENDERER.
The local VS Code ANGLE binaries used in this experiment are not a production
redistribution or reproducibility solution.

- `skia_cpu_semantics`: original minimal CPU probe.
- `skia_gpu_semantics`: CPU versus Ganesh/ANGLE at 96/144/192 DPI scale factors,
  nested transform/clip, group opacity, gradients, image upload, blur/shadow,
  ordinary and DirectWrite-shaped glyphs; two Win32 windows, resize,
  minimize/restore and explicit context recreation.
- `skia_text_semantics`: DirectWrite glyph-run rendering and a separate SkParagraph
  experiment with Latin ligatures, Chinese, Hebrew, Arabic, combining marks and
  emoji sequences. It checks UTF-16 grapheme ranges, hit positions and selection
  bounds and records layout differences.

The image threshold is fixed before testing: mean absolute RGBA channel error at
most 3, and at most 2% of pixels with maximum channel error over 16. Semantic
interior samples independently check opacity, clip spaces, rotation and effects.
These are prototype probes, not production control/IME acceptance. DPI scaling
is not a physical monitor transition. Explicit context recreation is not actual
DXGI driver/device removal. PPM files retain uncompressed image evidence.

The smaller `cpu.gn` remains available: build target `skia`, then configure the
probe without the text/GPU options. Its disabled shaping, Unicode and GPU modules
make its size unsuitable as a full-SDK estimate.

## Installed ABI consumers

```powershell
cmake --install out/skia-evaluation/probe --prefix out/skia-evaluation/sdk
cmake -S tools/development/skia-spike/consumer -B out/skia-evaluation/consumer -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/absolute/Sunfleck/out/skia-evaluation/sdk
cmake --build out/skia-evaluation/consumer -j 1
# Put the installed sdk/bin on this process’s PATH before running consumers.
ctest --test-dir out/skia-evaluation/consumer --output-on-failure
```

The installed ABI exercises CPU drawing and retained DirectWrite layout. The
separate strict SkParagraph test may fail on an incompatible candidate; packaging
checks do not suppress or reinterpret that failure.

Build and run `consumer/Consumer.csproj` with .NET 10 using a copied project under
`out` to keep generated files isolated. Supply an output directory to the executable.
The only public dependency is `evaluation.h` and its opaque implementation DLL;
no Skia or SkiaSharp types cross the ABI. This demonstrates packaging feasibility,
not installation acceptance of a Skia-enabled Sunfleck SDK.

The installed package contains ICU data next to the DLL and primary dependency
notices, including HarfBuzz’s Microsoft USE data notice and ICU’s complete notices.
Keep the matching MSVC runtime available. Check actual imports with `dumpbin /dependents`.
Production ANGLE and its transitive notices still need a separately pinned package.

## Actual browser-host experiment

`web.gn` uses Sunfleck’s existing Emscripten 3.1.56 with Skia’s pinned FreeType.
The SDK port is only FreeType 2.6.0; the isolated final host links the newer
FreeType for both libraries and removes `USE_FREETYPE` from its final link.
The existing core uses the stable FreeType 2 public ABI; browser tests must
validate this combination. Production build settings and existing port versions are
not replaced; Emscripten can populate its normal cache for this exception ABI. It sets
`is_canvaskit=true` only to select the externally managed static-library build
configuration; it does not build or load the CanvasKit JavaScript runtime. Do not
run this candidate’s `bin/activate-emsdk`, which permanently activates a different
SDK. Copy the GN file to `SKIA_ROOT/out/sunfleck-web/args.gn`, generate with GN, and
run `python tools/development/skia-spike/prepare-web-toolchain.py SKIA_ROOT/out/sunfleck-web`
from Sunfleck after generation, then build `skia` serially. The browser bridge does
not link SkParagraph; its editing compatibility is evaluated separately on Windows.
This changes
only the generated Ninja compiler rules to response files: unquoted FreeType
`<header>` definitions would otherwise be interpreted as Windows shell redirections. The bridge uses Skia/skcms and their pinned FreeType/libpng/zlib dependencies.

Configure `web/CMakeLists.txt` with the existing Emscripten CMake toolchain and:

- `SKIA_ROOT`: fixed checkout.
- `SUNFLECK_ROOT`: current Sunfleck workspace.
- `SUNFLECK_LIBRARY`: a current Wasm core archive built with `UI_WASM_NATIVE_EXCEPTIONS=ON`. Both evaluation hosts use this exception ABI.
- `EVALUATION_FONT`: a locally licensed TTF/OTF file; it is not committed or redistributed.

The resulting site copies the real `web/main.cpp`, `host.js` and text-input host.
Only its copied renderer is replaced. `bridge.cpp` executes the existing 20-byte
rectangle command stream in Skia in the same Wasm memory; it does not implement
M4’s future transform/layer command protocol or change the current text shaper.

`web/prepare-dotnet.py` prepares a copy of the real C# browser application under
`out` with native Skia references and `SkiaBridge.cs`. Supply a Sunfleck core built
with `UI_WASM_NATIVE_EXCEPTIONS=ON`, the bridge archive and the matching Skia archive.
Use the local .NET SDK with its wasm-tools workload. Native compilation must remain
serial, including `EMCC_CORES=1` and `DisableParallelEmccCompile=true`. The
generated project already disables the SDK’s internal parallel native tasks.

For this candidate, the final .NET link also uses the standalone Emscripten SDK
that compiled Skia and the core. Set `EmscriptenSdkToolsPath` and
`EmscriptenUpstreamBinPath` to that SDK's `upstream/` and `upstream/bin/`, and
`EmscriptenUpstreamEmscriptenPath` and `WasmCachePath` to `upstream/emscripten/`
and `upstream/emscripten/cache`. Set `BINARYEN_CORES=1` as well as `EMCC_CORES=1`.
The generated link flags include `-Wl,--threads=1`. Copy the core archive to
`sunfleck_native.a` and the bridge to `skia_bridge.a` before passing their absolute
paths to `prepare-dotnet.py`; these names match the P/Invoke modules.

Serve each generated site over localhost HTTP. `node web/browser.cjs URL EVIDENCE_DIR`
uses the repository’s Playwright dependency (or `SUNFLECK_BROWSER_TEST_MODULES`)
and installed Edge. It compares the real host against its original WebGL
compositor and tests pointer/async commands, text round-tripping, three device
scale factors, resize, context loss/restoration and close. The same Wasm also
compares the shared advanced graphics scene on CPU/WebGL, including actual font
byte transfer into Skia; the linked FreeType version is checked at runtime. The comparison threshold
for the existing integer rectangle stream is maximum channel difference 3. It does
not claim browser SkParagraph/IME correctness or real hardware driver loss.
The local SimHei fixture lacks some emoji/combining glyphs: string round-tripping
is checked separately from glyph coverage. The successful run used Edge's
SwiftShader software WebGL device, not a physical GPU. Failures produce
`result.json` and a screenshot as well as a nonzero exit status.

## Evidence

`collect-evidence.py --root ROOT --skia SKIA_ROOT --angle ANGLE_DIRECTORY --output FILE`
records revisions, dependency pins, build arguments, file sizes, SHA-256 hashes and
serialized build-action time. It does not measure runtime startup, memory or frame
performance. It also records final native executables, browser results, Wasm/JS/font
payloads and the identities of test logs. Preserve those logs alongside the manifest.
The full native test suite intentionally remains nonzero for the current candidate's
SkParagraph raw-hit incompatibility; do not change it to an expected failure or
weaken the grapheme assertion to make the suite green.

## Native Windows D3D12 evaluation

This is the fixed candidate's **Ganesh native Direct3D 12** backend, not
Graphite/Dawn. Keep the original ANGLE build for an equal-scope comparison.
Fetch `d3d12allocator` and `spirv-cross` with `fetch-dependencies.py --names`;
their revisions come from the same DEPS file. Copy `d3d12.gn` to
`SKIA_ROOT/out/sunfleck-d3d12/args.gn`, generate with GN as above, and build target
`skia` with `-j 1`. This configuration disables GL and ANGLE. It statically links
D3D12MemoryAllocator and SPIRV-Cross and uses the system D3DCompile interface.

In an x64 MSVC developer shell:

```powershell
cmake -S tools/development/skia-spike/d3d12 -B out/skia-evaluation/d3d12 -G Ninja -DCMAKE_BUILD_TYPE=Release -DSKIA_ROOT=SKIA_ROOT -DANGLE_DIRECTORY=ANGLE_DIRECTORY
cmake --build out/skia-evaluation/d3d12 -j 1
out/skia-evaluation/d3d12/d3d12_capabilities.exe
ctest --test-dir out/skia-evaluation/d3d12 -j 1 --verbose --output-on-failure
cmake --install out/skia-evaluation/d3d12 --prefix out/skia-evaluation/d3d12-sdk
```

`d3d12_probe --hardware DIRECTORY` requires a non-software adapter and never
silently falls back. `--warp` explicitly tests Microsoft's software D3D12 device.
Both use the same graphics, DirectWrite text and comparison thresholds as the
ANGLE experiment, plus repeated backbuffer presentation and bounded fence waits.
The test covers two windows, three scale factors, resize, minimize/restore,
orderly teardown and explicit device recreation. It does not simulate actual
device removal or physical monitor transitions. Hardware failure stays a failing
test even when WARP succeeds.
When the optional system D3D12 debug layer is available, the probe enables it and
fails on recorded error/corruption messages. Fence waits deliberately serialize
probe frames for correctness checks; this is not a production frame scheduler or
a throughput benchmark.

### Physical GPU retest and local correctness patch

`d3d12-fixes.patch` is a local patch against the fixed Skia revision above.
It queries GPU descriptor handles only for shader-visible heaps and initializes
newly allocated RT/DS resources with D3D12 `DiscardResource`, including scratch
targets and MSAA resolve targets. Creation transitions the resource to RT or
depth-write state, performs discard, then restores the declared initial state.
The allocation remains referenced until command completion. Wrapped resources
and existing contents retain their existing behavior. This patch is part of the
candidate identity; a patched result must not be attributed to unmodified Skia.

For an original-versus-patched comparison, build and run the unmodified library
first, retain that evidence, then apply the patch and rebuild serially:

```powershell
# From Sunfleck, after verifying SKIA_ROOT's exact revision:
git -C SKIA_ROOT apply --check --ignore-space-change D:/absolute/Sunfleck/tools/development/skia-spike/d3d12-fixes.patch
git -C SKIA_ROOT apply --ignore-space-change D:/absolute/Sunfleck/tools/development/skia-spike/d3d12-fixes.patch
ninja -C SKIA_ROOT/out/sunfleck-d3d12 -j 1 skia
cmake --build out/skia-evaluation/d3d12 --target d3d12_probe d3d12_capabilities -j 1
python tools/development/skia-spike/run-d3d12-validation.py --probe-directory out/skia-evaluation/d3d12 --skia SKIA_ROOT --output out/skia-evaluation/d3d12-hardware-fixed --mode hardware
python tools/development/skia-spike/run-d3d12-validation.py --probe-directory out/skia-evaluation/d3d12 --skia SKIA_ROOT --output out/skia-evaluation/d3d12-warp-fixed --mode warp
ctest --test-dir out/skia-evaluation/d3d12 -R '^d3d12_' -j 1 --verbose --output-on-failure
```

Use a new output directory for each runner invocation. It refuses existing
directories, requires the system debug layer and info queues, and retains
adapter LUIDs, driver versions, executable/archive/source SHA-256 identities,
GN arguments, dependency pins, the local D3D12 source diff, logs and `result.json`.
Message IDs and severities are logged; an error/corruption, queue overflow,
timeout or missing completion stage fails the gate. The CTest hardware test also
requires the debug layer. Direct probe invocations can request it with the final
`--require-debug-layer` argument.

The probe additionally verifies discard/full drawing and preservation across a
later partial update, both without MSAA and with requested 4x MSAA. It requires
4x MSAA capability. These are correctness regressions for the local patch.
Building only the two D3D12 targets and selecting `^d3d12_` avoids requiring the
separate ANGLE library and DLLs when retesting a migrated hardware machine.

The equal-scope `angle_comparison` and `d3d12_probe` omit SkParagraph/ICU from
their text sources. Compare each executable's size, plus ANGLE's two matching
DLLs for that route; shared CRT/system dependencies and the final production SDK
must be accounted for separately. The installed D3D12 probe package contains
dependency notices. Run its executable from its bin directory to check that no
unpackaged local runtime is needed. Module paths and dumpbin imports distinguish
system dependencies from application files. Performance acceptance remains deferred.

### Preparing the accepted source candidate

Run `python tools/development/skia-spike/prepare-d3d12.py --skia SKIA_ROOT --apply`
before rebuilding Skia. This checks the exact revision, refuses conflicting tracked
changes, applies the four-file patch, and verifies the resulting contents against
an independently reconstructed reference. Without `--apply` it only verifies.
Repeated preparation is safe. The D3D12 CMake configuration runs this check by
default and saves `source-candidate.json`. This checks source identity, not whether
existing static libraries were rebuilt; rebuild Skia and retain the validation
runner's archive hashes before claiming acceptance.

To reproduce the original failing baseline, explicitly configure
`-DSKIA_ALLOW_UNPATCHED_BASELINE=ON`. ANGLE comparison is now opt-in with
`-DSKIA_EVALUATE_ANGLE=ON`; default builds require only D3D12 libraries. Both
hardware and WARP CTest gates require the debug layer. Older full-comparison
commands above need the ANGLE option to include that third test.
