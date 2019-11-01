#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
    int pid;
    int status = 0, a = 0, b = 0;
    for (int j = 0; j < 10; j++)
    {
        pid = fork();
        status = 0;
        if (pid == 0)
        {
            for (volatile int i = 0; i < 100000000; i++)
                status = 1 ^ status;
            exit();
        }
        else
        {
            status = waitx(&a, &b);
        }
        printf(1, "Wait Time = %d\n Run Time = %d with Status %d \n", a, b, status);
    }
    exit();
}
