#include "types.h"
#include "stat.h"
#include "user.h"
#include "pstat.h"

int main( int argc,char *argv[])
{
    int pid = atoi(argv[1]);
    printf(1,"Pid given for getpinfo is %d\n",pid);
    struct proc_stat p;
    if (getpinfo(&p,pid)==0)
    printf(1,"Process with given pid not found\n");
    else {
        printf(1,"Process found\n\tPid = %d\n\tRtime(inticks)= %d\n\tNum_Run= %d\n\tCurrent_queue= %d\n\tTicks = {%d,%d,%d,%d,%d}\n",
        p.pid,p.runtime,p.num_run,p.current_queue,p.ticks[0],p.ticks[1],p.ticks[2],p.ticks[3],p.ticks[4]);
    }
    exit();
}