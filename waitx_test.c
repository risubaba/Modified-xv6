#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
    int status = 0;
    // int a = 0, b = 0;
    // int par_pid = 0;

    if (fork() == 0)

    {
        for (int j = 0; j < 10; j++)
        {
            int pid;
            pid = fork();
            status = 0;
            if (pid == 0)
            {
                set_priority(j * 10 + 1);
                for (volatile int i = 0; i < 1000000000; i++)
                    status = 1 ^ status;
                exit();
            }
            // else
            // {
            // status = waitx(&a, &b);
            // }
            // printf(1, "Wait Time = %d\n Run Time = %d with Status %d \n", a, b, status);
        }
        for (int i = 0; i < 10; i++)
        {
            wait();
            printf(1, "Prev fork finished\n");
        }
    }
    // printf(1, "Exiting waitx_test\n");
    exit();
}
