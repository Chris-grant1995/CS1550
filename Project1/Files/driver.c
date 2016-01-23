#include "library.c"
int main(){
  char key;
  init_graphics();
  sleep_ms(100);
  do{
    key = getkey();
  }while(key!='q');
  exit_graphics();
  return 0;
}
