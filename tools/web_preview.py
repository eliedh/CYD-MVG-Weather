#!/usr/bin/env python3
"""Preview the device's web pages without a device.

  python3 tools/web_preview.py serve         # http://localhost:8080/  (settings)
                                             # http://localhost:8080/setup
  python3 tools/web_preview.py screenshots   # docs/screenshots/web_*.png (needs playwright)

Serves web/*.html with a mock of the firmware's JSON API, using the fixtures
in test/fixtures for stop search and line lists.
"""
import json
import os
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIX = os.path.join(ROOT, "test", "fixtures")
TYPES = ["UBAHN", "SBAHN", "TRAM", "BUS", "REGIONAL_BUS", "BAHN", "SCHIFF"]

STATE = {
    "settings": {
        "v": 1, "lang": "de",
        "stops": [
            {"id": "de:09162:500", "name": "Münchner Freiheit", "label": "Münchner Freiheit",
             "lat": 48.16153, "lon": 11.5863, "walk": 4, "types": 255, "exclude": ["N40"]},
            {"id": "de:09188:7520", "name": "Gauting, Bahnhof", "label": "Gauting",
             "lat": 48.06321, "lon": 11.37766, "walk": 9, "types": 255, "exclude": []},
        ],
        "weather": {"override": False, "lat": 48.1374, "lon": 11.5755, "name": ""},
        "display": {"brightness": 80, "auto": True},
        "night": {"mode": "dim", "start": 1320, "end": 390},
    },
    "jobs": {},
    "next": 1,
}


def load(name):
    with open(os.path.join(FIX, name), encoding="utf-8") as f:
        return json.load(f)


def search_result():
    out = []
    for s in load("mvg_locations_freiheit.json"):
        if s.get("type") != "STATION":
            continue
        mask = 0
        for t in s.get("transportTypes", []):
            if t in TYPES:
                mask |= 1 << TYPES.index(t)
        out.append({"id": s["globalId"], "name": s["name"], "place": s["place"],
                    "lat": s["latitude"], "lon": s["longitude"], "types": mask})
    return out


def lines_result(stop_id):
    fx = "mvg_departures_gauting.json" if "7520" in stop_id else "mvg_departures_freiheit.json"
    seen, out = set(), []
    for d in load(fx):
        k = (d["label"], d["destination"])
        if k not in seen:
            seen.add(k)
            out.append({"label": d["label"], "dest": d["destination"], "type": d["transportType"]})
    out.sort(key=lambda x: (TYPES.index(x["type"]), len(x["label"]), x["label"], x["dest"]))
    return out


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def send(self, code, body, ctype="application/json"):
        data = body.encode("utf-8") if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def json(self, obj, code=200):
        self.send(code, json.dumps(obj, ensure_ascii=False))

    def page(self, name):
        with open(os.path.join(ROOT, "web", name), "rb") as f:
            self.send(200, f.read(), "text/html; charset=utf-8")

    def new_job(self, result):
        jid = STATE["next"]
        STATE["next"] += 1
        STATE["jobs"][jid] = (time.time() + 0.6, result)
        self.json({"job": jid})

    def do_GET(self):
        u = urlparse(self.path)
        q = parse_qs(u.query)
        if u.path in ("/", "/settings"):
            return self.page("settings.html")
        if u.path == "/setup":
            return self.page("setup.html")
        if u.path == "/api/info":
            return self.json({"portal": False, "net": "online", "ssid": "FRITZ!Box 7590 KL",
                              "ap": "Abfahrt-Setup-7F3A", "ip": "192.168.178.47",
                              "host": "abfahrt.local", "rssi": -58, "timeValid": True,
                              "lang": STATE["settings"]["lang"], "lastUpdate": int(time.time()) - 20,
                              "apiError": False, "version": "1.0.0", "heap": 118432, "uptime": 93784})
        if u.path == "/api/scan":
            return self.json({"scanning": False, "networks": [
                {"ssid": "FRITZ!Box 7590 KL", "rssi": -48, "open": False},
                {"ssid": "Vodafone-B2C4", "rssi": -63, "open": False},
                {"ssid": "MVG-Gast", "rssi": -71, "open": True},
                {"ssid": "o2-WLAN93", "rssi": -82, "open": False}]})
        if u.path == "/api/wifistatus":
            return self.json({"state": "ok", "ip": "192.168.178.47", "ssid": "FRITZ!Box 7590 KL",
                              "host": "abfahrt.local"})
        if u.path == "/api/settings":
            return self.json(STATE["settings"])
        if u.path == "/api/search":
            return self.new_job(search_result())
        if u.path == "/api/lines":
            return self.new_job(lines_result(q.get("id", [""])[0]))
        if u.path == "/api/job":
            jid = int(q.get("id", ["0"])[0])
            ready, result = STATE["jobs"].get(jid, (0, None))
            if result is None:
                return self.json({"ok": False}, 404)
            if time.time() < ready:
                return self.json({"state": "pending"})
            return self.json({"state": "done", "result": result})
        self.send(404, "not found", "text/plain")

    def do_POST(self):
        n = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(n).decode("utf-8") if n else ""
        if self.path == "/api/settings":
            STATE["settings"].update(json.loads(body))
            return self.json(STATE["settings"])
        return self.json({"ok": True})


def serve(port=8080):
    srv = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    return srv


def screenshots():
    from playwright.sync_api import sync_playwright

    srv = serve(8765)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    out = os.path.join(ROOT, "docs", "screenshots")
    os.makedirs(out, exist_ok=True)
    exe = "/opt/pw-browsers/chromium"  # preinstalled in some CI images; else default
    kw = {"executable_path": exe} if os.path.exists(exe) else {}
    base = "http://127.0.0.1:8765"
    with sync_playwright() as p:
        b = p.chromium.launch(**kw)
        pg = b.new_page(viewport={"width": 390, "height": 844}, device_scale_factor=2)

        pg.goto(base + "/setup")
        pg.wait_for_timeout(300)
        pg.screenshot(path=os.path.join(out, "web_setup_1_language.png"))
        pg.click("text=Weiter")
        pg.wait_for_timeout(500)
        pg.click("text=FRITZ!Box 7590 KL")
        pg.fill("#pass", "geheim123")
        pg.wait_for_timeout(400)
        pg.screenshot(path=os.path.join(out, "web_setup_2_wifi.png"))
        pg.click("#form .btn")
        pg.wait_for_timeout(1800)
        pg.screenshot(path=os.path.join(out, "web_setup_3_done.png"))

        pg.goto(base + "/")
        pg.wait_for_timeout(500)
        pg.screenshot(path=os.path.join(out, "web_settings_1_stops.png"))
        pg.click("#stops .card:first-child .expander")
        pg.wait_for_timeout(1500)
        pg.locator("#stops .card:first-child .lines").scroll_into_view_if_needed()
        pg.screenshot(path=os.path.join(out, "web_settings_2_lines.png"))
        pg.fill("#q", "Freiheit")
        pg.click("#addCard .btn")
        pg.wait_for_timeout(1500)
        pg.locator("#addCard").scroll_into_view_if_needed()
        pg.screenshot(path=os.path.join(out, "web_settings_3_search.png"))
        pg.click("#langSeg button[data-v=en]")
        pg.wait_for_timeout(300)
        pg.screenshot(path=os.path.join(out, "web_settings_4_full_en.png"), full_page=True)
        b.close()
    srv.shutdown()
    print("web screenshots written to", out)


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "screenshots":
        screenshots()
    else:
        s = serve(8080)
        print("serving on http://localhost:8080/ (settings) and /setup")
        s.serve_forever()
