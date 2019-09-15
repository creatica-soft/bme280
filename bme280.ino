//BME280 - 3.3V Barometric pressure, temperature and humidity sensor
//Has I2C and SPI interfaces (4- or 3-wire SPI intefaces are supported). 3-wire uses SDI for both input and output (must write "1" to spi3w_en register), SDO is not used (not connected)
//This sketch uses I2C (ports 20 - SDA and 21 - SCL on Arduino Mega)

#include <Wire.h>
//to enable SPI and disable i2c, CSB (chip select) -> GND
#define BME280_I2C_ADDR_PRIM 0x76 //
#define BME280_I2C_ADDR_SEC 0x77 //to change primary addr to secondary, SDO -> GND

#define BME280_CHIP_ID 0x60

//name Register Address
#define BME280_CHIP_ID_ADDR 0xD0
#define BME280_RESET_ADDR 0xE0
#define BME280_TEMP_PRESS_CALIB_DATA_ADDR 0x88
#define BME280_HUMIDITY_CALIB_DATA_ADDR 0xE1
#define BME280_PWR_CTRL_ADDR 0xF4
#define BME280_STATUS_ADDR 0xF3
#define BME280_CTRL_HUM_ADDR 0xF2
#define BME280_CTRL_MEAS_ADDR 0xF4
#define BME280_CONFIG_ADDR 0xF5
#define BME280_DATA_ADDR 0xF7

#define BME280_TEMP_PRESS_CALIB_DATA_LEN 26
#define BME280_HUMIDITY_CALIB_DATA_LEN 7
#define BME280_P_T_H_DATA_LEN 8

#define BME280_SENSOR_MODE_MSK 3
#define BME280_SENSOR_MODE_POS 0
#define BME280_CTRL_HUM_MSK 7
#define BME280_CTRL_HUM_POS 0
#define BME280_CTRL_PRESS_MSK 0x1C
#define BME280_CTRL_PRESS_POS 2
#define BME280_CTRL_TEMP_MSK 0xE0
#define BME280_CTRL_TEMP_POS 5
#define BME280_FILTER_MSK 0x1C
#define BME280_FILTER_POS 2
#define BME280_STANDBY_MSK 0xE0
#define BME280_STANDBY_POS 5

#define BME280_OSR_PRESS_SEL 1
#define BME280_OSR_TEMP_SEL 2
#define BME280_OSR_HUM_SEL 4
#define BME280_FILTER_SEL 8
#define BME280_STANDBY_SEL 16
#define BME280_ALL_SETTINGS_SEL 0x1F

#define BME280_NO_OVERSAMPLING 0x00
#define BME280_OVERSAMPLING_1X 0x01
#define BME280_OVERSAMPLING_2X 0x02
#define BME280_OVERSAMPLING_4X 0x03
#define BME280_OVERSAMPLING_8X 0x04
#define BME280_OVERSAMPLING_16X 0x05

#define BME280_FILTER_COEFF_OFF 0x00
#define BME280_FILTER_COEFF_2 0x01
#define BME280_FILTER_COEFF_4 0x02
#define BME280_FILTER_COEFF_8 0x03
#define BME280_FILTER_COEFF_16 0x04

#define BME280_STANDBY_TIME_0_5_MS 0
#define BME280_STANDBY_TIME_62_5_MS 1
#define BME280_STANDBY_TIME_125_MS 2
#define BME280_STANDBY_TIME_250_MS 3
#define BME280_STANDBY_TIME_500_MS 4
#define BME280_STANDBY_TIME_1000_MS 5
#define BME280_STANDBY_TIME_10_MS 6
#define BME280_STANDBY_TIME_20_MS 7

#define BME280_SLEEP_MODE 0x00
#define BME280_FORCED_MODE 0x01
#define BME280_NORMAL_MODE 0x03

#define BME280_PRESS 1
#define BME280_TEMP 2
#define BME280_HUM 4
#define BME280_ALL 0x07

#define OVERSAMPLING_SETTINGS 7
#define FILTER_STANDBY_SETTINGS 0x18

struct Settings
{
  uint8_t osr_p; //pressure oversampling
  uint8_t osr_t; //temperature oversampling
  uint8_t osr_h; //humidity oversampling
  uint8_t filter; //filter coefficient
  uint8_t standby_time; //standby time
} settings;

struct Data
{
  uint32_t p; //pressure
  int32_t t; //temperature
  uint32_t h; //humidity
};
struct UncompData
{
  uint32_t p; //pressure
  uint32_t t; //temperature
  uint32_t h; //humidity
};
struct CalibData
{
  uint16_t dig_T1;
  int16_t dig_T2;
  int16_t dig_T3;
  uint16_t dig_P1;
  int16_t dig_P2;
  int16_t dig_P3;
  int16_t dig_P4;
  int16_t dig_P5;
  int16_t dig_P6;
  int16_t dig_P7;
  int16_t dig_P8;
  int16_t dig_P9;
  uint8_t dig_H1;
  int16_t dig_H2;
  uint8_t dig_H3;
  int16_t dig_H4;
  int16_t dig_H5;
  int8_t dig_H6;
  int32_t t_fine;
} calibData;

uint8_t readRegister(uint8_t dev, uint8_t addr, uint8_t * buf, uint8_t len) {
  Wire.beginTransmission(dev);
  Wire.write(addr);
  Wire.endTransmission();
  Wire.requestFrom(dev, len);
  uint8_t i = 0;
  while (Wire.available()) {
    buf[i++] = Wire.read();
    if (i >= len) break;
  }
  return i;
}

uint8_t writeRegister(uint8_t dev, uint8_t addr, uint8_t val) {
  Wire.beginTransmission(dev);
  Wire.write(addr);
  Wire.write(val);
  return Wire.endTransmission();
}

uint8_t getChipId() {
  uint8_t chipId = 0;
  char s[64];
  readRegister(BME280_I2C_ADDR_PRIM, BME280_CHIP_ID_ADDR, &chipId, 1);
  if( chipId != BME280_CHIP_ID) {
      sprintf(s, String(F("getChipId error: wrong id %#X, expected BME280_CHIP_ID")).c_str(), chipId); 
      Serial.println(s);
  }
  return chipId;
}

void setHumiditySettings() {
  uint8_t ctrl_meas, err;
  uint8_t ctrl_hum = settings.osr_h & BME280_CTRL_HUM_MSK;
  if ((err = writeRegister(BME280_I2C_ADDR_PRIM, BME280_CTRL_HUM_ADDR, ctrl_hum)) != 0) {
    Serial.println(String(F("setHumiditySettings() writeRegister error")) + err);
    return;
  }
  //must write to ctrl_meas register to activate humidity settings
  if (readRegister(BME280_I2C_ADDR_PRIM, BME280_CTRL_MEAS_ADDR, &ctrl_meas, 1) != 1) {
    Serial.println(F("setHumiditySettings() readRegister error"));
    return;       
  }
  if ((err = writeRegister(BME280_I2C_ADDR_PRIM, BME280_CTRL_MEAS_ADDR, ctrl_meas)) != 0)
    Serial.println(String(F("setHumiditySettings() writeRegister2 error")) + err);
}

void setPressTempSettings(uint8_t sets) {
  uint8_t regData, err;
  if (readRegister(BME280_I2C_ADDR_PRIM, BME280_CTRL_MEAS_ADDR, &regData, 1) != 1) {
    Serial.println(F("setPressTempSettings() readRegister error"));
    return;    
  }
  if (sets & BME280_OSR_PRESS_SEL)
    regData = (regData & (~BME280_CTRL_PRESS_MSK)) | ((settings.osr_p << BME280_CTRL_PRESS_POS) & BME280_CTRL_PRESS_MSK);
  if (sets & BME280_OSR_TEMP_SEL)
    regData = (regData & (~BME280_CTRL_TEMP_MSK)) | ((settings.osr_t << BME280_CTRL_TEMP_POS) & BME280_CTRL_TEMP_MSK); 
  if ((err = writeRegister(BME280_I2C_ADDR_PRIM, BME280_CTRL_MEAS_ADDR, regData)) != 0)
    Serial.println(String(F("setPressTempSettings() writeRegister error")) + err);
}

uint8_t getMode() {
  uint8_t mode;
  if (readRegister(BME280_I2C_ADDR_PRIM, BME280_PWR_CTRL_ADDR, &mode, 1) != 1) {
    Serial.println(F("getMode() readRegister error"));
    return 4; //not such mode     
  }
  mode &= BME280_SENSOR_MODE_MSK;
  return mode;
}

void setSettings(uint8_t sets) {
  uint8_t regData[4];
  struct Settings s;
  if (getMode() != BME280_SLEEP_MODE) {
    //put dev to sleep - it is entered the sleep mode by default after power on reset
    if (readRegister(BME280_I2C_ADDR_PRIM, BME280_CTRL_HUM_ADDR, (uint8_t *)(&regData), 4) != 4) {
      Serial.println(F("setSettings() readRegister error"));
      return;           
    }
    parseSettings(regData, &s);
    softReset();
    reloadSettings(&s);
  }
  if (sets & BME280_OSR_HUM_SEL) setHumiditySettings();
  if (sets & (BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL)) setPressTempSettings(sets);
  if (sets & (BME280_FILTER_SEL | BME280_STANDBY_SEL)) setFilterStandbySettings(sets);
}

void setFilterStandbySettings(uint8_t sets) {
  uint8_t regData, err;
  if (readRegister(BME280_I2C_ADDR_PRIM, BME280_CONFIG_ADDR, &regData, 1) != 1) {
      Serial.println(F("setFilterStandbySettings() readRegister error"));
      return;               
  }
  if (sets & BME280_FILTER_SEL) 
    regData = (regData & (~BME280_FILTER_MSK)) | ((settings.filter << BME280_FILTER_POS) & BME280_FILTER_MSK);
  if (sets & BME280_STANDBY_SEL)
    regData = (regData & (~BME280_STANDBY_MSK)) | ((settings.standby_time << BME280_STANDBY_POS) & BME280_STANDBY_MSK);
  if ((err = writeRegister(BME280_I2C_ADDR_PRIM, BME280_CONFIG_ADDR, regData)) != 0)
    Serial.println(String(F("setFilterStandbySettings() writeRegister error")) + err);
}

void parseSettings(const uint8_t * regData, struct Settings * sets) {
  sets->osr_h = regData[0] & BME280_CTRL_HUM_MSK;
  sets->osr_p = (regData[2] & BME280_CTRL_PRESS_MSK) >> BME280_CTRL_PRESS_POS;
  sets->osr_t = (regData[2] & BME280_CTRL_TEMP_MSK) >> BME280_CTRL_TEMP_POS;
  sets->filter = (regData[3] & BME280_FILTER_MSK) >> BME280_FILTER_POS;
  sets->standby_time = (regData[3] & BME280_STANDBY_MSK) >> BME280_STANDBY_POS; 
}

void softReset() {
  uint8_t err;
  Serial.println(F("Resetting..."));
  if ((err = writeRegister(BME280_I2C_ADDR_PRIM, BME280_RESET_ADDR, 0xB6)) != 0)
    Serial.println(String(F("softReset() writeRegister error")) + err);
  delay(2);
}

void reloadSettings(const struct Settings * sets) {
  setSettings(BME280_ALL_SETTINGS_SEL);
  uint8_t regData = 0, err;
  if (readRegister(BME280_I2C_ADDR_PRIM, BME280_CONFIG_ADDR, &regData, 1) != 1) {
    Serial.println(F("reloadSettings() readRegister error"));
    return;
  }
  regData = (regData & (~BME280_FILTER_MSK)) | ((sets->filter << BME280_FILTER_POS) & BME280_FILTER_MSK);
  regData = (regData & (~BME280_STANDBY_MSK)) | ((sets->standby_time << BME280_STANDBY_POS) & BME280_STANDBY_MSK);
  if ((err = writeRegister(BME280_I2C_ADDR_PRIM, BME280_CONFIG_ADDR, regData)) != 0)
    Serial.println(String(F("reloadSettings() writeRegister error")) + err);    
}

//set PowerMode
void setMode(uint8_t mode) {
  uint8_t m;
  if (readRegister(BME280_I2C_ADDR_PRIM, BME280_PWR_CTRL_ADDR, &m, 1) != 1) {
    Serial.println(F("setMode() readRegister error"));
    return;
  }
  m = m & BME280_SENSOR_MODE_MSK;
  if (m != BME280_SLEEP_MODE) {
    //put dev to sleep
    uint8_t regData[4];
    struct Settings sets;
    if (readRegister(BME280_I2C_ADDR_PRIM, BME280_CTRL_HUM_ADDR, (uint8_t *)(&regData), 4) != 4) {
      Serial.println(F("setMode() readRegister2 error"));
      return;          
    }
    parseSettings(regData, &sets);
    softReset();
    reloadSettings(&sets);
  }
  uint8_t regData, err;
  if (readRegister(BME280_I2C_ADDR_PRIM, BME280_PWR_CTRL_ADDR, (uint8_t *)&regData, 1) != 1) {
    Serial.println(F("setMode() readRegister3 error"));
    return;    
  }
  regData = (regData & (~BME280_SENSOR_MODE_MSK)) | (mode & BME280_SENSOR_MODE_MSK);
  if ((err = writeRegister(BME280_I2C_ADDR_PRIM, BME280_PWR_CTRL_ADDR, regData)) != 0)
     Serial.println(String(F("setMode() writeRegister error")) + err); 
}

void parseData(uint8_t * regData, struct UncompData * data) {
  data->p = (((uint32_t)regData[0]) << 12) | (((uint32_t)regData[1]) << 4) | (((uint32_t)regData[2]) >> 4);
  data->t = (((uint32_t)regData[3]) << 12) | (((uint32_t)regData[4]) << 4) | (((uint32_t)regData[5]) >> 4);
  data->h = (((uint32_t)regData[6]) << 8) | ((uint32_t)regData[7]);
  
  /*char s[32];
  sprintf(s, String(F("T = %ld, H = %lu, P = %lu")).c_str(), (int32_t)(data->t), data->h, data->p);
  Serial.println(s);*/
}

int32_t compensateT(const struct UncompData * data, struct CalibData * calibData) {
  int32_t t = 0, var1, var2, t_min = -4000, t_max = 8500;
  var1 = ((((data->t >> 3) - ((int32_t)calibData->dig_T1 << 1))) * ((int32_t)calibData->dig_T2)) >> 11;
  var2 = (((((data->t >> 4) - ((int32_t)calibData->dig_T1)) * ((data->t >> 4) - ((int32_t)calibData->dig_T1))) >> 12) * ((int32_t)calibData->dig_T3)) >> 14;
  calibData->t_fine = var1 + var2;
  t = (calibData->t_fine * 5 + 128) >> 8;
  if (t < t_min) t = t_min;
  else if (t > t_max) t = t_max;
  
  /*char s[64];
  sprintf(s, String(F("var1 = %ld, var2 = %ld, t = %ld")).c_str(), var1, var2, t);
  Serial.println(s);*/
  
  return t; // t x 10^2 deg C
}

uint32_t compensateH(const struct UncompData * data, const struct CalibData * calibData) {
  int32_t h;

  h = calibData->t_fine - ((int32_t)76800L);
  h = (((((data->h << 14) - (((int32_t)calibData->dig_H4) << 20) - (((int32_t)calibData->dig_H5) * h)) + ((int32_t)16384L)) >> 15) * (((((((h * ((int32_t)calibData->dig_H6)) >> 10) * (((h * ((int32_t)calibData->dig_H3)) >> 11) + ((int32_t)32768L))) >> 10) + ((int32_t)2097152L)) * ((int32_t)calibData->dig_H2) + 8192) >> 14));
  h = (h - (((((h >> 15) * (h >> 15)) >> 7) * ((int32_t)calibData->dig_H1)) >> 4));
  h = h < 0 ? 0 : h;
  h = h > 419430400L ? 419430400L : h;

  /*char s[32];
  sprintf(s, String(F("var1 = %ld")).c_str(), var1);
  Serial.println(s);*/
 
  return (uint32_t)(h >> 12); //in Q22.10 format (22 integer and 10 fractional bits) %RH = %RF / 1024.0
}
uint32_t compensateP(const struct UncompData * data, const struct CalibData * calibData) {
  int64_t var1, var2, p;
  
  var1 = ((int64_t)calibData->t_fine) - 128000L;
  var2 = var1 * var1 * (int64_t)calibData->dig_P6;
  var2 = var2 + ((var1 * (int64_t)calibData->dig_P5) << 17);
  var2 = var2 + (((int64_t)calibData->dig_P4) << 35);
  var1 = ((var1 * var1 * (int64_t)calibData->dig_P3) >> 8) + ((var1 * (int64_t)calibData->dig_P2) << 12);
  var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calibData->dig_P1) >> 33;

  if (var1 == 0) return 0;
  p = 1048576L - (int32_t)(data->p);
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)calibData->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)calibData->dig_P8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + (((int64_t)calibData->dig_P7) << 4);

  return (uint32_t)p; // in Q24.8 format (24 integer and 8 fractional bits) in Pa; p = p / 256 Pa
}

void compensateData(uint8_t dataType, const struct UncompData * uncompData, struct Data * data, struct CalibData * calibData) {
  if (dataType & (BME280_PRESS | BME280_TEMP | BME280_HUM)) {
    data->t = compensateT(uncompData, calibData);
  }
  if (dataType & BME280_PRESS) {
    data->p = compensateP(uncompData, calibData);
  }
  if (dataType & BME280_HUM) {
    data->h = compensateH(uncompData, calibData);
  }
}

void getData(uint8_t dataType, struct Data * data) {
  uint8_t regData[BME280_P_T_H_DATA_LEN] = { 0 };
  struct UncompData uncompData = { 0, 0, 0 };
  if (readRegister(BME280_I2C_ADDR_PRIM, BME280_DATA_ADDR, (uint8_t *)&regData, BME280_P_T_H_DATA_LEN) != BME280_P_T_H_DATA_LEN) {
    Serial.println(F("getData() readRegister error"));
    return;       
  }
  parseData((uint8_t *)&regData, &uncompData);
  compensateData(dataType, &uncompData, data, &calibData);
}

void parseTempPresCalibData(uint8_t * data, struct CalibData * calibData) {
  calibData->dig_T1 = ((uint16_t)data[1] << 8) | (uint16_t)data[0];
  calibData->dig_T2 = ((int16_t)data[3] << 8) | (int16_t)data[2];
  calibData->dig_T3 = ((int16_t)data[5] << 8) | (uint16_t)data[4];
  calibData->dig_P1 = ((uint16_t)data[7] << 8) | (uint16_t)data[6];
  calibData->dig_P2 = ((int16_t)data[9] << 8) | (uint16_t)data[8];
  calibData->dig_P3 = ((int16_t)data[11] << 8) | (uint16_t)data[10];
  calibData->dig_P4 = ((int16_t)data[13] << 8) | (uint16_t)data[12];
  calibData->dig_P5 = ((int16_t)data[15] << 8) | (uint16_t)data[14];
  calibData->dig_P6 = ((int16_t)data[17] << 8) | (uint16_t)data[16];
  calibData->dig_P7 = ((int16_t)data[19] << 8) | (uint16_t)data[18];
  calibData->dig_P8 = ((int16_t)data[21] << 8) | (uint16_t)data[20];
  calibData->dig_P9 = ((int16_t)data[23] << 8) | (uint16_t)data[22];
  calibData->dig_H1 = data[25];
  
  /*char s[128];
  sprintf(s, String(F("t1 = %u, t2 = %d, t3 = %d")).c_str(), calibData->dig_T1, calibData->dig_T2, calibData->dig_T3);
  Serial.println(s);
  sprintf(s, String(F("p1 = %u, p2 = %d, p3 = %d, p4 = %d, p5 = %d, p6 = %d, p7 = %d, p8 = %d, p9 = %d")).c_str(), calibData->dig_P1, calibData->dig_P2, calibData->dig_P3, calibData->dig_P4, calibData->dig_P5, calibData->dig_P6, calibData->dig_P7, calibData->dig_P8, calibData->dig_P9);
  Serial.println(s);*/
}

void parseHumidCalibData(uint8_t * data, struct CalibData * calibData) {
  calibData->dig_H2 = ((int16_t)data[1] << 8) | (int16_t)data[0];
  calibData->dig_H3 = data[2];
  calibData->dig_H4 = (((int16_t)((int8_t)data[3])) << 4) | (((int16_t)data[4]) & 0xF);
  calibData->dig_H5 = (((int16_t)((int8_t)data[5])) << 4) | ((int16_t)(data[4] >> 4) & 0xF);
  calibData->dig_H6 = (int8_t)data[6];
  
  /*char s[64];
  sprintf(s, String(F("h1 = %u, h2 = %d, h3 = %u, h4 = %d, h5 = %d, h6 = %d")).c_str(), calibData->dig_H1, calibData->dig_H2, calibData->dig_H3, calibData->dig_H4, calibData->dig_H5, calibData->dig_H6);
  Serial.println(s);*/
}

void getCalibData(struct CalibData * calibData) {
  uint8_t cData[BME280_TEMP_PRESS_CALIB_DATA_LEN];
  memset((uint8_t *)cData, 0, BME280_TEMP_PRESS_CALIB_DATA_LEN);
  if (readRegister(BME280_I2C_ADDR_PRIM, BME280_TEMP_PRESS_CALIB_DATA_ADDR, (uint8_t *)&cData, BME280_TEMP_PRESS_CALIB_DATA_LEN) != BME280_TEMP_PRESS_CALIB_DATA_LEN) {
    Serial.println(F("getCalibData() readRegister error"));
    return;       
  }
  parseTempPresCalibData(cData, calibData);
  memset((uint8_t *)cData, 0, BME280_TEMP_PRESS_CALIB_DATA_LEN);
  if (readRegister(BME280_I2C_ADDR_PRIM, BME280_HUMIDITY_CALIB_DATA_ADDR, (uint8_t *)&cData, BME280_HUMIDITY_CALIB_DATA_LEN) != BME280_HUMIDITY_CALIB_DATA_LEN) {
    Serial.println(F("getCalibData() readRegister2 error"));
    return;       
  }
  parseHumidCalibData(cData, calibData);
}

void printData(struct Data * data) {
  float t, p, h;
  char s[64];
  uint8_t deg[3] = { 0xc2, 0xb0 }; //unicode degree symbol
  t = data->t / 100.0;
  p = data->p / 256.0;
  h = data->h / 1024.0;

  sprintf(s, String(F("T = %.1f%.2sC, H = %.1f%%, P = %.1fmb(hPa) (%.1fmm Hg)")).c_str(), t, (char *)(&deg), h, p / 100, p * 0.0075006157584566);
  Serial.println(s);
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println(F("ready"));
  Wire.begin();
  //Just to verify that we talk to the right device
  getChipId();
  softReset();
  getCalibData(&calibData);
  
  //oversampling reduces noise and increases the resolution if filter is off. 
  //Mostly the pressure is affected by enviromnental fluctuation and hence requires oversampling the most. Humidity is affected the least.
  //if both oversampling and filter are off, the resolution is 16 bit. 
  //if filter is on, the temperature and pressure resolution are 20 bits and humidity is 16 bit.
  //if filter is off, the resolution increases by 1 bit for each oversampling step (1, 2, 4, 8, 16) minus 1:
  //for example, with 1X oversampling, it is still 16 bits, with 2X - it is 17 bits and with 16X - it is 20.
  settings.osr_h = BME280_OVERSAMPLING_16X;
  settings.osr_t = BME280_OVERSAMPLING_16X;
  settings.osr_p = BME280_OVERSAMPLING_16X;

  //IIR low pass filter effectively reduces the bandwidth of temperature and pressure output signals and increases the resolution of those signals to 20 bits
  //IIR formula: data = (old_data * (filter_coeff - 1) + new_data) / filter_coeff
  //the higher the coefficient, the slower the sensor response as it takes more samples
  //with coeff = 2, it takes 8 samples to fully measure the environmental change; with coeff = 4 - 18 , with coeff = 8, more than 32, a and with coeff = 16, even much more samples
  settings.filter = BME280_FILTER_COEFF_16;
  settings.standby_time = BME280_STANDBY_TIME_10_MS;
  uint8_t settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL | BME280_FILTER_SEL | BME280_STANDBY_SEL;
  setSettings(settings_sel);
  //there are three modes: SLEEP, FORCED and NORMAL
  //in SLEEP_MODE all registers are accessible but no measurements are done; hence, the power consumption is minimum
  //in FORCED_MODE a single measurement is done in accordance with the selected measurements and filter options, then the sensor enters the SLEEP_MODE
  //for the next measurement the FORCED_MODE needs to be selected again
  //in NORMAL_MODE the sensor cycling between active and standby periods. The standby_time can be selected between 0.5 and 1000ms
  //in NORMAL_MODE data is always accessible without the need for further write accesses
  //NORMAL_MODE is recommended when using IIR filter to filter short-term environmental disturbances
  setMode(BME280_NORMAL_MODE);
}

void loop() {
  delay(10000);
  struct Data data;
  data.h = 0; data.p = 0; data.t = 0;
  getData(BME280_ALL, &data);
  printData(&data);
}
