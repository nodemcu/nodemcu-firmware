namespace eval expectnmcu::core {
  set panicre "powered by Lua \[0-9.\]+ on SDK \[0-9.\]+"
  set promptstr "\n> "

  namespace export reboot waitboot connect
  namespace export send_exp_prompt send_exp_res_prompt send_exp_prompt_c
}

package require cmdline

# Use DTR/RTS signaling to reboot the device
## I'm not sure why we have to keep resetting the mode, but so it goes.
proc ::expectnmcu::core::reboot { dev } {
  set victimfd [open ${dev} ]
  set mode [fconfigure ${victimfd} -mode ]
  fconfigure ${victimfd} -mode ${mode} -ttycontrol {DTR 0 RTS 1}
  sleep 0.1
  fconfigure ${victimfd} -mode ${mode} -ttycontrol {DTR 0 RTS 0}
  close ${victimfd}
}

proc ::expectnmcu::core::waitboot { victim } {
  expect {
    -i ${victim} "Formatting file system" {
      set timeout 120
      exp_continue
    }
    -i ${victim} "powered by Lua" { }
    timeout { return -code error "Timeout" }
  }
  # Catch nwf's system bootup, in case we're testing an existing system,
  # rather than a blank firmware.
  expect {
    -i ${victim} -re "Reset delay!.*${::expectnmcu::core::promptstr}" {
      send -i ${victim} "stop(true)\n"
      expect -i ${victim} -ex ${::expectnmcu::core::promptstr}
    }
    -i ${victim} -ex ${::expectnmcu::core::promptstr} { }
    timeout { return -code error "Timeout" }
  }

  # Do a little more active synchronization with the DUT: send it a command
  # and wait for the side-effect of that command to happen, thereby ensuring
  # that the next prompt we see is after this point in the input.
  send -i ${victim} "print(\"a\",\"z\")\n"
  expect {
    -i ${victim} -ex "a\tz" { }
  }
  expect {
    -i ${victim} -ex ${::expectnmcu::core::promptstr} { }
    timeout { return -code error "Timeout" }
  }
}

# Establish a serial connection to the device via socat.  Takes
# -baud=N, -reboot=0/1/dontwait, -waitboot=0/1 optional parameters
proc ::expectnmcu::core::connect { dev args } {
  set opts {
    { baud.arg 115200 }
    { reboot.arg 1 }
  }
  array set arg [::cmdline::getoptions args $opts]

  spawn "socat" "STDIO" "${dev},b${arg(baud)},raw,crnl"
  close -onexec 1 -i ${spawn_id}
  set victim ${spawn_id}

  # XXX?
  set victimfd [open ${dev} ]
  set mode [fconfigure ${victimfd} -mode ${arg(baud)},n,8,1 ]

  if { ${arg(reboot)} != 0 } {
    ::expectnmcu::core::reboot ${dev}
    if { ${arg(reboot)} != "dontwait" } {
      ::expectnmcu::core::waitboot ${victim}
    }
  }

  close ${victimfd}

  return ${victim}
}

# This one is somewhat "for experts only" -- it expects that you have either
# consumed whatever command you flung at the node or that you have some reason
# to not be concerned with its echo (and return)
proc ::expectnmcu::core::exp_prompt { sid } {
  expect {
   -i ${sid} -ex ${::expectnmcu::core::promptstr} { }
   -i ${sid} -re ${::expectnmcu::core::panicre} { return -code error "Panic!" }
   timeout { return -code error "Timeout" }
  }
}

proc ::expectnmcu::core::send_exp_prompt { sid cmd } {
  send -i ${sid} -- "${cmd}\n"
  expect {
   -i ${sid} -ex "${cmd}" { }
   timeout { return -code error "Timeout" }
  }
  expect {
   -i ${sid} -ex ${::expectnmcu::core::promptstr} { }
   -i ${sid} -re ${::expectnmcu::core::panicre} { return -code error "Panic!" }
   timeout { return -code error "Timeout" }
  }
}

proc ::expectnmcu::core::send_exp_res_prompt { sid cmd res } {
  send -i ${sid} -- "${cmd}\n"
  expect {
   -i ${sid} -ex "${cmd}" { }
   timeout { return -code error "Timeout" }
  }
  expect {
   -i ${sid} -re "${res}.*${::expectnmcu::core::promptstr}" { }
   -i ${sid} -re ${::expectnmcu::core::panicre} { return -code error "Panic!" }
   -i ${sid} -ex ${::expectnmcu::core::promptstr} { return -code error "Prompt before expected response" }
   timeout { return -code error "Timeout" }
  }
}

proc ::expectnmcu::core::send_exp_prompt_c { sid cmd } {
  send -i ${sid} -- "${cmd}\n"
  expect {
   -i ${sid} -ex "${cmd}" { }
   timeout { return -code error "Timeout" }
  }
  expect {
   -i ${sid} -ex "\n>> " { }
   -i ${sid} -ex ${::expectnmcu::core::promptstr} { return -code error "Non-continuation prompt" }
   -i ${sid} -re ${::expectnmcu::core::panicre} { return -code error "Panic!" }
   timeout { return -code error "Timeout" }
  }
}

package provide expectnmcu::core 1.0
