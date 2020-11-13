-- tests for file_lfs module
-- resource.lua with index.html or favicon.ico or test.txt file needs to be embedded in LFS
-- the file should be larger than 1kB

-- luacheck: globals file
-- luacheck: new read globals node.LFS.resource
file = require("file_lfs")

-- check standard SPIFFS file functions
require("NTest_file")

local N = require('NTest')("file_lfs")

N.test('resource.lua in LFS', function()
  ok(node.LFS.resource~=nil, "resource.lua embedded in LFS")
end)

local testfile = "index.html"
if node.LFS.resource("index.html") then
  testfile = "index.html"
elseif node.LFS.resource("favicon.ico") then
  testfile = "favicon.ico"
elseif node.LFS.resource("test.txt") then
  testfile = "test.txt"
end

N.test('exist file LFS', function()
  ok(file.exists(testfile), "existing file")
end)

N.test('getcontents file LFS', function()
  local testcontent = node.LFS.resource(testfile)
  local content = file.getcontents(testfile)
  ok(eq(testcontent, content),"contents")
end)

N.test('read more than 1K file LFS', function()
  local f = file.open(testfile,"r")
  local size = #node.LFS.resource(testfile)
  local buffer = f:read()
  print(#buffer)
  ok(eq(#buffer < 1024 and size or 1024, 1024), "first block")
  buffer = f:read()
  f:close()
  ok(eq(#buffer, size-1024 > 1024 and 1024 or size-1024), "second block")
end)

N.test('open existing file LFS', function()
  file.remove(testfile)

  local function testopen(mode, position)
    file.putcontents(testfile, "testcontent")
    ok(file.open(testfile, mode), mode)
    file.write("")
    ok(eq(file.seek(), position), "seek check")
    file.close()
  end

  testopen("r", 0)
  testopen("w", 0)
  testopen("a", 11)
  testopen("r+", 0)
  testopen("w+", 0)
  testopen("a+", 11)
  file.remove(testfile)
end)

N.test('seek file LFS', function()

  local testcontent = node.LFS.resource(testfile)
  local content

  local f = file.open(testfile)
  f:seek("set", 0)
  content = f:read(10)
  ok(eq(testcontent:sub(1,10), content),"set 0")

  f:seek("set", 99)
  content = f:read(10)
  ok(eq(testcontent:sub(100,109), content),"set 100")

  f:seek("cur", 10)
  content = f:read(10)
  ok(eq(testcontent:sub(120,129), content),"cur 10")

  f:seek("cur", -50)
  content = f:read(10)
  ok(eq(testcontent:sub(80,89), content),"cur -50")

  f:seek("end", -50)
  content = f:read(10)
  ok(eq(testcontent:sub(#testcontent-50,#testcontent-41), content),"end -50")
end)


N.test('remove file LFS', function()
  file.remove(testfile)
  file.putcontents(testfile, "testfile")

  ok(file.remove(testfile) == nil, "existing file")
  ok(file.remove(testfile) == nil, "non existing file")
end)

N.test('rename file LFS', function()
  file.remove(testfile)
  file.remove("testfile")

  file.putcontents("testfile", "testfile")

  ok(file.rename("testfile", testfile), "rename existing")
  nok(file.exists("testfile"), "old file removed")
  ok(file.exists(testfile), "new file exists")

  nok(file.rename("testfile", testfile), "rename non existing")

  file.putcontents("testfile", testfile)

  nok(file.rename("testfile", testfile), "rename to existing")
  ok(file.exists("testfile"), "from file exists")
  ok(file.exists(testfile), "to file exists")

  file.remove(testfile)
  file.remove("testfile")
end)
