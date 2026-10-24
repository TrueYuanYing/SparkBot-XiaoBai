#pragma once
#include <driver/i2c_master.h>
#include "bmi270.h"

struct HubImuReading { float x=0, y=0, z=0, gx=0, gy=0, gz=0; };
class HubImu {
    bmi2_dev sensor_{};
    i2c_master_dev_handle_t handle_ = nullptr;
    bool ready_ = false;
    uint8_t chip_id_=0;
    bool InitBmi220();
public:
    bool Init(i2c_master_bus_handle_t bus);
    bool Read(HubImuReading& reading);
    bool Ready() const { return ready_; }
    const char* Name() const { return chip_id_==0x26?"BMI220":chip_id_==0x24?"BMI270":"unknown"; }
};
