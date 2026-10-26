#!/usr/bin/env python3
"""Static checks for the embedded configuration page and its API contract."""

from __future__ import annotations

import re
import shutil
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HTML = ROOT / "main" / "web" / "index.html"
SERVER = ROOT / "main" / "web_server.c"

REQUIRED_IDS = {
    "chipAp", "firstRun", "wifiState", "apState", "quoteState", "portalActions",
    "closePortal", "ssid", "pass", "wifiSave", "scanBtn", "forgetBtn", "networks",
    "codes", "interval", "stocksSave", "quoteRows", "footAp", "toast",
    "brightness", "brightnessValue", "autoOff", "autoOffHint", "autoOffTime",
    "autoOffTimeField", "hotspot", "settingsSave",
}
REQUIRED_TEXT = ("股票盯盘", "Wi-Fi 配网", "设备设置", "自选股", "屏幕亮度",
                 "自动息屏", "息屏时间", "配网热点", "192.168.4.1", "新浪财经")
ROUTE_RE = re.compile(r'register_handler\(s_server, "([^"]+)",\s*HTTP_(\w+)')
FETCH_RE = re.compile(r'api\("(/[^"]*)"')
EMOJI_RE = re.compile(
    "[\U0001F000-\U0001FAFF\U00002600-\U000027BF\U0001F1E6-\U0001F1FF]")


def main() -> None:
    html = HTML.read_text(encoding="utf-8")
    server = SERVER.read_text(encoding="utf-8")

    assert html.lstrip().lower().startswith("<!doctype html>"), "missing doctype"
    assert '<html lang="zh-CN">' in html, "page language must be zh-CN"
    assert '<meta charset="utf-8">' in html, "missing utf-8 charset"
    assert 'name="viewport"' in html, "missing mobile viewport"

    ids = set(re.findall(r'\bid="([^"]+)"', html))
    assert not (REQUIRED_IDS - ids), f"missing element ids: {sorted(REQUIRED_IDS - ids)}"
    for text in REQUIRED_TEXT:
        assert text in html, f"missing UI copy: {text}"
    assert not EMOJI_RE.search(html), "page must not use emoji (device/browser fonts differ)"

    for marker in ('src="http://', 'src="https://', 'href="http://', 'href="https://',
                   "@import", "url(http"):
        assert marker not in html, f"external asset reference: {marker}"

    interval = re.search(r'<input id="interval"[^>]*>', html)
    assert interval, "missing interval input"
    assert 'min="3"' in interval.group(0) and 'max="300"' in interval.group(0), \
        "interval bounds must match firmware (3-300)"
    assert 'type="password"' in html, "Wi-Fi password field must be masked"

    brightness = re.search(r'<input id="brightness"[^>]*>', html)
    assert brightness, "missing brightness input"
    assert 'min="1"' in brightness.group(0) and 'max="5"' in brightness.group(0), \
        "brightness bounds must match firmware (1-5 levels)"
    assert 'value="3"' in brightness.group(0), "brightness default must be level 3"
    auto_off = re.search(r'<select id="autoOffTime">(.*?)</select>', html, re.S)
    assert auto_off, "missing auto-off time select"
    assert 'value="5"' in auto_off.group(1) and 'value="60"' in auto_off.group(1), \
        "auto-off time options must match firmware (5-60 seconds)"

    calls = set(FETCH_RE.findall(html))
    assert calls, "no API calls found"
    routes = {(path, method) for path, method in ROUTE_RE.findall(server)}
    for path in calls:
        assert any(route_path == path for route_path, _ in routes), \
            f"page calls unregistered endpoint {path}"
    for path in ("/api/status", "/api/config", "/api/settings", "/api/scan", "/api/wifi",
                 "/api/wifi/forget", "/api/portal/close"):
        assert path in calls, f"page never calls {path}"

    assert "function esc(" in html, "dynamic HTML must be escaped"
    assert html.count("esc(") >= 10, "esc() is not used consistently"
    assert "response.ok" in html and "JSON.parse" in html, "API errors are not handled"
    assert "localStorage" in html, "form values must be cached in the browser"
    assert 'var CACHE_KEY = "folotoy.stock.form.v1"' in html, "cache key must stay stable"
    assert html.count("localStorage.setItem") == 1, "cache writes must be centralized"
    assert "restoreForm();" in html, "cached values must be restored before polling"
    assert "applyDeviceSettings(data.config)" in html, \
        "only device settings may refresh the cache"
    assert "ap_enabled" in server, "status API must expose the hotspot switch"
    assert "data.device.ap_enabled" in html, "page must reflect a disabled hotspot"

    add_config = re.search(r"static void add_config\(.*?\n\}", server, re.S)
    assert add_config, "missing config serializer"
    for field in ("brightness", "auto_off_sec", "auto_off_enabled"):
        assert f'"{field}"' in add_config.group(0), f"config API must expose {field}"
    assert "settings_post" in server, "missing device settings endpoint"
    assert "renderSettings(data.config, data.device.ap_enabled)" in html, \
        "page must render the menu settings from device state"
    assert "state.settingsDirty" in html, "unsaved settings must survive polling"

    script = re.findall(r"<script>(.*?)</script>", html, re.S)
    assert script, "missing inline script"
    node = shutil.which("node")
    if node:
        with tempfile.NamedTemporaryFile("w", suffix=".js", delete=False,
                                         encoding="utf-8") as handle:
            handle.write(script[-1])
            script_path = Path(handle.name)
        try:
            result = subprocess.run([node, "--check", str(script_path)],
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, f"inline script syntax error:\n{result.stderr}"
        finally:
            script_path.unlink(missing_ok=True)
    else:
        print("node not found; skipped JS syntax check")

    print(f"web page: {len(ids)} ids, {len(calls)} API endpoints, escaped output; tests: PASS")


if __name__ == "__main__":
    main()
