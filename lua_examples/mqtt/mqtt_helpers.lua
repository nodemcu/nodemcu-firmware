-- ###########################################################################
--
-- Generic functions to subscribe and unsubscribe multiple topics at once.
--
-- subscribe_multi accepts a table of topic/qos entries. The topics are
-- subscribed one after another.
--
-- unsubscribe_multi accepts the same table to unsubscribe from topics.
-- The qos value is ignored.
--
-- Usage:
--   mytopics = {["topic1"] = 0, ["topic2"] = 1}
--   subscribe_multi(m, mytopics, function(client)
--     print("multiple topics subscription done")
--   end)
--
--   unsubscribe_multi(m, mytopics, function(client)
--     print("multiple topics unsubscription done")
--   end)
--
-- ###########################################################################


function subscribe_multi(client, uv_topics, uv_cb)
  local uv_topic, uv_qos = next(uv_topics, nil) -- more upvals

  local function subscribe_cb(client)
    print("subscribed to topic", uv_topic)
    uv_topic, uv_qos = next(uv_topics, uv_topic)
    if uv_topic ~= nil and uv_qos ~= nil then
      client:subscribe(uv_topic, uv_qos, subscribe_cb)
    else
      uv_cb(client)
    end
  end

  client:subscribe(uv_topic, uv_qos, subscribe_cb)
end

function unsubscribe_multi(client, uv_topics, uv_cb)
  local uv_topic = next(uv_topics, nil) -- more upval

  local function unsubscribe_cb(client)
    print("unsubscribed from topic", uv_topic)
    uv_topic = next(uv_topics, uv_topic)
    if uv_topic ~= nil then
      client:unsubscribe(uv_topic, unsubscribe_cb)
    else
      uv_cb(client)
    end
  end

  client:unsubscribe(uv_topic, unsubscribe_cb)
end
