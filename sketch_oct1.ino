#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <U8g2lib.h>
#include <math.h>

#define QMC5883L_ADDR 0x0D

// Registers
#define REG_X_LSB     0x00
#define REG_STATUS    0x06
#define REG_TEMP_LSB  0x07
#define REG_CONTROL1  0x09
#define REG_CONTROL2  0x0A
#define REG_SETRESET  0x0B

// Control Register 1
#define MODE_CONTINUOUS 0x01

#define ODR_10HZ   0x00
#define ODR_50HZ   0x04
#define ODR_100HZ  0x08
#define ODR_200HZ  0x0C

#define RNG_2G     0x00
#define RNG_8G     0x10

#define OSR_512    0x00
#define OSR_256    0x40
#define OSR_128    0x80
#define OSR_64     0xC0

//--------------------------------------------------
// OLED
//--------------------------------------------------``````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````

U8G2_SSD1327_WS_128X128_F_4W_HW_SPI oled(
    U8G2_R0,
    10,
    9,
    8
);

//--------------------------------------------------``````````````````````````````````````````````````````````````````````````````````````````````````````````````
// WiFi
//--------------------------------------------------

const char* ssid = "hotspot2";
const char* password = "";

const char* serverIP = "10.42.0.1";

const int sensorID = 1;

//--------------------------------------------------
// Sensor variables
//--------------------------------------------------

int16_t rawX = 0;
int16_t rawY = 0;
int16_t rawZ = 0;

float x = 0;
float y = 0;
float z = 0;
float b = 0;

//--------------------------------------------------
// Calibration
//--------------------------------------------------

// Hard Iron Offsets
float X_OFFSET = 6767;
float Y_OFFSET = 11792;
float Z_OFFSET = -67;

// Soft Iron Scale
float X_SCALE = 1.0;
float Y_SCALE = 1.0;
float Z_SCALE = 1.0;

// Datasheet:
// ±2G = 12000 LSB/G
// 1G = 100nT

const float nT_PER_LSB = 100000.0 / 12000.0;

//--------------------------------------------------
// WiFi status
//--------------------------------------------------

bool wifiConnected = false;

//--------------------------------------------------
// Buffers
//--------------------------------------------------

char buf[40];


//--------------------------------------------------
// I2C Functions
//--------------------------------------------------

void writeRegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(QMC5883L_ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

uint8_t readRegister(uint8_t reg)
{
    Wire.beginTransmission(QMC5883L_ADDR);
    Wire.write(reg);

    if (Wire.endTransmission(false) != 0)
        return 0;

    Wire.requestFrom(QMC5883L_ADDR, (uint8_t)1);

    if (Wire.available())
        return Wire.read();

    return 0;
}

//--------------------------------------------------
// Detect Sensor
//--------------------------------------------------

bool detectSensor()
{
    Wire.beginTransmission(QMC5883L_ADDR);

    if (Wire.endTransmission() == 0)
        return true;

    return false;
}

//--------------------------------------------------
// Initialize QMC5883L
//--------------------------------------------------

void initQMC5883L()
{
    // Soft Reset
    writeRegister(REG_CONTROL2, 0x80);

    delay(20);

    // Recommended SET/RESET period
    writeRegister(REG_SETRESET, 0x01);

    delay(5);

    /*
        Control Register 1

        OSR = 512
        Range = ±2G
        ODR = 10Hz
        Continuous Mode

        Binary:
        00000001
    */

    writeRegister(
        REG_CONTROL1,
        OSR_512 |
        RNG_2G |
        ODR_50HZ |
        MODE_CONTINUOUS
    );

    delay(50);
}

//--------------------------------------------------
// Read XYZ Registers
//--------------------------------------------------

bool readRawXYZ()
{
    uint8_t status = readRegister(REG_STATUS);

    // Data not ready
    if (!(status & 0x01))
        return false;

    // Overflow
    if (status & 0x02)
        return false;

    Wire.beginTransmission(QMC5883L_ADDR);
    Wire.write(REG_X_LSB);

    if (Wire.endTransmission(false) != 0)
        return false;

    Wire.requestFrom(QMC5883L_ADDR, (uint8_t)6);

    if (Wire.available() < 6)
        return false;

    uint8_t xl = Wire.read();
    uint8_t xh = Wire.read();

    uint8_t yl = Wire.read();
    uint8_t yh = Wire.read();

    uint8_t zl = Wire.read();
    uint8_t zh = Wire.read();

    rawX = (int16_t)((xh << 8) | xl);
    rawY = (int16_t)((yh << 8) | yl);
    rawZ = (int16_t)((zh << 8) | zl);

    return true;
}

//--------------------------------------------------
// Convert to nT
//--------------------------------------------------

void calculateField()
{
    x = ((rawX * nT_PER_LSB) - X_OFFSET) * X_SCALE;

    y = ((rawY * nT_PER_LSB) - Y_OFFSET) * Y_SCALE;

    z = ((rawZ * nT_PER_LSB) - Z_OFFSET) * Z_SCALE;

    b = sqrt(
            x * x +
            y * y +
            z * z
        );
}




//--------------------------------------------------
// Connect WiFi
//--------------------------------------------------

void connectWiFi()
{
    oled.clearBuffer();
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(0,15,"Connecting WiFi...");
    oled.sendBuffer();

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    uint8_t retry = 0;

    while (WiFi.status() != WL_CONNECTED && retry < 40)
    {
        delay(500);

        oled.clearBuffer();

        oled.drawStr(0,15,"Connecting WiFi...");

        sprintf(buf,"Attempt : %d",retry+1);
        oled.drawStr(0,35,buf);

        oled.sendBuffer();

        retry++;
    }

    if(WiFi.status()==WL_CONNECTED)
    {
        wifiConnected=true;

        oled.clearBuffer();

        oled.drawStr(0,20,"WiFi Connected");

        sprintf(buf,"%s",WiFi.localIP().toString().c_str());
        oled.drawStr(0,45,buf);

        oled.sendBuffer();

        Serial.println("WiFi Connected");
        Serial.println(WiFi.localIP());

        delay(1000);
    }
    else
    {
        wifiConnected=false;

        oled.clearBuffer();
        oled.drawStr(0,20,"WiFi Failed");
        oled.sendBuffer();

        Serial.println("WiFi Failed");

        delay(1000);
    }
}


//--------------------------------------------------
// Check WiFi
//--------------------------------------------------

void checkWiFi()
{
    if(WiFi.status()==WL_CONNECTED)
    {
        wifiConnected=true;
        return;
    }

    wifiConnected=false;

    WiFi.disconnect(true);

    delay(500);

    WiFi.begin(ssid,password);

    uint8_t retry=0;

    while(WiFi.status()!=WL_CONNECTED && retry<20)
    {
        delay(500);
        retry++;
    }

    wifiConnected=(WiFi.status()==WL_CONNECTED);
}



//--------------------------------------------------
// Send Data to Raspberry Pi
//--------------------------------------------------

void sendToServer()
{
    if(!wifiConnected)
        return;

    HTTPClient http;

    String url =
    "http://" +
    String(serverIP) +
    ":5000/data?sensor=" +
    String(sensorID) +
    "&x=" + String(x,5) +
    "&y=" + String(y,5) +
    "&z=" + String(z,5) +
    "&b=" + String(b,5);

    http.begin(url);

    int response = http.GET();

    Serial.print("HTTP : ");
    Serial.println(response);

    http.end();
}









//--------------------------------------------------
// OLED Display
//--------------------------------------------------

void updateOLED()
{
    oled.clearBuffer();

    oled.setFont(u8g2_font_6x10_tf);

    oled.drawStr(0,10,"HMC5883L Magnetometer");

    if(wifiConnected)
        oled.drawStr(0,25,"WiFi : Connected");
    else
        oled.drawStr(0,25,"WiFi : Disconnected");

    sprintf(buf,"X : %.2f nT",x);
    oled.drawStr(0,45,buf);

    sprintf(buf,"Y : %.2f nT",y);
    oled.drawStr(0,62,buf);

    sprintf(buf,"Z : %.2f nT",z);
    oled.drawStr(0,79,buf);

    sprintf(buf,"B : %.2f nT",b);
    oled.drawStr(0,100,buf);

    oled.sendBuffer();
}


//--------------------------------------------------
// OLED Error Messages
//--------------------------------------------------

void sensorNotFoundScreen()
{
    oled.clearBuffer();

    oled.setFont(u8g2_font_6x10_tf);

    oled.drawStr(10,45,"HMC5883L NOT");
    oled.drawStr(10,65,"DETECTED");

    oled.sendBuffer();
}

void sensorOverflowScreen()
{
    oled.clearBuffer();

    oled.setFont(u8g2_font_6x10_tf);

    oled.drawStr(10,45,"Sensor");
    oled.drawStr(10,65,"Overflow");

    oled.sendBuffer();
}









//--------------------------------------------------
// SETUP
//--------------------------------------------------

void setup()
{
    Serial.begin(115200);

    delay(1000);

    //--------------------------------------------------
    // I2C
    //--------------------------------------------------

    Wire.begin(4,5);

    Wire.setClock(400000);

    //--------------------------------------------------
    // OLED
    //--------------------------------------------------

    oled.begin();

    oled.setFont(u8g2_font_6x10_tf);

    oled.clearBuffer();
    oled.drawStr(0,20,"Starting...");
    oled.sendBuffer();

    delay(500);

    //--------------------------------------------------
    // Detect Sensor
    //--------------------------------------------------

    if(!detectSensor())
    {
        Serial.println("HMC5883L NOT FOUND");

        while(1)
        {
            sensorNotFoundScreen();
            delay(1000);
        }
    }

    Serial.println("HMC5883L Detected");

    //--------------------------------------------------
    // Initialize Sensor
    //--------------------------------------------------

    initQMC5883L();

    delay(100);

    //--------------------------------------------------
    // WiFi
    //--------------------------------------------------

    connectWiFi();

    //--------------------------------------------------
    // Ready Screen
    //--------------------------------------------------

    oled.clearBuffer();

    oled.drawStr(0,20,"System Ready");

    oled.sendBuffer();

    delay(1000);
}









//--------------------------------------------------
// LOOP
//--------------------------------------------------

void loop()
{
    //--------------------------------------------------
    // Check WiFi
    //--------------------------------------------------

    checkWiFi();

    //--------------------------------------------------
    // Read Sensor
    //--------------------------------------------------

    if(readRawXYZ())
    {
        calculateField();

        updateOLED();

        sendToServer();

        Serial.print("X : ");
        Serial.print(x,5);

        Serial.print("  Y : ");
        Serial.print(y,5);

        Serial.print("  Z : ");
        Serial.print(z,5);

        Serial.print("  B : ");
        Serial.println(b,5);
    }
    else
    {
        uint8_t status = readRegister(REG_STATUS);

        if(status & 0x02)
        {
            sensorOverflowScreen();
            Serial.println("Sensor Overflow");
        }
        else
        {
            oled.clearBuffer();

            oled.setFont(u8g2_font_6x10_tf);

            oled.drawStr(0,40,"Waiting for");
            oled.drawStr(0,60,"New Data");

            oled.sendBuffer();
        }
    }

    delay(1);
}