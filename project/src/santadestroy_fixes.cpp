// Santa Destroy Recomp - behaviour fixes that override ReXGlue kernel exports.
//
// The generated code refers to kernel imports as plain extern "C" symbols (e.g.
// __imp__XamUserGetSigninState) that normally come from rexruntime.lib. A definition in the game
// project wins at link time (libraries are only searched for symbols that are still undefined), so
// this file replaces individual kernel functions without rebuilding the SDK - the same trick as
// fast_fma.cpp.
//
// FIX (Xbox LIVE sign-in prompt, 2026-10-20): the runtime reports the local profile as
// XUSER_SIGNIN_STATE_SIGNED_IN_LOCALLY (1). The game polls XamUserGetSigninState for users 0-3
// (callers 0x82514310, 0x8256E8F8 via thunk 0x8263C288); state 2 sets both of its profile masks
// (+0xCC and +0xD0), state 1 only one, and the missing one makes it ask to sign in to Xbox LIVE.
// The title imports no XOnline/XUser networking calls, so reporting SIGNED_IN_TO_LIVE (2) is safe.
// Set SDR_LOCAL_SIGNIN=1 in the environment to restore the original state 1.

#include "generated/default/santadestroy_init.h"

#include <rex/system/kernel_state.h>
#include <rex/system/xam/user_profile.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr uint32_t kSignedInToLive = 2;
constexpr uint32_t kSignedInLocally = 1;
constexpr uint32_t kErrNoSuchUser = 0x000004DD;   // X_E_NO_SUCH_USER / ERROR_NO_SUCH_USER
constexpr uint32_t kErrInvalidArg = 0x80070057;   // X_E_INVALIDARG

uint32_t SigninState() {
  static const bool local = std::getenv("SDR_LOCAL_SIGNIN") != nullptr;
  return local ? kSignedInLocally : kSignedInToLive;
}

// Guest X_USER_SIGNIN_INFO (40 bytes, big-endian fields).
void PutBe32(uint8_t* p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
void PutBe64(uint8_t* p, uint64_t v) { PutBe32(p, uint32_t(v >> 32)); PutBe32(p + 4, uint32_t(v)); }

}  // namespace

// u32 XamUserGetSigninState(u32 user_index)
extern "C" REX_FUNC(__imp__XamUserGetSigninState) {
  (void)base;
  ctx.r3.u64 = ctx.r3.u32 == 0 ? SigninState() : 0;
}

// i32 XamUserGetSigninInfo(u32 user_index, u32 flags, X_USER_SIGNIN_INFO* info)
extern "C" REX_FUNC(__imp__XamUserGetSigninInfo) {
  const uint32_t user_index = ctx.r3.u32;
  const uint32_t info_ptr = ctx.r5.u32;
  if (!info_ptr) { ctx.r3.u64 = kErrInvalidArg; return; }
  uint8_t* info = base + info_ptr;
  std::memset(info, 0, 40);
  if (user_index != 0) { ctx.r3.u64 = kErrNoSuchUser; return; }
  const auto* profile = REX_KERNEL_STATE()->user_profile();
  PutBe64(info + 0x00, profile->xuid());
  PutBe32(info + 0x0C, SigninState());
  const std::string name = profile->name();
  std::strncpy(reinterpret_cast<char*>(info + 0x18), name.c_str(), 15);
  ctx.r3.u64 = 0;  // X_E_SUCCESS
}

// ---------------------------------------------------------------------------------------------
// DIAGNOSTIC: The game ends the title (fatal error / exit path) through XamLoaderTerminateTitle or
// XamLoaderLaunchTitle(NULL). The runtime then stops all guest threads and exits with only a
// debug-level log line, which looks exactly like a crash. Record who asked, with a guest call stack,
// in logs\crash_*.txt (reuses the crash logger via abort() reporting), then terminate as before.
#include <csignal>

namespace {
void ReportTitleExit(const char* via) {
  std::fprintf(stderr, "Game requested exit via %s (game fatal error / exit request)\n", via);
  std::raise(SIGABRT);  // crash_log.cpp's SIGABRT handler writes the report and returns
}
}  // namespace

extern "C" REX_FUNC(__imp__XamLoaderTerminateTitle) {
  (void)base;
  ReportTitleExit("XamLoaderTerminateTitle");
  REX_KERNEL_STATE()->TerminateTitle();
}

extern "C" REX_FUNC(__imp__XamLoaderLaunchTitle) {
  (void)base;
  ReportTitleExit(ctx.r3.u32 ? "XamLoaderLaunchTitle(path)" : "XamLoaderLaunchTitle(NULL = dashboard)");
  REX_KERNEL_STATE()->TerminateTitle();
}

// ---------------------------------------------------------------------------------------------
// CURSOR UNTIL FIRST SPACE / ENTER (2026-10-20): the first-boot "no save data" dialog is a clickable runtime dialog,
// but mouse look captures and hides the cursor from the first frame. Start with mouse look off (cursor
// visible, free) and turn it on at the first press of Space or Enter (keyboard A) or pad A while the game
// window is focused. SDR_CAPTURE_MOUSE_AT_START=1 restores immediate capture.
#include <rex/input/mnk/mnk_input_driver.h>
#include <thread>
#include <windows.h>
#include <xinput.h>

namespace {
struct CursorUntilFirstStart {
  CursorUntilFirstStart() {
    if (std::getenv("SDR_CAPTURE_MOUSE_AT_START") != nullptr) return;
    rex::input::mnk::SetMouseLookActive(false);
    std::thread([] {
      using GetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
      HMODULE xi = LoadLibraryW(L"xinput1_4.dll");
      if (!xi) xi = LoadLibraryW(L"xinput9_1_0.dll");
      auto get_state = xi ? reinterpret_cast<GetStateFn>(GetProcAddress(xi, "XInputGetState")) : nullptr;
      for (;;) {
        DWORD pid = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &pid);
        bool pressed = false;
        if (pid == GetCurrentProcessId()) {
          pressed = (GetAsyncKeyState(VK_SPACE) & 0x8000) || (GetAsyncKeyState(VK_RETURN) & 0x8000);
          for (DWORD i = 0; !pressed && get_state && i < 4; ++i) {
            XINPUT_STATE st{};
            if (get_state(i, &st) == ERROR_SUCCESS && (st.Gamepad.wButtons & XINPUT_GAMEPAD_A)) pressed = true;
          }
        }
        if (pressed) {
          rex::input::mnk::SetMouseLookActive(true);
          std::fprintf(stderr, "SDR: first Space/Enter/A press, mouse look captured\n");
          return;
        }
        Sleep(16);
      }
    }).detach();
  }
} g_cursor_until_first_start;
}  // namespace
