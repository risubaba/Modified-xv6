#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc,char *argvs[])
{
    int priority = atoi(argvs[2]);
    int ttime = atoi(argvs[1]);
    set_priority(priority);
    for (volatile long long int i=0;i<ttime * 10000000;i++)
    {
        i++;
        i--;
    }
    printf(1,"PBS_TEST finished\n");
    exit();
}