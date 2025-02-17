/*
 * Copyright (C) 2014 lenovo MIUI
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Descriptions:
 * -------------
 * Input Event Hook and Manager
 *
 */

//#define DEBUG //wangxf14_debug

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/poll.h>
#include <linux/input.h>
#include <pthread.h>
#include "../miui_inter.h"

//-- Input Device
#include "input_device.c"
//--screen black echo
//#include "miui_screen.c"	//lenovo-sw wangxf14 20130711 modify, modify for disable auto screen control

//-- GLOBAL EVENT VARIABLE
static  char      key_pressed[KEY_MAX + 1];

//-- AROMA CUSTOM MESSAGE
static  dword     atouch_winmsg[64];
static  byte      atouch_winmsg_n = 0;
static  int       atouch_message_code = 889;

//-- KEY QUEUE
static  int       key_queue[256];
static  int       key_queue_len = 0;
static pthread_mutex_t key_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t key_queue_cond = PTHREAD_COND_INITIALIZER;

//-- TOUCH SCREEN VAR
static  byte      evthread_active = 1;
static  int       evtouch_state   = 0;  //-- Touch State
static  int       evtouch_x       = 0;  //-- Translated X (Ready to use)
static  int       evtouch_y       = 0;  //-- Translated Y (Ready to use)
static  int       evtouch_code    = 888;//-- Touch Virtual Code

//-- PASS TOUCH STATE FUNCTIONS
int touchX()  { return evtouch_x; }
int touchY()  { return evtouch_y; }
int ontouch() { return ((evtouch_state==0)?0:1); }
  
dword atouch_winmsg_get(byte cleanup){
  dword out=0;
  if (atouch_winmsg_n>0){
    out=atouch_winmsg[0];
    if (cleanup){
      int i=0;
      for (i=0;i<atouch_winmsg_n;i++){
        atouch_winmsg[i]=atouch_winmsg[i+1];
      }
      atouch_winmsg_n--;
    }
  }
  return out;
}
byte atouch_winmsg_push(dword msg){
  if (atouch_winmsg_n<64){
    atouch_winmsg[atouch_winmsg_n++]=msg;
    return 1;
  }
  return 0;
}

//-- VIBRATE FUNCTION
int vibrate(int timeout_ms){
    char str[20];
    int fd;
    int ret;
    fd = open("/sys/class/timed_output/vibrator/enable", O_WRONLY);
    if (fd < 0) return -1;
    ret = snprintf(str, sizeof(str), "%d", timeout_ms);
    ret = write(fd, str, ret);
    close(fd);
    if (ret < 0)
       return -1;
    return 0;
}

//-- KEYPRESS MANAGER
int ui_key_pressed(int key){
    return key_pressed[key];
}
void set_key_pressed(int key,char val){
  key_pressed[key]=val;
}

//-- INPUT EVENT POST MESSAGE
void ev_post_message(int key, int value){
  set_key_pressed(key,value);
  pthread_mutex_lock(&key_queue_mutex);
  const int queue_max = sizeof(key_queue) / sizeof(key_queue[0]);
  if (key_queue_len<queue_max){
    key_queue[key_queue_len++] = key;
    pthread_cond_signal(&key_queue_cond);
  }
  pthread_mutex_unlock(&key_queue_mutex);
}

//-- INPUT CALLBACK
void ev_input_callback(struct input_event * ev){
  if (ev->type==EV_KEY){
    ev_post_message(ev->code,ev->value);
  }
  else if (ev->type == EV_ABS) {
    evtouch_x = ev->value >> 16;
    evtouch_y = ev->value & 0xFFFF;
    
    if ((evtouch_x>0)&&(evtouch_y>0)){
      if (ev->code==0){
        evtouch_state = 0;
      }
      else if (evtouch_state==0){
        evtouch_state = 1;
      }
      else{
        evtouch_state = 2;
      }
      ev_post_message(evtouch_code,evtouch_state);
    }
    else{
      //-- False Event
      evtouch_state = 0;
      evtouch_x = 0;
      evtouch_y = 0;
    }
  }
}

/* Begin, lenovo-sw wangxf14 20130711 add, add for auto affirm process */

static int affirm_key_set_time(time_t time);

//-- INPUT THREAD
static void *ev_input_thread(void *cookie){
  //-- Loop for Input
  while (evthread_active){
    struct input_event ev;
    byte res=aipGetInput(&ev, 0);
#if 0
    if (res){
      if (screen_is_black())
      {
          if (ev.type == EV_KEY)
          {
              screen_set_time(time((time_t *)NULL));
          }
      }
      else
      {
          screen_set_time(time((time_t *)NULL));
          ev_input_callback(&ev);
      }
    }
#else
    if (res){
          affirm_key_set_time(time((time_t *)NULL));
          miui_debug("ev.type = %d, ev.code = %d, ev.value = %d\n", ev.type, ev.code, ev.value);
          ev_input_callback(&ev);
    }
#endif	

  }
  return NULL;
}

/* End, lenovo-sw wangxf14 20130711 add, add for auto affirm process */

//-- INIT INPUT DEVICE
void ui_init(){
  miui_ev_init();
}

/*
 *function:filter out unused input event;
 *para:name->the file name ,execude path, no prefix
 *ins: "input0"
 *return value: 1 if filter out
 *              0 filter in
 *
 */
int event_filter(char *name)
{
    //if have filter ,return 1
    return_val_if_fail(strncmp(name, "event", 5) == 0, 0);
    return_val_if_fail(strlen >= 5, 0);
    return_val_if_fail(name != NULL, 0);
    u32 mask = acfg()->input_filter;//from acfg()->input_filter
    char a= *(name + 5);//"inputn", extract n
    return_val_if_fail(a >= 0x30, 0);
    return_val_if_fail(a < 0x40, 0);
    byte i = a - 0x30;
    return_val_if_fail(i > 0, 0);
    return_val_if_fail(i < 32, 0);
    if((mask>>i) & 0x1) {
        return 1; //filter out
}
    return 0;
}

/* Begin, lenovo-sw wangxf14 20130711 add, add for auto affirm process */

static time_t key_time_orig;
static time_t key_time_interval;

static int affirm_key_set_time(time_t time)
{
    return_val_if_fail(time > 0, -1);
    miui_debug("set time %ld\n", time);
    key_time_orig = time;
    return 0;
}

static int affirm_key_set_interval(int interval)
{
    return_val_if_fail(interval > 0, -1);
    key_time_interval = interval;
    return 0;
}

static void *auto_affirm_thread(void *cookie)
{
    while(1) 
    {
        if (difftime(time((time_t *)NULL), key_time_orig) > key_time_interval)
        {
		struct input_event ev;
		ev.type = EV_KEY;
		ev.code = KEY_POWER;
		ev.value = 1;
		if (ev.type==EV_KEY){
			ev_post_message(ev.code,ev.value);
		}

		sleep(1);

		ev.value = 0;
		if (ev.type==EV_KEY){
			ev_post_message(ev.code,ev.value);
		}

		affirm_key_set_time(time((time_t*)NULL));

        }
        sleep(1);
    }
    return NULL;
}
static pthread_t auto_affirm_thread_t;
static int auto_affirm_init()
{
    //default interval 120 seconds
    key_time_interval = 120;
    affirm_key_set_time(time((time_t*)NULL));
    
    pthread_create(&auto_affirm_thread_t, NULL, auto_affirm_thread, NULL);
    pthread_detach(auto_affirm_thread_t);
    return 0;
}
/* End, lenovo-sw wangxf14 20130711 add, add for auto affirm process */

int miui_ev_init(){
  aipInit(&event_filter);
  
  //-- Create Watcher Thread
  evthread_active = 1;
//  screen_init(); //lenovo-sw wangxf14 20130711 modify, disable auto screen control
  auto_affirm_init();//lenovo-sw wangxf14 20130711 add, add auto power key control init
  pthread_t input_thread_t;
  pthread_create(&input_thread_t, NULL, ev_input_thread, NULL);
  pthread_detach(input_thread_t);
  
  return 0;
}

//-- RELEASE INPUT DEVICE
void miui_ev_exit(void){
  evthread_active = 0;
  aipRelease();
}

//-- SEND ATOUCH CUSTOM MESSAGE
byte atouch_send_message(dword msg){
  if (atouch_winmsg_push(msg)){
    ev_post_message(atouch_message_code,0);
    return 1;
  }
  return 0;
}

//-- Clear Queue
void ui_clear_key_queue_ex(){
  pthread_mutex_lock(&key_queue_mutex);
  key_queue_len = 0;
  pthread_mutex_unlock(&key_queue_mutex);
  atouch_winmsg_n=0;
}
void ui_clear_key_queue() {
  pthread_mutex_lock(&key_queue_mutex);
  key_queue_len = 0;
  pthread_mutex_unlock(&key_queue_mutex);
  if (atouch_winmsg_n>0) ev_post_message(atouch_message_code,0);
}

//-- Wait For Key
int ui_wait_key(){
  pthread_mutex_lock(&key_queue_mutex);
  while (key_queue_len == 0){
    pthread_cond_wait(&key_queue_cond, &key_queue_mutex);
  }
  int key = key_queue[0];
  memcpy(&key_queue[0], &key_queue[1], sizeof(int) * --key_queue_len);
  pthread_mutex_unlock(&key_queue_mutex);
  return key;
}

static void input_event_dump(const char *str, struct input_event *ev)
{
    miui_debug("[%s]ev->type:%4x , ev->code:%4x, ev->value:%4x\n",str,  ev->type, ev->code, ev->value);
}

//-- AROMA Input Handler
int atouch_wait(ATEV *atev){
#ifdef DEBUG
	input_event_dump("atouch message", atev);
#endif
  return atouch_wait_ex(atev,0);
}
int atouch_wait_ex(ATEV *atev, byte calibratingtouch){
  atev->x = -1;
  atev->y = -1;
  
  while (1){
    int key = ui_wait_key();
    
    //-- Custom Message
    if (key==atouch_message_code){
      atev->msg = atouch_winmsg_get(1);
      atev->d   = 0;
      atev->x   = 0;
      atev->y   = 0;
      atev->k   = 0;
      return ATEV_MESSAGE;
    }
    
    atev->d = ui_key_pressed(key);
    atev->k = key;
    
    if (key==evtouch_code){
      if ((evtouch_x>0)&&(evtouch_y>0)){
        atev->x = evtouch_x;
        atev->y = evtouch_y;
        switch(evtouch_state){
          case 1:  return ATEV_MOUSEDN; break;
          case 2:  return ATEV_MOUSEMV; break;
          default: return ATEV_MOUSEUP; break;
        }
      }
    }
    else if ((key!=0)&&(key==acfg()->ckey_up))      return ATEV_UP;
    else if ((key!=0)&&(key==acfg()->ckey_down))    return ATEV_DOWN;
    else if ((key!=0)&&(key==acfg()->ckey_select))  return ATEV_SELECT;
    else if ((key!=0)&&(key==acfg()->ckey_back))    return ATEV_BACK;
    else if ((key!=0)&&(key==acfg()->ckey_menu))    return ATEV_MENU;
    else{
      /* DEFINED KEYS */
      switch (key){
        /* RIGHT */
        case KEY_RIGHT: return ATEV_RIGHT; break;
        /* LEFT */
        case KEY_LEFT:  return ATEV_LEFT; break;
        
        /* DOWN */
        case KEY_DOWN:
        case KEY_CAPSLOCK:
        case KEY_VOLUMEDOWN:
          return ATEV_DOWN; break;
        
        /* UP */
        case KEY_UP:
        case KEY_LEFTSHIFT:
        case KEY_VOLUMEUP:
          return ATEV_UP; break;
        
        /* SELECT */
        case KEY_LEFTBRACE:
        case KEY_POWER:
        case KEY_HOME:
        case BTN_MOUSE:
        case KEY_ENTER:
        case KEY_CENTER:
        case KEY_CAMERA:
        case KEY_F21:
        case KEY_SEND:
        case KEY_END:
        case KEY_HOMEPAGE:

        case KEY_SEARCH:
        case KEY_MENU:
           return ATEV_SELECT; break;
        
        /* SHOW MENU */
        case 229:
          return ATEV_MENU; break;
        
        /* BACK */
        case KEY_BACKSPACE:
        case KEY_BACK:
          return ATEV_BACK; break;
      }
    }
  }
  return 0;
}
//-- 
