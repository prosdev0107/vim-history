" Vim syntax support file
" Maintainer:	Bram Moolenaar <Bram@vim.org>
" Last Change:	2000 Nov 17

" This file is used for ":syntax on".
" It installs the autocommands and starts highlighting for all buffers.

if !has("syntax")
  finish
endif

" If Syntax highlighting appears to be on already, turn it off first, so that
" any leftovers are cleared.
if exists("syntax_on") || exists("syntax_manual")
  so <sfile>:p:h/nosyntax.vim
endif

" Load the Syntax autocommands and set the default methods for highlighting.
so <sfile>:p:h/synload.vim

" Load the FileType autocommands if not done yet.
if exists("did_load_filetypes")
  let s:did_ft = 1
else
  filetype on
  let s:did_ft = 0
endif

" Set up the connection between FileType and Syntax autocommands.
" This makes the syntax automatically set when the file type is detected.
augroup syntaxset
  au! FileType *	exe "set syntax=" . expand("<amatch>")
augroup END


" Execute the syntax autocommands for the each buffer.
" If the filetype wasn't detected yet, do that now.
if s:did_ft
  doautoall syntaxset FileType
else
  doautoall filetypedetect BufRead
endif
