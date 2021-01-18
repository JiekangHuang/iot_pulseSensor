
#include "config.h"
#include <Arduino.h>

#include <PubSubClient.h>
#include <TinyGsmClient.h>

#define USE_ARDUINO_INTERRUPTS true
#include <PulseSensorPlayground.h>

const int OUTPUT_TYPE = SERIAL_PLOTTER;
const int PULSE_INPUT = A0;
const int PULSE_BLINK = 13;     // Pin 13 is the on-board LED
const int PULSE_FADE  = 5;
const int THRESHOLD   = 550;     // Adjust this number to avoid noise when idle

PulseSensorPlayground pulseSensor;

#ifdef DEBUG_DUMP_AT_COMMAND
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger, AM7020_RESET);
#else
// 建立 AM7020 modem（設定 Serial 及 EN Pin）
TinyGsm modem(SerialAT, AM7020_RESET);
#endif
// 在 modem 架構上建立 Tcp Client
TinyGsmClient tcpClient(modem);
// 在 Tcp Client 架構上建立 MQTT Client
PubSubClient mqttClient(MQTT_BROKER, MQTT_PORT, tcpClient);

void mqttConnect(void);
void nbConnect(void);
void bubbleSort(int arr[], int n);

void setup()
{
    SerialMon.begin(BAUDRATE_115200);
    SerialAT.begin(BAUDRATE_115200);

    pulseSensor.analogInput(PULSE_INPUT);
    pulseSensor.blinkOnPulse(PULSE_BLINK);
    pulseSensor.fadeOnPulse(PULSE_FADE);

    pulseSensor.setSerial(Serial);
    pulseSensor.setOutputType(OUTPUT_TYPE);
    pulseSensor.setThreshold(THRESHOLD);

    // Now that everything is ready, start reading the PulseSensor signal.
    if (!pulseSensor.begin()) {
        while (true) {
            // Flash the led to show things didn't work.
            digitalWrite(PULSE_BLINK, LOW);
            delay(50);
            digitalWrite(PULSE_BLINK, HIGH);
            delay(50);
        }
    }
    // AM7020 NBIOT 連線基地台
    nbConnect();
    // 設定 MQTT KeepAlive time 為 270 秒
    mqttClient.setKeepAlive(270);
}

void loop()
{
    static unsigned long timer_0 = 0, timer_1 = 0, timer_2 = 0;
    static int           data[10];
    static unsigned int  idx  = 0;
    static int           beat = 0;

    if (millis() >= timer_2 && pulseSensor.sawStartOfBeat()) {
        timer_2 = millis() + 20;
        beat    = pulseSensor.getBeatsPerMinute();
        SerialMon.print("B=");
        SerialMon.println(beat);
    }
    if (millis() >= timer_0) {
        timer_0     = millis() + UPLOAD_INTERVAL;
        data[idx++] = beat;
    }
    // 檢查 MQTT Client 連線狀態
    if (millis() >= timer_1) {
        timer_1 = millis() + 5000;
        if (!mqttClient.connected()) {
            // 檢查 NBIOT 連線狀態
            if (!modem.isNetworkConnected()) {
                nbConnect();
            }
            SerialMon.println(F("=== MQTT NOT CONNECTED ==="));
            mqttConnect();
        }
    }
    if (idx > 9) {
        idx = 0;
        bubbleSort(data, 10);
        mqttClient.publish(PULSESENSOR_MAX_HR_TOPIC, String(data[9]).c_str());
        mqttClient.publish(PULSESENSOR_MID_HR_TOPIC, String((data[4] + data[5]) / 2).c_str());
        mqttClient.publish(PULSESENSOR_MIN_HR_TOPIC, String(data[0]).c_str());
    }
    // MQTT Client polling
    mqttClient.loop();
}

/**
 * AM7020 NBIOT 連線基地台
 */
void nbConnect(void)
{
    debugSerial.println(F("Initializing modem..."));
    // 初始化 & 連線基地台
    while (!modem.init() || !modem.nbiotConnect(APN, BAND)) {
        debugSerial.print(F("."));
    };

    debugSerial.print(F("Waiting for network..."));
    // 等待網路連線
    while (!modem.waitForNetwork()) {
        debugSerial.print(F("."));
    }
    debugSerial.println(F(" success"));
}

/**
 * MQTT Client 連線
 */
void mqttConnect(void)
{
    SerialMon.print(F("Connecting to "));
    SerialMon.print(MQTT_BROKER);
    SerialMon.print(F("..."));

    /* Connect to MQTT Broker */
    // MQTT Client 連線
    while (!mqttClient.connect("pulse_sensor_mqtt_id_20200118", MQTT_USERNAME, MQTT_PASSWORD)) {
        SerialMon.println(F(" fail"));
    }
    SerialMon.println(F(" success"));
}

/**
 * 排序（小到大）
 */
void bubbleSort(int arr[], int n)
{
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < i; ++j) {
            if (arr[j] > arr[i]) {
                int temp = arr[j];
                arr[j]   = arr[i];
                arr[i]   = temp;
            }
        }
    }
}
