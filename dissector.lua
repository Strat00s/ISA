--isa protocol sub tree
Isa_entry = Proto("ISA",  "ISA Protocol")
Data_length = ProtoField.int16("isa.data_length", "Data length", base.DEC)
Msg_sender  = ProtoField.string("isa.sender"    , "Sender")
Command     = ProtoField.string("isa.command"   , "Command")
Response    = ProtoField.string("isa.response"  , "Response")
Isa_entry.fields = {Data_length, Msg_sender, Command, Response}


--payload sub tree
Payload_entry = Proto("ISA_payload", "Payload")
Payload_length  = ProtoField.int16("isa.payload.length"  , "Payload length", base.DEC)
Payload_raw     = ProtoField.string("isa.payload.raw"    , "Payload (raw)")
Payload_msg_cnt = ProtoField.int16("isa.payload.msg_cnt" , "Message count", base.DEC)
Payload_entry.fields = {Payload_length, Payload_raw, Payload_msg_cnt}

--message sub tree
Message_entry = Proto("ISA_message", "Message")
Message_sender = ProtoField.string("isa.message_sender"  , "Message sender")
Message_sub    = ProtoField.string("isa.message_subject" , "Message subject")
Message_body   = ProtoField.string("isa.message_body"    , "Message body")
Payload_entry.fields = {Message_sender, Message_sub, Message_body}

--arguments sub tree entry
--arguments:
--    session hash | logout list send fetch
--    recipient    |             send
--    mail sub     |             send
--    mail body    |             send
--    msg id       |                  fetch
--    user         |                        login register
--    passwd hash  |                        login register
Arguments_entry = Proto("ISA_arguments", "Arguments")
Argument_cnt          = ProtoField.int16 ("isa.arguments.count"      , "Argument count", base.DEC)
Argument_session_hash = ProtoField.string("isa.argument.session_hash", "Session hash")
Argument_recipient    = ProtoField.string("isa.argument.recipient"   , "Recipient")
Argument_msg_sub      = ProtoField.string("isa.argument.msg_subject" , "Message subject")
Argument_msg_body     = ProtoField.string("isa.argument.msg_body"    , "Message body")
Argument_msg_id       = ProtoField.string("isa.argument.mgs_id"      , "Message id")
Argument_user         = ProtoField.string("isa.argument.username"    , "Username")
Argument_passwd_hash  = ProtoField.string("isa.argument.passwd_hash" , "Password hash")
Payload_entry.fields = {Argument_cnt, Argument_session_hash, Argument_recipient, Argument_msg_sub, Argument_msg_body, Argument_msg_id, Argument_user, Argument_passwd_hash}


--split line by delimiter
function SplitLine(payload, delimiter)
    local splits = {}
    local last_index = 0
    --loop and split
    while true do
        i, j = payload:find(delimiter, last_index + 1)    --find first space after last one
        if (i == nil) then break end    --break on end
        table.insert(splits, {last_index + 2, payload:sub(last_index + 1, i - 1)})  --save split
        last_index = i  --move in line
    end
    --fetch fix
    if (last_index + 2 > #payload) then
        last_index = last_index - 2
    end
    table.insert(splits, {last_index + 2, payload:sub(last_index + 1, #payload)})   --inser last element manually, as there are no more spaces
    return splits;
end

--split recieved messages
function SplitMessages(payload)
    local list = {};

    --empty message payload
    if (payload:sub(2, 2) == ")") then
        table.insert(list, 0)
        return list
    end

    --single message
    if (payload:sub(2, 2) == "\"") then
        table.insert(list, 1)
        table.insert(list, SplitLine(payload:sub(2, #payload - 1), " "))
        return list
    end

    --multiple messages --TODO
    if (payload:sub(2, 2) == "(") then
        local message_splits = SplitLine(payload, ")")  --remove bracket using sub and then split by left over brackets
        table.insert(list, #message_splits - 2)         --save message count (not corret because extra brackets)
        for i = 1, list[1] do
            table.insert(list, SplitLine(message_splits[i][2]:sub(3), " "))
        end
        return list
    end

    --not a message
    table.insert(list, -1)
    return list
end

--main function for dissecting
function Isa_entry.dissector(buffer, pinfo, tree)
    local proto_len = buffer:len()       --entire protocol length
    if proto_len == 0 then return end    --no data -> no protocol

    --(ISA ENTRY)--
    pinfo.cols.protocol = Isa_entry.name --rename collumn to ISA

    --start building the tree
    local isa_tree = tree:add(Isa_entry, buffer(), "ISA Protocol") --add ISA protocl sub tree
    isa_tree:add(Data_length, buffer(0, -1), proto_len)             --highlight entire message and show length
    
    --find response/command by first white space
    local data = buffer(0):string()
    local position = 0
    for i=1, proto_len do
        if (data:sub(i,i) == " ") then
            position = i
            break
        end
    end
    local code = buffer(1, position - 2):string()   --save response/command

    --decide between client and server packet
    local sender = "client"
    if (code == "ok" or code == "err") then
        sender = "server"
    end
    isa_tree:add(Msg_sender, sender)    --add sender

    --payload (and command) data
    payload_len = proto_len - position - 1  --calculate payload length
    payload_buffer = buffer(position, payload_len)

    --server has paylod
    if (sender == "server") then
        isa_tree:add(Response, code)    --add response
        
        payload_tree = isa_tree:add(Payload_entry, payload_buffer(), "Payload") --add payload entry
        payload_tree:add(Payload_length, payload_buffer(), payload_len)         --get payload length
        payload_tree:add(Payload_raw, payload_buffer())                         --print raw payload
        
        split_messages = SplitMessages(payload_buffer():string())   --split messages (if we have any)
        
        --empty message
        if (split_messages[1] == 0) then
            payload_tree:add(Payload_msg_cnt, split_messages[1])

        --single message
        elseif (split_messages[1] == 1) then
            message_buffer = payload_buffer(1, payload_len - 2)
            payload_tree:add(Payload_msg_cnt, split_messages[1])
            
            message_tree = payload_tree:add(Message_entry, payload_buffer(), "Message 1")
            message_tree:add(Message_sender, message_buffer(split_messages[2][1][1] - 1, #split_messages[2][1][2] - 2))
            message_tree:add(Message_sub,    message_buffer(split_messages[2][2][1] - 1, #split_messages[2][2][2] - 2))
            message_tree:add(Message_body,   message_buffer(split_messages[2][3][1] - 1, #split_messages[2][3][2] - 2))

        --not a message
        elseif (split_messages[1] == -1) then
            --nothing to do here

        --multiple messages
        else
            payload_tree:add(Payload_msg_cnt, split_messages[1])
            msg_index = split_messages[2][1][1] - 1   --start is on first message, first word -1 to add bracket
            msg_len = 0
            for i = 1, split_messages[1] do
                msg_len = #split_messages[i + 1][1][2] + #split_messages[i + 1][2][2] + #split_messages[i + 1][3][2] + 4 --2x space between words and 2x bracket¨
                message_buffer = payload_buffer(msg_index, msg_len)
                msg_index = msg_index + msg_len + 1 --skip space between messages

                message_tree = payload_tree:add(Message_entry, message_buffer(), "Message " .. tostring(i))
                message_tree:add(Message_sender, message_buffer(split_messages[i + 1][2][1], #split_messages[i + 1][2][2] - 2))
                message_tree:add(Message_sub,    message_buffer(split_messages[i + 1][3][1], #split_messages[i + 1][3][2] - 2))
            end
        end

    --client has commands
    else
        isa_tree:add(Command, code)                                                     --add command
        arguments_tree = isa_tree:add(Arguments_entry, payload_buffer(), "Argument(s)") --add arguments entry
        arguments = SplitLine(payload_buffer():string(), " ")                           --get arguments
        arguments_tree:add(Argument_cnt, #arguments)                                    --add argument count

        if (code == "logout" or code == "list") then
            arguments_tree:add(Argument_session_hash, payload_buffer(arguments[1][1] - 1, #arguments[1][2] - 2))   --add session hash without '"' and ')'
        end
        if (code == "register" or code == "login") then
            arguments_tree:add(Argument_user,        payload_buffer(arguments[1][1] - 1, #arguments[1][2] - 2)) --add username
            arguments_tree:add(Argument_passwd_hash, payload_buffer(arguments[2][1] - 1, #arguments[2][2] - 2)) --add password hash
        end
        if (code == "fetch") then
            arguments_tree:add(Argument_session_hash, payload_buffer(arguments[1][1] - 1, #arguments[1][2] - 2))
            arguments_tree:add(Argument_msg_id, payload_buffer(arguments[2][1]))    --add message id
        end
        if (code == "send") then
            arguments_tree:add(Argument_session_hash, payload_buffer(arguments[1][1] - 1, #arguments[1][2] - 2))
            arguments_tree:add(Argument_recipient,    payload_buffer(arguments[2][1] - 1, #arguments[2][2] - 2))   --add recipient
            arguments_tree:add(Argument_msg_sub,      payload_buffer(arguments[3][1] - 1, #arguments[3][2] - 2))   --add message sub
            arguments_tree:add(Argument_msg_body,     payload_buffer(arguments[4][1] - 1, #arguments[4][2] - 2))   --add message body
        end
    end
    --isa_tree:add(code, buffer(1, position - 2))
end

local tcp_port = DissectorTable.get("tcp.port") --specify port for ISA protocol
tcp_port:add(16853, Isa_entry)
