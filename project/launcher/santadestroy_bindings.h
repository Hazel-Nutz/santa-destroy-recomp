// Game action <-> Xbox 360 pad mapping and keyboard/mouse defaults.
// Actions are named after the Xbox 360 pad buttons (the game's own Controls screen shows what each
// one does). Combat on the mouse (X LMB, Y RMB, B MMB); around WASD: A Space, L3 Q + R3 E (dual-stick
// clicks), LT Shift (lock on target), RT R, LB Z, RB X. Wheel down = RB, wheel up = RT.
#pragma once

#include <array>
#include <string>
#include <vector>

namespace lc {

// ReXGlue keybind cvars (one per virtual pad input). Their value is a comma list of key names.
enum PadInput : int {
  kPadA, kPadB, kPadX, kPadY,
  kPadLB, kPadRB, kPadLT, kPadRT,
  kPadLS, kPadRS, kPadBack, kPadStart,
  kPadUp, kPadDown, kPadLeft, kPadRight,
  kLsUp, kLsDown, kLsLeft, kLsRight,
  kRsUp, kRsDown, kRsLeft, kRsRight,
  kPadCount
};

struct PadInfo {
  const char* cvar;   // ReXGlue cvar name
  const char* label;  // shown in the launcher
};

inline constexpr std::array<PadInfo, kPadCount> kPad = {{
    {"keybind_a", "A"},
    {"keybind_b", "B"},
    {"keybind_x", "X"},
    {"keybind_y", "Y"},
    {"keybind_left_shoulder", "LB"},
    {"keybind_right_shoulder", "RB"},
    {"keybind_left_trigger", "LT"},
    {"keybind_right_trigger", "RT"},
    {"keybind_lstick_press", "L3"},
    {"keybind_rstick_press", "R3"},
    {"keybind_back", "Back"},
    {"keybind_start", "Start"},
    {"keybind_dpad_up", "D-pad Up"},
    {"keybind_dpad_down", "D-pad Down"},
    {"keybind_dpad_left", "D-pad Left"},
    {"keybind_dpad_right", "D-pad Right"},
    {"keybind_lstick_up", "Left Stick Up"},
    {"keybind_lstick_down", "Left Stick Down"},
    {"keybind_lstick_left", "Left Stick Left"},
    {"keybind_lstick_right", "Left Stick Right"},
    {"keybind_rstick_up", "Right Stick Up"},
    {"keybind_rstick_down", "Right Stick Down"},
    {"keybind_rstick_left", "Right Stick Left"},
    {"keybind_rstick_right", "Right Stick Right"},
}};

struct Action {
  const char* id;        // stable key in santadestroy_launcher.ini
  const char* name;      // action name shown in the launcher
  const char* category;
  std::vector<PadInput> pad;  // pad inputs pressed by this action (combos press several)
  const char* note;      // extra hint ("" if none)
  std::array<const char*, 2> defaults;  // default keys ("" = unbound)
};

inline const std::vector<Action>& Actions() {
  static const std::vector<Action> kActions = {
      // Button names follow the Xbox 360 pad. The game's own Controls screen (pause / options) shows what
      // each button does; rebind here to taste.
      // Movement & abilities: left hand around WASD
      {"move_forward", "Move Forward", "Movement & Abilities (keyboard)", {kLsUp}, "", {"W", ""}},
      {"move_back", "Move Backward", "Movement & Abilities (keyboard)", {kLsDown}, "", {"S", ""}},
      {"move_left", "Move Left", "Movement & Abilities (keyboard)", {kLsLeft}, "", {"A", ""}},
      {"move_right", "Move Right", "Movement & Abilities (keyboard)", {kLsRight}, "", {"D", ""}},
      {"pad_a", "A Button", "Movement & Abilities (keyboard)", {kPadA}, "menu confirm", {"Space", "Return"}},
      {"lb", "LB", "Movement & Abilities (keyboard)", {kPadLB}, "", {"Z", ""}},
      {"rb", "RB", "Movement & Abilities (keyboard)", {kPadRB}, "", {"X", "MWheelDown"}},
      {"lt", "LT - Lock on target", "Movement & Abilities (keyboard)", {kPadLT}, "hold to lock on", {"Shift", ""}},
      {"ls_press", "Left Stick Click (L3)", "Movement & Abilities (keyboard)", {kPadLS}, "press with R3 for dual-stick prompts", {"Q", ""}},
      {"rs_press", "Right Stick Click (R3)", "Movement & Abilities (keyboard)", {kPadRS}, "PUSH RS, then flick; with L3 for dual-stick", {"E", ""}},
      {"rt", "RT", "Movement & Abilities (keyboard)", {kPadRT}, "", {"R", "MWheelUp"}},
      // Combat: right hand on the mouse (no side buttons needed)
      {"pad_x", "X Button", "Combat (mouse)", {kPadX}, "", {"LMB", ""}},
      {"pad_y", "Y Button", "Combat (mouse)", {kPadY}, "", {"RMB", ""}},
      {"pad_b", "B Button", "Combat (mouse)", {kPadB}, "menu back", {"MMB", "V"}},
      // D-pad & menus
      {"dpad_up", "D-pad Up", "D-pad & Menus", {kPadUp}, "", {"1", ""}},
      {"dpad_down", "D-pad Down", "D-pad & Menus", {kPadDown}, "", {"2", ""}},
      {"dpad_left", "D-pad Left", "D-pad & Menus", {kPadLeft}, "", {"3", ""}},
      {"dpad_right", "D-pad Right", "D-pad & Menus", {kPadRight}, "", {"4", ""}},
      {"pause", "Start (Pause)", "D-pad & Menus", {kPadStart}, "", {"Escape", ""}},
      {"back", "Back", "D-pad & Menus", {kPadBack}, "", {"Tab", ""}},
      // Optional keyboard camera (mouse camera is configured on the Mouse tab)
      {"look_up", "Camera Up (keys)", "Keyboard Camera (optional)", {kRsUp}, "", {"Up", ""}},
      {"look_down", "Camera Down (keys)", "Keyboard Camera (optional)", {kRsDown}, "", {"Down", ""}},
      {"look_left", "Camera Left (keys)", "Keyboard Camera (optional)", {kRsLeft}, "", {"Left", ""}},
      {"look_right", "Camera Right (keys)", "Keyboard Camera (optional)", {kRsRight}, "", {"Right", ""}},
  };
  return kActions;
}

}  // namespace lc
