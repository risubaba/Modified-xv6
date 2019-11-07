#include "types.h"
#include "stat.h"
#include "user.h"
#include "pstat.h"

int main(int argc, char *argvs[])
{
    // int pid = getpid();
    int priority = 1;
    for (volatile int i = 0; i < 10; i++)
    {
        int pid2 = 0;
        priority++;
        pid2 = fork();
        if (pid2 == 0)
        {
            set_priority(100 - (i));
            for (volatile int j = 0; j < i; j++)
            {
                sleep(2000);
            }
            for (volatile int j = i + 1; j < 10; j++)
            {
                for (volatile int k = 0; k < 100000000; k++)
                {
                    k--;
                    k++;
                }
            }
            exit();
        }
    }
    for (int volatile l = 0; l < 10; l++)
    {
        wait();
        printf(1, "Process over\n");
    }
    exit();
}