#include "line_style.h"

#include <string.h>

namespace core {

namespace {
constexpr uint32_t kWhite = 0xFFFFFF;

struct Named {
  const char* label;
  uint32_t bg, fg, bg2;
};

const Named kUBahn[] = {
    {"U1", 0x438136, kWhite, 0x438136}, {"U2", 0xC40C37, kWhite, 0xC40C37},
    {"U3", 0xF36E31, kWhite, 0xF36E31}, {"U4", 0x0AB38D, kWhite, 0x0AB38D},
    {"U5", 0xB8740E, kWhite, 0xB8740E}, {"U6", 0x006CB3, kWhite, 0x006CB3},
    {"U7", 0x438136, kWhite, 0xC40C37}, {"U8", 0xC40C37, kWhite, 0xF36E31},
};

const Named kSBahn[] = {
    {"S1", 0x16BAE7, kWhite, 0x16BAE7},   {"S2", 0x76B82A, kWhite, 0x76B82A},
    {"S3", 0x951B81, kWhite, 0x951B81},   {"S4", 0xE30613, kWhite, 0xE30613},
    {"S5", 0x005E82, kWhite, 0x005E82},   {"S6", 0x00975F, kWhite, 0x00975F},
    {"S7", 0x943126, kWhite, 0x943126},   {"S8", 0x1A1A1A, 0xFFCB06, 0x1A1A1A},
    {"S20", 0xF05A73, kWhite, 0xF05A73},  {"S27", 0xF05A73, kWhite, 0xF05A73},
};
}  // namespace

LineStyle lineStyle(TransportType type, const char* label) {
  if (!label) label = "";
  bool night = label[0] == 'N' && label[1] >= '0' && label[1] <= '9';
  switch (type) {
    case TransportType::UBahn:
      for (const Named& n : kUBahn)
        if (strcmp(n.label, label) == 0) return {n.bg, n.fg, n.bg2, BadgeShape::Rect};
      return {0x0065AE, kWhite, 0x0065AE, BadgeShape::Rect};
    case TransportType::SBahn:
      for (const Named& n : kSBahn)
        if (strcmp(n.label, label) == 0) return {n.bg, n.fg, n.bg2, BadgeShape::Pill};
      return {0x408335, kWhite, 0x408335, BadgeShape::Pill};
    case TransportType::Tram:
      if (night) return {0x8C1C24, kWhite, 0x8C1C24, BadgeShape::Rect};
      return {0xD82020, kWhite, 0xD82020, BadgeShape::Rect};
    case TransportType::Bus:
      if (night) return {0x24305E, kWhite, 0x24305E, BadgeShape::Rounded};
      if (label[0] == 'X') return {0x3E8B5E, kWhite, 0x3E8B5E, BadgeShape::Rounded};
      return {0x00586A, kWhite, 0x00586A, BadgeShape::Rounded};
    case TransportType::RegionalBus:
      return {0x1D5B8F, kWhite, 0x1D5B8F, BadgeShape::Rounded};
    case TransportType::Train:
      return {0x6B7280, kWhite, 0x6B7280, BadgeShape::Rect};
    case TransportType::Ship:
      return {0x0089C2, kWhite, 0x0089C2, BadgeShape::Rounded};
    default:
      return {0x4B5563, kWhite, 0x4B5563, BadgeShape::Rounded};
  }
}

}  // namespace core
