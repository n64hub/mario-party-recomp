// The settings: RecompFrontend's General, Controls, Graphics, Sound and Mods tabs, with its default bindings. Its own
// Graphics options carry what a recomp needs: Resolution and MS Anti-Aliasing (high resolution), Framerate
// (interpolation to the display's rate) and Aspect Ratio and HUD Placement (widescreen). The General tab ends with the
// board's options, Board Movement and Map Scrolling, which lib/mario-party-engine's src/board_options.cpp words and
// us.toml hooks, and Unlocks, which patches/unlock_all.c asks (recomp_unlock_all) and says what it opens.

#include <filesystem>
#include <string>
#include <vector>

#include "recomp.h"
#include "librecomp/helpers.hpp"
#include "recompui/config.h"
#include "util/file.h"
#include "../../lib/mario-party-engine/src/board_options.h"
#include "marioparty.h"

namespace {
    enum class Unlocks {
        Original,
        All,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> unlocks_options = {
        {Unlocks::Original, "Original", "Original"},
        {Unlocks::All, "All", "All"},
    };
    const std::string unlocks_option = "unlocks";
}

// recomp_unlock_all(): 1 when Unlocks is All (patches/unlock_all.c).
extern "C" void recomp_unlock_all(uint8_t* rdram, recomp_context* ctx) {
    auto value = recompui::config::get_general_config().get_option_value(unlocks_option);
    _return(ctx, static_cast<Unlocks>(std::get<uint32_t>(value)) == Unlocks::All ? 1 : 0);
}

void marioparty::init_config() {
    std::filesystem::path app_dir = recompui::file::get_app_folder_path();
    if (!app_dir.empty()) {
        std::filesystem::create_directories(app_dir);
    }

    recompui::config::GeneralTabOptions general_options{};
    general_options.has_rumble_strength = true;
    general_options.has_gyro_sensitivity = true;
    general_options.has_mouse_sensitivity = true;
    recomp::config::Config& general = recompui::config::create_general_tab(general_options);
    mp_board_options::add(general);
    general.add_enum_option(
        unlocks_option,
        "Unlocks",
        "What is open to play, whatever your save has earned."
        "<br/>"
        "<b>All:</b> every board (Bowser's Magma Mountain and Eternal Star too, and no First Map to play before "
        "picking one), every mini-game in the Mini-Game "
        "House, everything the Mushroom Shop sells once, and the Mini-Game House's and Option House's extras, from "
        "the next screen on. Your save keeps only what you have earned, so back on Original it is as you left it. "
        "<b>Original:</b> as on the console, earned by playing.",
        unlocks_options,
        Unlocks::Original
    );
    recompui::config::create_controls_tab();
    recompui::config::create_graphics_tab();
    recompui::config::create_sound_tab();
    recompui::config::create_mods_tab();
    recompui::config::finalize();
}
