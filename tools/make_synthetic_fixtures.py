#!/usr/bin/env python3
"""Writes SYNTHETIC test fixtures to test/fixtures/.

The build environment that created this project could not reach mvg.de or
open-meteo.com (egress blocked), so these files were built by hand from the
publicly documented / widely used response shapes of
  - https://www.mvg.de/api/bgw-pt/v3/{locations,departures,messages}
  - https://api.open-meteo.com/v1/forecast (timeformat=unixtime)
Replace them with real captures via tools/fetch_fixtures.sh when you can; the
tests are written against relative times and should keep passing.

All times are relative to FIXTURE_NOW (2026-10-08 18:00:00 Europe/Berlin).
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "test", "fixtures")
NOW = 1791475200  # 2026-10-08T16:00:00Z == 18:00 CEST
MIN = 60


def ms(t):
    return int(t * 1000)


def dep(offset_min, label, ttype, dest, delay=0, realtime=True, cancelled=False,
        platform=None, network="swm", diva=None):
    planned = NOW + offset_min * MIN
    d = {
        "plannedDepartureTime": ms(planned),
        "realtime": realtime,
        "delayInMinutes": delay if realtime else 0,
        "realtimeDepartureTime": ms(planned + (delay * MIN if realtime else 0)),
        "transportType": ttype,
        "label": label,
        "divaId": diva or ("010" + label),
        "network": network,
        "trainType": "",
        "destination": dest,
        "cancelled": cancelled,
        "sev": False,
        "stopPositionNumber": 1,
        "messages": [],
        "infos": [],
        "bannerHash": "",
        "occupancy": "LOW",
        "stopPointGlobalId": "de:09162:500:1:1",
    }
    if platform is not None:
        d["platform"] = platform
        d["platformChanged"] = False
    return d


def write(name, obj):
    with open(os.path.join(OUT, name), "w", encoding="utf-8", newline="\n") as f:
        json.dump(obj, f, ensure_ascii=False, indent=1)
        f.write("\n")


def main():
    os.makedirs(OUT, exist_ok=True)

    write("mvg_locations_freiheit.json", [
        {"type": "STATION", "latitude": 48.16153, "longitude": 11.58630, "place": "München",
         "name": "Münchner Freiheit", "globalId": "de:09162:500", "divaId": 500,
         "hasZoomData": True, "transportTypes": ["UBAHN", "TRAM", "BUS"],
         "aliases": "Muenchner Freiheit Schwabing", "tariffZones": "m"},
        {"type": "STATION", "latitude": 48.15910, "longitude": 11.58440, "place": "München",
         "name": "Münchner Freiheit (Leopoldstraße)", "globalId": "de:09162:501",
         "divaId": 501, "hasZoomData": False, "transportTypes": ["BUS"], "tariffZones": "m"},
        {"type": "POI", "latitude": 48.1610, "longitude": 11.5860, "place": "München",
         "name": "Freiheiz (POI, must be skipped)", "poiType": "LEISURE"},
        {"type": "STATION", "latitude": 48.06321, "longitude": 11.37766, "place": "Gauting",
         "name": "Gauting, Bahnhof", "globalId": "de:09188:7520", "divaId": 7520,
         "transportTypes": ["SBAHN", "REGIONAL_BUS"], "tariffZones": "1"},
    ])

    write("mvg_departures_freiheit.json", [
        dep(-1, "U6", "UBAHN", "Klinikum Großhadern", platform=2),
        dep(0, "U3", "UBAHN", "Fürstenried West", platform=2),
        dep(2, "23", "TRAM", "Schwabing Nord", delay=1),
        dep(3, "U6", "UBAHN", "Garching, Forschungszentrum", platform=1),
        dep(4, "59", "BUS", "Giesing Bf.", realtime=False),
        dep(5, "U3", "UBAHN", "Moosach", platform=1, delay=2),
        dep(6, "U6", "UBAHN", "Klinikum Großhadern", platform=2, cancelled=True),
        dep(8, "142", "BUS", "Bogenhausen, Herkomerplatz"),
        dep(9, "U3", "UBAHN", "Fürstenried West", platform=2),
        dep(11, "23", "TRAM", "Parzivalplatz"),
        dep(12, "U6", "UBAHN", "Garching, Forschungszentrum", platform=1, delay=4),
        dep(14, "59", "BUS", "Giesing Bf.", realtime=False),
        dep(15, "U3", "UBAHN", "Moosach", platform=1),
        dep(18, "U6", "UBAHN", "Klinikum Großhadern", platform=2),
        dep(22, "N40", "BUS", "Kurt-Eisner-Straße", realtime=False),
        dep(75, "U3", "UBAHN", "Fürstenried West", platform=2, realtime=False),
    ])

    write("mvg_departures_gauting.json", [
        dep(1, "S6", "SBAHN", "Zorneding", platform=1, network="ddb", diva="92M06"),
        dep(4, "965", "REGIONAL_BUS", "Starnberg Nord", network="mvv", realtime=False),
        dep(7, "S6", "SBAHN", "Tutzing", platform=2, delay=3, network="ddb", diva="92M06"),
        dep(13, "966", "REGIONAL_BUS", "Planegg, Bahnhof", network="mvv"),
        dep(21, "S6", "SBAHN", "Zorneding", platform=1, network="ddb", diva="92M06"),
        dep(27, "S6", "SBAHN", "Tutzing", platform=2, network="ddb", diva="92M06"),
        dep(33, "967", "REGIONAL_BUS", "Krailling, Margaretenstraße", network="mvv",
            realtime=False),
    ])

    # Older/alternative shape: wrapped object, renamed fields, seconds not ms.
    write("mvg_departures_alt_shape.json", {"servingLines": [], "departures": [
        {"departureTimePlanned": NOW + 5 * MIN, "departureTime": NOW + 7 * MIN, "realTime": True,
         "product": "SBAHN", "line": {"label": "S8", "transportType": "SBAHN"},
         "direction": "Flughafen München", "isCancelled": False, "platform": "3"},
        {"departureTimePlanned": NOW + 9 * MIN, "product": "TRAM",
         "line": {"label": "17"}, "direction": "Amalienburgstraße"},
        {"label": "broken entry without any time"},
    ]})

    long_html = (
        "<p>Wegen Bauarbeiten zwischen <b>Sendlinger Tor</b> und &bdquo;Implerstra&szlig;e&ldquo; "
        "fahren die Linien U3 und U6 vom 10. bis 26.&nbsp;Oktober jeweils ab 22:00&nbsp;Uhr "
        "nur im 10-Minuten-Takt.</p><p>Bitte planen Sie mehr Zeit ein. "
        "Ersatzverkehr mit Bussen (SEV) ist eingerichtet.</p><ul><li>Haltestelle Goetheplatz: "
        "Ersatzhaltestelle in der Lindwurmstra&#223;e</li><li>Haltestelle Poccistra&szlig;e: "
        "kein Halt</li></ul>"
    )
    write("mvg_messages.json", [
        {"title": "U3/U6: Eingeschränkter Betrieb am Abend", "description": long_html,
         "publication": ms(NOW - 86400), "validFrom": ms(NOW - 3600), "validTo": ms(NOW + 86400 * 10),
         "type": "SCHEDULE_CHANGE", "provider": "MVG", "links": [],
         "lines": [{"label": "U3", "transportType": "UBAHN", "network": "swm", "divaId": "010U3",
                    "sev": False},
                   {"label": "U6", "transportType": "UBAHN", "network": "swm", "divaId": "010U6",
                    "sev": False}],
         "stationGlobalIds": ["de:09162:2", "de:09162:500"]},
        {"title": "Tram 23: Umleitung wegen Veranstaltung",
         "description": "<p>Die Tram 23 wird umgeleitet.</p>",
         "validFrom": ms(NOW - 86400 * 3), "validTo": ms(NOW - 600), "type": "INCIDENT",
         "lines": [{"label": "23", "transportType": "TRAM"}]},
        {"title": "Bus 100: Haltestellenverlegung",
         "description": "<p>Haltestelle verlegt.</p>",
         "validFrom": ms(NOW - 3600), "validTo": ms(NOW + 3600), "type": "INCIDENT",
         "lines": [{"label": "100", "transportType": "BUS"}]},
        {"title": "S6: Signalstörung bei Gauting",
         "description": "Verspätungen von bis zu 10 Minuten. Grund: Reparatur an einem Signal.",
         "validFrom": ms(NOW - 1200), "type": "INCIDENT",
         "lines": [{"label": "S6", "transportType": "SBAHN"}]},
        {"title": "Tram 23 (Bus-Linie gleicher Nummer, darf nicht passen)",
         "description": "x", "validFrom": ms(NOW - 60), "validTo": ms(NOW + 60),
         "lines": [{"label": "23", "transportType": "BUS"}]},
        {"title": "U3/U6: Eingeschränkter Betrieb am Abend", "description": "duplicate title",
         "validFrom": ms(NOW - 60),
         "lines": [{"label": "U6", "transportType": "UBAHN"}]},
    ])

    hours = [NOW - 2 * 3600 + i * 3600 for i in range(48)]
    temps = [13.8, 13.1, 12.4, 11.6, 10.9, 10.2, 9.6, 9.1, 8.7, 8.4, 8.2, 8.0]
    temps = temps + [8.0 + (i % 12) * 0.6 for i in range(36)]
    probs = [5, 10, 20, 35, 55, 60, 40, 20, 10, 5, 5, 0] + [0] * 36
    codes = [2, 3, 3, 61, 61, 63, 80, 3, 2, 1, 0, 0] + [1] * 36
    isday = [1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0] + [1] * 36
    write("openmeteo_munich.json", {
        "latitude": 48.16, "longitude": 11.58, "generationtime_ms": 0.08,
        "utc_offset_seconds": 7200, "timezone": "Europe/Berlin", "timezone_abbreviation": "GMT+2",
        "elevation": 519.0,
        "current_units": {"time": "unixtime", "interval": "seconds", "temperature_2m": "°C"},
        "current": {"time": NOW - 900, "interval": 900, "temperature_2m": 12.6,
                    "apparent_temperature": 10.9, "weather_code": 3, "is_day": 1,
                    "wind_speed_10m": 11.2},
        "hourly_units": {"time": "unixtime", "temperature_2m": "°C"},
        "hourly": {"time": hours, "temperature_2m": temps,
                   "precipitation_probability": probs, "weather_code": codes, "is_day": isday},
        "daily_units": {"time": "unixtime"},
        "daily": {"time": [NOW - 18 * 3600, NOW + 6 * 3600],
                  "temperature_2m_max": [15.2, 14.1], "temperature_2m_min": [7.9, 6.3],
                  "precipitation_probability_max": [60, 10],
                  "sunrise": [NOW - 9 * 3600 - 1260, NOW + 15 * 3600 - 1200],
                  "sunset": [NOW + 1800 + 120, NOW + 86400 + 1800]},
    })


if __name__ == "__main__":
    main()
