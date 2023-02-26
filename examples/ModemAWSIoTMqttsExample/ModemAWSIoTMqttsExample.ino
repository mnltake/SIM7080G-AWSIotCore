/**
 * @file      ModemMqttPulishExample.ino
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2022  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2022-09-16
 *
 * @file      ModemAWSIoTMqttsExample.ino
 * @author    mnltake
 * @license   MIT
 * @date      2023-02-26
 */
#include <Arduino.h>

#ifdef ESP32  // M5stack M5stickC + CAT-M unit
    #include <M5StickC.h>
    #include <HardwareSerial.h>
    #define BOARD_MODEM_RXD_PIN 33 //modem → mcu
    #define BOARD_MODEM_TXD_PIN 32 //mcu → modem
#elif defined(RASPBERRYPI_PICO) // waveshare Pico SIM7080G Cat-M/NB-IoT
    #include "pico/stdlib.h"
    #include "pico.h"
    #include "hardware/structs/vreg_and_chip_reset.h"
    #include "hardware/vreg.h"
    #include "hardware/watchdog.h"
    #define BOARD_MODEM_RXD_PIN 1 //modem → mcu
    #define BOARD_MODEM_TXD_PIN 0 //mcu → modem
    #define BOARD_MODEM_PWR_PIN 14
    #define BOARD_MODEM_DTR_PIN 17
#endif


#define SerialAT Serial1
// See all AT commands, if wanted
#define DUMP_AT_COMMANDS

#define TINY_GSM_RX_BUFFER 1024

#define TINY_GSM_MODEM_SIM7080
#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
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

/*
 see https://qiita.com/kaz19610303/items/36ca6ab7398e45aa9e37
 DOWNLOAD https://certs.secureserver.net/repository/sf-class2-root.crt
*/
const char *sf_class2_root = R"(-----BEGIN CERTIFICATE-----
MIIEDzCCAvegAwIBAgIBADANBgkqhkiG9w0BAQUFADBoMQswCQYDVQQGEwJVUzEl
MCMGA1UEChMcU3RhcmZpZWxkIFRlY2hub2xvZ2llcywgSW5jLjEyMDAGA1UECxMp
U3RhcmZpZWxkIENsYXNzIDIgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMDQw
NjI5MTczOTE2WhcNMzQwNjI5MTczOTE2WjBoMQswCQYDVQQGEwJVUzElMCMGA1UE
ChMcU3RhcmZpZWxkIFRlY2hub2xvZ2llcywgSW5jLjEyMDAGA1UECxMpU3RhcmZp
ZWxkIENsYXNzIDIgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwggEgMA0GCSqGSIb3
DQEBAQUAA4IBDQAwggEIAoIBAQC3Msj+6XGmBIWtDBFk385N78gDGIc/oav7PKaf
8MOh2tTYbitTkPskpD6E8J7oX+zlJ0T1KKY/e97gKvDIr1MvnsoFAZMej2YcOadN
+lq2cwQlZut3f+dZxkqZJRRU6ybH838Z1TBwj6+wRir/resp7defqgSHo9T5iaU0
X9tDkYI22WY8sbi5gv2cOj4QyDvvBmVmepsZGD3/cVE8MC5fvj13c7JdBmzDI1aa
K4UmkhynArPkPw2vCHmCuDY96pzTNbO8acr1zJ3o/WSNF4Azbl5KXZnJHoe0nRrA
1W4TNSNe35tfPe/W93bC6j67eA0cQmdrBNj41tpvi/JEoAGrAgEDo4HFMIHCMB0G
A1UdDgQWBBS/X7fRzt0fhvRbVazc1xDCDqmI5zCBkgYDVR0jBIGKMIGHgBS/X7fR
zt0fhvRbVazc1xDCDqmI56FspGowaDELMAkGA1UEBhMCVVMxJTAjBgNVBAoTHFN0
YXJmaWVsZCBUZWNobm9sb2dpZXMsIEluYy4xMjAwBgNVBAsTKVN0YXJmaWVsZCBD
bGFzcyAyIENlcnRpZmljYXRpb24gQXV0aG9yaXR5ggEAMAwGA1UdEwQFMAMBAf8w
DQYJKoZIhvcNAQEFBQADggEBAAWdP4id0ckaVaGsafPzWdqbAYcaT1epoXkJKtv3
L7IezMdeatiDh6GX70k1PncGQVhiv45YuApnP+yz3SFmH8lU+nLMPUxA2IGvd56D
eruix/U0F47ZEUD0/CwqTRV/p2JdLiXTAAsgGh1o+Re49L2L7ShZ3U0WixeDyLJl
xy16paq8U4Zt3VekyvggQQto8PT7dL5WXXp59fkdheMtlb71cZBDzI0fmgAKhynp
VSJYACPq4xJDKVtHCN2MQWplBqjlIapBtJUhlbl90TSrE9atvNziPTnNvT51cKEY
WQPJIrSPnNVeKtelttQKbfi3QBFGmh95DmK/D5fs4C8fF5Q=
-----END CERTIFICATE-----
)";

/*
  Download files from AWS. thing.cer.pem ,thing.private.key 
*/
const char *sim7080G_cert_pem =R"(-----BEGIN CERTIFICATE-----
****************************************************************
****************************************************************
****************************************************************
-----END CERTIFICATE-----
)";

const char *sim7080G_private_key = R"(-----BEGIN RSA PRIVATE KEY-----
****************************************************************
****************************************************************
****************************************************************
-----END RSA PRIVATE KEY-----
)";


enum {
    MODEM_CATM = 1,
    MODEM_NB_IOT,
    MODEM_CATM_NBIOT,
};

// Your GPRS credentials, if any
const char apn[] = "povo.jp";//your apn
const char gprsUser[] = "";
const char gprsPass[] = "";

// cayenne server address and port
const char server[]   = "********-ats.iot.ap-northeast-1.amazonaws.com";//your endpoint
const int  port       = 8883;
const char topic[] = "SIM7080G/test";
char buffer[1024] = {0};
char clientID[] = "SIM7080G";

const int sleep_sec = 60;


bool isConnect()
{
    modem.sendAT("+SMSTATE?");
    if (modem.waitResponse("+SMSTATE: ")) {
        String res =  modem.stream.readStringUntil('\r');
        return res.toInt();
    }
    return false;
}

void modem_reset()
{
#ifdef     BOARD_MODEM_PWR_PIN
    pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
    digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
    delay(100);
    digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
    delay(1000);
    digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
#endif
     
}

void writeCaFiles(int index, const char *filename, const char *data,
                  size_t lenght)
{
    modem.sendAT("+CFSTERM");
    modem.waitResponse();


    modem.sendAT("+CFSINIT");
    if (modem.waitResponse() != 1) {
        Serial.println("INITFS FAILED");
        return;
    }
    // AT+CFSWFILE=<index>,<filename>,<mode>,<filesize>,<input time>
    // <index>
    //      Directory of AP filesystem:
    //      0 "/custapp/" 1 "/fota/" 2 "/datatx/" 3 "/customer/"
    // <mode>
    //      0 If the file already existed, write the data at the beginning of the
    //      file. 1 If the file already existed, add the data at the end o
    // <file size>
    //      File size should be less than 10240 bytes. <input time> Millisecond,
    //      should send file during this period or you can’t send file when
    //      timeout. The value should be less
    // <input time> Millisecond, should send file during this period or you can’t
    // send file when timeout. The value should be less than 10000 ms.

    size_t payloadLenght = lenght;
    size_t totalSize     = payloadLenght;
    size_t alardyWrite   = 0;

    while (totalSize > 0) {
        size_t writeSize = totalSize > 10000 ? 10000 : totalSize;

        modem.sendAT("+CFSWFILE=", index, ",", "\"", filename, "\"", ",",
                     !(totalSize == payloadLenght), ",", writeSize, ",", 10000);
        modem.waitResponse(30000UL, "DOWNLOAD");
REWRITE:
        modem.stream.write(data + alardyWrite, writeSize);
        if (modem.waitResponse(30000UL) == 1) {
            alardyWrite += writeSize;
            totalSize -= writeSize;
            Serial.printf("Writing:%d overage:%d\n", writeSize, totalSize);
        } else {
            Serial.println("Write failed!");
            delay(1000);
            goto REWRITE;
        }
    }

    Serial.println("Wirte done!!!");

    modem.sendAT("+CFSTERM");
    if (modem.waitResponse() != 1) {
        Serial.println("CFSTERM FAILED");
        return;
    }
}

void setup()
{



    /*********************************
     * step 1 : start modem
    ***********************************/
#ifdef ESP32
    M5.begin();
    delay(200);
    Serial.println("Wait...");
    SerialAT.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);
#elif defined(RASPBERRYPI_PICO)
    Serial.begin(115200);
    delay(200);
    Serial.println("Wait...");
    SerialAT.setRX(BOARD_MODEM_RXD_PIN);
    SerialAT.setTX(BOARD_MODEM_TXD_PIN);
    SerialAT.setFIFOSize(128);
    SerialAT.begin(115200);
    
#endif
    delay(1000);
    Serial.println("Initializing modem...");
    modem_reset();
    int retry = 0;
    while (!modem.testAT()) {
        Serial.print(".");
        if (retry++ > 6) {
            modem_reset();
            modem.restart();
            retry = 0;
            Serial.println("Retry start modem .");
        }
    }
    Serial.println();
    Serial.print("Modem started!");

    /*********************************
     * step 2 : Check if the SIM card is inserted
    ***********************************/
    String result ;


    if (modem.getSimStatus() != SIM_READY) {
        Serial.println("SIM Card is not insert!!!");
        return ;
    }


    /*********************************
     * step 3 : Set the network mode to MODEM_CATM
    ***********************************/

    modem.setNetworkMode(38);    //lte only

    modem.setPreferredMode(MODEM_CATM);

    uint8_t pre = modem.getPreferredMode();

    uint8_t mode = modem.getNetworkMode();

    Serial.printf("getNetworkMode:%u getPreferredMode:%u\n", mode, pre);


    /*********************************
    * step 4 : Wait for the network registration to succeed
    ***********************************/
    RegStatus s;
    do {
        s = modem.getRegistrationStatus();
        if (s != REG_OK_HOME && s != REG_OK_ROAMING) {
            Serial.print(".");
            delay(1000);
        }

    } while (s != REG_OK_HOME && s != REG_OK_ROAMING) ;

    Serial.println();
    Serial.print("Network register info:");
    Serial.println(register_info[s]);

    // Activate network bearer, APN can not be configured by default,
    // if the SIM card is locked, please configure the correct APN and user password, use the gprsConnect() method
    modem.gprsConnect(apn, gprsUser, gprsPass);
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

        // Enable Local Time Stamp for getting network time
    modem.sendAT(GF("+CLTS=1"));
    if (modem.waitResponse(10000L) != 1) {
        Serial.println("Enable Local Time Stamp Failed!");
        return;
    }

    // Serial.println("Watiing time sync.");
    // while (modem.waitResponse("*PSUTTZ:") != 1) {
    //     Serial.print(".");
    // }
    // Serial.println();

    // Before connecting, you need to confirm that the time has been synchronized.
    modem.sendAT("+CCLK?");
    modem.waitResponse(30000);


    /*********************************
     * step 5 : import  ca
     ***********************************/
    writeCaFiles(3, "aws-class2-root.crt", sf_class2_root, strlen(sf_class2_root));
    writeCaFiles(3, "myclient.crt", sim7080G_cert_pem, strlen(sim7080G_cert_pem));
    writeCaFiles(3, "myclient.key", sim7080G_private_key, strlen(sim7080G_private_key));
    /*********************************
     * step 6 : setup MQTT Client
     ***********************************/

    // // If it is already connected, disconnect it first
    modem.sendAT("+SMDISC");
    modem.waitResponse();

    modem.sendAT("+SMCONF=\"URL\",", server, ",", port);
    if (modem.waitResponse() != 1) {
        return;
    }

    modem.sendAT("+SMCONF=\"KEEPTIME\",180");
    if (modem.waitResponse() != 1) {}

    modem.sendAT("+SMCONF=\"CLEANSS\",1");
    if (modem.waitResponse() != 1) {}

    modem.sendAT("+SMCONF=\"CLIENTID\",", clientID);
    if (modem.waitResponse() != 1) {
        return;
    }
    modem.sendAT("+SMCONF?");
    if (modem.waitResponse(1000) != 1) {
        return;
    }

    // AT+CSSLCFG="SSLVERSION",<ctxindex>,<sslversion>
    modem.sendAT("+CSSLCFG=\"SSLVERSION\",0,3");
    modem.waitResponse();

    // <ssltype>
    //      1 QAPI_NET_SSL_CERTIFICATE_E
    //      2 QAPI_NET_SSL_CA_LIST_E
    //      3 QAPI_NET_SSL_PSK_TABLE_E
    // AT+CSSLCFG="CONVERT",2,"server-ca.crt"
    modem.sendAT("+CSSLCFG=\"CONVERT\",1,\"myclient.crt\",\"myclient.key\"");
    if (modem.waitResponse() != 1) {
        Serial.println("Convert myclient.crt failed!");
    }
    modem.sendAT("+CSSLCFG=\"CONVERT\",2,\"aws-class2-root.crt\"");
    if (modem.waitResponse() != 1) {
        Serial.println("Convert aws-class2-root.crt failed!");
    }

    /*
    Defined Values
    <index> SSL status, range: 0-6
            0 Not support SSL
            1-6 Corresponding to AT+CSSLCFG command parameter <ctindex>
            range 0-5
    <ca list> CA_LIST file name, Max length is 20 bytes
    <cert name> CERT_NAME file name, Max length is 20 bytes
    <len_calist> Integer type. Maximum length of parameter <ca list>.
    <len_certname> Integer type. Maximum length of parameter <cert name>.
    */
    modem.sendAT("+SMSSL=1,\"aws-class2-root.crt\",\"myclient.crt\"");
    if (modem.waitResponse() != 1) {
        Serial.println("Convert ca failed!");
    }


    modem.sendAT("+SMCONN");
    res = modem.waitResponse(30000);
    if (!res) {
        Serial.println("Connect failed, retry connect ...");
        delay(1000);
        return;
    }


    Serial.println("MQTT Client connected!");
}
void loop()
{
    if (!isConnect()) {
        Serial.println("MQTT Client disconnect!"); delay(1000);
    }

    Serial.println();
    // Publish fake temperature data
    float value = 123.45;
    char txt[] = "\"hello world!\"" ;
    String payload;
    payload.concat("{\"text\":");
    payload.concat(txt);
    payload.concat(",");
    payload.concat("\"value\":");
    payload.concat(value);
    payload.concat("}");
    // AT+SMPUB=<topic>,<content length>,<qos>,<retain><CR>message is enteredQuit edit mode if messagelength equals to <contentlength>
    // snprintf(buffer, 1024, "+SMPUB=\"v1/%s/things/%s/data/%d\",%d,1,1", username, clientID, data_channel, payload.length());
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
    Serial.println("modem.poweroff");
    modem.sendAT("+CPOWD=1");
    Serial.println("deepsleep");
#ifdef ESP32
    esp_deep_sleep(sleep_sec * 1000000ULL);
#elif defined(RASPBERRYPI_PICO)
    set_sys_clock_khz(10000, false); // Set System clock to 10000 kHz
    delay(2);
    vreg_set_voltage(VREG_VOLTAGE_0_95);
    delay(sleep_sec * 1000UL); 
    watchdog_reboot(0,0,0);
#endif
}
