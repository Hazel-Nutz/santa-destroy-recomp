// No More Heroes (Xbox 360) recompiled with ReXGlue

#pragma once

#include <rex/rex_app.h>

class NMHApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<NMHApp>(new NMHApp(ctx, "nmh",
        PPCImageConfig));
  }
};
