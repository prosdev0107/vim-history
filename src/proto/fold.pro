/* fold.c */
void copyFoldingState __ARGS((win_t *wp_from, win_t *wp_to));
int hasAnyFolding __ARGS((win_t *win));
int hasFolding __ARGS((linenr_t lnum, linenr_t *firstp, linenr_t *lastp));
int hasFoldingWin __ARGS((win_t *win, linenr_t lnum, linenr_t *firstp, linenr_t *lastp, int cache, int *levelp));
int lineFolded __ARGS((win_t *win, linenr_t lnum));
long foldedCount __ARGS((win_t *win, linenr_t lnum, int *levelp));
int foldmethodIsManual __ARGS((win_t *wp));
int foldmethodIsIndent __ARGS((win_t *wp));
int foldmethodIsExpr __ARGS((win_t *wp));
int foldmethodIsMarker __ARGS((win_t *wp));
int foldmethodIsSyntax __ARGS((win_t *wp));
void closeFold __ARGS((linenr_t lnum));
void closeFoldRecurse __ARGS((linenr_t lnum));
void opFoldRange __ARGS((linenr_t first, linenr_t last, int opening, int recurse));
void openFold __ARGS((linenr_t lnum));
void openFoldRecurse __ARGS((linenr_t lnum));
void foldOpen __ARGS((linenr_t line1, linenr_t line2));
void newFoldLevel __ARGS((void));
void foldCreate __ARGS((linenr_t start, linenr_t end));
void deleteFold __ARGS((linenr_t lnum));
void clearFolding __ARGS((win_t *win));
void foldUpdate __ARGS((win_t *wp, linenr_t top, linenr_t bot));
void foldUpdateAll __ARGS((win_t *win));
void foldInitWin __ARGS((win_t *newwin));
int find_wl_entry __ARGS((win_t *win, linenr_t lnum));
void foldAdjustVisual __ARGS((void));
void foldMarkAdjust __ARGS((win_t *wp, linenr_t line1, linenr_t line2, long amount, long amount_after));
int getDeepestNesting __ARGS((void));
int put_folds __ARGS((FILE *fd, win_t *wp));
/* vim: set ft=c : */
