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

void output(char *vals)
{
    //printf("%d\n", val);
    while(*vals)
    {
        outb(*vals, BASEPORT);
        usleep(speed);
        vals++;
    }
}

//
//      16                  8 
//   64    32          128      2
//      4                   1
//
void oneUp()
{
    char upData[] = {3,2,10,8,136,128,129,1,0}; 
    output(upData);
}

void oneDown()
{
    char downData[] = {129,128,136,8,10,2,3,1,0};
    output(downData);
}

void oneLeft()
{
    char leftData[] = {36,32,48,16,80,64,68,4,0};
    output(leftData);
}

void oneRight()
{
    char rightData[] = {68,64,80,16,48,32,36,4,0};
    output(rightData);
}

// Alfi binary protocol:
// byte no:
//
// 0 - scale
// 1..x - commands
//
// Alfi binary commands
//
// 0 - write 0 to lpt and stop print
// 1..8 - directions:
//
//   8 1 2
//   7   3
//   6 5 4
void outData(char *data)
{
    int i;
    int scale = *data;
    printf("scale=%d\n", scale);
    *data++;
    for(;;)
    {
        for(i = 0; i < scale; i++)
        {
            switch(*data)
            {
                case 1:
                    oneUp();
                    break;
                case 3:
                    oneRight();
                    break;
                case 5:
                    oneDown();
                    break;
                case 7:
                    oneLeft();
                    break;
                default:
                    outb(0, BASEPORT);
                    return;
            }
        }
        data++;
    }
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
        if(key == 'a')
        {
            oneLeft();
        }
        else if(key == 'd')
        {
            oneRight();
        }
        else if(key == 'w')
        {
            oneUp();
        }
        else if(key == 's')
        {
            oneDown();
        }
        else if(key == 'p')
        {
            outData(prn);
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
        outb(0, BASEPORT);
    }
    
    fcntl(0, F_SETFL, lpFlags);
    outb(0, BASEPORT);

    if (ioperm(BASEPORT, 3, 0)) {perror("ioperm"); exit(1);}
}
