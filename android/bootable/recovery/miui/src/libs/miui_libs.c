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
 * Main MIUI Installer Library / Common Functions
 *
 */

#include <sys/times.h>
#include <sys/statfs.h>
#include <dirent.h>
#include <sys/stat.h>
#include "../miui_inter.h"

//-- TICK TIME
long alib_tick(){
  struct tms tm;
  return times(&tm);
}

int * ai_rtrimw(int * chr,int len){
  int * res = chr;
  int i;
  for (i=len-1;i>=0;i--){
    if ((res[i]==' ')||(res[i]=='\n')||(res[i]=='\r')||(res[i]=='\t')){
      res[i]=0;
    }
    else break;
  }
  return res;
}
char * ai_rtrim(char * chr){
  char * res = chr;
  int i;
  for (i=strlen(res)-1;i>=0;i--){
    if ((res[i]==' ')||(res[i]=='\n')||(res[i]=='\r')||(res[i]=='\t')){
      res[i]=0;
    }
    else break;
  }
  return res;
}
char * ai_trim(char * chr){
  char * res = chr;
  char   off = 0;
  while ((off=*res)){
    byte nobreak = 0;
    switch (off){
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        nobreak = 1;
      break;
    }
    if (!nobreak) break;
    res++;
  }
  int i;
  for (i=strlen(res)-1;i>=0;i--){
    if ((res[i]==' ')||(res[i]=='\n')||(res[i]=='\r')||(res[i]=='\t')){
      res[i]=0;
    }
    else break;
  }
  return res;
}
byte ismounted(char * path){
  byte res=0;
  FILE* fp = fopen("/proc/mounts", "rb");
  if (fp){
    int c=EOF;
    do{
      c=0;
      do{
        c=fgetc(fp);
        if (c==EOF) goto done;
        else if (isspace(c)) break;
      }while(c!=EOF);
      
      char p[256];
      memset(p,0,256);
      int pl=0;
      do{
        c=fgetc(fp);
        if (c==EOF) goto done;
        else if (isspace(c)) break;
        p[pl++] = c;
      }while(c!=EOF);
      p[pl++] = 0;
      if (strcmp(p,path)==0){
        res=1;
        goto done;
      }
      do{
        c=fgetc(fp);
        if (c==EOF) goto done;
        else if (c=='\n') break;
      }while(c!=EOF);
    }
    while(c!=EOF);
    done:
      fclose(fp);
  }
  return res;
}
void create_directory(const char *path){
  mkdir(path,0777);
}
int remove_directory(const char *path)
{
   DIR *d = opendir(path);
   size_t path_len = strlen(path);
   int r = -1;
   if (d)
   {
      struct dirent *p;
      r = 0;
      while (!r && (p=readdir(d)))
      {
          int r2 = -1;
          char *buf;
          size_t len;
          if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
          {
             continue;
          }
          len = path_len + strlen(p->d_name) + 2; 
          buf = malloc(len);
          if (buf)
          {
             struct stat statbuf;
             snprintf(buf, len, "%s/%s", path, p->d_name);
             if (!stat(buf, &statbuf))
             {
                if (S_ISDIR(statbuf.st_mode))
                {
                   r2 = remove_directory(buf);
                }
                else
                {
                   r2 = unlink(buf);
                }
             }
             free(buf);
          }
          r = r2;
      }
      closedir(d);
   }
   if (!r)
   {
      r = rmdir(path);
   }
   return r;
}
//-- GET DISK USAGE
int alib_diskusage(const char * path){
  struct statfs fiData;
  if((statfs(path,&fiData))<0) {
    return -1;
  } else {
    int perc = round(( ((float) fiData.f_bfree) / ((float) fiData.f_blocks) ) * 100);
    return 100-perc;
  } 
}
byte alib_disksize(const char * path, unsigned long * ret, int division){
  struct statfs fiData;
  if((statfs(path,&fiData))<0) {
    return 0;
  } else {
    if (ret!=NULL){
      double block = ((double) fiData.f_blocks) / division;
      double sizek = block * fiData.f_bsize;

      if (block == (sizek/fiData.f_bsize))
        ret[0] = round(sizek);
      else
        return 0;
    }
    return 1;
  }
}
byte alib_diskfree(const char * path, unsigned long * ret, int division){
  struct statfs fiData;
  if((statfs(path,&fiData))<0) {
    return 0;
  } else {
    if (ret!=NULL){
      double block = ((double) fiData.f_bfree) / division;
      double sizek = block * fiData.f_bsize;

      if (block == (sizek/fiData.f_bsize))
        ret[0] = round(sizek);
      else
        return 0;
    }
    return 1;
  }
}
void alib_exec(char * cmd, char * arg){
  char** args2 = malloc(sizeof(char*) * 3);
  args2[0]    = cmd;
  args2[1]    = arg;
  args2[2]    = NULL;
  int pipefd[2];
  pipe(pipefd);
  
  pid_t pid = fork();
  if (pid == 0) {
      close(pipefd[0]);
      execv(args2[0], args2);
      _exit(-1);
  }
  close(pipefd[1]);
  char  buffer[16];
  FILE* from_child = fdopen(pipefd[0], "r");
  while (fgets(buffer, sizeof(buffer), from_child) != NULL) {}
  fclose(from_child);
  free(args2);
}
//-- KINETIC CALCULATOR
void akinetic_downhandler(AKINETIC * p, int mouseY){
  p->isdown            = 1;
  p->velocity          = 0;
  p->history_n         = 1;
  p->previousPoints[0] = mouseY;
  p->previousTimes[0]  = alib_tick();
}
int akinetic_movehandler(AKINETIC * p, int mouseY){
  if (!p->isdown) return 0;
  int   currPoint       = mouseY;
  long  currTime        = alib_tick();
  int   previousPoint   = p->previousPoints[p->history_n-1];
  int   diff            = previousPoint-currPoint;
  
  p->history_n++;
  if (p->history_n>AKINETIC_HISTORY_LENGTH){
    int i;
    for (i=1;i<AKINETIC_HISTORY_LENGTH;i++){
      p->previousPoints[i-1]=p->previousPoints[i];
      p->previousTimes[i-1]=p->previousTimes[i];
    }
    p->history_n--;
  }
  p->previousPoints[p->history_n-1]  = currPoint;
  p->previousTimes[p->history_n-1]   = currTime;
  
  return diff;
}
byte akinetic_uphandler(AKINETIC * p, int mouseY){
  if (!p->isdown) return 0;
  p->isdown             = 0;
  int   currPoint       = (mouseY==0)?p->previousPoints[p->history_n-1]:mouseY;
  long  currTime        = alib_tick();
  int   firstPoint      = p->previousPoints[0];
  long  firstTime       = p->previousTimes[0];
  
  if (currTime-firstTime<1) firstTime--;
  if (currTime-firstTime>25) return 0;
  int   diff            = firstPoint-currPoint;
  long  time            = (currTime - firstTime);
  p->velocity           = ((double) diff/(double) time)*4;
  
  return 1;
}
int akinetic_fling(AKINETIC * p){
  p->velocity = p->velocity * AKINETIC_DAMPERING;
  if (abs(p->velocity)<0.1){
    return 0;
  }
  return ceil(p->velocity);
}
int akinetic_fling_dampered(AKINETIC * p, float dampersz){
  p->velocity = p->velocity * dampersz;
  if (abs(p->velocity)<0.1){
    return 0;
  }
  return ceil(p->velocity);
}

//#define MAX_FILE_SIZE 65536
byte az_readmem(AZMEM *out, const char *name, byte bytesafe){

  struct stat st;
  return_val_if_fail(out != NULL, 0);
  return_val_if_fail(name != NULL, 0);
  if (stat(name, &st) < 0)
  {
      miui_printf("[%s]:%s is not exist.\n", __FUNCTION__, name);
      return 0;
  }
  out->sz = st.st_size + (bytesafe?0:1);
  out->data = malloc(out->sz);
  if (out->data == NULL) goto done;
  FILE* f = fopen(name, "rb");
  if (f == NULL) goto done;
  if (fread(out->data, 1, st.st_size, f) != st.st_size){
      fclose(f);
      goto done;
  }
  if(!bytesafe) out->data[st.st_size] = '\0';
  fclose(f);
  return 1;
done:
  free(out->data);
  return 0;
}
