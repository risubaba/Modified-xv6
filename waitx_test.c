#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc,char* argv[])
{
    int pid;
    int status = 0, a=0, b=0;
    pid = fork();
    status = 0;
    if (pid == 0)
    {
        // for (int i = 0; i < 100; i++)
            // for (int j = 0; j < 100; j++)
                // for (int k = 0; k < 100; k++)
                    // status = 1 - status;
        exec(argv[1],argv);
        printf(1,"Exec Failed\n");
    }
    else
    {
        status=wait();
        // status = waitx(&a, &b);
    }
    printf(1, "Wait Time = %d\n Run Time = %d with Status %d \n", a, b, status);
    exit();
}
