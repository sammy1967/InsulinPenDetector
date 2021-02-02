// Insulin Pen Detector

//#include <Arduino.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>

#include <ESP_Mail_Client.h>

#define triggerValue 100

//Your WiFi Credentials
const char* ssid = "XXXXXX";
const char* password = "XXXXXX";

// NTP Servers:

static const char ntpServerName[] = "pool.ntp.org";
float timeZone = 1.0;  //+1 GMT

WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

time_t getNtpTime();
void sendNTPpacket(IPAddress &address);

#define baud_rate 9600

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

const int oledAddress = 0x3C; // I2C Adresss OLED display

const int RECV_PIN = A0;

bool send_message = false;

#define SMTP_HOST "XXXXXX"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "esp8266@XXXXXX"
#define AUTHOR_PASSWORD "XXXXXX"
#define EMAIL_RECIPIENT "XXXXXX"

SMTPSession smtp;

long start_now = 0;

bool penDetected = false;

void smtpCallback(SMTP_Status status);

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status)
{
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success())
  {
    Serial.println("----------------");
    Serial.printf("Message sent success: %d\n", status.completedCount());
    Serial.printf("Message sent failled: %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);
      localtime_r(&result.timesstamp, &dt);

      Serial.printf("Message No: %d\n", i + 1);
      Serial.printf("Status: %s\n", result.completed ? "success" : "failed");
      Serial.printf("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      Serial.printf("Recipient: %s\n", result.recipients);
      Serial.printf("Subject: %s\n", result.subject);
    }
    Serial.println("----------------\n");
  }
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime() {
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1600) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
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
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address) {
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

void setup() {
    // Setup Environment
    Serial.begin(baud_rate);
    Serial.println("Starting Program Init");
    
    // Initialize Display
    
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    // Serial.println("Starting Display");
    if(!display.begin(SSD1306_SWITCHCAPVCC, oledAddress)) {
      Serial.println(F("SSD1306 allocation failed"));
    }
    
    // Show initial display buffer contents on the screen --
    // the library initializes this with an Adafruit splash screen.
    display.display();
    delay(1000); // Pause for 1 second
    outputdata ("Insulin Pen Detector","READY for use","ESP8266/V1.0", "Starting up");

    connectToWiFi();

    Udp.begin(localPort);
    setSyncProvider(getNtpTime);
    setSyncInterval(360);
    if (hour() == 0 && second() < 3) {
      setSyncProvider(getNtpTime);
    }
    
    Serial.println("Ended Program Init");
}

// Output data to the display 
void outputdata (String line1, String line2, String line3, String line4) {
    display.clearDisplay(); 
    display.setTextSize(1);             // Normal 1:1 pixel scale
    display.setCursor(0,0);             // Start at top-left corner
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Draw 'inverse' text
    display.println("---------------------");
    display.setTextColor(SSD1306_WHITE);        // Draw white text
    display.println(line1);
    display.println(line2);
    display.println(line3);
    display.println(line4); 
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Draw 'inverse' text
    display.println("---------------------");
    display.display();
    
}

// output date time
void outputtiming (long sNow, bool penThere){

   long runHours = 0;
   long runMinutes = 0;
  
   display.clearDisplay();
   display.setTextSize(2);             
   display.setCursor(0,0);             // Start at top-left corner

   display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Draw 'inverse' text
   display.print(hour());
   display.print(":");
   if (minute() < 10) {
      display.print ("0");
   }
   display.println (minute());   
   display.setTextSize(1);             // Normal 1:1 pixel scale
   display.setTextColor(SSD1306_WHITE);        // Draw white text
   display.println ("Elapsed Time");
   display.setTextSize(3);
   if (penThere == true) {
      // Serial.println ("Pen there printing");
      runHours = (now() - sNow)/3600;
      runMinutes = ((now() - sNow)%3600)/60;
      display.print (runHours);
      display.print (":");
      if (runMinutes < 10) {
        display.print ("0");
      }
      display.println (runMinutes); 
      if (runHours > 24 && send_message == false) {    
        send_email_message ();
        send_message = true;
      }
      if (runHours < 1 && send_message == true) {
        send_message = false;
      }
   } else {
      display.println ("NO PEN");                
   }
   
   display.display();
}

//Connect to WiFI
void connectToWiFi() {
  Serial.print("Connecting to: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("");
  Serial.println("Connected!");
}

void send_email_message () {
  
    smtp.debug(1);
    smtp.callback(smtpCallback);
    /* Declare the session config data */
    ESP_Mail_Session session;

    /* Set the session config */
    session.server.host_name = SMTP_HOST;
    session.server.port = SMTP_PORT;
    session.login.email = AUTHOR_EMAIL;
    session.login.password = AUTHOR_PASSWORD;
    session.login.user_domain = "mydomain.net";

    /* Declare the message class */
    SMTP_Message message;

    message.sender.name = "Insulin Pen Bot";
    message.sender.email = AUTHOR_EMAIL;
    message.subject = "Do not forget to take your insulin!";
    message.addRecipient("Someone", EMAIL_RECIPIENT);

    message.text.content = "I noticed you didnt use your insulin pen for 25 hours";

    message.text.charSet = "us-ascii";

    message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

    message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;

    message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;

    /* Set the custom message header */
    message.addHeader("Message-ID: <AUTHOR_EMAIL>");

    /* Connect to server with the session config */
    if (!smtp.connect(&session))
      return;

    /* Start sending Email and close the session */
    if (!MailClient.sendMail(&smtp, &message))
      Serial.println("Error sending Email, " + smtp.errorReason());
}

void loop() {
  
  // put your main code here, to run repeatedly:
  Serial.print ("Value pin ");
  Serial.println (analogRead(RECV_PIN));
  Serial.print ("Time ");
  Serial.print (day());  
  Serial.print (":");
  Serial.print (hour());
  Serial.print (":");
  Serial.println (minute());

  if (analogRead(RECV_PIN) < triggerValue && penDetected == false) {
      start_now = now();
      penDetected = true;
      Serial.print ("Starting clock ");
      Serial.println (start_now);
  } else if (analogRead(RECV_PIN) >= triggerValue && penDetected == true) {
      penDetected = false;
      Serial.println ("Stopped Clock");
  }

  outputtiming (start_now,penDetected);

  delay (1000);
}
