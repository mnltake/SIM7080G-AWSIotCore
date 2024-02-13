/**
 * @file      ModemMqttPulishExample.ino
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2022  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2022-09-16
 *
 */
#include <Arduino.h>
#define XPOWERS_CHIP_AXP2102
#include "XPowersLib.h"
#include "utilities.h"
#include <esp_sleep.h>
//WDT
#include "esp_system.h"
const int wdtTimeout = 30*1000;  //time in ms to trigger the watchdog
hw_timer_t *timer = NULL;

XPowersPMU  PMU;
#define SW_LOW 16
#define SW_HIGH 17
#define SW_COM 18
#define SECOND_SW_LOW 8
#define SECOND_SW_HIGH 3
#define SECOND_SW_COM 46
// See all AT commands, if wanted
// #define DUMP_AT_COMMANDS

#define TINY_GSM_RX_BUFFER 1024

#define TINY_GSM_MODEM_SIM7080
#include <TinyGsmClient.h>
#include "utilities.h"
#define SerialMon Serial
#define SerialAT Serial1

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(Serial1, Serial);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif

const char *register_info[] = {
    "Not registered, MT is not currently searching an operator to register to.The GPRS service is disabled, the UE is allowed to attach for GPRS if requested by the user.",
    "Registered, home network.",
    "Not registered, but MT is currently trying to attach or searching an operator to register to. The GPRS service is enabled, but an allowable PLMN is currently not available. The UE will start a GPRS attach as soon as an allowable PLMN is available.",
    "Registration denied, The GPRS service is disabled, the UE is not allowed to attach for GPRS if it is requested by the user.",
    "Unknown.",
    "Registered, roaming.",
};

enum {
    MODEM_CATM = 1,
    MODEM_NB_IOT,
    MODEM_CATM_NBIOT,
};

#define randMax 35
#define randMin 18
uint64_t sleepSec = 15*60 - 5;//60min-実行時間5ｓ
RTC_DATA_ATTR uint16_t bootCount = 0;
// const int deepsleep_sec = 10;
// Your GPRS credentials, if any
const char apn[] = "povo.jp";
const char gprsUser[] = "";
const char gprsPass[] = "";

// cayenne server address and port
// const char server[]   = "52.194.74.83";
const char server[]   =  "2406:da14:cf4:c600:cb00:4f76:999a:a797";
const int  port       = 1883;
const char topic[] = "LTE/SIM7080G02";
char buffer[1024] = {0};
const int16_t sensorID = 198;
// To create a device : https://cayenne.mydevices.com/cayenne/dashboard
//  1. Add new...
//  2. Device/Widget
//  3. Bring Your Own Thing
//  4. Copy the <MQTT USERNAME> <MQTT PASSWORD> <CLIENT ID> field to the bottom for replacement
char username[] = "";
char password[] = "";
char clientID[] = "SIM7080G";

// To create a widget
//  1. Add new...
//  2. Device/Widget
//  3. Custom Widgets
//  4. Value
//  5. Fill in the name and select the newly created equipment
//  6. Channel is filled as 0
//  7.  Choose ICON
//  8. Add Widget
int data_channel = 0;


bool isConnect()
{
    modem.sendAT("+SMSTATE?");
    if (modem.waitResponse("+SMSTATE: ")) {
        String res =  modem.stream.readStringUntil('\r');
        return res.toInt();
    }
    return false;
}

void IRAM_ATTR deep_sleep(){
    timerWrite(timer, 0);
    Serial.println("modem.poweroff");
    modem.sendAT(GF("+CPOWD=1"));
    PMU.disableDC3();
    Serial.printf("sleep \n");

    // if (bootCount == 0){
    //     delay(10000);
    // } 
    // if (bootCount < 10)
    // {
    //     sleepSec = 25;
    // }
    
    // bootCount++;

    // digitalWrite(LED1 ,LOW);
    esp_sleep_enable_timer_wakeup(sleepSec * 1000 * 1000);
    esp_deep_sleep_start();
}

void setup()
{
    pinMode( SW_LOW ,INPUT_PULLUP);
    pinMode( SW_HIGH ,INPUT_PULLUP);
    pinMode( SW_COM ,OUTPUT);
    digitalWrite ( SW_COM ,LOW);
    pinMode( SECOND_SW_LOW ,INPUT_PULLUP);
    pinMode( SECOND_SW_HIGH ,INPUT_PULLUP);
    pinMode( SECOND_SW_COM ,OUTPUT);
    digitalWrite ( SECOND_SW_COM ,LOW);
    timer = timerBegin(0, 80, true);                  //timer 0, div 80
    timerAttachInterrupt(timer, &deep_sleep, true);  //attach callback
    timerAlarmWrite(timer, wdtTimeout * 1000, false); //set time in us
    // timerAlarmEnable(timer);                          //enable interrupt
    timerWrite(timer, 0);
    Serial.begin(115200);

    //Start while waiting for Serial monitoring
    // while (!Serial);

    delay(3000);

    Serial.println();
    // Set the sleep mode to Deep Sleep
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_ON);
    // esp_sleep_enable_timer_wakeup(deepsleep_sec * 1000000ULL); // Set the sleep time to 600 seconds
    /*********************************
     *  step 1 : Initialize power chip,
     *  turn on modem and gps antenna power channel
    ***********************************/
    if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        Serial.println("Failed to initialize power.....");
        while (1) {
            delay(5000);
        }
    }
    //Set the working voltage of the modem, please do not modify the parameters
    PMU.setDC3Voltage(3000);    //SIM7080 Modem main power channel 2700~ 3400V
    PMU.enableDC3();

    //Modem GPS Power channel
    PMU.setBLDO2Voltage(3300);
    PMU.disableBLDO2();      //The antenna power must be turned on to use the GPS function

    // TS Pin detection must be disable, otherwise it cannot be charged
    PMU.disableTSPinMeasure();
    // Get the VSYS shutdown voltage
    uint16_t vol = PMU.getSysPowerDownVoltage();
    Serial.printf("->  getSysPowerDownVoltage:%u\n", vol);

    // Set VSY off voltage as 2600mV , Adjustment range 2600mV ~ 3300mV
    // PMU.setSysPowerDownVoltage(2600);
    // Enable internal ADC detection
    PMU.enableBattDetection();
    PMU.enableVbusVoltageMeasure();
    PMU.enableBattVoltageMeasure();
    PMU.enableSystemVoltageMeasure();
    /*********************************
     * step 2 : start modem
    ***********************************/

    Serial1.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);

    pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
    pinMode(BOARD_MODEM_DTR_PIN, OUTPUT);
    pinMode(BOARD_MODEM_RI_PIN, INPUT);

    int retry = 0;
    while (!modem.testAT(1000)) {
        Serial.print(".");
        retry++;
        if(retry < 6){
            delay(2000);
        }
        else if (retry > 6) {
            // Pull down PWRKEY for more than 1 second according to manual requirements
            digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
            delay(100);
            digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
            delay(1000);
            digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
             Serial.println("Retry start modem .");
            timerWrite(timer, 0);
        }else if (retry ==6 )
        {
            PMU.disableDC3();
            delay(1000);
            PMU.setDC3Voltage(3400);    //SIM7080 Modem main power channel 2700~ 3400V
            PMU.enableDC3();
            retry = 0;
        }
        
    }
    Serial.println();
    Serial.print("Modem started!");

  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem Info: ");
  SerialMon.println(modemInfo);
//   modem.gprsConnect("povo.jp", "", "");// 初回だけ必要
  SerialMon.print(F("waitForNetwork()"));
  while (!modem.waitForNetwork()) SerialMon.print(".");
  SerialMon.println(F(" Ok."));

  SerialMon.print(F("gprsConnect(povo.jp)"));
  modem.gprsConnect("povo.jp", "", "");
  SerialMon.println(F(" done."));

  SerialMon.print(F("isNetworkConnected()"));
  while (!modem.isNetworkConnected()) SerialMon.print(".");
  SerialMon.println(F(" Ok."));

  SerialMon.print(F("My IP addr: "));
  IPAddress ipaddr = modem.localIP();
  SerialMon.println(ipaddr);

    /*********************************
     * step 3 : Check if the SIM card is inserted
    ***********************************/
    String result ;


    if (modem.getSimStatus() != SIM_READY) {
        Serial.println("SIM Card is not insert!!!");
        return ;
    }


    /*********************************
     * step 4 : Set the network mode to NB-IOT
    ***********************************/
    timerWrite(timer, 0);
    modem.setNetworkMode(2);    //38:LTE only 2:auto

    modem.setPreferredMode(MODEM_CATM);

    uint8_t pre = modem.getPreferredMode();

    uint8_t mode = modem.getNetworkMode();

    Serial.printf("getNetworkMode:%u getPreferredMode:%u\n", mode, pre);


    /*********************************
    * step 5 : Wait for the network registration to succeed
    // ***********************************/
    RegStatus s;
    // do {
    //     s = modem.getRegistrationStatus();
    //     if (s != REG_OK_HOME && s != REG_OK_ROAMING) {
    //         Serial.print(".");
    //         delay(1000);
    //     }

    // } while (s != REG_OK_HOME && s != REG_OK_ROAMING) ;

    Serial.println();
    Serial.print("Network register info:");
    Serial.println(register_info[s]);

    // Activate network bearer, APN can not be configured by default,
    // if the SIM card is locked, please configure the correct APN and user password, use the gprsConnect() method
    timerWrite(timer, 0);
    bool res = modem.isGprsConnected();
    if (!res) {
        modem.sendAT("+CNACT=0,1");
        if (modem.waitResponse() != 1) {
            Serial.println("Activate network bearer Failed!");
            return;
        }
        if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
            return ;
        }
    }

    Serial.print("GPRS status:");
    Serial.println(res ? "connected" : "not connected");
    modem.sendAT("+CASRIP=1");
    modem.waitResponse();

    /*********************************
    * step 6 : setup MQTT Client
    ***********************************/

    // If it is already connected, disconnect it first
    modem.sendAT("+SMDISC");
    modem.waitResponse();


    snprintf(buffer, 1024, "+SMCONF=\"URL\",\"%s\",%d", server, port);
    modem.sendAT(buffer);
    if (modem.waitResponse() != 1) {
        return;
    }
    // snprintf(buffer, 1024, "+SMCONF=\"USERNAME\",\"%s\"", username);
    // modem.sendAT(buffer);
    // if (modem.waitResponse() != 1) {
    //     return;
    // }

    // snprintf(buffer, 1024, "+SMCONF=\"PASSWORD\",\"%s\"", password);
    // modem.sendAT(buffer);
    // if (modem.waitResponse() != 1) {
    //     return;
    // }

    snprintf(buffer, 1024, "+SMCONF=\"CLIENTID\",\"%s\"", clientID);
    modem.sendAT(buffer);
    if (modem.waitResponse() != 1) {
        return;
    }

    do {

        modem.sendAT("+SMCONN");
        res = modem.waitResponse(30000);
        if (!res) {
            Serial.println("Connect failed, retry connect ..."); delay(1000);
        }

    } while (res != 1);

    Serial.println("MQTT Client connected!");


}

void loop()
{
    // if (!isConnect()) {
    //     Serial.println("MQTT Client disconnect!"); delay(1000);
    //     return ;
    // }
    timerWrite(timer, 0);
    Serial.println();
    // Publish fake temperature data
    String payload = "{\"sensor\":\"lteGW\",\"ID\":\"";
    payload.concat(sensorID);
    payload.concat("\",\"water\":\"");
    int water1 = digitalRead( SW_LOW) * 49 + digitalRead( SW_HIGH) * 51; //ここに水位;
    payload.concat(water1);
    payload.concat("\",\"bootcount\":\"");
    int bootcount = 0; 
    payload.concat(bootcount); 
    payload.concat("\",\"rssi\":\"");
    int rssi = -100;
    payload.concat(rssi); 
    // payload.concat(",\"water2\":");
    // int water2 = digitalRead( SECOND_SW_LOW) * 49 + digitalRead( SECOND_SW_HIGH) * 51; //ここに水位;
    // payload.concat(water2);
    payload.concat("\",\"vbat\":\"");
    payload.concat(PMU.getBattVoltage()/42.2);
    payload.concat("\"}");
    // payload.concat("\",\"bootCount3\":\"");
    // payload.concat(bootCount);
    // payload.concat("\",\"lat\":\"34.953950\",\"lon\":\"136.935557\"}");
    Serial.println(payload);
    // AT+SMPUB=<topic>,<content length>,<qos>,<retain><CR>message is enteredQuit edit mode if messagelength equals to <contentlength>
    snprintf(buffer, 1024, "+SMPUB=\"%s\",%d,1,1", topic, payload.length());
    modem.sendAT(buffer);
    if (modem.waitResponse(">") == 1) {
        modem.stream.write(payload.c_str(), payload.length());
        Serial.print("Try publish payload: ");
        Serial.println(payload);

        if (modem.waitResponse(3000)) {
            Serial.println("Send Packet success!");
        } else {
            Serial.println("Send Packet failed!");
        }
    }
    /*********************************
    * step 6 : Set Modem to sleep mode
    ***********************************/
    Serial.print("Set Modem to sleep mode");

    modem.sendAT("+CSCLK=1");
    if (modem.waitResponse() != 1) {
        Serial.println("Failed!"); return;
    }
    Serial.println("Success!");

    //Pulling up DTR pin, module will go to normal sleep mode
    // After level conversion, set the DTR Pin output to low, then the module DTR pin is high
    digitalWrite(BOARD_MODEM_DTR_PIN, HIGH);

    // Wake up Modem after 60 seconds
    deep_sleep();


    // esp_deep_sleep_start(); 
}

