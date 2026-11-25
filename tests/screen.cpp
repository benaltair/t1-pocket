// Exercise the real page class using the ST7735 driver's coordinate scale and
// fixed-width font metrics. Output drawing commands for offline previews.
#include <cassert>
#include <ctime>
#include <T1BuildSettings.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <Packet.h>
#define T1_POCKET_UI
#define CTL_TYPE_NODE_DISCOVER_REQ 0x80
#define CTL_TYPE_NODE_DISCOVER_RESP 0x90
#define ADV_TYPE_REPEATER 2
#define PUSH_CODE_CONTROL_DATA 0x8e
mesh::Packet::Packet() : header(0), payload_len(0), path_len(0) {}
uint32_t nowMs = 0;
uint32_t millis() { return nowMs; }
// The display sleep window uses the FreeRTOS monotonic timeout API.
uint64_t elapsedMs = 0;
struct TimeOut_t { uint64_t at; };
constexpr int pdFALSE = 0;
void vTaskSetTimeOutState(TimeOut_t* time) { time->at = elapsedMs * 1024 / 1000; }
int xTaskCheckForTimeOut(TimeOut_t* time, uint32_t* remaining) {
  return elapsedMs * 1024 / 1000 - time->at >= *remaining;
}
struct UITask;
struct FakeSerial {
  bool connected = false;
  unsigned frames = 0;
  bool isConnected() { return connected; }
  void writeFrame(const uint8_t*, int) { ++frames; }
} serialPort;
struct FakeRadio {
  float getLastSNR() { return 0; }
  int getLastRSSI() { return -90; }
} fakeRadio;
#include <helpers/ui/UIScreen.h>
#include <T1CompanionClock.h>

uint32_t testTicks = 0;
T1CompanionClock rtc_clock;
struct { bool usb=true; bool isExternalPowered() { return usb; } } board;
struct BaseChatMesh {
  void logTx(mesh::Packet*, int) {}
  void logTxFail(mesh::Packet*, int) {}
};
struct MyMesh : BaseChatMesh {
  struct Queue {
    mesh::Packet* pkt=nullptr;
    int getOutboundTotal() { return pkt ? 1 : 0; }
    mesh::Packet* getOutboundByIdx(int i) { assert(i==0); return pkt; }
    mesh::Packet* removeOutboundByIdx(int i) { auto p=getOutboundByIdx(i);pkt=nullptr;return p; }
  } queue;
  Queue* _mgr=&queue;
  void releasePacket(mesh::Packet* p) { assert(p==&request); }
  void cancelPocketRequest(const mesh::Packet*,const uint8_t[4]);
  void logTx(mesh::Packet*,int);
  void logTxFail(mesh::Packet*,int);
  UITask* _listener = nullptr;
  FakeSerial* _serial = &serialPort;
  FakeRadio* _radio = &fakeRadio;
  uint8_t out_frame[256];
  unsigned requests = 0, allocations = 0, tag = 0;
  bool queueFull = false;
  struct { uint8_t pub_key[PUB_KEY_SIZE] = {}; } self_id;
  struct Contact { char name[32] = "Hill"; } known;
  Contact* lookupContactByPubKey(const uint8_t* key, int len) {
    assert(len == PUB_KEY_SIZE);
    return key[31] == 2 ? &known : nullptr;
  }
  MyMesh* getRNG() { return this; }
  void random(uint8_t* out, size_t len) { memset(out, ++tag, len); }
  mesh::Packet request;
  mesh::Packet* createControlData(const uint8_t* data, size_t len) {
    ++allocations;
    assert(len == 6 && data[0] == 0x80 && data[1] == 4);
    if (queueFull) return nullptr;
    request.header=PAYLOAD_TYPE_CONTROL<<PH_TYPE_SHIFT;
    request.payload_len=len;
    memcpy(request.payload, data, len);
    return &request;
  }
  void sendZeroHop(mesh::Packet* p) { assert(p == &request && !queue.pkt); queue.pkt=p; ++requests; }
  void onControlDataRecv(mesh::Packet*);
  void enterCLIRescue() { assert(false); }
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
  UIScreen *home=nullptr, *curr=nullptr, *splash=nullptr, *msg_preview=nullptr;
  DisplayDriver* _display=nullptr;
  SensorManager* _sensors=nullptr;
  NodePrefs* _node_prefs=nullptr;
  unsigned long _auto_off=0, _next_refresh=0, ui_started_at=0, _alert_expiry=0;
  char _alert[32]={};
  void begin(DisplayDriver*, SensorManager*, NodePrefs*);
  void loop();
  void setCurrScreen(UIScreen*);
  char checkDisplayOn(char);
  char handleLongPress(char);
  void userLedHandler() {}

  void onControlDataRecv(const mesh::Packet*);
  void onPocketTxDone(const mesh::Packet*,bool);
  bool gps=false, pressed=false, shutdownRequested=false;
  std::string alert;
  bool muted=true;
  unsigned beeperToggles=0;
  bool isBuzzerQuiet() { return muted; }
  void toggleBuzzer() { muted=!muted; ++beeperToggles; }
  bool getGPSState() { return gps; }
  unsigned millivolts=3810;
  unsigned batteryReads=0;
  unsigned getBattMilliVolts() { ++batteryReads; return millivolts; }
  int getMsgCount() { return 2; }
  void showAlert(const char* msg, int) { alert=msg; }
  void toggleGPS() { gps=!gps; }
  bool isButtonPressed() { return pressed; }
  void shutdown() { shutdownRequested=true; }
};
bool gpsFix = false;
int32_t fixLat = 48117300, fixLon = 11516667;
uint32_t fixAge = 0;
bool t1LastGpsPosition(int32_t& lat, int32_t& lon, uint32_t& age) {
  lat=fixLat; lon=fixLon; age=fixAge;
  return gpsFix;
}
#include <T1HomeScreen.h>
#include <T1PocketButtons.h>
T1PocketButtons pocketButtons;
#define AUTO_OFF_MILLIS T1_SCREEN_TIMEOUT_MS
enum { PIN_BUTTON1=1, PIN_BUTTON2=2, LOW=0, INPUT=0 };
int downPin=0;
int digitalRead(int pin) { return pin==downPin ? LOW : 1; }
void pinMode(int,int) {}
struct SplashScreen : UIScreen {
  explicit SplashScreen(UITask*) {}
  int render(DisplayDriver&) override { assert(false); return 0; }
};
struct MsgPreviewScreen : SplashScreen {
  MsgPreviewScreen(UITask* t,mesh::RTCClock*) : SplashScreen(t) {}
};
#include "t1-test-callbacks.h"

ColorVal UIColor::window_bkg=0, UIColor::popup_bkg=0, UIColor::popup_txt=1, UIColor::title_bkg=0x4227, UIColor::title_txt=0xbcf0,
         UIColor::primary_txt=0xded8, UIColor::secondary_txt=0x9cf1,
         UIColor::warning_txt=0xe58d;

struct Screen : public DisplayDriver {
  int x=0, y=0, size=1;
  ColorVal color=0;
  std::string largeText;
  std::string zone, voltage;
  std::string hint, detail, latitude, longitude, fixTime;
  std::vector<std::string> rows;
  int dots=0, bars=0;
  bool plug=false;
  Screen() : DisplayDriver(128,64) {}
  bool on=false;
  bool isOn() override {return on;}
  void turnOn() override {on=true;}
  void turnOff() override {on=false;}
  void clear() override {}
  void startFrame(ColorVal) override {}
  void setTextSize(int s) override {size=s;}
  void setColor(ColorVal c) override {color=c;}
  void setCursor(int a,int b) override {x=int(1.25*a);y=int(1.25*b);}
  void print(const char* s) override {
    assert(x>=0 && x+int(strlen(s))*6*size<=160 && y+8*size<=80);
    if (y>=20 && y<=50) rows.emplace_back(s);
    if (y==67) hint=s;
    if (y==48) detail=s;
    if (size==2 && y==18) latitude=s;
    if (size==2 && y==36) longitude=s;
    if (size==1 && y==52) fixTime=s;
    if (size == 2) largeText=s;
    if (size==1 && x==68 && y==30) zone=s;
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
// Exercise startup, the real button decoder, the idle transition and wake dispatch.
static void testWake() {
  UITask task;
  Screen panel;
  NodePrefs prefs;
  SensorManager sensors;
  auto step = [&](uint32_t ms) { elapsedMs+=ms; nowMs=uint32_t(elapsedMs); task.loop(); };
  auto tap = [&](int pin) { downPin=pin; step(1); step(26); downPin=0; step(1); step(26); };
  task.begin(&panel,&sensors,&prefs);
  step(1);
  assert(panel.on==T1Build::screenOnAtBoot);
  if (!panel.on) tap(PIN_BUTTON1);
  assert(panel.on && panel.hint=="Hold to send advert");
  tap(PIN_BUTTON1);tap(PIN_BUTTON1);
  assert(panel.largeText=="GPS off");
  step(T1_SCREEN_TIMEOUT_MS);assert(!panel.on);
  const auto readsAtSleep = task.batteryReads;
  step(20);
  assert(task.batteryReads == readsAtSleep);
  tap(PIN_BUTTON2);
  if (T1Build::resumeSeconds) {
    assert(panel.on && panel.largeText=="GPS off" && !task.gps);
  } else {
    assert(panel.on && panel.hint=="Hold to send advert");
    tap(PIN_BUTTON1);tap(PIN_BUTTON1);
  }
  step(T1_SCREEN_TIMEOUT_MS);assert(!panel.on);
  step(T1Build::resumeSeconds * 1000UL);tap(PIN_BUTTON1);
  assert(panel.on && panel.hint=="Hold to send advert" && !task.gps);
  tap(PIN_BUTTON2);assert(panel.hint=="Tap to browse");
  step(T1_SCREEN_TIMEOUT_MS);assert(!panel.on);
  bluetooth_interface.connected=true;step(1);assert(!panel.on);
  tap(PIN_BUTTON1);assert(panel.hint=="Hold to send advert");
  bluetooth_interface.connected=false;
  delete static_cast<HomeScreen*>(task.home);
  delete static_cast<SplashScreen*>(task.splash);
  delete static_cast<MsgPreviewScreen*>(task.msg_preview);
}
int main(){
  UITask task;
  NodePrefs prefs;
  Screen display;
  HomeScreen home(&task,&rtc_clock,nullptr,&prefs);
  the_mesh._listener = &task;
  task.home = &home;
  puts("PAGE sync");home.render(display);
  assert(display.largeText.empty() && display.zone.empty());
  assert(display.plug && display.bars==0 && display.voltage=="3.81 V");
  home.handleInput(KEY_ENTER);
  assert(!the_mesh.sent && !task.alert.empty());
  for (const uint32_t utc : {1795503540u, 1795503600u, 1782919800u, 1795478340u}) {
    rtc_clock.setCurrentTime(utc);
    const time_t shifted = int64_t(utc) + T1Build::utcOffsetMinutes * 60;
    char expected[6];
    strftime(expected, sizeof(expected), "%H:%M", gmtime(&shifted));
    display.zone.clear();
    puts("PAGE zone-check");home.render(display);
    assert(display.largeText==expected && display.zone==T1Build::clockLabel);
    assert(rtc_clock.getCurrentTime()==utc);
  }
  rtc_clock.setCurrentTime(1795470300);
  board.usb=false;
  for (const auto& test : {std::make_pair(3300u,0), {3450u,1}, {3650u,2}, {3800u,3}, {4000u,4}, {0u,0}, {5000u,0}}) {
    display.bars=0; display.plug=false; task.millivolts=test.first;
    nowMs += 3000; // Each standalone band check starts with a fresh sample.
    puts("PAGE battery-check");home.render(display);
    assert(display.bars==test.second && !display.plug);
    if (test.first==0 || test.first==5000) assert(display.voltage=="-- V");
  }
  auto voltageAfter = [&](unsigned ms, unsigned mv) {
    nowMs += ms; task.millivolts = mv;
    home.render(display);
    return display.voltage;
  };
  nowMs=0xfffff000; // Sampling crosses the 32-bit timer rollover.
  for (unsigned i=0; i<T1Build::batterySamples; ++i) voltageAfter(1000, 3800);
  auto before = task.batteryReads;
  assert(voltageAfter(500,4200)=="3.80 V" && task.batteryReads==before);
  auto spike=voltageAfter(500,4200);
  if (T1Build::batterySamples>=5) assert(spike=="3.80 V");
  if (T1Build::batterySamples==1) assert(spike=="4.20 V");
  for (unsigned i=0; i<T1Build::batterySamples; ++i) voltageAfter(1000,4200);
  assert(display.voltage=="4.20 V");
  assert(voltageAfter(1000,0)=="4.20 V");
  assert(voltageAfter(2000,0)=="-- V");
  assert(voltageAfter(1000,3810)=="3.81 V");
  board.usb=true;
  assert(voltageAfter(0,4180)=="4.18 V");
  board.usb=false;
  assert(voltageAfter(0,3810)=="3.81 V");
  puts("PAGE home");home.render(display);
  home.handleInput(KEY_ENTER);
  assert(the_mesh.sent==1);
  assert(the_mesh.flooded==T1Build::floodAdvert);
  board.usb=true;
  home.handleInput(KEY_NEXT);
  auto show = [&]() { display.rows.clear(); home.render(display); return display.hint; };
  puts("PAGE discovery-ready");
  assert(show()=="Hold to discover repeaters" && the_mesh.requests==0);
  nowMs=0xffffff00u;  // Reply window and cooldown both cross millis rollover.
  the_mesh.queueFull=true;
  home.handleInput(KEY_ENTER);
  assert(show()=="Retry shortly" && display.rows[0]=="Radio busy" && the_mesh.requests==0);
  home.handleInput(KEY_ENTER);assert(the_mesh.allocations==1);
  nowMs+=2000;
  the_mesh.queueFull=false;
  home.handleInput(KEY_ENTER);
  puts("PAGE discovery-queued");
  assert(show()=="Request queued" && the_mesh.requests==1);
  home.handleInput(KEY_ENTER);assert(the_mesh.requests==1);
  unsigned long deadline=nowMs;  // Even a deadline already due cannot hide the queued request.
  assert(!home.displayIdleExpired(nowMs,deadline));
  auto reply = [&](uint8_t k, int8_t snr) {
    mesh::Packet p;
    p.header=(PAYLOAD_TYPE_CONTROL << PH_TYPE_SHIFT) | ROUTE_TYPE_DIRECT;
    p.payload_len=38;
    p.payload[0]=0x92; p.payload[1]=static_cast<uint8_t>(-snr);
    memcpy(p.payload+2,the_mesh.request.payload+2,4);
    memset(p.payload+6,0,PUB_KEY_SIZE);
    p.payload[6]=0xab; p.payload[7]= k == 2 ? 1 : k; p.payload[37]=k;
    p._snr=snr;
    return p;
  };
  auto incoming = [&](mesh::Packet p) { the_mesh.onControlDataRecv(&p); };
  nowMs+=14000;
  assert(!home.displayIdleExpired(nowMs,deadline));  // Queuing time does not use any of the 15-second receive window.
  incoming(reply(1,70));assert(show()=="Request queued" && display.rows.empty());
  auto other=reply(1,70);the_mesh.logTx(&other,40);assert(show()=="Request queued");
  the_mesh.logTx(the_mesh.queue.removeOutboundByIdx(0),8);
  puts("PAGE discovery-listening");
  assert(show()=="Listening...");
  auto bad=reply(1,70);bad.payload[0]=0x93;incoming(bad);
  bad=reply(1,70);bad.payload_len=14;incoming(bad);
  bad=reply(1,70);bad.payload[2]^=1;incoming(bad);
  bad=reply(1,70);bad.path_len=1;incoming(bad);
  bad=reply(1,70);bad.header=(PAYLOAD_TYPE_CONTROL<<PH_TYPE_SHIFT)|ROUTE_TYPE_FLOOD;incoming(bad);
  bad=reply(1,70);memset(bad.payload+6,0,PUB_KEY_SIZE);incoming(bad);
  show();assert(display.rows==std::vector<std::string>{"Waiting for replies"});
  for (int k=1;k<=5;++k) incoming(reply(k,-k));
  serialPort.connected=true;
  nowMs+=14999;
  assert(!home.displayIdleExpired(nowMs,deadline));
  incoming(reply(2,19));incoming(reply(2,-20));incoming(reply(5,2));
  puts("PAGE discovery-results");
  assert(serialPort.frames==3);  // Each raw control still reaches the phone.
  show();
  assert((display.rows==std::vector<std::string>{"Hill","4.75 dB","AB050000","0.50 dB","AB010000","-0.25 dB","AB030000","-0.75 dB"}));
  nowMs+=1;
  incoming(reply(1,100));home.handleInput(KEY_ENTER);
  assert(!home.displayIdleExpired(nowMs,deadline));
  assert(!home.displayIdleExpired(nowMs + T1_SCREEN_TIMEOUT_MS - 1,deadline));
  assert(home.displayIdleExpired(nowMs + T1_SCREEN_TIMEOUT_MS,deadline));
  puts("PAGE discovery-cooldown");
  assert(show()=="Scan again in 15s" && display.rows[1]=="4.75 dB" && the_mesh.requests==1);
  nowMs+=15000;
  home.handleInput(KEY_ENTER);
  assert(show()=="Request queued" && the_mesh.requests==2);
  the_mesh.logTx(the_mesh.queue.removeOutboundByIdx(0),8);
  assert(show()=="Listening...");
  bad=reply(1,90);bad.payload[2]--;incoming(bad);
  nowMs+=15000;home.poll();show();
  assert(display.rows==std::vector<std::string>{"No repeaters heard"});
  nowMs+=15000;
  home.handleInput(KEY_ENTER);
  assert(!home.displayIdleExpired(nowMs,deadline));
  nowMs+=30000;home.poll();
  assert(!home.displayIdleExpired(nowMs,deadline) && uint32_t(deadline-nowMs)==T1_SCREEN_TIMEOUT_MS);
  assert(!the_mesh.queue.pkt && show()=="Retry shortly");
  assert(display.rows[0]=="TX unconfirmed" && the_mesh.requests==3);
  nowMs+=2000;home.handleInput(KEY_ENTER);
  the_mesh.logTxFail(the_mesh.queue.removeOutboundByIdx(0),8);
  assert(show()=="Retry shortly" && display.rows[0]=="Transmit failed" && the_mesh.requests==4);
  nowMs+=2000;
  // Visit from both directions and return to Home without transmitting.
  home.handleInput(KEY_PREV);home.handleInput(KEY_NEXT);
  assert(show()=="Hold to discover repeaters");
  home.handleInput(KEY_NEXT);home.handleInput(KEY_PREV);
  assert(show()=="Hold to discover repeaters" && the_mesh.sent==1 && the_mesh.requests==4);
  serialPort.connected=false;
  home.handleInput(KEY_NEXT);
  display.detail.clear();
  puts("PAGE gps-off");home.render(display);
  assert(display.detail.empty() && display.hint=="Hold to turn GPS on");
  home.handleInput(KEY_ENTER);assert(task.gps);
  puts("PAGE gps-on");home.render(display);
  assert(display.detail=="Searching for fix" && display.hint=="Hold to turn GPS off");
  gpsFix=true;
  puts("PAGE gps-fix");home.render(display);
  assert(display.latitude=="48.11730 N" && display.longitude=="11.51667 E");
  assert(display.fixTime=="Last fix just now" && display.hint=="Hold to turn GPS off");
  fixAge=180;
  puts("PAGE gps-lost");home.render(display);
  assert(display.fixTime=="Last fix 3m ago" && display.latitude=="48.11730 N" && display.longitude=="11.51667 E");
  fixLat=-89999999;fixLon=-179999999;
  puts("PAGE gps-bounds");home.render(display);
  assert(display.latitude=="90.00000 S" && display.longitude=="180.00000 W");
  for (auto test : {std::make_pair(59u,"Last fix 59s ago"),{60u,"Last fix 1m ago"},
                    {3600u,"Last fix 1h ago"},{86400u,"Last fix 1d ago"}}) {
    fixAge=test.first;home.render(display);assert(display.fixTime==test.second);
  }
  gpsFix=false;
  home.handleInput(KEY_NEXT);
  puts("PAGE beeper-off");home.render(display);
  assert(display.largeText=="Beeper off" && display.hint=="Hold to unmute" && task.beeperToggles==0);
  home.handleInput(KEY_ENTER);
  puts("PAGE beeper-on");home.render(display);
  assert(display.largeText=="Beeper on" && display.hint=="Hold to mute" && task.beeperToggles==1);
  home.handleInput(KEY_PREV);
  home.handleInput(KEY_NEXT);
  assert(!task.muted && task.beeperToggles==1);
  home.handleInput(KEY_ENTER);
  assert(task.muted && task.beeperToggles==2);
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
  assert(draw()=="000042" && display.hint=="Tap to browse" && display.dots==6);
  bluetooth_interface.connected=true;
  puts("PAGE bluetooth-connected");
  // Catch a connection arriving between the input poll and the next paint.
  assert(draw().find(':')!=std::string::npos && display.dots==5);
  home.handleInput(KEY_ENTER);
  assert(the_mesh.sent==1);
  assert(home.updatePairingState());
  assert(!home.updatePairingState());
  assert(draw().find(':')!=std::string::npos);
  home.handleInput(KEY_PREV);assert(draw()=="Hibernate");
  home.handleInput(KEY_NEXT);assert(draw().find(':')!=std::string::npos);
  home.handleInput(KEY_NEXT);assert(show()=="Hold to discover repeaters");
  home.handleInput(KEY_NEXT);assert(draw()=="GPS on");
  home.handleInput(KEY_NEXT);assert(draw()=="Beeper off");
  home.handleInput(KEY_NEXT);assert(draw()=="Hibernate");
  home.handleInput(KEY_NEXT);assert(draw().find(':')!=std::string::npos);

  // Disconnection leaves GPS visible. Lower from Home opens Pairing when available.
  home.handleInput(KEY_NEXT);home.handleInput(KEY_NEXT);
  bluetooth_interface.connected=false;
  assert(!home.updatePairingState() && draw()=="GPS on");
  home.resetPage();
  home.handleInput(KEY_PREV);assert(draw()=="000042");
  bluetooth_interface.enabled=false;
  assert(home.updatePairingState());assert(draw().find(':')!=std::string::npos && display.dots==5);
  bluetooth_interface.enabled=true;the_mesh.pin=0;
  assert(!home.updatePairingState());
  home.handleInput(KEY_PREV);assert(draw()=="Hibernate");
  assert(the_mesh.sent==1 && task.gps && !task.shutdownRequested);
  task.pressed=true;home.handleInput(KEY_ENTER);home.poll();assert(!task.shutdownRequested);
  task.pressed=false;home.poll();assert(task.shutdownRequested);
  char cleaned[33];
  const char rawName[] = "  Café 👩🏽‍🔧  🇨🇦 Ridge 1️⃣ ";
  PocketName::format(cleaned, rawName, sizeof(rawName));
  assert(strcmp(cleaned, "Caf\x82 Ridge") == 0);
  PocketName::format(cleaned, "🦅", 4); assert(!cleaned[0]);
  home.resetPage();board.usb=false;
  strcpy(prefs.node_name,"A very long node name to clip");
  puts("PAGE long-name");home.render(display);
  the_mesh.pin=42;
  puts("PAGE boot-wake-check");testWake();
}
