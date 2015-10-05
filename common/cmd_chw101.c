#include <common.h>
#include <command.h>

int do_chw101 (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	setenv("board","linino-chiwawa ext=101");
	setenv("console","ttyATH0,250000");
	setenv("baudrate","250000");
	saveenv();
	printf("Change to chiwawa101 env success\n");

}

U_BOOT_CMD(
	chw101,	CFG_MAXARGS,	1,	do_chw101,
NULL,
NULL
);
