/*
  Previous - options.c

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.

  Functions for showing and parsing all of Previous' command line options.
  Based on Hatari's options.c (same table format, parser and help output).

  To add a new option:
  - Add option ID to the enum
  - Add the option information to PreviousOptions[]
  - Add required actions for that ID to switch in Opt_ParseParameters()
*/
const char Options_fileid[] = "Previous options.c";

#include <ctype.h>
#include <assert.h>

#include "main.h"
#include "options.h"
#include "configuration.h"
#include "control.h"
#include "dialog.h"
#include "file.h"
#include "log.h"
#include "scc.h"
#include "str.h"
#include "debugui.h"

#ifdef WIN32
#include "gui-win/opencon.h"
#endif


/*  List of supported options. */
enum {
	OPT_HEADER,	/* options section header */
	OPT_HELP,		/* general options */
	OPT_VERSION,
	OPT_CONFIRMQUIT,
	OPT_CONFIGFILE,
	OPT_SAVECONFIG,
	OPT_DIALOGS,

	OPT_MACHINE,		/* system options */
	OPT_ROM,
	OPT_MEMSIZE,
	OPT_REALTIME,

	OPT_FULLSCREEN,		/* display options */
	OPT_WINDOW,
	OPT_STATUSBAR,
	OPT_SCRSHOT_DIR,

	OPT_SOUND,		/* sound options */

	OPT_BOOT,		/* boot options */

	OPT_SCSI,		/* disk options */
	OPT_SCSI_RO,
	OPT_CDROM,
	OPT_MO,
	OPT_MO_RO,
	OPT_FLOPPY,

	OPT_ETHERNET,		/* network options */
	OPT_NFS,

	OPT_SCCA_IN,		/* serial port options */
	OPT_SCCA_OUT,
	OPT_SCCB_IN,
	OPT_SCCB_OUT,

	OPT_TRACE,		/* debug options */
	OPT_TRACEFILE,
	OPT_MSG_REPEAT,
	OPT_PARSE,
	OPT_CONTROLSOCKET,
	OPT_CMDFIFO,
	OPT_LOGFILE,
	OPT_LOGLEVEL,
	OPT_ALERTLEVEL,

	OPT_TIMEOUT,		/* unattended run options */
	OPT_EXITWHEN,
	OPT_EXITON,

	OPT_ERROR,
	OPT_CONTINUE
};

typedef struct {
	unsigned int id;	/* option ID */
	const char *chr;	/* short option */
	const char *str;	/* long option */
	const char *arg;	/* type name for argument, if any */
	const char *desc;	/* option description */
} opt_t;

/* it's easier to edit these if they are kept in the same order as the enums */
static const opt_t PreviousOptions[] = {

	{ OPT_HEADER, NULL, NULL, NULL, "General" },
	{ OPT_HELP,      "-h", "--help",
	  NULL, "Print this help text and exit" },
	{ OPT_VERSION,   "-v", "--version",
	  NULL, "Print version number and exit" },
	{ OPT_CONFIRMQUIT, NULL, "--confirm-quit",
	  "<bool>", "Whether Previous confirms quit" },
	{ OPT_CONFIGFILE, "-c", "--configfile",
	  "<file>", "Read (additional) configuration values from <file>" },
	{ OPT_SAVECONFIG, NULL, "--saveconfig",
	  NULL, "Save current Previous configuration and exit" },
	{ OPT_DIALOGS, NULL, "--dialogs",
	  "<bool>", "Whether GUI dialogs may be shown (off for unattended runs)" },

	{ OPT_HEADER, NULL, NULL, NULL, "System" },
	{ OPT_MACHINE, NULL, "--machine",
	  "<x>", "Machine type (x = cube030/cube/cube-turbo/station/station-turbo/station-color/station-turbo-color)" },
	{ OPT_ROM, NULL, "--rom",
	  "<file>", "Use ROM image <file> for the selected machine type" },
	{ OPT_MEMSIZE, NULL, "--memsize",
	  "<x>", "Memory size in MiB, split over the 4 banks (multiple of 4)" },
	{ OPT_REALTIME, NULL, "--realtime",
	  "<bool>", "Use realtime mode" },

	{ OPT_HEADER, NULL, NULL, NULL, "Display" },
	{ OPT_FULLSCREEN,"-f", "--fullscreen",
	  NULL, "Start emulator in fullscreen mode" },
	{ OPT_WINDOW,    "-w", "--window",
	  NULL, "Start emulator in windowed mode" },
	{ OPT_STATUSBAR, NULL, "--statusbar",
	  "<bool>", "Show statusbar (with disk activity etc)" },
	{ OPT_SCRSHOT_DIR, NULL, "--screenshot-dir",
	  "<dir>", "Save screenshots (and printer output) in the directory <dir>" },

	{ OPT_HEADER, NULL, NULL, NULL, "Sound" },
	{ OPT_SOUND,   NULL, "--sound",
	  "<bool>", "Enable/disable sound" },

	{ OPT_HEADER, NULL, NULL, NULL, "Boot" },
	{ OPT_BOOT, NULL, "--boot",
	  "<x>", "Boot device (x = rom/scsi/ethernet/mo/floppy)" },

	{ OPT_HEADER, NULL, NULL, NULL, "Disk" },
	{ OPT_SCSI, NULL, "--scsi",
	  "<id>=<file>", "Attach hard disk image <file> as SCSI target <id> (0-6)" },
	{ OPT_SCSI_RO, NULL, "--scsi-ro",
	  "<id>=<file>", "Same as --scsi, but write protected" },
	{ OPT_CDROM, NULL, "--cdrom",
	  "<id>=<file>", "Attach CD-ROM image <file> as SCSI target <id> (0-6)" },
	{ OPT_MO, NULL, "--mo",
	  "<n>=<file>", "Insert magneto-optical disk image <file> in drive <n> (0-1)" },
	{ OPT_MO_RO, NULL, "--mo-ro",
	  "<n>=<file>", "Same as --mo, but write protected" },
	{ OPT_FLOPPY, NULL, "--floppy",
	  "<n>=<file>", "Insert floppy image <file> in drive <n> (0-1)" },

	{ OPT_HEADER, NULL, NULL, NULL, "Network" },
	{ OPT_ETHERNET, NULL, "--ethernet",
	  "<bool>", "Connect ethernet" },
	{ OPT_NFS, NULL, "--nfs",
	  "<dir>", "Share <dir> with the guest via the internal NFS server (up to 4)" },

	{ OPT_HEADER, NULL, NULL, NULL, "Serial port" },
	{ OPT_SCCA_IN, NULL, "--scc-a-in",
	  "<file>", "Enable SCC channel A and use <file> as the input" },
	{ OPT_SCCA_OUT, NULL, "--scc-a-out",
	  "<file>", "Enable SCC channel A and use <file> as the output" },
	{ OPT_SCCB_IN, NULL, "--scc-b-in",
	  "<file>", "Enable SCC channel B and use <file> as the input" },
	{ OPT_SCCB_OUT, NULL, "--scc-b-out",
	  "<file>", "Enable SCC channel B and use <file> as the output" },

	{ OPT_HEADER, NULL, NULL, NULL, "Debug" },
	{ OPT_TRACE,   NULL, "--trace",
	  "<flags>", "Activate emulation tracing, see '--trace help'" },
	{ OPT_TRACEFILE, NULL, "--trace-file",
	  "<file>", "Save trace output to <file> (default=stderr)" },
	{ OPT_MSG_REPEAT, NULL, "--msg-repeat",
	  NULL, "Toggle log/trace message repeats (default=suppress)" },
	{ OPT_PARSE, NULL, "--parse",
	  "<file>", "Parse/execute debugger commands from <file>" },
#if HAVE_UNIX_DOMAIN_SOCKETS
	{ OPT_CONTROLSOCKET, NULL, "--control-socket",
	  "<file>", "Previous connects to given socket for commands" },
	{ OPT_CMDFIFO, NULL, "--cmd-fifo",
	  "<file>", "Previous creates & reads commands from given fifo" },
#endif
	{ OPT_LOGFILE, NULL, "--log-file",
	  "<file>", "Save log output to <file> (default=stderr)" },
	{ OPT_LOGLEVEL, NULL, "--log-level",
	  "<x>", "Log output level (x=debug/todo/info/warn/error/fatal)" },
	{ OPT_ALERTLEVEL, NULL, "--alert-level",
	  "<x>", "Show dialog for log messages above given level" },

	{ OPT_HEADER, NULL, NULL, NULL, "Unattended run" },
	{ OPT_TIMEOUT, NULL, "--timeout",
	  "<x>", "Quit after <x> seconds (exit value 124)" },
	{ OPT_EXITWHEN, NULL, "--exit-when",
	  "<file>", "Quit as soon as <file> exists (exit value 0)" },
	{ OPT_EXITON, NULL, "--exit-on",
	  "<text>", "Quit as soon as the guest sends <text> on a serial port (exit value 0)" },

	{ OPT_ERROR, NULL, NULL, NULL, NULL }
};


/**
 * Show version string and license.
 */
static void Opt_ShowVersion(void)
{
#ifdef WIN32
	/* Opt_ShowVersion() is called for all info exit paths,
	 * so having this here should enable console for everything
	 * relevant on Windows.
	 */
	Win_ForceCon();
#endif
	printf("\n" PROG_NAME
	       " - the NeXT computer emulator.\n\n"
	       "Previous is free software licensed under the GNU General"
	       " Public License.\n\n");
}


/**
 * Calculate option + value len
 */
static unsigned int Opt_OptionLen(const opt_t *opt)
{
	unsigned int len;
	len = strlen(opt->str);
	if (opt->arg)
	{
		len += strlen(opt->arg);
		len += 1;
		/* with arg, short options go to another line */
	}
	else
	{
		if (opt->chr)
		{
			/* ' or -c' */
			len += 6;
		}
	}
	return len;
}


/**
 * Show single option
 */
static void Opt_ShowOption(const opt_t *opt, unsigned int maxlen)
{
	char buf[64];
	if (!maxlen)
	{
		maxlen = Opt_OptionLen(opt);
	}
	assert(maxlen < sizeof(buf));
	if (opt->arg)
	{
		sprintf(buf, "%s %s", opt->str, opt->arg);
		printf("  %-*s %s\n", maxlen, buf, opt->desc);
		if (opt->chr)
		{
			printf("    or %s %s\n", opt->chr, opt->arg);
		}
	}
	else
	{
		if (opt->chr)
		{
			sprintf(buf, "%s or %s", opt->str, opt->chr);
			printf("  %-*s %s\n", maxlen, buf, opt->desc);
		}
		else
		{
			printf("  %-*s %s\n", maxlen, opt->str, opt->desc);
		}
	}
}

/**
 * Show options for section starting from 'start_opt',
 * return next option after this section.
 */
static const opt_t *Opt_ShowHelpSection(const opt_t *start_opt)
{
	const opt_t *opt, *last;
	unsigned int len, maxlen = 0;
	const char *previous = NULL;

	/* find longest option name and check option IDs */
	for (opt = start_opt; opt->id != OPT_HEADER && opt->id != OPT_ERROR; opt++)
	{
		len = Opt_OptionLen(opt);
		if (len > maxlen)
		{
			maxlen = len;
		}
	}
	last = opt;

	/* output all options */
	for (opt = start_opt; opt != last; opt++)
	{
		if (previous != opt->str)
		{
			Opt_ShowOption(opt, maxlen);
		}
		previous = opt->str;
	}
	return last;
}


/**
 * Show help text.
 */
static void Opt_ShowHelp(void)
{
	const opt_t *opt = PreviousOptions;

	Opt_ShowVersion();
	printf("Usage:\n previous [options]\n");

	while(opt->id != OPT_ERROR)
	{
		if (opt->id == OPT_HEADER)
		{
			assert(opt->desc);
			printf("\n%s options:\n", opt->desc);
			opt++;
		}
		opt = Opt_ShowHelpSection(opt);
	}
	printf("\nSpecial option values:\n"
	       "<bool>\tDisable by using 'n', 'no', 'off', 'false', or '0'\n"
	       "\tEnable by using 'y', 'yes', 'on', 'true' or '1'\n"
	       "<file>\tSetting the file to 'none' disables given device or disk\n"
	       "\nExit values:\n"
	       "\t%d = ok, %d = bad option or configuration, %d = missing file,\n"
	       "\t%d = CPU halted, %d = --timeout reached\n",
	       QUIT_OK, QUIT_ERROR, QUIT_MISSING_FILE, QUIT_CPU_HALT, QUIT_TIMEOUT);
}


/**
 * Show Previous version and usage.
 * If 'error' given, show that error message.
 * If 'optid' != OPT_ERROR, tells for which option the error is,
 * otherwise 'value' is show as the option user gave.
 * Return false if error string was given, otherwise true
 */
static bool Opt_ShowError(unsigned int optid, const char *value, const char *error)
{
	const opt_t *opt;

	Opt_ShowVersion();
	printf("Usage:\n previous [options]\n\n"
	       "Try option \"-h\" or \"--help\" to display more information.\n");

	if (error)
	{
		if (optid == OPT_ERROR)
		{
			fprintf(stderr, "\nError: %s (%s)\n", error, value);
		}
		else
		{
			for (opt = PreviousOptions; opt->id != OPT_ERROR; opt++)
			{
				if (optid == opt->id)
					break;
			}
			if (value != NULL)
			{
				fprintf(stderr,
					"\nError while parsing argument \"%s\" for option \"%s\":\n"
					"  %s\n", value, opt->str, error);
			}
			else
			{
				fprintf(stderr, "\nError (%s): %s\n", opt->str, error);
			}
			fprintf(stderr, "\nOption usage:\n");
			Opt_ShowOption(opt, 0);
		}
		return false;
	}
	return true;
}


/**
 * If 'conf' given, set it:
 * - true if given option 'arg' is y/yes/on/true/1
 * - false if given option 'arg' is n/no/off/false/0
 * Return false for any other value, otherwise true
 */
static bool Opt_Bool(const char *arg, int optid, bool *conf)
{
	const char *enablers[] = { "y", "yes", "on", "true", "1", NULL };
	const char *disablers[] = { "n", "no", "off", "false", "0", NULL };
	const char **bool_str, *orig = arg;
	char *input, *str;

	input = strdup(arg);
	str = input;
	while (*str)
	{
		*str++ = tolower((unsigned char)*arg++);
	}
	for (bool_str = enablers; *bool_str; bool_str++)
	{
		if (strcmp(input, *bool_str) == 0)
		{
			free(input);
			if (conf)
			{
				*conf = true;
			}
			return true;
		}
	}
	for (bool_str = disablers; *bool_str; bool_str++)
	{
		if (strcmp(input, *bool_str) == 0)
		{
			free(input);
			if (conf)
			{
				*conf = false;
			}
			return true;
		}
	}
	free(input);
	return Opt_ShowError(optid, orig, "Not a <bool> value");
}


/**
 * Parse "<drive>=<value>". If single digit "<drive>" and/or '=' missing,
 * assume drive ID 0, and interpret whole arg as "<value>".
 * Return parsed "<value>", and set "<drive>".
 */
static const char *Opt_DriveValue(const char *arg, int *drive)
{
	if (strlen(arg) > 2 && isdigit((unsigned char)arg[0]) && arg[1] == '=')
	{
		*drive = arg[0] - '0';
		return arg + 2;
	}
	*drive = 0;
	return arg;
}


/**
 * matches string under given index in the argv against all Previous
 * short and long options. If match is found, returns ID for that,
 * otherwise shows help and returns OPT_ERROR.
 *
 * Checks also that if option is supposed to have argument,
 * whether there's one.
 */
static int Opt_WhichOption(int argc, const char * const argv[], int idx)
{
	const opt_t *opt;
	const char *str = argv[idx];

	for (opt = PreviousOptions; opt->id != OPT_ERROR; opt++)
	{
		/* exact option name matches? */
		if (!((opt->str && !strcmp(str, opt->str)) ||
		      (opt->chr && !strcmp(str, opt->chr))))
		{
			continue;
		}
		/* matched, check args */
		if (opt->arg)
		{
			if (idx+1 >= argc)
			{
				Opt_ShowError(opt->id, NULL, "Missing argument");
				return OPT_ERROR;
			}
			/* early check for bools */
			if (strcmp(opt->arg, "<bool>") == 0)
			{
				if (!Opt_Bool(argv[idx+1], opt->id, NULL))
				{
					return OPT_ERROR;
				}
			}
		}
		return opt->id;
	}
	Opt_ShowError(OPT_ERROR, argv[idx], "Unrecognized option");
	return OPT_ERROR;
}


typedef enum {
	CHECK_NONE,
	CHECK_FILE,
	CHECK_DIR
} fs_check_t;

/**
 * When 'check' is set, check whether given 'src' exists before copying
 * 'src' to 'dst'. Otherwise just copy option 'src' string to 'dst'.
 * If a pointer to (bool) 'option' is given, set that option to true.
 * - However, if src is "none", leave dst unmodified & set option to false.
 *   ("none" is used to disable options related to file arguments)
 * Relative paths are made absolute, since the working directory may change.
 * Return false if there were errors, otherwise true
 */
static bool Opt_StrCpy(int optid, fs_check_t check, char *dst, const char *src, size_t dstlen, bool *option)
{
	if (option)
	{
		*option = false;
		if(strcasecmp(src, "none") == 0)
		{
			return true;
		}
	}
	if (strlen(src) >= dstlen)
	{
		return Opt_ShowError(optid, src, "Path too long!");
	}

	switch (check)
	{
	case CHECK_NONE:
		break;
	case CHECK_FILE:
		if (!File_Exists(src))
		{
			return Opt_ShowError(optid, src, "Given file doesn't exist or permissions prevent access to it!");
		}
		break;
	case CHECK_DIR:
		if (!File_DirExists(src))
		{
			return Opt_ShowError(optid, src, "Given directory doesn't exist or permissions prevent access to it!");
		}
		break;
	}

	if (option)
	{
		*option = true;
	}
	strcpy(dst, src);
	if (check != CHECK_NONE || dst[0] != '/')
	{
		File_MakeAbsoluteName(dst);
	}
	return true;
}


/**
 * Select machine type, then let Configuration_SetSystemDefaults()
 * fill in the matching CPU, FPU, DSP, SCSI chip, RTC and memory.
 */
static bool Opt_SetMachine(const char *arg)
{
	static const struct {
		const char *name;
		MACHINETYPE type;
		bool turbo, color;
	} machines[] = {
		{ "cube030",             NEXT_CUBE030, false, false },
		{ "cube",                NEXT_CUBE040, false, false },
		{ "cube-turbo",          NEXT_CUBE040, true,  false },
		{ "station",             NEXT_STATION, false, false },
		{ "station-turbo",       NEXT_STATION, true,  false },
		{ "station-color",       NEXT_STATION, false, true  },
		{ "station-turbo-color", NEXT_STATION, true,  true  },
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(machines); i++)
	{
		if (strcasecmp(arg, machines[i].name) == 0)
		{
			ConfigureParams.System.nMachineType = machines[i].type;
			ConfigureParams.System.bTurbo = machines[i].turbo;
			ConfigureParams.System.bColor = machines[i].color;
			Configuration_SetSystemDefaults();
			return true;
		}
	}
	return Opt_ShowError(OPT_MACHINE, arg, "Unknown machine type");
}

/**
 * Return ROM path setting for the currently selected machine
 */
static char *Opt_RomSlot(void)
{
	if (ConfigureParams.System.nMachineType == NEXT_CUBE030)
		return ConfigureParams.Rom.szRom030FileName;
	if (ConfigureParams.System.bTurbo)
		return ConfigureParams.Rom.szRomTurboFileName;
	return ConfigureParams.Rom.szRom040FileName;
}

static bool Opt_SetScsi(int optid, const char *arg, SCSI_DEVTYPE type, bool ro)
{
	int id;
	const char *path = Opt_DriveValue(arg, &id);
	SCSIDISK *disk;

	if (id < 0 || id >= ESP_MAX_DEVS)
	{
		return Opt_ShowError(optid, arg, "Invalid SCSI target ID");
	}
	disk = &ConfigureParams.SCSI.target[id];
	if (!Opt_StrCpy(optid, CHECK_FILE, disk->szImageName, path,
	                sizeof(disk->szImageName), &disk->bDiskInserted))
	{
		return false;
	}
	disk->nDeviceType = disk->bDiskInserted ? type : SD_NONE;
	disk->bWriteProtected = ro;
	return true;
}

static bool Opt_SetMO(int optid, const char *arg, bool ro)
{
	int n;
	const char *path = Opt_DriveValue(arg, &n);
	MODISK *disk;

	if (n < 0 || n >= MO_MAX_DRIVES)
	{
		return Opt_ShowError(optid, arg, "Invalid MO drive number");
	}
	disk = &ConfigureParams.MO.drive[n];
	if (!Opt_StrCpy(optid, CHECK_FILE, disk->szImageName, path,
	                sizeof(disk->szImageName), &disk->bDiskInserted))
	{
		return false;
	}
	if (disk->bDiskInserted)
		disk->bDriveConnected = true;
	disk->bWriteProtected = ro;
	return true;
}

static bool Opt_SetFloppy(const char *arg)
{
	int n;
	const char *path = Opt_DriveValue(arg, &n);
	FLPDISK *disk;

	if (n < 0 || n >= FLP_MAX_DRIVES)
	{
		return Opt_ShowError(OPT_FLOPPY, arg, "Invalid floppy drive number");
	}
	disk = &ConfigureParams.Floppy.drive[n];
	if (!Opt_StrCpy(OPT_FLOPPY, CHECK_FILE, disk->szImageName, path,
	                sizeof(disk->szImageName), &disk->bDiskInserted))
	{
		return false;
	}
	if (disk->bDiskInserted)
		disk->bDriveConnected = true;
	return true;
}


/**
 * parse all Previous command line options and set Previous state accordingly.
 * Returns true if everything was OK, false otherwise.
 */
bool Opt_ParseParameters(int argc, const char * const argv[])
{
	const char *errstr;
	int i, ok = true;
	int val, nfs = 0;

	for(i = 1; i < argc; i++)
	{
		/* WhichOption() checks also that there is an argument,
		 * so we don't need to check that below
		 */
		switch(Opt_WhichOption(argc, argv, i))
		{

			/* general options */
		case OPT_HELP:
			Opt_ShowHelp();
			return false;

		case OPT_VERSION:
			Opt_ShowVersion();
			return false;

		case OPT_CONFIRMQUIT:
			ok = Opt_Bool(argv[++i], OPT_CONFIRMQUIT, &ConfigureParams.Log.bConfirmQuit);
			break;

		case OPT_CONFIGFILE:
			i += 1;
			/* true -> file needs to exist */
			ok = Opt_StrCpy(OPT_CONFIGFILE, CHECK_FILE, sConfigFileName,
					argv[i], sizeof(sConfigFileName), NULL);
			if (ok)
			{
				Configuration_Load(NULL);
			}
			break;

		case OPT_SAVECONFIG:
			Configuration_Save();
			exit(QUIT_OK);
			break;

		case OPT_DIALOGS:
			ok = Opt_Bool(argv[++i], OPT_DIALOGS, &bShowDialogs);
			break;

			/* system options */
		case OPT_MACHINE:
			ok = Opt_SetMachine(argv[++i]);
			break;

		case OPT_ROM:
			i += 1;
			ok = Opt_StrCpy(OPT_ROM, CHECK_FILE, Opt_RomSlot(),
					argv[i], FILENAME_MAX, NULL);
			break;

		case OPT_MEMSIZE:
			val = atoi(argv[++i]);
			if (val <= 0 || val % 4)
			{
				return Opt_ShowError(OPT_MEMSIZE, argv[i], "Memory size must be a positive multiple of 4");
			}
			for (int b = 0; b < 4; b++)
			{
				ConfigureParams.Memory.nMemoryBankSize[b] = val / 4;
			}
			break;

		case OPT_REALTIME:
			ok = Opt_Bool(argv[++i], OPT_REALTIME, &ConfigureParams.System.bRealtime);
			break;

			/* display options */
		case OPT_FULLSCREEN:
			ConfigureParams.Screen.bFullScreen = true;
			break;

		case OPT_WINDOW:
			ConfigureParams.Screen.bFullScreen = false;
			break;

		case OPT_STATUSBAR:
			ok = Opt_Bool(argv[++i], OPT_STATUSBAR, &ConfigureParams.Screen.bShowStatusbar);
			break;

		case OPT_SCRSHOT_DIR:
			i += 1;
			/* Previous saves screenshots next to the printer output */
			ok = Opt_StrCpy(OPT_SCRSHOT_DIR, CHECK_DIR, ConfigureParams.Printer.szPrintToFileName,
					argv[i], sizeof(ConfigureParams.Printer.szPrintToFileName), NULL);
			break;

			/* sound options */
		case OPT_SOUND:
			ok = Opt_Bool(argv[++i], OPT_SOUND, &ConfigureParams.Sound.bEnableSound);
			break;

			/* boot options */
		case OPT_BOOT:
			i += 1;
			if      (!strcasecmp(argv[i], "rom"))      ConfigureParams.Boot.nBootDevice = BOOT_ROM;
			else if (!strcasecmp(argv[i], "scsi"))     ConfigureParams.Boot.nBootDevice = BOOT_SCSI;
			else if (!strcasecmp(argv[i], "ethernet")) ConfigureParams.Boot.nBootDevice = BOOT_ETHERNET;
			else if (!strcasecmp(argv[i], "mo"))       ConfigureParams.Boot.nBootDevice = BOOT_MO;
			else if (!strcasecmp(argv[i], "floppy"))   ConfigureParams.Boot.nBootDevice = BOOT_FLOPPY;
			else return Opt_ShowError(OPT_BOOT, argv[i], "Unknown boot device");
			break;

			/* disk options */
		case OPT_SCSI:
			ok = Opt_SetScsi(OPT_SCSI, argv[++i], SD_HARDDISK, false);
			break;

		case OPT_SCSI_RO:
			ok = Opt_SetScsi(OPT_SCSI_RO, argv[++i], SD_HARDDISK, true);
			break;

		case OPT_CDROM:
			ok = Opt_SetScsi(OPT_CDROM, argv[++i], SD_CD, true);
			break;

		case OPT_MO:
			ok = Opt_SetMO(OPT_MO, argv[++i], false);
			break;

		case OPT_MO_RO:
			ok = Opt_SetMO(OPT_MO_RO, argv[++i], true);
			break;

		case OPT_FLOPPY:
			ok = Opt_SetFloppy(argv[++i]);
			break;

			/* network options */
		case OPT_ETHERNET:
			ok = Opt_Bool(argv[++i], OPT_ETHERNET, &ConfigureParams.Ethernet.bEthernetConnected);
			break;

		case OPT_NFS:
			i += 1;
			if (nfs >= EN_MAX_SHARES)
			{
				return Opt_ShowError(OPT_NFS, argv[i], "Too many NFS shares");
			}
			ok = Opt_StrCpy(OPT_NFS, CHECK_DIR, ConfigureParams.Ethernet.nfs[nfs].szPathName,
					argv[i], sizeof(ConfigureParams.Ethernet.nfs[nfs].szPathName), NULL);
			if (ok)
			{
				nfs++;
				ConfigureParams.Ethernet.bEthernetConnected = true;
			}
			break;

			/* serial port options */
		case OPT_SCCA_IN:
			i += 1;
			ok = Opt_StrCpy(OPT_SCCA_IN, CHECK_FILE, ConfigureParams.Serial.SccInFileName[0],
					argv[i], sizeof(ConfigureParams.Serial.SccInFileName[0]),
					&ConfigureParams.Serial.EnableScc[0]);
			break;

		case OPT_SCCA_OUT:
			i += 1;
			ok = Opt_StrCpy(OPT_SCCA_OUT, CHECK_NONE, ConfigureParams.Serial.SccOutFileName[0],
					argv[i], sizeof(ConfigureParams.Serial.SccOutFileName[0]),
					&ConfigureParams.Serial.EnableScc[0]);
			break;

		case OPT_SCCB_IN:
			i += 1;
			ok = Opt_StrCpy(OPT_SCCB_IN, CHECK_FILE, ConfigureParams.Serial.SccInFileName[1],
					argv[i], sizeof(ConfigureParams.Serial.SccInFileName[1]),
					&ConfigureParams.Serial.EnableScc[1]);
			break;

		case OPT_SCCB_OUT:
			i += 1;
			ok = Opt_StrCpy(OPT_SCCB_OUT, CHECK_NONE, ConfigureParams.Serial.SccOutFileName[1],
					argv[i], sizeof(ConfigureParams.Serial.SccOutFileName[1]),
					&ConfigureParams.Serial.EnableScc[1]);
			break;

			/* debug options */
		case OPT_TRACE:
			i += 1;
			errstr = Log_SetTraceOptions(argv[i]);
			if (errstr)
			{
				if (!errstr[0])
				{
					/* silent parsing termination */
					return false;
				}
				return Opt_ShowError(OPT_TRACE, argv[i], errstr);
			}
			break;

		case OPT_TRACEFILE:
			i += 1;
			ok = Opt_StrCpy(OPT_TRACEFILE, CHECK_NONE, ConfigureParams.Log.sTraceFileName,
					argv[i], sizeof(ConfigureParams.Log.sTraceFileName),
					NULL);
			break;

		case OPT_MSG_REPEAT:
			Log_ToggleMsgRepeat();
			break;

		case OPT_PARSE:
			i += 1;
			ok = DebugUI_AddParseFile(argv[i]);
			break;

		case OPT_CONTROLSOCKET:
			i += 1;
			errstr = Control_SetSocket(argv[i]);
			if (errstr)
			{
				return Opt_ShowError(OPT_CONTROLSOCKET, argv[i], errstr);
			}
			break;

		case OPT_CMDFIFO:
			i += 1;
			errstr = Control_SetFifo(argv[i]);
			if (errstr)
			{
				return Opt_ShowError(OPT_CMDFIFO, argv[i], errstr);
			}
			break;

		case OPT_LOGFILE:
			i += 1;
			ok = Opt_StrCpy(OPT_LOGFILE, CHECK_NONE, ConfigureParams.Log.sLogFileName,
					argv[i], sizeof(ConfigureParams.Log.sLogFileName),
					NULL);
			break;

		case OPT_LOGLEVEL:
			i += 1;
			ConfigureParams.Log.nTextLogLevel = Log_ParseOptions(argv[i]);
			if (ConfigureParams.Log.nTextLogLevel == LOG_NONE)
			{
				return Opt_ShowError(OPT_LOGLEVEL, argv[i], "Unknown log level!");
			}
			Log_SetLevels();
			break;

		case OPT_ALERTLEVEL:
			i += 1;
			ConfigureParams.Log.nAlertDlgLogLevel = Log_ParseOptions(argv[i]);
			if (ConfigureParams.Log.nAlertDlgLogLevel == LOG_NONE)
			{
				return Opt_ShowError(OPT_ALERTLEVEL, argv[i], "Unknown alert level!");
			}
			Log_SetLevels();
			break;

			/* unattended run options */
		case OPT_TIMEOUT:
			val = atoi(argv[++i]);
			if (val <= 0)
			{
				return Opt_ShowError(OPT_TIMEOUT, argv[i], "Timeout must be a positive number of seconds");
			}
			Main_SetRunTimeout(val);
			break;

		case OPT_EXITWHEN:
			i += 1;
			{
				char path[FILENAME_MAX];
				ok = Opt_StrCpy(OPT_EXITWHEN, CHECK_NONE, path, argv[i], sizeof(path), NULL);
				if (ok)
					Main_SetExitFile(path);
			}
			break;

		case OPT_EXITON:
			i += 1;
			if (!argv[i][0])
			{
				return Opt_ShowError(OPT_EXITON, argv[i], "Text must not be empty");
			}
			SCC_SetExitText(argv[i]);
			break;

		case OPT_ERROR:
			/* unknown option or missing option parameter */
			return false;

		default:
			return Opt_ShowError(OPT_ERROR, argv[i], "Internal Previous error, unhandled option");
		}
		if (!ok)
		{
			/* Opt_Bool() or Opt_StrCpy() failed */
			return false;
		}
	}

	return true;
}
