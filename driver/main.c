#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/io.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>

int getkey() {
    int character;
    struct termios orig_term_attr;
    struct termios new_term_attr;

    /* set the terminal to raw mode */
    tcgetattr(fileno(stdin), &orig_term_attr);
    memcpy(&new_term_attr, &orig_term_attr, sizeof(struct termios));
    new_term_attr.c_lflag &= ~(ECHO|ICANON);
    new_term_attr.c_cc[VTIME] = 0;
    new_term_attr.c_cc[VMIN] = 0;
    tcsetattr(fileno(stdin), TCSANOW, &new_term_attr);

    /* read a character from the stdin stream without blocking */
    /*   returns EOF (-1) if no character is available */
    character = fgetc(stdin);

    /* restore the original terminal attributes */
    tcsetattr(fileno(stdin), TCSANOW, &orig_term_attr);

    return character;
}


#define BASEPORT 0x378 /* lp1 */

int shift = 0;
int speed = 5000;

void output(int val)
{
    //printf("%d\n", val);
    outb(val, BASEPORT);
    usleep(speed);
}

#include "data.txt"

int main()
{
    int key;
    
    if (ioperm(BASEPORT, 3, 1)) {perror("ioperm");}

    int lpFlags = fcntl(0, F_GETFL, 0);
    fcntl (0, F_SETFL, (lpFlags | O_NDELAY));

    printf("p=tisknout\nm=move\n");

    for(;;)
    {
        key = getkey();
        if(key < 0)
        {
            continue;
        }
        /* terminate loop on ESC (0x1B) or Ctrl-D (0x04) on STDIN */
        if (key == 0x1B || key == 0x04) {
            break;
        }
        
        //      16                  8 
        //   64    32          128      2
        //      4                   1
        
        if(key == 'a')
        {
            output(36);
            output(32);
            output(48);
            output(16);
            output(80);
            output(64);
            output(68);
            output(4);
        }
        else if(key == 'd')
        {
            output(68);
            output(64);
            output(80);
            output(16);
            output(48);
            output(32);
            output(36);
            output(4);
        }
        else if(key == 'w')
        {
            output(3);
            output(2);
            output(10);
            output(8);
            output(136);
            output(128);
            output(129);
            output(1);
        }
        else if(key == 's')
        {
            output(129);
            output(128);
            output(136);
            output(8);
            output(10);
            output(2);
            output(3);
            output(1);
        }
        else if(key == 'p')
        {
            unsigned char *ch = prn;
            while(*ch)
            {
                output(*ch);
                ch++;
            }
            output(0);
        }
        else if(key == 'z')
        {
            speed -= 100;
            printf("speed %d\n", speed);
        }
        else if(key == 'x')
        {
            speed += 100;
            printf("speed %d\n", speed);
        }
        output(0);
    }
    
    fcntl(0, F_SETFL, lpFlags);
    outb(0, BASEPORT);

    if (ioperm(BASEPORT, 3, 0)) {perror("ioperm"); exit(1);}
}
