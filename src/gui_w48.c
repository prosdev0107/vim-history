/* vi:set ts=8 sts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *				GUI support by Robert Webb
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */
/*
 * gui_w48.c:  This file is included in gui_w16.c and gui_w32.c.
 *
 * GUI support for Microsoft Windows (Win16 + Win32 = Win48 :-)
 *
 * The combined efforts of:
 * George V. Reilly <george@reilly.org>
 * Robert Webb
 * Vince Negri
 * ...and contributions from many others
 *
 */

#include "vim.h"
#include "version.h"	/* used by dialog box routine for default title */
#include <windows.h>
#ifdef DEBUG
# include <tchar.h>
#endif
#ifndef __MINGW32__
# include <shellapi.h>
#endif
#ifdef FEAT_TOOLBAR
# include <commctrl.h>
#endif
#ifdef WIN16
# include <commdlg.h>
# include <shellapi.h>
# ifdef WIN16_3DLOOK
#  include <ctl3d.h>
# endif
#endif
#include <windowsx.h>

#ifdef GLOBAL_IME
# include "glbl_ime.h"
#endif

#ifdef FEAT_SMALL
# define MSWIN_FIND_REPLACE	/* include code for find/replace dialog */
# define MSWIN_FR_BUFSIZE 256
#endif
#ifdef FEAT_MENU
# define MENUHINTS		/* show menu hints in command line */
#endif

/* Some parameters for dialog boxes.  All in pixels. */
#define DLG_PADDING_X		10
#define DLG_PADDING_Y		10
#define DLG_OLD_STYLE_PADDING_X	5
#define DLG_OLD_STYLE_PADDING_Y	5
#define DLG_VERT_PADDING_X	4	/* For vertical buttons */
#define DLG_VERT_PADDING_Y	4
#define DLG_ICON_WIDTH		34
#define DLG_ICON_HEIGHT		34
#define DLG_MIN_WIDTH		150
#define DLG_FONT_NAME		"MS Sans Serif"
#define DLG_FONT_POINT_SIZE	8
#define DLG_MIN_MAX_WIDTH	400

#define DLG_NONBUTTON_CONTROL	5000	/* First ID of non-button controls */

#ifdef PROTO
/*
 * Define a few things for generating prototypes.  This is just to avoid
 * syntax errors, the defines do not need to be correct.
 */
# define HINSTANCE	void *
# define HWND		void *
typedef void *HDC;
# define HMENU		void *
# define UINT		int
# define INT		int
# define WPARAM		int
# define LPARAM		int
typedef int LOGFONT[];
# define ENUMLOGFONT	int
# define NEWTEXTMETRIC	int
# define VOID		void
# define CONST
# define CALLBACK
# define WORD		int
# define DWORD		int
# define HBITMAP	int
# define HDROP		int
# define BOOL		int
# define PWORD		int
# define LPWORD		int
# define LPRECT		int
# define LRESULT	int
# define WINAPI
# define APIENTRY
# define LPSTR		int
# define LPWINDOWPOS	int
# define RECT		int
# define LPCREATESTRUCT int
# define _cdecl
# define FINDREPLACE	int
# define LPCTSTR	int
# define OSVERSIONINFO	int
# define HBRUSH		int
# define NEAR
# define FAR
# define BYTE		int
# define LPCSTR		int
# undef MSG
# define MSG		int
#endif

#ifndef GET_X_LPARAM
# define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif

static void _OnPaint( HWND hwnd);
static void clear_rect(RECT *rcp);
static int gui_mswin_get_menu_height(int fix_window);

static WORD		s_dlgfntheight;		/* height of the dialog font */
static WORD		s_dlgfntwidth;		/* width of the dialog font */

#ifdef FEAT_MENU
static HMENU		s_menuBar = NULL;
#endif
#ifdef FEAT_TEAROFF
static void rebuild_tearoff(vimmenu_T *menu);
static HBITMAP	s_htearbitmap;	    /* bitmap used to indicate tearoff */
#endif

/* Flag that is set while processing a message that must not be interupted by
 * processing another message. */
static int		s_busy_processing = FALSE;

static int		destroying = FALSE;	/* call DestroyWindow() ourselves */

#ifdef MSWIN_FIND_REPLACE
static UINT		s_findrep_msg = 0;
static FINDREPLACE	s_findrep_struct;
#endif

static HINSTANCE	s_hinst = NULL;
#if !defined(FEAT_SNIFF) && !defined(FEAT_GUI)
static
#endif
HWND			s_hwnd = NULL;
static HDC		s_hdc = NULL;
static HBRUSH	s_brush = NULL;

#ifdef FEAT_TOOLBAR
static HWND		s_toolbarhwnd = NULL;
#endif

static WPARAM		s_wParam = 0;
static LPARAM		s_lParam = 0;

static HWND		s_textArea = NULL;
static UINT		s_uMsg = 0;

static char_u		*s_textfield; /* Used by dialogs to pass back strings */

static int		s_need_activate = FALSE;

#ifdef GLOBAL_IME
# define DefWindowProc(a, b, c, d) global_ime_DefWindowProc(a, b, c, d)
# define MyTranslateMessage(x) global_ime_TranslateMessage(x)
#else
# define MyTranslateMessage(x) TranslateMessage(x)
#endif

#ifdef FEAT_MBYTE
static int sysfixed_width = 0;
static int sysfixed_height = 0;
#endif

struct charset_pair
{
    char	*name;
    BYTE	charset;
};

static struct charset_pair
charset_pairs[] =
{
    {"ANSI",		ANSI_CHARSET},
    {"CHINESEBIG5",	CHINESEBIG5_CHARSET},
    {"DEFAULT",		DEFAULT_CHARSET},
    {"HANGEUL",		HANGEUL_CHARSET},
    {"OEM",		OEM_CHARSET},
    {"SHIFTJIS",	SHIFTJIS_CHARSET},
    {"SYMBOL",		SYMBOL_CHARSET},
#ifdef WIN3264
    {"ARABIC",		ARABIC_CHARSET},
    {"BALTIC",		BALTIC_CHARSET},
    {"EASTEUROPE",	EASTEUROPE_CHARSET},
    {"GB2312",		GB2312_CHARSET},
    {"GREEK",		GREEK_CHARSET},
    {"HEBREW",		HEBREW_CHARSET},
    {"JOHAB",		JOHAB_CHARSET},
    {"MAC",		MAC_CHARSET},
    {"RUSSIAN",		RUSSIAN_CHARSET},
    {"THAI",		THAI_CHARSET},
    {"TURKISH",		TURKISH_CHARSET},
# if !defined(_MSC_VER) || (_MSC_VER > 1010)
    {"VIETNAMESE",	VIETNAMESE_CHARSET},
# endif
#endif
    {NULL,		0}
};

static const LOGFONT s_lfDefault =
{
    -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
    PROOF_QUALITY, FIXED_PITCH | FF_DONTCARE,
    "Fixedsys"	/* see _ReadVimIni */
};

/* Initialise the "current height" to -12 (same as s_lfDefault) just
 * in case the user specifies a font in "guifont" with no size before a font
 * with an explicit size has been set. This defaults the size to this value
 * (-12 equates to roughly 9pt).
 */
static int current_font_height = -12;

static struct
{
    UINT    key_sym;
    char_u  vim_code0;
    char_u  vim_code1;
} special_keys[] =
{
    {VK_UP,		'k', 'u'},
    {VK_DOWN,		'k', 'd'},
    {VK_LEFT,		'k', 'l'},
    {VK_RIGHT,		'k', 'r'},

    {VK_F1,		'k', '1'},
    {VK_F2,		'k', '2'},
    {VK_F3,		'k', '3'},
    {VK_F4,		'k', '4'},
    {VK_F5,		'k', '5'},
    {VK_F6,		'k', '6'},
    {VK_F7,		'k', '7'},
    {VK_F8,		'k', '8'},
    {VK_F9,		'k', '9'},
    {VK_F10,		'k', ';'},

    {VK_F11,		'F', '1'},
    {VK_F12,		'F', '2'},
    {VK_F13,		'F', '3'},
    {VK_F14,		'F', '4'},
    {VK_F15,		'F', '5'},
    {VK_F16,		'F', '6'},
    {VK_F17,		'F', '7'},
    {VK_F18,		'F', '8'},
    {VK_F19,		'F', '9'},
    {VK_F20,		'F', 'A'},

    {VK_F21,		'F', 'B'},
    {VK_F22,		'F', 'C'},
    {VK_F23,		'F', 'D'},
    {VK_F24,		'F', 'E'},	/* winuser.h defines up to F24 */

    {VK_HELP,		'%', '1'},
    {VK_BACK,		'k', 'b'},
    {VK_INSERT,		'k', 'I'},
    {VK_DELETE,		'k', 'D'},
    {VK_HOME,		'k', 'h'},
    {VK_END,		'@', '7'},
    {VK_PRIOR,		'k', 'P'},
    {VK_NEXT,		'k', 'N'},
    {VK_PRINT,		'%', '9'},
    {VK_ADD,		'K', '6'},
    {VK_SUBTRACT,	'K', '7'},
    {VK_DIVIDE,		'K', '8'},
    {VK_MULTIPLY,	'K', '9'},
    {VK_SEPARATOR,	'K', 'A'},	/* Keypad Enter */
    {VK_DECIMAL,	'K', 'B'},

    {VK_NUMPAD0,	'K', 'C'},
    {VK_NUMPAD1,	'K', 'D'},
    {VK_NUMPAD2,	'K', 'E'},
    {VK_NUMPAD3,	'K', 'F'},
    {VK_NUMPAD4,	'K', 'G'},
    {VK_NUMPAD5,	'K', 'H'},
    {VK_NUMPAD6,	'K', 'I'},
    {VK_NUMPAD7,	'K', 'J'},
    {VK_NUMPAD8,	'K', 'K'},
    {VK_NUMPAD0,	'K', 'L'},

    /* Keys that we want to be able to use any modifier with: */
    {VK_SPACE,		' ', NUL},
    {VK_TAB,		TAB, NUL},
    {VK_ESCAPE,		ESC, NUL},
    {NL,		NL, NUL},
    {CR,		CR, NUL},

    /* End of list marker: */
    {0,			0, 0}
};

/* Local variables */
static int		s_button_pending = -1;
static int		s_x_pending;
static int		s_y_pending;
static UINT		s_kFlags_pending;
static UINT		s_wait_timer = 0;   /* Timer for get char from user */
static int		s_timed_out = FALSE;
static int		dead_key = 0;	/* 0 - no dead key, 1 - dead key pressed */
#ifdef MSWIN_FIND_REPLACE
static HWND		s_findrep_hwnd = NULL;
static int		s_findrep_is_find;
#endif

/*
 * For control IME.
 */
#ifdef FEAT_MBYTE
# ifdef USE_IM_CONTROL
static LOGFONT norm_logfont;
# endif
#endif

#ifdef FEAT_MBYTE_IME
static LRESULT _OnImeNotify(HWND hWnd, DWORD dwCommand, DWORD dwData);
#endif

#ifdef DEBUG
/*
 * Print out the last Windows error message
 */
    static void
print_windows_error(void)
{
    LPVOID  lpMsgBuf;

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		  NULL, GetLastError(),
		  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		  (LPTSTR) &lpMsgBuf, 0, NULL);
    TRACE1("Error: %s\n", lpMsgBuf);
    LocalFree(lpMsgBuf);
}
#endif /* DEBUG */

/*
 * Cursor blink functions.
 *
 * This is a simple state machine:
 * BLINK_NONE	not blinking at all
 * BLINK_OFF	blinking, cursor is not shown
 * BLINK_ON	blinking, cursor is shown
 */

#define BLINK_NONE  0
#define BLINK_OFF   1
#define BLINK_ON    2

static int		blink_state = BLINK_NONE;
static long_u		blink_waittime = 700;
static long_u		blink_ontime = 400;
static long_u		blink_offtime = 250;
static UINT		blink_timer = 0;

    void
gui_mch_set_blinking(long wait, long on, long off)
{
    blink_waittime = wait;
    blink_ontime = on;
    blink_offtime = off;
}

/* ARGSUSED */
    static VOID CALLBACK
_OnBlinkTimer(
    HWND hwnd,
    UINT uMsg,
    UINT idEvent,
    DWORD dwTime)
{
    MSG msg;

    /*
    TRACE2("Got timer event, id %d, blink_timer %d\n", idEvent, blink_timer);
    */

    KillTimer(NULL, idEvent);

    /* Eat spurious WM_TIMER messages */
    while (PeekMessage(&msg, hwnd, WM_TIMER, WM_TIMER, PM_REMOVE))
	;

    if (blink_state == BLINK_ON)
    {
	gui_undraw_cursor();
	blink_state = BLINK_OFF;
	blink_timer = (UINT) SetTimer(NULL, 0, (UINT)blink_offtime,
						    (TIMERPROC)_OnBlinkTimer);
    }
    else
    {
	gui_update_cursor(TRUE, FALSE);
	blink_state = BLINK_ON;
	blink_timer = (UINT) SetTimer(NULL, 0, (UINT)blink_ontime,
							 (TIMERPROC)_OnBlinkTimer);
    }
}

    static void
gui_mswin_rm_blink_timer(void)
{
    MSG msg;

    if (blink_timer != 0)
    {
	KillTimer(NULL, blink_timer);
	/* Eat spurious WM_TIMER messages */
	while (PeekMessage(&msg, s_hwnd, WM_TIMER, WM_TIMER, PM_REMOVE))
	    ;
	blink_timer = 0;
    }
}

/*
 * Stop the cursor blinking.  Show the cursor if it wasn't shown.
 */
    void
gui_mch_stop_blink(void)
{
    gui_mswin_rm_blink_timer();
    if (blink_state == BLINK_OFF)
	gui_update_cursor(TRUE, FALSE);
    blink_state = BLINK_NONE;
}

/*
 * Start the cursor blinking.  If it was already blinking, this restarts the
 * waiting time and shows the cursor.
 */
    void
gui_mch_start_blink(void)
{
    gui_mswin_rm_blink_timer();

    /* Only switch blinking on if none of the times is zero */
    if (blink_waittime && blink_ontime && blink_offtime && gui.in_focus)
    {
	blink_timer = (UINT)SetTimer(NULL, 0, (UINT)blink_waittime,
						    (TIMERPROC)_OnBlinkTimer);
	blink_state = BLINK_ON;
	gui_update_cursor(TRUE, FALSE);
    }
}

/*
 * Call-back routines.
 */

    static VOID CALLBACK
_OnTimer(
    HWND hwnd,
    UINT uMsg,
    UINT idEvent,
    DWORD dwTime)
{
    MSG msg;

    /*
    TRACE2("Got timer event, id %d, s_wait_timer %d\n", idEvent, s_wait_timer);
    */
    KillTimer(NULL, idEvent);
    s_timed_out = TRUE;

    /* Eat spurious WM_TIMER messages */
    while (PeekMessage(&msg, hwnd, WM_TIMER, WM_TIMER, PM_REMOVE))
	;
    if (idEvent == s_wait_timer)
	s_wait_timer = 0;
}

    static void
_OnDeadChar(
    HWND hwnd,
    UINT ch,
    int cRepeat)
{
    dead_key = 1;
}

/*
 * Key hit, add it to the input buffer.
 * Careful: CSI arrives as 0xffffff9b.
 */
    static void
_OnChar(
    HWND hwnd,
    UINT ch,
    int cRepeat)
{
    char_u	string[40];

    string[0] = ch;
    if (string[0] == Ctrl_C && ctrl_c_interrupts)
    {
	trash_input_buf();
	got_int = TRUE;
    }

    if (string[0] == CSI)
    {
	/* Insert CSI as K_CSI. */
	string[1] = KS_EXTRA;
	string[2] = (int)KE_CSI;
	add_to_input_buf(string, 3);
    }
    else
    {
	int	len = 1;

#ifdef FEAT_MBYTE
	if (input_conv.vc_type != CONV_NONE)
	    len = convert_input(string, len, sizeof(string));
#endif
	add_to_input_buf(string, len);
    }
}

/*
 * Alt-Key hit, add it to the input buffer.
 */
    static void
_OnSysChar(
    HWND hwnd,
    UINT cch,
    int cRepeat)
{
    char_u	string[40]; /* Enough for multibyte character */
    int		len;
    int		modifiers;
    int		ch = cch;   /* special keys are negative */

    /* TRACE("OnSysChar(%d, %c)\n", ch, ch); */

    /* OK, we have a character key (given by ch) which was entered with the
     * ALT key pressed. Eg, if the user presses Alt-A, then ch == 'A'. Note
     * that the system distinguishes Alt-a and Alt-A (Alt-Shift-a unless
     * CAPSLOCK is pressed) at this point.
     */
    modifiers = MOD_MASK_ALT;
    if (GetKeyState(VK_SHIFT) & 0x8000)
	modifiers |= MOD_MASK_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000)
	modifiers |= MOD_MASK_CTRL;

    ch = simplify_key(ch, &modifiers);
    /* remove the SHIFT modifier for keys where it's already included, e.g.,
     * '(' and '*' */
    if (ch < 0x100 && !isalpha(ch) && isprint(ch))
	modifiers &= ~MOD_MASK_SHIFT;

    /* Interpret the ALT key as making the key META, include SHIFT, etc. */
    ch = extract_modifiers(ch, &modifiers);
    if (ch == CSI)
	ch = K_CSI;

    len = 0;
    if (modifiers)
    {
	string[len++] = CSI;
	string[len++] = KS_MODIFIER;
	string[len++] = modifiers;
    }

    if (IS_SPECIAL((int)ch))
    {
	string[len++] = CSI;
	string[len++] = K_SECOND((int)ch);
	string[len++] = K_THIRD((int)ch);
    }
    else if (ch == CSI)
    {
	string[len++] = CSI;
	string[len++] = KS_EXTRA;
	string[len++] = (int)KE_CSI;
    }
    else
    {
	string[len++] = ch;
#ifdef FEAT_MBYTE
	if (input_conv.vc_type != CONV_NONE)
	    len += convert_input(string + len - 1, 1, sizeof(string) - len) - 1;
	else if (enc_utf8 && string[len - 1] >= 0x80)
	{
	    /* convert to utf-8 */
	    string[len] = string[len - 1] & 0xbf;
	    string[len - 1] = ((unsigned)string[len - 1] >> 6) + 0xc0;
	    if (string[len++] == CSI)
	    {
		string[len++] = KS_EXTRA;
		string[len++] = (int)KE_CSI;
	    }
	}
#endif
    }

    add_to_input_buf(string, len);
}

    static void
_OnMouseEvent(
    int button,
    int x,
    int y,
    int repeated_click,
    UINT keyFlags)
{
    int vim_modifiers = 0x0;

    if (keyFlags & MK_SHIFT)
	vim_modifiers |= MOUSE_SHIFT;
    if (keyFlags & MK_CONTROL)
	vim_modifiers |= MOUSE_CTRL;
    if (GetKeyState(VK_MENU) & 0x8000)
	vim_modifiers |= MOUSE_ALT;

    gui_send_mouse_event(button, x, y, repeated_click, vim_modifiers);
}

    static void
_OnMouseButtonDown(
    HWND hwnd,
    BOOL fDoubleClick,
    int x,
    int y,
    UINT keyFlags)
{
    static LONG	s_prevTime = 0;

    LONG    currentTime = GetMessageTime();
    int	    button = -1;
    int	    repeated_click;

    if (s_uMsg == WM_LBUTTONDOWN || s_uMsg == WM_LBUTTONDBLCLK)
	button = MOUSE_LEFT;
    else if (s_uMsg == WM_MBUTTONDOWN || s_uMsg == WM_MBUTTONDBLCLK)
	button = MOUSE_MIDDLE;
    else if (s_uMsg == WM_RBUTTONDOWN || s_uMsg == WM_RBUTTONDBLCLK)
	button = MOUSE_RIGHT;
#ifndef WIN16 //<VN>
    else if (s_uMsg == WM_CAPTURECHANGED)
    {
	/* on W95/NT4, somehow you get in here with an odd Msg
	 * if you press one button while holding down the other..*/
	if (s_button_pending == MOUSE_LEFT)
	    button = MOUSE_RIGHT;
	else
	    button = MOUSE_LEFT;
    }
#endif
    if (button >= 0)
    {
	repeated_click = ((int)(currentTime - s_prevTime) < p_mouset);

	/*
	 * Holding down the left and right buttons simulates pushing the middle
	 * button.
	 */
	if (repeated_click &&
		((button == MOUSE_LEFT && s_button_pending == MOUSE_RIGHT) ||
		 (button == MOUSE_RIGHT && s_button_pending == MOUSE_LEFT)))
	{
	    /*
	     * Hmm, gui.c will ignore more than one button down at a time, so
	     * pretend we let go of it first.
	     */
	    gui_send_mouse_event(MOUSE_RELEASE, x, y, FALSE, 0x0);
	    button = MOUSE_MIDDLE;
	    repeated_click = FALSE;
	    s_button_pending = -1;
	    _OnMouseEvent(button, x, y, repeated_click, keyFlags);
	}
	else if ((repeated_click)
		|| (mouse_model_popup() && (button == MOUSE_RIGHT)))
	{
	    if (s_button_pending > -1)
	    {
		    _OnMouseEvent(s_button_pending, x, y, FALSE, keyFlags);
		    s_button_pending = -1;
	    }
	    /* TRACE("Button down at x %d, y %d\n", x, y); */
	    _OnMouseEvent(button, x, y, repeated_click, keyFlags);
	}
	else
	{
	    /*
	     * If this is the first press (i.e. not a multiple click) don't
	     * action immediately, but store and wait for:
	     * i) button-up
	     * ii) mouse move
	     * iii) another button press
	     * before using it.
	     * This enables us to make left+right simulate middle button,
	     * without left or right being actioned first.  The side-effect is
	     * that if you click and hold the mouse without dragging, the
	     * cursor doesn't move until you release the button. In practice
	     * this is hardly a problem.
	     */
	    s_button_pending = button;
	    s_x_pending = x;
	    s_y_pending = y;
	    s_kFlags_pending = keyFlags;
	}

	s_prevTime = currentTime;
    }
}

    static void
_OnMouseMoveOrRelease(
    HWND hwnd,
    int x,
    int y,
    UINT keyFlags)
{
    int button;

    if (s_button_pending > -1)
    {
	/* Delayed action for mouse down event */
	_OnMouseEvent(s_button_pending, s_x_pending,
			s_y_pending, FALSE, s_kFlags_pending);
	s_button_pending = -1;
    }
    if (s_uMsg == WM_MOUSEMOVE)
    {
	/*
	 * It's only a MOUSE_DRAG if one or more mouse buttons are being held
	 * down.
	 */
	if (!(keyFlags & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)))
	{
	    gui_mouse_moved(x, y);
	    return;
	}

	/*
	 * While button is down, keep grabbing mouse move events when
	 * the mouse goes outside the window
	 */
	SetCapture(s_textArea);
	button = MOUSE_DRAG;
	/* TRACE("  move at x %d, y %d\n", x, y); */
    }
    else
    {
	ReleaseCapture();
	button = MOUSE_RELEASE;
	/* TRACE("  up at x %d, y %d\n", x, y); */
    }

    _OnMouseEvent(button, x, y, FALSE, keyFlags);
}
#ifdef FEAT_MENU
/*
 * Find the vimmenu_T with the given id
 */
    static vimmenu_T *
gui_mswin_find_menu(
    vimmenu_T	*pMenu,
    int		id)
{
    vimmenu_T	*pChildMenu;

    while (pMenu)
    {
	if (pMenu->id == (UINT)id)
	    break;
	if (pMenu->children != NULL)
	{
	    pChildMenu = gui_mswin_find_menu(pMenu->children, id);
	    if (pChildMenu)
	    {
		pMenu = pChildMenu;
		break;
	    }
	}
	pMenu = pMenu->next;
    }
    return pMenu;
}

    static void
_OnMenu(
    HWND	hwnd,
    int		id,
    HWND	hwndCtl,
    UINT	codeNotify)
{
    vimmenu_T	*pMenu;

    pMenu = gui_mswin_find_menu(root_menu, id);
    if (pMenu)
	gui_menu_cb(pMenu);
}
#endif

#if defined(MSWIN_FIND_REPLACE)
/*
 * Copy the "from" string to the end of cmd[], escaping any '/'.
 */
    static void
fr_copy_escape(char *from, char_u *to)
{
    char	*s;
    char_u	*d;

    d = to + STRLEN(to);
    for (s = from; *s != NUL; ++s)
    {
	if (*s == '/')
	    *d++ = '\\';
	*d++ = *s;
    }
    *d = NUL;
}

    static void
fr_setwhat(char_u *cmd)
{
    if (s_findrep_struct.Flags & FR_WHOLEWORD)
	STRCAT(cmd, "\\<");
    fr_copy_escape(s_findrep_struct.lpstrFindWhat, cmd);
    if (s_findrep_struct.Flags & FR_WHOLEWORD)
	STRCAT(cmd, "\\>");
}

    static void
fr_setreplcmd(char_u *cmd)
{
    STRCAT(cmd, ":%sno/");
    fr_setwhat(cmd);
    STRCAT(cmd, "/");
    fr_copy_escape(s_findrep_struct.lpstrReplaceWith, cmd);
    if (s_findrep_struct.Flags & FR_REPLACE)
	STRCAT(cmd, "/gc");
    else
	STRCAT(cmd, "/g");
    if (s_findrep_struct.Flags & FR_MATCHCASE)
	STRCAT(cmd, "I");
    else
	STRCAT(cmd, "i");
    STRCAT(cmd, "\r");
}

/*
 * Handle a Find/Replace window message.
 */
    static void
_OnFindRepl(void)
{
    char_u cmd[MSWIN_FR_BUFSIZE * 2 + 100]; //XXX kludge

    /* Add a char before the command if needed */
    if (State & INSERT)
	cmd[0] = Ctrl_O;
    else if ((State & NORMAL) == 0 && State != CONFIRM)
	cmd[0] = ESC;
    else
	cmd[0] = NUL;
    cmd[1] = NUL;

    if (s_findrep_struct.Flags & FR_DIALOGTERM)
    {
	if (State == CONFIRM)
	    add_to_input_buf("q", 1);
	return;
    }

    if (s_findrep_struct.Flags & FR_FINDNEXT)
    {
	if (State == CONFIRM)
	    STRCAT(cmd, "n");
	else
	{
	    /* Set 'ignorecase' just for this search command. */
	    if (!(s_findrep_struct.Flags & FR_MATCHCASE) == !p_ic)
	    {
		if (p_ic)
		    STRCAT(cmd, ":set noic\r");
		else
		    STRCAT(cmd, ":set ic\r");
		if (State & INSERT)
		    STRCAT(cmd, "\017");	/* CTRL-O */
	    }
	    if (s_findrep_struct.Flags & FR_DOWN)
		STRCAT(cmd, "/");
	    else
		STRCAT(cmd, "?");
	    fr_setwhat(cmd);
	    STRCAT(cmd, "\r");
	    if (!(s_findrep_struct.Flags & FR_MATCHCASE) == !p_ic)
	    {
		if (State & INSERT)
		    STRCAT(cmd, "\017");	/* CTRL-O */
		if (p_ic)
		    STRCAT(cmd, ":set ic\r");
		else
		    STRCAT(cmd, ":set noic\r");
	    }
	}
	/*
	 * Give main window the focus back: this is so
	 * the cursor isn't hollow.
	 */
	(void)SetFocus(s_hwnd);
    }
    else if (s_findrep_struct.Flags & FR_REPLACE)
    {
	if (State == CONFIRM)
	    STRCAT(cmd, "y");
	else
	    fr_setreplcmd(cmd);
	/*
	 * Give main window the focus back: this is to allow
	 * handling of the confirmation y/n/a/q stuff.
	 */
	(void)SetFocus(s_hwnd);
    }
    else if (s_findrep_struct.Flags & FR_REPLACEALL)
    {
	if (State == CONFIRM)
	    STRCAT(cmd, "a");
	else
	    fr_setreplcmd(cmd);
    }
    if (*cmd)
	add_to_input_buf(cmd, (int)STRLEN(cmd));
}
#endif

    static void
HandleMouseHide(UINT uMsg, LPARAM lParam)
{
    static LPARAM last_lParam = 0L;

    /* We sometimes get a mousemove when the mouse didn't move... */
    if (uMsg == WM_MOUSEMOVE)
    {
	if (lParam == last_lParam)
	    return;
	last_lParam = lParam;
    }

    /* Handle specially, to centralise coding. We need to be sure we catch all
     * possible events which should cause us to restore the cursor (as it is a
     * shared resource, we take full responsibility for it).
     */
    switch (uMsg)
    {
    case WM_KEYUP:
    case WM_CHAR:
	/*
	 * blank out the pointer if necessary
	 */
	if (p_mh)
	    gui_mch_mousehide(TRUE);
	break;

    case WM_SYSKEYUP:	 /* show the pointer when a system-key is pressed */
    case WM_SYSCHAR:
    case WM_MOUSEMOVE:	 /* show the pointer on any mouse action */
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_NCMOUSEMOVE:
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
    case WM_NCMBUTTONDOWN:
    case WM_NCMBUTTONUP:
    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP:
    case WM_KILLFOCUS:
	/*
	 * if the pointer is currently hidden, then we should show it.
	 */
	gui_mch_mousehide(FALSE);
	break;
    }
}

    static LRESULT CALLBACK
_TextAreaWndProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    /*
    TRACE("TextAreaWndProc: hwnd = %08x, msg = %x, wParam = %x, lParam = %x\n",
	  hwnd, uMsg, wParam, lParam);
    */

    HandleMouseHide(uMsg, lParam);

    s_uMsg = uMsg;
    s_wParam = wParam;
    s_lParam = lParam;

    switch (uMsg)
    {
	HANDLE_MSG(hwnd, WM_LBUTTONDBLCLK,_OnMouseButtonDown);
	HANDLE_MSG(hwnd, WM_LBUTTONDOWN,_OnMouseButtonDown);
	HANDLE_MSG(hwnd, WM_LBUTTONUP,	_OnMouseMoveOrRelease);
	HANDLE_MSG(hwnd, WM_MBUTTONDBLCLK,_OnMouseButtonDown);
	HANDLE_MSG(hwnd, WM_MBUTTONDOWN,_OnMouseButtonDown);
	HANDLE_MSG(hwnd, WM_MBUTTONUP,	_OnMouseMoveOrRelease);
	HANDLE_MSG(hwnd, WM_MOUSEMOVE,	_OnMouseMoveOrRelease);
	HANDLE_MSG(hwnd, WM_PAINT,	_OnPaint);
	HANDLE_MSG(hwnd, WM_RBUTTONDBLCLK,_OnMouseButtonDown);
	HANDLE_MSG(hwnd, WM_RBUTTONDOWN,_OnMouseButtonDown);
	HANDLE_MSG(hwnd, WM_RBUTTONUP,	_OnMouseMoveOrRelease);

    default:
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

/*
 * Called when the foreground or background color has been changed.
 */
    void
gui_mch_new_colors(void)
{
    /* nothing to do? */
}

/*
 * Open the GUI window which was created by a call to gui_mch_init().
 */
    int
gui_mch_open(void)
{
    /* Actually open the window */
    ShowWindow(s_hwnd, SW_SHOWNORMAL);

    return OK;
}

/*
 * Get the position of the top left corner of the window.
 */
    int
gui_mch_get_winpos(int *x, int *y)
{
    RECT    rect;

    GetWindowRect(s_hwnd, &rect);
    *x = rect.left;
    *y = rect.top;
    return OK;
}

/*
 * Set the position of the top left corner of the window to the given
 * coordinates.
 */
    void
gui_mch_set_winpos(int x, int y)
{
    SetWindowPos(s_hwnd, NULL, x, y, 0, 0,
		 SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}
    void
gui_mch_set_text_area_pos(int x, int y, int w, int h)
{
    SetWindowPos(s_textArea, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);

#ifdef FEAT_TOOLBAR
    if (vim_strchr(p_go, GO_TOOLBAR) != NULL)
	SendMessage(s_toolbarhwnd, WM_SIZE,
		(WPARAM)0, (LPARAM)(w + ((long)(TOOLBAR_BUTTON_HEIGHT+8)<<16)));
#endif
}


/*
 * Scrollbar stuff:
 */

    void
gui_mch_enable_scrollbar(
    scrollbar_T     *sb,
    int		    flag)
{
    ShowScrollBar(sb->id, SB_CTL, flag);

    /* TODO: When the window is maximized, the size of the window stays the
     * same, thus the size of the text area changes.  On Win98 it's OK, on Win
     * NT 4.0 it's not... */
}

    void
gui_mch_set_scrollbar_pos(
    scrollbar_T *sb,
    int		x,
    int		y,
    int		w,
    int		h)
{
    SetWindowPos(sb->id, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

    void
gui_mch_create_scrollbar(
    scrollbar_T *sb,
    int		orient)	/* SBAR_VERT or SBAR_HORIZ */
{
    sb->id = CreateWindow(
	"SCROLLBAR", "Scrollbar",
	WS_CHILD | ((orient == SBAR_VERT) ? SBS_VERT : SBS_HORZ), 0, 0,
	10,				/* Any value will do for now */
	10,				/* Any value will do for now */
	s_hwnd, NULL,
	s_hinst, NULL);
}

/*
 * Find the scrollbar with the given hwnd.
 */
	 static scrollbar_T *
gui_mswin_find_scrollbar(HWND hwnd)
{
    win_T	*wp;

    if (gui.bottom_sbar.id == hwnd)
	return &gui.bottom_sbar;
    FOR_ALL_WINDOWS(wp)
    {
	if (wp->w_scrollbars[SBAR_LEFT].id == hwnd)
	    return &wp->w_scrollbars[SBAR_LEFT];
	if (wp->w_scrollbars[SBAR_RIGHT].id == hwnd)
	    return &wp->w_scrollbars[SBAR_RIGHT];
    }
    return NULL;
}

/*
 * Get the character size of a font.
 */
    static void
GetFontSize(GuiFont font)
{
    HWND    hwnd = GetDesktopWindow();
    HDC	    hdc = GetWindowDC(hwnd);
    HFONT   hfntOld = SelectFont(hdc, (HFONT)font);
    TEXTMETRIC tm;

    GetTextMetrics(hdc, &tm);
    gui.char_width = tm.tmAveCharWidth + tm.tmOverhang;

    gui.char_height = tm.tmHeight
#ifndef MSWIN16_FASTTEXT
	+ p_linespace
#endif
	;

    SelectFont(hdc, hfntOld);

    ReleaseDC(hwnd, hdc);
}

    int
gui_mch_adjust_charsize(void)
{
    GetFontSize(gui.norm_font);
    return OK;
}

    static GuiFont
get_font_handle(LOGFONT *lf)
{
    HFONT   font = NULL;

    /* Load the font */
    font = CreateFontIndirect(lf);

    if (font == NULL)
	return NOFONT;

    return (GuiFont)font;
}

/* Convert a string representing a point size into pixels. The string should
 * be a positive decimal number, with an optional decimal point (eg, "12", or
 * "10.5"). The pixel value is returned, and a pointer to the next unconverted
 * character is stored in *end. The flag "vertical" says whether this
 * calculation is for a vertical (height) size or a horizontal (width) one.
 */
    static int
points_to_pixels(char_u *str, char_u **end, int vertical, HDC printer_dc)
{
    int		pixels;
    int		points = 0;
    int		divisor = 0;
    HWND	hwnd;
    HDC		hdc;

    while (*str != NUL)
    {
	if (*str == '.' && divisor == 0)
	{
	    /* Start keeping a divisor, for later */
	    divisor = 1;
	}
	else
	{
	    if (!isdigit(*str))
		break;

	    points *= 10;
	    points += *str - '0';
	    divisor *= 10;
	}
	++str;
    }

    if (divisor == 0)
	divisor = 1;

    if (printer_dc == NULL)
    {
	hwnd = GetDesktopWindow();
	hdc = GetWindowDC(hwnd);
    }
    else
	hdc = printer_dc;

    pixels = MulDiv(points,
		    GetDeviceCaps(hdc, vertical ? LOGPIXELSY : LOGPIXELSX),
		    72 * divisor);

    if (printer_dc == NULL)
	ReleaseDC(hwnd, hdc);

    *end = str;
    return pixels;
}

    static int
pixels_to_points(int pixels, int vertical)
{
    int		points;
    HWND	hwnd;
    HDC		hdc;

    hwnd = GetDesktopWindow();
    hdc = GetWindowDC(hwnd);

    points = MulDiv(pixels, 72,
		    GetDeviceCaps(hdc, vertical ? LOGPIXELSY : LOGPIXELSX));

    ReleaseDC(hwnd, hdc);

    return points;
}

    static int CALLBACK
font_enumproc(
    ENUMLOGFONT	    *elf,
    NEWTEXTMETRIC   *ntm,
    int		    type,
    LPARAM	    lparam)
{
    /* Return value:
     *	  0 = terminate now (monospace & ANSI)
     *	  1 = continue, still no luck...
     *	  2 = continue, but we have an acceptable LOGFONT
     *	      (monospace, not ANSI)
     * We use these values, as EnumFontFamilies returns 1 if the
     * callback function is never called. So, we check the return as
     * 0 = perfect, 2 = OK, 1 = no good...
     * It's not pretty, but it works!
     */

    LOGFONT *lf = (LOGFONT *)(lparam);

#ifndef FEAT_PROPORTIONAL_FONTS
    /* Ignore non-monospace fonts without further ado */
    if ((ntm->tmPitchAndFamily & 1) != 0)
	return 1;
#endif

    /* Remember this LOGFONT as a "possible" */
    *lf = elf->elfLogFont;

    /* Terminate the scan as soon as we find an ANSI font */
    if (lf->lfCharSet == ANSI_CHARSET
	    || lf->lfCharSet == OEM_CHARSET
	    || lf->lfCharSet == DEFAULT_CHARSET)
	return 0;

    /* Continue the scan - we have a non-ANSI font */
    return 2;
}
    static int
init_logfont(LOGFONT *lf)
{
    int		n;
    HWND	hwnd = GetDesktopWindow();
    HDC		hdc = GetWindowDC(hwnd);

    n = EnumFontFamilies(hdc,
			 (LPCSTR)lf->lfFaceName,
			 (FONTENUMPROC)font_enumproc,
			 (LPARAM)lf);

    ReleaseDC(hwnd, hdc);

    /* If we couldn't find a useable font, return failure */
    if (n == 1)
	return FAIL;

    /* Tidy up the rest of the LOGFONT structure. We set to a basic
     * font - get_logfont() sets bold, italic, etc based on the user's
     * input.
     */
    lf->lfHeight = current_font_height;
    lf->lfWidth = 0;
    lf->lfItalic = FALSE;
    lf->lfUnderline = FALSE;
    lf->lfStrikeOut = FALSE;
    lf->lfWeight = FW_NORMAL;

    /* Return success */
    return OK;
}

    int
get_logfont(
    LOGFONT *lf,
    char_u  *name,
    HDC printer_dc)
{
    char_u	*p;
    CHOOSEFONT	cf;
    int		i;
    static LOGFONT *lastlf = NULL;

    *lf = s_lfDefault;
    if (name == NULL)
	return 1;

    if (STRCMP(name, "*") == 0)
    {
	/* if name is "*", bring up std font dialog: */
	memset(&cf, 0, sizeof(cf));
	cf.lStructSize = sizeof(cf);
	cf.hwndOwner = s_hwnd;
	cf.Flags = CF_SCREENFONTS | CF_FIXEDPITCHONLY | CF_INITTOLOGFONTSTRUCT;
	if (lastlf != NULL)
	    *lf = *lastlf;
	cf.lpLogFont = lf;
	cf.nFontType = 0 ; //REGULAR_FONTTYPE;
	if (ChooseFont(&cf))
	    goto theend;
    }

    /*
     * Split name up, it could be <name>:h<height>:w<width> etc.
     */
    for (p = name; *p && *p != ':'; p++)
    {
	if (p - name + 1 > LF_FACESIZE)
	    return 0;			/* Name too long */
	lf->lfFaceName[p - name] = *p;
    }
    if (p != name)
	lf->lfFaceName[p - name] = NUL;

    /* First set defaults */
    lf->lfHeight = -12;
    lf->lfWidth = 0;
    lf->lfWeight = FW_NORMAL;
    lf->lfItalic = FALSE;
    lf->lfUnderline = FALSE;
    lf->lfStrikeOut = FALSE;

    /*
     * If the font can't be found, try replacing '_' by ' '.
     */
    if (init_logfont(lf) == FAIL)
    {
	int	did_replace = FALSE;

	for (i = 0; lf->lfFaceName[i]; ++i)
	    if (lf->lfFaceName[i] == '_')
	    {
		lf->lfFaceName[i] = ' ';
		did_replace = TRUE;
	    }
	if (!did_replace || init_logfont(lf) == FAIL)
	    return 0;
    }

    while (*p == ':')
	p++;

    /* Set the values found after ':' */
    while (*p)
    {
	switch (*p++)
	{
	    case 'h':
		lf->lfHeight = - points_to_pixels(p, &p, TRUE, printer_dc);
		break;
	    case 'w':
		lf->lfWidth = points_to_pixels(p, &p, FALSE, printer_dc);
		break;
	    case 'b':
#ifndef MSWIN16_FASTTEXT
		lf->lfWeight = FW_BOLD;
#endif
		break;
	    case 'i':
#ifndef MSWIN16_FASTTEXT
		lf->lfItalic = TRUE;
#endif
		break;
	    case 'u':
		lf->lfUnderline = TRUE;
		break;
	    case 's':
		lf->lfStrikeOut = TRUE;
		break;
	    case 'c':
		{
		    struct charset_pair *cp;

		    for (cp = charset_pairs; cp->name != NULL; ++cp)
			if (STRNCMP(p, cp->name, strlen(cp->name)) == 0)
			{
			    lf->lfCharSet = cp->charset;
			    p += strlen(cp->name);
			    break;
			}
		    if (cp->name == NULL)
		    {
			sprintf((char *)IObuff, _("E244: Illegal charset name \"%s\" in font name \"%s\""), p, name);
			EMSG(IObuff);
			break;
		    }
		    break;
		}
	    default:
		sprintf((char *)IObuff,
			_("E245: Illegal char '%c' in font name \"%s\""),
			p[-1], name);
		EMSG(IObuff);
		break;
	}
	while (*p == ':')
	    p++;
    }

theend:
    /* ron: init lastlf */
    vim_free(lastlf);
    lastlf = (LOGFONT *)alloc(sizeof(LOGFONT));
    if (lastlf != NULL)
	mch_memmove(lastlf, lf, sizeof(LOGFONT));

    return 1;
}
    GuiFont
gui_mch_get_font(
    char_u	*name,
    int		giveErrorIfMissing)
{
    LOGFONT	lf;
    GuiFont	font;

    get_logfont(&lf, name, NULL);
    font = get_font_handle(&lf);
    if (font == NOFONT && giveErrorIfMissing)
	EMSG2(_("E235: Unknown font: %s"), name);
    return font;
}
    void
gui_mch_free_font(GuiFont font)
{
    if (font)
	DeleteObject((HFONT)font);
}

    static int
hex_digit(int c)
{
    if (vim_isdigit(c))
	return c - '0';
    c = TO_LOWER(c);
    if (c >= 'a' && c <= 'f')
	return c - 'a' + 10;
    return -1000;
}
/*
 * Return the Pixel value (color) for the given color name.
 * Return -1 for error.
 */
    guicolor_T
gui_mch_get_color(char_u *name)
{
    typedef struct guicolor_tTable
    {
	char	    *name;
	COLORREF    color;
    } guicolor_tTable;

    static guicolor_tTable table[] =
    {
	{"Black",		RGB(0x00, 0x00, 0x00)},
	{"DarkGray",		RGB(0x80, 0x80, 0x80)},
	{"DarkGrey",		RGB(0x80, 0x80, 0x80)},
	{"Gray",		RGB(0xC0, 0xC0, 0xC0)},
	{"Grey",		RGB(0xC0, 0xC0, 0xC0)},
	{"LightGray",		RGB(0xE0, 0xE0, 0xE0)},
	{"LightGrey",		RGB(0xE0, 0xE0, 0xE0)},
	{"White",		RGB(0xFF, 0xFF, 0xFF)},
	{"DarkRed",		RGB(0x80, 0x00, 0x00)},
	{"Red",			RGB(0xFF, 0x00, 0x00)},
	{"LightRed",		RGB(0xFF, 0xA0, 0xA0)},
	{"DarkBlue",		RGB(0x00, 0x00, 0x80)},
	{"Blue",		RGB(0x00, 0x00, 0xFF)},
	{"LightBlue",		RGB(0xA0, 0xA0, 0xFF)},
	{"DarkGreen",		RGB(0x00, 0x80, 0x00)},
	{"Green",		RGB(0x00, 0xFF, 0x00)},
	{"LightGreen",		RGB(0xA0, 0xFF, 0xA0)},
	{"DarkCyan",		RGB(0x00, 0x80, 0x80)},
	{"Cyan",		RGB(0x00, 0xFF, 0xFF)},
	{"LightCyan",		RGB(0xA0, 0xFF, 0xFF)},
	{"DarkMagenta",		RGB(0x80, 0x00, 0x80)},
	{"Magenta",		RGB(0xFF, 0x00, 0xFF)},
	{"LightMagenta",	RGB(0xFF, 0xA0, 0xFF)},
	{"Brown",		RGB(0x80, 0x40, 0x40)},
	{"Yellow",		RGB(0xFF, 0xFF, 0x00)},
	{"LightYellow",		RGB(0xFF, 0xFF, 0xA0)},
	{"DarkYellow",		RGB(0xBB, 0xBB, 0x00)},
	{"SeaGreen",		RGB(0x2E, 0x8B, 0x57)},
	{"Orange",		RGB(0xFF, 0xA5, 0x00)},
	{"Purple",		RGB(0xA0, 0x20, 0xF0)},
	{"SlateBlue",		RGB(0x6A, 0x5A, 0xCD)},
	{"Violet",		RGB(0xEE, 0x82, 0xEE)},
    };

    typedef struct SysColorTable
    {
	char	    *name;
	int	    color;
    } SysColorTable;

    static SysColorTable sys_table[] =
    {
#ifdef WIN3264
	{"SYS_3DDKSHADOW", COLOR_3DDKSHADOW},
	{"SYS_3DHILIGHT", COLOR_3DHILIGHT},
#ifndef __MINGW32__
	{"SYS_3DHIGHLIGHT", COLOR_3DHIGHLIGHT},
#endif
	{"SYS_BTNHILIGHT", COLOR_BTNHILIGHT},
	{"SYS_BTNHIGHLIGHT", COLOR_BTNHIGHLIGHT},
	{"SYS_3DLIGHT", COLOR_3DLIGHT},
	{"SYS_3DSHADOW", COLOR_3DSHADOW},
	{"SYS_DESKTOP", COLOR_DESKTOP},
	{"SYS_INFOBK", COLOR_INFOBK},
	{"SYS_INFOTEXT", COLOR_INFOTEXT},
	{"SYS_3DFACE", COLOR_3DFACE},
#endif
	{"SYS_BTNFACE", COLOR_BTNFACE},
	{"SYS_BTNSHADOW", COLOR_BTNSHADOW},
	{"SYS_ACTIVEBORDER", COLOR_ACTIVEBORDER},
	{"SYS_ACTIVECAPTION", COLOR_ACTIVECAPTION},
	{"SYS_APPWORKSPACE", COLOR_APPWORKSPACE},
	{"SYS_BACKGROUND", COLOR_BACKGROUND},
	{"SYS_BTNTEXT", COLOR_BTNTEXT},
	{"SYS_CAPTIONTEXT", COLOR_CAPTIONTEXT},
	{"SYS_GRAYTEXT", COLOR_GRAYTEXT},
	{"SYS_HIGHLIGHT", COLOR_HIGHLIGHT},
	{"SYS_HIGHLIGHTTEXT", COLOR_HIGHLIGHTTEXT},
	{"SYS_INACTIVEBORDER", COLOR_INACTIVEBORDER},
	{"SYS_INACTIVECAPTION", COLOR_INACTIVECAPTION},
	{"SYS_INACTIVECAPTIONTEXT", COLOR_INACTIVECAPTIONTEXT},
	{"SYS_MENU", COLOR_MENU},
	{"SYS_MENUTEXT", COLOR_MENUTEXT},
	{"SYS_SCROLLBAR", COLOR_SCROLLBAR},
	{"SYS_WINDOW", COLOR_WINDOW},
	{"SYS_WINDOWFRAME", COLOR_WINDOWFRAME},
	{"SYS_WINDOWTEXT", COLOR_WINDOWTEXT}
    };

    int		    r, g, b;
    int		    i;

    if (name[0] == '#' && strlen(name) == 7)
    {
	/* Name is in "#rrggbb" format */
	r = hex_digit(name[1]) * 16 + hex_digit(name[2]);
	g = hex_digit(name[3]) * 16 + hex_digit(name[4]);
	b = hex_digit(name[5]) * 16 + hex_digit(name[6]);
	if (r < 0 || g < 0 || b < 0)
	    return (guicolor_T)-1;
	return RGB(r, g, b);
    }
    else
    {
	/* Check if the name is one of the colors we know */
	for (i = 0; i < sizeof(table) / sizeof(table[0]); i++)
	    if (STRICMP(name, table[i].name) == 0)
		return table[i].color;
    }

    /*
     * Try to look up a system colour.
     */
    for (i = 0; i < sizeof(sys_table) / sizeof(sys_table[0]); i++)
	if (STRICMP(name, sys_table[i].name) == 0)
	    return GetSysColor(sys_table[i].color);

    /*
     * Last attempt. Look in the file "$VIMRUNTIME/rgb.txt".
     */
    {
#define LINE_LEN 100
	FILE	*fd;
	char	line[LINE_LEN];
	char_u	*fname;

	fname = expand_env_save((char_u *)"$VIMRUNTIME/rgb.txt");
	if (fname == NULL)
	    return (guicolor_T)-1;

	fd = fopen((char *)fname, "rt");
	vim_free(fname);
	if (fd == NULL)
	    return (guicolor_T)-1;

	while (!feof(fd))
	{
	    int	    len;
	    int	    pos;
	    char    *color;

	    fgets(line, LINE_LEN, fd);
	    len = (int)STRLEN(line);

	    if (len <= 1 || line[len-1] != '\n')
		continue;

	    line[len-1] = '\0';

	    i = sscanf(line, "%d %d %d %n", &r, &g, &b, &pos);
	    if (i != 3)
		continue;

	    color = line + pos;

	    if (STRICMP(color, name) == 0)
	    {
		fclose(fd);
		return (guicolor_T) RGB(r, g, b);
	    }
	}

	fclose(fd);
    }

    return (guicolor_T)-1;
}
/*
 * Return OK if the key with the termcap name "name" is supported.
 */
    int
gui_mch_haskey(char_u *name)
{
    int i;

    for (i = 0; special_keys[i].vim_code1 != NUL; i++)
	if (name[0] == special_keys[i].vim_code0 &&
					 name[1] == special_keys[i].vim_code1)
	    return OK;
    return FAIL;
}

    void
gui_mch_beep(void)
{
    MessageBeep(MB_OK);
}
/*
 * Invert a rectangle from row r, column c, for nr rows and nc columns.
 */
    void
gui_mch_invert_rectangle(
    int	    r,
    int	    c,
    int	    nr,
    int	    nc)
{
    RECT    rc;

    /*
     * Note: InvertRect() excludes right and bottom of rectangle.
     */
    rc.left = FILL_X(c);
    rc.top = FILL_Y(r);
    rc.right = rc.left + nc * gui.char_width;
    rc.bottom = rc.top + nr * gui.char_height;
    InvertRect(s_hdc, &rc);
}

/*
 * Iconify the GUI window.
 */
    void
gui_mch_iconify(void)
{
    ShowWindow(s_hwnd, SW_MINIMIZE);
}
/*
 * Draw a cursor without focus.
 */
    void
gui_mch_draw_hollow_cursor(guicolor_T color)
{
    HBRUSH  hbr;
    RECT    rc;

    /*
     * Note: FrameRect() excludes right and bottom of rectangle.
     */
    rc.left = FILL_X(gui.col);
    rc.top = FILL_Y(gui.row);
    rc.right = rc.left + gui.char_width;
#ifdef FEAT_MBYTE
    if (mb_lefthalve(gui.row, gui.col))
	rc.right += gui.char_width;
#endif
    rc.bottom = rc.top + gui.char_height;
    hbr = CreateSolidBrush(color);
    FrameRect(s_hdc, &rc, hbr);
    DeleteBrush(hbr);
}
/*
 * Draw part of a cursor, "w" pixels wide, and "h" pixels high, using
 * color "color".
 */
    void
gui_mch_draw_part_cursor(
    int		w,
    int		h,
    guicolor_T	color)
{
    HBRUSH	hbr;
    RECT	rc;

    /*
     * Note: FillRect() excludes right and bottom of rectangle.
     */
    rc.left =
#ifdef FEAT_RIGHTLEFT
		/* vertical line should be on the right of current point */
		!(State & CMDLINE) && curwin->w_p_rl ? FILL_X(gui.col + 1) - w :
#endif
		    FILL_X(gui.col);
    rc.top = FILL_Y(gui.row) + gui.char_height - h;
    rc.right = rc.left + w;
    rc.bottom = rc.top + h;
    hbr = CreateSolidBrush(color);
    FillRect(s_hdc, &rc, hbr);
    DeleteBrush(hbr);
}

/*
 * Process a single Windows message.
 * If one is not available we hang until one is.
 */
    static void
process_message(void)
{
    MSG		msg;
    UINT	vk = 0;		/* Virtual key */
    char_u	string[3];
    int		i;
    int		modifiers = 0;
    int		key;

    GetMessage(&msg, NULL, 0, 0);

#ifdef FEAT_OLE
    /* Look after OLE Automation commands */
    if (msg.message == WM_OLE)
    {
	char_u *str = (char_u *)msg.lParam;
	add_to_input_buf(str, (int)STRLEN(str));
	vim_free(str);
	return;
    }
#endif

#ifdef FEAT_SNIFF
    if (sniff_request_waiting && want_sniff_request)
    {
	static char_u bytes[3] = {CSI, (char_u)KS_EXTRA, (char_u)KE_SNIFF};
	add_to_input_buf(bytes,3); /* K_SNIFF */
	sniff_request_waiting = 0;
	want_sniff_request = 0;
	/* request is handled in normal.c */
    }
    if (msg.message == WM_USER)
	return;
#endif

#ifdef MSWIN_FIND_REPLACE
    /* Don't process messages used by the dialog */
    if ((s_findrep_hwnd) && (IsDialogMessage(s_findrep_hwnd, &msg)))
    {
	HandleMouseHide(msg.message, msg.lParam);
	return;
    }
#endif

    /*
     * Check if it's a special key that we recognise.  If not, call
     * TranslateMessage().
     */
    if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)
    {
	vk = (int) msg.wParam;
	/* handle key after dead key, but ignore shift, alt and control */
	if (dead_key && vk != VK_SHIFT && vk != VK_MENU && vk != VK_CONTROL)
	{
	    dead_key = 0;
	    /* handle non-alphabetic keys (ones that hopefully cannot generate
	     * umlaut-characters), unless when control is down */
	    if (vk < 'A' || vk > 'Z' || (GetKeyState(VK_CONTROL) & 0x8000))
	    {
		MSG dm;

		dm.message = msg.message;
		dm.hwnd = msg.hwnd;
		dm.wParam = VK_SPACE;
		MyTranslateMessage(&dm);	/* generate dead character */
		if (vk != VK_SPACE) /* and send current character once more */
		    PostMessage(msg.hwnd, msg.message, msg.wParam, msg.lParam);
		return;
	    }
	}

	/* Check for CTRL-BREAK */
	if (vk == VK_CANCEL)
	{
	    trash_input_buf();
	    got_int = TRUE;
	    string[0] = Ctrl_C;
	    add_to_input_buf(string, 1);
	}

	for (i = 0; special_keys[i].key_sym != 0; i++)
	{
	    /* ignore VK_SPACE when ALT key pressed: system menu */
	    if (special_keys[i].key_sym == vk
		    && (vk != VK_SPACE || !(GetKeyState(VK_MENU) & 0x8000)))
	    {
#ifdef FEAT_MENU
		/* Check for <F10>: Windows selects the menu.  Ignore it when
		 * 'winaltkeys' is "yes" or "menu" */
		if (vk == VK_F10
			&& gui.menu_is_active
			&& (*p_wak == 'y' || *p_wak == 'm'))
		    break;
#endif
		if (GetKeyState(VK_SHIFT) & 0x8000)
		    modifiers |= MOD_MASK_SHIFT;
		/*
		 * Don't use caps-lock as shift, because these are special keys
		 * being considered here, and we only want letters to get
		 * shifted -- webb
		 */
		/*
		if (GetKeyState(VK_CAPITAL) & 0x0001)
		    modifiers ^= MOD_MASK_SHIFT;
		*/
		if (GetKeyState(VK_CONTROL) & 0x8000)
		    modifiers |= MOD_MASK_CTRL;
		if (GetKeyState(VK_MENU) & 0x8000)
		    modifiers |= MOD_MASK_ALT;

		if (special_keys[i].vim_code1 == NUL)
		    key = special_keys[i].vim_code0;
		else
		    key = TO_SPECIAL(special_keys[i].vim_code0,
						   special_keys[i].vim_code1);
		key = simplify_key(key, &modifiers);
		if (key == CSI)
		    key = K_CSI;

		if (modifiers)
		{
		    string[0] = CSI;
		    string[1] = KS_MODIFIER;
		    string[2] = modifiers;
		    add_to_input_buf(string, 3);
		}

		if (IS_SPECIAL(key))
		{
		    string[0] = CSI;
		    string[1] = K_SECOND(key);
		    string[2] = K_THIRD(key);
		    add_to_input_buf(string, 3);
		}
		else
		{
		    int	len = 1;

		    string[0] = key;
#ifdef FEAT_MBYTE
		    if (input_conv.vc_type != CONV_NONE)
			len = convert_input(string, len, sizeof(string));
#endif
		    add_to_input_buf(string, len);
		}
		break;
	    }
	}
	if (special_keys[i].key_sym == 0)
	{
	    /* Some keys need C-S- where they should only need C- */
	    if ((GetKeyState(VK_CONTROL) & 0x8000)
		    && !(GetKeyState(VK_SHIFT) & 0x8000)
		    && !(GetKeyState(VK_MENU) & 0x8000))
	    {
		if (vk == '6')
		{
		    string[0] = Ctrl_HAT;
		    add_to_input_buf(string, 1);
		}
		/* vk == 0xDB AZERTY for CTRL-'-', but CTRL-[ for * QWERTY! */
		else if (vk == 0xBD)	/* QWERTY for CTRL-'-' */
		{
		    string[0] = Ctrl__;
		    add_to_input_buf(string, 1);
		}
		/* Japanese keyboard map '^' to vk == 0xDE */
		else if (MapVirtualKey(vk, 2) == (UINT)'^')
		{
		    string[0] = Ctrl_HAT;
		    add_to_input_buf(string, 1);
		}
		else
		    MyTranslateMessage(&msg);
	    }
	    else
		MyTranslateMessage(&msg);
	}
    }
#ifdef FEAT_MBYTE_IME
    else if (msg.message == WM_IME_NOTIFY)
	_OnImeNotify(msg.hwnd, (DWORD)msg.wParam, (DWORD)msg.lParam);
#endif
#if !defined(FEAT_MBYTE_IME) && defined(GLOBAL_IME)
/* GIME_TEST */
    else if (msg.message == WM_IME_STARTCOMPOSITION)
    {
	POINT point;

	global_ime_set_font(&norm_logfont);
	point.x = FILL_X(gui.col);
	point.y = FILL_Y(gui.row);
	MapWindowPoints(s_textArea, s_hwnd, &point, 1);
	global_ime_set_position(&point);
    }
#endif
#ifdef FEAT_MENU
    /* Check for <F10>: Windows selects the menu.  Don't let Windows handle it
     * when 'winaltkeys' is "no" */
    if (vk != VK_F10 || *p_wak != 'n')
#endif
	DispatchMessage(&msg);
}

/*
 * Catch up with any queued events.  This may put keyboard input into the
 * input buffer, call resize call-backs, trigger timers etc.  If there is
 * nothing in the event queue (& no timers pending), then we return
 * immediately.
 */
    void
gui_mch_update(void)
{
    MSG	    msg;

    if (!s_busy_processing)
	while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)
						  && !vim_is_input_buf_full())
	    process_message();
}

/*
 * GUI input routine called by gui_wait_for_chars().  Waits for a character
 * from the keyboard.
 *  wtime == -1	    Wait forever.
 *  wtime == 0	    This should never happen.
 *  wtime > 0	    Wait wtime milliseconds for a character.
 * Returns OK if a character was found to be available within the given time,
 * or FAIL otherwise.
 */
    int
gui_mch_wait_for_chars(int wtime)
{
    MSG		msg;
    int		focus;

    s_timed_out = FALSE;

    if (wtime > 0)
    {
	/* Don't do anything while processing a (scroll) message. */
	if (s_busy_processing)
	    return FAIL;
	s_wait_timer = (UINT)SetTimer(NULL, 0, (UINT)wtime, (TIMERPROC)_OnTimer);
    }

    focus = gui.in_focus;
    while (!s_timed_out)
    {
	/* Stop or start blinking when focus changes */
	if (gui.in_focus != focus)
	{
	    if (gui.in_focus)
		gui_mch_start_blink();
	    else
		gui_mch_stop_blink();
	    focus = gui.in_focus;
	}

	if (s_need_activate)
	{
#ifdef WIN32
	    (void)SetForegroundWindow(s_hwnd);
#else
	    (void)SetActiveWindow(s_hwnd);
#endif
	    s_need_activate = FALSE;
	}

	/*
	 * Don't use gui_mch_update() because then we will spin-lock until a
	 * char arrives, instead we use GetMessage() to hang until an
	 * event arrives.  No need to check for input_buf_full because we are
	 * returning as soon as it contains a single char -- webb
	 */
	process_message();


	if (!vim_is_input_buf_empty())
	{
	    if (s_wait_timer != 0 && !s_timed_out)
	    {
		KillTimer(NULL, s_wait_timer);

		/* Eat spurious WM_TIMER messages */
		while (PeekMessage(&msg, s_hwnd,
					      WM_TIMER, WM_TIMER, PM_REMOVE));
		s_wait_timer = 0;
	    }
	    return OK;
	}
    }
    return FAIL;
}

/*
 * Clear a rectangular region of the screen from text pos (row1, col1) to
 * (row2, col2) inclusive.
 */
    void
gui_mch_clear_block(
    int		row1,
    int		col1,
    int		row2,
    int		col2)
{
    RECT	rc;

    /*
     * Clear one extra pixel at the far right, for when bold characters have
     * spilled over to the window border.
     * Note: FillRect() excludes right and bottom of rectangle.
     */
    rc.left = FILL_X(col1);
    rc.top = FILL_Y(row1);
    rc.right = FILL_X(col2 + 1) + (col2 == Columns - 1);
    rc.bottom = FILL_Y(row2 + 1);
    clear_rect(&rc);
}

/*
 * Clear the whole text window.
 */
    void
gui_mch_clear_all(void)
{
    RECT    rc;

    rc.left = 0;
    rc.top = 0;
    rc.right = Columns * gui.char_width + 2 * gui.border_width;
    rc.bottom = Rows * gui.char_height + 2 * gui.border_width;
    clear_rect(&rc);
}
/*
 * Menu stuff.
 */

    void
gui_mch_enable_menu(int flag)
{
#ifdef FEAT_MENU
    SetMenu(s_hwnd, flag ? s_menuBar : NULL);
#endif
}

    void
gui_mch_set_menu_pos(
    int	    x,
    int	    y,
    int	    w,
    int	    h)
{
    /* It will be in the right place anyway */
}

#if defined(FEAT_MENU) || defined(PROTO)
/*
 * Make menu item hidden or not hidden
 */
    void
gui_mch_menu_hidden(
    vimmenu_T	*menu,
    int		hidden)
{
    /*
     * This doesn't do what we want.  Hmm, just grey the menu items for now.
     */
    /*
    if (hidden)
	EnableMenuItem(s_menuBar, menu->id, MF_BYCOMMAND | MF_DISABLED);
    else
	EnableMenuItem(s_menuBar, menu->id, MF_BYCOMMAND | MF_ENABLED);
    */
    gui_mch_menu_grey(menu, hidden);
}

/*
 * This is called after setting all the menus to grey/hidden or not.
 */
    void
gui_mch_draw_menubar(void)
{
    DrawMenuBar(s_hwnd);
}
#endif /*FEAT_MENU*/

/* cproto doesn't create a prototype for main() */
int main __ARGS((int argc, char **argv));

#ifndef PROTO
void
#ifdef VIMDLL
_export
#endif
_cdecl
SaveInst(HINSTANCE hInst)
{
    s_hinst = hInst;
}
#endif

/*
 * Return the lightness of a pixel.  White is 255.
 */
    int
gui_mch_get_lightness(pixel)
    guicolor_T	pixel;
{
    return (GetRValue(pixel)*3 + GetGValue(pixel)*6 + GetBValue(pixel)) / 10;
}

#if (defined(FEAT_SYN_HL) && defined(FEAT_EVAL)) || defined(PROTO)
/*
 * Return the RGB value of a pixel as "#RRGGBB".
 */
    char_u *
gui_mch_get_rgb(
    guicolor_T	pixel)
{
    static char_u retval[10];

    sprintf((char *)retval, "#%02x%02x%02x",
	    GetRValue(pixel), GetGValue(pixel), GetBValue(pixel));
    return retval;
}
#endif
#if (defined(FEAT_SYN_HL) && defined(FEAT_PRINTER)) || defined(PROTO)
/*
 * Return the RGB value of a pixel as long.
 */
    unsigned long
gui_mch_get_rgb_long(
    guicolor_T	pixel)
{
    return (GetRValue(pixel) << 16) + (GetGValue(pixel) << 8) + GetBValue(pixel);
}
#endif

#if defined(FEAT_GUI_DIALOG) || defined(PROTO)
/* Convert pixels in X to dialog units */
    static WORD
PixelToDialogX(int numPixels)
{
    return (WORD)((numPixels * 4) / s_dlgfntwidth);
}

/* Convert pixels in Y to dialog units */
    static WORD
PixelToDialogY(int numPixels)
{
    return (WORD)((numPixels * 8) / s_dlgfntheight);
}

/* Return the width in pixels of the given text in the given DC. */
    static int
GetTextWidth(HDC hdc, char_u *str, int len)
{
    SIZE    size;

    GetTextExtentPoint(hdc, str, len, &size);
    return size.cx;
}

/*
 * A quick little routine that will center one window over another, handy for
 * dialog boxes.  Taken from the Win32SDK samples.
 */
    static BOOL
CenterWindow(
    HWND hwndChild,
    HWND hwndParent)
{
    RECT    rChild, rParent;
    int     wChild, hChild, wParent, hParent;
    int     wScreen, hScreen, xNew, yNew;
    HDC     hdc;

    GetWindowRect(hwndChild, &rChild);
    wChild = rChild.right - rChild.left;
    hChild = rChild.bottom - rChild.top;

    GetWindowRect(hwndParent, &rParent);
    wParent = rParent.right - rParent.left;
    hParent = rParent.bottom - rParent.top;

    hdc = GetDC(hwndChild);
    wScreen = GetDeviceCaps (hdc, HORZRES);
    hScreen = GetDeviceCaps (hdc, VERTRES);
    ReleaseDC(hwndChild, hdc);

    xNew = rParent.left + ((wParent - wChild) /2);
    if (xNew < 0)
    {
	xNew = 0;
    }
    else if ((xNew+wChild) > wScreen)
    {
	xNew = wScreen - wChild;
    }

    yNew = rParent.top	+ ((hParent - hChild) /2);
    if (yNew < 0)
	yNew = 0;
    else if ((yNew+hChild) > hScreen)
	yNew = hScreen - hChild;

    return SetWindowPos(hwndChild, NULL, xNew, yNew, 0, 0,
						   SWP_NOSIZE | SWP_NOZORDER);
}
#endif /* FEAT_GUI_DIALOG */

void
gui_mch_activate_window(void)
{
    (void)SetActiveWindow(s_hwnd);
}

#if defined(FEAT_TOOLBAR) || defined(PROTO)
    void
gui_mch_show_toolbar(int showit)
{
    if (s_toolbarhwnd == NULL)
	return;

    if (showit)
	ShowWindow(s_toolbarhwnd, SW_SHOW);
    else
	ShowWindow(s_toolbarhwnd, SW_HIDE);
}

/* Then number of bitmaps is fixed.  Exit is missing! */
#define TOOLBAR_BITMAP_COUNT 31

#endif

/*
 * ":simalt" command.
 */
    void
ex_simalt(exarg_T *eap)
{
    char_u *keys = eap->arg;

    PostMessage(s_hwnd, WM_SYSCOMMAND, (WPARAM)SC_KEYMENU, (LPARAM)0);
    while (*keys)
    {
	if (*keys == '~')
	    *keys = ' ';	    /* for showing system menu */
	PostMessage(s_hwnd, WM_CHAR, (WPARAM)*keys, (LPARAM)0);
	keys++;
    }
}

/*
 * Create the find & replace dialogs
 * You can't have both at once: ":find" when replace is showing, destroys
 * the replace dialog first.
 */
#ifdef MSWIN_FIND_REPLACE
    static void
initialise_findrep(char_u *initial_string)
{
    s_findrep_struct.hwndOwner = s_hwnd;
    s_findrep_struct.Flags = FR_DOWN;
    if (p_ic)
	s_findrep_struct.Flags &= ~FR_MATCHCASE;
    else
	s_findrep_struct.Flags |= FR_MATCHCASE;
    if (initial_string != NULL && *initial_string != NUL)
    {
	STRNCPY(s_findrep_struct.lpstrFindWhat, initial_string,
					       s_findrep_struct.wFindWhatLen);
	s_findrep_struct.lpstrFindWhat[s_findrep_struct.wFindWhatLen - 1] = NUL;
	s_findrep_struct.lpstrReplaceWith[0] = NUL;
    }
}
#endif

    void
gui_mch_find_dialog(exarg_T *eap)
{
#ifdef MSWIN_FIND_REPLACE
    if (s_findrep_msg != 0)
    {
	if (IsWindow(s_findrep_hwnd) && (s_findrep_is_find == FALSE))
	    DestroyWindow(s_findrep_hwnd);

	if (!IsWindow(s_findrep_hwnd))
	{
	    initialise_findrep(eap->arg);
	    s_findrep_hwnd = FindText((LPFINDREPLACE) &s_findrep_struct);
	}

	(void)SetWindowText(s_findrep_hwnd,
		       (LPCSTR)_("Find string (use '\\\\' to find  a '\\')"));
	(void)SetFocus(s_findrep_hwnd);

	s_findrep_is_find = TRUE;
    }
#endif
}


    void
gui_mch_replace_dialog(exarg_T *eap)
{
#ifdef MSWIN_FIND_REPLACE
    if (s_findrep_msg != 0)
    {
	if (IsWindow(s_findrep_hwnd) && (s_findrep_is_find == TRUE))
	    DestroyWindow(s_findrep_hwnd);

	if (!IsWindow(s_findrep_hwnd))
	{
	    initialise_findrep(eap->arg);
	    s_findrep_hwnd = ReplaceText((LPFINDREPLACE) &s_findrep_struct);
	}

	(void)SetWindowText(s_findrep_hwnd,
		    (LPCSTR)_("Find & Replace (use '\\\\' to find  a '\\')"));
	(void)SetFocus(s_findrep_hwnd);

	s_findrep_is_find = FALSE;
    }
#endif
}


/*
 * Set visibility of the pointer.
 */
    void
gui_mch_mousehide(int hide)
{
    if (hide != gui.pointer_hidden)
    {
	ShowCursor(!hide);
	gui.pointer_hidden = hide;
    }
}

    static void
gui_mch_show_popupmenu_at(vimmenu_T *menu, int x, int y)
{
    (void)TrackPopupMenu(
	(HMENU)menu->submenu_id,
	TPM_LEFTALIGN | TPM_LEFTBUTTON,
	x, y,
	(int)0,	    /*reserved param*/
	s_hwnd,
	NULL);
    /*
     * NOTE: The pop-up menu can eat the mouse up event.
     * We deal with this in normal.c.
     */
}

/*
 * Got a message when the system will go down.
 */
    static void
_OnEndSession(void)
{
    ml_close_notmod();		    /* close all not-modified buffers */
    ml_sync_all(FALSE, FALSE);	    /* preserve all swap files */
    ml_close_all(FALSE);	    /* close all memfiles, without deleting */
    getout(1);			    /* exit Vim properly */
}

/*
 * Get this message when the user clicks on the cross in the top right corner
 * of a Windows95 window.
 */
    static void
_OnClose(
    HWND hwnd)
{
    gui_shell_closed();
}

/*
 * Get a message when the window is being destroyed.
 */
    static void
_OnDestroy(
    HWND hwnd)
{
#ifdef WIN16_3DLOOK
    Ctl3dUnregister(s_hinst);
#endif
    if (!destroying)
	_OnClose(hwnd);
}

    static void
_OnPaint(
    HWND hwnd)
{
    if (!IsMinimized(hwnd))
    {
	PAINTSTRUCT ps;

	out_flush();	    /* make sure all output has been processed */
	(void)BeginPaint(hwnd, &ps);

#ifdef FEAT_MBYTE
	/* prevent multi-byte characters from misprinting on an invalid
	 * rectangle */
	if (has_mbyte)
	{
	    RECT rect;

	    GetClientRect(hwnd, &rect);
	    ps.rcPaint.left = rect.left;
	    ps.rcPaint.right = rect.right;
	}
#endif

	if (!IsRectEmpty(&ps.rcPaint))
	    gui_redraw(ps.rcPaint.left, ps.rcPaint.top,
		    ps.rcPaint.right - ps.rcPaint.left + 1,
		    ps.rcPaint.bottom - ps.rcPaint.top + 1);
	EndPaint(hwnd, &ps);
    }
}

    static void
_OnSize(
    HWND hwnd,
    UINT state,
    int cx,
    int cy)
{
    if (!IsMinimized(hwnd))
    {
	gui_resize_shell(cx, cy);

#ifdef FEAT_MENU
	/* Menu bar may wrap differently now */
	gui_mswin_get_menu_height(TRUE);
#endif
    }
}

    static void
_OnSetFocus(
    HWND hwnd,
    HWND hwndOldFocus)
{
    gui_focus_change(TRUE);
    (void)DefWindowProc(hwnd, WM_SETFOCUS, (WPARAM)hwndOldFocus, 0);
}

    static void
_OnKillFocus(
    HWND hwnd,
    HWND hwndNewFocus)
{
    gui_focus_change(FALSE);
    (void)DefWindowProc(hwnd, WM_KILLFOCUS, (WPARAM)hwndNewFocus, 0);
}

/*
 * Get a message when the user switches back to vim
 */
    static LRESULT
_OnActivateApp(
    HWND hwnd,
    BOOL fActivate,
    DWORD dwThreadId)
{
    /* we call gui_focus_change() in _OnSetFocus() */
    /* gui_focus_change((int)fActivate); */
    return DefWindowProc(hwnd, WM_ACTIVATEAPP, fActivate, dwThreadId);
}

    static BOOL
_OnCreate(HWND hwnd, LPCREATESTRUCT lpcs)
{
#ifdef FEAT_MBYTE
    /* get system fixed font size*/
    static const char ach[] = {'W', 'f', 'g', 'M'};

    HDC	    hdc = GetWindowDC(hwnd);
    HFONT   hfntOld = SelectFont(hdc, GetStockObject(SYSTEM_FIXED_FONT));
    SIZE    siz;

    GetTextExtentPoint(hdc, ach, sizeof(ach), &siz);

    sysfixed_width = siz.cx / sizeof(ach);
    /*
     * Make characters one pixel higher (by default), so that italic and bold
     * fonts don't draw off the bottom of their character space.  Also means
     * that we can underline an underscore for normal text.
     */
    sysfixed_height = siz.cy + p_linespace;

    SelectFont(hdc, hfntOld);

    ReleaseDC(hwnd, hdc);
#endif

    return 0;
}

#if defined(FEAT_WINDOWS) || defined(PROTO)
    void
gui_mch_destroy_scrollbar(scrollbar_T *sb)
{
    DestroyWindow(sb->id);
}
#endif

/*
 * Get current x mouse coordinate in text window.
 * Return -1 when unknown.
 */
    int
gui_mch_get_mouse_x(void)
{
    RECT rct;
    POINT mp;

    (void)GetWindowRect(s_textArea, &rct);
    (void)GetCursorPos((LPPOINT)&mp);
    return (int)(mp.x - rct.left);
}

/*
 * Get current y mouse coordinate in text window.
 * Return -1 when unknown.
 */
    int
gui_mch_get_mouse_y(void)
{
    RECT rct;
    POINT mp;

    (void)GetWindowRect(s_textArea, &rct);
    (void)GetCursorPos((LPPOINT)&mp);
    return (int)(mp.y - rct.top);
}

/*
 * Move mouse pointer to character at (x, y).
 */
    void
gui_mch_setmouse(int x, int y)
{
    RECT rct;

    (void)GetWindowRect(s_textArea, &rct);
    (void)SetCursorPos(x + gui.border_offset + rct.left,
		       y + gui.border_offset + rct.top);
}

    static void
gui_mswin_get_valid_dimensions(
    int w,
    int h,
    int *valid_w,
    int *valid_h)
{
    int	    base_width, base_height;

    base_width = gui_get_base_width()
	+ GetSystemMetrics(SM_CXFRAME) * 2;
    base_height = gui_get_base_height()
	+ GetSystemMetrics(SM_CYFRAME) * 2
	+ GetSystemMetrics(SM_CYCAPTION)
#ifdef FEAT_MENU
	+ gui_mswin_get_menu_height(FALSE)
#endif
	;
    *valid_w = base_width +
		    ((w - base_width) / gui.char_width) * gui.char_width;
    *valid_h = base_height +
		    ((h - base_height) / gui.char_height) * gui.char_height;
}

    void
gui_mch_get_screen_dimensions(int *screen_w, int *screen_h)
{

    *screen_w = GetSystemMetrics(SM_CXSCREEN)
	      - GetSystemMetrics(SM_CXFRAME) * 2;
    *screen_h = GetSystemMetrics(SM_CYFULLSCREEN)
	      - GetSystemMetrics(SM_CYFRAME) * 2
	      - GetSystemMetrics(SM_CYCAPTION)
#ifdef FEAT_MENU
	      - gui_mswin_get_menu_height(FALSE)
#endif
	      ;
}

    void
gui_mch_flash(int msec)
{
    RECT    rc;

    /*
     * Note: InvertRect() excludes right and bottom of rectangle.
     */
    rc.left = 0;
    rc.top = 0;
    rc.right = gui.num_cols * gui.char_width;
    rc.bottom = gui.num_rows * gui.char_height;
    InvertRect(s_hdc, &rc);
    gui_mch_flush();			/* make sure it's displayed */

    ui_delay((long)msec, TRUE);	/* wait for a few msec */

    InvertRect(s_hdc, &rc);
}

/*
 * Delete the given number of lines from the given row, scrolling up any
 * text further down within the scroll region.
 */
    void
gui_mch_delete_lines(
    int	    row,
    int	    num_lines)
{
    RECT	rc;

    rc.left = FILL_X(gui.scroll_region_left);
    rc.right = FILL_X(gui.scroll_region_right + 1);
    rc.top = FILL_Y(row);
    rc.bottom = FILL_Y(gui.scroll_region_bot + 1);
    /* The SW_INVALIDATE is required when part of the window is covered or
     * off-screen.  How do we avoid it when it's not needed? */
    ScrollWindowEx(s_textArea, 0, -num_lines * gui.char_height,
	    &rc, &rc, NULL, NULL, SW_INVALIDATE);

    UpdateWindow(s_textArea);
    /* This seems to be required to avoid the cursor disappearing when
     * scrolling such that the cursor ends up in the top-left character on
     * the screen...   But why?  (Webb) */
    /* It's probably fixed by disabling drawing the cursor while scrolling. */
    /* gui.cursor_is_valid = FALSE; */

    gui_clear_block(gui.scroll_region_bot - num_lines + 1,
						       gui.scroll_region_left,
	gui.scroll_region_bot, gui.scroll_region_right);
}

/*
 * Insert the given number of lines before the given row, scrolling down any
 * following text within the scroll region.
 */
    void
gui_mch_insert_lines(
    int		row,
    int		num_lines)
{
    RECT	rc;

    rc.left = FILL_X(gui.scroll_region_left);
    rc.right = FILL_X(gui.scroll_region_right + 1);
    rc.top = FILL_Y(row);
    rc.bottom = FILL_Y(gui.scroll_region_bot + 1);
    /* The SW_INVALIDATE is required when part of the window is covered or
     * off-screen.  How do we avoid it when it's not needed? */
    ScrollWindowEx(s_textArea, 0, num_lines * gui.char_height,
	    &rc, &rc, NULL, NULL, SW_INVALIDATE);

    gui_undraw_cursor();	/* Is this really necessary? */
    UpdateWindow(s_textArea);

    gui_clear_block(row, gui.scroll_region_left,
				row + num_lines - 1, gui.scroll_region_right);
}


    void
gui_mch_exit(int rc)
{
    ReleaseDC(s_textArea, s_hdc);
    DeleteObject(s_brush);

#ifdef FEAT_TEAROFF
    /* Unload the tearoff bitmap */
    (void)DeleteObject((HGDIOBJ)s_htearbitmap);
#endif

    /* Destroy our window (if we have one). */
    if (s_hwnd != NULL)
    {
	destroying = TRUE;	/* ignore WM_DESTROY message now */
	DestroyWindow(s_hwnd);
    }

#ifdef GLOBAL_IME
    global_ime_end();
#endif
}

/*
 * Initialise vim to use the font with the given name.	Return FAIL if the font
 * could not be loaded, OK otherwise.
 */
    int
gui_mch_init_font(char_u *font_name, int fontset)
{
    LOGFONT	lf;
    GuiFont	font = NOFONT;
    char	*p;

    /* Load the font */
    if (get_logfont(&lf, font_name, NULL))
	font = get_font_handle(&lf);
    if (font == NOFONT)
	return FAIL;
    if (font_name == NULL)
	font_name = lf.lfFaceName;
#if defined(FEAT_MBYTE_IME) || defined(GLOBAL_IME)
    norm_logfont = lf;
#endif
#ifdef FEAT_MBYTE_IME
    im_set_font(&lf);
#endif
    gui_mch_free_font(gui.norm_font);
    gui.norm_font = font;
    current_font_height = lf.lfHeight;
    GetFontSize(font);
    hl_set_font_name(lf.lfFaceName);
    if (STRCMP(font_name, "*") == 0)
    {
	struct charset_pair *cp;

	/* Try to find a charset we recognize. */
	for (cp = charset_pairs; cp->name != NULL; ++cp)
	    if (lf.lfCharSet == cp->charset)
		break;

	p = alloc((unsigned)(strlen(lf.lfFaceName) + 14
		    + (cp->name == NULL ? 0 : strlen(cp->name) + 2)));
	if (p != NULL)
	{
	    /* make a normal font string out of the lf thing:*/
	    sprintf(p, "%s:h%d", lf.lfFaceName, pixels_to_points(
			 lf.lfHeight < 0 ? -lf.lfHeight : lf.lfHeight, TRUE));
	    vim_free(p_guifont);
	    p_guifont = p;
	    while (*p)
	    {
		if (*p == ' ')
		    *p = '_';
		++p;
	    }
#ifndef MSWIN16_FASTTEXT
	    if (lf.lfItalic)
		strcat(p, ":i");
	    if (lf.lfWeight >= FW_BOLD)
		strcat(p, ":b");
#endif
	    if (lf.lfUnderline)
		strcat(p, ":u");
	    if (lf.lfStrikeOut)
		strcat(p, ":s");
	    if (cp->name != NULL)
	    {
		strcat(p, ":c");
		strcat(p, cp->name);
	    }
	}
    }
#ifndef MSWIN16_FASTTEXT
    if (!lf.lfItalic)
    {
	lf.lfItalic = TRUE;
	gui.ital_font = get_font_handle(&lf);
	lf.lfItalic = FALSE;
    }
    if (lf.lfWeight < FW_BOLD)
    {
	lf.lfWeight = FW_BOLD;
	gui.bold_font = get_font_handle(&lf);
	if (!lf.lfItalic)
	{
	    lf.lfItalic = TRUE;
	    gui.boldital_font = get_font_handle(&lf);
	}
    }
#endif

    return OK;
}

/*
 * Return TRUE if the GUI window is maximized, filling the whole screen.
 */
    int
gui_mch_maximized()
{
    return IsZoomed(s_hwnd);
}

/*
 * Called when the font changed while the window is maximized.  Compute the
 * new Rows and Columsn.
 */
    void
gui_mch_newfont()
{
    RECT	rect;

    GetWindowRect(s_hwnd, &rect);
    gui_resize_shell(rect.right - rect.left
			- GetSystemMetrics(SM_CXFRAME) * 2,
		     rect.bottom - rect.top
		        - GetSystemMetrics(SM_CYFRAME) * 2
			- GetSystemMetrics(SM_CYCAPTION)
#ifdef FEAT_MENU
			- gui_mswin_get_menu_height(FALSE)
#endif
	    );
}

/*
 * Set the window title
 */
    void
gui_mch_settitle(
    char_u  *title,
    char_u  *icon)
{
    SetWindowText(s_hwnd, (LPCSTR)(title == NULL ? "VIM" : (char *)title));
}

#ifdef FEAT_MOUSESHAPE
/* Table for shape IDCs.  Keep in sync with the mshape_names[] table in
 * misc2.c! */
static LPCSTR mshape_idcs[] =
{
    MAKEINTRESOURCE(IDC_ARROW),		/* arrow */
    MAKEINTRESOURCE(0),			/* blank */
    MAKEINTRESOURCE(IDC_IBEAM),		/* beam */
    MAKEINTRESOURCE(IDC_SIZENS),	/* updown */
    MAKEINTRESOURCE(IDC_SIZENS),	/* udsizing */
    MAKEINTRESOURCE(IDC_SIZEWE),	/* leftright */
    MAKEINTRESOURCE(IDC_SIZEWE),	/* lrsizing */
    MAKEINTRESOURCE(IDC_WAIT),		/* busy */
#ifdef WIN3264
    MAKEINTRESOURCE(IDC_NO),		/* no */
#else
    MAKEINTRESOURCE(IDC_ICON),		/* no */
#endif
    MAKEINTRESOURCE(IDC_ARROW),		/* crosshair */
    MAKEINTRESOURCE(IDC_ARROW),		/* hand1 */
    MAKEINTRESOURCE(IDC_ARROW),		/* hand2 */
    MAKEINTRESOURCE(IDC_ARROW),		/* pencil */
    MAKEINTRESOURCE(IDC_ARROW),		/* question */
    MAKEINTRESOURCE(IDC_ARROW),		/* right-arrow */
    MAKEINTRESOURCE(IDC_UPARROW),	/* up-arrow */
    MAKEINTRESOURCE(IDC_ARROW)		/* last one */
};

    void
mch_set_mouse_shape(int shape)
{
    LPCSTR idc;

    if (shape == MSHAPE_HIDE)
	ShowCursor(FALSE);
    else
    {
	if (shape >= MSHAPE_NUMBERED)
	    idc = MAKEINTRESOURCE(IDC_ARROW);
	else
	    idc = mshape_idcs[shape];
#ifdef _WIN64
	SetClassLongPtr(s_textArea, GCLP_HCURSOR, (LONG_PTR)LoadCursor(NULL, idc));
#else
# ifdef WIN32
	SetClassLong(s_textArea, GCL_HCURSOR, (LONG)LoadCursor(NULL, idc));
# else
	SetClassWord(s_textArea, GCW_HCURSOR, LoadCursor(NULL, idc));
# endif
#endif
	if (!p_mh)
	{
	    POINT mp;

	    /* Set the position to make it redrawn with the new shape. */
	    (void)GetCursorPos((LPPOINT)&mp);
	    (void)SetCursorPos(mp.x, mp.y);
	    ShowCursor(TRUE);
	}
    }
}
#endif

#ifdef FEAT_BROWSE
/*
 * Convert the string s to the proper format for a filter string by replacing
 * the \t and \n delimeters with \0.
 * Returns the converted string in allocated memory.
 */
    static char_u *
convert_filter(char_u *s)
{
    char_u	*res;
    unsigned	s_len = (unsigned)STRLEN(s);
    unsigned	i;

    res = alloc(s_len + 3);
    if (res != NULL)
    {
	STRCPY(res, s);
	for (i = 0; i < s_len; ++i)
	    if ((res[i] == '\t') || (res[i] == '\n'))
		res[i] = '\0';
	/* Add two extra NULs to make sure it's properly terminated. */
	res[s_len + 1] = NUL;
	res[s_len + 2] = NUL;
    }
    return res;
}

/*
 * Pop open a file browser and return the file selected, in allocated memory,
 * or NULL if Cancel is hit.
 *  saving  - TRUE if the file will be saved to, FALSE if it will be opened.
 *  title   - Title message for the file browser dialog.
 *  dflt    - Default name of file.
 *  ext     - Default extension to be added to files without extensions.
 *  initdir - directory in which to open the browser (NULL = current dir)
 *  filter  - Filter for matched files to choose from.
 */
    char_u *
gui_mch_browse(
	int saving,
	char_u *title,
	char_u *dflt,
	char_u *ext,
	char_u *initdir,
	char_u *filter)
{
    OPENFILENAME	fileStruct;
    char_u		fileBuf[MAXPATHL], *p;

    if (dflt == NULL)
	fileBuf[0] = '\0';
    else
    {
	STRNCPY(fileBuf, dflt, MAXPATHL - 1);
	fileBuf[MAXPATHL - 1] = NUL;
    }

    /* Convert the filter to Windows format. */
    filter = convert_filter(filter);

    memset(&fileStruct, 0, sizeof(OPENFILENAME));
    fileStruct.lStructSize = sizeof(OPENFILENAME);
    fileStruct.lpstrFilter = filter;
    fileStruct.lpstrFile = fileBuf;
    fileStruct.nMaxFile = MAXPATHL;
    fileStruct.lpstrTitle = title;
    fileStruct.lpstrDefExt = ext;
    fileStruct.hwndOwner = s_hwnd;		/* main Vim window is owner*/
    /* has an initial dir been specified? */
    if (initdir != NULL && *initdir != NUL)
	fileStruct.lpstrInitialDir = initdir;

    /*
     * TODO: Allow selection of multiple files.  Needs another arg to this
     * function to ask for it, and need to use OFN_ALLOWMULTISELECT below.
     * Also, should we use OFN_FILEMUSTEXIST when opening?  Vim can edit on
     * files that don't exist yet, so I haven't put it in.  What about
     * OFN_PATHMUSTEXIST?
     * Don't use OFN_OVERWRITEPROMPT, Vim has its own ":confirm" dialog.
     */
    fileStruct.Flags = (OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
#ifdef FEAT_SHORTCUT
    if (curbuf->b_p_bin)
	fileStruct.Flags |= OFN_NODEREFERENCELINKS;
#endif
    if (saving)
    {
	if (!GetSaveFileName(&fileStruct))
	    return NULL;
    }
    else
    {
	if (!GetOpenFileName(&fileStruct))
	    return NULL;
    }

    vim_free(filter);

    /* Shorten the file name if possible */
    mch_dirname(IObuff, IOSIZE);
    p = shorten_fname(fileBuf, IObuff);
    if (p == NULL)
	p = fileBuf;
    return vim_strsave(p);
}
#endif /* FEAT_BROWSE */
