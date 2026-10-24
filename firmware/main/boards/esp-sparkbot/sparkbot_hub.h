#pragma once
#include "hub_core.h"
#include "hub_camera.h"
#include "hub_imu.h"
#include "hub_touch.h"
#include "hub_voyage.h"
#include "hub_dock.h"
#include <atomic>
#include <mutex>
#include <string>
#include <cJSON.h>

enum class HubApp { Chat, Muyu, Dice, Game, Camera, Clock, Balance, Settings, Menu };
class SparkbotHub {
    Display* display_;
    HubCamera* camera_;
    HubImu imu_;
    HubTouch touch_;
    int selected_=1;
    std::string device_id_,device_name_="小白";
    std::array<std::string,4> colors_{"#cad9ed","#172a42","#26354a","#82e8bc"};
    std::string chassis_="tracks";
    sparkbot::BaseEcho base_echo_;
    sparkbot::DockPresence dock_;
    bool dock_connected_=false,auto_hold_=true,manual_hold_=false,home_hold_=false,popup_pending_=false;
    lv_obj_t* dock_popup_=nullptr;
    int64_t popup_until_=0;
    int shown_selection_=-1;
    std::array<lv_obj_t*,7> menu_dots_{};
    void UpdateDock();
    uint32_t input_count_=0;
    HubImuReading last_reading_;
    uint32_t imu_samples_=0;
    float neutral_x_=0,neutral_y_=0;
    sparkbot::Input last_input_=sparkbot::Input::None;
    std::mutex mutex_;
    std::mutex motion_mutex_;
    sparkbot::Motion motion_;
    sparkbot::Game2048 game_;
    sparkbot::VoyageGame voyage_;
    HubApp app_ = HubApp::Chat;
    uint32_t count_ = 0, voice_seq_ = 0;
    int dice_ = 1, light_ = 2, pending_light_ = -1;
    int64_t dance_until_ = 0, dice_until_ = 0, last_hit_ = 0;
    int64_t last_render_at_=0;
    int64_t last_base_reply_ = 0;
    bool dirty_ = true;
    std::atomic<bool> started_{false};
    lv_obj_t *chat_screen_=nullptr, *screen_=nullptr, *title_=nullptr, *value_=nullptr, *hint_=nullptr, *picture_=nullptr;
    std::array<lv_obj_t*,16> tiles_{};
    std::array<lv_obj_t*,7> pips_{};
    lv_image_dsc_t muyu_image_{};
    std::array<lv_image_dsc_t,8> app_images_{};
    HubApp rendered_app_ = HubApp::Chat;
    void MotorLoop();
    void AppLoop();
    void CameraLoop();
    void Render();
    void CreateScreen(HubApp app);
    void HandleInput(sparkbot::Input input);
    void Navigate(int delta);
public:
    SparkbotHub(Display* display, HubCamera* camera, i2c_master_bus_handle_t bus);
    void Start();
    bool Move(uint32_t owner, uint32_t seq, float x, float y, int duration=350);
    void Stop(bool emergency=false);
    void Arm();
    void Disconnect(uint32_t owner);
    bool VoiceMove(float x,float y,int duration=500);
    bool Dance();
    bool SetLight(int mode);
    void SetApp(HubApp app);
    void NextApp();
    void Click();
    void Knock();
    void Roll();
    void GameMove(sparkbot::Direction direction);
    void NewGame();
    cJSON* Status();
    cJSON* Identity();
    bool Action(const cJSON* json, std::string& error);
    HubCamera* Camera() { return camera_; }
};
