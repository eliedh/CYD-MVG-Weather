// All user-facing device strings in German and English.
// Every entry must provide both languages (enforced by the X-macro shape and
// checked for emptiness/format-specifier parity by test/test_i18n).
// The web pages carry their own DE/EN dictionaries (checked by tools/build_web.py).
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "settings.h"

namespace core {

// clang-format off
#define CORE_I18N_TABLE(X) \
  X(Starting,          "Startet …",                                   "Starting…") \
  X(WelcomeTitle,      "Willkommen!",                                  "Welcome!") \
  X(SetupStep1,        "Code mit dem Handy scannen",                   "Scan the code with your phone") \
  X(SetupStep2,        "Seite öffnet sich",                            "Setup page opens") \
  X(SetupStep3,        "WLAN wählen – fertig!",                        "Pick your Wi-Fi – done!") \
  X(SetupOpenNet,      "kein Passwort",                                "no password") \
  X(SetupNoPage,       "Seite öffnet nicht? %s",                       "Page not opening? %s") \
  X(ConnectingTitle,   "Verbinde …",                                   "Connecting…") \
  X(ConnectingBody,    "mit „%s“",                                     "to “%s”") \
  X(NoWifiTitle,       "Kein WLAN",                                    "No Wi-Fi") \
  X(NoWifiBody,        "„%s“ ist nicht erreichbar.",                   "Can’t reach “%s”.") \
  X(NoWifiRetry,       "Neuer Versuch läuft automatisch.",             "Retrying automatically.") \
  X(NoWifiHint,        "Lange tippen: WLAN neu einrichten",            "Long-press: set up Wi-Fi again") \
  X(AlmostTitle,       "Fast fertig!",                                 "Almost done!") \
  X(AlmostBody,        "Code scannen und Haltestellen wählen",         "Scan the code to choose your stops") \
  X(AlmostSameWifi,    "Das Handy muss im selben WLAN sein.",          "Your phone must be on the same Wi-Fi.") \
  X(SettingsTitle,     "Einstellungen",                                "Settings") \
  X(SettingsBody,      "Im selben WLAN mit dem Handy öffnen:",         "Open on a phone in the same Wi-Fi:") \
  X(TapToClose,        "Tippen zum Schließen",                         "Tap to close") \
  X(Now,               "jetzt",                                        "now") \
  X(MinShort,          "Min.",                                         "min") \
  X(LeaveIn,           "in %d Min. losgehen",                          "leave in %d min") \
  X(LeaveNow,          "jetzt losgehen",                               "leave now") \
  X(Cancelled,         "fällt aus",                                    "cancelled") \
  X(StaleMin,          "Stand vor %d Min.",                            "%d min old") \
  X(StaleHour,         "Stand vor %d Std.",                            "%d h old") \
  X(NoDepartures,      "Gerade keine erreichbaren Abfahrten",          "No reachable departures right now") \
  X(Loading,           "Abfahrten werden geladen …",                   "Loading departures…") \
  X(ApiError,          "MVG gerade nicht erreichbar",                  "MVG is unreachable right now") \
  X(WeatherTitle,      "Wetter",                                       "Weather") \
  X(FeelsLike,         "gefühlt %d°",                                  "feels like %d°") \
  X(Wind,              "Wind %d km/h",                                 "wind %d km/h") \
  X(RainChance,        "Regen %d %%",                                  "rain %d%%") \
  X(NextHours,         "Die nächsten Stunden",                         "Next hours") \
  X(NowCap,            "Jetzt",                                        "Now") \
  X(WeatherNA,         "Wetter nicht verfügbar",                       "Weather unavailable") \
  X(Notice,            "Betriebshinweis",                              "Service notice") \
  X(MoreNotices,       "+%d weitere",                                  "+%d more") \
  X(PageOf,            "%d / %d",                                      "%d / %d") \
  X(TapNext,           "Tippen für mehr",                              "Tap for more") \
  X(ResetHold,         "Halten zum Zurücksetzen … %d",                 "Keep holding to reset… %d") \
  X(ResetRelease,      "Jetzt loslassen: Touch kalibrieren",           "Release now: calibrate touch") \
  X(ResetButton,       "Zurücksetzen …",                               "Reset…") \
  X(ResetAskTitle,     "Alles zurücksetzen?",                          "Reset everything?") \
  X(ResetAskBody,      "WLAN, Haltestellen und alle Einstellungen werden gelöscht. Danach startet die Einrichtung neu.", "Wi-Fi, stops and all settings will be erased. Setup then starts again.") \
  X(Cancel,            "Abbrechen",                                    "Cancel") \
  X(Erase,             "Löschen",                                      "Erase") \
  X(HoldToErase,       "Zum Löschen „Löschen“ gedrückt halten",        "Press and hold “Erase” to erase") \
  X(ResetDone,         "Zurückgesetzt. Neustart …",                    "Reset done. Restarting…") \
  X(CalTitle,          "Touch kalibrieren",                            "Calibrate touch") \
  X(CalBody,           "Tippe genau auf die Pfeilspitze in jeder Ecke.", "Tap exactly on the arrow tip in each corner.") \
  X(CalDone,           "Gespeichert",                                  "Saved") \
  X(WifiLost,          "WLAN getrennt",                                "Wi-Fi lost") \
  X(Wx0,  "Klar",                         "Clear") \
  X(Wx1,  "Überwiegend klar",             "Mainly clear") \
  X(Wx2,  "Teils bewölkt",                "Partly cloudy") \
  X(Wx3,  "Bedeckt",                      "Overcast") \
  X(Wx45, "Nebel",                        "Fog") \
  X(Wx48, "Reifnebel",                    "Rime fog") \
  X(Wx51, "Leichter Nieselregen",         "Light drizzle") \
  X(Wx53, "Nieselregen",                  "Drizzle") \
  X(Wx55, "Starker Nieselregen",          "Dense drizzle") \
  X(Wx56, "Gefrierender Nieselregen",     "Freezing drizzle") \
  X(Wx57, "Starker gefr. Nieselregen",    "Heavy freezing drizzle") \
  X(Wx61, "Leichter Regen",               "Light rain") \
  X(Wx63, "Regen",                        "Rain") \
  X(Wx65, "Starker Regen",                "Heavy rain") \
  X(Wx66, "Gefrierender Regen",           "Freezing rain") \
  X(Wx67, "Starker gefr. Regen",          "Heavy freezing rain") \
  X(Wx71, "Leichter Schneefall",          "Light snow") \
  X(Wx73, "Schneefall",                   "Snow") \
  X(Wx75, "Starker Schneefall",           "Heavy snow") \
  X(Wx77, "Schneegriesel",                "Snow grains") \
  X(Wx80, "Leichte Regenschauer",         "Light showers") \
  X(Wx81, "Regenschauer",                 "Rain showers") \
  X(Wx82, "Heftige Regenschauer",         "Violent showers") \
  X(Wx85, "Schneeschauer",                "Snow showers") \
  X(Wx86, "Starke Schneeschauer",         "Heavy snow showers") \
  X(Wx95, "Gewitter",                     "Thunderstorm") \
  X(Wx96, "Gewitter mit Hagel",           "Thunderstorm with hail") \
  X(Wx99, "Schweres Gewitter mit Hagel",  "Severe thunderstorm, hail") \
  X(WxUnknown, "Wetter",                  "Weather") \
  X(Day0, "So.", "Sun") X(Day1, "Mo.", "Mon") X(Day2, "Di.", "Tue") X(Day3, "Mi.", "Wed") \
  X(Day4, "Do.", "Thu") X(Day5, "Fr.", "Fri") X(Day6, "Sa.", "Sat") \
  X(Mon1, "Jan.", "Jan") X(Mon2, "Feb.", "Feb") X(Mon3, "März", "Mar") X(Mon4, "Apr.", "Apr") \
  X(Mon5, "Mai", "May") X(Mon6, "Juni", "Jun") X(Mon7, "Juli", "Jul") X(Mon8, "Aug.", "Aug") \
  X(Mon9, "Sept.", "Sep") X(Mon10, "Okt.", "Oct") X(Mon11, "Nov.", "Nov") X(Mon12, "Dez.", "Dec")
// clang-format on

enum class Str : uint16_t {
#define CORE_I18N_ENUM(key, de, en) key,
  CORE_I18N_TABLE(CORE_I18N_ENUM)
#undef CORE_I18N_ENUM
      Count
};

const char* tr(Str key, Lang lang);
const char* weatherText(uint8_t wmoCode, Lang lang);

// "Mi., 8. Okt." / "Wed, 8 Oct"
void formatDate(char* out, size_t size, const struct tm& t, Lang lang);

}  // namespace core
