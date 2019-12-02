\ Test program for MQTT interface

anew mqtt-test-marker 

include t_file.fth

create topic-buffer 2048 allot
create message-buffer 2048 allot

variable buffer-ptr
: .buffer ( buffer -- )
  dup @ swap cell+ swap type
;

: connect
  S" nuc1" 
  S" hubitat/#" 
  MQTT-CONNECT if
  	." connected!" cr
	0sp
  then
  2 sleep
   false S" on" S" hubitat/Ceiling Fan Light/set/switch" mqtt-publish
  begin
  	2048 message-buffer topic-buffer mqtt-read
	if
\     topic-buffer @ . ." "
\     topic-buffer dup @ 32 + dump cr
\     topic-buffer dup @ swap cell+ swap type cr
\     message-buffer 2048 dump

\     ." topic '" topic-buffer .buffer ." ' ===> "  
\     ." '" message-buffer .buffer ." '" 
\     cr
	then
  10 usleep 
  again
;

false mqtt-debug \ verbosity
connect
