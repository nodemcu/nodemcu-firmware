-- A generic fifo module.  See docs/lua-modules/fifo.md for use examples.

local tr, ti = table.remove, table.insert

-- Remove an element and pass it to k, together with a boolean indicating that
-- this is the last element in the queue; if that returns a value, leave that
-- pending at the top of the fifo.
--
-- If k returns nil, the fifo will be advanced.  Moreover, k may return a
-- second result, a boolean, indicating "phantasmic" nature of this element.
-- If this boolean is true, then the fifo will advance again, passing the next
-- value, if there is one, to k, or priming itself for immediate execution at
-- the next call to queue.
--
-- If the queue is empty, do not invoke k but flag it to enable immediate
-- execution at the next call to queue.
--
-- Returns 'true' if the queue contained at least one non-phantom entry,
-- 'false' otherwise.
local function dequeue(q,k)
  if #q > 0
   then
     local new, again = k(q[1], #q == 1)
     if new == nil
       then tr(q,1)
            if again then return dequeue(q, k) end -- note tail call
       else q[1] = new
     end
     return true
   else q._go = true ; return false
  end
end

-- Queue a on queue q and dequeue with `k` if the fifo had previously emptied.
local function queue(q,a,k)
  ti(q,a)
  if k ~= nil and q._go then q._go = false; dequeue(q, k) end
end

-- return a table containing just the FIFO constructor
return {
  ['new'] = function()
    return { ['_go'] = true ; ['queue'] = queue ; ['dequeue'] = dequeue }
   end
}
