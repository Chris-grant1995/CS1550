#include "iso_font.h"
#include <time.h>

void init_graphics(){
  open("/dev/fb0");
  //Read and write
  ioctl(FBIOGET_VSCREENINFO,FBIOGET_FSCREENINFO);
  //Get size of the screen

  mmap(MAP_SHARED);
  //Map the size of the screen into memory

  ioctl(echo,TCGets, cannoical);
  ioctl(echo, TCSets, cannoical);
  //Disable cannoical and echo mode

  typedef unsigned short color_t;
  // First 5 bits are red, next 6 are green, final 5 are blue

}
void exit_graphics(){
  //Enable echo and canotical mode
  //TCgets and TCSets

  //Use munmap to unmap the memory

  //Close frame buffer

}
void clear_screen(){
  //write(“\033[2J”)
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
