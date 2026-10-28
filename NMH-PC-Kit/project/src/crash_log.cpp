// No More Heroes PC - crash logger.
// On an unhandled fatal exception, writes logs\crash_<time>.txt next to nmh.exe with the exception
// code, the faulting host address, and a host call stack where every frame that lies inside recompiled
// game code is mapped back to its guest (PowerPC) function address (sub_8XXXXXXX). The ReXGlue runtime
// installs its own first-position vectored handler for guest memory-mapped I/O faults; this one is added
// in last position, so it only sees faults the runtime did not resolve.

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>

#include <rex/ppc/func.h>

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

namespace {

struct HostFunc {
  uintptr_t host;
  uint32_t guest;
};

std::vector<HostFunc>& Table() {
  static std::vector<HostFunc> table = [] {
    std::vector<HostFunc> t;
    for (const PPCFuncMapping* m = PPCFuncMappings; m->host; ++m) {
      t.push_back({reinterpret_cast<uintptr_t>(m->host), static_cast<uint32_t>(m->guest)});
    }
    std::sort(t.begin(), t.end(), [](const HostFunc& a, const HostFunc& b) { return a.host < b.host; });
    return t;
  }();
  return table;
}

// Largest recompiled function whose host entry is <= addr. Returns 0 if none within 1 MB (not game code).
uint32_t GuestFor(uintptr_t addr, uintptr_t* func_host) {
  const auto& t = Table();
  auto it = std::upper_bound(t.begin(), t.end(), addr,
                             [](uintptr_t a, const HostFunc& f) { return a < f.host; });
  if (it == t.begin()) return 0;
  --it;
  if (addr - it->host > (1u << 20)) return 0;
  *func_host = it->host;
  return it->guest;
}

void ModuleName(uintptr_t addr, char* out, size_t n, uintptr_t* base) {
  HMODULE mod = nullptr;
  *base = 0;
  out[0] = 0;
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(addr), &mod) && mod) {
    char path[MAX_PATH];
    DWORD len = GetModuleFileNameA(mod, path, MAX_PATH);
    const char* name = path;
    for (DWORD i = 0; i < len; ++i) if (path[i] == '\\' || path[i] == '/') name = path + i + 1;
    strncpy_s(out, n, name, _TRUNCATE);
    *base = reinterpret_cast<uintptr_t>(mod);
  }
}

const char* CodeName(DWORD code) {
  switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
    case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
    case EXCEPTION_BREAKPOINT: return "BREAKPOINT (runtime rex::debug::Break: guest RtlRaiseException / KeBugCheck / C++ throw)";
    case 0xC0000409: return "STACK_BUFFER_OVERRUN / fail-fast";
    default: return "";
  }
}

bool Fatal(DWORD code) {
  switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_BREAKPOINT:   // rex::debug::Break(): RtlRaiseException / KeBugCheck / C++ throw from guest code
    case 0xC0000409:
      return true;
    default:
      return false;
  }
}

void Describe(FILE* f, const char* label, uintptr_t addr) {
  uintptr_t fh = 0, base = 0;
  char mod[64];
  ModuleName(addr, mod, sizeof mod, &base);
  uint32_t g = GuestFor(addr, &fh);
  if (g) {
    fprintf(f, "%s %p  sub_%08X+0x%llX  (%s+0x%llX)\n", label, (void*)addr, g,
            (unsigned long long)(addr - fh), mod, (unsigned long long)(addr - base));
  } else if (base) {
    fprintf(f, "%s %p  %s+0x%llX\n", label, (void*)addr, mod, (unsigned long long)(addr - base));
  } else {
    fprintf(f, "%s %p  (unknown)\n", label, (void*)addr);
  }
}

std::atomic<int> g_reported{0};

void WriteReport(PEXCEPTION_POINTERS ex, const char* how) {
  if (g_reported.fetch_add(1)) return;  // first fatal exception only
  char exe[MAX_PATH];
  GetModuleFileNameA(nullptr, exe, MAX_PATH);
  char* slash = strrchr(exe, '\\');
  if (slash) slash[1] = 0;
  char dir[MAX_PATH];
  snprintf(dir, sizeof dir, "%slogs", exe);
  CreateDirectoryA(dir, nullptr);
  time_t now = time(nullptr);
  struct tm tmv;
  localtime_s(&tmv, &now);
  char path[MAX_PATH];
  snprintf(path, sizeof path, "%s\\crash_%04d%02d%02d_%02d%02d%02d.txt", dir, tmv.tm_year + 1900, tmv.tm_mon + 1,
           tmv.tm_mday, tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
  FILE* f = nullptr;
  if (fopen_s(&f, path, "w") || !f) return;

  const EXCEPTION_RECORD* r = ex->ExceptionRecord;
  CONTEXT* c = ex->ContextRecord;
  fprintf(f, "No More Heroes PC crash report (%s)\n", how);
  fprintf(f, "exception 0x%08lX %s  thread %lu\n", r->ExceptionCode, CodeName(r->ExceptionCode), GetCurrentThreadId());
  if (r->ExceptionCode == EXCEPTION_ACCESS_VIOLATION || r->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) {
    const char* kind = r->ExceptionInformation[0] == 0 ? "read" : r->ExceptionInformation[0] == 1 ? "write" : "execute";
    fprintf(f, "%s of address %p\n", kind, (void*)r->ExceptionInformation[1]);
    // Guest addresses live in the 4 GB arena at virtual base 0x1_0000_0000: print the guest offset too.
    uintptr_t a = r->ExceptionInformation[1];
    if (a >= 0x100000000ull && a < 0x200000000ull) fprintf(f, "  = guest virtual 0x%08llX\n", (unsigned long long)(a - 0x100000000ull));
  }
  Describe(f, "fault at ", reinterpret_cast<uintptr_t>(r->ExceptionAddress));
  fprintf(f, "rip %p rsp %p rbp %p\nrax %p rbx %p rcx %p rdx %p\nrsi %p rdi %p r8  %p r9  %p\nr10 %p r11 %p r12 %p r13 %p\nr14 %p r15 %p\n",
          (void*)c->Rip, (void*)c->Rsp, (void*)c->Rbp, (void*)c->Rax, (void*)c->Rbx, (void*)c->Rcx, (void*)c->Rdx,
          (void*)c->Rsi, (void*)c->Rdi, (void*)c->R8, (void*)c->R9, (void*)c->R10, (void*)c->R11, (void*)c->R12,
          (void*)c->R13, (void*)c->R14, (void*)c->R15);

  fprintf(f, "\ncall stack (host frames; sub_8XXXXXXX = recompiled guest function):\n");
  HANDLE proc = GetCurrentProcess();
  SymInitialize(proc, nullptr, TRUE);
  CONTEXT ctx = *c;
  STACKFRAME64 sf = {};
  sf.AddrPC.Offset = ctx.Rip;    sf.AddrPC.Mode = AddrModeFlat;
  sf.AddrFrame.Offset = ctx.Rbp; sf.AddrFrame.Mode = AddrModeFlat;
  sf.AddrStack.Offset = ctx.Rsp; sf.AddrStack.Mode = AddrModeFlat;
  for (int i = 0; i < 64; ++i) {
    if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, GetCurrentThread(), &sf, &ctx, nullptr,
                     SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) break;
    if (!sf.AddrPC.Offset) break;
    char label[16];
    snprintf(label, sizeof label, "  #%02d", i);
    Describe(f, label, static_cast<uintptr_t>(sf.AddrPC.Offset));
  }
  fprintf(f, "\nguest functions seen on the stack, innermost first (for codegen/hook work):\n");
  // second pass: compact list of guest addresses only
  ctx = *c;
  sf = {};
  sf.AddrPC.Offset = ctx.Rip;    sf.AddrPC.Mode = AddrModeFlat;
  sf.AddrFrame.Offset = ctx.Rbp; sf.AddrFrame.Mode = AddrModeFlat;
  sf.AddrStack.Offset = ctx.Rsp; sf.AddrStack.Mode = AddrModeFlat;
  for (int i = 0; i < 64; ++i) {
    if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, GetCurrentThread(), &sf, &ctx, nullptr,
                     SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) break;
    if (!sf.AddrPC.Offset) break;
    uintptr_t fh = 0;
    uint32_t g = GuestFor(static_cast<uintptr_t>(sf.AddrPC.Offset), &fh);
    if (g) fprintf(f, "  0x%08X\n", g);
  }
  fclose(f);
}

// abort() path: the ReXGlue runtime's rex_assert()/FatalError() print to stderr (captured in
// logs\stderr.txt below) and call std::abort(), which never reaches an exception handler.
void OnAbort(int) {
  CONTEXT c;
  RtlCaptureContext(&c);
  EXCEPTION_RECORD r = {};
  r.ExceptionCode = 0xC0000409;  // reported as fail-fast
  r.ExceptionAddress = reinterpret_cast<PVOID>(c.Rip);
  EXCEPTION_POINTERS ex = {&r, &c};
  WriteReport(&ex, "abort() - see logs\\stderr.txt for the assertion / fatal error text");
  fflush(stderr);
}

void CaptureStderr() {
  char exe[MAX_PATH];
  GetModuleFileNameA(nullptr, exe, MAX_PATH);
  char* slash = strrchr(exe, '\\');
  if (slash) slash[1] = 0;
  char dir[MAX_PATH];
  snprintf(dir, sizeof dir, "%slogs", exe);
  CreateDirectoryA(dir, nullptr);
  char path[MAX_PATH];
  snprintf(path, sizeof path, "%s\\stderr.txt", dir);
  FILE* f = nullptr;
  if (freopen_s(&f, path, "w", stderr) == 0 && f) setvbuf(stderr, nullptr, _IONBF, 0);
}

// ---- stack overflow: almost no stack is left, so no CRT, no StackWalk64, no allocation. ----
HANDLE g_so_file = INVALID_HANDLE_VALUE;  // pre-opened logs\crash_stackoverflow.txt
char g_so_buf[4096];

size_t PutStr(char* d, size_t n, const char* s) { size_t i = 0; while (s[i] && i + 1 < n) { d[i] = s[i]; ++i; } return i; }
size_t PutHex(char* d, size_t n, uint64_t v, int digits) {
  static const char* hx = "0123456789ABCDEF";
  size_t i = 0;
  for (int k = digits - 1; k >= 0 && i + 1 < n; --k) d[i++] = hx[(v >> (k * 4)) & 15];
  return i;
}
void OpenStackOverflowFile() {
  char exe[MAX_PATH];
  GetModuleFileNameA(nullptr, exe, MAX_PATH);
  char* slash = strrchr(exe, '\\');
  if (slash) slash[1] = 0;
  char path[MAX_PATH];
  snprintf(path, sizeof path, "%slogs\\crash_stackoverflow.txt", exe);
  g_so_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
  // Leave the file empty unless a stack overflow happens (an empty file = no overflow occurred).
}
void StackOverflowReport(PEXCEPTION_POINTERS ex) {
  if (g_so_file == INVALID_HANDLE_VALUE) return;
  char* b = g_so_buf; size_t n = sizeof g_so_buf, i = 0;
  const CONTEXT* c = ex->ContextRecord;
  i += PutStr(b + i, n - i, "STACK_OVERFLOW thread ");
  i += PutHex(b + i, n - i, GetCurrentThreadId(), 8);
  i += PutStr(b + i, n - i, " rip ");
  i += PutHex(b + i, n - i, c->Rip, 16);
  i += PutStr(b + i, n - i, " rsp ");
  i += PutHex(b + i, n - i, c->Rsp, 16);
  uintptr_t fh = 0;
  uint32_t g = GuestFor(static_cast<uintptr_t>(c->Rip), &fh);
  if (g) { i += PutStr(b + i, n - i, " in sub_"); i += PutHex(b + i, n - i, g, 8); }
  i += PutStr(b + i, n - i, "\r\nguest functions found on the stack (conservative scan from rsp, innermost first):\r\n");
  // Scan the dead stack for return addresses inside recompiled code. Most are real frames; a few
  // may be stale values. Consecutive duplicates are collapsed so a recursion shows as a short loop.
  const uintptr_t* sp = reinterpret_cast<const uintptr_t*>(c->Rsp & ~uintptr_t(7));
  uint32_t last = 0; int shown = 0; int repeats = 0;
  for (size_t k = 0; k < (2u << 20) / sizeof(uintptr_t) && shown < 400 && i + 64 < n; ++k) {
    uintptr_t v;
    if (IsBadReadPtr(sp + k, sizeof v)) break;
    v = sp[k];
    uintptr_t f = 0; uint32_t gg = GuestFor(v, &f);
    if (!gg) continue;
    if (gg == last) { ++repeats; continue; }
    if (repeats) { i += PutStr(b + i, n - i, "    (x"); i += PutHex(b + i, n - i, repeats + 1, 4); i += PutStr(b + i, n - i, ")\r\n"); repeats = 0; }
    i += PutStr(b + i, n - i, "  0x"); i += PutHex(b + i, n - i, gg, 8); i += PutStr(b + i, n - i, "\r\n");
    last = gg; ++shown;
    if (i + 64 >= n) { DWORD w; WriteFile(g_so_file, b, DWORD(i), &w, nullptr); i = 0; }
  }
  DWORD w;
  WriteFile(g_so_file, b, DWORD(i), &w, nullptr);
  FlushFileBuffers(g_so_file);
}

// Helper thread with a healthy stack: the overflowing thread only stores the exception pointers,
// signals, and waits (a few hundred bytes of stack), while this thread writes the full report.
HANDLE g_helper_req = nullptr, g_helper_done = nullptr;
PEXCEPTION_POINTERS g_helper_ex = nullptr;
DWORD WINAPI HelperThread(LPVOID) {
  for (;;) {
    WaitForSingleObject(g_helper_req, INFINITE);
    PEXCEPTION_POINTERS ex = g_helper_ex;
    if (ex) {
      StackOverflowReport(ex);       // minimal, always works
      g_reported.store(0);
      WriteReport(ex, "stack overflow (written by the helper thread)");  // full host/guest stack walk
    }
    SetEvent(g_helper_done);
  }
  return 0;
}

LONG CALLBACK Vectored(PEXCEPTION_POINTERS ex) {
  const DWORD code = ex->ExceptionRecord->ExceptionCode;
  if (code == EXCEPTION_STACK_OVERFLOW) {
    if (!g_reported.fetch_add(1)) {
      if (g_helper_req && g_helper_done) {
        g_helper_ex = ex;
        SetEvent(g_helper_req);
        WaitForSingleObject(g_helper_done, 15000);
      } else {
        StackOverflowReport(ex);
      }
    }
    return EXCEPTION_CONTINUE_SEARCH;
  }
  if (Fatal(code)) WriteReport(ex, "vectored handler");
  return EXCEPTION_CONTINUE_SEARCH;
}

LONG CALLBACK Unhandled(PEXCEPTION_POINTERS ex) {
  WriteReport(ex, "unhandled exception filter");
  return EXCEPTION_CONTINUE_SEARCH;
}

struct Installer {
  Installer() {
    Table();  // build the host->guest table now, not inside the crash handler
    CaptureStderr();
    OpenStackOverflowFile();
    g_helper_req = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_helper_done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    HANDLE t = CreateThread(nullptr, 4 << 20, HelperThread, nullptr, 0, nullptr);
    if (t) CloseHandle(t); else { g_helper_req = g_helper_done = nullptr; }
    AddVectoredExceptionHandler(0 /* last */, Vectored);
    SetUnhandledExceptionFilter(Unhandled);
    signal(SIGABRT, OnAbort);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  }
} g_installer;

}  // namespace
#endif  // _WIN32
