local N = require('NTest')("file")

local function cleanup() 
    file.remove("testfile")
    file.remove("testfile2")
    local testfiles = {"testfile1&", "testFILE2"}
    for _, n in ipairs(testfiles) do
      file.remove(n,n)
    end
end

local function buildfn(fn, ...)
  params = {...}
  local fnp = function() fn(unpack(params)) end
  return fnp
end

N.test('exist', function()
  cleanup()
  nok(file.exists("non existing file"), "non existing file")

  file.putcontents("testfile", "testcontents")
  ok(file.exists("testfile"), "existing file")
end)


N.test('fscfg', function()
  cleanup()
  local start, size = file.fscfg()
  ok(start, "start")
  ok(size, "size")
end)

N.test('fsinfo', function()
  cleanup()
  local remaining, used, total = file.fsinfo()
  ok(remaining, "remaining")
  ok(used, "used")
  ok(total, "total")
  ok(eq(remaining+used, total), "size maths")
end)

N.test('getcontents', function()
  cleanup()
  local testcontent = "some content \0 and more"
  file.putcontents("testfile", testcontent)
  local content = file.getcontents("testfile")
  ok(eq(testcontent, content),"contents")
end)

N.test('getcontents non existent file', function()
  cleanup()
  nok(file.getcontents("non existing file"), "non existent file")
end)

N.test('getcontents more than 1K', function()
  cleanup()
  local f = file.open("testfile", "w")
  local i
  for i = 1,100 do
    f:write("some text to test")
  end
  f:close()
  content = file.getcontents("testfile")
  ok(eq(#content, 1700), "long contents")
end)

N.test('read more than 1K', function()
  cleanup()
  local f = file.open("testfile", "w")
  local i
  for i = 1,100 do
    f:write("some text to test")
  end
  f:close()
  f = file.open("testfile","r")
  local buffer = f:read()
  ok(eq(#buffer, 1024), "first block")
  buffer = f:read()
  f:close()
  ok(eq(#buffer, 1700-1024), "second block")  
end)

function makefile(name, num128)
  local s128 = "16 bytes written"
  s128 = s128..s128..s128..s128
  s128 = s128..s128
  local f = file.open(name, "w")
  for i = 1, num128 do
    f:write(s128)
  end
  f:close()
end

N.test('read 7*128 bytes', function()
  cleanup()
  makefile("testfile", 7)
  f = file.open("testfile","r")
  local buffer = f:read()
  f:close()
  nok(eq(buffer, nil), "nil first block")
  ok(eq(#buffer, 128*7), "length first block")
end)

N.test('read 8*128 bytes long file', function()
  cleanup()
  makefile("testfile", 8)
  local f = file.open("testfile","r")
  local buffer = f:read()
  nok(eq(buffer, nil), "nil first block")
  ok(eq(#buffer, 128*8), "size first block")
  buffer = f:read()
  f:close()
  ok(eq(buffer, nil), "nil second block")
end)

N.test('read 9*128 bytes', function()
  cleanup()
  makefile("testfile", 9)
  f = file.open("testfile","r")
  local buffer = f:read()
  nok(eq(buffer, nil), "nil first block")
  ok(eq(#buffer, 1024), "size first block")
  buffer = f:read()
  f:close()
  nok(eq(buffer, nil), "nil second block")
  ok(eq(#buffer, 128*8-1024), "size second block")
end)

N.test('list', function()
  cleanup()
  local files

  local function count(files)
    local filecount = 0
    for k,v in pairs(files) do filecount = filecount+1 end
    return filecount
  end

  local function testfile(name)
    ok(eq(files[name],#name), "length matches name length")
  end

  local testfiles = {"testfile1&", "testFILE2"}
  for _, n in ipairs(testfiles) do
    file.putcontents(n,n)
  end

  files = file.list("testfile%.*")
  ok(eq(count(files), 1), "found file")
  testfile("testfile1&")

  files = file.list("^%l*%u+%d%.-")
  ok(eq(count(files), 1), "found file regexp")
  testfile("testFILE2")

  files = file.list()
  ok(count(files) >= 2, "several files found")
end)

N.test('open non existing', function()
  cleanup()
  local function testopen(test, filename, mode)
    test(file.open(filename, mode), mode)
    file.close()
    file.remove(filename)
  end
  
  testopen(nok, "testfile", "r")
  testopen(ok, "testfile", "w")
  testopen(ok, "testfile", "a")
  testopen(nok, "testfile", "r+")
  testopen(ok, "testfile", "w+")
  testopen(ok, "testfile", "a+")

  fail(buildfn(file.open, "testfile"), "errormsg", "x")  -- shouldn't this fail?
end)

N.test('open existing', function()
  cleanup()
  local function testopen(mode, position)
    file.putcontents("testfile", "testcontent")
    ok(file.open("testfile", mode), mode)
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

  fail(buildfn(file.open, "testfile"), "errormsg", "x")  -- shouldn't this fail?
end)

N.test('remove', function()
  cleanup()
  file.putcontents("testfile", "testfile")

  ok(file.remove("testfile") == nil, "existing file")
  ok(file.remove("testfile") == nil, "non existing file")
end)

N.test('rename', function()
  cleanup()
  file.putcontents("testfile", "testfile")

  ok(file.rename("testfile", "testfile2"), "rename existing")
  nok(file.exists("testfile"), "old file removed")
  ok(file.exists("testfile2"), "new file exists")

  nok(file.rename("testfile", "testfile3"), "rename non existing")

  file.putcontents("testfile", "testfile")

  nok(file.rename("testfile", "testfile2"), "rename to existing")
  ok(file.exists("testfile"), "from file exists")
  ok(file.exists("testfile2"), "to file exists")
end)

N.test('stat existing file', function()
  cleanup()
  file.putcontents("testfile", "testfile")

  local stat = file.stat("testfile")
  ok(stat, "stat existing")
  ok(eq(stat.size, 8), "size")
  ok(eq(stat.name, "testfile"), "name")
  ok(stat.time, "no time")
  ok(eq(stat.time.year, 1970), "year")
  ok(eq(stat.time.mon, 01), "mon")
  ok(eq(stat.time.day, 01), "day")
  ok(eq(stat.time.hour, 0), "hour")
  ok(eq(stat.time.min, 0), "min")
  ok(eq(stat.time.sec, 0), "sec")
  nok(stat.is_dir, "is_dir")
  nok(stat.is_rdonly, "is_rdonly")
  nok(stat.is_hidden, "is_hidden")
  nok(stat.is_sys, "is_sys")
  nok(stat.is_arch, "is_arch")
end)

N.test('stat non existing file', function()
  cleanup()
  local stat = file.stat("not existing file")
  
  ok(stat == nil, "stat empty")
end)
