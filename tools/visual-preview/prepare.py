"""Generate a headless LVGL replay from the actual firmware drawing functions."""
from pathlib import Path
import hashlib,json,re,shutil
root=Path(__file__).resolve().parents[2]
source=root/'firmware'
out=root/'visual-preview-output/renderer';out.mkdir(parents=True,exist_ok=True)
display=(source/'main/boards/esp-sparkbot/sparkbot_display.cc').read_text(encoding='utf8')
hub=(source/'main/boards/esp-sparkbot/sparkbot_hub.cc').read_text(encoding='utf8')
lcd=(source/'main/display/lcd_display.cc').read_text(encoding='utf8')
def between(text,start,end):return text[text.index(start):text.index(end,text.index(start))]
for name in ['hub_core.h','hub_input.h','hub_voyage.h','hub_dock.h','hub_profile.h']:
    (out/name).write_bytes((source/'main/boards/esp-sparkbot'/name).read_bytes())
header=(source/'main/boards/esp-sparkbot/sparkbot_display.h').read_text(encoding='utf8')
header='\n'.join(line for line in header.splitlines() if not line.startswith('#include') and line!='#pragma once')
header=header.replace('class SparkbotDisplay : public SpiLcdDisplay {','class SparkbotDisplay : public SpiLcdDisplay {\npublic:')
hheader=(source/'main/boards/esp-sparkbot/sparkbot_hub.h').read_text(encoding='utf8')
hheader='\n'.join(line for line in hheader.splitlines() if not line.startswith('#include') and line!='#pragma once')
hheader=hheader.replace('class SparkbotHub {','class SparkbotHub {\npublic:')
setup=lcd[lcd.index('void LcdDisplay::SetupUI()',lcd.index('#else',lcd.index('#define  MAX_MESSAGES'))):]
setup=setup[:setup.index('\nvoid LcdDisplay::SetPreviewImage')].replace('LcdDisplay::SetupUI','SpiLcdDisplay::SetupUI')
emotion=between(lcd,'void LcdDisplay::SetEmotion','\nvoid LcdDisplay::SetIcon').replace('LcdDisplay::SetEmotion','SpiLcdDisplay::SetEmotion')
body=display[display.index('void SparkbotDisplay::InitFace'):]
render=between(hub,'static lv_obj_t* Label','\nvoid SparkbotHub::AppLoop')
render=render.replace('time_t t=time(nullptr);','time_t t=demo_clock();')
constructor=between(hub,'SparkbotHub::SparkbotHub','\nvoid SparkbotHub::Start')
motion=between(hub,'static void EnterMotion','static const char* app_names')
titles=between(hub,'static const char* app_names','static std::string HotspotName')
(out/'extracted.cc').write_text('#include "host.h"\n'+header+'\n'+hheader+'\n'+setup+'\n'+emotion+'\n'+body+'\n'+motion+'\n'+titles+'\n'+constructor+'\n'+render+'\n#include "driver.inc"\n',encoding='utf8')
lvgl=(root/'firmware/managed_components/lvgl__lvgl').as_posix()
fonts=(root/'firmware/managed_components/78__xiaozhi-fonts').as_posix()
(out/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.20)
project(sparkbot_screen_replay LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
file(GLOB_RECURSE LV_SOURCES "{lvgl}/src/*.c")
file(GLOB EMOJI_SOURCES "{fonts}/src/emoji/*_64.c")
add_library(replay_lvgl STATIC ${{LV_SOURCES}})
target_include_directories(replay_lvgl PUBLIC "{lvgl}" "{out.as_posix()}")
target_compile_definitions(replay_lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE LV_LVGL_H_INCLUDE_SIMPLE)
add_executable(screen-replay extracted.cc "{fonts}/src/font_puhui_20_4.c" "{fonts}/src/font_awesome_20_4.c" "{fonts}/src/font_awesome_30_4.c" "{fonts}/src/font_awesome.c" "{fonts}/src/font_emoji_64.c" ${{EMOJI_SOURCES}})
target_include_directories(screen-replay PRIVATE "{fonts}/include")
target_link_libraries(screen-replay PRIVATE replay_lvgl)
if(MSVC)
 target_compile_options(replay_lvgl PRIVATE /utf-8 /wd4244 /wd4267 /wd4996)
 target_compile_options(screen-replay PRIVATE /utf-8 /wd4244 /wd4267 /wd4996)
endif()
''',encoding='utf8')
(out/'lv_conf.h').write_text('''#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 16
#define LV_USE_OS LV_OS_NONE
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_FMT_TXT_LARGE 1
#define LV_USE_IMGFONT 1
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 0
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80
#define LV_DEF_REFR_PERIOD 20
#endif
''',encoding='utf8')
(out/'esp_log.h').write_text('#pragma once\n',encoding='ascii')
manifest=dict(method='Headless LVGL 9.3 replay of actual firmware UI functions; deterministic state fixtures, no ESP32 execution.',
    source_files={p:hashlib.sha256((source/p).read_bytes()).hexdigest() for p in ['main/boards/esp-sparkbot/sparkbot_display.cc','main/boards/esp-sparkbot/sparkbot_hub.cc','main/display/lcd_display.cc']},
    functions=['InitFace','Animate','DrawMecha','SetEmotion','SetTouchFeedback','CreateScreen','Render','non-WeChat SetupUI'],
    adaptations=['Hardware services are local stubs','Class fields exposed only to fixture driver','Clock and camera data are demonstration inputs','No dialogue server or motor commands','25 FPS output encoding; not measured hardware FPS'])
(out.parent/'render-provenance.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
shutil.copy2(Path(__file__).with_name('screen-replay-host.h'),out/'host.h')
shutil.copy2(Path(__file__).with_name('screen-replay-driver.inc'),out/'driver.inc')
print('Generated actual-code replay:',out)
