/**
 * @file      MinimalModemGPSExample.ino
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

XPowersPMU  PMU;

// See all AT commands, if wanted
#define DUMP_AT_COMMANDS

#define TINY_GSM_RX_BUFFER 1024

#define TINY_GSM_MODEM_SIM7080
#include <TinyGsmClient.h>
#include "utilities.h"


#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(Serial1, Serial);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif

float lat2      = 0;
float lon2      = 0;
float speed2    = 0;
float alt2      = 0;
int   vsat2     = 0;
int   usat2     = 0;
float accuracy2 = 0;
int   year2     = 0;
int   month2    = 0;
int   day2      = 0;
int   hour2     = 0;
int   min2      = 0;
int   sec2      = 0;
bool  level     = false;

// Your GPRS credentials, if any
const char apn[] = "povo.jp";
const char gprsUser[] = "";
const char gprsPass[] = "";
// mqtt server address and port
const char server[]   = "52.194.74.83";
const int  port       = 1883;
const char topic[] = "SIM7080G/01/temp";
char buffer[1024] = {0};
char username[] = "";
char password[] = "";
char clientID[] = "SIM7080G";


void setup()
{

    Serial.begin(115200);

    //Start while waiting for Serial monitoring
    while (!Serial);

    delay(3000);

    Serial.println();

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
    PMU.enableBLDO2();      //The antenna power must be turned on to use the GPS function

    // TS Pin detection must be disable, otherwise it cannot be charged
    PMU.disableTSPinMeasure();


    /*********************************
     * step 2 : start modem
    ***********************************/

    Serial1.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);

    pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);

    digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
    delay(100);
    digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
    delay(1000);
    digitalWrite(BOARD_MODEM_PWR_PIN, LOW);

    int retry = 0;
    while (!modem.testAT(1000)) {
        Serial.print(".");
        if (retry++ > 15) {
            // Pull down PWRKEY for more than 1 second according to manual requirements
            digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
            delay(100);
            digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
            delay(1000);
            digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
            retry = 0;
            Serial.println("Retry start modem .");
        }
    }
    Serial.println();
    Serial.print("Modem started!");
    // modem.gprsConnect("povo.jp", "", "");
    Serial.print(F("waitForNetwork()"));
    while (!modem.waitForNetwork()) Serial.print(".");
    Serial.println(F(" Ok."));
    Serial.print(F("gprsConnect(povp.jp)"));
    modem.gprsConnect("povo.jp", "", "");
    Serial.println(F(" done."));

    Serial.print(F("isNetworkConnected()"));
    while (!modem.isNetworkConnected()) Serial.print(".");
    Serial.println(F(" Ok."));

    // Serial.print(F("My IP addr: "));
    // IPAddress ipaddr = modem.localIP();
    // Serial.println(ipaddr);
    /*********************************
     * step 3 : start modem gps function
    ***********************************/
    // GPS function needs to be enabled for the first use
    if (modem.enableGPS() == false) {
        Serial.print("Modem enable gps function failed!!");
        while (1) {
            delay(5000);
        }
    }
    
    modem.sendAT("+CLTS=1");
    while(!modem.waitResponse(3000)){
        Serial.print(".");
    };
    modem.sendAT("+HTTPTOFS=\"http://xtrapath1.izatcloud.net/xtra3grc.bin\",\"/customer/Xtra3.bin\"");
    while(!modem.waitResponse(3000)){
        Serial.print(".");
    };
    modem.sendAT("+CGNSXTRA?");
    while(!modem.waitResponse(3000)){
        Serial.print(".");
    };
    modem.sendAT("+CGNSCPY");
        while(!modem.waitResponse(3000)){
        Serial.print(".");
    };
        modem.sendAT("+CGNSXTRA=1");
     while(!modem.waitResponse(3000)){
        Serial.print(".");
    };
    modem.sendAT("+CGNSXTRA");
    while(!modem.waitResponse(3000)){
        Serial.print(".");
    };
    modem.sendAT("+CGNSXTRA=1");
     while(!modem.waitResponse(3000)){
        Serial.print(".");
    };
    modem.sendAT("+CGNSCOLD");
        while(!modem.waitResponse(3000)){
        Serial.print(".");
    };
    // modem.gprsDisconnect();
    modem.sendAT("+SGNSCMD=0");
        while(!modem.waitResponse(3000)){
        Serial.print(".");
    };
    modem.sendAT("+SGNSCMD=1,0");
        while(!modem.waitResponse(3000)){
        Serial.print(".");
    };
    // // modem.sendAT("+SGNSCMD=2,1000,0,1");
    // modem.waitResponse();
    modem.sendAT("+CGNSPWR=1");
        while(!modem.waitResponse(3000)){
        Serial.print(".");
    };
    modem.waitResponse();
    // modem.sendAT("+SGNSCFG?");
    // modem.waitResponse();
}

void loop()
{
    if (modem.getGPS(&lat2, &lon2, &speed2, &alt2, &vsat2, &usat2, &accuracy2,
                     &year2, &month2, &day2, &hour2, &min2, &sec2)) {
        Serial.println();
        Serial.print("lat:"); Serial.print(String(lat2, 8)); Serial.print("\t");
        Serial.print("lon:"); Serial.print(String(lon2, 8)); Serial.println();
        Serial.print("speed:"); Serial.print(speed2); Serial.print("\t");
        Serial.print("altitude:"); Serial.print(alt2); Serial.println();
        Serial.print("year:"); Serial.print(year2);
        Serial.print(" montoh:"); Serial.print(month2);
        Serial.print(" day:"); Serial.print(day2);
        Serial.print(" hour:"); Serial.print(hour2);
        Serial.print(" minutes:"); Serial.print(min2);
        Serial.print(" second:"); Serial.print(sec2); Serial.println();
        Serial.println();

        // After successful positioning, the PMU charging indicator flashes quickly
        PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_4HZ);
        // while (1) {
            delay(1000);
        // }
    } else {
        // Blinking PMU charging indicator
        PMU.setChargingLedMode(level ? XPOWERS_CHG_LED_ON : XPOWERS_CHG_LED_OFF);
        level ^= 1;
        delay(1000);
    }
}
