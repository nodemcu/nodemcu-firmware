require 'mispec'

describe('file', function(it)

--    it:initialize(function() end)

    it:cleanup(function() 
        file.remove("testfile")
        file.remove("testfile2")
        local testfiles = {"testfile1&", "testFILE2"}
        for _, n in ipairs(testfiles) do
          file.remove(n,n)
        end
    end)

    it:should('exist', function()
        ko(file.exists("non existant file"), "non existant file")

        file.putcontents("testfile", "testcontents")
        ok(file.exists("testfile"))
    end)

    it:should('fscfg', function()
        local start, size = file.fscfg()
        ok(start, "start")
        ok(size, "size")
    end)

    it:should('fsinfo', function()
        local remaining, used, total = file.fsinfo()
        ok(remaining, "remaining")
        ok(used, "used")
        ok(total, "total")
        ok(eq(remaining+used, total), "size maths")
    end)

    it:should('getcontents', function()
        local testcontent = "some content \0 and more"
        file.putcontents("testfile", testcontent)
        local content = file.getcontents("testfile")
        ok(testcontent, content)
    end)

    it:should('getcontents non existent file', function()
        ko(file.getcontents("non existant file"), "non existent file")
    end)

    it:should('getcontents more than 1K', function()
        local f = file.open("testfile", "w")
        local i
        for i = 1,100 do
          f:write("some text to test")
        end
        f:close()
        content = file.getcontents("testfile")
        eq(#content, 1700, "partial read")
    end)

    it:should('list', function()
        local files

        local function count(files)
          local filecount = 0
          for k,v in pairs(files) do filecount = filecount+1 end
          return filecount
        end

        local function testfile(name)
          ok(eq(files[name],#name))
        end

        local testfiles = {"testfile1&", "testFILE2"}
        for _, n in ipairs(testfiles) do
          file.putcontents(n,n)
        end

        files = file.list("testfile%.*")
        ok(eq(count(files), 1))
        testfile("testfile1&")

        files = file.list("^%l*%u+%d%.-")
        ok(eq(count(files), 1))
        testfile("testFILE2")

        files = file.list()
        ok(count(files) >= 2)
    end)

    it:should('open non existing', function()
        local function testopen(outcome, filename, mode)
          outcome(file.open(filename, mode), mode)
          file.close()
          file.remove(filename)
        end
        
        testopen(ko, "testfile", "r")
        testopen(ok, "testfile", "w")
        testopen(ok, "testfile", "a")
        testopen(ko, "testfile", "r+")
        testopen(ok, "testfile", "w+")
        testopen(ok, "testfile", "a+")

        fail(file.open, "testfile", "x")  -- shouldn't this fail?
    end)

    it:should('open existing', function()
        local function testopen(mode, position)
          file.putcontents("testfile", "testfile")
          ok(file.open("testfile", mode), mode)
          file.write("")
          ok(eq(file.seek(), position))
          file.close()
        end

        testopen("r", 0)
        testopen("w", 0)
        testopen("a", 8)
        testopen("r+", 0)
        testopen("w+", 0)
        testopen("a+", 8)

        fail(file.open, "testfile", "x")  -- shouldn't this fail?
    end)

    it:should('remove', function()
        file.putcontents("testfile", "testfile")

        ok(file.remove("testfile") == nil, "existing file")
        ok(file.remove("testfile") == nil, "non existing file")
    end)

    it:should('rename', function()
        file.putcontents("testfile", "testfile")

        ok(file.rename("testfile", "testfile2"), "rename existing")
        ko(file.exists("testfile"), "old file removed")
        ok(file.exists("testfile2"), "new file exists")

        ko(file.rename("testfile", "testfile3"), "rename non existing")

        file.putcontents("testfile", "testfile")

        ko(file.rename("testfile", "testfile2"), "rename to existing")
        ok(file.exists("testfile"), "from file exists")
        ok(file.exists("testfile2"), "to file exists")
    end)

    it:should('stat existing file', function()
        file.putcontents("testfile", "testfile")

        local stat = file.stat("testfile")
        
        ko(stat == nil, "stat existing")
        ok(eq(stat.size, 8), "size")
        ok(eq(stat.name, "testfile"), "name")
        ko(stat.time == nil, "no time")
        ok(eq(stat.time.year, 1970), "year")
        ok(eq(stat.time.mon, 01), "mon")
        ok(eq(stat.time.day, 01), "day")
        ok(eq(stat.time.hour, 0), "hour")
        ok(eq(stat.time.min, 0), "min")
        ok(eq(stat.time.sec, 0), "sec")
        ko(stat.is_dir, "is_dir")
        ko(stat.is_rdonly, "is_rdonly")
        ko(stat.is_hidden, "is_hidden")
        ko(stat.is_sys, "is_sys")
        ko(stat.is_arch, "is_arch")
    end)

    it:should('stat non existing file', function()
        local stat = file.stat("not existing file")
        
        ok(stat == nil, "stat empty")
    end)
end)

mispec.run()
