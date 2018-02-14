#
# This is very much a work in progress to show how we can use macros to make the 
# GDB interface a lot more useable.  For example the next / step commands only 
# work if the stepper doesn't leave the current scope.  Beyond that you have a 
# single hardware breakpoint which can be used as an hb or a wa.  You have to 
# remember to delete the previous one, so the br macro does this for you. 
#
set confirm off
set print null-stop
define br 
  d
  hb $arg0
end
set pagination off

define prTV
  if $arg0
    set $type = $arg0.tt
    set $val  = $arg0.value

    if $type == 0
      # NIL
      printf "Nil\n"
    end
    if $type == 1
       # Boolean
       printf "Boolean: %u\n", $val->n
    end
    if $type == 2
       # ROTable
       printf "ROTable: %p\n", $val->p
    end
    if $type == 3
        # Light Function
       printf "Light Func: %p\n",p $arg $val->p
    end
    if $type == 4
       # Light User Data
       printf "Light Udata: %p\n", $val->pend
    end
    if $type == 5
       # Number
       printf "Boolean: %u\n", $val->n
    end
    if $type == 6
       # TString
      set $o = &($val.gc->ts.tsv)
      printf "Common header: next = %p, marked = 0x%01x\n", $o->next, $o->marked
      printf "String: hash = 0x%08x, len = %u : %s\n", $o->hash, $o->len, (char *)($ts+1)
    end
    if $type == 7
      # Table
      __printComHdr $arg0
      set $ts = (TString *)$val->p
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

set history save on
set history size 1000

