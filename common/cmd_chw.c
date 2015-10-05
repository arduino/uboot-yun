#include <common.h>
#include <command.h>

int do_chw (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	setenv("board","linino-chiwawa");
	setenv("console","ttyATH0,115200");
	setenv("baudrate","115200");
	saveenv();
	printf("Change to chiwawa env success\n");
}

U_BOOT_CMD(
	chw,	CFG_MAXARGS,	1,	do_chw,
NULL,
NULL
);
