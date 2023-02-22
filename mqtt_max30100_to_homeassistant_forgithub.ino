/**
 * 訂閱mqtt/cmd可以從hass獲取指令控制板子上的led燈
 * 向mqtt/state發佈on、off上傳led等的狀態
 * 向mqtt/temp發佈環境溫度
 * 向mqtt/humi發佈環境溼度
 */
#include <ESP8266WiFi.h>
#include "MAX30100_PulseOximeter.h"
#include <PubSubClient.h>
#include <Wire.h>
#include "Adafruit_GFX.h"
#include "OakOLED.h"

#define REPORTING_PERIOD_MS 1000
OakOLED oled;

#define MEASURE_RH_HOLD 0xE5
#define READ_T_FROM_PRE_RH_MEASUREMENT 0xE0
byte buffer[] = {0, 0, 0};
byte crcHumi;

const char* ssid = "xxxxxxxx";//你要讓板子鏈接的WiFi的名字
const char* password = "xxxxxxxx";//該WiFi的密碼
const char* mqtt_server = "x.x.x.x";//mqtt服務器地址
int mqtt_port = 1883;
const char* user_name = "xxxxxxxx"; // 連接 MQTT broker 的帳號密碼
const char* user_password = "xxxxxxxx";
PulseOximeter pox;

float BPM, SpO2;
uint32_t tsLastReport = 0;


const unsigned char bitmap [] PROGMEM=
{
0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x18, 0x00, 0x0f, 0xe0, 0x7f, 0x00, 0x3f, 0xf9, 0xff, 0xc0,
0x7f, 0xf9, 0xff, 0xc0, 0x7f, 0xff, 0xff, 0xe0, 0x7f, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xf0,
0xff, 0xf7, 0xff, 0xf0, 0xff, 0xe7, 0xff, 0xf0, 0xff, 0xe7, 0xff, 0xf0, 0x7f, 0xdb, 0xff, 0xe0,
0x7f, 0x9b, 0xff, 0xe0, 0x00, 0x3b, 0xc0, 0x00, 0x3f, 0xf9, 0x9f, 0xc0, 0x3f, 0xfd, 0xbf, 0xc0,
0x1f, 0xfd, 0xbf, 0x80, 0x0f, 0xfd, 0x7f, 0x00, 0x07, 0xfe, 0x7e, 0x00, 0x03, 0xfe, 0xfc, 0x00,
0x01, 0xff, 0xf8, 0x00, 0x00, 0xff, 0xf0, 0x00, 0x00, 0x7f, 0xe0, 0x00, 0x00, 0x3f, 0xc0, 0x00,
0x00, 0x0f, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

void onBeatDetected()
{
    Serial.println("Beat Detected!");
    oled.drawBitmap( 60, 20, bitmap, 28, 28, 1);
    oled.display();
}

void setup_wifi() {

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  Wire.begin();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}//鏈接WiFi

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }//串口打印出收到的信息
  Serial.println();

  // 如果收到1作爲第一個字符，則打開LED
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // 這裏他說接受的‘1’就打開燈 但是我在用的時候 接收到0纔會打開  這一行的‘LOW’和下面的‘HIGH’應該換下位置，下面也說了 ESP-01是這樣的
    client.publish("mqtt/state", "1");//鏈接成功後 會發布這個主題和語句    
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
    client.publish("mqtt/state", "0");//鏈接成功後 會發布這個主題和語句
  }

}
// 重新連接mqtt server
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";//該板子的鏈接名稱
    clientId += String(random(0xffff), HEX);//產生一個隨機數字 以免多塊板子重名
    //嘗試連接
    if (client.connect(clientId.c_str(),user_name,user_password)){
      Serial.println("connected");
      // 連接後，發佈公告......
      client.publish("mqtt/temp", "0");//鏈接成功後 會發布這個主題和語句
      // ......並訂閱
      client.subscribe("mqtt/cmd");//這個是你讓板子訂閱的主題（接受該主題的消息）
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // 如果鏈接失敗 等待五分鐘重新鏈接
      delay(500);
    }
  }
}
//設置
void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // 將BUILTIN_LED引腳初始化爲輸出
  Serial.begin(115200);
  oled.begin();
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(1);
  oled.setCursor(0, 0);
  oled.println("Initializing pulse oximeter..");
  oled.display();
  setup_wifi();
  client.setServer(mqtt_server, 1883);//MQTT默認端口是1883
  client.setCallback(callback);
  client.publish("mqtt/state", "0");//鏈接成功後 會發布這個主題和語句
  pinMode(16, OUTPUT);

  
  
  Serial.print("Initializing Pulse Oximeter..");
  if (!pox.begin())
  {
       Serial.println("FAILED");
       oled.clearDisplay();
       oled.setTextSize(1);
       oled.setTextColor(1);
       oled.setCursor(0, 0);
       oled.println("FAILED");
       oled.display();
       for(;;);
  }
  else
  {
       oled.clearDisplay();
       oled.setTextSize(1);
       oled.setTextColor(1);
       oled.setCursor(0, 0);
       oled.println("SUCCESS");
       oled.display();
       Serial.println("SUCCESS");
       pox.setOnBeatDetectedCallback(onBeatDetected);
  }
}

void loop() {
  pox.update();
  BPM = pox.getHeartRate();
  SpO2 = pox.getSpO2();
  if (millis() - tsLastReport > REPORTING_PERIOD_MS)
    {
        Serial.print("Heart rate:");
        Serial.print(BPM);
        Serial.print(" SpO2:");
        Serial.print(SpO2);
        Serial.println(" %");
 
               
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setTextColor(1);
        oled.setCursor(0,16);
        oled.println(pox.getHeartRate());
 
        oled.setTextSize(1);
        oled.setTextColor(1);
        oled.setCursor(0, 0);
        oled.println("Heart BPM");
      
        oled.setTextSize(1);
        oled.setTextColor(1);
        oled.setCursor(0, 30);
        oled.println("Spo2");
 
        oled.setTextSize(1);
        oled.setTextColor(1);
        oled.setCursor(0,45);
        oled.println(pox.getSpO2());
        oled.display();
        if (!client.connected()) {
           reconnect();
         }
        snprintf (msg, SpO2, "%f", SpO2);
        client.publish("mqtt/temp", msg);//接收該主題消息
        snprintf (msg, BPM, "%f", BPM);
        client.publish("mqtt/humi", msg);//接收該主題消息
        client.loop();
        tsLastReport = millis();
        client.loop();
     }
  
  
  
  
  
  
  
  
}
