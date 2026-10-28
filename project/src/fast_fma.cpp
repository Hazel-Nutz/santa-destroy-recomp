// Fast fma for CPUs without FMA3 (e.g. Ivy Bridge i5-3470).
//
// The recompiler turns every PowerPC fmadd/fmadds/fmsub/fnmadd/... into std::fma(). With FMA3 hardware
// clang inlines that as one vfmadd instruction and this file is unused. Without it, clang emits a call to
// the C runtime's fma(), which ucrtbase implements as an exact software routine costing ~100+ cycles
// (measured in another ReXGlue port: 74% of the audio mixer thread).
//
// Defining fma here makes the linker bind the game's calls to this unfused multiply-add instead of the
// CRT import. No headers on purpose: <math.h> declares fma dllimport, which would conflict.
extern "C" double fma(double a, double b, double c) { return a * b + c; }
extern "C" float fmaf(float a, float b, float c) { return static_cast<float>(static_cast<double>(a) * b + c); }
