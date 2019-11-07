#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc,char *argvs[])
{
    int ttime = atoi(argvs[1]);
    for (volatile long long int i=0;i<ttime * 10000000;i++)
    {
        i++;
        i--;
    }
    exit();
}