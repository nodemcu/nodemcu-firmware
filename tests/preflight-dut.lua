print("\nTAP: 1..1\n")

-- On-DUT configuration of RCRs.
-- XXX NOT YET
-- node.startup{command = '!_init'}

-- Do this in the future so the prompt from dofile gets emitted
tmr.create():alarm(1, tmr.ALARM_SINGLE, function()
  print("\nTAP: ok 1 # preflight-dut completed\n")
end)
