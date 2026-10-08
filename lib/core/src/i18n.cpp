#include "i18n.h"

#include <stdio.h>

namespace core {

namespace {
#define CORE_I18N_DE(key, de, en) de,
#define CORE_I18N_EN(key, de, en) en,
const char* const kDe[] = {CORE_I18N_TABLE(CORE_I18N_DE)};
const char* const kEn[] = {CORE_I18N_TABLE(CORE_I18N_EN)};
#undef CORE_I18N_DE
#undef CORE_I18N_EN
static_assert(sizeof(kDe) / sizeof(kDe[0]) == (size_t)Str::Count, "DE table incomplete");
static_assert(sizeof(kEn) / sizeof(kEn[0]) == (size_t)Str::Count, "EN table incomplete");
}  // namespace

const char* tr(Str key, Lang lang) {
  size_t i = (size_t)key;
  if (i >= (size_t)Str::Count) return "";
  return lang == Lang::En ? kEn[i] : kDe[i];
}

const char* weatherText(uint8_t c, Lang l) {
  Str k;
  switch (c) {
    case 0: k = Str::Wx0; break;
    case 1: k = Str::Wx1; break;
    case 2: k = Str::Wx2; break;
    case 3: k = Str::Wx3; break;
    case 45: k = Str::Wx45; break;
    case 48: k = Str::Wx48; break;
    case 51: k = Str::Wx51; break;
    case 53: k = Str::Wx53; break;
    case 55: k = Str::Wx55; break;
    case 56: k = Str::Wx56; break;
    case 57: k = Str::Wx57; break;
    case 61: k = Str::Wx61; break;
    case 63: k = Str::Wx63; break;
    case 65: k = Str::Wx65; break;
    case 66: k = Str::Wx66; break;
    case 67: k = Str::Wx67; break;
    case 71: k = Str::Wx71; break;
    case 73: k = Str::Wx73; break;
    case 75: k = Str::Wx75; break;
    case 77: k = Str::Wx77; break;
    case 80: k = Str::Wx80; break;
    case 81: k = Str::Wx81; break;
    case 82: k = Str::Wx82; break;
    case 85: k = Str::Wx85; break;
    case 86: k = Str::Wx86; break;
    case 95: k = Str::Wx95; break;
    case 96: k = Str::Wx96; break;
    case 99: k = Str::Wx99; break;
    default: k = Str::WxUnknown; break;
  }
  return tr(k, l);
}

void formatDate(char* out, size_t size, const struct tm& t, Lang l) {
  const char* day = tr((Str)((int)Str::Day0 + (t.tm_wday % 7)), l);
  const char* mon = tr((Str)((int)Str::Mon1 + (t.tm_mon % 12)), l);
  if (l == Lang::En) snprintf(out, size, "%s, %d %s", day, t.tm_mday, mon);
  else snprintf(out, size, "%s, %d. %s", day, t.tm_mday, mon);
}

}  // namespace core
