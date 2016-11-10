#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#define BUF_SAMPLES 4410
#define WAVE_TABLE_SIZE 128
#define SAMPLING_FREQ 44100
#define UPRATE 1.0594
#define OUTPUT_FREQ (27.500)
#define OVER_SAMPLING_GAIN 16
#define SOFT_RESAMPLE       1
#define LATENCY             50000
#define WAVE_FORMAT_ID      1
#define DEF_CHANNELS        2
#define DEF_SAMPLING_RATE   48000
#define DEF_BITPERSAMPLE    16
#define PLAY_SOUND_NUM 10
#define SOUND_TIME 44100
#define PCM_DEVICE          "default"
#define ENVE_CALC_INTERVAL 32

typedef struct{
  unsigned int vel1;
  unsigned int vel2;
  unsigned int time1;
  unsigned int time2;
  unsigned int time3;
  unsigned int time4;
}enve_t;

typedef struct{
  unsigned int time;
  unsigned int len;
  unsigned int key;
  unsigned int vel;
}sound_dat_t;

typedef struct{
  const enve_t *enve;
  sound_dat_t data;
  unsigned int theta;
  unsigned int active;
  unsigned int time;
  int old_data;
  int new_data;
}sound_t;

const sound_dat_t sound_dat[]={
#include "sound_data.h"
};

void play(void);
void data_flush(int16_t data[]);
int get_enve(int len, int time, const enve_t *enve);
void mk_table(void);
void init_sound(void);
int set_sound(int cur_sound);
char input_real_time(void);
int kbhit(void);
int add_sound(char key, unsigned int vel, unsigned int len);
int lerp32(int old_data, int new_data, int per32);

uint16_t freq_table[128]={0};
const int16_t t_sin[]={
  1607,3211,4807,6392,7961,9511,11038,12539,14009,15446,16845,18204,19519,20787,22005,23169,24278,25329,26319,27245,28105,28898,29621,30273,30852,31356,31785,32137,32412,32609,32728,32767,32728,32609,32412,32137,31785,31356,30852,30273,29621,28898,28105,27245,26319,25329,24278,23169,22005,20787,19519,18204,16845,15446,14009,12539,11038,9511,7961,6392,4807,3211,1607,-1,-1609,-3213,-4809,-6394,-7963,-9513,-11040,-12541,-14011,-15448,-16847,-18206,-19521,-20789,-22007,-23171,-24280,-25331,-26321,-27247,-28107,-28900,-29623,-30275,-30854,-31358,-31787,-32139,-32414,-32611,-32730,-32768,-32730,-32611,-32414,-32139,-31787,-31358,-30854,-30275,-29623,-28900,-28107,-27247,-26321,-25331,-24280,-23171,-22007,-20789,-19521,-18206,-16847,-15448,-14011,-12541,-11040,-9513,-7963,-6394,-4809,-3213,-1609,-1};

const uint8_t t_exp[]={
  10.3,10.6,10.9,11.3,11.6,12,12.3,12.7,13.1,13.5,13.9,14.3,14.8,15.2,15.7,16.2,16.7,17.2,17.7,18.2,18.8,19.3,19.9,20.5,21.2,21.8,22.5,23.2,23.9,24.6,25.3,26.1,26.9,27.7,28.6,29.4,30.3,31.3,32.2,33.2,34.2,35.3,36.3,37.4,38.6,39.7,41,42.2,43.5,44.8,46.2,47.6,49,50.5,52.1,53.7,55.3,57,58.7,60.5,62.3,64.2,66.2,68.2,70.3,72.4,74.6,76.9,79.2,81.7,84.1,86.7,89.4,92.1,94.9,97.8,100.7,103.8,107,110.2,113.6,117,120.6,124.3,128.1,132,136,140.1,144.4,148.8,153.3,158,162.8,167.8,172.9,178.1,183.6,189.2,194.9,200.9};

const enve_t enve = {
  .vel1 = 50,
  .vel2 = 20,
  .time1 = (int)(0.20*SAMPLING_FREQ),
  .time2 = (int)(0.3*SAMPLING_FREQ),
  .time3 = (int)(6*SAMPLING_FREQ),
  .time4 = (int)(0.5SAMPLING_FREQ),
};

sound_t sound[PLAY_SOUND_NUM];

int main(void){
  mk_table();
  init_sound();
  play();
  return EXIT_SUCCESS;
}

void play(void){ 
  
  int16_t data[BUF_SAMPLES];
  snd_pcm_t               *handle = NULL;
  static snd_pcm_format_t format = SND_PCM_FORMAT_S16;  /* 符号付き16bit */
  int time = 0;
  int cur_sound = 0;

  /* PCMデバイスのオープン */
  snd_pcm_open(&handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
  /* PCMパラメーターの設定 */
  snd_pcm_set_params(handle, format, SND_PCM_ACCESS_RW_INTERLEAVED, 1,SAMPLING_FREQ, SOFT_RESAMPLE,LATENCY);

  //sound.freq = WAVE_TABLE_SIZE * OVER_SAMPLING_GAIN * OUTPUT_FREQ / SAMPLING_FREQ ; 
  
  while(1){
    {
      int i,j;
      char c;
      
      data_flush(data);

      /* if(kbhit()){ */
      /* 	add_sound(getchar(), 50, 44100); */
      /* } */
      /* printf("cu%d,%d\n",cur_sound,time); */
      
      while(time > sound_dat[cur_sound].time){
	set_sound(cur_sound);
	cur_sound++;
      }
      
      for(i=0;i<PLAY_SOUND_NUM;i++){
	if(!sound[i].active)continue;
	
	for(j=0;j<BUF_SAMPLES;j++){
	  int per32 = sound[i].time & (ENVE_CALC_INTERVAL-1);
	  if(per32 == 0){
	    sound[i].new_data = sound[i].old_data;
	    sound[i].old_data = get_enve(sound[i].data.len,sound[i].time,sound[i].enve);
	  }
	  data[j] +=
	      ((t_sin[sound[i].theta / OVER_SAMPLING_GAIN] * lerp32(sound[i].old_data, sound[i].new_data, per32)  >>8)* sound[i].data.vel) >>7;
	  
	  sound[i].time++;
	  sound[i].theta = (sound[i].theta + freq_table[sound[i].data.key]) & (WAVE_TABLE_SIZE * OVER_SAMPLING_GAIN -1);
	}
	
	if(sound[i].time > sound[i].data.len + sound[i].enve->time4){sound[i].active = 0;}
      }
      snd_pcm_writei(handle, (const void*)data, BUF_SAMPLES);
    }

    time+=100;
  }
    
  snd_pcm_drain(handle);
  snd_pcm_close(handle);
}

void data_flush(int16_t data[]){
  int i;
  for(i=0;i<BUF_SAMPLES;i++){
    data[i]=0;
  }
}

void mk_table(void){
  int i;
  double freq;
  for(i=0;i<128;i++){
    freq = OUTPUT_FREQ*pow(UPRATE,i);
    freq_table[i] = WAVE_TABLE_SIZE * OVER_SAMPLING_GAIN * freq / SAMPLING_FREQ ;  
  }
}

void init_sound(void){
  int i;

  for(i=0;i<PLAY_SOUND_NUM;i++){
    sound[i].enve = &enve;
    sound[i].theta = 0;
    sound[i].active = 0;
    sound[i].time = 0;
    sound[i].old_data = 0;
    sound[i].new_data = 0;
  }
}

int set_sound(int cur_sound){
  int i;

  for(i=0;i<PLAY_SOUND_NUM;i++){
    if(sound[i].active)continue;
    sound[i].data = sound_dat[cur_sound];
    sound[i].active = 1;
    sound[i].time = 0;
    return 1;
  }

  return 0;
}

int add_sound(char key, unsigned int vel, unsigned int len){
  /* int i; */

  /* for(i=0;i<PLAY_SOUND_NUM;i++){ */
  /*   if(sound[i].active)continue; */
  /*   sound[i].data->freq = key; */
  /*   sound[i].data->vel = vel; */
  /*   sound[i].data->len = len; */
  /*   sound[i].active = 1; */
  /*   sound[i].time = 0; */
  /*   return 1; */
  /* } */

  return 0;
}

int lerp32(int old_data, int new_data, int per32){
  return (old_data * (32-per32) + new_data * per32)>>5;
}

int get_enve(int len ,int time, const enve_t *enve){
  int target_time, target_vel, prev_target_vel, prev_target_time,res;

  if(time < enve->time1){
    target_time = enve->time1;
    target_vel = enve->vel1;
    prev_target_time = 0;
    prev_target_vel = 0;
  }else if(time < enve->time2){
    target_time = enve->time2;
    target_vel = enve->vel2;
    prev_target_time = enve->time1;
    prev_target_vel = enve->vel1;
  }else if(time < enve->time3){
    target_time = enve->time3;
    target_vel = 0;
    prev_target_time = enve->time2;
    prev_target_vel = enve->vel2;
  }else{
    return 0;
  }

  if(time < len){
    res = (time - prev_target_time) * (target_vel - prev_target_vel) / (target_time - prev_target_time) + prev_target_vel;
  }else{
    int len_target_vel = (len - prev_target_time) * (target_vel - prev_target_vel) / (target_time - prev_target_time) + prev_target_vel;
    res = (time - len) * (0 - len_target_vel) / (int)enve->time4  + len_target_vel;
  }

  if(res>0)return t_exp[res];
  else return 0;
}

char input_real_time(void){
  struct termios old_termios_cfg,new_termios_cfg;
  int c,file_descriptor_flg;
 
  tcgetattr(STDIN_FILENO, &old_termios_cfg);//端末の設定を取得
  new_termios_cfg = old_termios_cfg;
  new_termios_cfg.c_lflag &= ~(ICANON | ECHO);//端末を非カノニカルモードかつ、エコーをしないように設定
  tcsetattr(STDIN_FILENO, TCSANOW, &new_termios_cfg);//設定を適応
  file_descriptor_flg = fcntl(STDIN_FILENO, F_GETFL);//ファイルディスクリプターフラグを読み出す
  fcntl (STDIN_FILENO, F_SETFL , file_descriptor_flg | O_NONBLOCK);//入出力操作でブロックしないように設定
 
  c = getchar();
 
  tcsetattr(STDIN_FILENO, TCSANOW, &old_termios_cfg);//端末の設定を元に戻す
  fcntl(STDIN_FILENO, F_SETFL, file_descriptor_flg);//ファイルディスクリプターフラグを元に戻す
 
  if(c != EOF){
    ungetc(c, stdin);//ストリームに読み込んだ文字を押し戻す
    return c;
  }
 
  return '\0';
   
}

int kbhit(void){
  struct termios old_termios_cfg,new_termios_cfg;
  int c,file_descriptor_flg;
 
  tcgetattr(STDIN_FILENO, &old_termios_cfg);//端末の設定を取得
  new_termios_cfg = old_termios_cfg;
  new_termios_cfg.c_lflag &= ~(ICANON | ECHO);//端末を非カノニカルモードかつ、エコーをしないように設定
  tcsetattr(STDIN_FILENO, TCSANOW, &new_termios_cfg);//設定を適応
  file_descriptor_flg = fcntl(STDIN_FILENO, F_GETFL);//ファイルディスクリプターフラグを読み出す
  fcntl (STDIN_FILENO, F_SETFL , file_descriptor_flg | O_NONBLOCK);//入出力操作でブロックしないように設定
 
  c = getchar();
 
  tcsetattr(STDIN_FILENO, TCSANOW, &old_termios_cfg);//端末の設定を元に戻す
  fcntl(STDIN_FILENO, F_SETFL, file_descriptor_flg);//ファイルディスクリプターフラグを元に戻す
 
  if(c != EOF){
    ungetc(c, stdin);//ストリームに読み込んだ文字を押し戻す
    return 1;
  }
 
  return 0;
   
}



















