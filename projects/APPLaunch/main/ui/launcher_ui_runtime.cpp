/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "launcher_ui_runtime.h"

#include "launch.h"
#include "ui_launch_page.h"

#include "hal_lvgl_bsp.h"
#include "launcher_platform.hpp"

void LauncherUiRuntime::create_display()
{

    dispp_ = lv_disp_get_default();
    theme_ = lv_theme_default_init(dispp_, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
                                   false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp_, theme_);
}

void LauncherUiRuntime::build_launcher_home()
{
    launch_page_->create_screen();
    apply_home_wallpaper();
    launch_->bind_ui();
    launch_page_->init_input_group();
}

// Alpine fork: the stock home background is the theme's flat black. The
// product wants the Alpine wallpaper on the 320x170 panel, so the home
// screen paints it as a background image (pages keep their own dark
// surfaces). A missing asset falls back to the old black silently.
void LauncherUiRuntime::apply_home_wallpaper()
{
    lv_obj_t *home = launch_page_ ? launch_page_->screen() : nullptr;
    if (!home) return;
    const char *wallpaper = launcher_platform::path_c("alpine_wallpaper.png");
    if (!wallpaper || !wallpaper[0]) return;
    lv_obj_set_style_bg_color(home, lv_color_hex(0x0A1628), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(home, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_src(home, wallpaper, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_opa(home, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_tiled(home, false, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void LauncherUiRuntime::show_initial_screen()
{
#ifndef APPLAUNCH_STARTUP_ANIMATION
    launch_page_->load_home_screen();
#else
#ifdef HAL_PLATFORM_SDL
    launch_page_->load_home_screen();
#else
    bool gif_exists = false;
    cp0_signal_filesystem_api({"Exists", launcher_platform::path("logo_output.gif")}, [&](int code, std::string data) {
        gif_exists = code == 0 && data == "1";
    });
    if (gif_exists) {
        launch_page_->start_startup_gif();
    } else {
        launch_page_->load_home_screen();
    }
#endif
#endif
}

LauncherUiRuntime::LauncherUiRuntime()
{
    create_display();

    launch_ = std::make_unique<Launch>();
    launch_page_ = std::make_shared<UILaunchPage>(launch_.get());
    launch_->set_launch_page(launch_page_);
}

LauncherUiRuntime::~LauncherUiRuntime() = default;

void LauncherUiRuntime::start()
{
    build_launcher_home();
    show_initial_screen();
}
