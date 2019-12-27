anew lib-include-task

private{
create include.filename  2048 allot
}private

: SMART-INCLUDED ( c-addr u -- )
." SMART_INCLUDED "
	2dup type cr
	\ Print messages.
        trace-include @
        IF
                >newline ." Include " 2dup type cr
        THEN
        here >r
        2dup r/o open-file
        IF  ( -- c-addr u bad-fid )
		drop drop drop
		false
        ELSE ( -- c-addr u good-fid )
		-rot include.mark.start
                depth >r
                include-file    \ will also close the file
                depth 1+ r> -
                IF
                        ." Warning: stack depth changed during include!" cr
                        .s cr
                        0sp
                THEN
                include.mark.end
        trace-include @
        IF
                ."     include added " here r@ - . ." bytes,"
                codelimit here - . ." left." cr
        THEN
        rdrop
	true
        THEN
	cr .s cr
;

private{
variable smart-include-count
variable smart-include-name
}private

: INCLUDE ( c_addr u -- )
     smart-include-count !  smart-include-name !

     smart-include-name @ smart-include-count @
     include.filename  place
     include.filename count smart-included
     cr .s cr
      if
	     include.filename count type
	      ."  loaded" cr
      	exit
      then

\       smart-include-name @
\       smart-include-count @
\       type ." not found" cr

     S" /usr/local/pforth/" include.filename  place
     smart-include-name @ smart-include-count @ 
     include.filename $APPEND
\      include.filename cr count type
     include.filename count smart-included
     not if
    	smart-include-name @
	smart-include-count @
	type ." not found" cr
	abort
     then
;

