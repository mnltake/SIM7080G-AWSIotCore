#include <HardwareSerial.h>
#include <Arduino.h>
#define XPOWERS_CHIP_AXP2102
#include "XPowersLib.h"
#include "utilities.h"

XPowersPMU  PMU;

// See all AT commands, if wanted
// #define DUMP_AT_COMMANDS

#define TINY_GSM_RX_BUFFER 1024

#define TINY_GSM_MODEM_SIM7080
#include <TinyGsmClient.h>
#include "utilities.h"
#define SerialMon Serial
#define SerialAT Serial1
HardwareSerial sim7080Serial(1); // RX=GPIO16, TX=GPIO17
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(Serial1, Serial);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(sim7080Serial);
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



#define SIM7080_RX 4
#define SIM7080_TX 5

// Sleep and wakeup intervals (milliseconds)
#define SLEEP_INTERVAL 30000 // 5 minutes
#define WAKEUP_INTERVAL 1000 // 1 second

void setup() {
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

    sim7080Serial.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);

    pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
    pinMode(BOARD_MODEM_DTR_PIN, OUTPUT);
    pinMode(BOARD_MODEM_RI_PIN, INPUT);

    int retry = 0;
    while (!modem.testAT(1000)) {
        Serial.print(".");
        if (retry++ > 6) {
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

  initSIM7080();
}

void loop() {
  getGNSS();
  sim7080Sleep();
  delay(SLEEP_INTERVAL);
  sim7080WakeUp();
  delay(WAKEUP_INTERVAL);
}

void initSIM7080() {
  sendATcommand("AT+CFUN=1");
  sendATcommand("AT+CGNSPWR=1"); // Enable GNSS
}

void getGNSS() {
  sim7080Serial.println("AT+CGNSINF");
  delay(500);

  String response = "";
  while (sim7080Serial.available()) {
    response += (char)sim7080Serial.read();
  }

  int index = response.indexOf("+CGNSINF:");
  if (index > 0) {
    String gnssInfo = response.substring(index + 9);
    Serial.println("GNSS Info:");
    Serial.println(gnssInfo);
  } else {
    Serial.println("Unable to retrieve GNSS info");
  }
}

void sim7080Sleep() {
  sendATcommand("AT+CSCLK=1"); // Enable sleep mode
}

void sim7080WakeUp() {
  sendATcommand("AT+CSCLK=0"); // Disable sleep mode
}

void sendATcommand(String command) {
  sim7080Serial.println(command);
  delay(500);

  while (sim7080Serial.available()) {
    char c = sim7080Serial.read();
    Serial.print(c);
  }
}