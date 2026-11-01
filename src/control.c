/*
  Previous - control.c

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.

  This code processes commands from the Previous control socket / FIFO.
  Based on Hatari's control.c.
*/
const char Control_fileid[] = "Previous control.c";

#include <assert.h>

#include "main.h"
#include "host.h"
#include "change.h"
#include "control.h"
#include "debugui.h"
#include "log.h"
#include "shortcut.h"
#include "str.h"
#include "timing.h"

#if HAVE_UNIX_DOMAIN_SOCKETS
# include <sys/socket.h>
# include <sys/stat.h>	/* mkfifo() */
# include <sys/un.h>
# include <fcntl.h>
# include <unistd.h>
# include <sys/select.h>
#endif

#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

/* Pausing triggered remotely */
static bool bRemotePaused;


/*-----------------------------------------------------------------------*/
/**
 * Show Previous remote usage info and return false
 */
static bool Control_Usage(const char *cmd)
{
	fprintf(stderr, "ERROR: missing arg or unrecognized command: '%s'!\n", cmd);
	fprintf(stderr,
		"Supported commands are:\n"
		"- previous-debug <Debug UI command>\n"
		"- previous-option <command line options>\n"
		"- previous-shortcut <shortcut name>\n"
		"- previous-stop\n"
		"- previous-cont\n"
		"The last two can be used to stop and continue the Previous emulation.\n"
		"All commands need to be separated by newlines.  Spaces in command\n"
		"line option arguments need to be quoted with \\.\n"
		);
	return false;
}

/*-----------------------------------------------------------------------*/
/**
 * Parse Previous debug/option/shortcut command buffer.
 */
void Control_ProcessBuffer(const char *orig)
{
	char *cmd, *cmdend, *arg, *buffer;
	int ok = true;

	/* this is called from several different places,
	 * so take a copy of the original buffer so
	 * that it can be sliced & diced
	 */
	buffer = strdup(orig);
	assert(buffer);

	cmd = buffer;
	do {
		/* command terminator? */
		cmdend  = strchr(cmd, '\n');
		if (cmdend) {
			*cmdend = '\0';
		}
		/* arguments? */
		arg = strchr(cmd, ' ');
		if (arg) {
			*arg = '\0';
			arg = Str_Trim(arg+1);
		}
		if (arg) {
			if (strcmp(cmd, "previous-option") == 0) {
				ok = Change_ApplyCommandline(arg);
			} else if (strcmp(cmd, "previous-debug") == 0) {
				ok = DebugUI_ParseLine(arg);
			} else if (strcmp(cmd, "previous-shortcut") == 0) {
				ok = Shortcut_Invoke(arg);
			} else {
				ok = Control_Usage(cmd);
			}
		} else if (*cmd) {
			if (strcmp(cmd, "previous-stop") == 0) {
				bRemotePaused = true;
			} else if (strcmp(cmd, "previous-cont") == 0) {
				bRemotePaused = false;
			} else {
				ok = Control_Usage(cmd);
			}
		}
		if (cmdend) {
			cmd = cmdend + 1;
		}
	} while (ok && cmdend && *cmd);
	free(buffer);
}


#if HAVE_UNIX_DOMAIN_SOCKETS

/* two-way socket to which Previous connects, reads control commands
 * from, and where the command responses (if any) are written to
 */
static int ControlSocket;

/* one-way fifo which Previous creates and reads commands from */
static char *FifoPath;
static int ControlFifo;


/*-----------------------------------------------------------------------*/
/**
 * Read and execute pending commands from the FIFO or socket.
 * Return false if the input was closed.
 */
static bool Control_ReadCommands(int fd, bool wait)
{
	/* setting all trace options, or paths takes a lot of space */
	char buffer[4096];
	struct timeval tv;
	fd_set readfds;
	ssize_t bytes;
	int status;

	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	tv.tv_sec = 0;
	tv.tv_usec = wait ? 100000 : 0;
	status = select(fd+1, &readfds, NULL, NULL, &tv);
	if (status < 0) {
		perror("Control socket select() error");
		return true;
	}
	if (status == 0 || !FD_ISSET(fd, &readfds)) {
		return true;
	}

	/* assume whole command can be read in one go */
	bytes = read(fd, buffer, sizeof(buffer)-1);
	if (bytes < 0) {
		perror("Control socket read error");
		return true;
	}
	if (bytes == 0) {
		return false;
	}
	buffer[bytes] = '\0';
	Control_ProcessBuffer(buffer);
	return true;
}

/*-----------------------------------------------------------------------*/
/**
 * Check ControlSocket / ControlFifo for new commands and execute them.
 * Commands should be separated by newlines.
 *
 * Called from the emulator thread. While emulation is stopped remotely
 * (previous-stop), this doesn't return until it's continued or Previous
 * quits.
 *
 * Return true if remote pause ON (and connected), false otherwise
 */
bool Control_CheckUpdates(void)
{
	bool paused = false;

	if (!ControlFifo && !ControlSocket)
		return false;

	do {
		if (ControlFifo) {
			/* EOF on a FIFO only means there's no writer at the moment */
			Control_ReadCommands(ControlFifo, bRemotePaused);
		} else if (!Control_ReadCommands(ControlSocket, bRemotePaused)) {
			fprintf(stderr, "ready control socket with 0 bytes available -> close socket\n");
			close(ControlSocket);
			ControlSocket = 0;
			bRemotePaused = false;
		}
		if (bRemotePaused && !paused) {
			Timing_Pause(true);
			paused = true;
		}
		if (bRemotePaused && ControlFifo) {
			/* a FIFO without writer makes select() return at once */
			host_sleep_ms(50);
		}
	} while (bRemotePaused && !bQuitProgram && (ControlFifo || ControlSocket));

	if (paused) {
		Timing_Pause(false);
	}
	return false;
}


/*-----------------------------------------------------------------------*/
/**
 * Close and remove FIFO file
 */
void Control_RemoveFifo(void)
{
	if (ControlFifo) {
		close(ControlFifo);
		ControlFifo = 0;
	}
	if (FifoPath) {
		Log_Printf(LOG_DEBUG, "removing command FIFO: %s\n", FifoPath);
		if (remove(FifoPath) < 0)
		{
			perror("Remove FIFO failed");
		}
		free(FifoPath);
		FifoPath = NULL;
	}
}

/*-----------------------------------------------------------------------*/
/**
 * Open given command FIFO
 * Return NULL for success, otherwise an error string
 */
const char *Control_SetFifo(const char *path)
{
	int fifo;

	if (ControlSocket) {
		return "Can't use a FIFO at the same time with a control socket";
	}

	Control_RemoveFifo();
	Log_Printf(LOG_DEBUG, "creating command FIFO: %s\n", path);

	if (mkfifo(path, S_IRUSR | S_IWUSR)) {
		perror("FIFO creation error");
		return "Can't create FIFO file";
	}
	FifoPath = strdup(path);

	fifo = open(path, O_RDONLY | O_NONBLOCK);
	if (fifo < 0) {
		perror("FIFO open error");
		Control_RemoveFifo();
		return "opening non-blocking read-only FIFO failed";
	}
	ControlFifo = fifo;
	return NULL;
}


/*-----------------------------------------------------------------------*/
/**
 * Open given control socket.
 * Return NULL for success, otherwise an error string
 */
const char *Control_SetSocket(const char *socketpath)
{
	struct sockaddr_un address;
	int newsock;

	if (ControlFifo) {
		return "Can't use a FIFO at the same time with a control socket";
	}

	newsock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (newsock < 0) {
		perror("socket creation error");
		return "Can't create AF_UNIX socket";
	}

	address.sun_family = AF_UNIX;
	strncpy(address.sun_path, socketpath, sizeof(address.sun_path));
	address.sun_path[sizeof(address.sun_path)-1] = '\0';
	Log_Printf(LOG_INFO, "Connecting to control socket '%s'...\n", address.sun_path);
	if (connect(newsock, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("socket connect error");
		close(newsock);
		return "connection to control socket failed";
	}

	if (ControlSocket) {
		close(ControlSocket);
	}
	ControlSocket = newsock;
	Log_Printf(LOG_INFO, "new control socket is '%s'\n", socketpath);
	return NULL;
}

#endif /* HAVE_UNIX_DOMAIN_SOCKETS */
