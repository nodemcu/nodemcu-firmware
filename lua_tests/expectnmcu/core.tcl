namespace eval expectnmcu::core {
  set panicre "powered by Lua \[0-9.\]+ on SDK \[0-9.\]+"
}

# Establish a serial connection to the device via socat
proc ::expectnmcu::core::connect { dev baud } {
  spawn "socat" "STDIO" "${dev},b${baud},rawer,crnl"
  close -onexec 1 -i ${spawn_id}
  return ${spawn_id}
}

# Use DTR/RTS signaling to reboot the device
## I'm not sure why we have to keep resetting the mode, but so it goes.
proc ::expectnmcu::core::reboot { dev baud } {
  set victimfd [open "${dev}"]
  fconfigure ${victimfd} -mode ${baud},n,8,1 -ttycontrol {DTR 0 RTS 1}
  sleep 0.1
  fconfigure ${victimfd} -mode ${baud},n,8,1 -ttycontrol {DTR 0 RTS 0}
  close ${victimfd}
}

proc ::expectnmcu::core::waitboot { victim } {
  expect {
    -i ${victim} "Formatting file system" {
      set timeout 60
      exp_continue
    }
    -i ${victim} "powered by Lua" { }
    timeout { return -code error "Timeout" }
  }
  # Catch nwf's system bootup, in case we're testing an existing system,
  # rather than a blank firmware.
  expect {
    -i ${victim} -re "Reset delay!\[^\n]*\n> " {
      send -i ${victim} "stop()\n"
      expect -i ${victim} "> "
    }
    -i ${victim} "> " { }
    timeout { return -code error "Timeout" }
  }
}

proc ::expectnmcu::core::send_exp_prompt { sid cmd } {
  send -i ${sid} -- "${cmd}\n"
  expect {
   -i ${sid} -ex "\n> " { }
   -i ${sid} -re ${::expectnmcu::core::panicre} { return -code error "Panic!" }
   timeout { return -code error "Timeout" }
  }
}

proc ::expectnmcu::core::send_exp_res_prompt { sid cmd res } {
  send -i ${sid} -- "${cmd}\n"
  expect {
   -i ${sid} -re "${res}.*\n> " { }
   -i ${sid} -re ${::expectnmcu::core::panicre} { return -code error "Panic!" }
   -i ${sid} -re "\n> " { return -code error "Prompt before expected response" }
   timeout { return -code error "Timeout" }
  }
}

proc ::expectnmcu::core::send_exp_prompt_c { sid cmd } {
  send -i ${sid} -- "${cmd}\n"
  expect {
   -i ${sid} -ex "\n>> " { }
   -i ${sid} -re ${::expectnmcu::core::panicre} { return -code error "Panic!" }
   timeout { return -code error "Timeout" }
  }
}

package provide expectnmcu::core 1.0
