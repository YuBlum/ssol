" Vim syntax file
" Language: Simple-SOL

" Usage Instructions
" Put this file in .vim/syntax/ssol.vim
" and add in your .vimrc file the next line:
" autocmd BufRead,BufNewFile *.ssol set filetype=ssol

if exists("b:current_syntax")
  finish
endif

syntax keyword ssolTodos TODO XXX FIXME NOTE

" Keywords
syntax keyword ssolKeywords if else loop do proc const var end import export

" Comments
syntax region ssolCommentLine start="//" end="$"   contains=ssolTodos

" Strings
syntax region ssolString start=/\v"/ skip=/\v\\./ end=/\v"/
syntax region ssolString start=/\v'/ skip=/\v\\./ end=/\v'/

" Set highlights
highlight default link ssolTodos Todo
highlight default link ssolKeywords Keyword
highlight default link ssolCommentLine Comment
    highlight default link ssolString String

let b:current_syntax = "ssol"
