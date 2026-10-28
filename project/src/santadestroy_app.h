// Santa Destroy Recomp - ReXGlue app class

#pragma once

#include <rex/rex_app.h>

class SantaDestroyApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<SantaDestroyApp>(new SantaDestroyApp(ctx, "santadestroy",
        PPCImageConfig));
  }
};
