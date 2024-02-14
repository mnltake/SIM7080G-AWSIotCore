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
//LoRa
#include "esp32_e220900t22s_jp_lib.h"
#include <Arduino_JSON.h>
struct  msgStruct{ 
    uint8_t conf_0 = 0x00;
    uint8_t conf_1 = 0x00;
    uint8_t channel = 0x09;
    uint16_t sensorID  ;
    uint16_t water ;
    int16_t rssi;
    uint16_t bootcount;
    volatile int status; /* 0:ready, 1:busy */
    volatile bool toCloud = false;
} ;

struct LoRaConfigItem_t config = {
    0x0000,   // own_address 0
    0b011,    // baud_rate 9600 bps
    0b10000,  // air_data_rate SF:9 BW:125
    0b00,     // subpacket_size 200
    0b1,      // rssi_ambient_noise_flag 有効
    0b00,     // transmitting_power 13 dBm
    0x09,     // own_channel 9
    0b1,      // rssi_byte_flag 有効
    0b1,      // transmission_method_type 固定送信モード
    0b111,    // wor_cycle 4000 ms
    0x0000,   // encryption_key 0
    0xFFFF,   // target_address ブロードキャスト
    0x09      // target_channel 9
};
CLoRa lora;

struct RecvFrameE220900T22SJP_t loradata;
msgStruct msg;
/** prototype declaration **/
void LoRaRecvTask(void *pvParameters);
//WDT
#include "esp_system.h"
const int wdtTimeout = 30*1000;  //time in ms to trigger the watchdog
hw_timer_t *timer = NULL;
const long activeTime = 1*60; //1分
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


uint64_t sleepSec = 60*15;//60min-実行時間5ｓ
// RTC_DATA_ATTR uint16_t bootCount = 0;
// const int deepsleep_sec = 10;
// Your GPRS credentials, if any
const char apn[] = "povo.jp";
const char gprsUser[] = "";
const char gprsPass[] = "";

// cayenne server address and port
const char server[]   = "52.194.74.83";
// const char server[]   =  "2406:da14:cf4:c600:cb00:4f76:999a:a797";
const int  port       = 1883;
const char topic[] = "LTE/SIM7080G02";
char buffer[1024] = {0};
// const int16_t sensorID = 198;
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

void LoRaRecvTask(void *pvParameters);
void MQTTSendTask(void *pvParameters);

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
    Serial.println("LTE.poweroff");
    modem.sendAT(GF("+CPOWD=1"));
    PMU.disableDC3();
    Serial.println("lora.poweroff");
    lora.SwitchToConfigurationMode();
    PMU.disableDC5();
    Serial.printf("deelsleep %dsec \n",sleepSec );
    esp_sleep_enable_timer_wakeup((sleepSec * 1000 )* 1000);
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

    //LoRa init
    // E220-900T22S(JP)へのLoRa初期設定
    PMU.setDC5Voltage(3700);    //LoRa main power channel 1400~ 3700V
    PMU.enableDC5();
    if (lora.InitLoRaModule(config)) {
        SerialMon.printf("Lora init error\n");
        // return;
    } else {
        Serial.printf("Lora init ok\n");
    }
    // Mode1　WOR送信モード(M0=0,M1=1)へ移行する
    SerialMon.printf("switch to Mode1\n");
    lora.SwitchToWORSendingMode();
    char msg[1] = { 0 }; //子機を起こすためメッセージは受信されない
    if (lora.SendFrame(config, (uint8_t *)msg, strlen(msg)) == 0) {
        SerialMon.printf("send succeeded.\n");
        SerialMon.printf("\n");
        } else {
        SerialMon.printf("send failed.\n");
        SerialMon.printf("\n");
        }
    SerialMon.printf("switch to Mode3\n");
    lora.SwitchToNormalMode();
    SerialLoRa.flush();

    xTaskCreateUniversal(LoRaRecvTask, "LoRaRecvTask", 8192, NULL, 1, NULL,
                       APP_CPU_NUM);
    xTaskCreateUniversal(MQTTSendTask, "MQTTSendTask", 8192, NULL, 1, NULL,
                       APP_CPU_NUM);
}

void loop()
{
    delay(1000);
}

void LoRaRecvTask(void *pvParameters) {
    while (1) {
        int  ret;
        if (lora.RecieveFrame(&loradata) == 0) {
            SerialMon.printf("Lora recv data:\n");
            SerialMon.printf("hex dump:\n");
            for (int i = 0; i < loradata.recv_data_len; i++) {
                SerialMon.printf("%02x ", loradata.recv_data[i]);
            }
            SerialMon.println();
            msg.sensorID = loradata.recv_data[0] | (loradata.recv_data[1]<<8) ;
            msg.water = loradata.recv_data[2] | (loradata.recv_data[3]<<8) ;
            msg.bootcount = loradata.recv_data[4] | (loradata.recv_data[5]<<8) ;
            msg.rssi = loradata.rssi;

            if (msg.status == 0) {
                /* status -> busy */
                msg.status = 1;
                msg.toCloud  = true;
            }
        }
    }
  }

void MQTTSendTask(void *pvParameters) {
    while (1) {
        if (msg.toCloud ){
        JSONVar  msgJson;
        msg.status =1;
        msgJson["sensor"] = "lteGW";
        msgJson["ID"] = String(msg.sensorID );
        msgJson["water"] = msg.water;
        msgJson["bootcount"] = msg.bootcount;
        msgJson["rssi"] = msg.rssi;
        msgJson["vbat"] = int(PMU.getBattVoltage());
        msgJson["GWname"] = clientID;
        String payload = JSON.stringify(msgJson);
        Serial.println(payload);
        msg.status =0;
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
        msg.toCloud  =false;
        }
        // activeTimeを超えたらDeepsleep
        if (millis() > activeTime*1000) { 

            // /*********************************
            // * step 6 : Set Modem to sleep mode
            // ***********************************/
            // Serial.print("Set Modem to sleep mode");

            // modem.sendAT("+CSCLK=1");
            // if (modem.waitResponse() != 1) {
            //     Serial.println("Failed!"); return;
            // }
            // Serial.println("Success!");

            //Pulling up DTR pin, module will go to normal sleep mode
            // After level conversion, set the DTR Pin output to low, then the module DTR pin is high
            // digitalWrite(BOARD_MODEM_DTR_PIN, HIGH);


            deep_sleep();
        }
    }
}

