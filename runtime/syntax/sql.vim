" Vim syntax file
" Language:	SQL, PL/SQL (Oracle 8i)
" Maintainer:	Paul Moore <gustav@morpheus.demon.co.uk>
" Last Change:	2001 Jan 06

" Remove any old syntax stuff hanging around
syn clear

syn case ignore

" The SQL reserved words, defined as keywords.

syn keyword sqlSpecial  false null true

syn keyword sqlKeyword	access add as asc begin by check cluster column
syn keyword sqlKeyword	compress connect current cursor decimal default desc
syn keyword sqlKeyword	else elsif end exception exclusive file for from
syn keyword sqlKeyword	function group having identified if immediate increment
syn keyword sqlKeyword	index initial into is level loop maxextents mode modify
syn keyword sqlKeyword	nocompress nowait of offline on online start
syn keyword sqlKeyword	successful synonym table then to trigger uid
syn keyword sqlKeyword	unique user validate values view whenever
syn keyword sqlKeyword	where with option order pctfree privileges procedure
syn keyword sqlKeyword	public resource return row rowlabel rownum rows
syn keyword sqlKeyword	session share size smallint type using

syn keyword sqlOperator	not and or
syn keyword sqlOperator	in any some all between exists
syn keyword sqlOperator	like escape
syn keyword sqlOperator union intersect minus
syn keyword sqlOperator prior distinct
syn keyword sqlOperator	sysdate out

syn keyword sqlStatement alter analyze audit comment commit create
syn keyword sqlStatement delete drop execute explain grant insert lock noaudit
syn keyword sqlStatement rename revoke rollback savepoint select set
syn keyword sqlStatement truncate update

syn keyword sqlType	boolean char character date float integer long
syn keyword sqlType	mlslabel number raw rowid varchar varchar2 varray

" Strings and characters:
syn region sqlString		start=+"+  skip=+\\\\\|\\"+  end=+"+
syn region sqlString		start=+'+  skip=+\\\\\|\\'+  end=+'+

" Numbers:
syn match sqlNumber		"-\=\<\d*\.\=[0-9_]\>"

" Comments:
syn region sqlComment    start="/\*"  end="\*/"
syn match sqlComment	"--.*"

syn sync ccomment sqlComment

" The default highlighting.
hi def link sqlComment		Comment
hi def link sqlKeyword		sqlSpecial
hi def link sqlNumber		Number
hi def link sqlOperator		sqlStatement
hi def link sqlSpecial		Special
hi def link sqlStatement	Statement
hi def link sqlString		String
hi def link sqlType		Type

let b:current_syntax = "sql"

" vim: ts=8
