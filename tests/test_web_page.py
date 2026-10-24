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
}
REQUIRED_TEXT = ("股票盯盘", "Wi-Fi 配网", "自选股", "192.168.4.1", "新浪财经")
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

    calls = set(FETCH_RE.findall(html))
    assert calls, "no API calls found"
    routes = {(path, method) for path, method in ROUTE_RE.findall(server)}
    for path in calls:
        assert any(route_path == path for route_path, _ in routes), \
            f"page calls unregistered endpoint {path}"
    for path in ("/api/status", "/api/config", "/api/scan", "/api/wifi",
                 "/api/wifi/forget", "/api/portal/close"):
        assert path in calls, f"page never calls {path}"

    assert "function esc(" in html, "dynamic HTML must be escaped"
    assert html.count("esc(") >= 10, "esc() is not used consistently"
    assert "response.ok" in html and "JSON.parse" in html, "API errors are not handled"
    assert "ap_enabled" in server, "status API must expose the hotspot switch"
    assert "data.device.ap_enabled" in html, "page must reflect a disabled hotspot"

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
