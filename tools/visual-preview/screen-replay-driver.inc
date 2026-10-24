// Scene inputs only. All shapes, fonts and transitions come from extracted firmware functions.
static std::vector<uint8_t> framebuffer(240*240*2),drawbuffer(240*240*2);
static std::vector<uint8_t> ReadBytes(const std::string&path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Missing fixture: "+path);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
int main(int argc,char**argv){
 try{
 if(argc!=4)throw std::runtime_error("Usage: screen-replay firmware-assets output-folder camera-fixture.rgb565");
 const std::string assets=argv[1],output=argv[2];
 muyu_bytes=ReadBytes(assets+"/muyu.rgb565a8");icon_bytes=ReadBytes(assets+"/app-icons.rgb565a8");muyu_img_start=muyu_bytes.data();app_icons_start=icon_bytes.data();
 lv_init();auto display=lv_display_create(240,240);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
 lv_display_set_buffers(display,drawbuffer.data(),nullptr,uint32_t(drawbuffer.size()),LV_DISPLAY_RENDER_MODE_FULL);
 lv_display_set_flush_cb(display,[](lv_display_t*d,const lv_area_t*a,uint8_t*data){for(int y=a->y1;y<=a->y2;++y)memcpy(framebuffer.data()+(y*240+a->x1)*2,data+(y-a->y1)*(a->x2-a->x1+1)*2,(a->x2-a->x1+1)*2);lv_display_flush_ready(d);});
 SparkbotDisplay face;face.SetupUI();face.InitFace();face.SetStatus("待命");face.SetEmotion("neutral");
 HubCamera camera;camera.pixels=ReadBytes(argv[3]);camera.preview.header.magic=LV_IMAGE_HEADER_MAGIC;camera.preview.header.cf=LV_COLOR_FORMAT_RGB565;camera.preview.header.w=240;camera.preview.header.h=240;camera.preview.header.stride=480;camera.preview.data_size=uint32_t(camera.pixels.size());camera.preview.data=camera.pixels.data();
 SparkbotHub hub(&face,&camera,nullptr);hub.chat_screen_=lv_screen_active();
 std::ofstream index(output+"/scenes.tsv");
 auto scene=[&](const char*name,int duration,const std::function<void(int)>&input){
  std::ofstream frames(output+"/"+name+".rgb565",std::ios::binary);if(!frames)throw std::runtime_error("Cannot create frame output");
  for(int t=0;t<duration;t+=20){lv_tick_inc(20);input(t);hub.Render();lv_timer_handler();if(t%40==0){lv_refr_now(display);frames.write((const char*)framebuffer.data(),framebuffer.size());}}
  index<<name<<"\t"<<duration/40<<"\t25\n";std::cout<<"Rendered "<<name<<std::endl;
 };
 auto home=[&](int style){hub.app_=HubApp::Chat;hub.dirty_=true;face.style_=style;face.subtitles_=false;face.SetChatMessage("assistant","");Application::GetInstance().state=kDeviceStateIdle;face.SetTouchFeedback({});face.SetEmotion("neutral");};
 auto idle=[&](int){face.next_expression_=lv_tick_get()+100000;};
 home(0);scene("eyes-idle",5200,idle);
 const char*emotions[]={"happy","curious","sleepy","surprised","winking","shy","confident"};
 for(const char*e:emotions){home(0);face.SetEmotion(e);scene((std::string("eyes-")+e).c_str(),2200,idle);}
 for(int zone=1;zone<=3;++zone){home(0);sparkbot::TouchGestures touch;scene((std::string("eyes-hit-")+std::to_string(zone)).c_str(),2000,[&](int t){if(t==400)touch.Edge(zone,true,Now());if(t==520)touch.Edge(zone,false,Now());touch.Poll(Now());face.SetTouchFeedback(touch.Feedback(Now()));idle(t);});}
 for(int zone=1;zone<=3;++zone){home(0);sparkbot::TouchGestures touch;scene((std::string("eyes-pet-")+std::to_string(zone)).c_str(),3600,[&](int t){if(t==200)touch.Edge(zone,true,Now());if(t==2200)touch.Edge(zone,false,Now());touch.Poll(Now());face.SetTouchFeedback(touch.Feedback(Now()));idle(t);});}
 home(0);scene("eyes-dialogue",4800,[&](int t){face.subtitles_=true;Application::GetInstance().state=t<1500?kDeviceStateListening:kDeviceStateSpeaking;face.SetChatMessage(t<1500?"user":"assistant",t<1500?"嘿，小白":"你好，我是小白。");idle(t);});
 home(1);scene("mecha-idle",4200,idle);
 home(1);scene("mecha-dialogue",4200,[&](int t){Application::GetInstance().state=t<1200?kDeviceStateListening:kDeviceStateSpeaking;idle(t);});
 for(int zone=1;zone<=3;++zone){home(1);sparkbot::TouchGestures touch;scene((std::string("mecha-hit-")+std::to_string(zone)).c_str(),2000,[&](int t){if(t==400)touch.Edge(zone,true,Now());if(t==520)touch.Edge(zone,false,Now());touch.Poll(Now());face.SetTouchFeedback(touch.Feedback(Now()));idle(t);});}
 for(int zone=1;zone<=3;++zone){home(1);sparkbot::TouchGestures touch;scene((std::string("mecha-pet-")+std::to_string(zone)).c_str(),3600,[&](int t){if(t==200)touch.Edge(zone,true,Now());if(t==2200)touch.Edge(zone,false,Now());touch.Poll(Now());face.SetTouchFeedback(touch.Feedback(Now()));idle(t);});}
 home(2);scene("classic-xiaozhi",4800,[&](int t){face.subtitles_=true;face.SetEmotion(t<1600?"neutral":t<3200?"happy":"thinking");face.SetChatMessage("assistant",t<1600?"嘿，小白":t<3200?"你好，我是小白。":"让我想一想。");});
 home(0);hub.app_=HubApp::Menu;hub.dirty_=true;scene("page-menu",7000,[&](int t){int selected=1+t/1000;if(hub.selected_!=selected){hub.selected_=selected;hub.dirty_=true;}});
 hub.app_=HubApp::Muyu;hub.dirty_=true;scene("page-muyu",3200,[&](int t){if(t%600==200){++hub.count_;hub.last_hit_=Now();hub.dirty_=true;}});
 hub.app_=HubApp::Dice;hub.dirty_=true;hub.dice_until_=Now()+700;scene("page-dice",3000,[&](int t){if(t==1500)hub.dice_until_=Now()+700;});
 hub.app_=HubApp::Game;hub.game_.SetBoard({2,2,4,0,0,4,4,0,0,0,8,0,0,0,0,2});hub.dirty_=true;scene("page-2048",4000,[&](int t){if(t>0&&t%800==0){hub.game_.Slide(static_cast<sparkbot::Direction>((t/800)%4));hub.game_.Spawn(esp_random());hub.dirty_=true;}});
 hub.app_=HubApp::Camera;hub.dirty_=true;scene("page-camera",2400,[](int){});
 hub.app_=HubApp::Clock;hub.dirty_=true;scene("page-clock",3400,[](int){});
 hub.app_=HubApp::Balance;hub.voyage_.Reset();hub.voyage_.Toggle();hub.dirty_=true;scene("page-voyage",6000,[&](int t){if(t%500==0)hub.voyage_.Steer(hub.voyage_.CoinLane()-hub.voyage_.Lane());hub.voyage_.Step(.02f);});
 hub.app_=HubApp::Settings;hub.dirty_=true;scene("page-settings",4000,[&](int t){face.style_=(t/1400)%3;face.subtitles_=t>=2200;});
 home(0);scene("page-dock",3200,[&](int t){if(t==400){hub.dock_connected_=true;hub.home_hold_=true;hub.popup_pending_=true;hub.dirty_=true;}});hub.home_hold_=false;hub.dock_connected_=false;
 home(0);scene("page-transitions",10000,[&](int t){HubApp apps[]{HubApp::Chat,HubApp::Menu,HubApp::Muyu,HubApp::Menu,HubApp::Dice,HubApp::Menu,HubApp::Game,HubApp::Menu,HubApp::Settings,HubApp::Chat};auto app=apps[t/1000];if(hub.app_!=app){hub.app_=app;hub.dirty_=true;}idle(t);});
 return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}
}
