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
  UIScreen* home = nullptr;
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
  unsigned getBattMilliVolts() { return millivolts; }
  int getMsgCount() { return 2; }
  void showAlert(const char* msg, int) { alert=msg; }
  void toggleGPS() { gps=!gps; }
  bool isButtonPressed() { return pressed; }
  void shutdown() { shutdownRequested=true; }
};
bool gpsFix = false;
bool t1HasGpsFix() { return gpsFix; }
#include <T1HomeScreen.h>
#include "t1-test-callbacks.h"

ColorVal UIColor::title_bkg=0x4227, UIColor::title_txt=0xbcf0,
         UIColor::primary_txt=0xded8, UIColor::secondary_txt=0x9cf1,
         UIColor::warning_txt=0xe58d;

struct Screen : public DisplayDriver {
  int x=0, y=0, size=1;
  ColorVal color=0;
  std::string largeText;
  std::string zone, voltage;
  std::string hint, detail;
  std::vector<std::string> rows;
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
    if (y>=20 && y<=50) rows.emplace_back(s);
    if (y==67) hint=s;
    if (y==48) detail=s;
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
    puts("PAGE battery-check");home.render(display);
    assert(display.bars==test.second && !display.plug);
    if (test.first==0 || test.first==5000) assert(display.voltage=="-- V");
  }
  task.millivolts=3810;
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
  assert(display.detail=="Position fix");
  gpsFix=false;
  puts("PAGE gps-lost");home.render(display);
  assert(display.detail=="Searching for fix");
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
  home.wakeForButton();
  assert(draw()=="000042" && display.hint=="Tap to dismiss" && display.dots==6);
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
  assert(draw().find(':')!=std::string::npos && display.dots==5);
  home.handleInput(KEY_ENTER);
  assert(the_mesh.sent==1);
  assert(home.updatePairingState());
  assert(!home.updatePairingState());
  home.wakeForButton();
  assert(draw().find(':')!=std::string::npos);
  home.handleInput(KEY_PREV);assert(draw()=="Hibernate");
  home.handleInput(KEY_NEXT);assert(draw().find(':')!=std::string::npos);
  home.handleInput(KEY_NEXT);assert(show()=="Hold to discover repeaters");
  home.handleInput(KEY_NEXT);assert(draw()=="GPS on");
  home.handleInput(KEY_NEXT);assert(draw()=="Beeper off");
  home.handleInput(KEY_NEXT);assert(draw()=="Hibernate");
  home.handleInput(KEY_NEXT);assert(draw().find(':')!=std::string::npos);

  // Disconnecting while awake leaves the current page alone, then promotes
  // pairing once on the next physical wake. Either tap can dismiss the prompt.
  home.handleInput(KEY_NEXT);
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
  home.wakeForButton();assert(draw().find(':')!=std::string::npos && display.dots==5);
  bluetooth_interface.enabled=true;
  the_mesh.pin=0;
  assert(!home.updatePairingState());
  home.wakeForButton();assert(draw().find(':')!=std::string::npos && display.dots==5);
  home.handleInput(KEY_PREV);assert(draw()=="Hibernate");
  assert(the_mesh.sent==1 && task.gps && !task.shutdownRequested);
  task.pressed=true;home.handleInput(KEY_ENTER);home.poll();assert(!task.shutdownRequested);
  task.pressed=false;home.poll();assert(task.shutdownRequested);
  home.resetPage();board.usb=false;
  strcpy(prefs.node_name,"A very long node name to clip");
  puts("PAGE long-name");home.render(display);
}
