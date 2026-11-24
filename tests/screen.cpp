// Exercise the real page class using the ST7735 driver's coordinate scale and
// fixed-width font metrics. Output drawing commands for offline previews.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <helpers/ui/UIScreen.h>
#include <T1CompanionClock.h>

uint32_t testTicks = 0;
T1CompanionClock rtc_clock;
struct { bool usb=true; bool isExternalPowered() { return usb; } } board;
struct {
  unsigned sent=0;
  bool flooded=false;
  uint32_t pin=42;
  bool advert(bool flood) { ++sent; flooded=flood; return true; }
  uint32_t getBLEPin() { return pin; }
} the_mesh;
struct {
  bool enabled=true, connected=false;
  bool isEnabled() const { return enabled; }
  bool isConnected() const { return connected; }
} bluetooth_interface;
struct NodePrefs { char node_name[32]="Pocket"; };
struct SensorManager {};
struct UITask {
  bool gps=false, pressed=false, shutdownRequested=false;
  std::string alert;
  bool getGPSState() { return gps; }
  unsigned millivolts=3810;
  unsigned getBattMilliVolts() { return millivolts; }
  int getMsgCount() { return 2; }
  void showAlert(const char* msg, int) { alert=msg; }
  void toggleGPS() { gps=!gps; }
  bool isButtonPressed() { return pressed; }
  void shutdown() { shutdownRequested=true; }
};
#include <T1HomeScreen.h>

ColorVal UIColor::title_bkg=0x4227, UIColor::title_txt=0xbcf0,
         UIColor::primary_txt=0xded8, UIColor::secondary_txt=0x9cf1,
         UIColor::warning_txt=0xe58d;

struct Screen : public DisplayDriver {
  int x=0, y=0, size=1;
  ColorVal color=0;
  std::string largeText;
  std::string zone, voltage;
  std::string hint, detail;
  int dots=0, bars=0;
  bool plug=false;
  Screen() : DisplayDriver(128,64) {}
  bool isOn() override {return true;}
  void turnOn() override {}
  void turnOff() override {}
  void clear() override {}
  void startFrame(ColorVal) override {}
  void setTextSize(int s) override {size=s;}
  void setColor(ColorVal c) override {color=c;}
  void setCursor(int a,int b) override {x=int(1.25*a);y=int(1.25*b);}
  void print(const char* s) override {
    assert(x>=0 && x+int(strlen(s))*6*size<=160 && y+8*size<=80);
    if (y==67) hint=s;
    if (y==48) detail=s;
    if (size == 2) largeText=s;
    if (strcmp(s,"PCT")==0 || strcmp(s,"UTC")==0) zone=s;
    if (strstr(s," V")) voltage=s;
    printf("T %u %d %d %d %s\n",color,x,y,size,s);
  }
  void fillRect(int a,int b,int w,int h) override {
    assert(a>=0 && b>=0 && a+w<=128 && b+h<=64);
    if (b==56 && w==3) ++dots;
    if (b==5 && w==2 && h==4 && a>=107 && a<=119) ++bars;
    if (a==110 && b==6 && w==7 && h==2) plug=true;
    printf("R %u %d %d %d %d\n",color,int(a*1.25),int(b*1.25),int(w*1.25),int(h*1.25));
  }
  void drawRect(int,int,int,int) override {}
  void drawXbm(int,int,const uint8_t*,int,int) override {}
  uint16_t getTextWidth(const char* s) override {return strlen(s)*6*size/1.25;}
  void endFrame() override {}
};
int main(){
  UITask task;
  NodePrefs prefs;
  Screen display;
  HomeScreen home(&task,&rtc_clock,nullptr,&prefs);
  puts("PAGE sync");home.render(display);
  assert(display.largeText.empty() && display.zone.empty());
  assert(display.plug && display.bars==0 && display.voltage=="3.81 V");
  home.handleInput(KEY_ENTER);
  assert(!the_mesh.sent && !task.alert.empty());
  const struct { uint32_t utc; const char* pacific; const char* universal; } times[] = {
    {1795503540, "23:59", "06:59"},
    {1795503600, "00:00", "07:00"},
    {1782919800, "08:30", "15:30"},
  };
  for (const auto& test : times) {
    rtc_clock.setCurrentTime(test.utc);
    display.zone.clear();
    puts("PAGE zone-check");home.render(display);
#ifdef T1_PACIFIC_TIME
    assert(display.largeText==test.pacific && display.zone=="PCT");
#else
    assert(display.largeText==test.universal && display.zone=="UTC");
#endif
    assert(rtc_clock.getCurrentTime()==test.utc);
  }
  rtc_clock.setCurrentTime(1795470300);
  board.usb=false;
  for (const auto& test : {std::make_pair(3300u,0), {3450u,1}, {3650u,2}, {3800u,3}, {4000u,4}, {0u,0}, {5000u,0}}) {
    display.bars=0; display.plug=false; task.millivolts=test.first;
    puts("PAGE battery-check");home.render(display);
    assert(display.bars==test.second && !display.plug);
    if (test.first==0 || test.first==5000) assert(display.voltage=="-- V");
  }
  task.millivolts=3810;
  puts("PAGE home");home.render(display);
  home.handleInput(KEY_ENTER);
  assert(the_mesh.sent==1);
#ifdef T1_FLOOD_ADVERT
  assert(the_mesh.flooded);
#else
  assert(!the_mesh.flooded);
#endif
  board.usb=true;
  home.handleInput(KEY_NEXT);
  display.detail.clear();
  puts("PAGE gps-off");home.render(display);
  assert(display.detail.empty() && display.hint=="Hold to turn GPS on");
  home.handleInput(KEY_ENTER);assert(task.gps);
  puts("PAGE gps-on");home.render(display);
  assert(display.detail.empty() && display.hint=="Hold to turn GPS off");
  home.handleInput(KEY_NEXT);
  puts("PAGE hibernate");home.render(display);
  home.handleInput(KEY_NEXT);
  puts("PAGE pairing");home.render(display);
  assert(display.largeText=="000042");
  assert(!home.handleInput(KEY_ENTER));
  home.poll();
  assert(!task.shutdownRequested && the_mesh.sent==1 && task.gps);
  auto draw = [&]() {
    display.dots=0;
    display.largeText.clear();
    home.render(display);
    return display.largeText;
  };
  // USB is present. Only a secured Bluetooth session hides the PIN.
  puts("PAGE pairing");
  home.wakeForButton();
  assert(draw()=="000042" && display.hint=="Tap to dismiss" && display.dots==4);
  puts("PAGE dismissal-check");
  assert(!home.handleInput(KEY_ENTER));
  home.handleInput(KEY_PREV);
  assert(draw().find(':')!=std::string::npos);
  home.wakeForButton();
  assert(draw().find(':')!=std::string::npos);
  home.handleInput(KEY_PREV);
  assert(draw()=="000042");
  bluetooth_interface.connected=true;
  puts("PAGE bluetooth-connected");
  // Catch a connection arriving between the input poll and the next paint.
  assert(draw().find(':')!=std::string::npos && display.dots==3);
  home.handleInput(KEY_ENTER);
  assert(the_mesh.sent==1);
  assert(home.updatePairingState());
  assert(!home.updatePairingState());
  home.wakeForButton();
  assert(draw().find(':')!=std::string::npos);
  home.handleInput(KEY_PREV);assert(draw()=="Hibernate");
  home.handleInput(KEY_NEXT);assert(draw().find(':')!=std::string::npos);
  home.handleInput(KEY_NEXT);assert(draw()=="GPS on");
  home.handleInput(KEY_NEXT);assert(draw()=="Hibernate");
  home.handleInput(KEY_NEXT);assert(draw().find(':')!=std::string::npos);

  // Disconnecting while awake leaves the current page alone, then promotes
  // pairing once on the next physical wake. Either tap can dismiss the prompt.
  home.handleInput(KEY_NEXT);
  bluetooth_interface.connected=false;
  assert(!home.updatePairingState() && draw()=="GPS on");
  home.resetPage();  // Explicit USB request to show Home.
  assert(draw().find(':')!=std::string::npos);
  home.wakeForButton();assert(draw()=="000042");
  home.handleInput(KEY_NEXT);assert(draw().find(':')!=std::string::npos);
  home.wakeForButton();assert(draw().find(':')!=std::string::npos);
  home.handleInput(KEY_PREV);assert(draw()=="000042");
  bluetooth_interface.enabled=false;
  assert(home.updatePairingState());
  home.wakeForButton();assert(draw().find(':')!=std::string::npos && display.dots==3);
  bluetooth_interface.enabled=true;
  the_mesh.pin=0;
  assert(!home.updatePairingState());
  home.wakeForButton();assert(draw().find(':')!=std::string::npos && display.dots==3);
  home.handleInput(KEY_PREV);assert(draw()=="Hibernate");
  assert(the_mesh.sent==1 && task.gps && !task.shutdownRequested);
  task.pressed=true;home.handleInput(KEY_ENTER);home.poll();assert(!task.shutdownRequested);
  task.pressed=false;home.poll();assert(task.shutdownRequested);
  home.resetPage();board.usb=false;
  strcpy(prefs.node_name,"A very long node name to clip");
  puts("PAGE long-name");home.render(display);
}
