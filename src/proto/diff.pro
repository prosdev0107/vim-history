/* diff.c */
void diff_new_buffer __ARGS((void));
void diff_buf_delete __ARGS((buf_T *buf));
void diff_buf_add __ARGS((buf_T *buf));
void diff_invalidate __ARGS((void));
void diff_mark_adjust __ARGS((linenr_T line1, linenr_T line2, long amount, long amount_after));
void diff_redraw __ARGS((void));
void ex_diffupdate __ARGS((exarg_T *eap));
void ex_diffpatch __ARGS((exarg_T *eap));
void ex_diffsplit __ARGS((exarg_T *eap));
void diff_win_options __ARGS((win_T *wp, int addbuf));
int diff_check __ARGS((win_T *wp, linenr_T lnum));
int diff_check_fill __ARGS((win_T *wp, linenr_T lnum));
void diff_set_topline __ARGS((win_T *fromwin, win_T *towin));
int diffopt_changed __ARGS((void));
void diff_find_change __ARGS((win_T *wp, linenr_T lnum, int *startp, int *endp));
int diff_infold __ARGS((win_T *wp, linenr_T lnum));
void ex_diffgetput __ARGS((exarg_T *eap));
int diff_mode_buf __ARGS((buf_T *buf));
/* vim: set ft=c : */