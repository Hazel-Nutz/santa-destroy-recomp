// No More Heroes (recompiled) launcher: key/mouse rebinding (keyboard+mouse act
// as an Xbox 360 pad, real XInput pads keep working at the same time), mouse camera tuning,
// display settings, a live input test, and Play. Writes nmh.toml (read by nmh.exe) and
// nmh_launcher.ini. Colours are taken from the game's cover palette (hot pink, magenta, cyan, yellow on black).
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>  // provides WinMain for the WIN32 (GUI) build
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "nmh_bindings.h"
#include "nmh_emblem.h"

namespace fs = std::filesystem;

#if defined(_WIN32)
static const char* kGameExe = "nmh.exe";
#else
static const char* kGameExe = "nmh";
#endif
static const char* kGameToml = "nmh.toml";

// ---------------------------------------------------------------------------------------------
// Black / blood-red palette taken from the star emblem (nmh_emblem.h): red-tinted blacks, deep and
// bright emblem reds, bone-white text. Names are kept from the earlier kits; values are the red theme.
static ImVec4 Rgb(int hex, float a = 1.0f) {
  return ImVec4(((hex >> 16) & 0xFF) / 255.0f, ((hex >> 8) & 0xFF) / 255.0f, (hex & 0xFF) / 255.0f, a);
}
namespace pal {
constexpr int kCharcoal = 0x0A0303,   // window background (red-tinted black)
              kPanel = 0x150606,      // panels, popups, alternate rows
              kFrame = 0x231010,      // input fields, idle buttons
              kOlive = 0xA00000,      // main accent (selected tab, borders)
              kOliveLight = 0xE01010, // bright emblem red (hover, sliders, header rule)
              kLime = 0xF2E8E8,       // check marks, "found"
              kRust = 0x4A0000,       // deep blood red (title bars, header gradient)
              kRustLight = 0xE01010,  // hover red
              kTan = 0x8C6A6A,        // muted text
              kCream = 0xF2E8E8,      // bone-white text
              kSand = 0xFF3B3B;       // warnings / conflicts / subtitle
}

static void ApplyTheme() {
  ImGui::StyleColorsDark();
  ImGuiStyle& st = ImGui::GetStyle();
  st.WindowRounding = 0;  // hard edges suit the emblem
  st.FrameRounding = 0;
  st.GrabRounding = 0;
  st.TabRounding = 0;
  st.ScrollbarRounding = 0;
  st.FrameBorderSize = 1;
  st.TabBorderSize = 1;
  st.ItemSpacing = ImVec2(8, 6);
  ImVec4* c = st.Colors;
  c[ImGuiCol_Text] = Rgb(pal::kCream);
  c[ImGuiCol_TextDisabled] = Rgb(pal::kTan);
  c[ImGuiCol_WindowBg] = Rgb(pal::kCharcoal);
  c[ImGuiCol_ChildBg] = Rgb(pal::kCharcoal);
  c[ImGuiCol_PopupBg] = Rgb(pal::kPanel, 0.98f);
  c[ImGuiCol_Border] = Rgb(pal::kOlive, 0.45f);
  c[ImGuiCol_FrameBg] = Rgb(pal::kFrame);
  c[ImGuiCol_FrameBgHovered] = Rgb(pal::kRust);
  c[ImGuiCol_FrameBgActive] = Rgb(pal::kOlive, 0.8f);
  c[ImGuiCol_TitleBg] = Rgb(pal::kPanel);
  c[ImGuiCol_TitleBgActive] = Rgb(pal::kRust);
  c[ImGuiCol_CheckMark] = Rgb(pal::kLime);
  c[ImGuiCol_SliderGrab] = Rgb(pal::kOliveLight);
  c[ImGuiCol_SliderGrabActive] = Rgb(pal::kLime);
  c[ImGuiCol_Button] = Rgb(pal::kFrame);
  c[ImGuiCol_ButtonHovered] = Rgb(pal::kOlive);
  c[ImGuiCol_ButtonActive] = Rgb(pal::kOliveLight);
  c[ImGuiCol_Header] = Rgb(pal::kRust);
  c[ImGuiCol_HeaderHovered] = Rgb(pal::kOlive, 0.8f);
  c[ImGuiCol_HeaderActive] = Rgb(pal::kOliveLight);
  c[ImGuiCol_Separator] = Rgb(pal::kOlive, 0.6f);
  c[ImGuiCol_Tab] = Rgb(pal::kPanel);
  c[ImGuiCol_TabHovered] = Rgb(pal::kRust);
  c[ImGuiCol_TabDimmed] = Rgb(pal::kPanel);
  c[ImGuiCol_TextSelectedBg] = Rgb(pal::kOlive, 0.6f);
  c[ImGuiCol_NavCursor] = Rgb(pal::kOliveLight);
  c[ImGuiCol_TabSelected] = Rgb(pal::kOlive);
  c[ImGuiCol_TabSelectedOverline] = Rgb(pal::kOliveLight);
  c[ImGuiCol_TableHeaderBg] = Rgb(pal::kPanel);
  c[ImGuiCol_TableRowBg] = Rgb(pal::kCharcoal, 0.0f);
  c[ImGuiCol_TableRowBgAlt] = Rgb(pal::kPanel, 0.8f);
  c[ImGuiCol_PlotHistogram] = Rgb(pal::kRustLight);
  c[ImGuiCol_ModalWindowDimBg] = Rgb(0x000000, 0.55f);
  c[ImGuiCol_ScrollbarBg] = Rgb(pal::kCharcoal);
  c[ImGuiCol_ScrollbarGrab] = Rgb(pal::kRust);
  c[ImGuiCol_ScrollbarGrabHovered] = Rgb(pal::kOliveLight);
}

// ---------------------------------------------------------------------------------------------
// Settings
struct Settings {
  std::map<std::string, std::array<std::string, 2>> binds;
  bool mouse_look = true;
  float sensitivity = 2.0f;
  bool invert_y = false;
  float deadzone_comp = 0.18f;
  float curve = 1.0f;
  float smoothing = 0.0f;
  int window_mode = 1;  // 0 windowed, 1 borderless fullscreen, 2 exclusive fullscreen
  int width = 1280, height = 720;
  bool vsync = false;
  int fps_cap = 0;  // 0 = off, 30, 60 (applied in the game's present)
  int render_scale = 1;
  int monitor = 0;
  std::string game_dir = "assets";
  std::string update_dir = "update";
  std::string user_dir = "user";
  bool close_on_play = true;
};

static void ResetBinds(Settings& s) {
  s.binds.clear();
  for (auto& a : lc::Actions()) s.binds[a.id] = {a.defaults[0], a.defaults[1]};
}

static fs::path BaseDir() {
  const char* b = SDL_GetBasePath();
  return b ? fs::path(b) : fs::current_path();
}

static std::string Trim(std::string s) {
  auto ws = [](unsigned char c) { return std::isspace(c); };
  while (!s.empty() && ws(s.back())) s.pop_back();
  size_t i = 0;
  while (i < s.size() && ws(s[i])) ++i;
  return s.substr(i);
}

// nmh_launcher.ini: simple key=value. bind.<id>=K1|K2
static void LoadSettings(Settings& s, const fs::path& ini) {
  ResetBinds(s);
  std::ifstream f(ini);
  std::string line;
  while (std::getline(f, line)) {
    auto eq = line.find('=');
    if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;
    std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
    auto f2 = [&](float& o) { o = std::strtof(v.c_str(), nullptr); };
    auto i2 = [&](int& o) { o = std::atoi(v.c_str()); };
    auto b2 = [&](bool& o) { o = (v == "1" || v == "true"); };
    if (k.rfind("bind.", 0) == 0) {
      std::string id = k.substr(5);
      if (!s.binds.count(id)) continue;
      auto bar = v.find('|');
      s.binds[id] = {Trim(v.substr(0, bar)), bar == std::string::npos ? "" : Trim(v.substr(bar + 1))};
    } else if (k == "mouse_look") b2(s.mouse_look);
    else if (k == "sensitivity") f2(s.sensitivity);
    else if (k == "invert_y") b2(s.invert_y);
    else if (k == "deadzone_comp") f2(s.deadzone_comp);
    else if (k == "curve") f2(s.curve);
    else if (k == "smoothing") f2(s.smoothing);
    else if (k == "window_mode") i2(s.window_mode);
    else if (k == "width") i2(s.width);
    else if (k == "height") i2(s.height);
    else if (k == "vsync") b2(s.vsync);
    else if (k == "fps_cap") { i2(s.fps_cap); if (s.fps_cap != 30 && s.fps_cap != 60) s.fps_cap = 0; }
    else if (k == "render_scale") i2(s.render_scale);
    else if (k == "monitor") i2(s.monitor);
    else if (k == "game_dir") s.game_dir = v;
    else if (k == "update_dir") s.update_dir = v;
    else if (k == "user_dir") s.user_dir = v;
    else if (k == "close_on_play") b2(s.close_on_play);
  }
}

static void SaveSettings(const Settings& s, const fs::path& ini) {
  std::ofstream f(ini);
  f << "# No More Heroes launcher settings (edit in the launcher)\n";
  for (auto& a : lc::Actions()) {
    auto& b = s.binds.at(a.id);
    f << "bind." << a.id << "=" << b[0] << "|" << b[1] << "\n";
  }
  f << "mouse_look=" << s.mouse_look << "\nsensitivity=" << s.sensitivity << "\ninvert_y=" << s.invert_y
    << "\ndeadzone_comp=" << s.deadzone_comp << "\ncurve=" << s.curve << "\nsmoothing=" << s.smoothing
    << "\nwindow_mode=" << s.window_mode << "\nwidth=" << s.width << "\nheight=" << s.height
    << "\nvsync=" << s.vsync << "\nfps_cap=" << s.fps_cap << "\nrender_scale=" << s.render_scale << "\nmonitor=" << s.monitor
    << "\ngame_dir=" << s.game_dir << "\nupdate_dir=" << s.update_dir << "\nuser_dir=" << s.user_dir
    << "\nclose_on_play=" << s.close_on_play << "\n";
}

static fs::path Resolve(const std::string& p) {
  fs::path q(p);
  return q.is_absolute() ? q : (BaseDir() / q).lexically_normal();
}

// Writes the managed cvars into nmh.toml, keeping any other lines the user added.
static std::string WriteGameConfig(const Settings& s, const fs::path& toml, const std::string& game_dir) {
  std::map<std::string, std::string> kv;
  // Aggregate action keys per pad input (combo actions feed two inputs).
  std::array<std::vector<std::string>, lc::kPadCount> per_pad;
  for (auto& a : lc::Actions()) {
    for (auto& key : s.binds.at(a.id)) {
      if (key.empty()) continue;
      for (auto p : a.pad) {
        auto& v = per_pad[p];
        if (std::find(v.begin(), v.end(), key) == v.end()) v.push_back(key);
      }
    }
  }
  for (int p = 0; p < lc::kPadCount; ++p) {
    std::string joined;
    for (auto& k : per_pad[p]) joined += (joined.empty() ? "" : ",") + k;
    kv[lc::kPad[p].cvar] = "\"" + joined + "\"";
  }
  char buf[64];
  auto num = [&](double v) { std::snprintf(buf, sizeof buf, "%.3f", v); return std::string(buf); };
  kv["mnk_mode"] = "true";
  kv["mnk_mouse"] = s.mouse_look ? "true" : "false";
  kv["mnk_sensitivity"] = num(s.sensitivity);
  kv["mnk_invert_y"] = s.invert_y ? "true" : "false";
  kv["mnk_deadzone_comp"] = num(s.deadzone_comp);
  kv["mnk_curve"] = num(s.curve);
  kv["mnk_smoothing"] = num(s.smoothing);
  kv["fullscreen"] = s.window_mode != 0 ? "true" : "false";
  kv["fullscreen_exclusive"] = s.window_mode == 2 ? "true" : "false";
  kv["window_width"] = std::to_string(s.width);
  kv["window_height"] = std::to_string(s.height);
  kv["vsync"] = s.vsync ? "true" : "false";
  kv["frame_rate_limit"] = std::to_string(s.fps_cap);
  kv["resolution_scale"] = std::to_string(std::clamp(s.render_scale, 1, 3));
  kv["monitor"] = std::to_string(s.monitor);
  kv["gpu_plugin"] = "\"xenos\"";
  // TOML basic strings with escaping: a single-quoted literal breaks on paths containing an
  // apostrophe ("No More Heroes Heroes' Paradise").
  auto toml_path = [](const fs::path& p) {
    std::string out = "\"";
    for (char ch : p.string()) {
      if (ch == '\\' || ch == '"') out += '\\';
      out += ch;
    }
    return out + "\"";
  };
  kv["game_data_root"] = toml_path(Resolve(game_dir));
  kv["update_data_root"] = toml_path(Resolve(s.update_dir));
  kv["user_data_root"] = toml_path(Resolve(s.user_dir));

  std::vector<std::string> kept;
  {
    std::ifstream in(toml);
    std::string line;
    while (std::getline(in, line)) {
      for (size_t b; (b = line.find("\xEF\xBB\xBF")) != std::string::npos;) line.erase(b, 3);
      std::string t = Trim(line);
      auto eq = t.find('=');
      if (t.rfind("# No More Heroes launcher", 0) == 0) continue;
      if (eq != std::string::npos && kv.count(Trim(t.substr(0, eq)))) continue;
      kept.push_back(line);
    }
  }
  std::ofstream out(toml);
  if (!out) return "Could not write " + toml.string();
  out << "# No More Heroes launcher managed settings (regenerated on Save; other lines are kept)\n";
  for (auto& [k, v] : kv) out << k << " = " << v << "\n";
  for (auto& l : kept)
    if (!Trim(l).empty()) out << l << "\n";
  return "";
}

// ---------------------------------------------------------------------------------------------
// Key names (must match ReXGlue's ParseVirtualKey names)
static std::string ScancodeName(SDL_Scancode sc) {
  if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) return std::string(1, char('A' + (sc - SDL_SCANCODE_A)));
  if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) return std::string(1, char('1' + (sc - SDL_SCANCODE_1)));
  if (sc == SDL_SCANCODE_0) return "0";
  if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12) return "F" + std::to_string(1 + sc - SDL_SCANCODE_F1);
  if (sc >= SDL_SCANCODE_F13 && sc <= SDL_SCANCODE_F24) return "F" + std::to_string(13 + sc - SDL_SCANCODE_F13);
  if (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_9) return "Numpad" + std::to_string(1 + sc - SDL_SCANCODE_KP_1);
  switch (sc) {
    case SDL_SCANCODE_KP_0: return "Numpad0";
    case SDL_SCANCODE_SPACE: return "Space";
    case SDL_SCANCODE_RETURN: return "Return";
    case SDL_SCANCODE_ESCAPE: return "Escape";
    case SDL_SCANCODE_TAB: return "Tab";
    case SDL_SCANCODE_BACKSPACE: return "Backspace";
    case SDL_SCANCODE_DELETE: return "Delete";
    case SDL_SCANCODE_INSERT: return "Insert";
    case SDL_SCANCODE_HOME: return "Home";
    case SDL_SCANCODE_END: return "End";
    case SDL_SCANCODE_PAGEUP: return "PageUp";
    case SDL_SCANCODE_PAGEDOWN: return "PageDown";
    case SDL_SCANCODE_LEFT: return "Left";
    case SDL_SCANCODE_RIGHT: return "Right";
    case SDL_SCANCODE_UP: return "Up";
    case SDL_SCANCODE_DOWN: return "Down";
    case SDL_SCANCODE_LSHIFT: case SDL_SCANCODE_RSHIFT: return "Shift";
    case SDL_SCANCODE_LCTRL: case SDL_SCANCODE_RCTRL: return "Control";
    case SDL_SCANCODE_LALT: case SDL_SCANCODE_RALT: return "Alt";
    case SDL_SCANCODE_GRAVE: return "Backtick";
    case SDL_SCANCODE_MINUS: return "Minus";
    case SDL_SCANCODE_EQUALS: return "Plus";
    case SDL_SCANCODE_COMMA: return "Comma";
    case SDL_SCANCODE_PERIOD: return "Period";
    case SDL_SCANCODE_SEMICOLON: return "Semicolon";
    case SDL_SCANCODE_SLASH: return "Slash";
    case SDL_SCANCODE_BACKSLASH: return "Backslash";
    case SDL_SCANCODE_LEFTBRACKET: return "LBracket";
    case SDL_SCANCODE_RIGHTBRACKET: return "RBracket";
    case SDL_SCANCODE_APOSTROPHE: return "Quote";
    case SDL_SCANCODE_KP_ENTER: return "NumpadEnter";
    case SDL_SCANCODE_KP_PLUS: return "NumpadPlus";
    case SDL_SCANCODE_KP_MINUS: return "NumpadMinus";
    case SDL_SCANCODE_KP_MULTIPLY: return "NumpadStar";
    case SDL_SCANCODE_KP_DIVIDE: return "NumpadSlash";
    case SDL_SCANCODE_PRINTSCREEN: return "PrintScreen";
    case SDL_SCANCODE_PAUSE: return "Pause";
    case SDL_SCANCODE_CAPSLOCK: return "CapsLock";
    case SDL_SCANCODE_NUMLOCKCLEAR: return "NumLock";
    case SDL_SCANCODE_SCROLLLOCK: return "ScrollLock";
    default: return "";
  }
}

static std::string MouseButtonName(int b) {
  switch (b) {
    case SDL_BUTTON_LEFT: return "LMB";
    case SDL_BUTTON_RIGHT: return "RMB";
    case SDL_BUTTON_MIDDLE: return "MMB";
    case SDL_BUTTON_X1: return "Mouse4";
    case SDL_BUTTON_X2: return "Mouse5";
    default: return "";
  }
}

// Friendly text for a bind name.
static std::string Pretty(const std::string& k) {
  static const std::map<std::string, std::string> m = {
      {"LMB", "Left Mouse"}, {"RMB", "Right Mouse"}, {"MMB", "Middle Mouse"},
      {"Mouse4", "Mouse 4"}, {"Mouse5", "Mouse 5"}, {"MWheelUp", "Wheel Up"},
      {"MWheelDown", "Wheel Down"}, {"Control", "Ctrl"}, {"Escape", "Esc"}};
  auto it = m.find(k);
  return it != m.end() ? it->second : k;
}

// ---------------------------------------------------------------------------------------------
// Live input test: evaluates the same bindings the game will use.
struct LiveInput {
  Uint64 wheel_up_until = 0, wheel_down_until = 0;
  float mouse_dx = 0, mouse_dy = 0;
};

static bool NamePressed(const std::string& name, const LiveInput& li) {
  if (name.empty()) return false;
  Uint64 now = SDL_GetTicks();
  if (name == "MWheelUp") return now < li.wheel_up_until;
  if (name == "MWheelDown") return now < li.wheel_down_until;
  SDL_MouseButtonFlags mb = SDL_GetMouseState(nullptr, nullptr);
  if (name == "LMB") return mb & SDL_BUTTON_LMASK;
  if (name == "RMB") return mb & SDL_BUTTON_RMASK;
  if (name == "MMB") return mb & SDL_BUTTON_MMASK;
  if (name == "Mouse4") return mb & SDL_BUTTON_X1MASK;
  if (name == "Mouse5") return mb & SDL_BUTTON_X2MASK;
  int n = 0;
  const bool* ks = SDL_GetKeyboardState(&n);
  for (int sc = 0; sc < n; ++sc)
    if (ks[sc] && ScancodeName(SDL_Scancode(sc)) == name) return true;
  return false;
}

// ---------------------------------------------------------------------------------------------
struct Capture {
  std::string action;
  int slot = -1;
  ImVec2 box_min{}, box_max{};
};

static void SetBind(Settings& s, const std::string& id, int slot, const std::string& name) {
  auto& b = s.binds[id];
  b[slot] = name;
  if (!name.empty() && b[1 - slot] == name) b[1 - slot].clear();
}

int main(int, char**) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window* window = SDL_CreateWindow("No More Heroes - Launcher", 1100, 760,
                                        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
  if (!renderer) {
    std::fprintf(stderr, "Hardware renderer unavailable (%s); using software\n", SDL_GetError());
    renderer = SDL_CreateRenderer(window, SDL_SOFTWARE_RENDERER);
  }
  if (!renderer) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "No More Heroes Launcher", SDL_GetError(), window);
    return 1;
  }
  std::fprintf(stderr, "Renderer: %s\n", SDL_GetRendererName(renderer));
  SDL_SetRenderVSync(renderer, 1);

  // Emblem: window icon + header texture (64x64 RGBA from nmh_emblem.h).
  SDL_Texture* emblem = nullptr;
  if (SDL_Surface* em = SDL_CreateSurfaceFrom(kEmblemSize, kEmblemSize, SDL_PIXELFORMAT_ABGR8888,
                                              const_cast<unsigned char*>(kEmblemRGBA), kEmblemSize * 4)) {
    SDL_SetWindowIcon(window, em);
    emblem = SDL_CreateTextureFromSurface(renderer, em);
    if (emblem) SDL_SetTextureScaleMode(emblem, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(em);
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  ApplyTheme();
  ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer3_Init(renderer);

  const fs::path ini = BaseDir() / "nmh_launcher.ini";
  Settings s;
  LoadSettings(s, ini);
  if (!fs::exists(ini)) {
    // First run: start at the desktop resolution.
    if (const SDL_DisplayMode* dm = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay())) {
      s.width = dm->w;
      s.height = dm->h;
    }
  }

  // Resolution list from the primary display.
  std::vector<std::pair<int, int>> resolutions;
  {
    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(SDL_GetPrimaryDisplay(), &count);
    std::set<std::pair<int, int>> uniq;
    for (int i = 0; i < count; ++i) uniq.insert({modes[i]->w, modes[i]->h});
    SDL_free(modes);
    for (auto r : {std::pair{1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}}) uniq.insert(r);
    resolutions.assign(uniq.rbegin(), uniq.rend());
  }

  Capture cap;
  LiveInput live;
  std::string status;
  Uint64 status_until = 0;
  bool dirty = false;
  bool running = true;
  std::vector<SDL_Gamepad*> pads;

  auto flash = [&](const std::string& m) { status = m; status_until = SDL_GetTicks() + 4000; };
  auto save_all = [&]() -> bool {
    SaveSettings(s, ini);
    std::string err = WriteGameConfig(s, BaseDir() / kGameToml, s.game_dir);
    if (!err.empty()) { flash(err); return false; }
    dirty = false;
    flash("Saved nmh.toml and nmh_launcher.ini");
    return true;
  };

  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      bool swallow = false;
      if (!cap.action.empty()) {
        if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
          std::string n = ScancodeName(e.key.scancode);
          if (n == "Control" || n == "Alt") {
            // The runtime matches modifiers exactly: a bare modifier bind never fires, and holding
            // one mutes every other bind. Refuse it here instead of producing a dead key.
            flash("Ctrl / Alt can't be bound. Pick another key (Shift is fine).");
          } else if (!n.empty()) { SetBind(s, cap.action, cap.slot, n); cap.action.clear(); dirty = true; }
          swallow = true;
        } else if (e.type == SDL_EVENT_KEY_UP) {
          swallow = true;
        } else if (e.type == SDL_EVENT_MOUSE_WHEEL && e.wheel.y != 0) {
          SetBind(s, cap.action, cap.slot, e.wheel.y > 0 ? "MWheelUp" : "MWheelDown");
          cap.action.clear(); dirty = true; swallow = true;
        } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
          float x = e.button.x, y = e.button.y;
          if (x >= cap.box_min.x && x <= cap.box_max.x && y >= cap.box_min.y && y <= cap.box_max.y) {
            std::string n = MouseButtonName(e.button.button);
            if (!n.empty()) { SetBind(s, cap.action, cap.slot, n); cap.action.clear(); dirty = true; }
            swallow = true;
          }
        }
      }
      if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        if (e.wheel.y > 0) live.wheel_up_until = SDL_GetTicks() + 150;
        if (e.wheel.y < 0) live.wheel_down_until = SDL_GetTicks() + 150;
      }
      if (e.type == SDL_EVENT_MOUSE_MOTION) { live.mouse_dx += e.motion.xrel; live.mouse_dy += e.motion.yrel; }
      if (e.type == SDL_EVENT_GAMEPAD_ADDED) pads.push_back(SDL_OpenGamepad(e.gdevice.which));
      if (e.type == SDL_EVENT_GAMEPAD_REMOVED) {
        pads.erase(std::remove_if(pads.begin(), pads.end(), [&](SDL_Gamepad* g) {
          if (SDL_GetGamepadID(g) == e.gdevice.which) { SDL_CloseGamepad(g); return true; }
          return false; }), pads.end());
      }
      if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) running = false;
      if (!swallow) ImGui_ImplSDL3_ProcessEvent(&e);
    }

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("##root", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    {
      // Header band: black-to-blood gradient, emblem, bone-white title, red subtitle, red rule.
      ImDrawList* dl = ImGui::GetWindowDrawList();
      ImVec2 p = ImGui::GetCursorScreenPos();
      float w = ImGui::GetContentRegionAvail().x;
      ImU32 black = ImGui::GetColorU32(Rgb(0x000000)), blood = ImGui::GetColorU32(Rgb(pal::kRust));
      dl->AddRectFilledMultiColor(ImVec2(p.x - 8, p.y - 8), ImVec2(p.x + w + 8, p.y + 40), black, blood, blood, black);
      dl->AddRectFilled(ImVec2(p.x - 8, p.y + 40), ImVec2(p.x + w + 8, p.y + 42), ImGui::GetColorU32(Rgb(pal::kOliveLight)));
      dl->AddRectFilled(ImVec2(p.x - 8, p.y + 42), ImVec2(p.x + w + 8, p.y + 43), ImGui::GetColorU32(Rgb(pal::kRust)));
      float tx = p.x + 4;
      if (emblem) {
        dl->AddImage(ImTextureRef((ImTextureID)(intptr_t)emblem), ImVec2(p.x - 2, p.y - 6), ImVec2(p.x + 42, p.y + 38));
        tx = p.x + 50;
      }
      ImGui::SetCursorScreenPos(ImVec2(tx, p.y + 4));
      ImGui::SetWindowFontScale(1.45f);
      ImGui::TextColored(Rgb(pal::kCream), "NO MORE HEROES");
      ImGui::SetWindowFontScale(1.0f);
      ImGui::SameLine();
      ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
      ImGui::TextColored(Rgb(pal::kOliveLight), "HEROES' PARADISE");
      ImGui::SameLine();
      ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
      ImGui::TextColored(Rgb(pal::kTan), "   Xbox 360 recompiled");
      ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 52));
    }

    const float footer = 46.0f;
    ImGui::BeginChild("##tabs", ImVec2(0, -footer));
    if (ImGui::BeginTabBar("tabs")) {
      // ---------------- Controls
      if (ImGui::BeginTabItem("Controls")) {
        ImGui::TextWrapped("Keyboard and mouse act as an Xbox 360 controller. A real controller works at "
                           "the same time. Click a key box to rebind; each action takes up to two keys. "
                           "Combat sits on the mouse, movement and abilities around WASD. PUSH RS: E, then flick the mouse. LS+RS together: Q+E. Lock on: hold Shift.");
        if (ImGui::Button("Reset all to defaults")) { ResetBinds(s); dirty = true; }
        ImGui::Spacing();
        ImGui::BeginChild("##binds");
        std::string last_cat;
        // conflict map: key -> count of actions using it
        std::map<std::string, int> uses;
        for (auto& a : lc::Actions())
          for (auto& k : s.binds[a.id]) if (!k.empty()) uses[k]++;
        bool table_open = false;
        for (auto& a : lc::Actions()) {
          if (a.category != last_cat) {
            if (table_open) ImGui::EndTable();
            last_cat = a.category;
            ImGui::SeparatorText(a.category);
            table_open = ImGui::BeginTable(a.category, 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 2.2f);
            ImGui::TableSetupColumn("Controller", ImGuiTableColumnFlags_WidthStretch, 1.4f);
            ImGui::TableSetupColumn("Key 1", ImGuiTableColumnFlags_WidthStretch, 1.2f);
            ImGui::TableSetupColumn("Key 2", ImGuiTableColumnFlags_WidthStretch, 1.2f);
          }
          if (!table_open) continue;
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(a.name);
          ImGui::TableNextColumn();
          std::string padtxt;
          for (auto p : a.pad) padtxt += (padtxt.empty() ? "" : " + ") + std::string(lc::kPad[p].label);
          ImGui::TextDisabled("%s%s%s%s", padtxt.c_str(), a.note[0] ? " (" : "", a.note, a.note[0] ? ")" : "");
          for (int slot = 0; slot < 2; ++slot) {
            ImGui::TableNextColumn();
            auto& k = s.binds[a.id][slot];
            bool conflict = !k.empty() && uses[k] > 1;
            std::string label = (k.empty() ? std::string("-") : Pretty(k)) + "##" + a.id + std::to_string(slot);
            if (conflict) ImGui::PushStyleColor(ImGuiCol_Text, Rgb(pal::kSand));
            if (ImGui::Button(label.c_str(), ImVec2(-1, 0))) { cap.action = a.id; cap.slot = slot; }
            if (conflict) {
              ImGui::PopStyleColor();
              if (ImGui::IsItemHovered()) ImGui::SetTooltip("Also bound to another action");
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
              k.clear(); dirty = true;
            }
          }
        }
        if (table_open) ImGui::EndTable();
        ImGui::TextDisabled("Right-click a key box to clear it. Gold = key used by more than one action.");
        ImGui::EndChild();
        ImGui::EndTabItem();
      }
      // ---------------- Mouse
      if (ImGui::BeginTabItem("Mouse")) {
        ImGui::PushItemWidth(420);
        dirty |= ImGui::Checkbox("Mouse camera (mouse drives the right stick)", &s.mouse_look);
        ImGui::BeginDisabled(!s.mouse_look);
        dirty |= ImGui::SliderFloat("Sensitivity", &s.sensitivity, 0.1f, 5.0f, "%.2f");
        dirty |= ImGui::Checkbox("Invert Y", &s.invert_y);
        dirty |= ImGui::SliderFloat("Anti-deadzone", &s.deadzone_comp, 0.0f, 0.5f, "%.2f");
        ImGui::SameLine(); ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Games ignore small stick movements. This adds a minimum stick\n"
                            "deflection as soon as the mouse moves. Raise it if small\n"
                            "mouse movements don't turn the camera; lower it if the camera feels jumpy.");
        dirty |= ImGui::SliderFloat("Response curve", &s.curve, 0.3f, 2.0f, "%.2f");
        ImGui::SameLine(); ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("1.0 = linear. Below 1 = faster small movements.");
        dirty |= ImGui::SliderFloat("Smoothing", &s.smoothing, 0.0f, 0.9f, "%.2f");
        ImGui::EndDisabled();
        ImGui::Spacing();
        ImGui::TextWrapped("Note: the mouse is translated into right-stick movement, so the game's own camera "
                           "speed and acceleration still apply. Tune sensitivity here.");
        ImGui::PopItemWidth();
        ImGui::EndTabItem();
      }
      // ---------------- Display
      if (ImGui::BeginTabItem("Display")) {
        ImGui::PushItemWidth(420);
        const char* modes[] = {"Windowed", "Borderless fullscreen", "Exclusive fullscreen"};
        dirty |= ImGui::Combo("Window mode", &s.window_mode, modes, 3);
        std::string cur = std::to_string(s.width) + " x " + std::to_string(s.height);
        if (ImGui::BeginCombo(s.window_mode == 0 ? "Window size" : "Resolution", cur.c_str())) {
          for (auto [w, h] : resolutions) {
            std::string t = std::to_string(w) + " x " + std::to_string(h);
            if (ImGui::Selectable(t.c_str(), w == s.width && h == s.height)) { s.width = w; s.height = h; dirty = true; }
          }
          ImGui::EndCombo();
        }
        if (s.window_mode == 1) ImGui::TextDisabled("Borderless uses the desktop resolution; size applies when leaving fullscreen.");
        dirty |= ImGui::Checkbox("VSync", &s.vsync);
        {
          const char* caps[] = {"Off (game decides, VSync paces it)", "30 fps", "60 fps"};
          int ci = s.fps_cap == 30 ? 1 : s.fps_cap == 60 ? 2 : 0;
          if (ImGui::Combo("FPS cap", &ci, caps, 3)) { s.fps_cap = ci == 1 ? 30 : ci == 2 ? 60 : 0; dirty = true; }
          ImGui::SameLine(); ImGui::TextDisabled("(?)");
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Limits how many frames per second the game renders.\n"
                              "Off lets the game run at its own pace (recommended first).\n"
                              "30 gives a steady frame rate on slower CPUs and leaves CPU time for audio.");
        }
        const char* scales[] = {"1x  (native 360 - most authentic)", "2x", "3x"};
        int sc = std::clamp(s.render_scale, 1, 3) - 1;
        if (ImGui::Combo("Render scale", &sc, scales, 3)) { s.render_scale = sc + 1; dirty = true; }
        ImGui::SameLine(); ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Renders the game's 3D at a multiple of its 360 resolution.\n"
                            "1x keeps the original look; higher is sharper but heavier on the GPU\n"
                            "and can introduce visual glitches.");
        dirty |= ImGui::InputInt("Monitor (0 = default)", &s.monitor);
        s.monitor = std::clamp(s.monitor, 0, 16);
        ImGui::PopItemWidth();
        ImGui::EndTabItem();
      }
      // ---------------- Input test
      if (ImGui::BeginTabItem("Input Test")) {
        ImGui::TextWrapped("Press keys / buttons to see what the game will receive (keyboard+mouse merged "
                           "with any connected controller).");
        bool kbm[lc::kPadCount] = {};
        for (auto& a : lc::Actions())
          for (auto& k : s.binds[a.id])
            if (NamePressed(k, live))
              for (auto p : a.pad) kbm[p] = true;
        // controllers
        bool padb[lc::kPadCount] = {};
        float lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
        for (auto* g : pads) {
          auto bt = [&](SDL_GamepadButton b) { return SDL_GetGamepadButton(g, b); };
          padb[lc::kPadA] |= bt(SDL_GAMEPAD_BUTTON_SOUTH);
          padb[lc::kPadB] |= bt(SDL_GAMEPAD_BUTTON_EAST);
          padb[lc::kPadX] |= bt(SDL_GAMEPAD_BUTTON_WEST);
          padb[lc::kPadY] |= bt(SDL_GAMEPAD_BUTTON_NORTH);
          padb[lc::kPadLB] |= bt(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
          padb[lc::kPadRB] |= bt(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
          padb[lc::kPadLS] |= bt(SDL_GAMEPAD_BUTTON_LEFT_STICK);
          padb[lc::kPadRS] |= bt(SDL_GAMEPAD_BUTTON_RIGHT_STICK);
          padb[lc::kPadBack] |= bt(SDL_GAMEPAD_BUTTON_BACK);
          padb[lc::kPadStart] |= bt(SDL_GAMEPAD_BUTTON_START);
          padb[lc::kPadUp] |= bt(SDL_GAMEPAD_BUTTON_DPAD_UP);
          padb[lc::kPadDown] |= bt(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
          padb[lc::kPadLeft] |= bt(SDL_GAMEPAD_BUTTON_DPAD_LEFT);
          padb[lc::kPadRight] |= bt(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
          auto ax = [&](SDL_GamepadAxis a) { return SDL_GetGamepadAxis(g, a) / 32767.0f; };
          auto big = [](float a, float b) { return std::fabs(b) > std::fabs(a) ? b : a; };
          lx = big(lx, ax(SDL_GAMEPAD_AXIS_LEFTX)); ly = big(ly, -ax(SDL_GAMEPAD_AXIS_LEFTY));
          rx = big(rx, ax(SDL_GAMEPAD_AXIS_RIGHTX)); ry = big(ry, -ax(SDL_GAMEPAD_AXIS_RIGHTY));
          lt = std::max(lt, ax(SDL_GAMEPAD_AXIS_LEFT_TRIGGER)); rt = std::max(rt, ax(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
        }
        // keyboard sticks / triggers (digital), merged by larger magnitude like the runtime
        auto merge = [](float& v, float k) { if (std::fabs(k) > std::fabs(v)) v = k; };
        merge(lx, (kbm[lc::kLsRight] ? 1.f : 0.f) - (kbm[lc::kLsLeft] ? 1.f : 0.f));
        merge(ly, (kbm[lc::kLsUp] ? 1.f : 0.f) - (kbm[lc::kLsDown] ? 1.f : 0.f));
        float krx = (kbm[lc::kRsRight] ? 1.f : 0.f) - (kbm[lc::kRsLeft] ? 1.f : 0.f);
        float kry = (kbm[lc::kRsUp] ? 1.f : 0.f) - (kbm[lc::kRsDown] ? 1.f : 0.f);
        if (s.mouse_look) {
          float vx = live.mouse_dx * s.sensitivity * (200.0f / 32767.0f);
          float vy = -live.mouse_dy * s.sensitivity * (200.0f / 32767.0f) * (s.invert_y ? -1.f : 1.f);
          float m = std::sqrt(vx * vx + vy * vy);
          if (m > 1e-6f) {
            float out = s.deadzone_comp + (1 - s.deadzone_comp) * std::pow(std::min(m, 1.0f), s.curve);
            krx += vx / m * out; kry += vy / m * out;
          }
        }
        live.mouse_dx = live.mouse_dy = 0;
        merge(rx, std::clamp(krx, -1.f, 1.f));
        merge(ry, std::clamp(kry, -1.f, 1.f));
        if (kbm[lc::kPadLT]) lt = 1; if (kbm[lc::kPadRT]) rt = 1;
        for (int p = 0; p < lc::kPadCount; ++p) padb[p] |= kbm[p];

        ImGui::Spacing();
        auto pill = [&](int p) {
          bool on = padb[p];
          ImGui::PushStyleColor(ImGuiCol_Button, on ? Rgb(pal::kRustLight) : Rgb(pal::kFrame));
          ImGui::Button(lc::kPad[p].label, ImVec2(92, 30));
          ImGui::PopStyleColor();
        };
        int order[] = {lc::kPadLB, lc::kPadRB, lc::kPadBack, lc::kPadStart, lc::kPadA, lc::kPadB, lc::kPadX,
                       lc::kPadY, lc::kPadLS, lc::kPadRS, lc::kPadUp, lc::kPadDown, lc::kPadLeft, lc::kPadRight};
        for (int i = 0; i < 14; ++i) { pill(order[i]); if (i % 4 != 3) ImGui::SameLine(); }
        ImGui::NewLine();
        ImGui::ProgressBar(lt, ImVec2(220, 0), "LT"); ImGui::SameLine(); ImGui::ProgressBar(rt, ImVec2(220, 0), "RT");
        auto stick = [&](const char* name, float x, float y) {
          ImGui::BeginGroup();
          ImGui::Text("%s  (%.2f, %.2f)", name, x, y);
          ImVec2 p = ImGui::GetCursorScreenPos();
          ImDrawList* dl = ImGui::GetWindowDrawList();
          ImVec2 c(p.x + 60, p.y + 60);
          dl->AddCircle(c, 55, ImGui::GetColorU32(Rgb(pal::kOliveLight)), 48, 2);
          dl->AddCircleFilled(ImVec2(c.x + x * 50, c.y - y * 50), 9, ImGui::GetColorU32(Rgb(pal::kRustLight)));
          ImGui::Dummy(ImVec2(130, 125));
          ImGui::EndGroup();
        };
        stick("Left stick", lx, ly); ImGui::SameLine(0, 40); stick("Right stick", rx, ry);
        ImGui::Text("Controllers connected: %d", int(pads.size()));
        for (auto* g : pads) ImGui::BulletText("%s", SDL_GetGamepadName(g));
        ImGui::EndTabItem();
      }
      // ---------------- Game
      if (ImGui::BeginTabItem("Game")) {
        ImGui::PushItemWidth(420);
        auto path_row = [&](const char* label, std::string& v, const char* tip) {
          char buf[1024];
          std::snprintf(buf, sizeof buf, "%s", v.c_str());
          if (ImGui::InputText(label, buf, sizeof buf)) { v = buf; dirty = true; }
          bool ok = fs::exists(Resolve(v));
          ImGui::SameLine();
          ImGui::TextColored(ok ? Rgb(pal::kLime) : Rgb(pal::kSand), ok ? "found" : "MISSING");
          ImGui::TextDisabled("%s", tip);
        };
        path_row("Game files", s.game_dir, "Disc files (default.xex + Media folder)");
        path_row("User data", s.user_dir, "Save data and profile");
        dirty |= ImGui::Checkbox("Close launcher when the game starts", &s.close_on_play);
        ImGui::Spacing();
        ImGui::SeparatorText("Language");
        ImGui::Text("Japanese");
        ImGui::TextDisabled("The game runs in its original Japanese.");
        ImGui::PopItemWidth();
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
    ImGui::EndChild();

    // footer
    ImGui::Separator();
    if (ImGui::Button("Save", ImVec2(120, 30))) save_all();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, Rgb(pal::kOlive));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Rgb(pal::kOliveLight));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Rgb(0xFF2A2A));
    if (ImGui::Button("PLAY", ImVec2(160, 30))) {
      if (save_all()) {
        std::string exe = (BaseDir() / kGameExe).string();
        if (!fs::exists(exe)) {
          flash("Game executable not found: " + exe);
        } else {
          const char* args[] = {exe.c_str(), nullptr};
          // A WIN32 (GUI) app has no console handles; SDL's default "inherit stdio" then fails
          // with "DuplicateHandle(): The handle is invalid". Give the game null stdio instead.
          SDL_PropertiesID props = SDL_CreateProperties();
          SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void*)args);
          SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
          SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_NULL);
          SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);
          SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
          const std::string workdir = BaseDir().string();
          SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, workdir.c_str());
          SDL_Process* proc = SDL_CreateProcessWithProperties(props);
          SDL_DestroyProperties(props);
          if (!proc) flash(std::string("Could not start the game: ") + SDL_GetError());
          else { SDL_DestroyProcess(proc); flash("Game started"); if (s.close_on_play) running = false; }
        }
      }
    }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    if (SDL_GetTicks() < status_until) ImGui::TextUnformatted(status.c_str());
    else if (dirty) ImGui::TextDisabled("Unsaved changes");
    ImGui::End();

    // capture popup
    if (!cap.action.empty()) {
      ImGui::OpenPopup("Rebind");
      ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
      if (ImGui::BeginPopupModal("Rebind", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        const char* nm = cap.action.c_str();
        for (auto& a : lc::Actions()) if (cap.action == a.id) nm = a.name;
        ImGui::Text("Binding: %s  (key %d)", nm, cap.slot + 1);
        ImGui::Spacing();
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::Button("Press a key, scroll the wheel,\nor click a mouse button HERE", ImVec2(360, 90));
        cap.box_min = p;
        cap.box_max = ImVec2(p.x + 360, p.y + 90);
        if (ImGui::Button("Clear", ImVec2(176, 0))) { SetBind(s, cap.action, cap.slot, ""); cap.action.clear(); dirty = true; }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(176, 0))) cap.action.clear();
        if (cap.action.empty()) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
      }
    }

    ImGui::Render();
    SDL_SetRenderDrawColorFloat(renderer, 0x1D / 255.0f, 0x20 / 255.0f, 0x1A / 255.0f, 1);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
  }

  if (dirty) save_all();
  for (auto* g : pads) SDL_CloseGamepad(g);
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  if (emblem) SDL_DestroyTexture(emblem);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
