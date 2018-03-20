#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
//const char ssid[] = "xxx";  //  your network SSID (name)
//const char pass[] = "xxx";       // your network password
//const uint16_t PixelCount = 76; // this example assumes 4 pixels, making it smaller will cause a failure

const char* host = "esp8266";
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
const uint8_t PixelPin = 2;  // make sure to set this to the correct pin, ignored for Esp8266
#define colorSaturation 250
NeoPixelAnimator animations(1); // only ever need 2 animations
NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> strip(PixelCount, PixelPin);
RgbColor red(colorSaturation, 0, 0);
RgbColor greenLessIntense(0, 60, 0);
RgbColor greenHighIntense(0, 250, 0);
RgbColor blue(0, 0, colorSaturation);
RgbColor white(30);
RgbColor black(0);

int secDisplayPixel=1,minDisplayPixel=1,hourDisplayPixel=1;
int count=0;
int stripPixels[PixelCount]={0};

static const char ntpServerName[] = "us.pool.ntp.org";
const int timeZone = 5;     // Central European Time
WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}


void setStrip()
{
  secDisplayPixel=second()*(PixelCount-1)/60.0;
  Serial.print("sec pixel:");
  Serial.println(secDisplayPixel);
  minDisplayPixel=minute()*(PixelCount-1)/60.0;
  Serial.print("min pixel:");
  Serial.println(minDisplayPixel);
  if(hour()<=12)
    hourDisplayPixel=hour()*(PixelCount-1)/12.0;
  else
    hourDisplayPixel=(hour()-12)*(PixelCount-1)/12.0;
  Serial.print("hour pixel:");
  Serial.println(hourDisplayPixel);
  
  for(int loopCount=0;loopCount<PixelCount;loopCount++)
  {
    strip.SetPixelColor(loopCount, black);    
  }
  for(int looCount=0;looCount<60;)
  {
    strip.SetPixelColor(looCount*(PixelCount-1)/60.0, white);  
    looCount+=5; 
  }
  strip.SetPixelColor(hourDisplayPixel, red);   
  strip.SetPixelColor(minDisplayPixel, blue);  
  strip.SetPixelColor(secDisplayPixel, greenHighIntense);
 
  secDisplayPixel--;
  if(secDisplayPixel<0)
    secDisplayPixel=(PixelCount-1);
   strip.SetPixelColor(secDisplayPixel, greenLessIntense);
}

void LoopAnimUpdate(const AnimationParam& param)
{
   if (param.state == AnimationState_Completed)
    {
         animations.RestartAnimation(param.index);
         setStrip();
    }
}

void setup()
{
    Serial.begin(115200);
    while (!Serial); // wait for serial attach
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, pass);
  
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
  
    Serial.print("IP number assigned by DHCP is ");
    Serial.println(WiFi.localIP());
    Serial.println("Starting UDP");
    Udp.begin(localPort);
    Serial.print("Local port: ");
    Serial.println(Udp.localPort());
    Serial.println("waiting for sync");
    setSyncProvider(getNtpTime);
    setSyncInterval(500);
    strip.Begin();         
    animations.StartAnimation(0, 500, LoopAnimUpdate);     
    Serial.println();
    Serial.println("Running...");
    MDNS.begin(host);
    httpUpdater.setup(&httpServer);
    httpServer.begin();
    MDNS.addService("http", "tcp", 80);
}

void loop(){
    httpServer.handleClient();
    animations.UpdateAnimations();
    strip.Show();
}
