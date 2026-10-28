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
                     "ui_app_update_menu", "ui_app_update_detail", "ui_app_show_detail"):
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

    def test_refresh_interval_menu_and_manual_mode(self) -> None:
        menu = read("main/menu_model.c")
        store = read("main/config_store.c")
        self.assertIn("MENU_ITEM_REFRESH", read("main/menu_model.h"))
        self.assertIn("refresh_interval_step", menu)
        self.assertIn("MENU_CHANGE_REFRESH_INTERVAL", self.controller)
        self.assertIn("config_store_save_interval", self.controller)
        self.assertIn("quote_worker_set_interval", self.controller)
        self.assertIn("refresh_interval_normalize", store)
        self.assertIn("REFRESH_INTERVAL_MANUAL", self.quotes)
        self.assertIn("portMAX_DELAY", self.quotes)

    def test_auto_refresh_limited_to_trading_hours(self) -> None:
        self.assertIn("market_hours_is_trading", self.quotes)
        self.assertIn("market_hours_seconds_until_open", self.quotes)
        self.assertIn("time_app_now_hms", self.quotes)
        self.assertIn("market_hours_is_trading", self.controller)
        self.assertIn("market_open", self.ui)
        self.assertIn("休市", self.ui)

    def test_wifi_indicator_reflects_connection_on_every_screen(self) -> None:
        self.assertIn("#define WIFI_BAR_COUNT 4", self.ui)
        self.assertIn("wifi_signal_level", self.ui)
        for name in ("ui_app_update_list", "ui_app_update_setup", "ui_app_update_menu",
                     "ui_app_update_detail"):
            body = function_body(self.ui, name)
            self.assertIn("set_wifi(", body)
        self.assertIn(".wifi_connected = wifi->sta_connected", self.controller)
        self.assertIn(".rssi = wifi->rssi", self.controller)

    def test_status_line_shows_refresh_spinner_while_fetching(self) -> None:
        self.assertIn("lv_spinner_create", self.ui)
        body = function_body(self.ui, "ui_app_update_list")
        self.assertIn("snapshot->fetching", body)
        self.assertIn("s_list_refresh", body)

    def test_weak_signal_roaming_is_wired(self) -> None:
        self.assertIn("roam_policy_should_switch", self.wifi)
        self.assertIn("esp_wifi_scan_start", self.wifi)
        self.assertIn("s_roam_in_progress", self.wifi)
        self.assertIn("ROAM_PERIOD_MS", self.wifi)

    def test_cost_and_lots_drive_profit_display(self) -> None:
        store = read("main/config_store.c")
        server = read("main/web_server.c")
        self.assertIn('KEY_POSITIONS "positions"', store)
        self.assertIn("config_store_save_positions", store)
        self.assertIn("stock_positions_parse", store)
        self.assertIn("stock_positions_serialize", store)
        self.assertIn("stock_position_profit", self.ui)
        self.assertIn("stock_position_today_profit", self.ui)
        self.assertIn("stock_position_is_set", self.ui)
        self.assertIn("lv_font_cn_12_pnl", self.ui)
        self.assertIn("lv_label_set_recolor", self.ui)
        self.assertIn("总:%s", self.ui)
        self.assertIn("今:%s", self.ui)
        self.assertNotIn("盈亏", self.ui)
        self.assertIn("memcmp(a->cost_price", self.controller)
        self.assertIn("memcmp(a->lots", self.controller)
        self.assertIn("/api/positions", server)
        self.assertIn("positions_post", server)
        self.assertIn('"pnl"', server)
        self.assertIn('"pnl_today"', server)
        self.assertIn("stock_position_today_profit", server)

    def test_detail_page_opens_on_ok_and_shares_refresh_cadence(self) -> None:
        self.assertIn("open_detail()", self.controller)
        self.assertIn("close_detail()", self.controller)
        self.assertIn("quote_worker_set_detail", self.controller)
        self.assertIn("quote_worker_detail", self.controller)
        self.assertIn("detail_generation", self.controller)
        self.assertIn("ui_app_show_detail", self.ui)
        self.assertIn("ui_app_update_detail", self.ui)
        # 详情与列表在同一次轮询里拉取,刷新节奏与手动模式完全一致。
        body = function_body(self.quotes, "poll_once")
        self.assertIn("poll_list()", body)
        self.assertIn("poll_detail()", body)
        self.assertIn("quote_parse_tencent_detail", self.quotes)
        self.assertIn("quote_parse_minute_list", self.quotes)

        header = read("main/quote_parse.h")
        for token in ("volume_hand", "market_cap_yi", "pe", "open", "high", "low", "ask[",
                      "bid[", "minute[", "minute_slot["):
            self.assertIn(token, header)

        self.assertIn("lv_line_create", self.ui)
        self.assertIn("lv_font_cn_12_detail", self.ui)
        self.assertIn("无分时数据", self.ui)
        # 时间刻度已移除,开盘/午休/收盘改用面板内的竖分隔线定位。
        self.assertNotIn("11:30/13:00", self.ui)
        self.assertIn("chart_slot_x", self.ui)
        self.assertIn("separator_slots", self.ui)
        # 详情页与列表共用同一轮刷新:旋转指示覆盖整轮,按 fetching 显示。
        self.assertIn(".fetching = quotes->fetching", self.controller)
        detail_body = function_body(self.ui, "ui_app_update_detail")
        self.assertIn("view->fetching", detail_body)
        self.assertIn("s_detail_refresh", detail_body)
        body = function_body(self.quotes, "poll_once")
        self.assertLess(body.index("set_fetching(true)"), body.index("poll_list()"))
        self.assertGreater(body.index("set_fetching(false)"), body.index("poll_detail()"))
        self.assertIn("短按 OK 刷新", self.ui)
        self.assertIn("长按 OK 返回", self.ui)
        for text in ("额", "市值", "卖一", "买一", "高", "开", "低"):
            self.assertIn(text, self.ui)
        self.assertIn("range_value", self.ui)


if __name__ == "__main__":
    unittest.main()
