#include <string.h>
#include <unity.h>

#include <string>

#include "i18n.h"

using namespace core;

void setUp() {}
void tearDown() {}

// Extracts the sequence of printf conversions ("%d%s") so both languages
// can be checked to take the same arguments in the same order.
static std::string specs(const char* s) {
  std::string r;
  for (const char* p = s; *p; p++) {
    if (*p != '%') continue;
    p++;
    if (*p == '%') continue;
    while (*p && strchr("-+ #0123456789.l", *p)) p++;
    if (*p) r += *p;
  }
  return r;
}

void test_all_keys_present_in_both_languages() {
  for (int i = 0; i < (int)Str::Count; i++) {
    const char* de = tr((Str)i, Lang::De);
    const char* en = tr((Str)i, Lang::En);
    TEST_ASSERT_NOT_NULL(de);
    TEST_ASSERT_NOT_NULL(en);
    char msg[48];
    snprintf(msg, sizeof(msg), "key index %d", i);
    TEST_ASSERT_TRUE_MESSAGE(strlen(de) > 0, msg);
    TEST_ASSERT_TRUE_MESSAGE(strlen(en) > 0, msg);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(specs(de).c_str(), specs(en).c_str(), msg);
  }
}

void test_weather_codes_translated() {
  const int codes[] = {0, 1, 2, 3, 45, 48, 51, 53, 55, 56, 57, 61, 63, 65, 66, 67,
                       71, 73, 75, 77, 80, 81, 82, 85, 86, 95, 96, 99};
  for (int c : codes) {
    TEST_ASSERT_TRUE(strcmp(weatherText(c, Lang::De), tr(Str::WxUnknown, Lang::De)) != 0);
    TEST_ASSERT_TRUE(strlen(weatherText(c, Lang::En)) > 0);
  }
  TEST_ASSERT_EQUAL_STRING("Bedeckt", weatherText(3, Lang::De));
  TEST_ASSERT_EQUAL_STRING("Overcast", weatherText(3, Lang::En));
  TEST_ASSERT_EQUAL_STRING("Wetter", weatherText(200, Lang::De));
}

void test_out_of_range_key() { TEST_ASSERT_EQUAL_STRING("", tr(Str::Count, Lang::De)); }

void test_date_format() {
  struct tm t = {};
  t.tm_wday = 4;  // Thursday
  t.tm_mday = 8;
  t.tm_mon = 9;  // October
  char buf[32];
  formatDate(buf, sizeof(buf), t, Lang::De);
  TEST_ASSERT_EQUAL_STRING("Do., 8. Okt.", buf);
  formatDate(buf, sizeof(buf), t, Lang::En);
  TEST_ASSERT_EQUAL_STRING("Thu, 8 Oct", buf);
}

// Every non-ASCII character used by the UI strings must be in the embedded
// fonts (Latin-1 + a few typographic extras, see tools/gen_fonts.py).
void test_strings_use_font_charset() {
  for (int i = 0; i < (int)Str::Count; i++) {
    for (Lang l : {Lang::De, Lang::En}) {
      const unsigned char* p = (const unsigned char*)tr((Str)i, l);
      while (*p) {
        uint32_t cp;
        if (*p < 0x80) { cp = *p++; }
        else if ((*p & 0xE0) == 0xC0) { cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); p += 2; }
        else { cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; }
        bool ok = cp < 0x100 || cp == 0x2013 || cp == 0x2014 || cp == 0x2018 || cp == 0x2019 ||
                  cp == 0x201C || cp == 0x201D || cp == 0x201E || cp == 0x2026 || cp == 0x2022;
        char msg[48];
        snprintf(msg, sizeof(msg), "key %d uses U+%04X", i, (unsigned)cp);
        TEST_ASSERT_TRUE_MESSAGE(ok, msg);
      }
    }
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_all_keys_present_in_both_languages);
  RUN_TEST(test_weather_codes_translated);
  RUN_TEST(test_out_of_range_key);
  RUN_TEST(test_date_format);
  RUN_TEST(test_strings_use_font_charset);
  return UNITY_END();
}
