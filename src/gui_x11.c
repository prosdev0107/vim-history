/* vi:set ts=8 sts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *				GUI/Motif support by Robert Webb
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/cursorfont.h>

#include "vim.h"
#ifdef FEAT_SIGNS
# include <X11/xpm.h>
#endif

#ifdef FEAT_XFONTSET
# ifdef X_LOCALE
#  include <X11/Xlocale.h>
# else
#  include <locale.h>
# endif
#endif

#ifdef HAVE_X11_SUNKEYSYM_H
# include <X11/Sunkeysym.h>
#endif

#ifdef HAVE_X11_XMU_EDITRES_H
# include <X11/Xmu/Editres.h>
#endif

#define VIM_NAME	"vim"
#define VIM_CLASS	"Vim"

/* Default resource values */
#define DFLT_FONT		"7x13"
#ifdef FEAT_GUI_ATHENA
# define DFLT_MENU_BG_COLOR	"gray77"
# define DFLT_MENU_FG_COLOR	"black"
# define DFLT_SCROLL_BG_COLOR	"gray60"
# define DFLT_SCROLL_FG_COLOR	"gray77"
#else
/* use the default (CDE) colors */
# define DFLT_MENU_BG_COLOR	""
# define DFLT_MENU_FG_COLOR	""
# define DFLT_SCROLL_BG_COLOR	""
# define DFLT_SCROLL_FG_COLOR	""
#endif

Widget vimShell = (Widget)0;

static Atom   wm_atoms[2];	/* Window Manager Atoms */
#define DELETE_WINDOW_IDX 0	/* index in wm_atoms[] for WM_DELETE_WINDOW */
#define SAVE_YOURSELF_IDX 1	/* index in wm_atoms[] for WM_SAVE_YOURSELF */

#ifdef FEAT_XFONTSET
/*
 * We either draw with a fontset (when current_fontset != NULL) or with a
 * normal font (current_fontset == NULL, use gui.text_gc and gui.back_gc).
 */
static XFontSet current_fontset = NULL;

#define XDrawString(dpy, win, gc, x, y, str, n) \
	do \
	{ \
	    if (current_fontset != NULL) \
		XmbDrawString(dpy, win, current_fontset, gc, x, y, str, n); \
	    else \
		XDrawString(dpy, win, gc, x, y, str, n); \
	} while (0)

#define XDrawString16(dpy, win, gc, x, y, str, n) \
	do \
	{ \
	    if (current_fontset != NULL) \
		XwcDrawString(dpy, win, current_fontset, gc, x, y, (wchar_t *)str, n); \
	    else \
		XDrawString16(dpy, win, gc, x, y, str, n); \
	} while (0)

static int check_fontset_sanity __ARGS((XFontSet fs));
static int fontset_width __ARGS((XFontSet fs));
static int fontset_height __ARGS((XFontSet fs));
static int fontset_ascent __ARGS((XFontSet fs));
#endif

static guicolor_T	prev_fg_color = (guicolor_T)-1;
static guicolor_T	prev_bg_color = (guicolor_T)-1;

#if defined(FEAT_GUI_MOTIF) && defined(FEAT_MENU)
static XButtonPressedEvent last_mouse_event;
#endif

static int find_closest_color __ARGS((Colormap colormap, XColor *colorPtr));
static void gui_x11_timer_cb __ARGS((XtPointer timed_out, XtIntervalId *interval_id));
static void gui_x11_visibility_cb __ARGS((Widget w, XtPointer dud, XEvent *event, Boolean *dum));
static void gui_x11_expose_cb __ARGS((Widget w, XtPointer dud, XEvent *event, Boolean *dum));
static void gui_x11_resize_window_cb __ARGS((Widget w, XtPointer dud, XEvent *event, Boolean *dum));
static void gui_x11_focus_change_cb __ARGS((Widget w, XtPointer data, XEvent *event, Boolean *dum));
static void gui_x11_enter_cb __ARGS((Widget w, XtPointer data, XEvent *event, Boolean *dum));
static void gui_x11_leave_cb __ARGS((Widget w, XtPointer data, XEvent *event, Boolean *dum));
static void gui_x11_mouse_cb __ARGS((Widget w, XtPointer data, XEvent *event, Boolean *dum));
#ifdef FEAT_SNIFF
static void gui_x11_sniff_request_cb __ARGS((XtPointer closure, int *source, XtInputId *id));
#endif
static void gui_x11_check_copy_area __ARGS((void));
static void gui_x11_wm_protocol_handler __ARGS((Widget, XtPointer, XEvent *, Boolean *));
static void gui_x11_blink_cb __ARGS((XtPointer timed_out, XtIntervalId *interval_id));
static Cursor gui_x11_create_blank_mouse __ARGS((void));


static struct specialkey
{
    KeySym  key_sym;
    char_u  vim_code0;
    char_u  vim_code1;
} special_keys[] =
{
    {XK_Up,		'k', 'u'},
    {XK_Down,		'k', 'd'},
    {XK_Left,		'k', 'l'},
    {XK_Right,		'k', 'r'},

    {XK_F1,		'k', '1'},
    {XK_F2,		'k', '2'},
    {XK_F3,		'k', '3'},
    {XK_F4,		'k', '4'},
    {XK_F5,		'k', '5'},
    {XK_F6,		'k', '6'},
    {XK_F7,		'k', '7'},
    {XK_F8,		'k', '8'},
    {XK_F9,		'k', '9'},
    {XK_F10,		'k', ';'},

    {XK_F11,		'F', '1'},
    {XK_F12,		'F', '2'},
    {XK_F13,		'F', '3'},
    {XK_F14,		'F', '4'},
    {XK_F15,		'F', '5'},
    {XK_F16,		'F', '6'},
    {XK_F17,		'F', '7'},
    {XK_F18,		'F', '8'},
    {XK_F19,		'F', '9'},
    {XK_F20,		'F', 'A'},

    {XK_F21,		'F', 'B'},
    {XK_F22,		'F', 'C'},
    {XK_F23,		'F', 'D'},
    {XK_F24,		'F', 'E'},
    {XK_F25,		'F', 'F'},
    {XK_F26,		'F', 'G'},
    {XK_F27,		'F', 'H'},
    {XK_F28,		'F', 'I'},
    {XK_F29,		'F', 'J'},
    {XK_F30,		'F', 'K'},

    {XK_F31,		'F', 'L'},
    {XK_F32,		'F', 'M'},
    {XK_F33,		'F', 'N'},
    {XK_F34,		'F', 'O'},
    {XK_F35,		'F', 'P'},	/* keysymdef.h defines up to F35 */
#ifdef SunXK_F36
    {SunXK_F36,		'F', 'Q'},
    {SunXK_F37,		'F', 'R'},
#endif

    {XK_Help,		'%', '1'},
    {XK_Undo,		'&', '8'},
    {XK_BackSpace,	'k', 'b'},
    {XK_Insert,		'k', 'I'},
    {XK_Delete,		'k', 'D'},
    {XK_Home,		'k', 'h'},
    {XK_End,		'@', '7'},
    {XK_Prior,		'k', 'P'},
    {XK_Next,		'k', 'N'},
    {XK_Print,		'%', '9'},

    /* Keypad keys: */
#ifdef XK_KP_Left
    {XK_KP_Left,	'k', 'l'},
    {XK_KP_Right,	'k', 'r'},
    {XK_KP_Up,		'k', 'u'},
    {XK_KP_Down,	'k', 'd'},
    {XK_KP_Insert,	'k', 'I'},
    {XK_KP_Delete,	'k', 'D'},
    {XK_KP_Home,	'k', 'h'},
    {XK_KP_End,		'@', '7'},
    {XK_KP_Prior,	'k', 'P'},
    {XK_KP_Next,	'k', 'N'},

    {XK_KP_Add,		'K', '6'},
    {XK_KP_Subtract,	'K', '7'},
    {XK_KP_Divide,	'K', '8'},
    {XK_KP_Multiply,	'K', '9'},
    {XK_KP_Enter,	'K', 'A'},
    {XK_KP_Decimal,	'K', 'B'},

    {XK_KP_0,		'K', 'C'},
    {XK_KP_1,		'K', 'D'},
    {XK_KP_2,		'K', 'E'},
    {XK_KP_3,		'K', 'F'},
    {XK_KP_4,		'K', 'G'},
    {XK_KP_5,		'K', 'H'},
    {XK_KP_6,		'K', 'I'},
    {XK_KP_7,		'K', 'J'},
    {XK_KP_8,		'K', 'K'},
    {XK_KP_9,		'K', 'L'},
#endif

    /* End of list marker: */
    {(KeySym)0,	    0, 0}
};

#define XtNboldFont		"boldFont"
#define XtCBoldFont		"BoldFont"
#define XtNitalicFont		"italicFont"
#define XtCItalicFont		"ItalicFont"
#define XtNboldItalicFont	"boldItalicFont"
#define XtCBoldItalicFont	"BoldItalicFont"
#define XtNscrollbarWidth	"scrollbarWidth"
#define XtCScrollbarWidth	"ScrollbarWidth"
#define XtNmenuHeight		"menuHeight"
#define XtCMenuHeight		"MenuHeight"
#define XtNmenuFont		"menuFont"
#define XtCMenuFont		"MenuFont"
#define XtNmenuFontSet		"menuFontSet"
#define XtCMenuFontSet		"MenuFontSet"


/* Resources for setting the foreground and background colors of menus */
#define XtNmenuBackground	"menuBackground"
#define XtCMenuBackground	"MenuBackground"
#define XtNmenuForeground	"menuForeground"
#define XtCMenuForeground	"MenuForeground"

/* Resources for setting the foreground and background colors of scrollbars */
#define XtNscrollBackground	"scrollBackground"
#define XtCScrollBackground	"ScrollBackground"
#define XtNscrollForeground	"scrollForeground"
#define XtCScrollForeground	"ScrollForeground"

/*
 * X Resources:
 */
static XtResource vim_resources[] =
{
    {
	XtNforeground,
	XtCForeground,
	XtRPixel,
	sizeof(Pixel),
	XtOffsetOf(gui_T, def_norm_pixel),
	XtRString,
	XtDefaultForeground
    },
    {
	XtNbackground,
	XtCBackground,
	XtRPixel,
	sizeof(Pixel),
	XtOffsetOf(gui_T, def_back_pixel),
	XtRString,
	XtDefaultBackground
    },
    {
	XtNfont,
	XtCFont,
	XtRString,
	sizeof(String *),
	XtOffsetOf(gui_T, dflt_font),
	XtRImmediate,
	XtDefaultFont
    },
    {
	XtNboldFont,
	XtCBoldFont,
	XtRString,
	sizeof(String *),
	XtOffsetOf(gui_T, dflt_bold_fn),
	XtRImmediate,
	""
    },
    {
	XtNitalicFont,
	XtCItalicFont,
	XtRString,
	sizeof(String *),
	XtOffsetOf(gui_T, dflt_ital_fn),
	XtRImmediate,
	""
    },
    {
	XtNboldItalicFont,
	XtCBoldItalicFont,
	XtRString,
	sizeof(String *),
	XtOffsetOf(gui_T, dflt_boldital_fn),
	XtRImmediate,
	""
    },
    {
	XtNgeometry,
	XtCGeometry,
	XtRString,
	sizeof(String *),
	XtOffsetOf(gui_T, geom),
	XtRImmediate,
	""
    },
    {
	XtNreverseVideo,
	XtCReverseVideo,
	XtRBool,
	sizeof(Bool),
	XtOffsetOf(gui_T, rev_video),
	XtRImmediate,
	(XtPointer)False
    },
    {
	XtNborderWidth,
	XtCBorderWidth,
	XtRInt,
	sizeof(int),
	XtOffsetOf(gui_T, border_width),
	XtRImmediate,
	(XtPointer)2
    },
    {
	XtNscrollbarWidth,
	XtCScrollbarWidth,
	XtRInt,
	sizeof(int),
	XtOffsetOf(gui_T, scrollbar_width),
	XtRImmediate,
	(XtPointer)SB_DEFAULT_WIDTH
    },
#ifdef FEAT_MENU
    {
	XtNmenuHeight,
	XtCMenuHeight,
	XtRInt,
	sizeof(int),
	XtOffsetOf(gui_T, menu_height),
	XtRImmediate,
	(XtPointer)MENU_DEFAULT_HEIGHT	    /* Should figure out at run time */
    },
    {
	XtNmenuFont,
	XtCMenuFont,
	XtRFontStruct,
	sizeof(XFontStruct *),
	XtOffsetOf(gui_T, menu_font),
	XtRImmediate,
	(XtPointer)NOFONT
    },
#ifdef EXPERIMENTAL
    {
	XtNmenuFontSet,
	XtCMenuFontSet,
	XtRFontSet,
	sizeof(XFontSet *),
	XtOffsetOf(gui_T, menu_fontset),
	XtRImmediate,
	(XtPointer)NOFONTSET
    },
#endif
#endif
    {
	XtNmenuForeground,
	XtCMenuForeground,
	XtRString,
	sizeof(char *),
	XtOffsetOf(gui_T, menu_fg_color),
	XtRString,
	DFLT_MENU_FG_COLOR
    },
    {
	XtNmenuBackground,
	XtCMenuBackground,
	XtRString,
	sizeof(char *),
	XtOffsetOf(gui_T, menu_bg_color),
	XtRString,
	DFLT_MENU_BG_COLOR
    },
    {
	XtNscrollForeground,
	XtCScrollForeground,
	XtRString,
	sizeof(char *),
	XtOffsetOf(gui_T, scroll_fg_color),
	XtRString,
	DFLT_SCROLL_FG_COLOR
    },
    {
	XtNscrollBackground,
	XtCScrollBackground,
	XtRString,
	sizeof(char *),
	XtOffsetOf(gui_T, scroll_bg_color),
	XtRString,
	DFLT_SCROLL_BG_COLOR
    },
#ifdef FEAT_XIM
    {
	"preeditType",
	"PreeditType",
	XtRString,
	sizeof(char*),
	XtOffsetOf(gui_T, preedit_type),
	XtRString,
	(XtPointer)"OverTheSpot,OffTheSpot,Root"
    },
    {
	"inputMethod",
	"InputMethod",
	XtRString,
	sizeof(char*),
	XtOffsetOf(gui_T, input_method),
	XtRString,
	NULL
    },
    {
	"openIM",
	"OpenIM",
	XtRBoolean,
	sizeof(Boolean),
	XtOffsetOf(gui_T, open_im),
	XtRImmediate,
	(XtPointer)TRUE
    },
#endif /* FEAT_XIM */
#ifdef FEAT_BEVAL
    {
	XtNballoonEvalForeground,
	XtCForeground,
	XtRPixel,
	sizeof(Pixel),
	XtOffsetOf(gui_T, balloonEval_fg_pixel),
	XtRString,
	"#000000000000"
    },
    {
	XtNballoonEvalBackground,
	XtCBackground,
	XtRPixel,
	sizeof(Pixel),
	XtOffsetOf(gui_T, balloonEval_bg_pixel),
	XtRString,
	"#ffffffff9191"
    },
#ifdef FEAT_GUI_MOTIF
    {
	XmNballoonEvalFontList,
	XmCFontList,
	XmRFontList,
	sizeof(XmFontList),
	XtOffsetOf(gui_T, balloonEval_fontList),
	XtRString,
	"fixed"
    },
#endif
#endif /* FEAT_BEVAL */
};

/*
 * This table holds all the X GUI command line options allowed.  This includes
 * the standard ones so that we can skip them when vim is started without the
 * GUI (but the GUI might start up later).
 * When changing this, also update doc/vim_gui.txt and the usage message!!!
 */
static XrmOptionDescRec cmdline_options[] =
{
    /* We handle these options ourselves */
    {"-bg",		".background",	    XrmoptionSepArg,	NULL},
    {"-background",	".background",	    XrmoptionSepArg,	NULL},
    {"-fg",		".foreground",	    XrmoptionSepArg,	NULL},
    {"-foreground",	".foreground",	    XrmoptionSepArg,	NULL},
    {"-fn",		".font",	    XrmoptionSepArg,	NULL},
    {"-font",		".font",	    XrmoptionSepArg,	NULL},
    {"-boldfont",	".boldFont",	    XrmoptionSepArg,	NULL},
    {"-italicfont",	".italicFont",	    XrmoptionSepArg,	NULL},
    {"-geom",		".geometry",	    XrmoptionSepArg,	NULL},
    {"-geometry",	".geometry",	    XrmoptionSepArg,	NULL},
    {"-reverse",	"*reverseVideo",    XrmoptionNoArg,	"True"},
    {"-rv",		"*reverseVideo",    XrmoptionNoArg,	"True"},
    {"+reverse",	"*reverseVideo",    XrmoptionNoArg,	"False"},
    {"+rv",		"*reverseVideo",    XrmoptionNoArg,	"False"},
    {"-display",	".display",	    XrmoptionSepArg,	NULL},
    {"-iconic",		"*iconic",	    XrmoptionNoArg,	"True"},
    {"-name",		".name",	    XrmoptionSepArg,	NULL},
    {"-bw",		".borderWidth",	    XrmoptionSepArg,	NULL},
    {"-borderwidth",	".borderWidth",	    XrmoptionSepArg,	NULL},
    {"-sw",		".scrollbarWidth",  XrmoptionSepArg,	NULL},
    {"-scrollbarwidth",	".scrollbarWidth",  XrmoptionSepArg,	NULL},
    {"-mh",		".menuHeight",	    XrmoptionSepArg,	NULL},
    {"-menuheight",	".menuHeight",	    XrmoptionSepArg,	NULL},
    {"-mf",		".menuFont",	    XrmoptionSepArg,	NULL},
    {"-menufont",	".menuFont",	    XrmoptionSepArg,	NULL},
    {"-xrm",		NULL,		    XrmoptionResArg,	NULL}
};

static int gui_argc = 0;
static char **gui_argv = NULL;

/*
 * Call-back routines.
 */

/* ARGSUSED */
    static void
gui_x11_timer_cb(timed_out, interval_id)
    XtPointer	    timed_out;
    XtIntervalId    *interval_id;
{
    *((int *)timed_out) = TRUE;
}

/* ARGSUSED */
    static void
gui_x11_visibility_cb(w, dud, event, dum)
    Widget	w;
    XtPointer	dud;
    XEvent	*event;
    Boolean	*dum;
{
    if (event->type != VisibilityNotify)
	return;

    gui.visibility = event->xvisibility.state;

    /*
     * When we do an XCopyArea(), and the window is partially obscured, we want
     * to receive an event to tell us whether it worked or not.
     */
    XSetGraphicsExposures(gui.dpy, gui.text_gc,
	gui.visibility != VisibilityUnobscured);
}

/* ARGSUSED */
    static void
gui_x11_expose_cb(w, dud, event, dum)
    Widget	w;
    XtPointer	dud;
    XEvent	*event;
    Boolean	*dum;
{
    XExposeEvent	*gevent;
    int			new_x;

    if (event->type != Expose)
	return;

    out_flush();	    /* make sure all output has been processed */

    gevent = (XExposeEvent *)event;
    gui_redraw(gevent->x, gevent->y, gevent->width, gevent->height);

    new_x = FILL_X(0);

    /* Clear the border areas if needed */
    if (gevent->x < new_x)
	XClearArea(gui.dpy, gui.wid, 0, 0, new_x, 0, False);
    if (gevent->y < FILL_Y(0))
	XClearArea(gui.dpy, gui.wid, 0, 0, 0, FILL_Y(0), False);
    if (gevent->x > FILL_X(Columns))
	XClearArea(gui.dpy, gui.wid, FILL_X((int)Columns), 0, 0, 0, False);
    if (gevent->y > FILL_Y(Rows))
	XClearArea(gui.dpy, gui.wid, 0, FILL_Y((int)Rows), 0, 0, False);
}

/* ARGSUSED */
    static void
gui_x11_resize_window_cb(w, dud, event, dum)
    Widget	w;
    XtPointer	dud;
    XEvent	*event;
    Boolean	*dum;
{
    static int lastWidth, lastHeight;

    if (event->type != ConfigureNotify)
	return;

    if (event->xconfigure.width != lastWidth
	    || event->xconfigure.height != lastHeight)
    {
	lastWidth = event->xconfigure.width;
	lastHeight = event->xconfigure.height;
	gui_resize_shell(event->xconfigure.width, event->xconfigure.height
#ifdef FEAT_XIM
						- xim_get_status_area_height()
#endif
		     );
    }
#ifdef FEAT_SUN_WORKSHOP
    if (usingSunWorkShop)
    {
	XRectangle  rec;

	shellRectangle(w, &rec);
	workshop_frame_moved(rec.x, rec.y, rec.width, rec.height);
    }
#endif
#ifdef FEAT_XIM
    xim_set_preedit();
#endif
}

/* ARGSUSED */
    static void
gui_x11_focus_change_cb(w, data, event, dum)
    Widget	w;
    XtPointer	data;
    XEvent	*event;
    Boolean	*dum;
{
    gui_focus_change(event->type == FocusIn);
}

/* ARGSUSED */
    static void
gui_x11_enter_cb(w, data, event, dum)
    Widget	w;
    XtPointer	data;
    XEvent	*event;
    Boolean	*dum;
{
    gui_focus_change(TRUE);
}

/* ARGSUSED */
    static void
gui_x11_leave_cb(w, data, event, dum)
    Widget	w;
    XtPointer	data;
    XEvent	*event;
    Boolean	*dum;
{
    gui_focus_change(FALSE);
}

#if defined(X_HAVE_UTF8_STRING) && defined(FEAT_MBYTE)
# if X_HAVE_UTF8_STRING
#  define USE_UTF8LOOKUP
# endif
#endif

/* ARGSUSED */
    void
gui_x11_key_hit_cb(w, dud, event, dum)
    Widget	w;
    XtPointer	dud;
    XEvent	*event;
    Boolean	*dum;
{
    XKeyPressedEvent	*ev_press;
#ifdef FEAT_XIM
    char_u		string2[256];
    char_u		string_shortbuf[256];
    char_u		*string = string_shortbuf;
    Boolean		string_alloced = False;
    Status		status;
#else
    char_u		string[4], string2[3];
#endif
    KeySym		key_sym, key_sym2;
    int			len, len2;
    int			i;
    int			modifiers;
    int			key;

    ev_press = (XKeyPressedEvent *)event;

#ifdef FEAT_XIM
    if (xic)
    {
# ifdef USE_UTF8LOOKUP
	/* XFree86 4.0.2 or newer: Be able to get UTF-8 charatcers even when
	 * the locale isn't utf-8.  */
	if (enc_utf8)
	    len = Xutf8LookupString(xic, ev_press, (char *)string,
				  sizeof(string_shortbuf), &key_sym, &status);
	else
# endif
	    len = XmbLookupString(xic, ev_press, (char *)string,
				  sizeof(string_shortbuf), &key_sym, &status);
	if (status == XBufferOverflow)
	{
	    string = (char_u *)XtMalloc(len + 1);
	    string_alloced = True;
# ifdef USE_UTF8LOOKUP
	    /* XFree86 4.0.2 or newer: Be able to get UTF-8 characters even
	     * when the locale isn't utf-8.  */
	    if (enc_utf8)
		len = Xutf8LookupString(xic, ev_press, (char *)string,
						      len, &key_sym, &status);
	    else
# endif
		len = XmbLookupString(xic, ev_press, (char *)string,
						      len, &key_sym, &status);
	}
	if (status == XLookupNone || status == XLookupChars)
	    key_sym = XK_VoidSymbol;

# ifdef FEAT_MBYTE
	/* Do conversion from 'termencoding' to 'encoding'.  When using
	 * Xutf8LookupString() it has already been done. */
	if (len > 0 && input_conv.vc_type != CONV_NONE
#  ifdef USE_UTF8LOOKUP
		&& !enc_utf8
#  endif
		)
	{
	    int		maxlen = len * 4 + 40;  /* guessed */
	    char_u	*p = (char_u *)XtMalloc(maxlen);

	    mch_memmove(p, string, len);
	    if (string_alloced)
		XtFree((char *)string);
	    string = p;
	    string_alloced = True;
	    len = convert_input(p, len, maxlen);
	}
# endif

	/* Translate CSI to K_CSI, otherwise it could be recognized as the
	 * start of a special key. */
	for (i = 0; i < len; ++i)
	    if (string[i] == CSI)
	    {
		char_u	*p = (char_u *)XtMalloc(len + 3);

		mch_memmove(p, string, i);
		p[i + 1] = KS_EXTRA;
		p[i + 2] = (int)KE_CSI;
		mch_memmove(p + i + 3, string + i + 1, len - i);
		if (string_alloced)
		    XtFree((char *)string);
		string = p;
		string_alloced = True;
		i += 2;
	    }
    }
    else
#endif
	len = XLookupString(ev_press, (char *)string, sizeof(string),
		&key_sym, NULL);

#ifdef SunXK_F36
    /*
    * These keys have bogus lookup strings, and trapping them here is
    * easier than trying to XRebindKeysym() on them with every possible
    * combination of modifiers.
    */
    if (key_sym == SunXK_F36 || key_sym == SunXK_F37)
	len = 0;
#endif

#ifdef FEAT_HANGULIN
    if ((key_sym == XK_space) && (ev_press->state & ShiftMask))
    {
	hangul_input_state_toggle();
	goto theend;
    }
#endif

    if (key_sym == XK_space)
	string[0] = ' ';	/* Otherwise Ctrl-Space doesn't work */

    /*
     * Only on some machines ^_ requires Ctrl+Shift+minus.  For consistency,
     * allow just Ctrl+minus too.
     */
    if (key_sym == XK_minus && (ev_press->state & ControlMask))
	string[0] = Ctrl__;

#ifdef XK_ISO_Left_Tab
    /* why do we get XK_ISO_Left_Tab instead of XK_Tab for shift-tab? */
    if (key_sym == XK_ISO_Left_Tab)
    {
	key_sym = XK_Tab;
	string[0] = TAB;
	len = 1;
    }
#endif

    /* Check for Alt/Meta key (Mod1Mask), but not for a BS, DEL or character
     * that already has the 8th bit set. */
    if (len == 1
	    && (ev_press->state & Mod1Mask)
	    && !(key_sym == XK_BackSpace || key_sym == XK_Delete)
	    && (string[0] & 0x80) == 0)
    {
#if defined(FEAT_MENU) && defined(FEAT_GUI_MOTIF)
	/* Ignore ALT keys when they are used for the menu only */
	if (gui.menu_is_active
		&& (p_wak[0] == 'y'
		    || (p_wak[0] == 'm' && gui_is_menu_shortcut(string[0]))))
	    goto theend;
#endif
	/*
	 * Before we set the 8th bit, check to make sure the user doesn't
	 * already have a mapping defined for this sequence. We determine this
	 * by checking to see if the input would be the same without the
	 * Alt/Meta key.
	 * Don't do this for <S-M-Tab>, that should become K_S_TAB with ALT.
	 */
	ev_press->state &= ~Mod1Mask;
	len2 = XLookupString(ev_press, (char *)string2, sizeof(string2),
							     &key_sym2, NULL);
	if (key_sym2 == XK_space)
	    string2[0] = ' ';	    /* Otherwise Meta-Ctrl-Space doesn't work */
	if (	   len2 == 1
		&& string[0] == string2[0]
		&& !(key_sym == XK_Tab && (ev_press->state & ShiftMask)))
	{
	    string[0] |= 0x80;
#ifdef FEAT_MBYTE
	    if (enc_utf8) /* convert to utf-8 */
	    {
		string[1] = string[0] & 0xbf;
		string[0] = ((unsigned)string[0] >> 6) + 0xc0;
		if (string[1] == CSI)
		{
		    string[2] = KS_EXTRA;
		    string[3] = (int)KE_CSI;
		    len = 4;
		}
		else
		    len = 2;
	    }
#endif
	}
	else
	    ev_press->state |= Mod1Mask;
    }

    if (len == 1 && string[0] == CSI)
    {
	string[1] = KS_EXTRA;
	string[2] = (int)KE_CSI;
	len = -3;
    }

    /* Check for special keys.  Also do this when len == 1 (key has an ASCII
     * value) to detect backspace, delete and keypad keys. */
    if (len == 0 || len == 1)
    {
	for (i = 0; special_keys[i].key_sym != (KeySym)0; i++)
	{
	    if (special_keys[i].key_sym == key_sym)
	    {
		string[0] = CSI;
		string[1] = special_keys[i].vim_code0;
		string[2] = special_keys[i].vim_code1;
		len = -3;
		break;
	    }
	}
    }

    /* Unrecognised key is ignored. */
    if (len == 0)
	goto theend;

    /* Special keys (and a few others) may have modifiers */
    if (len == -3 || key_sym == XK_space || key_sym == XK_Tab
	|| key_sym == XK_Return || key_sym == XK_Linefeed
	|| key_sym == XK_Escape)
    {
	modifiers = 0;
	if (ev_press->state & ShiftMask)
	    modifiers |= MOD_MASK_SHIFT;
	if (ev_press->state & ControlMask)
	    modifiers |= MOD_MASK_CTRL;
	if (ev_press->state & Mod1Mask)
	    modifiers |= MOD_MASK_ALT;

	/*
	 * For some keys a shift modifier is translated into another key
	 * code.
	 */
	if (len == -3)
	    key = TO_SPECIAL(string[1], string[2]);
	else
	    key = string[0];
	key = simplify_key(key, &modifiers);
	if (key == CSI)
	    key = K_CSI;
	if (IS_SPECIAL(key))
	{
	    string[0] = CSI;
	    string[1] = K_SECOND(key);
	    string[2] = K_THIRD(key);
	    len = 3;
	}
	else
	{
	    string[0] = key;
	    len = 1;
	}

	if (modifiers != 0)
	{
	    string2[0] = CSI;
	    string2[1] = KS_MODIFIER;
	    string2[2] = modifiers;
	    add_to_input_buf(string2, 3);
	}
    }

    if (len == 1 && ((string[0] == Ctrl_C && ctrl_c_interrupts)
#ifdef UNIX
	    || (intr_char != 0 && string[0] == intr_char)
#endif
	    ))
    {
	trash_input_buf();
	got_int = TRUE;
    }

    add_to_input_buf(string, len);

    /*
     * blank out the pointer if necessary
     */
    if (p_mh)
	gui_mch_mousehide(TRUE);

theend:
#ifdef VMS
    {}	    /* compiler needs some statement */
#endif
#ifdef FEAT_XIM
    if (string_alloced)
	XtFree((char *)string);
#endif
}

/* ARGSUSED */
    static void
gui_x11_mouse_cb(w, dud, event, dum)
    Widget	w;
    XtPointer	dud;
    XEvent	*event;
    Boolean	*dum;
{
    static XtIntervalId timer = (XtIntervalId)0;
    static int	timed_out = TRUE;

    int		button;
    int		repeated_click = FALSE;
    int		x, y;
    int_u	x_modifiers;
    int_u	vim_modifiers;

    if (event->type == MotionNotify)
    {
	/* Get the latest position, avoids lagging behind on a drag. */
	x = event->xmotion.x;
	y = event->xmotion.y;
	x_modifiers = event->xmotion.state;
	button = (x_modifiers & (Button1Mask | Button2Mask | Button3Mask))
		? MOUSE_DRAG : ' ';

	/*
	 * if our pointer is currently hidden, then we should show it.
	 */
	gui_mch_mousehide(FALSE);

	if (button != MOUSE_DRAG)	/* just moving the rodent */
	{
#ifdef FEAT_MENU
	    if (dud)			/* moved in vimForm */
		y -= gui.menu_height;
#endif
	    gui_mouse_moved(x, y);
	    return;
	}
    }
    else
    {
	x = event->xbutton.x;
	y = event->xbutton.y;
	if (event->type == ButtonPress)
	{
	    /* Handle multiple clicks */
	    if (!timed_out)
	    {
		XtRemoveTimeOut(timer);
		repeated_click = TRUE;
	    }
	    timed_out = FALSE;
	    timer = XtAppAddTimeOut(app_context, (long_u)p_mouset,
			gui_x11_timer_cb, &timed_out);
	    switch (event->xbutton.button)
	    {
		case Button1:	button = MOUSE_LEFT;	break;
		case Button2:	button = MOUSE_MIDDLE;	break;
		case Button3:	button = MOUSE_RIGHT;	break;
		case Button4:	button = MOUSE_4;	break;
		case Button5:	button = MOUSE_5;	break;
		default:
		    return;	/* Unknown button */
	    }
	}
	else if (event->type == ButtonRelease)
	    button = MOUSE_RELEASE;
	else
	    return;	/* Unknown mouse event type */

	x_modifiers = event->xbutton.state;
#if defined(FEAT_GUI_MOTIF) && defined(FEAT_MENU)
	last_mouse_event = event->xbutton;
#endif
    }

    vim_modifiers = 0x0;
    if (x_modifiers & ShiftMask)
	vim_modifiers |= MOUSE_SHIFT;
    if (x_modifiers & ControlMask)
	vim_modifiers |= MOUSE_CTRL;
    if (x_modifiers & Mod1Mask)	    /* Alt or Meta key */
	vim_modifiers |= MOUSE_ALT;

    gui_send_mouse_event(button, x, y, repeated_click, vim_modifiers);
}

#ifdef FEAT_SNIFF
/* ARGSUSED */
    static void
gui_x11_sniff_request_cb(closure, source, id)
    XtPointer	closure;
    int		*source;
    XtInputId	*id;
{
    static char_u bytes[3] = {CSI, (int)KS_EXTRA, (int)KE_SNIFF};

    add_to_input_buf(bytes, 3);
}
#endif

/*
 * End of call-back routines
 */

/*
 * Parse the GUI related command-line arguments.  Any arguments used are
 * deleted from argv, and *argc is decremented accordingly.  This is called
 * when vim is started, whether or not the GUI has been started.
 */
    void
gui_mch_prepare(argc, argv)
    int	    *argc;
    char    **argv;
{
    int	    arg;
    int	    i;

    /*
     * Move all the entries in argv which are relevant to X into gui_argv.
     */
    gui_argc = 0;
    gui_argv = (char **)lalloc((long_u)(*argc * sizeof(char *)), FALSE);
    if (gui_argv == NULL)
	return;
    gui_argv[gui_argc++] = argv[0];
    arg = 1;
    while (arg < *argc)
    {
	/* Look for argv[arg] in cmdline_options[] table */
	for (i = 0; i < XtNumber(cmdline_options); i++)
	    if (strcmp(argv[arg], cmdline_options[i].option) == 0)
		break;

	if (i < XtNumber(cmdline_options))
	{
	    /* Remember finding "-rv" or "-reverse" */
	    if (strcmp("-rv", argv[arg]) == 0
		    || strcmp("-reverse", argv[arg]) == 0)
		found_reverse_arg = TRUE;
	    else if ((strcmp("-fn", argv[arg]) == 0
			|| strcmp("-font", argv[arg]) == 0)
		    && arg + 1 < *argc)
		font_argument = argv[arg + 1];

	    /* Found match in table, so move it into gui_argv */
	    gui_argv[gui_argc++] = argv[arg];
	    if (--*argc > arg)
	    {
		mch_memmove(&argv[arg], &argv[arg + 1], (*argc - arg)
						    * sizeof(char *));
		if (cmdline_options[i].argKind != XrmoptionNoArg)
		{
		    /* Move the options argument as well */
		    gui_argv[gui_argc++] = argv[arg];
		    if (--*argc > arg)
			mch_memmove(&argv[arg], &argv[arg + 1], (*argc - arg)
							    * sizeof(char *));
		}
	    }
	}
	else
#ifdef FEAT_SUN_WORKSHOP
	    if (strcmp("-ws", argv[arg]) == 0)
	{
	    usingSunWorkShop++;
	    gui.dofork = FALSE;	/* don't fork() when starting GUI */
	    mch_memmove(&argv[arg], &argv[arg + 1],
					    (--*argc - arg) * sizeof(char *));
# ifdef WSDEBUG
	    wsdebug_wait(WT_ENV | WT_WAIT | WT_STOP, "SPRO_GVIM_WAIT", 20);
	    wsdebug_log_init("SPRO_GVIM_DEBUG", "SPRO_GVIM_DLEVEL");
# endif
	}
	else
#endif
	    arg++;
    }
}

#ifndef XtSpecificationRelease
# define CARDINAL (Cardinal *)
#else
# if XtSpecificationRelease == 4
# define CARDINAL (Cardinal *)
# else
# define CARDINAL (int *)
# endif
#endif

/*
 * Check if the GUI can be started.  Called before gvimrc is sourced.
 * Return OK or FAIL.
 */
    int
gui_mch_init_check()
{
#ifdef FEAT_XIM
    XtSetLanguageProc(NULL, NULL, NULL);
#endif
    open_app_context();
    if (app_context != NULL)
	gui.dpy = XtOpenDisplay(app_context, 0, VIM_NAME, VIM_CLASS,
	    cmdline_options, XtNumber(cmdline_options),
	    CARDINAL &gui_argc, gui_argv);

    if (app_context == NULL || gui.dpy == NULL)
    {
	gui.dying = TRUE;
	EMSG(_("cannot open display"));
	return FAIL;
    }
    return OK;
}

/*
 * Initialise the X GUI.  Create all the windows, set up all the call-backs etc.
 * Returns OK for success, FAIL when the GUI can't be started.
 */
    int
gui_mch_init()
{
    long_u	gc_mask;
    XGCValues	gc_vals;
    int		x, y, mask;
    unsigned	w, h;

#if 0
    /* Uncomment this to enable synchronous mode for debugging */
    XSynchronize(gui.dpy, True);
#endif

#if 0 /* not needed? */
    /*
     * So converters work.
     */
    XtInitializeWidgetClass(applicationShellWidgetClass);
    XtInitializeWidgetClass(topLevelShellWidgetClass);
#endif

    vimShell = XtVaAppCreateShell(VIM_NAME, VIM_CLASS,
	    applicationShellWidgetClass, gui.dpy, NULL);

    /*
     * Get the application resources
     */
    XtVaGetApplicationResources(vimShell, (XtPointer)&gui,
	vim_resources, XtNumber(vim_resources), NULL);

    gui.scrollbar_height = gui.scrollbar_width;

    /*
     * Get the colors ourselves.  Using the automatic conversion doesn't
     * handle looking for approximate colors.
     */
    gui.menu_fg_pixel = gui_mch_get_color((char_u *)gui.menu_fg_color);
    gui.menu_bg_pixel = gui_mch_get_color((char_u *)gui.menu_bg_color);
    gui.scroll_fg_pixel = gui_mch_get_color((char_u *)gui.scroll_fg_color);
    gui.scroll_bg_pixel = gui_mch_get_color((char_u *)gui.scroll_bg_color);

#ifdef FEAT_MENU
    /* If the menu height was set, don't change it at runtime */
    if (gui.menu_height != MENU_DEFAULT_HEIGHT)
	gui.menu_height_fixed = TRUE;
#endif

    /* Set default foreground and background colours */
    gui.norm_pixel = gui.def_norm_pixel;
    gui.back_pixel = gui.def_back_pixel;

    /* Check if reverse video needs to be applied (on Sun it's done by X) */
    if (gui.rev_video && gui_mch_get_lightness(gui.back_pixel)
				      > gui_mch_get_lightness(gui.norm_pixel))
    {
	gui.norm_pixel = gui.def_back_pixel;
	gui.back_pixel = gui.def_norm_pixel;
	gui.def_norm_pixel = gui.norm_pixel;
	gui.def_back_pixel = gui.back_pixel;
    }

    /* Get the colors from the "Normal" and "Menu" group (set in syntax.c or
     * in a vimrc file) */
    set_normal_colors();

    /*
     * Check that none of the colors are the same as the background color
     */
    gui_check_colors();

    /*
     * Set up the GCs.	The font attributes will be set in gui_init_font().
     */
    gc_mask = GCForeground | GCBackground;
    gc_vals.foreground = gui.norm_pixel;
    gc_vals.background = gui.back_pixel;
    gui.text_gc = XCreateGC(gui.dpy, DefaultRootWindow(gui.dpy), gc_mask,
	    &gc_vals);

    gc_vals.foreground = gui.back_pixel;
    gc_vals.background = gui.norm_pixel;
    gui.back_gc = XCreateGC(gui.dpy, DefaultRootWindow(gui.dpy), gc_mask,
	    &gc_vals);

    gc_mask |= GCFunction;
    gc_vals.foreground = gui.norm_pixel ^ gui.back_pixel;
    gc_vals.background = gui.norm_pixel ^ gui.back_pixel;
    gc_vals.function   = GXxor;
    gui.invert_gc = XCreateGC(gui.dpy, DefaultRootWindow(gui.dpy), gc_mask,
	    &gc_vals);

    gui.visibility = VisibilityUnobscured;
    x11_setup_atoms(gui.dpy);

    /* Now adapt the supplied(?) geometry-settings */
    /* Added by Kjetil Jacobsen <kjetilja@stud.cs.uit.no> */
    if (gui.geom != NULL && *gui.geom != NUL)
    {
	mask = XParseGeometry((char *)gui.geom, &x, &y, &w, &h);
	if (mask & WidthValue)
	    Columns = w;
	if (mask & HeightValue)
	    Rows = h;
	/*
	 * Set the (x,y) position of the main window only if specified in the
	 * users geometry, so we get good defaults when they don't. This needs
	 * to be done before the shell is popped up.
	 */
	if (mask & (XValue|YValue))
	    XtVaSetValues(vimShell, XtNgeometry, gui.geom, NULL);
    }

    gui_x11_create_widgets();

   /*
    * Add an icon to Vim (Marcel Douben: 11 May 1998).
    */
    if (vim_strchr(p_go, GO_ICON) != NULL)
    {
#include "vim_icon.xbm"
#include "vim_mask.xbm"

	Arg	arg[2];

	XtSetArg(arg[0], XtNiconPixmap,
		XCreateBitmapFromData(gui.dpy,
		    DefaultRootWindow(gui.dpy),
		    (char *)vim_icon_bits,
		    vim_icon_width,
		    vim_icon_height));
	XtSetArg(arg[1], XtNiconMask,
		XCreateBitmapFromData(gui.dpy,
		    DefaultRootWindow(gui.dpy),
		    (char *)vim_mask_icon_bits,
		    vim_mask_icon_width,
		    vim_mask_icon_height));
	XtSetValues(vimShell, arg, (Cardinal)2);
    }

    if (gui.color_approx)
	EMSG(_("Vim: Cannot allocate colormap entry, some colors may be incorrect"));

#ifdef FEAT_SUN_WORKSHOP
    if (usingSunWorkShop)
	workshop_connect(app_context);
#endif

    return OK;
}

/*
 * Called when starting the GUI fails after calling gui_mch_init().
 */
    void
gui_mch_uninit()
{
    gui_x11_destroy_widgets();
#ifndef LESSTIF_VERSION
    XtCloseDisplay(gui.dpy);
#endif
    gui.dpy = NULL;
    vimShell = (Widget)0;
}

/*
 * Called when the foreground or background color has been changed.
 */
    void
gui_mch_new_colors()
{
    long_u	gc_mask;
    XGCValues	gc_vals;

    gc_mask = GCForeground | GCBackground;
    gc_vals.foreground = gui.norm_pixel;
    gc_vals.background = gui.back_pixel;
    if (gui.text_gc != NULL)
	XChangeGC(gui.dpy, gui.text_gc, gc_mask, &gc_vals);

    gc_vals.foreground = gui.back_pixel;
    gc_vals.background = gui.norm_pixel;
    if (gui.back_gc != NULL)
	XChangeGC(gui.dpy, gui.back_gc, gc_mask, &gc_vals);

    gc_mask |= GCFunction;
    gc_vals.foreground = gui.norm_pixel ^ gui.back_pixel;
    gc_vals.background = gui.norm_pixel ^ gui.back_pixel;
    gc_vals.function   = GXxor;
    if (gui.invert_gc != NULL)
	XChangeGC(gui.dpy, gui.invert_gc, gc_mask, &gc_vals);

    gui_x11_set_back_color();
}

/*
 * Open the GUI window which was created by a call to gui_mch_init().
 */
    int
gui_mch_open()
{
    /* Actually open the window */
    XtPopup(vimShell, XtGrabNone);

    gui.wid = gui_x11_get_wid();
    gui.blank_pointer = gui_x11_create_blank_mouse();

    /*
     * Add a callback for the Close item on the window managers menu, and the
     * save-yourself event.
     */
    wm_atoms[SAVE_YOURSELF_IDX] =
			      XInternAtom(gui.dpy, "WM_SAVE_YOURSELF", False);
    wm_atoms[DELETE_WINDOW_IDX] =
			      XInternAtom(gui.dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(gui.dpy, XtWindow(vimShell), wm_atoms, 2);
    XtAddEventHandler(vimShell, NoEventMask, True, gui_x11_wm_protocol_handler,
							     NULL);
#ifdef HAVE_X11_XMU_EDITRES_H
    /*
     * Enable editres protocol (see "man editres").
     * Usually will need to add -lXmu to the linker line as well.
     */
    XtAddEventHandler(vimShell, 0, True, _XEditResCheckMessages,
	    (XtPointer)NULL);
#endif

#if defined(FEAT_MENU) && defined(FEAT_GUI_ATHENA)
    /* The Athena GUI needs this again after opening the window */
    gui_position_menu();
# ifdef FEAT_TOOLBAR
    gui_mch_set_toolbar_pos(0, gui.menu_height, gui.menu_width,
			    gui.toolbar_height);
# endif
#endif

    /* Get the colors for the highlight groups (gui_check_colors() might have
     * changed them) */
    highlight_gui_started();		/* re-init colors and fonts */

#ifdef FEAT_HANGULIN
    hangul_keyboard_set();
#endif
#ifdef FEAT_XIM
    xim_init();
#endif
#ifdef FEAT_SUN_WORKSHOP
    workshop_postinit();
#endif

    return OK;
}

/*ARGSUSED*/
    void
gui_mch_exit(rc)
    int		rc;
{
#if 0
    /* Lesstif gives an error message here, and so does Solaris.  The man page
     * says that this isn't needed when exiting, so just skip it. */
    XtCloseDisplay(gui.dpy);
#endif
}

/*
 * Get the position of the top left corner of the window.
 */
    int
gui_mch_get_winpos(x, y)
    int		*x, *y;
{
    Dimension	xpos, ypos;

    XtVaGetValues(vimShell,
	XtNx,	&xpos,
	XtNy,	&ypos,
	NULL);
    *x = xpos;
    *y = ypos;
    return OK;
}

/*
 * Set the position of the top left corner of the window to the given
 * coordinates.
 */
    void
gui_mch_set_winpos(x, y)
    int		x, y;
{
    XtVaSetValues(vimShell,
	XtNx,	x,
	XtNy,	y,
	NULL);
}

    void
gui_mch_set_shellsize(width, height, min_width, min_height,
		    base_width, base_height)
    int		width;
    int		height;
    int		min_width;
    int		min_height;
    int		base_width;
    int		base_height;
{
    XtVaSetValues(vimShell,
	XtNwidthInc,	gui.char_width,
	XtNheightInc,	gui.char_height,
#if defined(XtSpecificationRelease) && XtSpecificationRelease >= 4
	XtNbaseWidth,	base_width,
	XtNbaseHeight,	base_height,
#endif
	XtNminWidth,	min_width,
	XtNminHeight,	min_height,
	XtNwidth,	width,
#ifdef FEAT_XIM
	XtNheight,	height + xim_get_status_area_height(),
#else
	XtNheight,	height,
#endif
	NULL);
}

/*
 * Allow 10 pixels for horizontal borders, 30 for vertical borders.
 * Is there no way in X to find out how wide the borders really are?
 */
    void
gui_mch_get_screen_dimensions(screen_w, screen_h)
    int	    *screen_w;
    int	    *screen_h;
{
    *screen_w = DisplayWidth(gui.dpy, DefaultScreen(gui.dpy)) - 10;
    *screen_h = DisplayHeight(gui.dpy, DefaultScreen(gui.dpy)) - p_ghr;
}

/*
 * Initialise vim to use the font "font_name".  If it's NULL, pick a default
 * font.
 * If "fontset" is TRUE, load the "font_name" as a fontset.
 * Return FAIL if the font could not be loaded, OK otherwise.
 */
/*ARGSUSED*/
    int
gui_mch_init_font(font_name, do_fontset)
    char_u	*font_name;
    int		do_fontset;
{
    XFontStruct	*font = NULL;

#ifdef FEAT_XFONTSET
    XFontSet	fontset = NULL;

    if (do_fontset)
    {
	/* If 'guifontset' is set, VIM treats all font specifications as if
	 * they were fontsets, and 'guifontset' becomes the default. */
	if (font_name != NULL)
	{
	    fontset = (XFontSet)gui_mch_get_fontset(font_name, FALSE);
	    if (fontset == NULL)
		return FAIL;
	}
    }
    else
#endif
    {
	if (font_name == NULL)
	{
	    /*
	     * If none of the fonts in 'font' could be loaded, try the one set
	     * in the X resource, and finally just try using DFLT_FONT, which
	     * will hopefully always be there.
	     */
	    font_name = gui.dflt_font;
	    font = (XFontStruct *)gui_mch_get_font(font_name, FALSE);
	    if (font == NULL)
		font_name = (char_u *)DFLT_FONT;
	}
	if (font == NULL)
	    font = (XFontStruct *)gui_mch_get_font(font_name, FALSE);
	if (font == NULL)
	    return FAIL;
    }

    gui_mch_free_font(gui.norm_font);
#ifdef FEAT_XFONTSET
    gui_mch_free_fontset(gui.fontset);

    if (fontset != NULL)
    {
	gui.norm_font = NOFONT;
	gui.fontset = (GuiFontset)fontset;
	gui.char_width = fontset_width(fontset);
	gui.char_height = fontset_height(fontset) + p_linespace;
	gui.char_ascent = fontset_ascent(fontset) + p_linespace / 2;
    }
    else
#endif
    {
	gui.norm_font = (GuiFont)font;
#ifdef FEAT_XFONTSET
	gui.fontset = NOFONTSET;
#endif
	gui.char_width = font->max_bounds.width;
	gui.char_height = font->ascent + font->descent + p_linespace;
	gui.char_ascent = font->ascent + p_linespace / 2;
    }

    hl_set_font_name(font_name);

    /*
     * Try to load other fonts for bold, italic, and bold-italic.
     * We should also try to work out what font to use for these when they are
     * not specified by X resources, but we don't yet.
     */
    if (font_name == gui.dflt_font)
    {
	if (gui.bold_font == NOFONT
		&& gui.dflt_bold_fn != NULL
		&& *gui.dflt_bold_fn != NUL)
	    gui.bold_font = gui_mch_get_font(gui.dflt_bold_fn, FALSE);
	if (gui.ital_font == NOFONT
		&& gui.dflt_ital_fn != NULL
		&& *gui.dflt_ital_fn != NUL)
	    gui.ital_font = gui_mch_get_font(gui.dflt_ital_fn, FALSE);
	if (gui.boldital_font == NOFONT
		&& gui.dflt_boldital_fn != NULL
		&& *gui.dflt_boldital_fn != NUL)
	    gui.boldital_font = gui_mch_get_font(gui.dflt_boldital_fn, FALSE);
    }
    else
    {
	/* When not using the font specified by the resources, also don't use
	 * the bold/italic fonts, otherwise setting 'guifont' will look very
	 * strange. */
	if (gui.bold_font != NOFONT)
	{
	    XFreeFont(gui.dpy, (XFontStruct *)gui.bold_font);
	    gui.bold_font = NOFONT;
	}
	if (gui.ital_font != NOFONT)
	{
	    XFreeFont(gui.dpy, (XFontStruct *)gui.ital_font);
	    gui.ital_font = NOFONT;
	}
	if (gui.boldital_font != NOFONT)
	{
	    XFreeFont(gui.dpy, (XFontStruct *)gui.boldital_font);
	    gui.boldital_font = NOFONT;
	}
    }

    return OK;
}

/*
 * Get a font structure for highlighting.
 */
    GuiFont
gui_mch_get_font(name, giveErrorIfMissing)
    char_u	*name;
    int		giveErrorIfMissing;
{
    XFontStruct	*font;

    if (!gui.in_use || name == NULL)    /* can't do this when GUI not running */
	return NOFONT;

    font = XLoadQueryFont(gui.dpy, (char *)name);

    if (font == NULL)
    {
	if (giveErrorIfMissing)
	    EMSG2(_("Unknown font: %s"), name);
	return NOFONT;
    }

#ifdef DEBUG
    printf("Font Information for '%s':\n", name);
    printf("  w = %d, h = %d, ascent = %d, descent = %d\n",
	   font->max_bounds.width, font->ascent + font->descent,
	   font->ascent, font->descent);
    printf("  max ascent = %d, max descent = %d, max h = %d\n",
	   font->max_bounds.ascent, font->max_bounds.descent,
	   font->max_bounds.ascent + font->max_bounds.descent);
    printf("  min lbearing = %d, min rbearing = %d\n",
	   font->min_bounds.lbearing, font->min_bounds.rbearing);
    printf("  max lbearing = %d, max rbearing = %d\n",
	   font->max_bounds.lbearing, font->max_bounds.rbearing);
    printf("  leftink = %d, rightink = %d\n",
	   (font->min_bounds.lbearing < 0),
	   (font->max_bounds.rbearing > font->max_bounds.width));
    printf("\n");
#endif

    if (font->max_bounds.width != font->min_bounds.width)
    {
	EMSG2(_("Font \"%s\" is not fixed-width"), name);
	XFreeFont(gui.dpy, font);
	return NOFONT;
    }
    return (GuiFont)font;
}

    int
gui_mch_adjust_charsize()
{
#ifdef FEAT_XFONTSET
    if (gui.fontset != NOFONTSET)
    {
	gui.char_height = fontset_height((XFontSet)gui.fontset) + p_linespace;
	gui.char_ascent = fontset_ascent((XFontSet)gui.fontset)
							    + p_linespace / 2;
    }
    else
#endif
    {
	XFontStruct *font = (XFontStruct *)gui.norm_font;

	gui.char_height = font->ascent + font->descent + p_linespace;
	gui.char_ascent = font->ascent + p_linespace / 2;
    }
    return OK;
}

/*
 * Set the current text font.
 */
    void
gui_mch_set_font(font)
    GuiFont	font;
{
    static Font	prev_font = (Font)-1;
    Font	fid = ((XFontStruct *)font)->fid;

    if (fid != prev_font)
    {
	XSetFont(gui.dpy, gui.text_gc, fid);
	XSetFont(gui.dpy, gui.back_gc, fid);
	prev_font = fid;
	gui.char_ascent = ((XFontStruct *)font)->ascent + p_linespace / 2;
    }
#ifdef FEAT_XFONTSET
    current_fontset = (XFontSet)NULL;
#endif
}

#if defined(FEAT_XFONTSET) || defined(PROTO)
/*
 * Set the current text fontset.
 * Adjust the ascent, in case it's different.
 */
    void
gui_mch_set_fontset(fontset)
    GuiFontset	fontset;
{
    current_fontset = (XFontSet)fontset;
    gui.char_ascent = fontset_ascent(current_fontset) + p_linespace / 2;
}
#endif

#if 0 /* not used */
/*
 * Return TRUE if the two fonts given are equivalent.
 */
    int
gui_mch_same_font(f1, f2)
    GuiFont	f1;
    GuiFont	f2;
{
#ifdef FEAT_XFONTSET
    if (gui.fontset != NULL)
	return f1 == f2;
    else
#endif
    return ((XFontStruct *)f1)->fid == ((XFontStruct *)f2)->fid;
}
#endif

/*
 * If a font is not going to be used, free its structure.
 */
    void
gui_mch_free_font(font)
    GuiFont	font;
{
    if (font != NOFONT)
	XFreeFont(gui.dpy, (XFontStruct *)font);
}

#if defined(FEAT_XFONTSET) || defined(PROTO)
/*
 * If a fontset is not going to be used, free its structure.
 */
    void
gui_mch_free_fontset(fontset)
    GuiFontset	fontset;
{
    if (fontset != NOFONTSET)
	XFreeFontSet(gui.dpy, (XFontSet)fontset);
}

/*
 * Load the fontset "name".
 * Return a reference to the fontset, or NOFONTSET when failing.
 */
    GuiFontset
gui_mch_get_fontset(name, giveErrorIfMissing)
    char_u	*name;
    int		giveErrorIfMissing;
{
    XFontSet	fontset;
    char	**missing, *def_str;
    int		num_missing;

    if (!gui.in_use || name == NULL)
	return NOFONTSET;

    fontset = XCreateFontSet(gui.dpy, (char *)name, &missing, &num_missing,
			     &def_str);
    if (num_missing > 0)
    {
	int i;

	if (giveErrorIfMissing)
	{
	    EMSG2(_("Error: During loading fontset %s"), name);
	    EMSG(_("Font(s) for the following character sets are missing:"));
	    for (i = 0; i < num_missing; i++)
		EMSG2("%s", missing[i]);
	}
	XFreeStringList(missing);
    }

    if (fontset == NULL)
    {
	if (giveErrorIfMissing)
	    EMSG2(_("Unknown fontset: %s"), name);
	return NOFONTSET;
    }

    if (check_fontset_sanity(fontset) == FAIL)
    {
	XFreeFontSet(gui.dpy, fontset);
	return NOFONT;
    }
    return (GuiFontset)fontset;
}

    static int
check_fontset_sanity(fs)
    XFontSet fs;
{
    XFontStruct	**xfs;
    char	**font_name;
    int		fn;
    char	*base_name;
    int		i;
    int		min_width;
    int		min_font_idx = 0;

    base_name = XBaseFontNameListOfFontSet(fs);
    fn = XFontsOfFontSet(fs, &xfs, &font_name);
#if 0
    if (fn < 2)
    {
	char *locale = XLocaleOfFontSet(fs);

	if (!locale || locale[0] == 'C')
	{
	    EMSG(_("locale is not set correctly"));
	    MSG(_("Set LANG environment variable to your locale"));
	    MSG(_("For korean:"));
	    MSG(_("   csh: setenv LANG ko"));
	    MSG(_("   sh : export LANG=ko"));
	}
	EMSG2(_("fontset name: %s"), base_name);
	EMSG(_("Your language Font missing"));
	EMSG2(_("loaded fontname: %s"), font_name[0]);
	return FAIL;
    }
#endif
    for (i = 0; i < fn; i++)
    {
	if (xfs[i]->max_bounds.width != xfs[i]->min_bounds.width)
	{
	    EMSG2(_("Fontset name: %s"), base_name);
	    EMSG2(_("Font '%s' is not fixed-width"), font_name[i]);
	    return FAIL;
	}
    }
    /* scan base font width */
    min_width = 32767;
    for (i = 0; i < fn; i++)
    {
	if (xfs[i]->max_bounds.width<min_width)
	{
	    min_width = xfs[i]->max_bounds.width;
	    min_font_idx = i;
	}
    }
    for (i = 0; i < fn; i++)
    {
	if (	   xfs[i]->max_bounds.width != 2 * min_width
		&& xfs[i]->max_bounds.width != min_width)
	{
	    EMSG2(_("Fontset name: %s\n"), base_name);
	    EMSG2(_("Font0: %s\n"), font_name[min_font_idx]);
	    EMSG2(_("Font1: %s\n"), font_name[i]);
	    EMSGN(_("Font%d width is not twice that of font0\n"), i);
	    EMSGN(_("Font0 width: %ld\n"), xfs[min_font_idx]->max_bounds.width);
	    EMSGN(_("Font1 width: %ld\n\n"), xfs[i]->max_bounds.width);
	    return FAIL;
	}
    }
    /* it seems ok. Good Luck!! */
    return OK;
}

    static int
fontset_width(fs)
    XFontSet fs;
{
    return XmbTextEscapement(fs, "Vim", 3) / 3;
}

    static int
fontset_height(fs)
    XFontSet fs;
{
    XFontSetExtents *extents;

    extents = XExtentsOfFontSet(fs);
    return extents->max_logical_extent.height;
}

/* NOT USED YET
    static int
fontset_descent(fs)
    XFontSet fs;
{
    XFontSetExtents *extents;

    extents = XExtentsOfFontSet (fs);
    return extents->max_logical_extent.height + extents->max_logical_extent.y;
}
*/

    static int
fontset_ascent(fs)
    XFontSet fs;
{
    XFontSetExtents *extents;

    extents = XExtentsOfFontSet(fs);
    return -extents->max_logical_extent.y;
}

#endif /* FEAT_XFONTSET */

/*
 * Return the Pixel value (color) for the given color name.
 * Return -1 for error.
 */
    guicolor_T
gui_mch_get_color(reqname)
    char_u *reqname;
{
    int		i;
    char_u	*name = reqname;
    Colormap	colormap;
    XColor      color;
    static char *(vimnames[][2]) =
    {
	/* A number of colors that some X11 systems don't have */
	{"LightRed",	"#FFBBBB"},
	{"LightGreen",	"#88FF88"},
	{"LightMagenta","#FFBBFF"},
	{"DarkCyan",	"#008888"},
	{"DarkBlue",	"#0000BB"},
	{"DarkRed",	"#BB0000"},
	{"DarkMagenta",	"#BB00BB"},
	{"DarkGrey",	"#BBBBBB"},
	{"DarkYellow",	"#BBBB00"},
	{NULL, NULL}
    };

    /* can't do this when GUI not running */
    if (!gui.in_use || *reqname == NUL)
	return (guicolor_T)-1;

    colormap = DefaultColormap(gui.dpy, XDefaultScreen(gui.dpy));

    /* Do this twice if the name isn't recognized. */
    while (name != NULL)
    {
	if (XParseColor(gui.dpy, colormap, (char *)name, &color) != 0
		&& (XAllocColor(gui.dpy, colormap, &color) != 0
		    || find_closest_color(colormap, &color) == OK))
	    return (guicolor_T)color.pixel;

	/* check for a few builtin names */
	for (i = 0; ; ++i)
	{
	    if (vimnames[i][0] == NULL)
	    {
		name = NULL;
		break;
	    }
	    if (STRICMP(name, vimnames[i][0]) == 0)
	    {
		name = (char_u *)vimnames[i][1];
		break;
	    }
	}
    }
    EMSG2(_("Cannot allocate color %s"), reqname);

    return (guicolor_T)-1;
}

/*
 * Find closest color for "colorPtr" in "colormap".  set "colorPtr" to the
 * resulting color.
 * Based on a similar function in TCL.
 * Return FAIL if not able to find or allocate a color.
 */
    static int
find_closest_color(colormap, colorPtr)
    Colormap	colormap;
    XColor	*colorPtr;
{
    double	tmp, distance, closestDistance;
    int		i, closest, numFound, cmap_size;
    XColor	*colortable;
    XVisualInfo	template, *visInfoPtr;

    template.visualid = XVisualIDFromVisual(DefaultVisual(gui.dpy,
						    XDefaultScreen(gui.dpy)));
    visInfoPtr = XGetVisualInfo(gui.dpy, (long)VisualIDMask,
							&template, &numFound);
    if (numFound < 1)
	/* FindClosestColor couldn't lookup visual */
	return FAIL;

    cmap_size = visInfoPtr->colormap_size;
    XFree((char *)visInfoPtr);
    colortable = (XColor *)alloc((unsigned)(cmap_size * sizeof(XColor)));
    if (!colortable)
	return FAIL;  /* out of memory */

    for (i = 0; i  < cmap_size; i++)
	colortable[i].pixel = (unsigned long)i;
    XQueryColors (gui.dpy, colormap, colortable, cmap_size);

    /*
     * Find the color that best approximates the desired one, then
     * try to allocate that color.  If that fails, it must mean that
     * the color was read-write (so we can't use it, since it's owner
     * might change it) or else it was already freed.  Try again,
     * over and over again, until something succeeds.
     */
    closestDistance = 1e30;
    closest = 0;
    for (i = 0; i < cmap_size; i++)
    {
	/*
	 * Use Euclidean distance in RGB space, weighted by Y (of YIQ)
	 * as the objective function;  this accounts for differences
	 * in the color sensitivity of the eye.
	 */
	tmp = .30 * (((int)colorPtr->red) - (int)colortable[i].red);
	distance = tmp * tmp;
	tmp = .61 * (((int)colorPtr->green) - (int)colortable[i].green);
	distance += tmp * tmp;
	tmp = .11 * (((int)colorPtr->blue) - (int)colortable[i].blue);
	distance += tmp * tmp;
	if (distance < closestDistance)
	{
	    closest = i;
	    closestDistance = distance;
	}
    }

    if (XAllocColor(gui.dpy, colormap, &colortable[closest]) != 0)
    {
	gui.color_approx = TRUE;
	*colorPtr = colortable[closest];
    }

    free(colortable);
    return OK;
}

    void
gui_mch_set_fg_color(color)
    guicolor_T	color;
{
    if (color != prev_fg_color)
    {
	XSetForeground(gui.dpy, gui.text_gc, (Pixel)color);
	prev_fg_color = color;
    }
}

/*
 * Set the current text background color.
 */
    void
gui_mch_set_bg_color(color)
    guicolor_T	color;
{
    if (color != prev_bg_color)
    {
	XSetBackground(gui.dpy, gui.text_gc, (Pixel)color);
	prev_bg_color = color;
    }
}

/*
 * create a mouse pointer that is blank
 */
    static Cursor
gui_x11_create_blank_mouse()
{
    Pixmap blank_pixmap = XCreatePixmap(gui.dpy, gui.wid, 1, 1, 1);
    GC gc = XCreateGC(gui.dpy, blank_pixmap, (unsigned long)0, (XGCValues*)0);
    XDrawPoint(gui.dpy, blank_pixmap, gc, 0, 0);
    XFreeGC(gui.dpy, gc);
    return XCreatePixmapCursor(gui.dpy, blank_pixmap, blank_pixmap,
	    (XColor*)&gui.norm_pixel, (XColor*)&gui.norm_pixel, 0, 0);
}

    void
gui_mch_draw_string(row, col, s, len, flags)
    int		row;
    int		col;
    char_u	*s;
    int		len;
    int		flags;
{
    int			cells = len;
#ifdef FEAT_MBYTE
    static XChar2b	*buf = NULL;
    static int		buflen = 0;
    char_u		*p;
    int			wlen = 0;
    int			c;

    if (enc_utf8)
    {
	/* Convert UTF-8 byte sequence to 16 bit characters for the X
	 * functions.  Need a buffer for the 16 bit characters.  Keep it
	 * between calls, because allocating it each time is slow. */
	if (buflen < len)
	{
	    XtFree((char *)buf);
	    buf = (XChar2b *)XtMalloc(len * sizeof(XChar2b));
	    buflen = len;
	}
	p = s;
	cells = 0;
	while (p < s + len)
	{
	    c = utf_ptr2char(p);
	    if (c >= 0x10000)	/* show chars > 0xffff as ? */
		c = 0xbf;
	    buf[wlen].byte1 = (unsigned)c >> 8;
	    buf[wlen].byte2 = c;
	    ++wlen;
	    cells += utf_char2cells(c);
	    p += utf_ptr2len_check(p);
	}
    }
    else if (has_mbyte)
    {
	cells = 0;
	for (p = s; p < s + len; )
	{
	    cells += ptr2cells(p);
	    p += (*mb_ptr2len_check)(p);
	}
    }

#endif

#ifdef FEAT_XFONTSET
    if (current_fontset != NULL)
    {
	/* Setup a clip rectangle to avoid spilling over in the next or
	 * previous line.  This is apparently needed for some fonts which are
	 * used in a fontset. */
	XRectangle	clip;

	clip.x = 0;
	clip.y = 0;
	clip.height = gui.char_height;
	clip.width = gui.char_width * cells + 1;
	XSetClipRectangles(gui.dpy, gui.text_gc, FILL_X(col), FILL_Y(row),
		&clip, 1, Unsorted);
    }
#endif

    if (flags & DRAW_TRANSP)
    {
#ifdef FEAT_MBYTE
	if (enc_utf8)
	    XDrawString16(gui.dpy, gui.wid, gui.text_gc, TEXT_X(col),
		    TEXT_Y(row), buf, wlen);
	else
#endif
	    XDrawString(gui.dpy, gui.wid, gui.text_gc, TEXT_X(col),
		    TEXT_Y(row), (char *)s, len);
    }
    else if (p_linespace
#ifdef FEAT_XFONTSET
	    || current_fontset != NULL
#endif
	    )
    {
	XSetForeground(gui.dpy, gui.text_gc, prev_bg_color);
	XFillRectangle(gui.dpy, gui.wid, gui.text_gc, FILL_X(col),
		FILL_Y(row), gui.char_width * cells, gui.char_height);
	XSetForeground(gui.dpy, gui.text_gc, prev_fg_color);
#ifdef FEAT_MBYTE
	if (enc_utf8)
	    XDrawString16(gui.dpy, gui.wid, gui.text_gc, TEXT_X(col),
		    TEXT_Y(row), buf, wlen);
	else
#endif
	    XDrawString(gui.dpy, gui.wid, gui.text_gc, TEXT_X(col),
		    TEXT_Y(row), (char *)s, len);
    }
    else
    {
	/* XmbDrawImageString has bug, don't use it for fontset. */
#ifdef FEAT_MBYTE
	if (enc_utf8)
	    XDrawImageString16(gui.dpy, gui.wid, gui.text_gc, TEXT_X(col),
		    TEXT_Y(row), buf, wlen);
	else
#endif
	    XDrawImageString(gui.dpy, gui.wid, gui.text_gc, TEXT_X(col),
		    TEXT_Y(row), (char *)s, len);
    }

    /* Bold trick: draw the text again with a one-pixel offset. */
    if (flags & DRAW_BOLD)
    {
#ifdef FEAT_MBYTE
	if (enc_utf8)
	    XDrawString16(gui.dpy, gui.wid, gui.text_gc, TEXT_X(col) + 1,
		    TEXT_Y(row), buf, wlen);
	else
#endif
	    XDrawString(gui.dpy, gui.wid, gui.text_gc, TEXT_X(col) + 1,
		    TEXT_Y(row), (char *)s, len);
    }

    /* Underline: draw a line at the bottom of the character cell. */
    if (flags & DRAW_UNDERL)
	XDrawLine(gui.dpy, gui.wid, gui.text_gc, FILL_X(col),
	     FILL_Y(row + 1) - 1, FILL_X(col + cells) - 1, FILL_Y(row + 1) - 1);

#ifdef FEAT_XFONTSET
    if (current_fontset != NULL)
	XSetClipMask(gui.dpy, gui.text_gc, None);
#endif
}

/*
 * Return OK if the key with the termcap name "name" is supported.
 */
    int
gui_mch_haskey(name)
    char_u  *name;
{
    int i;

    for (i = 0; special_keys[i].key_sym != (KeySym)0; i++)
	if (name[0] == special_keys[i].vim_code0 &&
					 name[1] == special_keys[i].vim_code1)
	    return OK;
    return FAIL;
}

/*
 * Return the text window-id and display.  Only required for X-based GUI's
 */
    int
gui_get_x11_windis(win, dis)
    Window  *win;
    Display **dis;
{
    *win = XtWindow(vimShell);
    *dis = gui.dpy;
    return OK;
}

    void
gui_mch_beep()
{
    XBell(gui.dpy, 0);
}

    void
gui_mch_flash(msec)
    int		msec;
{
    /* Do a visual beep by reversing the foreground and background colors */
    XFillRectangle(gui.dpy, gui.wid, gui.invert_gc, 0, 0,
	    FILL_X((int)Columns) + gui.border_offset,
	    FILL_Y((int)Rows) + gui.border_offset);
    XSync(gui.dpy, False);
    ui_delay((long)msec, TRUE);	/* wait for a few msec */
    XFillRectangle(gui.dpy, gui.wid, gui.invert_gc, 0, 0,
	    FILL_X((int)Columns) + gui.border_offset,
	    FILL_Y((int)Rows) + gui.border_offset);
}

/*
 * Invert a rectangle from row r, column c, for nr rows and nc columns.
 */
    void
gui_mch_invert_rectangle(r, c, nr, nc)
    int	    r;
    int	    c;
    int	    nr;
    int	    nc;
{
    XFillRectangle(gui.dpy, gui.wid, gui.invert_gc,
	FILL_X(c), FILL_Y(r), (nc) * gui.char_width, (nr) * gui.char_height);
}

/*
 * Iconify the GUI window.
 */
    void
gui_mch_iconify()
{
    XIconifyWindow(gui.dpy, XtWindow(vimShell), DefaultScreen(gui.dpy));
}

/*
 * Draw a cursor without focus.
 */
    void
gui_mch_draw_hollow_cursor(color)
    guicolor_T color;
{
    int		w = 1;

#ifdef FEAT_MBYTE
    if (mb_lefthalve(gui.row, gui.col))
	w = 2;
#endif
    gui_mch_set_fg_color(color);
    XDrawRectangle(gui.dpy, gui.wid, gui.text_gc, FILL_X(gui.col),
	    FILL_Y(gui.row), w * gui.char_width - 1, gui.char_height - 1);
}

/*
 * Draw part of a cursor, "w" pixels wide, and "h" pixels high, using
 * color "color".
 */
    void
gui_mch_draw_part_cursor(w, h, color)
    int		w;
    int		h;
    guicolor_T	color;
{
    gui_mch_set_fg_color(color);

    XFillRectangle(gui.dpy, gui.wid, gui.text_gc,
#ifdef FEAT_RIGHTLEFT
	    /* vertical line should be on the right of current point */
	    !(State & CMDLINE) && curwin->w_p_rl ? FILL_X(gui.col + 1) - w :
#endif
		FILL_X(gui.col),
	    FILL_Y(gui.row) + gui.char_height - h + (int)p_linespace / 2,
	    w, h - (int)p_linespace);
}

/*
 * Catch up with any queued X events.  This may put keyboard input into the
 * input buffer, call resize call-backs, trigger timers etc.  If there is
 * nothing in the X event queue (& no timers pending), then we return
 * immediately.
 */
    void
gui_mch_update()
{
    XtInputMask mask, desired;

#ifdef ALT_X_INPUT
    if (suppress_alternate_input)
	desired = (XtIMXEvent | XtIMTimer);
    else
#endif
	desired = (XtIMAll);
    while ((mask = XtAppPending(app_context)) && (mask & desired) &&
		!vim_is_input_buf_full())
	XtAppProcessEvent(app_context, desired);
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
gui_mch_wait_for_chars(wtime)
    long    wtime;
{
    int		    focus;

    /*
     * Make this static, in case gui_x11_timer_cb is called after leaving
     * this function (otherwise a random value on the stack may be changed).
     */
    static int	    timed_out;
    XtIntervalId    timer = (XtIntervalId)0;
    XtInputMask	    desired;
#ifdef FEAT_SNIFF
    static int	    sniff_on = 0;
    static XtInputId sniff_input_id = 0;
#endif

    timed_out = FALSE;

#ifdef FEAT_SNIFF
    if (sniff_on && !want_sniff_request)
    {
	if (sniff_input_id)
	    XtRemoveInput(sniff_input_id);
	sniff_on = 0;
    }
    else if (!sniff_on && want_sniff_request)
    {
	sniff_input_id = XtAppAddInput(app_context, fd_from_sniff,
		     (XtPointer)XtInputReadMask, gui_x11_sniff_request_cb, 0);
	sniff_on = 1;
    }
#endif

    if (wtime > 0)
	timer = XtAppAddTimeOut(app_context, (long_u)wtime, gui_x11_timer_cb,
								  &timed_out);

    focus = gui.in_focus;
#ifdef ALT_X_INPUT
    if (suppress_alternate_input)
	desired = (XtIMXEvent | XtIMTimer);
    else
#endif
	desired = (XtIMAll);
    while (!timed_out)
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

	/*
	 * Don't use gui_mch_update() because then we will spin-lock until a
	 * char arrives, instead we use XtAppProcessEvent() to hang until an
	 * event arrives.  No need to check for input_buf_full because we are
	 * returning as soon as it contains a single char.  Note that
	 * XtAppNextEvent() may not be used because it will not return after a
	 * timer event has arrived -- webb
	 */
	XtAppProcessEvent(app_context, desired);

	if (!vim_is_input_buf_empty())
	{
	    if (timer != (XtIntervalId)0 && !timed_out)
		XtRemoveTimeOut(timer);
	    return OK;
	}
    }
    return FAIL;
}

/*
 * Output routines.
 */

/* Flush any output to the screen */
    void
gui_mch_flush()
{
    XFlush(gui.dpy);
}

/*
 * Clear a rectangular region of the screen from text pos (row1, col1) to
 * (row2, col2) inclusive.
 */
    void
gui_mch_clear_block(row1, col1, row2, col2)
    int		row1;
    int		col1;
    int		row2;
    int		col2;
{
    int		x;

    x = FILL_X(col1);

    /* Clear one extra pixel at the far right, for when bold characters have
     * spilled over to the next column. */
    XFillRectangle(gui.dpy, gui.wid, gui.back_gc, x, FILL_Y(row1),
	    (col2 - col1 + 1) * gui.char_width + (col2 == Columns - 1),
	    (row2 - row1 + 1) * gui.char_height);
}

    void
gui_mch_clear_all()
{
    XClearArea(gui.dpy, gui.wid, 0, 0, 0, 0, False);
}

/*
 * Delete the given number of lines from the given row, scrolling up any
 * text further down within the scroll region.
 */
    void
gui_mch_delete_lines(row, num_lines)
    int	    row;
    int	    num_lines;
{
    if (gui.visibility == VisibilityFullyObscured)
	return;	    /* Can't see the window */

    /* copy one extra pixel at the far right, for when bold has spilled
     * over */
    XCopyArea(gui.dpy, gui.wid, gui.wid, gui.text_gc,
	FILL_X(gui.scroll_region_left), FILL_Y(row + num_lines),
	gui.char_width * (gui.scroll_region_right - gui.scroll_region_left + 1)
			       + (gui.scroll_region_right == Columns - 1),
	gui.char_height * (gui.scroll_region_bot - row - num_lines + 1),
	FILL_X(gui.scroll_region_left), FILL_Y(row));

    gui_clear_block(gui.scroll_region_bot - num_lines + 1,
						       gui.scroll_region_left,
			  gui.scroll_region_bot, gui.scroll_region_right);
    gui_x11_check_copy_area();
}

/*
 * Insert the given number of lines before the given row, scrolling down any
 * following text within the scroll region.
 */
    void
gui_mch_insert_lines(row, num_lines)
    int	    row;
    int	    num_lines;
{
    if (gui.visibility == VisibilityFullyObscured)
	return;	    /* Can't see the window */

    /* copy one extra pixel at the far right, for when bold has spilled
     * over */
    XCopyArea(gui.dpy, gui.wid, gui.wid, gui.text_gc,
	FILL_X(gui.scroll_region_left), FILL_Y(row),
	gui.char_width * (gui.scroll_region_right - gui.scroll_region_left + 1)
			       + (gui.scroll_region_right == Columns - 1),
	gui.char_height * (gui.scroll_region_bot - row - num_lines + 1),
	FILL_X(gui.scroll_region_left), FILL_Y(row + num_lines));

    gui_clear_block(row, gui.scroll_region_left,
				row + num_lines - 1, gui.scroll_region_right);
    gui_x11_check_copy_area();
}

/*
 * Update the region revealed by scrolling up/down.
 */
    static void
gui_x11_check_copy_area()
{
    XEvent		    event;
    XGraphicsExposeEvent    *gevent;

    if (gui.visibility != VisibilityPartiallyObscured)
	return;

    XFlush(gui.dpy);

    /* Wait to check whether the scroll worked or not */
    for (;;)
    {
	if (XCheckTypedEvent(gui.dpy, NoExpose, &event))
	    return;	/* The scroll worked. */

	if (XCheckTypedEvent(gui.dpy, GraphicsExpose, &event))
	{
	    gevent = (XGraphicsExposeEvent *)&event;
	    gui_redraw(gevent->x, gevent->y, gevent->width, gevent->height);
	    if (gevent->count == 0)
		return;		/* This was the last expose event */
	}
	XSync(gui.dpy, False);
    }
}

/*
 * X Selection stuff, for cutting and pasting text to other windows.
 */

    void
clip_mch_lose_selection(cbd)
    VimClipboard	*cbd;
{
    clip_x11_lose_selection(vimShell, cbd);
}

    int
clip_mch_own_selection(cbd)
    VimClipboard	*cbd;
{
    return clip_x11_own_selection(vimShell, cbd);
}

    void
clip_mch_request_selection(cbd)
    VimClipboard	*cbd;
{
    clip_x11_request_selection(vimShell, gui.dpy, cbd);
}

    void
clip_mch_set_selection(cbd)
    VimClipboard	*cbd;
{
    clip_x11_set_selection(cbd);
}

#if defined(FEAT_MENU) || defined(PROTO)
/*
 * Menu stuff.
 */

/*
 * Make a menu either grey or not grey.
 */
    void
gui_mch_menu_grey(menu, grey)
    vimmenu_T	*menu;
    int		grey;
{
    if (menu->id != (Widget)0)
    {
	gui_mch_menu_hidden(menu, False);
	if (grey
#ifdef FEAT_GUI_MOTIF
		|| !menu->sensitive
#endif
		)
	    XtSetSensitive(menu->id, False);
	else
	    XtSetSensitive(menu->id, True);
    }
}

/*
 * Make menu item hidden or not hidden
 */
    void
gui_mch_menu_hidden(menu, hidden)
    vimmenu_T	*menu;
    int		hidden;
{
    if (menu->id != (Widget)0)
    {
	if (hidden)
	    XtUnmanageChild(menu->id);
	else
	    XtManageChild(menu->id);
    }
}

/*
 * This is called after setting all the menus to grey/hidden or not.
 */
    void
gui_mch_draw_menubar()
{
    /* Nothing to do in X */
}

/* ARGSUSED */
    void
gui_x11_menu_cb(w, client_data, call_data)
    Widget	w;
    XtPointer	client_data, call_data;
{
    gui_menu_cb((vimmenu_T *)client_data);
}

#endif /* FEAT_MENU */



/*
 * Function called when window closed.	Works like ":qa".
 * Should put up a requester!
 */
/*ARGSUSED*/
    static void
gui_x11_wm_protocol_handler(w, client_data, event, dum)
    Widget	w;
    XtPointer	client_data;
    XEvent	*event;
    Boolean	*dum;
{
    /*
     * Only deal with Client messages.
     */
    if (event->type != ClientMessage)
	return;

    /*
     * The WM_SAVE_YOURSELF event arrives when the window manager wants to
     * exit.  That can be cancelled though, thus Vim shouldn't exit here.
     * Just sync our swap files.
     */
    if (((XClientMessageEvent *)event)->data.l[0] ==
						  wm_atoms[SAVE_YOURSELF_IDX])
    {
	out_flush();
	ml_sync_all(FALSE, FALSE);	/* preserve all swap files */

	/* Set the window's WM_COMMAND property, to let the window manager
	 * know we are done saving ourselves.  We don't want to be restarted,
	 * thus set argv to NULL. */
	XSetCommand(gui.dpy, XtWindow(vimShell), NULL, 0);
	return;
    }

    if (((XClientMessageEvent *)event)->data.l[0] !=
						  wm_atoms[DELETE_WINDOW_IDX])
	return;

    gui_shell_closed();
}

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
static XtIntervalId	blink_timer = (XtIntervalId)0;

    void
gui_mch_set_blinking(waittime, on, off)
    long    waittime, on, off;
{
    blink_waittime = waittime;
    blink_ontime = on;
    blink_offtime = off;
}

/*
 * Stop the cursor blinking.  Show the cursor if it wasn't shown.
 */
    void
gui_mch_stop_blink()
{
    if (blink_timer != (XtIntervalId)0)
    {
	XtRemoveTimeOut(blink_timer);
	blink_timer = (XtIntervalId)0;
    }
    if (blink_state == BLINK_OFF)
	gui_update_cursor(TRUE, FALSE);
    blink_state = BLINK_NONE;
}

/*
 * Start the cursor blinking.  If it was already blinking, this restarts the
 * waiting time and shows the cursor.
 */
    void
gui_mch_start_blink()
{
    if (blink_timer != (XtIntervalId)0)
	XtRemoveTimeOut(blink_timer);
    /* Only switch blinking on if none of the times is zero */
    if (blink_waittime && blink_ontime && blink_offtime && gui.in_focus)
    {
	blink_timer = XtAppAddTimeOut(app_context, blink_waittime,
						      gui_x11_blink_cb, NULL);
	blink_state = BLINK_ON;
	gui_update_cursor(TRUE, FALSE);
    }
}

/* ARGSUSED */
    static void
gui_x11_blink_cb(timed_out, interval_id)
    XtPointer	    timed_out;
    XtIntervalId    *interval_id;
{
    if (blink_state == BLINK_ON)
    {
	gui_undraw_cursor();
	blink_state = BLINK_OFF;
	blink_timer = XtAppAddTimeOut(app_context, blink_offtime,
						      gui_x11_blink_cb, NULL);
    }
    else
    {
	gui_update_cursor(TRUE, FALSE);
	blink_state = BLINK_ON;
	blink_timer = XtAppAddTimeOut(app_context, blink_ontime,
						      gui_x11_blink_cb, NULL);
    }
}

/*
 * Return the lightness of a pixel.  White is 255.
 */
    int
gui_mch_get_lightness(pixel)
    guicolor_T	pixel;
{
    XColor	xc;
    Colormap	colormap;

    colormap = DefaultColormap(gui.dpy, XDefaultScreen(gui.dpy));

    xc.pixel = pixel;
    XQueryColor(gui.dpy, colormap, &xc);

    return (int)(xc.red * 3 + xc.green * 6 + xc.blue) / (10 * 256);
}

#if (defined(FEAT_SYN_HL) && defined(FEAT_EVAL)) || defined(PROTO)
/*
 * Return the RGB value of a pixel as "#RRGGBB".
 */
    char_u *
gui_mch_get_rgb(pixel)
    guicolor_T	pixel;
{
    XColor	xc;
    Colormap	colormap;
    static char_u retval[10];

    colormap = DefaultColormap(gui.dpy, XDefaultScreen(gui.dpy));

    xc.pixel = pixel;
    XQueryColor(gui.dpy, colormap, &xc);

    sprintf((char *)retval, "#%02x%02x%02x",
	    (unsigned)xc.red >> 8,
	    (unsigned)xc.green >> 8,
	    (unsigned)xc.blue >> 8);
    return retval;
}
#endif

/*
 * Add the callback functions.
 */
    void
gui_x11_callbacks(textArea, vimForm)
    Widget textArea;
    Widget vimForm;
{
    XtAddEventHandler(textArea, VisibilityChangeMask, FALSE,
	gui_x11_visibility_cb, (XtPointer)0);

    XtAddEventHandler(textArea, ExposureMask, FALSE, gui_x11_expose_cb,
	(XtPointer)0);

    XtAddEventHandler(vimShell, StructureNotifyMask, FALSE,
	gui_x11_resize_window_cb, (XtPointer)0);

    XtAddEventHandler(vimShell, FocusChangeMask, FALSE, gui_x11_focus_change_cb,
	(XtPointer)0);
    /*
     * Only install these enter/leave callbacks when 'p' in 'guioptions'.
     * Only needed for some window managers.
     */
    if (vim_strchr(p_go, GO_POINTER) != NULL)
    {
	XtAddEventHandler(vimShell, LeaveWindowMask, FALSE, gui_x11_leave_cb,
	    (XtPointer)0);
	XtAddEventHandler(textArea, LeaveWindowMask, FALSE, gui_x11_leave_cb,
	    (XtPointer)0);
	XtAddEventHandler(textArea, EnterWindowMask, FALSE, gui_x11_enter_cb,
	    (XtPointer)0);
	XtAddEventHandler(vimShell, EnterWindowMask, FALSE, gui_x11_enter_cb,
	    (XtPointer)0);
    }

    XtAddEventHandler(vimForm, KeyPressMask, FALSE, gui_x11_key_hit_cb,
	(XtPointer)0);
    XtAddEventHandler(textArea, KeyPressMask, FALSE, gui_x11_key_hit_cb,
	(XtPointer)0);

    /* get pointer moved events from scrollbar, needed for 'mousefocus' */
    XtAddEventHandler(vimForm, PointerMotionMask,
	FALSE, gui_x11_mouse_cb, (XtPointer)1);
    XtAddEventHandler(textArea, ButtonPressMask | ButtonReleaseMask |
					 ButtonMotionMask | PointerMotionMask,
	FALSE, gui_x11_mouse_cb, (XtPointer)0);
}

/*
 * Get current y mouse coordinate in text window.
 * Return -1 when unknown.
 */
    int
gui_mch_get_mouse_x()
{
    int		rootx, rooty, winx, winy;
    Window	root, child;
    unsigned int mask;

    if (XQueryPointer(gui.dpy, XtWindow(vimShell), &root, &child,
		&rootx, &rooty, &winx, &winy, &mask))
    {
	if (gui.which_scrollbars[SBAR_LEFT])
	    return winx - gui.scrollbar_width;
	return winx;
    }
    return -1;
}

    int
gui_mch_get_mouse_y()
{
    int		rootx, rooty, winx, winy;
    Window	root, child;
    unsigned int mask;

    if (XQueryPointer(gui.dpy, XtWindow(vimShell), &root, &child,
		&rootx, &rooty, &winx, &winy, &mask))
	return winy
#ifdef FEAT_MENU
	    - gui.menu_height
#endif
	    ;
    return -1;
}

    void
gui_mch_setmouse(x, y)
    int		x;
    int		y;
{
    if (gui.which_scrollbars[SBAR_LEFT])
	x += gui.scrollbar_width;
    XWarpPointer(gui.dpy, (Window)0, XtWindow(vimShell), 0, 0, 0, 0,
						      x, y
#ifdef FEAT_MENU
						      + gui.menu_height
#endif
						      );
}

#if (defined(FEAT_GUI_MOTIF) && defined(FEAT_MENU)) || defined(PROTO)
    XButtonPressedEvent *
gui_x11_get_last_mouse_event()
{
    return &last_mouse_event;
}
#endif

#if defined(FEAT_SIGNS) || defined(PROTO)

/* Signs are currently always 2 chars wide.  Hopefully the font is big enough
 * to provide room for the bitmap! */
# define SIGN_WIDTH (gui.char_width * 2)

#if 0	/* not used */
    void
gui_mch_clearsign(row)
    int		row;
{
    if (gui.in_use)
	XClearArea(gui.dpy, gui.wid, 0, TEXT_Y(row) - gui.char_height,
		SIGN_WIDTH, gui.char_height, FALSE);
}
#endif

    void
gui_mch_drawsign(row, sign_idx)
    int		row;
    int		sign_idx;		/* index into the highlight table */
{
    XImage	*sign;

    if (gui.in_use && (sign = get_debug_sign(sign_idx)) != NULL)
    {
	XClearArea(gui.dpy, gui.wid, 0, TEXT_Y(row) - sign->height,
		SIGN_WIDTH, gui.char_height, FALSE);
	XPutImage(gui.dpy, gui.wid, gui.text_gc, sign, 0, 0,
		(SIGN_WIDTH - sign->width) / 2,
		TEXT_Y(row) - sign->height,
		sign->width, sign->height);
    }
}

    XImage *
gui_mch_register_sign(signfile)
    char	    *signfile;
{
    XpmAttributes   attrs;
    XImage	    *sign;
    int		    status;

    /*
     * Setup the color substitution table.
     */
    sign = NULL;
    if ((signfile[0] != 0) && (signfile[0] != '-'))
    {
	sign = (XImage *)alloc(sizeof(XImage));
	if (sign == NULL)
	    EMSG(_("Cannot allocate memory for debugger sign"));
	else
	{
	    attrs.valuemask = XpmColorSymbols;
	    attrs.numsymbols = 2;
	    attrs.colorsymbols = (XpmColorSymbol *)
		alloc(sizeof(XpmColorSymbol) * attrs.numsymbols);
	    attrs.colorsymbols[0].name = "BgColor";
	    attrs.colorsymbols[0].value = NULL;
	    attrs.colorsymbols[0].pixel = gui.back_pixel;
	    attrs.colorsymbols[1].name = "FgColor";
	    attrs.colorsymbols[1].value = NULL;
	    attrs.colorsymbols[1].pixel = gui.norm_pixel;
	    status = XpmReadFileToImage(gui.dpy, signfile, &sign, NULL, &attrs);

	    if (status == 0)
	    {
		/* Sign width is fixed at two columns now.
		if (sign->width > gui.sign_width)
		    gui.sign_width = sign->width + 8; */
	    }
	    else
	    {
		vim_free(sign);
		sign = NULL;
		EMSG(_("Error -- couldn't read in sign data!"));
	    }
	    vim_free((char *) attrs.colorsymbols);
	}
    }

    return sign;
}
#endif


#ifdef FEAT_MOUSESHAPE
/* The last set mouse pointer shape is remembered, to be used when it goes
 * from hidden to not hidden. */
static int last_shape = 0;
#endif

/*
 * Use the blank mouse pointer or not.
 */
    void
gui_mch_mousehide(hide)
    int		hide;	/* TRUE = use blank ptr, FALSE = use parent ptr */
{
    if (gui.pointer_hidden != hide)
    {
	gui.pointer_hidden = hide;
	if (hide)
	    XDefineCursor(gui.dpy, gui.wid, gui.blank_pointer);
	else
#ifdef FEAT_MOUSESHAPE
	    mch_set_mouse_shape(last_shape);
#else
	    XUndefineCursor(gui.dpy, gui.wid);
#endif
    }
}

#if defined(FEAT_MOUSESHAPE) || defined(PROTO)

/* Table for shape IDs.  Keep in sync with the mshape_names[] table in
 * misc2.c! */
static int mshape_ids[] =
{
    XC_left_ptr,		/* arrow */
    0,				/* blank */
    XC_xterm,			/* beam */
    XC_sb_v_double_arrow,	/* updown */
    XC_sizing,			/* udsizing */
    XC_sb_h_double_arrow,	/* leftright */
    XC_sizing,			/* lrsizing */
    XC_watch,			/* busy */
    XC_X_cursor,		/* no */
    XC_crosshair,		/* crosshair */
    XC_hand1,			/* hand1 */
    XC_hand2,			/* hand2 */
    XC_pencil,			/* pencil */
    XC_question_arrow,		/* question */
    XC_right_ptr,		/* right-arrow */
    XC_center_ptr,		/* up-arrow */
    XC_left_ptr			/* last one */
};

    void
mch_set_mouse_shape(shape)
    int	shape;
{
    int	    id;

    if (!gui.in_use)
	return;

    if (shape == MSHAPE_HIDE || gui.pointer_hidden)
	XDefineCursor(gui.dpy, gui.wid, gui.blank_pointer);
    else
    {
	if (shape >= MSHAPE_NUMBERED)
	{
	    id = shape - MSHAPE_NUMBERED;
	    if (id >= XC_num_glyphs)
		id = XC_left_ptr;
	    else
		id &= ~1;	/* they are always even (why?) */
	}
	else
	    id = mshape_ids[shape];

	XDefineCursor(gui.dpy, gui.wid, XCreateFontCursor(gui.dpy, id));
    }
    if (shape != MSHAPE_HIDE)
	last_shape = shape;
}
#endif

#if defined(FEAT_TOOLBAR) || defined(PROTO)
/*
 * Icons used by the toolbar code.
 */
#include "../pixmaps/tb_new.xpm"
#include "../pixmaps/tb_open.xpm"
#include "../pixmaps/tb_close.xpm"
#include "../pixmaps/tb_save.xpm"
#include "../pixmaps/tb_print.xpm"
#include "../pixmaps/tb_cut.xpm"
#include "../pixmaps/tb_copy.xpm"
#include "../pixmaps/tb_paste.xpm"
#include "../pixmaps/tb_find.xpm"
#include "../pixmaps/tb_find_next.xpm"
#include "../pixmaps/tb_find_prev.xpm"
#include "../pixmaps/tb_find_help.xpm"
#include "../pixmaps/tb_exit.xpm"
#include "../pixmaps/tb_undo.xpm"
#include "../pixmaps/tb_redo.xpm"
#include "../pixmaps/tb_help.xpm"
#include "../pixmaps/tb_macro.xpm"
#include "../pixmaps/tb_make.xpm"
#include "../pixmaps/tb_save_all.xpm"
#include "../pixmaps/tb_jump.xpm"
#include "../pixmaps/tb_ctags.xpm"
#include "../pixmaps/tb_load_session.xpm"
#include "../pixmaps/tb_save_session.xpm"
#include "../pixmaps/tb_new_session.xpm"
#include "../pixmaps/tb_blank.xpm"
#include "../pixmaps/tb_maximize.xpm"
#include "../pixmaps/tb_split.xpm"
#include "../pixmaps/tb_minimize.xpm"
#include "../pixmaps/tb_shell.xpm"
#include "../pixmaps/tb_replace.xpm"
#include "../pixmaps/tb_vsplit.xpm"
#include "../pixmaps/tb_maxwidth.xpm"
#include "../pixmaps/tb_minwidth.xpm"

/*
 * Those are the pixmaps used for the default buttons.
 */
struct NameToPixmap
{
    char *name;
    char **xpm;
};

static const struct NameToPixmap built_in_pixmaps[] =
{
    {"New", tb_new_xpm},
    {"Open", tb_open_xpm},
    {"Save", tb_save_xpm},
    {"Undo", tb_undo_xpm},
    {"Redo", tb_redo_xpm},
    {"Cut", tb_cut_xpm},
    {"Copy", tb_copy_xpm},
    {"Paste", tb_paste_xpm},
    {"Print", tb_print_xpm},
    {"Help", tb_help_xpm},
    {"Find", tb_find_xpm},
    {"SaveAll",	tb_save_all_xpm},
    {"SaveSesn", tb_save_session_xpm},
    {"NewSesn", tb_new_session_xpm},
    {"LoadSesn", tb_load_session_xpm},
    {"RunScript", tb_macro_xpm},
    {"Replace",	tb_replace_xpm},
    {"WinClose", tb_close_xpm},
    {"WinMax",	tb_maximize_xpm},
    {"WinMin", tb_minimize_xpm},
    {"WinSplit", tb_split_xpm},
    {"Shell", tb_shell_xpm},
    {"FindPrev", tb_find_prev_xpm},
    {"FindNext", tb_find_next_xpm},
    {"FindHelp", tb_find_help_xpm},
    {"Make", tb_make_xpm},
    {"TagJump", tb_jump_xpm},
    {"RunCtags", tb_ctags_xpm},
    {"Exit", tb_exit_xpm},
    {"WinVSplit", tb_vsplit_xpm},
    {"WinMaxWidth", tb_maxwidth_xpm},
    {"WinMinWidth", tb_minwidth_xpm},
    { NULL, NULL} /* end tag */
};

#ifdef FEAT_SUN_WORKSHOP
static const char *(sunws_pixmaps[]) =
{
    "Build",	"$SPRO_WSDIR/lib/locale/%L/graphics/build.xpm",
    "Stop At",	"$SPRO_WSDIR/lib/locale/%L/graphics/stopAt.xpm",
    "Stop In",	"$SPRO_WSDIR/lib/locale/%L/graphics/stopIn.xpm",
    "Clear At",	"$SPRO_WSDIR/lib/locale/%L/graphics/clearAt.xpm",
    "Start",	"$SPRO_WSDIR/lib/locale/%L/graphics/start.xpm",
    "Evaluate",	"$SPRO_WSDIR/lib/locale/%L/graphics/evaluate.xpm",
    "Eval *",	"$SPRO_WSDIR/lib/locale/%L/graphics/evaluate-star.xpm",
    "Up",	"$SPRO_WSDIR/lib/locale/%L/graphics/up.xpm",
    "Down",	"$SPRO_WSDIR/lib/locale/%L/graphics/down.xpm",
    "Go",	"$SPRO_WSDIR/lib/locale/%L/graphics/go.xpm",
    "StepInto",	"$SPRO_WSDIR/lib/locale/%L/graphics/stepInto.xpm",
    "StepOver",	"$SPRO_WSDIR/lib/locale/%L/graphics/stepOver.xpm",
    "StepOut",	"$SPRO_WSDIR/lib/locale/%L/graphics/stepOut.xpm",
    "Fix",	"$SPRO_WSDIR/lib/locale/%L/graphics/fix.xpm",
    "Def",	"$SPRO_WSDIR/lib/locale/%L/graphics/findDef.xpm",
    "Refs",	"$SPRO_WSDIR/lib/locale/%L/graphics/findRefs.xpm",
    NULL,	NULL
};

static Boolean filePredicate __ARGS((String cp));

    static Boolean
filePredicate(cp)
    String cp;
{
    return True;
}
#endif

static void createXpmImages __ARGS((char_u *path, char **xpm, Pixmap *sen, Pixmap *insen));

    void
get_pixmap(name, sen, insen)
    char_u	*name;
    Pixmap	*sen;
    Pixmap	*insen;
{
    int		builtin_num;		/* index into builtin table */
    int		num_pixmaps;		/* entries in builtin pixmap table */
    char_u	buf[MAXPATHL];		/* buffer storing expanded pathname */
#ifdef FEAT_SUN_WORKSHOP
    char	locbuf[MAXPATHL];	/* generate locale pathname */
#endif
    char	**xpm = NULL;		/* xpm array */
    int		i;

    buf[0] = NUL;			/* start with NULL path */
    num_pixmaps = (sizeof(built_in_pixmaps) / sizeof(built_in_pixmaps[0])) - 1;
    if (STRNCMP(name, "BuiltIn", (size_t)7) == 0)
    {
	if (isdigit((int)name[7]) && isdigit((int)name[8]))
	{
	    builtin_num = atoi((char *)name + 7);
	    if (builtin_num >= 0 && builtin_num < num_pixmaps)
		xpm = built_in_pixmaps[builtin_num].xpm;
	    else
		xpm = tb_blank_xpm;
	}
    }
    else
    {
	if (gui_find_bitmap(name, buf, "xpm") == FAIL)
	{
	    for (i = 0; i < num_pixmaps; i++)
	    {
		if (STRCMP(name, built_in_pixmaps[i].name) == 0)
		{
		    xpm = built_in_pixmaps[i].xpm;
		    break;
		}
	    }
#ifdef FEAT_SUN_WORKSHOP
	    if (xpm == NULL)
	    {
		char_u	*path;		/* path with %L resolved to locale */

		for (i = 0; sunws_pixmaps[i] != NULL; i += 2)
		{
		    if (STRCMP(name, sunws_pixmaps[i]) == 0)
		    {
			path = (char_u *)XtResolvePathname(gui.dpy, NULL,
				NULL, ".xpm", sunws_pixmaps[i + 1],
				NULL, 0, filePredicate);
			if (path == NULL)  /* neither LANG nor LC_ALL is set */
			{
			    char *p = strcpy(locbuf, sunws_pixmaps[i + 1]);

			    while ((p = strstr(p, "%L")) != NULL)
			    {
				*p++ = 'C';
				strcpy(p, &p[1]);
			    }
			    path = (char_u *)locbuf;
			    expand_env(path, buf, MAXPATHL);
			}
			else
			{
			    expand_env(path, buf, MAXPATHL);
			    XtFree(path);
			}
			break;
		    }
		}
	    }
#endif
	}
    }

    if (xpm != NULL || buf[0] != NUL)
	createXpmImages(buf, xpm, sen, insen);
}

#include <X11/xpm.h>

/*
 * Read an Xpm file, doing color substitutions for the foreground and
 * background colors. If there is an error reading a color xpm file,
 * drop back and read the monochrome file. If successful, create the
 * insensitive Pixmap too.
 */
    static void
createXpmImages(path, xpm, sen, insen)
    char_u	*path;
    char	**xpm;
    Pixmap	*sen;
    Pixmap	*insen;
{
    Window	rootWindow;
    XpmAttributes attrs;
    int		screenNum;
    int		status;
    GC		mask_gc;
    GC		back_gc;
    XGCValues	gcvalues;
    int		startX;
    int		x, y;
    Pixmap	mask;
    Pixmap	map;
    Pixel	bg_pixel;
    Pixel	fg_pixel;

    gui_mch_get_toolbar_colors(&bg_pixel, &fg_pixel);

    /* Setup the color subsititution table */
    attrs.valuemask = XpmColorSymbols;
    attrs.numsymbols = 2;
    attrs.colorsymbols = (XpmColorSymbol *)
			  XtMalloc(sizeof(XpmColorSymbol) * attrs.numsymbols);
    attrs.colorsymbols[0].name = "BgColor";
    attrs.colorsymbols[0].value = NULL;
    attrs.colorsymbols[0].pixel = bg_pixel;
    attrs.colorsymbols[1].name = "FgColor";
    attrs.colorsymbols[1].value = NULL;
    attrs.colorsymbols[1].pixel = fg_pixel;

    screenNum = DefaultScreen(gui.dpy);
    rootWindow = RootWindow(gui.dpy, screenNum);

    /* Create the "sensitive" pixmap */
    if (xpm != NULL)
	status = XpmCreatePixmapFromData(gui.dpy, rootWindow, xpm,
							 &map, &mask, &attrs);
    else
	status = XpmReadFileToPixmap(gui.dpy, rootWindow, (char *)path,
							 &map, &mask, &attrs);
    if (status == XpmSuccess && map != 0)
    {
	/* Need to create new Pixmaps with the mask applied. */
	gcvalues.foreground = bg_pixel;
	back_gc = XCreateGC(gui.dpy, map, GCForeground, &gcvalues);
	mask_gc = XCreateGC(gui.dpy, map, GCForeground, &gcvalues);
	XSetClipMask(gui.dpy, mask_gc, mask);

	/* Create the "sensitive" pixmap. */
	*sen = XCreatePixmap(gui.dpy, rootWindow,
		 attrs.width, attrs.height, DefaultDepth(gui.dpy, screenNum));
	XFillRectangle(gui.dpy, *sen, back_gc, 0, 0,
						   attrs.width, attrs.height);
	XCopyArea(gui.dpy, map, *sen, mask_gc, 0, 0,
					     attrs.width, attrs.height, 0, 0);

	/* Create the "insensitive" pixmap.  It's a copy of the "sensitive"
	 * pixmap with half the pixels set to the background color. */
	*insen = XCreatePixmap(gui.dpy, rootWindow,
		 attrs.width, attrs.height, DefaultDepth(gui.dpy, screenNum));
	XCopyArea(gui.dpy, *sen, *insen, back_gc, 0, 0,
					     attrs.width, attrs.height, 0, 0);
	for (y = 0; y < attrs.height; y++)
	{
	    if (y % 2 == 0)
		startX = 0;
	    else
		startX = 1;
	    for (x = startX; x < attrs.width; x += 2)
		XDrawPoint(gui.dpy, *insen, back_gc, x, y);
	}
	XFreeGC(gui.dpy, back_gc);
	XFreeGC(gui.dpy, mask_gc);
	XFreePixmap(gui.dpy, map);
	/* XFreePixmap(gui.dpy, mask); causes a crash, probably XFreeGC
	 * already freed it. */
    }
    else
	*insen = *sen = 0;

    XtFree((char *)attrs.colorsymbols);
    XpmFreeAttributes(&attrs);
}
#endif
