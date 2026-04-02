// Full_Particle_Bridge_Compliant.ino
// HomeSpan 2.1.5 — Full Lights.html bridge (compliance fixes)
// Wijzigingen:
//  - Vervangen van defecte Zitplaats controller (New ID: 200033000547373336323230)
//  - homeSpan.setLogLevel(1)
//  - homeSpan.begin(Category::Bridges, ...)
//  - voor elk button maken we een *nieuw* SpanAccessory() + AccessoryInformation()
//  - ParticleControl gebruikt ongewijzigd (LightBulb subclass)

#include "HomeSpan.h"
#include <WiFiClientSecure.h>
#include <Preferences.h>

Preferences prefs;

// ===== CONFIG =====
const char* PARTICLE_HOST   = "api.particle.io";
const int   PARTICLE_PORT   = 443;
const char* PARTICLE_TOKEN  = "ba9d9e1ed9f70cc5db24de2db21764a3a3afe28b"; // from your HTML

struct ControllerDef { const char* name; const char* deviceId; const char* command; const char* buttons[8]; };
ControllerDef controllers[] = {
  { "Zitplaats", "200033000547373336323230", "manual", {"muurspots","tvback","clocktime","clockbrightness", nullptr} },
  { "BandB",     "30002c000547343233323032", "manual", {"bandb","buitentrap", nullptr} },
  { "Badkamer",  "5600420005504b464d323520", "manual", {"badkamer","leeslampjes", nullptr} },
  { "Inkom",     "420035000e47343432313031", "manual", {"inkom","dressing","trapcolor", nullptr} },
  { "Keuken",    "310017001647373335333438", "manual", {"spots", nullptr} },
  { "Wasplaats", "33004f000e504b464d323520", "manual", {"wasplaats","toilet", nullptr} },
  { "Eetplaats", "210042000b47343432313031", "manual", {"ramenintensity","tafel","bib", nullptr} },
  { "Slaapkamer","210042000b47343432313031", "manual", {"buro","bed", nullptr} },
  { "Buiten",    "2d0032001247333438353733", "manual", {"S","W2","W1", nullptr} },
  { "Outside",   "390028001147343339383037", "manual", {"sunlightmode", nullptr} },
  { "HVAC",      "3e003f001447343338333633", "manual", {"hvacmode","sch","won", nullptr} }
};
const int NUM_CONTROLLERS = sizeof(controllers)/sizeof(controllers[0]);

struct MultiStateDef { const char* buttonId; const char* states[8]; };
MultiStateDef multiDefs[] = {
  { "clocktime",     {"summer","winter", nullptr} },
  { "clockbrightness", {"h","m","l", nullptr} },
  { "trapcolor",     {"trapoff","trapon","traproodon","trapgroenon","trapblauwon", nullptr} },
  { "ramenintensity", {"ramenlowoff","ramenhion","ramenmedon","ramenlowon", nullptr} },
  { "sunlightmode",  {"auto","day","night", nullptr} },
  { "hvacmode",      {"home","out", nullptr} }
};
const int NUM_MULTI = sizeof(multiDefs)/sizeof(multiDefs[0]);

const char** getStatesFor(const char* id) {
  for (int i=0;i<NUM_MULTI;i++) {
    if (strcmp(multiDefs[i].buttonId, id)==0) {
      return (const char**)multiDefs[i].states;
    }
  }
  return nullptr;
}

String displayNameFor(const char* tag) {
  if (strcmp(tag,"badkamer") == 0) return "Badkamerlicht";
  if (strcmp(tag,"leeslampjes") == 0) return "Leeslampjes";
  if (strcmp(tag,"muurspots") == 0) return "Muurspots";
  if (strcmp(tag,"tvback") == 0) return "TV Backlight";
  if (strcmp(tag,"clocktime") == 0) return "Zomer/Winter";
  if (strcmp(tag,"clockbrightness") == 0) return "Klok Helderheid";
  if (strcmp(tag,"bandb") == 0) return "BandB licht";
  if (strcmp(tag,"buitentrap") == 0) return "Buitentrap";
  if (strcmp(tag,"inkom") == 0) return "Inkom";
  if (strcmp(tag,"dressing") == 0) return "Dressing";
  if (strcmp(tag,"trapcolor") == 0) return "Trap kleur";
  if (strcmp(tag,"spots") == 0) return "Keuken Spots";
  if (strcmp(tag,"wasplaats") == 0) return "Wasplaats";
  if (strcmp(tag,"toilet") == 0) return "Toilet/Vestiaire";
  if (strcmp(tag,"ramenintensity") == 0) return "Ramen Intensiteit";
  if (strcmp(tag,"tafel") == 0) return "Tafel";
  if (strcmp(tag,"bib") == 0) return "Bib";
  if (strcmp(tag,"buro") == 0) return "Buro";
  if (strcmp(tag,"bed") == 0) return "Bed";
  if (strcmp(tag,"S") == 0) return "Terras";
  if (strcmp(tag,"W2") == 0) return "Veldkant";
  if (strcmp(tag,"W1") == 0) return "Voordeur";
  if (strcmp(tag,"sunlightmode") == 0) return "Sunlight Mode";
  if (strcmp(tag,"hvacmode") == 0) return "HVAC Mode";
  if (strcmp(tag,"sch") == 0) return "ECO => SCHUUR";
  if (strcmp(tag,"won") == 0) return "ECO => WONING";
  return String(tag);
}

// ===== Particle POST helper (on original code) =====
void sendParticleCommand(const char* deviceId, const char* command, const char* arg) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi not connected — cannot contact Particle");
    return;
  }
  WiFiClientSecure client;
  client.setInsecure();

  String path = String("/v1/devices/") + deviceId + "/" + command;
  String body = String("access_token=") + PARTICLE_TOKEN + "&args=" + arg;

  Serial.print("➡️ POST "); Serial.print(path); Serial.print("  body="); Serial.println(body);

  if (!client.connect(PARTICLE_HOST, PARTICLE_PORT)) {
    Serial.println("❌ Connection to Particle failed");
    return;
  }

  client.print(String("POST ") + path + " HTTP/1.1\r\n" +
               "Host: " + PARTICLE_HOST + "\r\n" +
               "User-Agent: FilipsESP32/1.0\r\n" +
               "Content-Type: application/x-www-form-urlencoded\r\n" +
               "Content-Length: " + body.length() + "\r\n" +
               "Connection: close\r\n\r\n" +
               body);

  String statusLine = client.readStringUntil('\n'); statusLine.trim();
  Serial.print("HTTP status: "); Serial.println(statusLine);

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) break;
  }

  String resp = client.readString();
  resp.trim();
  if (resp.length()) {
    Serial.print("Response: "); Serial.println(resp);
  } else Serial.println("No response body.");
  client.stop();
}

// ===== Service implementation (unchanged) =====
struct ParticleControl : Service::LightBulb {
  Characteristic::On *power;
  const char* deviceId;
  const char* command;
  String baseTag;
  const char** states; // null => binary toggle
  String display;
  String msKey; // prefs key for multi-state index

  ParticleControl(const char* dev, const char* cmd, const char* tag, const char* displayName) : Service::LightBulb() {
    deviceId = dev; command = cmd; baseTag = String(tag); states = nullptr; display = String(displayName);
    power = new Characteristic::On();
    new Characteristic::Name(displayName);
  }

  ParticleControl(const char* dev, const char* cmd, const char* tag, const char* displayName, const char** stateArr) : Service::LightBulb() {
    deviceId = dev; command = cmd; baseTag = String(tag); states = stateArr; display = String(displayName);
    power = new Characteristic::On();
    new Characteristic::Name(displayName);
    msKey = String("ms_") + tag;
    if (!prefs.isKey(msKey.c_str())) prefs.putUInt(msKey.c_str(), 0);
  }

  boolean update() override {
    bool newVal = power->getNewVal();

    if (states == nullptr) {
      String arg = baseTag + (newVal ? "on" : "off");
      Serial.print("🔁 "); Serial.print(display); Serial.print(" -> "); Serial.println(newVal ? "ON":"OFF");
      sendParticleCommand(deviceId, command, arg.c_str());
    } else {
      if (newVal) {
        unsigned int idx = prefs.getUInt(msKey.c_str(), 0);
        int count = 0; while (states[count]) count++;
        const char* argState = states[idx];
        Serial.print("🔁 Multi "); Serial.print(display); Serial.print(" -> idx="); Serial.print(idx); Serial.print(" state="); Serial.println(argState);
        sendParticleCommand(deviceId, command, argState);

        idx = (idx + 1) % count;
        prefs.putUInt(msKey.c_str(), idx);

        power->setVal(false);
      }
    }
    return true;
  }
};

// ===== Force-new-ID helpers (unchanged) =====
String makeRandomSerial() {
  uint32_t a = (uint32_t)esp_random();
  uint32_t b = (uint32_t)millis();
  char buf[32];
  snprintf(buf, sizeof(buf), "BR-%08X%08X", a, b);
  return String(buf);
}

void forceNewAccessoryRegistrationIfRequested() {
  if (!prefs.isKey("request_new_id")) return;
  bool req = prefs.getBool("request_new_id", false);
  if (!req) return;

  Serial.println("⚠️  Force-new-ID requested: clearing HomeSpan NVS and generating new serial...");

  Preferences hs;
  if (hs.begin("HomeSpan", false)) {
    hs.clear();
    hs.end();
    Serial.println("✅ HomeSpan NVS cleared.");
  } else {
    Serial.println("⚠️ Could not open HomeSpan prefs to clear (continuing).");
  }

  String s = makeRandomSerial();
  prefs.putString("bridge_serial", s.c_str());
  prefs.putBool("request_new_id", false);
  Serial.print("🔐 New bridge serial: "); Serial.println(s);
  Serial.println("Restarting shortly to apply new registration...");
  delay(500);
  ESP.restart();
}

// ===== Setup and main (modified accessory creation) =====
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("--- Filips Final Particle Bridge (Compliant) ---");

  prefs.begin("delbridge", false);

#if FORCE_NEW_ON_FIRST_BOOT
  if (!prefs.getBool("seen_boot_once", false)) {
    prefs.putBool("request_new_id", true);
  }
#endif

  forceNewAccessoryRegistrationIfRequested();

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  setCpuFrequencyMhz(240);
  Serial.println("⚡ Sleep disabled, CPU at 240MHz");

  // show useful logs but not insane debug
  homeSpan.setLogLevel(1);

  // Start as Bridge category
  homeSpan.begin(Category::Bridges, "Zarlardinge Bridge");

  // Bridge serial number
  String bridgeSerial;
  if (prefs.isKey("bridge_serial")) {
    bridgeSerial = prefs.getString("bridge_serial");
  } else {
    bridgeSerial = makeRandomSerial();
    prefs.putString("bridge_serial", bridgeSerial.c_str());
  }

  Serial.print("Bridge serial (used in AccessoryInformation): ");
  Serial.println(bridgeSerial);

  // ===== Create Bridge Accessory (AID=1) =====
  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Name("Zarlardinge Bridge");
      new Characteristic::Manufacturer("Delannoy");
      new Characteristic::SerialNumber(bridgeSerial.c_str());
      new Characteristic::Model("ParticleBridge-Full");
      new Characteristic::FirmwareRevision("1.0");
      new Characteristic::Identify();

  // ===== For each controller/button create separate accessory =====
  for (int i=0;i<NUM_CONTROLLERS;i++) {
    ControllerDef &c = controllers[i];
    const char** btns = c.buttons;
    for (int b=0; btns[b]!=nullptr; b++) {
      const char* tag = btns[b];
      const char* devId = c.deviceId;
      const char* cmd = c.command;
      String dName = displayNameFor(tag);          // ensure stable String lifetime
      const char* displayName = dName.c_str();

      // build a serial for the child accessory: bridgeSerial + tag
      char childSerialBuf[48];
      snprintf(childSerialBuf, sizeof(childSerialBuf), "%s_%s", bridgeSerial.c_str(), tag);

      // start new accessory (child)
      new SpanAccessory();
        new Service::AccessoryInformation();
          new Characteristic::Name(displayName);
          new Characteristic::Manufacturer("Delannoy");
          new Characteristic::Model("ParticleChild");
          new Characteristic::SerialNumber(childSerialBuf);
          new Characteristic::FirmwareRevision("1.0");
          new Characteristic::Identify();

        // find multi-state
        const char** states = getStatesFor(tag);
        if (states) {
          new ParticleControl(devId, cmd, tag, displayName, states);
        } else {
          new ParticleControl(devId, cmd, tag, displayName);
        }
    }
  }

  prefs.putBool("seen_boot_once", true);

  Serial.println("Ready. Pair via Home.app if needed.");
  Serial.println("Serial commands: 'N' + Enter -> force new registration (clear pairing & create new serial).");
  Serial.println("Tip: keep Erase Flash = OFF to preserve pairing between uploads.");
}

// Serial CLI replacement: een CLI die werkt op HomeSpan 2.1.5
void printCLIHelp() {
  Serial.println(F("\n*** HomeSpan Custom CLI ***"));
  Serial.println(F(" ?: print this help"));
  Serial.println(F(" N : force new accessory registration (existing behavior)"));
  Serial.println(F(" E : Erase HomeSpan pairing data (HomeSpan NVS) and restart"));
  Serial.println(F(" F : Factory reset (erase HomeSpan + app prefs) and restart"));
  Serial.println(F(" L <0|1|2> : set HomeSpan log level (0..3)"));
  Serial.println(F(" i : print lightweight info (IP, bridge serial, #accessories)"));
  Serial.println(F(" R : restart device"));
  Serial.println(F(" *** End CLI ***\n"));
}

void doEraseHomeSpanNVS() {
  Preferences hs;
  if (hs.begin("HomeSpan", false)) {
    hs.clear();
    hs.end();
    Serial.println("✅ HomeSpan NVS cleared.");
  } else {
    Serial.println("⚠️ Could not open HomeSpan prefs to clear.");
  }
  delay(200);
  Serial.println("Restarting...");
  delay(200);
  ESP.restart();
}

void doFactoryResetAll() {
  // Clear HomeSpan and your app prefs (delbridge)
  Preferences hs;
  if (hs.begin("HomeSpan", false)) {
    hs.clear();
    hs.end();
    Serial.println("✅ HomeSpan NVS cleared.");
  } else {
    Serial.println("⚠️ Could not open HomeSpan prefs to clear.");
  }

  Preferences app;
  if (app.begin("delbridge", false)) {
    app.clear();
    app.end();
    Serial.println("✅ App prefs (delbridge) cleared.");
  } else {
    Serial.println("⚠️ Could not open app prefs (delbridge).");
  }

  Serial.println("Factory reset done. Restarting...");
  delay(300);
  ESP.restart();
}

void printLightweightInfo() {
  Serial.println(F("\n--- Lightweight Info ---"));
  Serial.print("WiFi status: ");
  switch (WiFi.status()) {
    case WL_CONNECTED: Serial.print("CONNECTED"); break;
    case WL_NO_SSID_AVAIL: Serial.print("NO_SSID_AVAIL"); break;
    case WL_CONNECT_FAILED: Serial.print("CONNECT_FAILED"); break;
    default: Serial.print(WiFi.status()); break;
  }
  Serial.print("  IP=");
  if (WiFi.status() == WL_CONNECTED) Serial.println(WiFi.localIP());
  else Serial.println("N/A");

  // Bridge serial from prefs (if present)
  if (prefs.isKey("bridge_serial")) {
    Serial.print("Bridge serial (prefs): ");
    Serial.println(prefs.getString("bridge_serial").c_str());
  } else Serial.println("Bridge serial (prefs): <none>");

  // Count accessories/services roughly from HomeSpan internal DB is private.
  // Provide a crude count by iterating Span's global list is not public, so show configured controllers count.
  Serial.print("Configured controllers (from compile-time list): ");
  Serial.println(NUM_CONTROLLERS);
  Serial.println("-------------------------\n");
}

String readLineFromSerial() {
  // read until newline, return trimmed string
  String s;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (s.length()) break;
      else continue;
    }
    s += c;
    // small safety cap
    if (s.length() > 128) break;
  }
  s.trim();
  return s;
}

void processCLI() {
  // Read a full line if available
  if (!Serial.available()) return;
  String line = readLineFromSerial();
  if (line.length() == 0) return;

  // Handle simple one-letter commands and parametrized ones
  if (line.equalsIgnoreCase("N")) {
    // keep your existing behavior
    prefs.putBool("request_new_id", true);
    Serial.println("Request to force new ID recorded. Restarting now...");
    delay(200);
    ESP.restart();
    return;
  }

  if (line.equalsIgnoreCase("E")) {
    Serial.println("Erasing HomeSpan pairing data (namespace 'HomeSpan')...");
    doEraseHomeSpanNVS();
    return;
  }

  if (line.equalsIgnoreCase("F")) {
    Serial.println("Performing factory reset (HomeSpan + app prefs)...");
    doFactoryResetAll();
    return;
  }

  if (line.equalsIgnoreCase("?")) {
    printCLIHelp();
    return;
  }

  if (line.equalsIgnoreCase("R")) {
    Serial.println("Restarting now...");
    delay(200);
    ESP.restart();
    return;
  }

  if (line.startsWith("L ") || line.startsWith("l ")) {
    // extract numeric param
    int sp = line.indexOf(' ');
    String param = (sp >= 0) ? line.substring(sp+1) : "";
    int lvl = param.toInt();
    if (lvl < 0) lvl = 0;
    if (lvl > 3) lvl = 3;
    homeSpan.setLogLevel(lvl);
    Serial.print("Log level set to ");
    Serial.println(lvl);
    return;
  }

  if (line.equalsIgnoreCase("i")) {
    printLightweightInfo();
    return;
  }

  // Unknown command fallback
  Serial.print("Unknown command: '");
  Serial.print(line);
  Serial.println("'. Type '?' for list of commands.");
}



void loop() {
  homeSpan.poll();
}
