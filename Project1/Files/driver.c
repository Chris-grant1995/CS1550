#include "library.c"
#include <stdio.h>
int main(){
  char key;
  init_graphics();
  sleep_ms(100);
  do{
    key = getkey();
    fill_rect(50,50,100,100,15);
    draw_rect(150,50,100,100,9);
    draw_char(25,25, 'A', 0xFFFF);
    draw_text(50,25,"Hello World", 0xFFFF);
  }while(key!='q');
  exit_graphics();

  return 0;
}
