/* os_amiga.c */
void win_resize_on __ARGS((void));
void win_resize_off __ARGS((void));
void mch_write __ARGS((char_u *p, int len));
int mch_inchar __ARGS((char_u *buf, int maxlen, long time));
int mch_char_avail __ARGS((void));
long_u mch_avail_mem __ARGS((int special));
void mch_delay __ARGS((long msec, int ignoreinput));
void mch_suspend __ARGS((void));
void mch_shellinit __ARGS((void));
int mch_check_win __ARGS((int argc, char **argv));
int mch_input_isatty __ARGS((void));
void fname_case __ARGS((char_u *name));
void mch_settitle __ARGS((char_u *title, char_u *icon));
void mch_restore_title __ARGS((int which));
int mch_can_restore_title __ARGS((void));
int mch_can_restore_icon __ARGS((void));
int mch_get_user_name __ARGS((char_u *s, int len));
void mch_get_host_name __ARGS((char_u *s, int len));
long mch_get_pid __ARGS((void));
int mch_dirname __ARGS((char_u *buf, int len));
int mch_FullName __ARGS((char_u *fname, char_u *buf, int len, int force));
int mch_isFullName __ARGS((char_u *fname));
long mch_getperm __ARGS((char_u *name));
int mch_setperm __ARGS((char_u *name, long perm));
void mch_hide __ARGS((char_u *name));
int mch_isdir __ARGS((char_u *name));
void mch_mkdir __ARGS((char_u *name));
int mch_can_exe __ARGS((char_u *name));
int mch_nodetype __ARGS((char_u *name));
void mch_init __ARGS((void));
void mch_windexit __ARGS((int r));
void mch_settmode __ARGS((int tmode));
int mch_screenmode __ARGS((char_u *arg));
int mch_get_shellsize __ARGS((void));
void mch_set_shellsize __ARGS((void));
void mch_new_shellsize __ARGS((void));
int mch_call_shell __ARGS((char_u *cmd, int options));
void mch_breakcheck __ARGS((void));
long Chk_Abort __ARGS((void));
int mch_expandpath __ARGS((garray_t *gap, char_u *pat, int flags));
int mch_has_wildcard __ARGS((char_u *p));
char_u *mch_getenv __ARGS((char_u *var));
int mch_setenv __ARGS((char *var, char *value, int x));
/* vim: set ft=c : */
