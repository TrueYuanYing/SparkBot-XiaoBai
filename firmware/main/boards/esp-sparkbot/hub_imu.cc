#include "hub_imu.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_rom_sys.h>
#include <esp_log.h>
#include <cstring>
#include <initializer_list>
#include "bmi220/accelgyro_bmi220_config_tbin.h"

bool HubImu::Init(i2c_master_bus_handle_t bus) {
    int address = 0;
    for (int candidate : {0x68, 0x69}) {
        if (i2c_master_probe(bus, candidate, 40) != ESP_OK) continue;
        i2c_device_config_t cfg{}; cfg.device_address=candidate; cfg.scl_speed_hz=400000;
        if (i2c_master_bus_add_device(bus, &cfg, &handle_) != ESP_OK) continue;
        uint8_t reg=0, id=0;
        auto result=i2c_master_transmit_receive(handle_,&reg,1,&id,1,100);
        ESP_LOGI("HubImu","Probe address=0x%x reg0=0x%x result=%s",candidate,id,esp_err_to_name(result));
        if(result==ESP_OK && (id==BMI270_CHIP_ID || id==0x26)) { address=candidate;chip_id_=id;break; }
        reg=0x75;id=0;
        result=i2c_master_transmit_receive(handle_,&reg,1,&id,1,100);
        ESP_LOGI("HubImu","Probe address=0x%x reg75=0x%x result=%s",candidate,id,esp_err_to_name(result));
        i2c_master_bus_rm_device(handle_);handle_=nullptr;
    }
    if (!address) { ESP_LOGW("HubImu", "BMI270 not detected"); return false; }
    if(chip_id_==0x26) {
        ready_=InitBmi220();
        ESP_LOGI("HubImu","BMI220 ready=%d address=0x%x",ready_,address);
        return ready_;
    }
    sensor_.intf = BMI2_I2C_INTF; sensor_.intf_ptr=handle_; sensor_.read_write_len=32;
    sensor_.read = [](uint8_t reg, uint8_t* data, uint32_t len, void* context) -> int8_t {
        return i2c_master_transmit_receive(static_cast<i2c_master_dev_handle_t>(context), &reg, 1, data, len, 100) == ESP_OK ? 0 : -1;
    };
    sensor_.write = [](uint8_t reg, const uint8_t* data, uint32_t len, void* context) -> int8_t {
        if (len>32) return -1;
        uint8_t payload[33]; payload[0]=reg; memcpy(payload+1,data,len);
        return i2c_master_transmit(static_cast<i2c_master_dev_handle_t>(context),payload,len+1,100)==ESP_OK ? 0 : -1;
    };
    sensor_.delay_us = [](uint32_t period, void*) {
        if (period >= 10000) vTaskDelay(pdMS_TO_TICKS((period+999)/1000));
        else esp_rom_delay_us(period);
    };
    int result = bmi270_init(&sensor_);
    if (result != BMI2_OK) { ESP_LOGE("HubImu", "BMI270 init: %d",result); return false; }
    bmi2_sens_config settings[2]{};
    settings[0].type=BMI2_ACCEL; settings[1].type=BMI2_GYRO;
    if (bmi2_get_sensor_config(settings,2,&sensor_) != BMI2_OK) return false;
    settings[0].cfg.acc.odr=BMI2_ACC_ODR_100HZ;
    settings[0].cfg.acc.range=BMI2_ACC_RANGE_4G;
    settings[0].cfg.acc.bwp=BMI2_ACC_NORMAL_AVG4;
    settings[0].cfg.acc.filter_perf=BMI2_PERF_OPT_MODE;
    settings[1].cfg.gyr.odr=BMI2_GYR_ODR_100HZ;
    settings[1].cfg.gyr.range=BMI2_GYR_RANGE_2000;
    if (bmi2_set_sensor_config(settings,2,&sensor_) != BMI2_OK) return false;
    uint8_t sensors[]{BMI2_ACCEL,BMI2_GYRO};
    ready_=bmi2_sensor_enable(sensors,2,&sensor_)==BMI2_OK;
    ESP_LOGI("HubImu", "BMI270 ready=%d address=0x%x",ready_,address);
    return ready_;
}
bool HubImu::Read(HubImuReading& out) {
    if (!ready_) return false;
    if(chip_id_==0x26) {
        uint8_t reg=0x0c,data[12]{};
        if(i2c_master_transmit_receive(handle_,&reg,1,data,sizeof(data),50)!=ESP_OK)return false;
        auto value=[&](int offset){return static_cast<int16_t>(data[offset]|(data[offset+1]<<8));};
        out={value(0)/8192.f,value(2)/8192.f,value(4)/8192.f,
             value(6)/16.384f,value(8)/16.384f,value(10)/16.384f};
        return true;
    }
    bmi2_sens_data data{};
    if (bmi2_get_sensor_data(&data,&sensor_)!=BMI2_OK) return false;
    out={data.acc.x/8192.f,data.acc.y/8192.f,data.acc.z/8192.f,
         data.gyr.x/16.384f,data.gyr.y/16.384f,data.gyr.z/16.384f};
    return true;
}

bool HubImu::InitBmi220() {
    // BMI220 sequence and Bosch firmware from Chromium EC's BMI220/BMI260 driver.
    // Do not feed this chip the BMI270 image: it returns INTERNAL_STATUS=drv_err.
    auto write=[&](uint8_t reg,uint8_t value) {
        uint8_t data[]{reg,value};
        return i2c_master_transmit(handle_,data,sizeof(data),100)==ESP_OK;
    };
    auto read=[&](uint8_t reg,uint8_t& value) {
        return i2c_master_transmit_receive(handle_,&reg,1,&value,1,100)==ESP_OK;
    };
    if(!write(0x7e,0xb6))return false;
    vTaskDelay(pdMS_TO_TICKS(10));
    if(!write(0x7c,2))return false;
    vTaskDelay(pdMS_TO_TICKS(10));
    if(!write(0x59,0))return false;
    for(unsigned offset=0;offset<g_bmi220_config_tbin_len;offset+=32) {
        uint8_t address[]{0x5b,static_cast<uint8_t>((offset/2)&15),static_cast<uint8_t>((offset/2)>>4)};
        uint8_t payload[33];payload[0]=0x5e;memcpy(payload+1,g_bmi220_config_tbin+offset,32);
        if(i2c_master_transmit(handle_,address,sizeof(address),100)!=ESP_OK ||
           i2c_master_transmit(handle_,payload,sizeof(payload),100)!=ESP_OK)return false;
    }
    if(!write(0x59,1))return false;
    uint8_t status=0;
    for(int i=0;i<20;++i) {
        vTaskDelay(pdMS_TO_TICKS(10));
        if(!read(0x21,status))return false;
        if((status&15)==1)break;
    }
    ESP_LOGI("HubImu","BMI220 internal status=0x%x",status);
    if((status&15)!=1)return false;
    // Performance mode, 100 Hz, ±4 g and ±2000 degrees/s.
    if(!write(0x40,0xa8)||!write(0x41,1)||!write(0x42,0xe8)||!write(0x43,0)||!write(0x7d,0x0e))return false;
    vTaskDelay(pdMS_TO_TICKS(80));
    uint8_t range=0,power=0;
    if(!read(0x41,range)||!read(0x7d,power)||!read(0x03,status))return false;
    ESP_LOGI("HubImu","BMI220 range=%u power=0x%x data-ready=0x%x",range,power,status);
    return (range&3)==1 && (power&6)==6 && (status&0x80)!=0;
}
