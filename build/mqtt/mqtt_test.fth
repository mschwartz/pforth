\ Test program for MQTT interface

anew mqtt-test-marker 

create topic-buffer 2048 allot
create message-buffer 2048 allot

: r-on s" on" ;
: r-off s" off" ;
: r-ceiling-fan-light-set s" hubitat/Ceiling Fan Light/set/switch" ;
: r-ceiling-fan-light-status s" hubitat/Ceiling Fan Light/status/switch" ;

variable buffer-ptr
: .buffer ( buffer -- )
  dup @ swap cell+ swap type
;

: connect
  S" nuc1" S" hubitat/#" MQTT-CONNECT if
  	." connected!" cr
    0sp
  then
  2 sleep
  false r-on r-ceiling-fan-light-set .s mqtt-publish
\   false S" on" S" hubitat/Ceiling Fan Light/set/switch" mqtt-publish
  begin
    2048 message-buffer topic-buffer mqtt-read
    if
      ." topic '" topic-buffer .buffer ." ' ===> "  
      ." '" message-buffer .buffer ." '" 
      cr
    then
    10 usleep 
  again
;

\ main program
connect
