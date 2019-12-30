-- ****************************************************************************
-- Play file with pcm module.
--
-- Upload jump_8k.u8 to spiffs before running this script.
--
-- ****************************************************************************


local function cb_drained()
  print("drained "..node.heap())

  file.seek("set", 0)
  -- uncomment the following line for continuous playback
  --d:play(pcm.RATE_8K)
end

local function cb_stopped()
  print("playback stopped")
  file.seek("set", 0)
end

local function cb_paused()
  print("playback paused")
end

do
  file.open("jump_8k.u8", "r")

  local drv = pcm.new(pcm.SD, 1)

  -- fetch data in chunks of FILE_READ_CHUNK (1024) from file
  drv:on("data", function(driver) return file.read() end) -- luacheck: no unused

  -- get called back when all samples were read from the file
  drv:on("drained", cb_drained)

  drv:on("stopped", cb_stopped)
  drv:on("paused", cb_paused)

  -- start playback
  drv:play(pcm.RATE_8K)
end
