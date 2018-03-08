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
set pagination off

define prTS
  set $o = &(((TString *)($arg0))->tsv)
  printf "Common header: next = %p, marked = 0x%01x\n", $o->next, $o->marked
  printf "String: hash = 0x%08x, len = %u : %s\n", $o->hash, $o->len, (char *)(&$o[1])
end
define prTnodes
  set $o = (Table *)($arg0)
  set $n = 1<<($o->lsizenode)
  set $i = 0
  while $i < $n
    set $nd = ($o->node) + $i
    if $nd->i_key.nk.tt && $nd->i_val.tt
      if $nd->i_key.nk.tt == 6
        printf "%4u: %s  %2i\n", $i, $nd->i_key.nk.tt , $nd->i_val.tt      else
        printf "%4u: %2i  %2i\n", $i, $nd->i_key.nk.tt , $nd->i_val.tt
      end
    end
    set $i = $i +1
  end
end
define prTV
  if $arg0
    set $type = ($arg0).tt
    set $val  = ($arg0).value

    if $type == 0
      # NIL
      printf "Nil\n"
    end
    if $type == 1
       # Boolean
       printf "Boolean: %u\n", $val.n
    end
    if $type == 2
       # ROTable
       printf "ROTable: %p\n", $val.p
    end
    if $type == 3
        # Light Function
       printf "Light Func: %p\n", $val.p
    end
    if $type == 4
       # Light User Data
       printf "Light Udata: %p\n", $val.p
    end
    if $type == 5
       # Number
       printf "Number: %u\n", $val.n
    end
    if $type == 6
      prTS $arg0
    end
    if $type == 7
      # Table
      set $o = &($val->gc.h)
      printf "Common header: next = %p, marked = 0x%01x\n", $o->next, $o->marked
      printf "Nodes: %4i %p\n", 2<<($o->lsizenode), $o->nodes
      printf "Arry:  %4i %p\n", $o->sizearray, $o->array
    end
    if $type == 8
      # Function
      set $o = &($val->gc.cl.c)
      printf "Common header: next = %p, marked = 0x%01x\n", $o->next, $o->marked
       if $o->isC == 0
        set $o = &($val->gc.cl.l)
        printf "LClosure: nupvalues = %u, gclist = %p, env = %p, p = %p\n", $o->nupvalues, $o->gclist, $o->env, $o->p
      else
        printf "CClosure: nupvalues = %u, gclist = %p, env = %p, f = %p\np", $o->nupvalues, $o->gclist, $o->env, $o->f
      end
    end
    if $type == 9
      # UserData
    end
    if $type == 10
      # Thread
    end
  end
end

define prL
  if L > 0
    printf " stack:   %u\n", L->top-L->base
    printf " hooking: %u, %u, %u, %u, %p\n",  L->hookmask, L->allowhook, L->basehookcount, L->hookcount, L->hook
  end
end

define dumpstrt
  set $st = $arg0
  set $i = 0
  while $i< $st->size
    set $o = &(((TString *)($st->hash[$i]))->tsv)
    while $o
      if $o->next
        printf "Slot: %5i   %p  %p  %08x  %02x  %4u", $i, $o, $o->next, $o->hash, $o->marked, $o->len
      else
        printf "Slot: %5i   %p              %08x  %02x  %4u", $i, $o, $o->hash, $o->marked, $o->len
      end
      if $o->marked & 0x80
        printf "* %s\n", *(char **)($o+1)
      else
        printf "  %s\n", (char *)($o+1)
      end
      set $o = &(((TString *)($o->next))->tsv)
    end
    set $i = $i + 1
  end
end

define dumpRAMstrt
  dumpstrt &(L->l_G->strt)
end

define dumpROstrt
  dumpstrt &(L->l_G->ROstrt)
end

define graylist
  set $n = $arg0
  while  $n
    printf "%p %2u %02X\n",$n, $n->gch.tt, $n->gch.marked
    set $n=$n->gch.next
  end
end

define prPC
 printf "Excuting instruction %i: %08x\n", (pc - cl->p->code)+1-1, i
end

define prT
 print *(Table*)($arg0)
end
set history save on
set history size 1000

