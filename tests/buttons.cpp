#include <cassert>
#include <T1PocketButtons.h>

int main() {
  using B = T1PocketButtons;
  B buttons;
  uint32_t now = UINT32_MAX - 350;  // Test gestures across the millisecond rollover.
  auto sample = [&](B::Keys keys, uint32_t elapsed) {
    now += elapsed;
    return buttons.poll(keys, now);
  };
  auto tap = [&](B::Keys key, B::Event expected) {
    assert(sample(key, 2)==B::None);
    assert(sample(key, 26)==B::None);
    assert(sample(B::Released, 45)==B::None);
    assert(sample(B::Released, 25)==expected);
    assert(!buttons.isPressed());
  };
  auto hold = [&](B::Keys key, B::Event expected) {
    assert(sample(key, 10)==B::None);
    assert(buttons.isPressed());
    assert(sample(key, 25)==B::None);
    assert(sample(key, 974)==B::None);
    assert(sample(key, 1)==expected);
    assert(sample(key, 2000)==B::None);
    assert(sample(B::Released, 5)==B::None);
    assert(buttons.isPressed());
    assert(sample(B::Released, 25)==B::None);
    assert(!buttons.isPressed());
  };
  // Switch bounce shorter than a stable press creates no gesture.
  assert(sample(B::Upper, 5)==B::None);
  assert(sample(B::Released, 6)==B::None);
  assert(sample(B::Released, 25)==B::None);
  tap(B::Upper, B::Next);
  tap(B::Upper, B::Next);
  tap(B::Lower, B::Previous);
  hold(B::Upper, B::UpperHold);
  hold(B::Lower, B::LowerHold);

  // A short overlap cancels both gestures, including the last release.
  assert(sample(B::Lower, 10)==B::None);
  assert(sample(B::Lower, 25)==B::None);
  assert(sample(B::Both, 2)==B::None);
  assert(sample(B::Upper, 4)==B::None);
  assert(sample(B::Upper, 1500)==B::None);
  assert(sample(B::Upper, 1500)==B::None);
  assert(sample(B::Released, 2)==B::None);
  assert(sample(B::Released, 25)==B::None);
  // Connecting while a PIN is showing cancels any in-flight gesture.
  assert(sample(B::Lower, 50)==B::None);
  assert(sample(B::Lower, 26)==B::None);
  buttons.cancelUntilRelease();
  assert(sample(B::Lower, 1500)==B::None);
  assert(sample(B::Released, 2)==B::None);
  assert(sample(B::Released, 25)==B::None);
  tap(B::Lower, B::Previous);
}
