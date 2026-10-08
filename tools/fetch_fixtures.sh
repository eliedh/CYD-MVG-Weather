#!/usr/bin/env bash
# Captures REAL API responses into test/fixtures/real/ so the parsers can be
# checked against live data. Run on any machine with internet access:
#
#   ./tools/fetch_fixtures.sh            # default stops
#   STOP_QUERY="Gauting" ./tools/fetch_fixtures.sh
#
# Then run:  pio test -e native   (test_real_fixtures picks up whatever exists)
# The synthetic fixtures in test/fixtures/*.json are left untouched.
set -euo pipefail

cd "$(dirname "$0")/.."
OUT=test/fixtures/real
mkdir -p "$OUT"
UA="Mozilla/5.0 (CYD-MVG-Weather fixture fetch)"
BASE="https://www.mvg.de/api/bgw-pt/v3"
QUERY="${STOP_QUERY:-Münchner Freiheit}"

urlencode() { python3 -c 'import sys,urllib.parse;print(urllib.parse.quote(sys.argv[1]))' "$1"; }

echo "-> locations for '$QUERY'"
curl -fsS -A "$UA" "$BASE/locations?query=$(urlencode "$QUERY")&locationTypes=STATION" \
  | python3 -m json.tool --no-ensure-ascii > "$OUT/mvg_locations.json"

GID=$(python3 -c 'import json;d=json.load(open("'"$OUT"'/mvg_locations.json"));print(next(x["globalId"] for x in d if x.get("type")=="STATION"))')
LAT=$(python3 -c 'import json;d=json.load(open("'"$OUT"'/mvg_locations.json"));print(next(x["latitude"] for x in d if x.get("type")=="STATION"))')
LON=$(python3 -c 'import json;d=json.load(open("'"$OUT"'/mvg_locations.json"));print(next(x["longitude"] for x in d if x.get("type")=="STATION"))')
echo "   first station: $GID ($LAT, $LON)"

echo "-> departures"
curl -fsS -A "$UA" "$BASE/departures?globalId=$GID&limit=20&transportTypes=UBAHN,TRAM,SBAHN,BUS,REGIONAL_BUS,BAHN" \
  | python3 -m json.tool --no-ensure-ascii > "$OUT/mvg_departures.json"

echo "-> messages (can be large)"
curl -fsS -A "$UA" "$BASE/messages" | python3 -m json.tool --no-ensure-ascii > "$OUT/mvg_messages.json"

echo "-> open-meteo"
curl -fsS "https://api.open-meteo.com/v1/forecast?latitude=$LAT&longitude=$LON&current=temperature_2m,apparent_temperature,weather_code,is_day,wind_speed_10m&hourly=temperature_2m,precipitation_probability,weather_code,is_day&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset&timezone=Europe%2FBerlin&timeformat=unixtime&forecast_days=2" \
  | python3 -m json.tool --no-ensure-ascii > "$OUT/openmeteo.json"

date +%s > "$OUT/CAPTURED_AT"
ls -la "$OUT"
echo "Done. Commit test/fixtures/real/ if you want CI to check them too."
