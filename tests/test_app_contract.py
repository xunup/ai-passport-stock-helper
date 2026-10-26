#!/usr/bin/env python3
"""Static contracts for the stock ticker application wiring."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def function_body(source: str, name: str) -> str:
    match = re.search(rf"\b{re.escape(name)}\s*\([^;]*?\)\s*\{{", source)
    if not match:
        raise AssertionError(f"function not found: {name}")
    start = match.end() - 1
    depth = 0
    for index in range(start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start + 1:index]
    raise AssertionError(f"function is unterminated: {name}")


class AppWiringTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.main = read("main/main.c")
        cls.controller = read("main/app_controller.c")
        cls.ui = read("main/ui_app.c")
        cls.wifi = read("main/wifi_app.c")
        cls.quotes = read("main/quote_worker.c")

    def test_main_starts_display_then_application(self) -> None:
        self.assertIn("app_controller_start()", self.main)
        self.assertNotIn("demo_", self.main)
        self.assertNotIn("ui_pixel", self.main)
        self.assertLess(self.main.index("bsp_display_init()"),
                        self.main.index("app_controller_start()"))
        self.assertLess(self.main.index("bsp_lvgl_init()"),
                        self.main.index("app_controller_start()"))

    def test_button_callback_only_enqueues(self) -> None:
        body = function_body(self.controller, "on_key")
        self.assertIn("xQueueSend(", body)
        self.assertRegex(body, r"xQueueSend\([^;]*,\s*0\)")
        self.assertNotIn("lv_", body)
        self.assertNotIn("bsp_", body)

    def test_lvgl_calls_are_confined_and_locked(self) -> None:
        self.assertNotIn("lv_", self.controller)
        self.assertNotIn("lv_", self.wifi)
        self.assertNotIn("lv_", self.quotes)
        for name in ("ui_app_init", "ui_app_update_list", "ui_app_update_setup",
                     "ui_app_update_menu"):
            body = function_body(self.ui, name)
            self.assertIn("bsp_lvgl_lock(", body)
            self.assertIn("bsp_lvgl_unlock(", body)

    def test_hotspot_can_be_switched_at_runtime(self) -> None:
        self.assertIn("WIFI_MODE_APSTA", self.wifi)
        self.assertIn("WIFI_MODE_STA", self.wifi)
        self.assertRegex(self.wifi, r"configure_ap\(\)")
        self.assertIn("wifi_app_set_ap_enabled", self.wifi)
        self.assertIn("wifi_app_start(s_config.ssid, s_config.pass, s_ap_enabled)",
                      self.controller)

    def test_web_settings_sync_into_controller_state(self) -> None:
        self.assertIn("s_brightness = next.brightness", self.controller)
        self.assertIn("s_auto_off_enabled = next.auto_off_enabled", self.controller)
        self.assertIn("s_ap_enabled = next.ap_enabled", self.controller)
        self.assertIn("s_menu.ap_enabled = s_ap_enabled", self.controller)

    def test_brightness_defaults_to_level_three(self) -> None:
        limits = read("main/settings_limits.h")
        self.assertIn("#define SETTINGS_BRIGHTNESS_DEFAULT 3", limits)

    def test_long_press_opens_settings_menu(self) -> None:
        self.assertIn("menu_model_activate", self.controller)
        self.assertIn("menu_model_move", self.controller)
        self.assertIn("open_menu()", self.controller)
        self.assertIn("MENU_ACTIVATE_TOGGLE_HOTSPOT", self.controller)
        self.assertIn("config_store_save_display", self.controller)
        self.assertIn("bsp_display_backlight(0)", self.controller)
        self.assertIn("长按 OK 设置", self.ui)

    def test_screen_off_slows_polling_and_swallows_wake_key(self) -> None:
        self.assertIn("QUOTE_IDLE_INTERVAL_SEC 60", self.quotes)
        self.assertIn("quote_worker_set_idle(true)", self.controller)
        self.assertIn("quote_worker_set_idle(false)", self.controller)
        self.assertIn("s_swallow_wake", self.controller)
        self.assertIn("BSP_BTN_PRESS", self.controller)
        self.assertIn("BSP_BTN_RELEASE", self.controller)

    def test_wifi_indicator_reflects_connection_on_every_screen(self) -> None:
        self.assertIn("LV_SYMBOL_WIFI", self.ui)
        for name in ("ui_app_update_list", "ui_app_update_setup", "ui_app_update_menu"):
            body = function_body(self.ui, name)
            self.assertIn("set_wifi(", body)
        self.assertIn(".wifi_connected = wifi->sta_connected", self.controller)

    def test_weak_signal_roaming_is_wired(self) -> None:
        self.assertIn("roam_policy_should_switch", self.wifi)
        self.assertIn("esp_wifi_scan_start", self.wifi)
        self.assertIn("s_roam_in_progress", self.wifi)
        self.assertIn("ROAM_PERIOD_MS", self.wifi)


if __name__ == "__main__":
    unittest.main()
