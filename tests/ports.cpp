#include <cassert>
#include <cstddef>
#include "T1CompanionPort.h"
#include "T1CompanionClock.h"
uint32_t testTicks = 0;

class Port : public BaseSerialInterface {
public:
  bool enabled = false, connected = false;
  unsigned written = 0;
  uint8_t pending = 0;
  void enable() override { enabled = true; }
  void disable() override { enabled = false; }
  bool isEnabled() const override { return enabled; }
  bool isConnected() const override { return connected; }
  bool isWriteBusy() const override { return false; }
  size_t writeFrame(const uint8_t*, size_t len) override { ++written; return len; }
  size_t checkRecvFrame(uint8_t dest[]) override {
    if (!pending) return 0;
    dest[0] = pending;
    pending = 0;
    return 1;
  }
};

int main() {
  T1CompanionClock clock;
  assert(clock.contactTimeForClient(2000000000UL) == 1);
  assert(clock.contactCursorFromClient(2000000000UL) == 0);
  clock.setCurrentTime(1795472407UL);
  assert(clock.contactTimeForClient(2400000000UL) == 1);
  assert(clock.contactCursorFromClient(2400000000UL) == 0);
  assert(clock.contactTimeForClient(4201255210UL) == 1);
  assert(clock.contactCursorFromClient(4201255210UL) == 0);
  assert(clock.contactTimeForClient(1795472407UL) == 1795472407UL);
  assert(clock.contactCursorFromClient(1795472407UL) == 1795472407UL);
  T1CompanionPort manager;
  Port ble, usb;
  manager.addInterface(InterfaceType::Bluetooth, &ble);
  manager.addInterface(InterfaceType::USB, &usb);
  manager.bind(ble, usb);
  manager.enable();
  uint8_t data[MAX_FRAME_SIZE] = {};
  usb.connected = true;
  assert(!manager.isConnected()); // opening a monitor leaves the radio available
  ble.connected = true;
  ble.pending = 1;
  assert(manager.checkRecvFrame(data) == 1 && data[0] == 1);
  assert(!manager.isUsbOwner());
  usb.pending = 5;
  assert(manager.checkRecvFrame(data) == 0 && !usb.pending);
  assert(manager.writeFrame(data,1) == 1 && ble.written == 1 && !usb.written);
  ble.connected = false;
  manager.loop();
  assert(!manager.isConnected() && manager.writeFrame(data,1) == 0);
  usb.pending = 1;
  assert(manager.checkRecvFrame(data) == 1);
  ble.connected = true;
  assert(manager.isUsbOwner());
  ble.pending = 5;
  assert(manager.checkRecvFrame(data) == 0 && !ble.pending);
  assert(manager.writeFrame(data,1) == 1 && usb.written == 1 && ble.written == 1);
  usb.connected = false;
  manager.loop();
  ble.pending = 1;
  assert(manager.checkRecvFrame(data) == 1 && manager.isConnected());
  manager.disable();
  assert(!manager.isConnected() && !manager.checkRecvFrame(data));
}
