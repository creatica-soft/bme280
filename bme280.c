//BME280 - 3.3V Barometric pressure, temperature and humidity sensor
//Has I2C and SPI interfaces (4- or 3-wire SPI intefaces are supported).
//3-wire uses SDI for both input and output (must write "1" to spi3w_en register)
//SDO is not used (not connected)
//gcc -O3 -o bme280 bme280.cpp -li2c

#define I2C_DEV_RETRIES 3
#define I2C_TIMEOUT 100 //in 10ms intervals
#define DEFAULT_SAMPLING_RATE_SEC 1
#define DEFAULT_NUMBER_OF_SAMPLES 1

#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
//#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>

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

void setSettings(int fd, uint8_t sets);

int readRegister(int fd, uint16_t addr, uint8_t * buf, uint16_t len) {
	int i;
	for (i = 0; i < len; i++)
		buf[i] = i2c_smbus_read_byte_data(fd, addr + i);
	return i;		
}

int writeRegister(int fd, uint16_t addr, uint8_t val) {
	return i2c_smbus_write_byte_data(fd, addr, val);
}

uint8_t getChipId(int fd) {
	uint8_t chipId = 0;
	readRegister(fd, BME280_CHIP_ID_ADDR, &chipId, 1);
	if (chipId != BME280_CHIP_ID) {
		printf("getChipId error: wrong id %#hhX, expected %#hhX\n", chipId, BME280_CHIP_ID);
	}
	return chipId;
}

void setHumiditySettings(int fd) {
	uint8_t ctrl_meas, err;
	uint8_t ctrl_hum = settings.osr_h & BME280_CTRL_HUM_MSK;
	if ((err = writeRegister(fd, BME280_CTRL_HUM_ADDR, ctrl_hum)) != 0) {
		printf("setHumiditySettings() writeRegister error %hhd\n", err);
		return;
	}
	//must write to ctrl_meas register to activate humidity settings
	if (readRegister(fd, BME280_CTRL_MEAS_ADDR, &ctrl_meas, 1) != 1) {
		printf("setHumiditySettings() readRegister error\n");
		return;
	}
	if ((err = writeRegister(fd, BME280_CTRL_MEAS_ADDR, ctrl_meas)) != 0)
		printf("setHumiditySettings() writeRegister2 error %hhd\n", err);
}

void setPressTempSettings(int fd, uint8_t sets) {
	uint8_t regData, err;
	if (readRegister(fd, BME280_CTRL_MEAS_ADDR, &regData, 1) != 1) {
		printf("setPressTempSettings() readRegister error\n");
		return;
	}
	if (sets & BME280_OSR_PRESS_SEL)
		regData = (regData & (~BME280_CTRL_PRESS_MSK)) | ((settings.osr_p << BME280_CTRL_PRESS_POS) & BME280_CTRL_PRESS_MSK);
	if (sets & BME280_OSR_TEMP_SEL)
		regData = (regData & (~BME280_CTRL_TEMP_MSK)) | ((settings.osr_t << BME280_CTRL_TEMP_POS) & BME280_CTRL_TEMP_MSK);
	if ((err = writeRegister(fd, BME280_CTRL_MEAS_ADDR, regData)) != 0)
		printf("setPressTempSettings() writeRegister error %hhd\n", err);
}

uint8_t getMode(int fd) {
	uint8_t mode;
	if (readRegister(fd, BME280_PWR_CTRL_ADDR, &mode, 1) != 1) {
		printf("getMode() readRegister error\n");
		return 4; //not such mode     
	}
	mode &= BME280_SENSOR_MODE_MSK;
	return mode;
}

void parseSettings(const uint8_t* regData, struct Settings* sets) {
	sets->osr_h = regData[0] & BME280_CTRL_HUM_MSK;
	sets->osr_p = (regData[2] & BME280_CTRL_PRESS_MSK) >> BME280_CTRL_PRESS_POS;
	sets->osr_t = (regData[2] & BME280_CTRL_TEMP_MSK) >> BME280_CTRL_TEMP_POS;
	sets->filter = (regData[3] & BME280_FILTER_MSK) >> BME280_FILTER_POS;
	sets->standby_time = (regData[3] & BME280_STANDBY_MSK) >> BME280_STANDBY_POS;
}

void softReset(int fd) {
	uint8_t err;
	//printf("Resetting...\n");
	if ((err = writeRegister(fd, BME280_RESET_ADDR, 0xB6)) != 0)
		printf("softReset() writeRegister error %hhd\n", err);
	struct timespec req;
	//req.tv_sec = 0; req.tv_nsec = 2000000;
	//nanosleep(&req, NULL);
	sleep(1);
}

void reloadSettings(int fd, const struct Settings* sets) {
	setSettings(fd, BME280_ALL_SETTINGS_SEL);
	uint8_t regData = 0, err;
	if (readRegister(fd, BME280_CONFIG_ADDR, &regData, 1) != 1) {
		printf("reloadSettings() readRegister error\n");
		return;
	}
	regData = (regData & (~BME280_FILTER_MSK)) | ((sets->filter << BME280_FILTER_POS) & BME280_FILTER_MSK);
	regData = (regData & (~BME280_STANDBY_MSK)) | ((sets->standby_time << BME280_STANDBY_POS) & BME280_STANDBY_MSK);
	if ((err = writeRegister(fd, BME280_CONFIG_ADDR, regData)) != 0)
		printf("reloadSettings() writeRegister error %hhd\n", err);
}

void setFilterStandbySettings(int fd, uint8_t sets) {
	uint8_t regData, err;
	if (readRegister(fd, BME280_CONFIG_ADDR, &regData, 1) != 1) {
		printf("setFilterStandbySettings() readRegister error\n");
		return;
	}
	if (sets & BME280_FILTER_SEL)
		regData = (regData & (~BME280_FILTER_MSK)) | ((settings.filter << BME280_FILTER_POS) & BME280_FILTER_MSK);
	if (sets & BME280_STANDBY_SEL)
		regData = (regData & (~BME280_STANDBY_MSK)) | ((settings.standby_time << BME280_STANDBY_POS) & BME280_STANDBY_MSK);
	if ((err = writeRegister(fd, BME280_CONFIG_ADDR, regData)) != 0)
		printf("setFilterStandbySettings() writeRegister error %hhd\n", err);
}

void setSettings(int fd, uint8_t sets) {
	uint8_t regData[4];
	struct Settings s;
	if (getMode(fd) != BME280_SLEEP_MODE) {
		//put dev to sleep - it is entered the sleep mode by default after power on reset
		if (readRegister(fd, BME280_CTRL_HUM_ADDR, (uint8_t *)(&regData), 4) != 4) {
			printf("setSettings() readRegister error\n");
			return;
		}
		parseSettings(regData, &s);
		softReset(fd);
		reloadSettings(fd, &s);
	}
	if (sets & BME280_OSR_HUM_SEL) setHumiditySettings(fd);
	if (sets & (BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL)) setPressTempSettings(fd, sets);
	if (sets & (BME280_FILTER_SEL | BME280_STANDBY_SEL)) setFilterStandbySettings(fd, sets);
}

//set PowerMode
void setMode(int fd, uint8_t mode) {
	uint8_t m;
	if (readRegister(fd, BME280_PWR_CTRL_ADDR, &m, 1) != 1) {
		printf("setMode() read BME280_PWR_CTRL_ADDR register error");
		return;
	}
	m = m & BME280_SENSOR_MODE_MSK;
	if (m != BME280_SLEEP_MODE) {
		//put dev to sleep
		uint8_t regData[4];
		struct Settings sets;
		if (readRegister(fd, BME280_CTRL_HUM_ADDR, (uint8_t *)(&regData), 4) != 4) {
			printf("setMode() read BME280_CTRL_HUM_ADDR register error");
			return;
		}
		parseSettings(regData, &sets);
		softReset(fd);
		reloadSettings(fd, &sets);
	}
	uint8_t regData, err;
	if (readRegister(fd, BME280_PWR_CTRL_ADDR, (uint8_t *)&regData, 1) != 1) {
		printf("setMode() read BME280_PWR_CTRL_ADDR register error");
		return;
	}
	regData = (regData & (~BME280_SENSOR_MODE_MSK)) | (mode & BME280_SENSOR_MODE_MSK);
	if ((err = writeRegister(fd, BME280_PWR_CTRL_ADDR, regData)) != 0)
		printf("setMode() write BME280_PWR_CTRL_ADDR register error %hhd", err);
}

void parseData(uint8_t * regData, struct UncompData * data) {
	data->p = ((uint32_t)regData[0] << 12) | ((uint32_t)regData[1] << 4) | ((uint32_t)regData[2] >> 4);
	data->t = ((uint32_t)regData[3] << 12) | ((uint32_t)regData[4] << 4) | ((uint32_t)regData[5] >> 4);
	data->h = ((uint32_t)regData[6] << 8) | (uint32_t)regData[7];

	//printf("T = %ld, H = %lu, P = %lu\n", (int32_t)(data->t), data->h, data->p);
}

int32_t compensateT(const struct UncompData * data, struct CalibData * calibData) {
	int32_t t = 0, var1, var2, t_min = -4000, t_max = 8500;
	var1 = ((((data->t >> 3) - ((int32_t)calibData->dig_T1 << 1))) * ((int32_t)calibData->dig_T2)) >> 11;
	var2 = (((((data->t >> 4) - ((int32_t)calibData->dig_T1)) * ((data->t >> 4) - ((int32_t)calibData->dig_T1))) >> 12) * ((int32_t)calibData->dig_T3)) >> 14;
	calibData->t_fine = var1 + var2;
	t = (calibData->t_fine * 5 + 128) >> 8;
	if (t < t_min) t = t_min;
	else if (t > t_max) t = t_max;

	//printf("compensateT: var1 = %ld, var2 = %ld, t = %ld\n", var1, var2, t);

	return t; // t x 10^2 deg C
}

uint32_t compensateH(const struct UncompData * data, const struct CalibData * calibData) {
	int32_t h;

	h = calibData->t_fine - ((int32_t)76800L);
	h = (((((data->h << 14) - (((int32_t)calibData->dig_H4) << 20) - (((int32_t)calibData->dig_H5) * h)) + ((int32_t)16384L)) >> 15) * (((((((h * ((int32_t)calibData->dig_H6)) >> 10) * (((h * ((int32_t)calibData->dig_H3)) >> 11) + ((int32_t)32768L))) >> 10) + ((int32_t)2097152L)) * ((int32_t)calibData->dig_H2) + 8192) >> 14));
	h = (h - (((((h >> 15) * (h >> 15)) >> 7) * ((int32_t)calibData->dig_H1)) >> 4));
	h = h < 0 ? 0 : h;
	h = h > 419430400L ? 419430400L : h;

	//printf("compensateH: h = %ld\n", h);

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

	//printf("compensateP: var1 = %lld, var2 = %lld, p = %ld\n", var1, var2, (uint32_t)p);

	return (uint32_t)p; // in Q24.8 format (24 integer and 8 fractional bits) in Pa; p = p / 256 Pa
}

void compensateData(uint8_t dataType, const struct UncompData * uncompData, struct Data * data, struct CalibData * calibData) {
	if (dataType & (BME280_PRESS | BME280_TEMP | BME280_HUM)) data->t = compensateT(uncompData, calibData);
	if (dataType & BME280_PRESS) data->p = compensateP(uncompData, calibData);
	if (dataType & BME280_HUM) data->h = compensateH(uncompData, calibData);
}

void getData(int fd, uint8_t dataType, struct Data * data) {
	uint8_t regData[BME280_P_T_H_DATA_LEN] = { 0 };
	struct UncompData uncompData = { 0, 0, 0 };
	if (readRegister(fd, BME280_DATA_ADDR, (uint8_t *)&regData, BME280_P_T_H_DATA_LEN) != BME280_P_T_H_DATA_LEN) {
		printf("getData() readRegister error\n");
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

	//printf("t1 = %u, t2 = %d, t3 = %d\n", calibData->dig_T1, calibData->dig_T2, calibData->dig_T3);
	//printf("p1 = %u, p2 = %d, p3 = %d, p4 = %d, p5 = %d, p6 = %d, p7 = %d, p8 = %d, p9 = %d\n", calibData->dig_P1, calibData->dig_P2, calibData->dig_P3, calibData->dig_P4, calibData->dig_P5, calibData->dig_P6, calibData->dig_P7, calibData->dig_P8, calibData->dig_P9);
}

void parseHumidCalibData(uint8_t * data, struct CalibData * calibData) {
	calibData->dig_H2 = ((int16_t)data[1] << 8) | (int16_t)data[0];
	calibData->dig_H3 = data[2];
	calibData->dig_H4 = (((int16_t)((int8_t)data[3])) << 4) | (((int16_t)data[4]) & 0xF);
	calibData->dig_H5 = (((int16_t)((int8_t)data[5])) << 4) | ((int16_t)(data[4] >> 4) & 0xF);
	calibData->dig_H6 = (int8_t)data[6];

	//printf("h1 = %u, h2 = %d, h3 = %u, h4 = %d, h5 = %d, h6 = %d\n", calibData->dig_H1, calibData->dig_H2, calibData->dig_H3, calibData->dig_H4, calibData->dig_H5, calibData->dig_H6);
}

void getCalibData(int fd, struct CalibData * calibData) {
	uint8_t cData[BME280_TEMP_PRESS_CALIB_DATA_LEN];
	memset((uint8_t *)cData, 0, BME280_TEMP_PRESS_CALIB_DATA_LEN);
	if (readRegister(fd, BME280_TEMP_PRESS_CALIB_DATA_ADDR, (uint8_t *)&cData, BME280_TEMP_PRESS_CALIB_DATA_LEN) != BME280_TEMP_PRESS_CALIB_DATA_LEN) {
		printf("getCalibData() readRegister error\n");
		return;
	}
	parseTempPresCalibData(cData, calibData);
	memset((uint8_t *)cData, 0, BME280_TEMP_PRESS_CALIB_DATA_LEN);
	if (readRegister(fd, BME280_HUMIDITY_CALIB_DATA_ADDR, (uint8_t *)&cData, BME280_HUMIDITY_CALIB_DATA_LEN) != BME280_HUMIDITY_CALIB_DATA_LEN) {
		printf("getCalibData() readRegister2 error\n");
		return;
	}
	parseHumidCalibData(cData, calibData);
}

void printData(struct Data * data, bool raw) {
	double t, p, h;
	uint8_t deg[3] = { 0xc2, 0xb0, 0 }; //unicode degree symbol
	t = data->t / 100.0;
	p = data->p / 256.0;
	h = data->h / 1024.0;

	if (raw)
		printf("t=%.1f&h=%.1f&p=%.1f\n", t, h, p / 100);
	else 
		printf("T = %.1f%sC, H = %.1f%%, P = %.1fmb(hPa) (%.1fmm Hg)\n", t, (char *)(&deg), h, p / 100, p * 0.0075006157584566);
}

void setup(int fd) {
	//Just to verify that we talk to the right device
	getChipId(fd);
	softReset(fd);
	getCalibData(fd, &calibData);

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
	setSettings(fd, settings_sel);
	//there are three modes: SLEEP, FORCED and NORMAL
	//in SLEEP_MODE all registers are accessible but no measurements are done; hence, the power consumption is minimum
	//in FORCED_MODE a single measurement is done in accordance with the selected measurements and filter options, then the sensor enters the SLEEP_MODE
	//for the next measurement the FORCED_MODE needs to be selected again
	//in NORMAL_MODE the sensor cycling between active and standby periods. The standby_time can be selected between 0.5 and 1000ms
	//in NORMAL_MODE data is always accessible without the need for further write accesses
	//NORMAL_MODE is recommended when using IIR filter to filter short-term environmental disturbances
	setMode(fd, BME280_NORMAL_MODE);
}

void loop(int fd, int sampling_rate_sec, bool raw) {
	sleep(sampling_rate_sec);
	struct Data data;
	data.h = 0; data.p = 0; data.t = 0;
	getData(fd, BME280_ALL, &data);
	printData(&data, raw);
}

void i2c_funcs(int fd) {
	int res;
	ulong funcs;
	bool i2c, addr10bit, prot_mangling, smbus_pec,
		nostart, slave, block_proc_call, quick,
		read_byte, write_byte,
		read_byte_data, write_byte_data,
		read_word_data, write_word_data, proc_call,
		read_block_data, write_block_data,
		read_i2c_block, write_i2c_block, host_notify;

	res = ioctl(fd, I2C_FUNCS, &funcs);
	if (res < 0) {
		perror("getting i2c functionality mask failed");
		return;
	}

	i2c = I2C_FUNC_I2C & funcs ? true : false;
	addr10bit = I2C_FUNC_10BIT_ADDR & funcs ? true : false;
	prot_mangling = I2C_FUNC_PROTOCOL_MANGLING & funcs ? true : false;
	smbus_pec = I2C_FUNC_SMBUS_PEC & funcs ? true : false;
	nostart = I2C_FUNC_NOSTART & funcs ? true : false;
	slave = I2C_FUNC_SLAVE & funcs ? true : false;
	block_proc_call = I2C_FUNC_SMBUS_BLOCK_PROC_CALL & funcs ? true : false;
	quick = I2C_FUNC_SMBUS_QUICK & funcs ? true : false;
	read_byte = I2C_FUNC_SMBUS_READ_BYTE & funcs ? true : false;
	write_byte = I2C_FUNC_SMBUS_WRITE_BYTE & funcs ? true : false;
	read_byte_data = I2C_FUNC_SMBUS_READ_BYTE_DATA & funcs ? true : false;
	write_byte_data = I2C_FUNC_SMBUS_WRITE_BYTE_DATA & funcs ? true : false;
	read_word_data = I2C_FUNC_SMBUS_READ_WORD_DATA & funcs ? true : false;
	write_word_data = I2C_FUNC_SMBUS_WRITE_WORD_DATA & funcs ? true : false;
	proc_call = I2C_FUNC_SMBUS_PROC_CALL & funcs ? true : false;
	read_block_data = I2C_FUNC_SMBUS_READ_BLOCK_DATA & funcs ?  true : false;
	write_block_data = I2C_FUNC_SMBUS_WRITE_BLOCK_DATA & funcs ?  true : false;
	read_i2c_block = I2C_FUNC_SMBUS_READ_I2C_BLOCK & funcs ?  true : false;
	write_i2c_block = I2C_FUNC_SMBUS_WRITE_I2C_BLOCK & funcs ? true : false;
	host_notify = I2C_FUNC_SMBUS_HOST_NOTIFY & funcs ? true : false;
	printf("i2c adapter functionality mask %d:\n i2c %d\n 10bit_addr %d\n protocol_mangling %d\n smbus_pec %d\n nostart %d\n slave %d\n block_proc_call %d\n smbus_quick %d\n smbus_read_byte %d\n smbus_write_byte %d\n smbus_read_byte_data %d\n smbus_write_byte_data %d\n smbus_read_word_data %d\n smbus_write_word_data %d\n smbus_proc_call %d\n smbus_read_block_data %d\n smbus_write_block_data %d\n smbus_read_i2c_block %d\n smbus_write_i2c_block %d\n smbus_host_notify %d\n",
		res, i2c, addr10bit, prot_mangling, smbus_pec, nostart, slave, block_proc_call,
		quick, read_byte, write_byte, read_byte_data, write_byte_data,
		read_word_data, write_word_data, proc_call, read_block_data, write_block_data,
		read_i2c_block, write_i2c_block, host_notify);
}

int main(int argc, char ** argv) {
	int res, counter = 0;

	if (argc > 5 || argc < 2) {
		printf("Usage: bme280 <i2c-dev> [sampling_rate_sec] [number_of_samples] [--raw]\n");
		return -1;
	}
	char device[16];

	int len = strlen(argv[1]);
	int sampling_rate_sec = DEFAULT_SAMPLING_RATE_SEC;
	if (argc >= 3) {
		if (!isdigit(argv[2][0])) {
			printf("sampling_rate_sec %s is not a number\n", argv[2]);
			return -1;
		}
		sampling_rate_sec = atoi(argv[2]);
	}
	int number_of_samples = DEFAULT_NUMBER_OF_SAMPLES;
	if (argc >= 4) {
		if (!isdigit(argv[3][0])) {
			printf("number_of_samples %s is not a number\n", argv[2]);
			return -1;
		}
		number_of_samples = atoi(argv[3]);
	}
	bool raw = false;
	if (argc == 5) {
		if (strncmp(argv[4], "--raw", 5) == 0) raw = true;
	}
	if (len >= 16) {
		printf("error: i2c-dev string \'%s\' is too long, must be less than 16 chars\n", argv[1]);
		return -1;
	}
	strncpy(device, argv[1], strlen(argv[1]) + 1);

	int fd = open(device, O_RDWR);
	//printf("fd %d\n", fd);
	if (fd < 0) {
		perror("Unable to open i2c device");
		return -1;
	}
	res = ioctl(fd, I2C_RETRIES, I2C_DEV_RETRIES);
	if (res < 0) {
		perror("setting number of retries failed");
		return -1;
	}
	res = ioctl(fd, I2C_TIMEOUT, I2C_TIMEOUT);
	if (res < 0) {
		perror("setting timeout failed");
		return -1;
	}
	res = ioctl(fd, I2C_SLAVE, BME280_I2C_ADDR_PRIM);
	if (res < 0) {
		perror("setting slave address failed");
		return -1;
	}
	
	//i2c_funcs(fd);

	setup(fd);
	while (counter++ < number_of_samples)	loop(fd, sampling_rate_sec, raw);
	close(fd);
}
