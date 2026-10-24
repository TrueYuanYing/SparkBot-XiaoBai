#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "settings.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <driver/uart.h>
#include <cstring>

#include "hub_camera.h"
#include "sparkbot_hub.h"
#include "hub_server.h"
#include "sparkbot_display.h"
#include <ssid_manager.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_mac.h>
#include <esp_sntp.h>

#define TAG "esp_sparkbot"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

class EspSparkBot : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Display* display_;
    HubCamera* camera_;
    SparkbotHub* hub_ = nullptr;
    HubServer* hub_server_ = nullptr;
    light_mode_t light_mode_ = LIGHT_MODE_ALWAYS_ON;

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_GPIO;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_GPIO;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() { if (hub_) hub_->Click(); });
        boot_button_.OnDoubleClick([this]() { if (hub_) hub_->NextApp(); });
        boot_button_.OnLongPress([this]() { if (hub_) { hub_->Stop(true); display_->ShowNotification("急停已锁定，请在网页解除"); } });
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_GPIO;
        io_config.dc_gpio_num = DISPLAY_DC_GPIO;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_disp_on_off(panel, true);
        display_ = new SparkbotDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_20_4,
                                        .icon_font = &font_awesome_20_4,
                                        .emoji_font = font_emoji_64_init(),
                                    });
        static_cast<SparkbotDisplay*>(display_)->InitFace();
    }

    void InitializeCamera() {
        camera_config_t camera_config = {};

        camera_config.pin_pwdn = SPARKBOT_CAMERA_PWDN;
        camera_config.pin_reset = SPARKBOT_CAMERA_RESET;
        camera_config.pin_xclk = SPARKBOT_CAMERA_XCLK;
        camera_config.pin_pclk = SPARKBOT_CAMERA_PCLK;
        camera_config.pin_sccb_sda = SPARKBOT_CAMERA_SIOD;
        camera_config.pin_sccb_scl = SPARKBOT_CAMERA_SIOC;

        camera_config.pin_d0 = SPARKBOT_CAMERA_D0;
        camera_config.pin_d1 = SPARKBOT_CAMERA_D1;
        camera_config.pin_d2 = SPARKBOT_CAMERA_D2;
        camera_config.pin_d3 = SPARKBOT_CAMERA_D3;
        camera_config.pin_d4 = SPARKBOT_CAMERA_D4;
        camera_config.pin_d5 = SPARKBOT_CAMERA_D5;
        camera_config.pin_d6 = SPARKBOT_CAMERA_D6;
        camera_config.pin_d7 = SPARKBOT_CAMERA_D7;

        camera_config.pin_vsync = SPARKBOT_CAMERA_VSYNC;
        camera_config.pin_href = SPARKBOT_CAMERA_HSYNC;
        camera_config.pin_pclk = SPARKBOT_CAMERA_PCLK;
        camera_config.xclk_freq_hz = SPARKBOT_CAMERA_XCLK_FREQ;
        camera_config.ledc_timer = SPARKBOT_LEDC_TIMER;
        camera_config.ledc_channel = SPARKBOT_LEDC_CHANNEL;
        camera_config.fb_location = CAMERA_FB_IN_PSRAM;
        
        camera_config.sccb_i2c_port = I2C_NUM_0;
        
        camera_config.pixel_format = PIXFORMAT_RGB565;
        camera_config.frame_size = FRAMESIZE_240X240;
        camera_config.jpeg_quality = 12;
        camera_config.fb_count = 1;
        camera_config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
        
        camera_ = new HubCamera(camera_config);

        Settings settings("sparkbot", false);
        // 考虑到部分复刻使用了不可动摄像头的设计，默认启用翻转
        bool camera_flipped = static_cast<bool>(settings.GetInt("camera-flipped", 1));
        camera_->SetHMirror(camera_flipped);
        camera_->SetVFlip(camera_flipped);
    }

    /*
        ESP-SparkBot 的底座
        https://gitee.com/esp-friends/esp_sparkbot/tree/master/example/tank/c2_tracked_chassis
    */
    void InitializeEchoUart() {
        uart_config_t uart_config = {
            .baud_rate = ECHO_UART_BAUD_RATE,
            .data_bits = UART_DATA_8_BITS,
            .parity    = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
        };
        int intr_alloc_flags = 0;

        ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
        ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
        ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, UART_ECHO_TXD, UART_ECHO_RXD, UART_ECHO_RTS, UART_ECHO_CTS));

        SendUartMessage("w2");
    }

    void SendUartMessage(const char * command_str) {
        uint8_t len = strlen(command_str);
        uart_write_bytes(ECHO_UART_PORT_NUM, command_str, len);
        ESP_LOGI(TAG, "Sent command: %s", command_str);
    }

    void InitializeTools() {
        auto& mcp = McpServer::GetInstance();
        mcp.AddTool("self.chassis.go_forward", "前进半秒后自动停车", PropertyList(),
            [this](const PropertyList&) -> ReturnValue {return hub_->VoiceMove(0,0.6f);});
        mcp.AddTool("self.chassis.go_back", "后退半秒后自动停车", PropertyList(),
            [this](const PropertyList&) -> ReturnValue {return hub_->VoiceMove(0,-0.6f);});
        mcp.AddTool("self.chassis.turn_left", "向左转半秒后自动停车", PropertyList(),
            [this](const PropertyList&) -> ReturnValue {return hub_->VoiceMove(-0.6f,0);});
        mcp.AddTool("self.chassis.turn_right", "向右转半秒后自动停车", PropertyList(),
            [this](const PropertyList&) -> ReturnValue {return hub_->VoiceMove(0.6f,0);});
        mcp.AddTool("self.chassis.stop", "立即停止坦克", PropertyList(),
            [this](const PropertyList&) -> ReturnValue {hub_->Stop();return true;});
        mcp.AddTool("self.chassis.dance", "跳舞三秒，可随时停止", PropertyList(),
            [this](const PropertyList&) -> ReturnValue {return hub_->Dance();});
        mcp.AddTool("self.chassis.switch_light_mode", "灯效：2常亮3闪烁4慢呼吸5快呼吸6流光7灯光秀8关闭",
            PropertyList({Property("light_mode",kPropertyTypeInteger,2,8)}),
            [this](const PropertyList& p) -> ReturnValue {return hub_->SetLight(p["light_mode"].value<int>());});
        mcp.AddTool("self.entertainment.open", "打开机器人应用：chat小智，muyu木鱼，dice骰子，game2048，camera摄像头，clock时钟",
            PropertyList({Property("name",kPropertyTypeString)}),[this](const PropertyList& p) -> ReturnValue {
                auto j=cJSON_CreateObject();cJSON_AddStringToObject(j,"action","app");
                cJSON_AddStringToObject(j,"name",p["name"].value<std::string>().c_str());
                std::string error;bool ok=hub_->Action(j,error);cJSON_Delete(j);return ok;
            });
        mcp.AddTool("self.entertainment.knock", "敲一次木鱼", PropertyList(),
            [this](const PropertyList&) -> ReturnValue {hub_->SetApp(HubApp::Muyu);hub_->Knock();return true;});
        mcp.AddTool("self.entertainment.roll_dice", "摇骰子", PropertyList(),
            [this](const PropertyList&) -> ReturnValue {hub_->SetApp(HubApp::Dice);hub_->Roll();return true;});
        mcp.AddTool("self.camera.set_camera_flipped", "翻转摄像头图像方向", PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                Settings settings("sparkbot",true);bool flipped=!settings.GetInt("camera-flipped",1);
                bool ok=camera_->SetHMirror(flipped)&&camera_->SetVFlip(flipped);
                if(ok) settings.SetInt("camera-flipped",flipped);
                return ok;
            });
    }

public:
    EspSparkBot() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        InitializeCamera();
        InitializeEchoUart();
        hub_ = new SparkbotHub(display_, camera_, i2c_bus_);
        InitializeTools();
        GetBacklight()->RestoreBrightness();
    }

    void StartNetwork() override {
        hub_->Start();
        // Apply staged credentials before WifiStation starts its scan task.
        { Settings pending("hub_wifi",true);auto ssid=pending.GetString("ssid");
          if(!ssid.empty()){SsidManager::GetInstance().AddSsid(ssid,pending.GetString("password"));pending.EraseAll();} }
        wifi_config_mode_=false;
        auto& station=WifiStation::GetInstance();
        station.Start();
        esp_netif_t* ap=esp_netif_create_default_wifi_ap();
        ESP_ERROR_CHECK(ap?ESP_OK:ESP_ERR_NO_MEM);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        wifi_config_t config{};
        uint8_t mac[6];ESP_ERROR_CHECK(esp_read_mac(mac,ESP_MAC_WIFI_SOFTAP));
        snprintf(reinterpret_cast<char*>(config.ap.ssid),sizeof(config.ap.ssid),"SparkBot-%02X%02X",mac[4],mac[5]);
        strncpy(reinterpret_cast<char*>(config.ap.password),"sparkbot2026",sizeof(config.ap.password)-1);
        config.ap.authmode=WIFI_AUTH_WPA2_PSK;config.ap.max_connection=3;config.ap.channel=1;
        config.ap.pmf_cfg.required=false;
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&config));
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
        hub_server_=new HubServer(*hub_);hub_server_->Start();
        ESP_LOGI(TAG,"Unified hotspot: %s; fixed URL http://192.168.4.1",config.ap.ssid);
        display_->ShowNotification("左侧三击选应用\n右侧三击回主页",7000);
        while(!station.WaitForConnected(1000)) { /* Local apps and control run independently. */ }
        setenv("TZ","CST-8",1);tzset();
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0,"ntp.aliyun.com");esp_sntp_init();
    }
    void SetPowerSaveMode(bool) override { esp_wifi_set_ps(WIFI_PS_NONE); }

    virtual AudioCodec* GetAudioCodec() override {
         static Es8311AudioCodec audio_codec(i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, true, false, true);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }
};

DECLARE_BOARD(EspSparkBot);
