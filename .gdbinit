#
# This is very much a work in progress to show how we can use macros to make the 
# GDB interface a lot more useable.  For example the next / step commands only 
# work if the stepper doesn't leave the current scope.  Beyond that you have a 
# single hardware breakpoint which can be used as an hb or a wa.  You have to 
# remember to delete the previous one, so the br macro does this for you. 
#
file app/.output/eagle/debug/image/eagle.app.v6.out
#set remotedebug 1
set remotelogfile gdb_rsp_logfile.txt
set serial baud 115200
set remote hardware-breakpoint-limit 1
set remote hardware-watchpoint-limit 1
#set debug xtensa 4
target remote /dev/ttyUSB0

set confirm off
set print null-stop
define br 
  d
  hb $arg0
end

define upto 
  d
  hb $arg0
  c
end

set pagination off
set history filename ~/.gdb_history
set history save on
set history size 1000

