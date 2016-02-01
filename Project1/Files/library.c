#include "iso_font.h"
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <linux/fb.h>
#include <termios.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

typedef unsigned short color_t;
int fd;
long xres;
long yres;
long size;
unsigned short * screenMem;
void clear_screen(){
  write(1,"\033[2J",8);
  //Write those 8 bytes to our fd
}
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
  screenMem = (unsigned short *) mmap (NULL, yres*size, PROT_WRITE, MAP_SHARED, fd, 0);
  //Map the size of the screen into memory

  ioctl(STDIN_FILENO,TCGETS, &terminalSettings);
  terminalSettings.c_lflag &= ~ICANON;
  terminalSettings.c_lflag &= ~ECHO;
  ioctl(STDIN_FILENO,TCSETS, &terminalSettings);
  //Disable cannoical and echo mode
  //typedef unsigned short color_t;
  // First 5 bits are red, next 6 are green, final 5 are blue

  //Ready to go, clear the screen
  clear_screen();
}
void exit_graphics(){
  clear_screen();
  struct termios terminalSettings;
  ioctl(STDIN_FILENO,TCGETS, &terminalSettings);
  terminalSettings.c_lflag |= ICANON;
  terminalSettings.c_lflag |= ECHO;
  ioctl(STDIN_FILENO,TCSETS, &terminalSettings);
  //Enable echo and canotical mode
  munmap(screenMem, yres*size);
  //Use munmap to unmap the memory
  close(fd);
  //Close frame buffer

}

char getkey(){
  fd_set fdset;
	struct timeval timeout;
	int pressed;
	char ch;
	FD_ZERO(&fdset);
	FD_SET( STDIN_FILENO, &fdset );
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	pressed = select( STDIN_FILENO+1, &fdset, NULL, NULL, &timeout );
	if( pressed > 0 )
	{
		read( 0, &ch, sizeof(ch) );
	}
	return ch;

}
void sleep_ms(long ms){
  struct timespec timer;
  timer.tv_sec =0;
  timer.tv_nsec = ms*1000000;
  nanosleep(&timer, NULL);
}
void draw_pixel(int x, int y, color_t color){
  //Get appropirate position in memory for X and Y
  //cast the position to the color
  if(x < 0 || x >= xres)
	{
		return;
	}
  if(y < 0 || y >= yres)
  {
    return;
  }
 	unsigned long v = (size/2) * y;
	unsigned long h = x;
	unsigned short *s = (screenMem + v + h);
	*s = color;


}
void draw_rect(int x1, int y1, int width, int height, color_t c){
  int vert =0;
  int hori = 0;
  for(hori = x1; hori < width+x1; hori++){
    draw_pixel(hori,y1,c);
    draw_pixel(hori,y1+height,c);
  }
  for(vert =y1; vert<height+y1; vert++){
    draw_pixel(x1, vert, c);
    draw_pixel(x1+width, vert,c);
  }
}
void fill_rect(int x1, int y1, int width, int height, color_t c){
  int i = 0;
  int x = 0;
  for(i =x1; i < height+x1; i++ ){
    for(x =y1; x< width+y1; x++){
      draw_pixel(i,x,c);
    }

  }
}
void draw_char(int x, int y, char ch, color_t c){
  int letter = (int) ch;
  int i=0;
  int j=0;
  for( i =0; i<16; i++ ){
    unsigned char row = iso_font[letter*16+i];
    for( j =0; j<8; j++){
      if((row >> (j))& 1 == 1){
        draw_pixel(x+j, y+i, c);
      }

    }
  }
}
void draw_text(int x, int y, const char *text, color_t  c){
  int counter =0;
  while(text[counter] != '\0'){
    draw_char(x,y,text[counter], c);
    x+=8;
    counter++;
  }
}
