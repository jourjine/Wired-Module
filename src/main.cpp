#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>          // Библиотека для Обновления по ОТА
#include <PubSubClient.h>        // MQTT клиент
#include <ESP8266WebServer.h>    // Библиотека для Веб Сервера
#include <WiFiManager.h>         // https://github.com/tzapu/WiFiManager
#include "secrets.txt"           // Файл с паролями и настройками IP
#include <Bounce2.h>             // Библиотека антидребезга.

// ************************************************
// ПинАут платы версия 1
// ************************************************
#define LED         5       // светодиод на плате
#define INPUT_1     10      // Вход 1
#define INPUT_2     9       // Вход 2
#define INPUT_3     13      // Вход 3
#define INPUT_4     14      // Вход 4
#define OUTPUT_1    16      // Выход 1
#define OUTPUT_2    12      // Выход 2
#define OUTPUT_3    4       // Выход 3
#define BUTTON_PRG  0       // кнопка на плате, она же для программирования
#define ADC_1       A0      // аналоговый вход

// ************************************************
// Переменные
// ************************************************

unsigned long  currentTimeMain, loopTimeMain, currentTime5sec, loopTime5sec;  // Переменные для главного цикла
unsigned int onesecond = 1000;                                                // одна секунда в милисекундах
unsigned int fivesecond = 5000;                                               // пять секунд в милисекундах
const float Vout = 5.056;                                                     // опорное напряжение для аналогово входа
const float adcRes = 1024;                                                    // разрядность аналогово входа
float average;                                                                // среднее показание RAW ADC
float mVolts;                                                                 // Миливольты
float zeroAmp;                                                                // нулевые показания токого чипа ACS712
const float sensetivity = 0.185;                                              // чувствительность датчка мв/А на 5А
//float divider;

// ************************************************
// Переменные для кнопок.
// ************************************************
int button1Pressed, button2Pressed, button3Pressed, button4Pressed;
int button1State, relay1State;
int button2State, relay2State;
int button3State, relay3State;
int currentOutput, currentRelayState, currentButtonState;
int value;
unsigned long buttonPressTimeStamp;

// ************************************************
// MQTT Variables
// ************************************************
int mystatus;
int rssistr;
double my_double;
float my_float;
int my_int;
bool my_bool;
int lastReconnectAttempt = 0;
char buffer[10];                        // буффер для MQTT
char data[40];                          // mqtt буффер


// ************************************************
// Подписываемые топики
// ************************************************
const char* subcribe_topic[] ={"/esp/wired1/led",
                               "/esp/wired1/output1",
                               "/esp/wired1/output2",
                               "/esp/wired1/output3",
                               "/esp/wired1/input1",
                               "/esp/wired1/input2",
                               "/esp/wired1/input3",
                               "/esp/wired1/input4"
                            };

char *dtostrf(double val, signed char width, unsigned char prec, char *s); //конвертируем числа в строку для MQTT

// ************************************************
// Иницилизация инстантов
// ************************************************
WiFiClient espClient;               // ФайФай менеджер + настройки ВайФая
PubSubClient mqttclient(espClient); // MQTT клиент

Bounce debouncer1 = Bounce();       // Антидребез 1 входа
Bounce debouncer2 = Bounce();       // Антидребез 2 входа
Bounce debouncer3 = Bounce();       // Антидребез 3 входа
Bounce debouncer4 = Bounce();       // Антидребез 4 входа

// ************************************************
// функия подучение топиков с сервера
// ************************************************

void callback(char* topic, byte* payload, unsigned int length) {

    int i = 0;                                            // индекс в ноль
    for (i = 0; i < length; i++) {                        // перенвод строки в символьный буфер
      data[i] = payload[i];
    }
    data[i] = '\0';                                       // последний символ в массиве должен быть '\0'
    const char *p_payload = data;
    char *end;
    my_double = strtod(p_payload, &end);
    if (end == p_payload) {

    } else {
        my_int = atoi(p_payload);                         // перевод строки в целое значение
        my_float = atof(p_payload);                       // перевод строки в флоат значение
        if (my_int < 0 || my_int > 0) my_bool = true;     // если ноль то лож иначе правда, перевод в логиское значение.
        else my_bool = false;
      }

      if (strcmp(topic, "/esp/wired1/led") == 0 ) {       // обработка если пришел топик для светодиода
        if (my_bool) digitalWrite(LED, HIGH);             // если пришела цифра "Один" то включаем
        else digitalWrite(LED, LOW);                      // иначе выключаем
      }
      if (strcmp(topic, "/esp/wired1/output1") == 0 ) {
        if (my_bool) digitalWrite(OUTPUT_1, HIGH);
        else digitalWrite(OUTPUT_1, LOW);
      }
      if (strcmp(topic, "/esp/wired1/output2") == 0 ) {
        if (my_bool) digitalWrite(OUTPUT_2, HIGH);
        else digitalWrite(OUTPUT_2, LOW);
      }

      if (strcmp(topic, "/esp/wired1/output3") == 0 ) {
        if (my_bool) digitalWrite(OUTPUT_3, HIGH);
        else digitalWrite(OUTPUT_3, LOW);
      }


      if (strcmp(topic, "/esp/wired1/input1") == 0 ) {
        if (my_bool) digitalWrite(LED, HIGH);
        else digitalWrite(LED, LOW);
      }
      if (strcmp(topic, "/esp/wired1/input3") == 0 ) {
        if (my_bool) digitalWrite(OUTPUT_2, HIGH);
        else digitalWrite(OUTPUT_2, LOW);
      }

      if (strcmp(topic, "/esp/wired1/input3") == 0 ) {
        if (my_bool) digitalWrite(OUTPUT_3, HIGH);
        else digitalWrite(OUTPUT_3, LOW);
      }

      if (strcmp(topic, "/esp/wired1/input4") == 0 ) {
        if (my_bool) digitalWrite(OUTPUT_1, HIGH);
        else digitalWrite(OUTPUT_1, LOW);
      }

    }

// ************************************************
// функция подписывания на топики
// ************************************************

void broker_subcribe() {
   for (int i = 0; i < (sizeof(subcribe_topic)/sizeof(int)); i++){
     mqttclient.subscribe(subcribe_topic[i]);
     mqttclient.loop();
  }
}

// ************************************************
// функция подключения к MQTT серверу
// ************************************************
boolean reconnect() {
    mqttclient.connect(MQTTCLIENTID, mqtt_username, mqtt_password);
    mqttclient.publish("/esp/wired1/status", "Соеденение установленно!!!");
    return mqttclient.connected();
}

// ************************************************
// функция уровня сигнала Wi-Fi
// ************************************************
void wifiRSSI(){
  rssistr = WiFi.RSSI();
  dtostrf(rssistr, 3, 0, buffer);               // Перевод числа в строку, три символа, ноль после запятой.
  mqttclient.publish("/esp/wired1/rssi", buffer);

}

int calibrateSensor() { // увеличиваем разядность с 10 до 12 бит
        int ad2 = 0;
        int indexadc = 0;
        while (indexadc < 16) {
              ad2 += analogRead(ADC_1);
              indexadc++;
         }
         ad2 = ad2 >> 4;
         return ad2;
       }

// void readAllInputs() {
//   if (!digitalRead(INPUT_1) && !input1Pressed) {
//     input1Pressed = true;
//   } else if (!input1Pressed) {
//     input1Pressed = false;
//   }
//
//
//

//
//
//
//   mqttclient.publish("/esp/wired1/input1", "0");
//     mqttclient.publish("/esp/wired1/input1", "1");
//   if (!digitalRead(INPUT_2)) mqttclient.publish("/esp/wired1/input2", "1");
//   else mqttclient.publish("/esp/wired1/input2", "0");
//   if (!digitalRead(INPUT_3)) mqttclient.publish("/esp/wired1/input3", "1");
//   else mqttclient.publish("/esp/wired1/input3", "0");
//   if (!digitalRead(INPUT_4)) mqttclient.publish("/esp/wired1/input4", "1");
//   else mqttclient.publish("/esp/wired1/input4", "0");
//   //mqttclient.loop();
// }

float readRMSAmps(){
  uint32_t period = 200;
  uint32_t measurements_count = 0;
  uint32_t Inow = 0;
  unsigned long rSquaredSum = 0;
  uint32_t t_start = millis();

	while ((millis() - t_start) < period) {
		Inow += analogRead(ADC_1) - zeroAmp;
    rSquaredSum = Inow*Inow;
		measurements_count++;
	}

	float Irms = sqrt((float)rSquaredSum / (float)measurements_count) / divider;//27985.64;
  return Irms;
}

void readButtonStatus() {
  button1Pressed = debouncer1.update();
  button2Pressed = debouncer2.update();
  button3Pressed = debouncer3.update();
  button4Pressed = debouncer4.update();
}

void input1Logic() {

  int button1Index, button1DblPress;

  if (debouncer1.read()) {
    button1Pressed = LOW;
  } else {
    button1Pressed = HIGH;
    button1Index++;
      if (button1Index == 2) {
        button1DblPress = true;
        button1Index = 0;
      }
    }

    if (button1DblPress) {
      digitalWrite(LED, LOW);
      digitalWrite(OUTPUT_1, LOW);
      digitalWrite(OUTPUT_2, LOW);
      digitalWrite(OUTPUT_3, LOW);
      button1DblPress = false;
    }
}

void input2Logic() {

  if (debouncer2.rose()) button2Pressed = HIGH;

  if (debouncer2.fell()) button2Pressed = LOW;

  digitalWrite(LED, button2Pressed);
}

void measureRMSA() {
  float U = 230;

  // To measure current we need to know the frequency of current
  // By default 50Hz is used, but you can specify own, if necessary
  float I = sensor.getCurrentAC();

  // To calculate the power we need voltage multiplied by current
  float P = U * I;

  Serial.println(String("I = ") + I + " A");
  Serial.println(String("P = ") + P + " Watts");
}

void setup() {



  Serial.begin(115200);

 // **************************
 // PIN inizialice
 // **************************

 pinMode(LED, OUTPUT);            // Красный светодиод
 pinMode(OUTPUT_1, OUTPUT);            // Красный светодиод
 pinMode(OUTPUT_2, OUTPUT);            // Красный светодиод
 pinMode(OUTPUT_3, OUTPUT);            // Красный светодиод

 digitalWrite(LED, LOW);
 digitalWrite(OUTPUT_1, LOW);
 digitalWrite(OUTPUT_2, LOW);
 digitalWrite(OUTPUT_3, LOW);


 //pinMode(BUTTON_PRG, INPUT);
 pinMode(BUTTON_PRG, INPUT_PULLUP);

 pinMode(INPUT_1, INPUT_PULLUP);
 pinMode(INPUT_2, INPUT_PULLUP);
 pinMode(INPUT_3, INPUT_PULLUP);
 pinMode(INPUT_4, INPUT);
 pinMode(ADC_1, INPUT);




 // **************************
 // WiFi inizialice
 // **************************
 WiFiManager wifiManager;
 IPAddress _ip,_gw,_sn;
 _ip.fromString(static_ip);
 _gw.fromString(static_gw);
 _sn.fromString(static_sn);
 wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);
 if (!wifiManager.autoConnect("Wired1Config", "password")) {
     Serial.println("failed to connect, we should reset as see if it connects");
     delay(15000);
     ESP.reset();
     delay(5000);
   }

 // **************************
 // OTA inizialice
 // **************************
     ArduinoOTA.onStart([]() {
       Serial.println("Start");
       digitalWrite(LED, HIGH);

     });
     ArduinoOTA.onEnd([]() {
       digitalWrite(LED, LOW);

      Serial.println("\nEnd");

    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));

    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

     ArduinoOTA.begin();

     // **************************
     // MQTT inizialice
     // **************************
     mqttclient.setServer(mqtt_server, 1883);
     mqttclient.setCallback(callback);
     lastReconnectAttempt = 0;

       Serial.println("Calibrating... Ensure that no current flows through the sensor at this moment");
       sensor.calibrate();
       Serial.println("Done!");

//     zeroAmp = calibrateSensor();
//     divider = float(adcRes * Vout * sensetivity);

     // After setting up the button, setup the Bounce instance :
       debouncer1.attach(INPUT_1);
       debouncer1.interval(50); // interval in ms
       debouncer2.attach(INPUT_2);
       debouncer2.interval(50); // interval in ms
      //  debouncer3.attach(INPUT_3);
      //  debouncer3.interval(20); // interval in ms
      //  debouncer4.attach(INPUT_4);
      //  debouncer4.interval(20); // interval in ms


//     calibrateSensor();
}

void loop() {
  ArduinoOTA.handle();
  //readButtonStatus();
  if (debouncer1.update()) input1Logic();
  if (debouncer2.update()) input2Logic();

    if (!mqttclient.connected()) {
      long now = millis();
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        // Attempt to reconnect
        if (reconnect()) {
          lastReconnectAttempt = 0;
          broker_subcribe();
        }
      }
    } else {
      mqttclient.loop();


      }


      // Секундный интервал

          currentTimeMain = millis();
          if (currentTimeMain >= (loopTimeMain + onesecond)) {               // сравниваем текущий таймер с переменной loopTime + 1 секунда
          wifiRSSI();


//          readAllInputs();
          // int rawraw = analogRead(ADC_1);
          // Serial.print("Raw ADC: ");
          // Serial.println(rawraw);

        //  currentRead();
            loopTimeMain = currentTimeMain;
          }

          // Пяти Секундный интервал

              currentTime5sec = millis();
              if (currentTime5sec >= (loopTime5sec + fivesecond)) {               // сравниваем текущий таймер с переменной loopTime + 1 секунда
                measureRMSA();
        //        float irms = readRMSAmps();

          //      dtostrf(irms, 5, 3, buffer);
          //      mqttclient.publish("/esp/wired1/current", buffer);
                // Serial.print("RMS I: ");
                // Serial.print(irms);
                // Serial.println(" A");
                loopTime5sec = currentTime5sec;
              }


}
