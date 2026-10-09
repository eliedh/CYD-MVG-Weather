#include "vm_builder.h"

#include <string.h>

#include "text_util.h"

namespace ui {

void fillFromSnapshot(ViewModel& vm, const core::Settings& s, const core::DataSnapshot& d,
                      int64_t now, bool timeValid) {
  vm.lang = s.lang;
  vm.now = now;
  vm.timeValid = timeValid;
  if (timeValid) {
    time_t t = (time_t)now;
    localtime_r(&t, &vm.local);
  }
  vm.weather = d.weather;

  vm.stopCount = (int)s.stops.size();
  for (int i = 0; i < core::kMaxStops; i++) {
    vm.stopLabels[i][0] = 0;
    if (i < vm.stopCount) core::copyUtf8(vm.stopLabels[i], sizeof(vm.stopLabels[i]),
                                         s.stops[i].label.c_str());
  }
  vm.hasDepartureData = false;
  for (int i = 0; i < vm.stopCount && i < core::kMaxStops; i++)
    if (d.stops[i].valid) vm.hasDepartureData = true;
  vm.departuresError = d.departuresError;
  vm.departuresFetchedAt = d.departuresFetchedAt;
  vm.rowCount = timeValid ? core::buildBoard(d.stops, s, now, vm.rows, kMaxBoardRows) : 0;

  // (boardPage is clamped when drawing; the app resets it after a timeout)
  vm.messageCount = d.messageCount < core::kMaxMessages ? d.messageCount : core::kMaxMessages;
  vm.messages = d.messages;
  if (vm.messageIndex >= vm.messageCount) {
    vm.messageIndex = 0;
    vm.messagePage = 0;
  }
}

}  // namespace ui
