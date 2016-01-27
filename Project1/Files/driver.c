#include "library.c"
#include <stdio.h>
int main(){
  char key;
  char oldKey;
  int x = 320;
  int y = 200;
  color_t c = 16;//blue
  init_graphics();
  sleep_ms(100);
  draw_rect(2,15,20,20,0xFFFF); //Current Key Box
  draw_text(2,0,"Current Key", 0xFFFF);
  draw_char(8,15, key, 0xFFFF);//Currently Pressed Key
  do{
    fill_rect(x,y,25,25,0);
    key = getkey();
    if(key != oldKey){
      fill_rect(2,15,20,20,0);
      draw_rect(2,15,20,20,0xFFFF); //Current Key Box
      draw_text(2,0,"Current Key", 0xFFFF);//TF
      draw_char(8,15, key, 0xFFFF);//Currently Pressed Key
      oldKey=key;
    }
    if(key == 'w') y-=10;
    else if(key == 's') y+=10;
    else if(key == 'a') x-=10;
    else if(key == 'd') x+=10; // Thanks Professor Misurda
    else if(key == '1') c = 0xFFFF;
    else if(key == '2') c = 16;


    draw_rect(100,50, 538, 428, 0xFFFF);
    fill_rect(x,y, 25,25, c);


    fill_rect(125,10, 25,25, 0xFFFF);
    draw_char(137,40,'1', 0xFFFF);
    fill_rect(150,10, 25,25, 16);
    draw_char(165,40,'2', 0xFFFF);




    /*fill_rect(50,50,100,100,15);
    draw_rect(150,50,100,100,9);
    draw_char(25,25, 'A', 0xFFFF);
    draw_text(50,25,"Hello World", 0xFFFF);
    */

    sleep_ms(20);
  }while(key!='q');
  exit_graphics();

  return 0;
}
