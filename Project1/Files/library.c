#include "iso_font.h"
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <linux/fb.h>
#include <termios.h>


int fd;
long xres;
long yres;
long size;
char * screenMem;
void init_graphics(){

  struct fb_var_screeninfo screenInfo;
  struct fb_fix_screeninfo bitDepth;
  struct termios terminalSettings;

  fd = open("/dev/fb0", O_RDWR);
  //Read and write

  ioctl(fd, FBIOGET_VSCREENINFO, &screenInfo);
  ioctl(fd, FBIOGET_FSCREENINFO, &bitDepth);

  xres = screenInfo.xres_virtual;
  yres = screenInfo.yres_virtual;
  size = bitDepth.line_length;
  //Get size of the screen
  screenMem = (char *) mmap (null, yres*size, PROT_WRITE, MAP_SHARED, fd, 0);
  //Map the size of the screen into memory

  ioctl("/dev/tty0",TCGETS, &terminalSettings);
  terminalSettings.c_lflag &= ~ICANON;
  terminalSettings.c_lflag &= ~ECHO;
  ioctl("/dev/tty0",TCSETS, &terminalSettings);
  //Disable cannoical and echo mode

  //typedef unsigned short color_t;
  // First 5 bits are red, next 6 are green, final 5 are blue

  //Ready to go, clear the screen
  clear_screen();
}
void exit_graphics(){
  //Enable echo and canotical mode
  //TCgets and TCSets

  //Use munmap to unmap the memory

  //Close frame buffer

}
void clear_screen(){
  write(fd,"\033[2J",8);
  //Write those 8 bytes to our fd 
}
char getkey(){
  //select() to block for 0 seconds
}
void sleep_ms(long ms){
  nanosleep(ms*1000000, null);
}
void draw_pixel(int x, int y, color_t color){
  //Get appropirate position in memory for X and Y
  //cast the position to the color

}
void draw_rect(int x1, int y1, int width, int height, color_t c){
  int i = 0;
  int x = 0;
  for(i =0; i < height; i++ ){
    for(x =0; x< width; x++){
      if(i == 0 || i == height -1){

      }
      else{
        draw_pixel(x1, x, c);
        draw_pixel(width-1, x,c);
      }
    }
  }
}
void fill_rect(int x1, int y1, int width, int height, color_t c){
  int i = 0;
  int x = 0;
  for(i =0; i < height; i++ ){
    for(x =0; x< width; x++){
      draw_pixel(x,i,c);
    }
  }
}
void draw_text(int x, int y, const char *text, color_t  c){

}
