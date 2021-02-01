namespace eval expectnmcu::xfer {
}

package require expectnmcu::core

# Open remote file `which` on `dev` in `mode` as Lua object `dfh`
proc ::expectnmcu::xfer::open { dev dfh which mode } {
  ::expectnmcu::core::send_exp_prompt ${dev} "${dfh} = nil"
  ::expectnmcu::core::send_exp_prompt ${dev} "${dfh} = file.open(\"${which}\",\"${mode}\")"
  ::expectnmcu::core::send_exp_res_prompt ${dev} "=type(${dfh})" "userdata"
}

# Close Lua file object `dfh` on `dev`
proc ::expectnmcu::xfer::close { dev dfh } {
  ::expectnmcu::core::send_exp_prompt ${dev} "${dfh}:close()"
}

# Write to `dfh` on `dev` at `where` `what`, using base64 as transport
#
# This does not split lines; write only short amounts of data.
proc ::expectnmcu::xfer::pwrite { dev dfh where what } {
  send -i ${dev} -- [string cat \
    "do local d,e = encoder.fromBase64(\"[binary encode base64 -maxlen 0 ${what}]\");" \
    "${dfh}:seek(\"set\",${where});" \
    "print(${dfh}:write(d));" \
    "end\n" \
  ]
  expect {
    -i ${dev} -re "true\[\r\n\]+> " { }
    -i ${dev} -re ${::expectnmcu::core::panicre} { return -code error "Panic!" }
    -i ${dev} -ex "\n> " { return -code error "Bad result from pwrite" }
    timeout { return -code error "Timeout while waiting for pwrite" }
  }
}

# Read `howmuch` byetes from `dfh` on `dev` at `where`, using base64
# as transport.  This buffers the whole data and its base64 encoding
# in device RAM; read only short strings.
proc ::expectnmcu::xfer::pread { dev dfh where howmuch } {
  send -i ${dev} -- "${dfh}:seek(\"set\",${where}); print(encoder.toBase64(${dfh}:read(${howmuch})))\n"
  expect {
    -i ${dev} -re "\\)\\)\\)\[\r\n\]+(\[^\r\n\]+)\[\r\n\]+> " {
      return [binary decode base64 ${expect_out(1,string)}]
    }
    -i ${dev} -ex "\n> " { return -code error "No reply to pread" }
    -i ${dev} -re ${::expectnmcu::core::panicre} { return -code error "Panic!" }
    timeout { return -code error "Timeout while pread-ing" }
  }
}

# Check for pipeutils on the target device
proc ::expectnmcu::xfer::haspipeutils { dev } {
  send -i ${dev} -- "local ok, pu = pcall(require, \"pipeutils\"); print(ok and type(pu) == \"table\" and pu.chunker and pu.debase64 and true or false)\n"
  expect {
    -i ${dev} -re "\[\r\n\]+false\[\r\n\]+> " { return 0 }
    -i ${dev} -re "\[\r\n\]+true\[\r\n\]+> " { return 1 }
    -i ${dev} -ex "\n> " { return -code error "No reply to pipeutils probe" }
    -i ${dev} -re ${::expectnmcu::core::panicre} { return -code error "Panic!" }
    timeout { return -code error "Timeout while probing for pipeutils" }
  }
}

# Send local file `lfn` to the remote filesystem on `dev` and name it `rfn`.
# Use `dfo` as the Lua handle to the remote file for the duration of writing,
# (and `nil` it out afterwards)
proc ::expectnmcu::xfer::sendfile { dev lfn rfn {dfo "xfo"} } {
  package require sha256

  set has_pipeutils [::expectnmcu::xfer::haspipeutils ${dev} ]

  set ltf [::open ${lfn} ]
  fconfigure ${ltf} -translation binary
  file stat ${lfn} lfstat
  ::expectnmcu::xfer::open ${dev} ${dfo} "${rfn}.sf" "w+"

  if { ${has_pipeutils} } {
    # Send over a loader program
    ::expectnmcu::core::send_exp_prompt_c ${dev} "do"
    ::expectnmcu::core::send_exp_prompt_c ${dev} " local pu = require \"pipeutils\""
    ::expectnmcu::core::send_exp_prompt_c ${dev} " local ch = pu.chunker(function(d) ${dfo}:write(d) end, 256)"
    ::expectnmcu::core::send_exp_prompt_c ${dev} " local db = pu.debase64(ch.write, function(ed,ee)"
    ::expectnmcu::core::send_exp_prompt_c ${dev} "  if ed:match(\"^%.\[\\r\\n\]*$\") then ch.flush() print(\"F I N\")"
    ::expectnmcu::core::send_exp_prompt_c ${dev} "  else print(\"ABORT\", ee, ed) end"
    ::expectnmcu::core::send_exp_prompt_c ${dev} "  uart.on(\"data\") end)"
    # TODO: make echo use CRC not full string; probably best add to crypto module
    ::expectnmcu::core::send_exp_prompt_c ${dev} " uart.on(\"data\", \"\\n\", function(x) db.write(x); uart.write(0, \"OK: \", x) end, 0)"
    ::expectnmcu::core::send_exp_prompt ${dev}   "end"
    set xln 90
  } else {
    set xln 48
  }

  set lho [sha2::SHA256Init]

  set fpos 0
  while { 1 } {
    send_user ">> xfer ${fpos} of ${lfstat(size)}\n"
    set data [read ${ltf} ${xln}]
    sha2::SHA256Update ${lho} ${data}
    if { ${has_pipeutils} } {
      set estr [binary encode base64 -maxlen 0 ${data}]
      send -i ${dev} -- "${estr}\n"
      expect {
        -i ${dev} -ex "OK: ${estr}" { expect -i ${dev} -re "\[\r\n\]+" {} }
        -i ${dev} -ex "\n> " { return -code error "Prompt while sending data" }
        -i ${dev} -re ${::expectnmcu::core::panicre} { return -code error "Panic!" }
        timeout { return -code error "Timeout while sending data" }
      }
    } else {
      ::expectnmcu::xfer::pwrite ${dev} ${dfo} ${fpos} ${data}
    }
    set fpos [expr $fpos + ${xln}]
    if { [string length ${data}] != ${xln} } { break }
  }

  if { ${has_pipeutils} } {
    send -i ${dev} -- ".\n"
    expect {
      -i ${dev} -re "F I N\[\r\n\]+" { }
      -i ${dev} -ex "\n> " { return -code error "Prompt while awaiting acknowledgement" }
      -i ${dev} -re ${::expectnmcu::core::panicre} { return -code error "Panic!" }
      timeout { return -code error "Timeout while awaiting acknowledgement" }
    }
  }

  ::close ${ltf}
  ::expectnmcu::xfer::close ${dev} ${dfo}
  ::expectnmcu::core::send_exp_prompt ${dev} "${dfo} = nil"

  set exphash [sha2::Hex [sha2::SHA256Final ${lho}]]

  send -i ${dev} "=encoder.toHex(crypto.fhash(\"sha256\",\"${rfn}.sf\"))\n"
  expect {
    -i ${dev} -re "\[\r\n\]+(\[a-f0-9\]+)\[\r\n\]+> " {
      if { ${expect_out(1,string)} != ${exphash} } {
        return -code error \
          "Sendfile checksum mismatch: ${expect_out(1,string)} != ${exphash}"
      }
    }
    -i ${dev} -re ${::expectnmcu::core::panicre} { return -code error "Panic!" }
    timeout { return -code error "Timeout while verifying checksum" }
  }

  ::expectnmcu::core::send_exp_prompt ${dev} "file.remove(\"${rfn}\")"
  ::expectnmcu::core::send_exp_res_prompt ${dev} "=file.rename(\"${rfn}.sf\", \"${rfn}\")" "true"
}

package provide expectnmcu::xfer 1.0
