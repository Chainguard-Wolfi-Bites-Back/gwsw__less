/*
 * Copyright (c) 1984,1985,1989,1994,1995,1996  Mark Nudelman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice in the documentation and/or other materials provided with 
 *    the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN 
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * Routines which deal with the characteristics of the terminal.
 * Uses termcap to be as terminal-independent as possible.
 */

#include "less.h"
#include "cmd.h"

#if MSDOS_COMPILER
#include "pckeys.h"
#if MSDOS_COMPILER==MSOFTC
#include <graph.h>
#else
#if MSDOS_COMPILER==BORLANDC || MSDOS_COMPILER==DJGPPC
#include <conio.h>
#if MSDOS_COMPILER==DJGPPC
#include <pc.h>
#endif
#else
#if MSDOS_COMPILER==WIN32C
#include <windows.h>
#endif
#endif
#endif
#include <time.h>

#else

#if HAVE_TERMIOS_H && HAVE_TERMIOS_FUNCS
#include <termios.h>
#if HAVE_SYS_IOCTL_H && !defined(TIOCGWINSZ)
#include <sys/ioctl.h>
#endif
#else
#if HAVE_TERMIO_H
#include <termio.h>
#else
#if HAVE_SGSTAT_H
#include <sgstat.h>
#else
#include <sgtty.h>
#endif
#if HAVE_SYS_IOCTL_H && (defined(TIOCGWINSZ) || defined(TCGETA) || defined(TIOCGETP) || defined(WIOCGETD))
#include <sys/ioctl.h>
#endif
#endif
#endif

#if HAVE_TERMCAP_H
#include <termcap.h>
#endif
#ifdef _OSK
#include <signal.h>
#endif
#if OS2
#include <sys/signal.h>
#endif
#if HAVE_SYS_WINDOW_H
#include <sys/window.h>
#endif
#if HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
#if HAVE_SYS_PTEM_H
#include <sys/ptem.h>
#endif

#endif /* MSDOS_COMPILER */

/*
 * Check for broken termios package that forces you to manually
 * set the line discipline.
 */
#ifdef __ultrix__
#define MUST_SET_LINE_DISCIPLINE 1
#else
#define MUST_SET_LINE_DISCIPLINE 0
#endif

#if OS2
#define	DEFAULT_TERM		"ansi"
#else
#define	DEFAULT_TERM		"unknown"
#endif

#if MSDOS_COMPILER==MSOFTC
static int videopages;
static long msec_loops;
#define	SETCOLORS(fg,bg)	_settextcolor(fg); _setbkcolor(bg);
#endif

#if MSDOS_COMPILER==BORLANDC || MSDOS_COMPILER==DJGPPC
static unsigned short *whitescreen;
#define _settextposition(y,x)   gotoxy(x,y)
#define _clearscreen(m)         clrscr()
#define _outtext(s)             cputs(s)
#define	SETCOLORS(fg,bg)	textcolor(fg); textbackground(bg);
#endif

#if MSDOS_COMPILER==WIN32C
struct keyRecord
{
	int ascii;
	int scan;
} currentKey;

static int keyCount = 0;
static WORD curr_attr;
static int pending_scancode = 0;
static WORD *whitescreen;
static constant COORD TOPLEFT = {0, 0};
#define FG_COLORS       (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define BG_COLORS       (BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY)
HANDLE con_out;
#define	MAKEATTR(fg,bg)		((WORD)((fg)|((bg)<<4)))
#define	SETCOLORS(fg,bg)	curr_attr = MAKEATTR(fg,bg); \
				SetConsoleTextAttribute(con_out, curr_attr);
#endif

#if MSDOS_COMPILER
public int nm_fg_color;		/* Color of normal text */
public int nm_bg_color;
public int bo_fg_color;		/* Color of bold text */
public int bo_bg_color;
public int ul_fg_color;		/* Color of underlined text */
public int ul_bg_color;
public int so_fg_color;		/* Color of standout text */
public int so_bg_color;
public int bl_fg_color;		/* Color of blinking text */
public int bl_bg_color;
static int sy_fg_color;		/* Color of system text (before less) */
static int sy_bg_color;

static int flash_created = 0;

#else

/*
 * Strings passed to tputs() to do various terminal functions.
 */
static char
	*sc_pad,		/* Pad string */
	*sc_home,		/* Cursor home */
	*sc_addline,		/* Add line, scroll down following lines */
	*sc_lower_left,		/* Cursor to last line, first column */
	*sc_move,		/* General cursor positioning */
	*sc_clear,		/* Clear screen */
	*sc_eol_clear,		/* Clear to end of line */
	*sc_eos_clear,		/* Clear to end of screen */
	*sc_s_in,		/* Enter standout (highlighted) mode */
	*sc_s_out,		/* Exit standout mode */
	*sc_u_in,		/* Enter underline mode */
	*sc_u_out,		/* Exit underline mode */
	*sc_b_in,		/* Enter bold mode */
	*sc_b_out,		/* Exit bold mode */
	*sc_bl_in,		/* Enter blink mode */
	*sc_bl_out,		/* Exit blink mode */
	*sc_visual_bell,	/* Visual bell (flash screen) sequence */
	*sc_backspace,		/* Backspace cursor */
	*sc_s_keypad,		/* Start keypad mode */
	*sc_e_keypad,		/* End keypad mode */
	*sc_init,		/* Startup terminal initialization */
	*sc_deinit;		/* Exit terminal de-initialization */
#endif

static int init_done = 0;

public int auto_wrap;		/* Terminal does \r\n when write past margin */
public int ignaw;		/* Terminal ignores \n immediately after wrap */
public int erase_char, kill_char; /* The user's erase and line-kill chars */
public int werase_char;		/* The user's word-erase char */
public int sc_width, sc_height;	/* Height & width of screen */
public int bo_s_width, bo_e_width;	/* Printing width of boldface seq */
public int ul_s_width, ul_e_width;	/* Printing width of underline seq */
public int so_s_width, so_e_width;	/* Printing width of standout seq */
public int bl_s_width, bl_e_width;	/* Printing width of blink seq */
public int above_mem, below_mem;	/* Memory retained above/below screen */
public int can_goto_line;		/* Can move cursor to any line */
public int missing_cap = 0;	/* Some capability is missing */

static char *cheaper();
static void tmodes();

/*
 * These two variables are sometimes defined in,
 * and needed by, the termcap library.
 */
#if MUST_DEFINE_OSPEED
extern short ospeed;	/* Terminal output baud rate */
extern char PC;		/* Pad character */
#endif
#ifdef _OSK
short ospeed;
char PC_, *UP, *BC;
#endif

extern int quiet;		/* If VERY_QUIET, use visual bell for bell */
extern int no_back_scroll;
extern int swindow;
extern int no_init;
#if HILITE_SEARCH
extern int hilite_search;
#endif

extern char *tgetstr();
extern char *tgoto();


/*
 * Change terminal to "raw mode", or restore to "normal" mode.
 * "Raw mode" means 
 *	1. An outstanding read will complete on receipt of a single keystroke.
 *	2. Input is not echoed.  
 *	3. On output, \n is mapped to \r\n.
 *	4. \t is NOT expanded into spaces.
 *	5. Signal-causing characters such as ctrl-C (interrupt),
 *	   etc. are NOT disabled.
 * It doesn't matter whether an input \n is mapped to \r, or vice versa.
 */
	public void
raw_mode(on)
	int on;
{
	static int curr_on = 0;

	if (on == curr_on)
		return;
#if HAVE_TERMIOS_H && HAVE_TERMIOS_FUNCS
    {
	struct termios s;
	static struct termios save_term;
	static int saved_term = 0;

	if (on) 
	{
		/*
		 * Get terminal modes.
		 */
		tcgetattr(2, &s);

		/*
		 * Save modes and set certain variables dependent on modes.
		 */
		if (!saved_term)
		{
			save_term = s;
			saved_term = 1;
		}
#if HAVE_OSPEED
		switch (cfgetospeed(&s))
		{
#ifdef B0
		case B0: ospeed = 0; break;
#endif
#ifdef B50
		case B50: ospeed = 1; break;
#endif
#ifdef B75
		case B75: ospeed = 2; break;
#endif
#ifdef B110
		case B110: ospeed = 3; break;
#endif
#ifdef B134
		case B134: ospeed = 4; break;
#endif
#ifdef B150
		case B150: ospeed = 5; break;
#endif
#ifdef B200
		case B200: ospeed = 6; break;
#endif
#ifdef B300
		case B300: ospeed = 7; break;
#endif
#ifdef B600
		case B600: ospeed = 8; break;
#endif
#ifdef B1200
		case B1200: ospeed = 9; break;
#endif
#ifdef B1800
		case B1800: ospeed = 10; break;
#endif
#ifdef B2400
		case B2400: ospeed = 11; break;
#endif
#ifdef B4800
		case B4800: ospeed = 12; break;
#endif
#ifdef B9600
		case B9600: ospeed = 13; break;
#endif
#ifdef EXTA
		case EXTA: ospeed = 14; break;
#endif
#ifdef EXTB
		case EXTB: ospeed = 15; break;
#endif
#ifdef B57600
		case B57600: ospeed = 16; break;
#endif
#ifdef B115200
		case B115200: ospeed = 17; break;
#endif
		default: ;
		}
#endif
		erase_char = s.c_cc[VERASE];
		kill_char = s.c_cc[VKILL];
#ifdef VWERASE
		werase_char = s.c_cc[VWERASE];
#else
		werase_char = CONTROL('W');
#endif

		/*
		 * Set the modes to the way we want them.
		 */
		s.c_lflag &= ~(0
#ifdef ICANON
			| ICANON
#endif
#ifdef ECHO
			| ECHO
#endif
#ifdef ECHOE
			| ECHOE
#endif
#ifdef ECHOK
			| ECHOK
#endif
#if ECHONL
			| ECHONL
#endif
		);

		s.c_oflag |= (0
#ifdef XTABS
			| XTABS
#else
#ifdef TAB3
			| TAB3
#else
#ifdef OXTABS
			| OXTABS
#endif
#endif
#endif
#ifdef OPOST
			| OPOST
#endif
#ifdef ONLCR
			| ONLCR
#endif
		);

		s.c_oflag &= ~(0
#ifdef ONOEOT
			| ONOEOT
#endif
#ifdef OCRNL
			| OCRNL
#endif
#ifdef ONOCR
			| ONOCR
#endif
#ifdef ONLRET
			| ONLRET
#endif
		);
		s.c_cc[VMIN] = 1;
		s.c_cc[VTIME] = 0;
#ifdef VLNEXT
		s.c_cc[VLNEXT] = 0;
#endif
#ifdef VDSUSP
		s.c_cc[VDSUSP] = 0;
#endif
#if MUST_SET_LINE_DISCIPLINE
		/*
		 * System's termios is broken; need to explicitly 
		 * request TERMIODISC line discipline.
		 */
		s.c_line = TERMIODISC;
#endif
	} else
	{
		/*
		 * Restore saved modes.
		 */
		s = save_term;
	}
	tcsetattr(2, TCSADRAIN, &s);
#if MUST_SET_LINE_DISCIPLINE
	if (!on)
	{
		/*
		 * Broken termios *ignores* any line discipline
		 * except TERMIODISC.  A different old line discipline
		 * is therefore not restored, yet.  Restore the old
		 * line discipline by hand.
		 */
		ioctl(2, TIOCSETD, &save_term.c_line);
	}
#endif
    }
#else
#ifdef TCGETA
    {
	struct termio s;
	static struct termio save_term;
	static int saved_term = 0;

	if (on)
	{
		/*
		 * Get terminal modes.
		 */
		ioctl(2, TCGETA, &s);

		/*
		 * Save modes and set certain variables dependent on modes.
		 */
		if (!saved_term)
		{
			save_term = s;
			saved_term = 1;
		}
#if HAVE_OSPEED
		ospeed = s.c_cflag & CBAUD;
#endif
		erase_char = s.c_cc[VERASE];
		kill_char = s.c_cc[VKILL];
#ifdef VWERASE
		werase_char = s.c_cc[VWERASE];
#else
		werase_char = CONTROL('W');
#endif

		/*
		 * Set the modes to the way we want them.
		 */
		s.c_lflag &= ~(ICANON|ECHO|ECHOE|ECHOK|ECHONL);
		s.c_oflag |=  (OPOST|ONLCR|TAB3);
		s.c_oflag &= ~(OCRNL|ONOCR|ONLRET);
		s.c_cc[VMIN] = 1;
		s.c_cc[VTIME] = 0;
	} else
	{
		/*
		 * Restore saved modes.
		 */
		s = save_term;
	}
	ioctl(2, TCSETAW, &s);
    }
#else
#ifdef TIOCGETP
    {
	struct sgttyb s;
	static struct sgttyb save_term;
	static int saved_term = 0;

	if (on)
	{
		/*
		 * Get terminal modes.
		 */
		ioctl(2, TIOCGETP, &s);

		/*
		 * Save modes and set certain variables dependent on modes.
		 */
		if (!saved_term)
		{
			save_term = s;
			saved_term = 1;
		}
#if HAVE_OSPEED
		ospeed = s.sg_ospeed;
#endif
		erase_char = s.sg_erase;
		kill_char = s.sg_kill;
		werase_char = CONTROL('W');

		/*
		 * Set the modes to the way we want them.
		 */
		s.sg_flags |= CBREAK;
		s.sg_flags &= ~(ECHO|XTABS);
	} else
	{
		/*
		 * Restore saved modes.
		 */
		s = save_term;
	}
	ioctl(2, TIOCSETN, &s);
    }
#else
#ifdef _OSK
    {
	struct sgbuf s;
	static struct sgbuf save_term;
	static int saved_term = 0;

	if (on)
	{
		/*
		 * Get terminal modes.
		 */
		_gs_opt(2, &s);

		/*
		 * Save modes and set certain variables dependent on modes.
		 */
		if (!saved_term)
		{
			save_term = s;
			saved_term = 1;
		}
		erase_char = s.sg_bspch;
		kill_char = s.sg_dlnch;
		werase_char = CONTROL('W');

		/*
		 * Set the modes to the way we want them.
		 */
		s.sg_echo = 0;
		s.sg_eofch = 0;
		s.sg_pause = 0;
		s.sg_psch = 0;
	} else
	{
		/*
		 * Restore saved modes.
		 */
		s = save_term;
	}
	_ss_opt(2, &s);
    }
#else
	/* MS-DOS, Windows, or OS2 */
#if OS2
	/* OS2 */
	LSIGNAL(SIGINT, SIG_IGN);
#endif
	erase_char = '\b';
#if MSDOS_COMPILER==DJGPPC
	kill_char = CONTROL('U');
#else
	kill_char = ESC;
#endif
	werase_char = CONTROL('W');
#endif
#endif
#endif
#endif
	curr_on = on;
}

#if !MSDOS_COMPILER
/*
 * Some glue to prevent calling termcap functions if tgetent() failed.
 */
static int hardcopy;

	static char *
ltget_env(capname)
	char *capname;
{
	char name[16];

	strcpy(name, "LESS_TERMCAP_");
	strcat(name, capname);
	return (lgetenv(name));
}

	static int
ltgetflag(capname)
	char *capname;
{
	char *s;

	if ((s = ltget_env(capname)) != NULL)
		return (*s != '\0' && *s != '0');
	if (hardcopy)
		return (0);
	return (tgetflag(capname));
}

	static int
ltgetnum(capname)
	char *capname;
{
	char *s;

	if ((s = ltget_env(capname)) != NULL)
		return (atoi(s));
	if (hardcopy)
		return (-1);
	return (tgetnum(capname));
}

	static char *
ltgetstr(capname, pp)
	char *capname;
	char **pp;
{
	char *s;

	if ((s = ltget_env(capname)) != NULL)
		return (s);
	if (hardcopy)
		return (NULL);
	return (tgetstr(capname, pp));
}
#endif /* MSDOS_COMPILER */

/*
 * Get size of the output screen.
 */
	public void
scrsize()
{
	register char *s;

	sc_width = sc_height = 0;

#if MSDOS_COMPILER==MSOFTC
	{
		struct videoconfig w;
		_getvideoconfig(&w);
		sc_height = w.numtextrows;
		sc_width = w.numtextcols;
	}
#else
#if MSDOS_COMPILER==BORLANDC || MSDOS_COMPILER==DJGPPC
	{
		struct text_info w;
		gettextinfo(&w);
		sc_height = w.screenheight;
		sc_width = w.screenwidth;
	}
#else
#if MSDOS_COMPILER==WIN32C
	{
		CONSOLE_SCREEN_BUFFER_INFO scr;
		GetConsoleScreenBufferInfo(con_out, &scr);
		sc_height = scr.srWindow.Bottom - scr.srWindow.Top + 1;
		sc_width = scr.srWindow.Right - scr.srWindow.Left + 1;
	}
#else
#if OS2
	{
		int s[2];
		_scrsize(s);
		sc_width = s[0];
		sc_height = s[1];
	}
#else
#ifdef TIOCGWINSZ
	{
		struct winsize w;
		if (ioctl(2, TIOCGWINSZ, &w) == 0)
		{
			if (w.ws_row > 0)
				sc_height = w.ws_row;
			if (w.ws_col > 0)
				sc_width = w.ws_col;
		}
	}
#else
#ifdef WIOCGETD
	{
		struct uwdata w;
		if (ioctl(2, WIOCGETD, &w) == 0)
		{
			if (w.uw_height > 0)
				sc_height = w.uw_height / w.uw_vs;
			if (w.uw_width > 0)
				sc_width = w.uw_width / w.uw_hs;
		}
	}
#endif
#endif
#endif
#endif
#endif
#endif
	if (sc_height > 0)
		;
	else if ((s = lgetenv("LINES")) != NULL)
		sc_height = atoi(s);
#if !MSDOS_COMPILER
	else
 		sc_height = ltgetnum("li");
#endif
	if (sc_height <= 0)
#if MSDOS_COMPILER
		sc_height = 25;
#else
		sc_height = 24;
#endif

	if (sc_width > 0)
		;
	else if ((s = lgetenv("COLUMNS")) != NULL)
		sc_width = atoi(s);
#if !MSDOS_COMPILER
	else
 		sc_width = ltgetnum("co");
#endif
 	if (sc_width <= 0)
  		sc_width = 80;
}

#if MSDOS_COMPILER==MSOFTC
/*
 * Figure out how many empty loops it takes to delay a millisecond.
 */
	static void
get_clock()
{
	clock_t start;
	
	/*
	 * Get synchronized at the start of a tick.
	 */
	start = clock();
	while (clock() == start)
		;
	/*
	 * Now count loops till the next tick.
	 */
	start = clock();
	msec_loops = 0;
	while (clock() == start)
		msec_loops++;
	/*
	 * Convert from (loops per clock) to (loops per millisecond).
	 */
	msec_loops *= CLOCKS_PER_SEC;
	msec_loops /= 1000;
}

/*
 * Delay for a specified number of milliseconds.
 */
	static void
dummy_func()
{
	static long delay_dummy = 0;
	delay_dummy++;
}

	static void
delay(msec)
	int msec;
{
	long i;
	
	while (msec-- > 0)
	{
		for (i = 0;  i < msec_loops;  i++)
		{
			/*
			 * Make it look like we're doing something here,
			 * so the optimizer doesn't remove the whole loop.
			 */
			dummy_func();
		}
	}
}
#endif

/*
 * Take care of the "variable" keys.
 * Certain keys send escape sequences which differ on different terminals
 * (such as the arrow keys, INSERT, DELETE, etc.)
 * Construct the commands based on these keys.
 */
	public void
get_editkeys()
{
#if MSDOS_COMPILER
/*
 * Table of line editting characters, for editchar() in decode.c.
 */
static char kecmdtable[] = {
	'\340',PCK_RIGHT,0,	EC_RIGHT,	/* RIGHTARROW */
	'\340',PCK_LEFT,0,	EC_LEFT,	/* LEFTARROW */
	'\340',PCK_CTL_RIGHT,0,	EC_W_RIGHT,	/* CTRL-RIGHTARROW */
	'\340',PCK_CTL_LEFT,0,	EC_W_LEFT,	/* CTRL-LEFTARROW */
	'\340',PCK_INSERT,0,	EC_INSERT,	/* INSERT */
	'\340',PCK_DELETE,0,	EC_DELETE,	/* DELETE */
	'\340',PCK_CTL_DELETE,0,EC_W_DELETE,	/* CTRL-DELETE */
	'\177',0,		EC_W_BACKSPACE,	/* CTRL-BACKSPACE */
	'\340',PCK_HOME,0,	EC_HOME,	/* HOME */
	'\340',PCK_END,0,	EC_END,		/* END */
	'\340',PCK_UP,0,	EC_UP,		/* UPARROW */
	'\340',PCK_DOWN,0,	EC_DOWN,	/* DOWNARROW */
	'\t',0,			EC_F_COMPLETE,	/* TAB */
	'\17',0,		EC_B_COMPLETE,	/* BACKTAB (?) */
	'\340',PCK_SHIFT_TAB,0,	EC_B_COMPLETE,	/* BACKTAB */
	'\14',0,		EC_EXPAND,	/* CTRL-L */
	0  /* Extra byte to terminate; subtracted from size, below */
};
static int sz_kecmdtable = sizeof(kecmdtable) -1;

static char kfcmdtable[] =
{
	/*
	 * PC function keys.
	 * Note that '\0' is converted to '\340' on input.
	 */
	'\340',PCK_DOWN,0,	A_F_LINE,	/* DOWNARROW */
	'\340',PCK_PAGEDOWN,0,	A_F_SCREEN,	/* PAGEDOWN */
	'\340',PCK_UP,0,	A_B_LINE,	/* UPARROW */
	'\340',PCK_PAGEUP,0,	A_B_SCREEN,	/* PAGEUP */
	'\340',PCK_RIGHT,0,	A_RSHIFT,	/* RIGHTARROW */
	'\340',PCK_LEFT,0,	A_LSHIFT,	/* LEFTARROW */
	'\340',PCK_HOME,0,	A_GOLINE,	/* HOME */
	'\340',PCK_END,0,	A_GOEND,	/* END */
	'\340',PCK_F1,0,	A_HELP,		/* F1 */
	'\340',PCK_ALT_E,0,	A_EXAMINE,	/* Alt-E */
	0
};
static int sz_kfcmdtable = sizeof(kfcmdtable) - 1;
#else
	char *sp;
	char *s;
	char tbuf[40];

	static char kfcmdtable[400];
	int sz_kfcmdtable = 0;
	static char kecmdtable[400];
	int sz_kecmdtable = 0;

#define	put_cmd(str,action,tbl,sz) { \
	strcpy(tbl+sz, str);	\
	sz += strlen(str) + 1;	\
	tbl[sz++] = action; }
#define	put_esc_cmd(str,action,tbl,sz) { \
	tbl[sz++] = ESC; \
	put_cmd(str,action,tbl,sz); }

#define	put_fcmd(str,action)	put_cmd(str,action,kfcmdtable,sz_kfcmdtable)
#define	put_ecmd(str,action)	put_cmd(str,action,kecmdtable,sz_kecmdtable)
#define	put_esc_fcmd(str,action) put_esc_cmd(str,action,kfcmdtable,sz_kfcmdtable)
#define	put_esc_ecmd(str,action) put_esc_cmd(str,action,kecmdtable,sz_kecmdtable)

	/*
	 * Look at some interesting keys and see what strings they send.
	 * Create commands (both command keys and line-edit keys).
	 */

	/* RIGHT ARROW */
	sp = tbuf;
	if ((s = ltgetstr("kr", &sp)) != NULL)
	{
		put_ecmd(s, EC_RIGHT);
		put_esc_ecmd(s, EC_W_RIGHT);
		put_fcmd(s, A_RSHIFT);
	}
	
	/* LEFT ARROW */
	sp = tbuf;
	if ((s = ltgetstr("kl", &sp)) != NULL)
	{
		put_ecmd(s, EC_LEFT);
		put_esc_ecmd(s, EC_W_LEFT);
		put_fcmd(s, A_LSHIFT);
	}
	
	/* UP ARROW */
	sp = tbuf;
	if ((s = ltgetstr("ku", &sp)) != NULL) 
	{
		put_ecmd(s, EC_UP);
		put_fcmd(s, A_B_LINE);
	}
		
	/* DOWN ARROW */
	sp = tbuf;
	if ((s = ltgetstr("kd", &sp)) != NULL) 
	{
		put_ecmd(s, EC_DOWN);
		put_fcmd(s, A_F_LINE);
	}

	/* PAGE UP */
	sp = tbuf;
	if ((s = ltgetstr("kP", &sp)) != NULL) 
	{
		put_fcmd(s, A_B_SCREEN);
	}

	/* PAGE DOWN */
	sp = tbuf;
	if ((s = ltgetstr("kN", &sp)) != NULL) 
	{
		put_fcmd(s, A_F_SCREEN);
	}
	
	/* HOME */
	sp = tbuf;
	if ((s = ltgetstr("kh", &sp)) != NULL) 
	{
		put_ecmd(s, EC_HOME);
	}

	/* END */
	sp = tbuf;
	if ((s = ltgetstr("@7", &sp)) != NULL) 
	{
		put_ecmd(s, EC_END);
	}

	/* DELETE */
	sp = tbuf;
	if ((s = ltgetstr("kD", &sp)) == NULL) 
	{
		/* Use DEL (\177) if no "kD" termcap. */
		tbuf[1] = '\177';
		tbuf[2] = '\0';
		s = tbuf+1;
	}
	put_ecmd(s, EC_DELETE);
	put_esc_ecmd(s, EC_W_DELETE);
		
	/* BACKSPACE */
	tbuf[0] = ESC;
	tbuf[1] = erase_char;
	tbuf[2] = '\0';
	put_ecmd(tbuf, EC_W_BACKSPACE);

	if (werase_char != 0)
	{
		tbuf[0] = werase_char;
		tbuf[1] = '\0';
		put_ecmd(tbuf, EC_W_BACKSPACE);
	}
#endif /* MSDOS_COMPILER */

	/*
	 * Register the two tables.
	 */
	add_fcmd_table(kfcmdtable, sz_kfcmdtable);
	add_ecmd_table(kecmdtable, sz_kecmdtable);
}

#if DEBUG
	static void
get_debug_term()
{
	auto_wrap = 1;
	ignaw = 1;
	so_s_width = so_e_width = 0;
	bo_s_width = bo_e_width = 0;
	ul_s_width = ul_e_width = 0;
	bl_s_width = bl_e_width = 0;
	sc_s_keypad =	"(InitKey)";
	sc_e_keypad =	"(DeinitKey)";
	sc_init =	"(InitTerm)";
	sc_deinit =	"(DeinitTerm)";
	sc_eol_clear =	"(ClearEOL)";
	sc_eos_clear =	"(ClearEOS)";
	sc_clear =	"(ClearScreen)";
	sc_move =	"(Move<%d,%d>)";
	sc_s_in =	"(SO+)";
	sc_s_out =	"(SO-)";
	sc_u_in =	"(UL+)";
	sc_u_out =	"(UL-)";
	sc_b_in =	"(BO+)";
	sc_b_out =	"(BO-)";
	sc_bl_in =	"(BL+)";
	sc_bl_out =	"(BL-)";
	sc_visual_bell ="(VBell)";
	sc_backspace =	"(BS)";
	sc_home =	"(Home)";
	sc_lower_left =	"(LL)";
	sc_addline =	"(AddLine)";
}
#endif

/*
 * Get terminal capabilities via termcap.
 */
	public void
get_term()
{
#if MSDOS_COMPILER
	auto_wrap = 1;
	ignaw = 0;
	can_goto_line = 1;
	/*
	 * Set up default colors.
	 * The xx_s_width and xx_e_width vars are already initialized to 0.
	 */
	nm_fg_color = 7;
	nm_bg_color = 0;
	bo_fg_color = 15;
	bo_bg_color = 0;
	ul_fg_color = 9;
	ul_bg_color = 0;
	so_fg_color = 0;
	so_bg_color = 7;
	bl_fg_color = 12;
	bl_bg_color = 0;
#if MSDOS_COMPILER==MSOFTC
	sy_bg_color = _getbkcolor();
	sy_fg_color = _gettextcolor();
	get_clock();
#else
#if MSDOS_COMPILER==BORLANDC || MSDOS_COMPILER==DJGPPC
    {
	struct text_info w;
	gettextinfo(&w);
	sy_bg_color = (w.attribute >> 4) & 0x0F;
	sy_fg_color = (w.attribute >> 0) & 0x0F;
    }
#else
#if MSDOS_COMPILER==WIN32C
    {
	DWORD nread;
	CONSOLE_SCREEN_BUFFER_INFO scr;

	con_out = GetStdHandle(STD_OUTPUT_HANDLE);

	GetConsoleScreenBufferInfo(con_out, &scr);
	ReadConsoleOutputAttribute(con_out, &curr_attr, 
					1, scr.dwCursorPosition, &nread);
	sy_bg_color = (curr_attr & BG_COLORS) >> 4; /* normalize */
	sy_fg_color = curr_attr & FG_COLORS;

#if 0
	/*
	 * If we use the default bg colors, for some as-yet-undetermined 
	 * reason, when you scroll the text colors get messed up in what 
	 * seems to be a random manner. Portions of text will be in 
	 * nm_bg_color but the remainder of that line (which was fine 
	 * before the scroll) will now appear in sy_bg_color. Too strange!
	 */
	nm_bg_color = sy_bg_color;
	bo_bg_color = sy_bg_color;
	ul_bg_color = sy_bg_color;
	bl_bg_color = sy_bg_color;

	/* Make standout = inverse video. */
	so_fg_color = sy_bg_color;
	so_bg_color = sy_fg_color;
#endif
    }
#endif
#endif
#endif
	/*
	 * Get size of the screen.
	 */
	scrsize();
	pos_init();


#else /* !MSDOS_COMPILER */

	char *sp;
	register char *t1, *t2;
	char *term;
	char termbuf[TERMBUF_SIZE];

	static char sbuf[TERMSBUF_SIZE];

#if OS2
	/*
	 * Make sure the termcap database is available.
	 */
	sp = lgetenv("TERMCAP");
	if (sp == NULL || *sp == '\0')
	{
		char *termcap;
		if ((sp = homefile("termcap.dat")) != NULL)
		{
			termcap = (char *) ecalloc(strlen(sp)+9, sizeof(char));
			sprintf(termcap, "TERMCAP=%s", sp);
			free(sp);
			putenv(termcap);
		}
	}
#endif
	/*
	 * Find out what kind of terminal this is.
	 */
 	if ((term = lgetenv("TERM")) == NULL)
 		term = DEFAULT_TERM;
	hardcopy = 0;
 	if (tgetent(termbuf, term) <= 0)
 		hardcopy = 1;
 	if (ltgetflag("hc"))
		hardcopy = 1;

	/*
	 * Get size of the screen.
	 */
	scrsize();
	pos_init();

#if DEBUG
	if (strncmp(term,"LESSDEBUG",9) == 0)
	{
		get_debug_term();
		return;
	}
#endif /* DEBUG */

	auto_wrap = ltgetflag("am");
	ignaw = ltgetflag("xn");
	above_mem = ltgetflag("da");
	below_mem = ltgetflag("db");

	/*
	 * Assumes termcap variable "sg" is the printing width of:
	 * the standout sequence, the end standout sequence,
	 * the underline sequence, the end underline sequence,
	 * the boldface sequence, and the end boldface sequence.
	 */
	if ((so_s_width = ltgetnum("sg")) < 0)
		so_s_width = 0;
	so_e_width = so_s_width;

	bo_s_width = bo_e_width = so_s_width;
	ul_s_width = ul_e_width = so_s_width;
	bl_s_width = bl_e_width = so_s_width;

#if HILITE_SEARCH
	if (so_s_width > 0 || so_e_width > 0)
		/*
		 * Disable highlighting by default on magic cookie terminals.
		 * Turning on highlighting might change the displayed width
		 * of a line, causing the display to get messed up.
		 * The user can turn it back on with -g, 
		 * but she won't like the results.
		 */
		hilite_search = 0;
#endif

	/*
	 * Get various string-valued capabilities.
	 */
	sp = sbuf;

#if HAVE_OSPEED
	sc_pad = ltgetstr("pc", &sp);
	if (sc_pad != NULL)
		PC = *sc_pad;
#endif

	sc_s_keypad = ltgetstr("ks", &sp);
	if (sc_s_keypad == NULL)
		sc_s_keypad = "";
	sc_e_keypad = ltgetstr("ke", &sp);
	if (sc_e_keypad == NULL)
		sc_e_keypad = "";
		
	sc_init = ltgetstr("ti", &sp);
	if (sc_init == NULL)
		sc_init = "";

	sc_deinit= ltgetstr("te", &sp);
	if (sc_deinit == NULL)
		sc_deinit = "";

	sc_eol_clear = ltgetstr("ce", &sp);
	if (sc_eol_clear == NULL || *sc_eol_clear == '\0')
	{
		missing_cap = 1;
		sc_eol_clear = "";
	}

	sc_eos_clear = ltgetstr("cd", &sp);
	if (below_mem && (sc_eos_clear == NULL || *sc_eos_clear == '\0'))
	{
		missing_cap = 1;
		sc_eol_clear = "";
	}

	sc_clear = ltgetstr("cl", &sp);
	if (sc_clear == NULL || *sc_clear == '\0')
	{
		missing_cap = 1;
		sc_clear = "\n\n";
	}

	sc_move = ltgetstr("cm", &sp);
	if (sc_move == NULL || *sc_move == '\0')
	{
		/*
		 * This is not an error here, because we don't 
		 * always need sc_move.
		 * We need it only if we don't have home or lower-left.
		 */
		sc_move = "";
		can_goto_line = 0;
	} else
		can_goto_line = 1;

	tmodes("so", "se", &sc_s_in, &sc_s_out, "", "", &sp);
	tmodes("us", "ue", &sc_u_in, &sc_u_out, sc_s_in, sc_s_out, &sp);
	tmodes("md", "me", &sc_b_in, &sc_b_out, sc_s_in, sc_s_out, &sp);
	tmodes("mb", "me", &sc_bl_in, &sc_bl_out, sc_s_in, sc_s_out, &sp);

	sc_visual_bell = ltgetstr("vb", &sp);
	if (sc_visual_bell == NULL)
		sc_visual_bell = "";

	if (ltgetflag("bs"))
		sc_backspace = "\b";
	else
	{
		sc_backspace = ltgetstr("bc", &sp);
		if (sc_backspace == NULL || *sc_backspace == '\0')
			sc_backspace = "\b";
	}

	/*
	 * Choose between using "ho" and "cm" ("home" and "cursor move")
	 * to move the cursor to the upper left corner of the screen.
	 */
	t1 = ltgetstr("ho", &sp);
	if (t1 == NULL)
		t1 = "";
	if (*sc_move == '\0')
		t2 = "";
	else
	{
		strcpy(sp, tgoto(sc_move, 0, 0));
		t2 = sp;
		sp += strlen(sp) + 1;
	}
	sc_home = cheaper(t1, t2, "|\b^");

	/*
	 * Choose between using "ll" and "cm"  ("lower left" and "cursor move")
	 * to move the cursor to the lower left corner of the screen.
	 */
	t1 = ltgetstr("ll", &sp);
	if (t1 == NULL)
		t1 = "";
	if (*sc_move == '\0')
		t2 = "";
	else
	{
		strcpy(sp, tgoto(sc_move, 0, sc_height-1));
		t2 = sp;
		sp += strlen(sp) + 1;
	}
	sc_lower_left = cheaper(t1, t2, "\r");

	/*
	 * Choose between using "al" or "sr" ("add line" or "scroll reverse")
	 * to add a line at the top of the screen.
	 */
	t1 = ltgetstr("al", &sp);
	if (t1 == NULL)
		t1 = "";
	t2 = ltgetstr("sr", &sp);
	if (t2 == NULL)
		t2 = "";
#if OS2
	if (*t1 == '\0' && *t2 == '\0')
		sc_addline = "";
	else
#endif
	if (above_mem)
		sc_addline = t1;
	else
		sc_addline = cheaper(t1, t2, "");
	if (*sc_addline == '\0')
	{
		/*
		 * Force repaint on any backward movement.
		 */
		no_back_scroll = 1;
	}
#endif /* MSDOS_COMPILER */
}

#if !MSDOS_COMPILER
/*
 * Return the cost of displaying a termcap string.
 * We use the trick of calling tputs, but as a char printing function
 * we give it inc_costcount, which just increments "costcount".
 * This tells us how many chars would be printed by using this string.
 * {{ Couldn't we just use strlen? }}
 */
static int costcount;

/*ARGSUSED*/
	static int
inc_costcount(c)
	int c;
{
	costcount++;
	return (c);
}

	static int
cost(t)
	char *t;
{
	costcount = 0;
	tputs(t, sc_height, inc_costcount);
	return (costcount);
}

/*
 * Return the "best" of the two given termcap strings.
 * The best, if both exist, is the one with the lower 
 * cost (see cost() function).
 */
	static char *
cheaper(t1, t2, def)
	char *t1, *t2;
	char *def;
{
	if (*t1 == '\0' && *t2 == '\0')
	{
		missing_cap = 1;
		return (def);
	}
	if (*t1 == '\0')
		return (t2);
	if (*t2 == '\0')
		return (t1);
	if (cost(t1) < cost(t2))
		return (t1);
	return (t2);
}

	static void
tmodes(incap, outcap, instr, outstr, def_instr, def_outstr, spp)
	char *incap;
	char *outcap;
	char **instr;
	char **outstr;
	char *def_instr;
	char *def_outstr;
	char **spp;
{
	*instr = ltgetstr(incap, spp);
	if (*instr == NULL)
	{
		/* Use defaults. */
		*instr = def_instr;
		*outstr = def_outstr;
		return;
	}

	*outstr = ltgetstr(outcap, spp);
	if (*outstr == NULL)
		/* No specific out capability; use "me". */
		*outstr = ltgetstr("me", spp);
	if (*outstr == NULL)
		/* Don't even have "me"; use a null string. */
		*outstr = "";
}

#endif /* MSDOS_COMPILER */


/*
 * Below are the functions which perform all the 
 * terminal-specific screen manipulation.
 */


#if MSDOS_COMPILER

#if MSDOS_COMPILER==WIN32C
        static void
_settextposition(int row, int col)
{
	COORD cpos;
	cpos.X = (short)(col-1);
	cpos.Y = (short)(row-1);
	SetConsoleCursorPosition(con_out, cpos);
}
#endif

/*
 * Initialize the screen to the correct color at startup.
 */
	static void
initcolor()
{
	SETCOLORS(nm_fg_color, nm_bg_color);
#if 0
	/*
	 * This clears the screen at startup.  This is different from
	 * the behavior of other versions of less.  Disable it for now.
	 */
	char *blanks;
	int row;
	int col;
	
	/*
	 * Create a complete, blank screen using "normal" colors.
	 */
	SETCOLORS(nm_fg_color, nm_bg_color);
	blanks = (char *) ecalloc(width+1, sizeof(char));
	for (col = 0;  col < sc_width;  col++)
		blanks[col] = ' ';
	blanks[sc_width] = '\0';
	for (row = 0;  row < sc_height;  row++)
		_outtext(blanks);
	free(blanks);
#endif
}
#endif

/*
 * Initialize terminal
 */
	public void
init()
{
	if (no_init)
		return;
#if !MSDOS_COMPILER
	tputs(sc_init, sc_height, putchr);
	tputs(sc_s_keypad, sc_height, putchr);
#else
	initcolor();
	flush();
#endif
	init_done = 1;
}

/*
 * Deinitialize terminal
 */
	public void
deinit()
{
	if (no_init)
		return;
	if (!init_done)
		return;
#if !MSDOS_COMPILER
	tputs(sc_e_keypad, sc_height, putchr);
	tputs(sc_deinit, sc_height, putchr);
#else
	SETCOLORS(sy_fg_color, sy_bg_color);
	putstr("\n");
#endif
	init_done = 0;
}

/*
 * Home cursor (move to upper left corner of screen).
 */
	public void
home()
{
#if !MSDOS_COMPILER
	tputs(sc_home, 1, putchr);
#else
	flush();
	_settextposition(1,1);
#endif
}

/*
 * Add a blank line (called with cursor at home).
 * Should scroll the display down.
 */
	public void
add_line()
{
#if !MSDOS_COMPILER
	tputs(sc_addline, sc_height, putchr);
#else
	flush();
#if MSDOS_COMPILER==MSOFTC
	_scrolltextwindow(_GSCROLLDOWN);
	_settextposition(1,1);
#else
#if MSDOS_COMPILER==BORLANDC || MSDOS_COMPILER==DJGPPC
	movetext(1,1, sc_width,sc_height-1, 1,2);
	gotoxy(1,1);
	clreol();
#else
#if MSDOS_COMPILER==WIN32C
    {
	CHAR_INFO fillchar;
	SMALL_RECT rcSrc, rcClip;
	COORD new_org = {0, 1};

	rcClip.Left = 0;
	rcClip.Top = 0;
	rcClip.Right = (short)(sc_width - 1);
	rcClip.Bottom = (short)(sc_height - 1);
	rcSrc.Left = 0;
	rcSrc.Top = 0;
	rcSrc.Right = (short)(sc_width - 1);
	rcSrc.Bottom = (short)(sc_height - 2);
	fillchar.Char.AsciiChar = ' ';
	curr_attr = MAKEATTR(nm_fg_color, nm_bg_color);
	fillchar.Attributes = curr_attr;
	ScrollConsoleScreenBuffer(con_out, &rcSrc, &rcClip, new_org, &fillchar);
	_settextposition(1,1);
    }
#endif
#endif
#endif
#endif
}

/*
 * Move cursor to lower left corner of screen.
 */
	public void
lower_left()
{
#if !MSDOS_COMPILER
	tputs(sc_lower_left, 1, putchr);
#else
	flush();
	_settextposition(sc_height, 1);
#endif
}

/*
 * Goto a specific line on the screen.
 */
	public void
goto_line(slinenum)
	int slinenum;
{
#if !MSDOS_COMPILER
	tputs(tgoto(sc_move, 0, slinenum), 1, putchr);
#else
	flush();
	_settextposition(slinenum+1, 1);
#endif
}

#if MSDOS_COMPILER
/*
 * Create an alternate screen which is all white.
 * This screen is used to create a "flash" effect, by displaying it
 * briefly and then switching back to the normal screen.
 * {{ Yuck!  There must be a better way to get a visual bell. }}
 */
	static void
create_flash()
{
#if MSDOS_COMPILER==MSOFTC
	struct videoconfig w;
	char *blanks;
	int row, col;
	
	_getvideoconfig(&w);
	videopages = w.numvideopages;
	if (videopages < 2)
	{
		so_enter();
		so_exit();
	} else
	{
		_setactivepage(1);
		so_enter();
		blanks = (char *) ecalloc(w.numtextcols, sizeof(char));
		for (col = 0;  col < w.numtextcols;  col++)
			blanks[col] = ' ';
		for (row = w.numtextrows;  row > 0;  row--)
			_outmem(blanks, w.numtextcols);
		_setactivepage(0);
		_setvisualpage(0);
		free(blanks);
		so_exit();
	}
#else
#if MSDOS_COMPILER==BORLANDC
	register int n;

	whitescreen = (unsigned short *) 
		malloc(sc_width * sc_height * sizeof(short));
	if (whitescreen == NULL)
		return;
	for (n = 0;  n < sc_width * sc_height;  n++)
		whitescreen[n] = 0x7020;
#else
#if MSDOS_COMPILER==WIN32C
	register int n;

	whitescreen = (WORD *)
		malloc(sc_height * sc_width * sizeof(WORD));
	if (whitescreen == NULL)
		return;
	/* Invert the standard colors. */
	for (n = 0;  n < sc_width * sc_height;  n++)
		whitescreen[n] = (WORD)((nm_fg_color << 4) | nm_bg_color);
#endif
#endif
#endif
	flash_created = 1;
}
#endif /* MSDOS_COMPILER */

/*
 * Output the "visual bell", if there is one.
 */
	public void
vbell()
{
#if !MSDOS_COMPILER
	if (*sc_visual_bell == '\0')
		return;
	tputs(sc_visual_bell, sc_height, putchr);
#else
#if MSDOS_COMPILER==DJGPPC
	ScreenVisualBell();
#else
#if MSDOS_COMPILER==MSOFTC
	/*
	 * Create a flash screen on the second video page.
	 * Switch to that page, then switch back.
	 */
	if (!flash_created)
		create_flash();
	if (videopages < 2)
		return;
	_setvisualpage(1);
	delay(100);
	_setvisualpage(0);
#else
#if MSDOS_COMPILER==BORLANDC
	unsigned short *currscreen;

	/*
	 * Get a copy of the current screen.
	 * Display the flash screen.
	 * Then restore the old screen.
	 */
	if (!flash_created)
		create_flash();
	if (whitescreen == NULL)
		return;
	currscreen = (unsigned short *) 
		malloc(sc_width * sc_height * sizeof(short));
	if (currscreen == NULL)
		return;
	gettext(1, 1, sc_width, sc_height, currscreen);
	puttext(1, 1, sc_width, sc_height, whitescreen);
	delay(100);
	puttext(1, 1, sc_width, sc_height, currscreen);
	free(currscreen);
#else
#if MSDOS_COMPILER==WIN32C
	WORD *currscreen;
	DWORD nread;

	if (!flash_created)
		create_flash();
	if (whitescreen == NULL)
		return;
	currscreen = (WORD *) 
		malloc(sc_width * sc_height * sizeof(WORD));
	if (currscreen == NULL)
		return;
	ReadConsoleOutputAttribute(con_out, currscreen, 
				sc_height * sc_width, TOPLEFT, &nread);
	WriteConsoleOutputAttribute(con_out, whitescreen,
				sc_height * sc_width, TOPLEFT, &nread);
	Sleep(100);
	WriteConsoleOutputAttribute(con_out, currscreen,
				sc_height * sc_width, TOPLEFT, &nread);
	free(currscreen);
#endif
#endif
#endif
#endif
#endif
}

/*
 * Make a noise.
 */
	static void
beep()
{
#if !MSDOS_COMPILER
	putchr('\7');
#else
#if MSDOS_COMPILER==WIN32C
	MessageBeep(0);
#else
	write(1, "\7", 1);
#endif
#endif
}

/*
 * Ring the terminal bell.
 */
	public void
bell()
{
	if (quiet == VERY_QUIET)
		vbell();
	else
		beep();
}

/*
 * Clear the screen.
 */
	public void
clear()
{
#if !MSDOS_COMPILER
	tputs(sc_clear, sc_height, putchr);
#else
	flush();
#if MSDOS_COMPILER==WIN32C
    {
	DWORD nchars;
	curr_attr = MAKEATTR(nm_fg_color, nm_bg_color);
	FillConsoleOutputCharacter(con_out, ' ', 
			sc_width * sc_height, TOPLEFT, &nchars);
	FillConsoleOutputAttribute(con_out, curr_attr, 
			sc_width * sc_height, TOPLEFT, &nchars);
    }
#else
	_clearscreen(_GCLEARSCREEN);
#endif
#endif
}

/*
 * Clear from the cursor to the end of the cursor's line.
 * {{ This must not move the cursor. }}
 */
	public void
clear_eol()
{
#if !MSDOS_COMPILER
	tputs(sc_eol_clear, 1, putchr);
#else
#if MSDOS_COMPILER==MSOFTC
	short top, left;
	short bot, right;
	struct rccoord tpos;
	
	flush();
	/*
	 * Save current state.
	 */
	tpos = _gettextposition();
	_gettextwindow(&top, &left, &bot, &right);
	/*
	 * Set a temporary window to the current line,
	 * from the cursor's position to the right edge of the screen.
	 * Then clear that window.
	 */
	_settextwindow(tpos.row, tpos.col, tpos.row, sc_width);
	_clearscreen(_GWINDOW);
	/*
	 * Restore state.
	 */
	_settextwindow(top, left, bot, right);
	_settextposition(tpos.row, tpos.col);
#else
#if MSDOS_COMPILER==BORLANDC || MSDOS_COMPILER==DJGPPC
	flush();
	clreol();
#else
#if MSDOS_COMPILER==WIN32C
	DWORD           nchars;
	COORD           cpos;
	CONSOLE_SCREEN_BUFFER_INFO scr;

	flush();
	memset(&scr, 0, sizeof(scr));
	GetConsoleScreenBufferInfo(con_out, &scr);
	cpos.X = scr.dwCursorPosition.X;
	cpos.Y = scr.dwCursorPosition.Y;
	curr_attr = MAKEATTR(nm_fg_color, nm_bg_color);
	FillConsoleOutputAttribute(con_out, curr_attr,
		sc_width - cpos.X, cpos, &nchars);
	FillConsoleOutputCharacter(con_out, ' ',
		sc_width - cpos.X, cpos, &nchars);
#endif
#endif
#endif
#endif
}

/*
 * Clear the bottom line of the display.
 * Leave the cursor at the beginning of the bottom line.
 */
	public void
clear_bot()
{
	lower_left();
#if MSDOS_COMPILER
#if MSDOS_COMPILER==BORLANDC || MSDOS_COMPILER==DJGPPC
	{
		unsigned char save_attr;
		struct text_info txinfo;

		/*
		 * Clear bottom, but with the background color of the text
		 * window, not with background of stand-out color, so the the
		 * bottom line stays stand-out only to the extent of prompt.
		 */
		gettextinfo(&txinfo);
		save_attr = txinfo.attribute;
		lower_left();
		textbackground(nm_bg_color);
		clear_eol();
		textbackground(save_attr >> 4);
	}
#else
	clear_eol();
#endif
#else
	if (below_mem)
		tputs(sc_eos_clear, 1, putchr);
	else
		tputs(sc_eol_clear, 1, putchr);
#endif
}

/*
 * Begin "standout" (bold, underline, or whatever).
 */
	public void
so_enter()
{
#if !MSDOS_COMPILER
	tputs(sc_s_in, 1, putchr);
#else
	flush();
	SETCOLORS(so_fg_color, so_bg_color);
#endif
}

/*
 * End "standout".
 */
	public void
so_exit()
{
#if !MSDOS_COMPILER
	tputs(sc_s_out, 1, putchr);
#else
	flush();
	SETCOLORS(nm_fg_color, nm_bg_color);
#endif
}

/*
 * Begin "underline" (hopefully real underlining, 
 * otherwise whatever the terminal provides).
 */
	public void
ul_enter()
{
#if !MSDOS_COMPILER
	tputs(sc_u_in, 1, putchr);
#else
	flush();
	SETCOLORS(ul_fg_color, ul_bg_color);
#endif
}

/*
 * End "underline".
 */
	public void
ul_exit()
{
#if !MSDOS_COMPILER
	tputs(sc_u_out, 1, putchr);
#else
	flush();
	SETCOLORS(nm_fg_color, nm_bg_color);
#endif
}

/*
 * Begin "bold"
 */
	public void
bo_enter()
{
#if !MSDOS_COMPILER
	tputs(sc_b_in, 1, putchr);
#else
	flush();
	SETCOLORS(bo_fg_color, bo_bg_color);
#endif
}

/*
 * End "bold".
 */
	public void
bo_exit()
{
#if !MSDOS_COMPILER
	tputs(sc_b_out, 1, putchr);
#else
	flush();
	SETCOLORS(nm_fg_color, nm_bg_color);
#endif
}

/*
 * Begin "blink"
 */
	public void
bl_enter()
{
#if !MSDOS_COMPILER
	tputs(sc_bl_in, 1, putchr);
#else
	flush();
	SETCOLORS(bl_fg_color, bl_bg_color);
#endif
}

/*
 * End "blink".
 */
	public void
bl_exit()
{
#if !MSDOS_COMPILER
	tputs(sc_bl_out, 1, putchr);
#else
	flush();
	SETCOLORS(nm_fg_color, nm_bg_color);
#endif
}

/*
 * Erase the character to the left of the cursor 
 * and move the cursor left.
 */
	public void
backspace()
{
#if !MSDOS_COMPILER
	/* 
	 * Erase the previous character by overstriking with a space.
	 */
	tputs(sc_backspace, 1, putchr);
	putchr(' ');
	tputs(sc_backspace, 1, putchr);
#else
#if MSDOS_COMPILER==MSOFTC
	struct rccoord tpos;
	
	flush();
	tpos = _gettextposition();
	if (tpos.col <= 1)
		return;
	_settextposition(tpos.row, tpos.col-1);
	_outtext(" ");
	_settextposition(tpos.row, tpos.col-1);
#else
#if MSDOS_COMPILER==BORLANDC || MSDOS_COMPILER==DJGPPC
	cputs("\b");
#else
#if MSDOS_COMPILER==WIN32C
        COORD cpos;
	CONSOLE_SCREEN_BUFFER_INFO scr;

        flush();
        GetConsoleScreenBufferInfo(con_out, &scr);
        cpos.X = scr.dwCursorPosition.X;
        cpos.Y = scr.dwCursorPosition.Y;
        if (cpos.X <= 0)
                return;
        _settextposition(cpos.Y, cpos.X-1);
        putch(' ');
        _settextposition(cpos.Y, cpos.X-1);
#endif
#endif
#endif
#endif
}

/*
 * Output a plain backspace, without erasing the previous char.
 */
	public void
putbs()
{
#if !MSDOS_COMPILER
	tputs(sc_backspace, 1, putchr);
#else
	int row, col;

	flush();
	{
#if MSDOS_COMPILER==MSOFTC
		struct rccoord tpos;
		tpos = _gettextposition();
		row = tpos.row;
		col = tpos.col;
#else
#if MSDOS_COMPILER==BORLANDC || MSDOS_COMPILER==DJGPPC
		row = wherey();
		col = wherex();
#else
#if MSDOS_COMPILER==WIN32C
		CONSOLE_SCREEN_BUFFER_INFO scr;
		GetConsoleScreenBufferInfo(con_out, &scr);
		row = scr.dwCursorPosition.Y + 1;
		col = scr.dwCursorPosition.X + 1;
#endif
#endif
#endif
	}
	if (col <= 1)
		return;
	_settextposition(row, col-1);
#endif /* MSDOS_COMPILER */
}

#if MSDOS_COMPILER==WIN32C
	static int
win32_kbhit(tty)
	HANDLE tty;
{
	INPUT_RECORD ip;
	DWORD read;

	if (keyCount > 0)
		return (TRUE);

	currentKey.ascii = 0;
	currentKey.scan = 0;

	/*
	 * Wait for a real key-down event, but
	 * ignore SHIFT and CONTROL key events.
	 */
	do
	{
		PeekConsoleInput(tty, &ip, 1, &read);
		if (read == 0)
			return (FALSE);
		ReadConsoleInput(tty, &ip, 1, &read);
	} while (ip.EventType != KEY_EVENT ||
		ip.Event.KeyEvent.bKeyDown != TRUE ||
		ip.Event.KeyEvent.wVirtualScanCode == 0 ||
		ip.Event.KeyEvent.wVirtualKeyCode == VK_SHIFT ||
		ip.Event.KeyEvent.wVirtualKeyCode == VK_CONTROL ||
		ip.Event.KeyEvent.wVirtualKeyCode == VK_MENU);
		
	currentKey.ascii = ip.Event.KeyEvent.uChar.AsciiChar;
	currentKey.scan = ip.Event.KeyEvent.wVirtualScanCode;
	keyCount = ip.Event.KeyEvent.wRepeatCount;

	if (ip.Event.KeyEvent.dwControlKeyState & 
		(LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
	{
		switch (currentKey.scan)
		{
		case PCK_ALT_E:     /* letter 'E' */
			currentKey.ascii = 0;
			break;
		}
	} else if (ip.Event.KeyEvent.dwControlKeyState & 
		(LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
	{
		switch (currentKey.scan)
		{
		case PCK_RIGHT: /* right arrow */
			currentKey.scan = PCK_CTL_RIGHT;
			break;
		case PCK_LEFT: /* left arrow */
			currentKey.scan = PCK_CTL_LEFT;
			break;
		case PCK_DELETE: /* delete */
			currentKey.scan = PCK_CTL_DELETE;
			break;
		}
	}
	return (TRUE);
}

	public char
WIN32getch(tty)
	int tty;
{
	int ascii;

	if (pending_scancode)
	{
		pending_scancode = 0;
		return ((char)(currentKey.scan & 0x00FF));
	}

	while (win32_kbhit((HANDLE)tty) == FALSE)
	{
		Sleep(20);
		continue;
	}
	keyCount --;
	ascii = currentKey.ascii;
	/*
	 * On PC's, the extended keys return a 2 byte sequence beginning 
	 * with '00', so if the ascii code is 00, the next byte will be 
	 * the lsb of the scan code.
	 */
	pending_scancode = (ascii == 0x00);
	return ((char)ascii);
}
#endif
