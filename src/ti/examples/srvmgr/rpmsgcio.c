#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/knl/Task.h>

#include <stdio.h>
#include <file.h>

static Void rpcioFxn(UArg arg0, UArg arg1);

int RPCIO_open(const char *path, unsigned flags, int llv_fd)
{
    return 0;
}

int RPCIO_close(int dev_fd)
{
    return 0;
}

int RPCIO_read(int dev_fd, char *buf, unsigned count)
{
    return 0;
}

int RPCIO_write(int dev_fd, const char *buf, unsigned count)
{
    return 0;
}

fpos_t RPCIO_lseek(int dev_fd, fpos_t offset, int origin)
{
    return 0;
}

int RPCIO_unlink(const char *path)
{
    return 0;
}

int RPCIO_rename(const char *old_name, const char *new_name)
{
    return 0;
}

void initRpmsgCio()
{
    Task_Params taskParams;

    Task_Params_init(&taskParams);
    taskParams.priority = 5;
    Task_create(rpcioFxn, &taskParams, NULL);
}

static Void rpcioFxn(UArg arg0, UArg arg1)
{
    char buf[128];

    add_device("rpcio", _MSA, RPCIO_open, RPCIO_close, RPCIO_read,
        RPCIO_write, RPCIO_lseek, RPCIO_unlink, RPCIO_rename);
    freopen("rpcio:2", "w", stdout);
    freopen("rpcio:2", "r", stdin);
    setvbuf(stdout, NULL, _IONBF, 128);
    setvbuf(stdin, NULL, _IONBF, 80);

    printf("Hello from SYS/BIOS\n");

    while (1) {
	printf("Enter something: ");
	gets(buf);
	printf("Got: %s\n", buf);
    }
}

