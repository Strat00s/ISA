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
--TODO add payload session hash
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



--slightly edited function from client.cpp
--responses to commands
    --err, send, logout, register: 1 bracket pair,  1 string    -> raw payload
    --fetch:                       2 bracket pairs, 3 strings   -> message
    --list:                        3 bracket pairs, x*2 strings -> messages
    --list empty:                  2 bracket pairs, 0 strings   -> no message
    --login:                       1 bracket pair,  2 strings   -> login
function SplitByQuotes(s, pinfo)
    local splits = {} --splits = {{start, end, len}, {start, end, len}, ...}
    local start_index = -1
    local bracket_cnt = 0
    local max_brackets = 0
    local response_type = 0
    local i = 1
    while true do
        --save max bracket count for payload determination
        if (max_brackets < bracket_cnt) then
            max_brackets = bracket_cnt
        end

        --count brackets to find out if the entire message was received
        if (s:sub(i, i) == "(" and start_index == -1) then
            bracket_cnt = bracket_cnt + 1
        end
        if (s:sub(i, i) == ")" and start_index == -1) then
            bracket_cnt = bracket_cnt - 1
        end

        --exit or request more data when at the end of the string
        if (i >= #s) then
            
            --request another segment once end of the current buffer is reached and we still dont have the entire response
            if (bracket_cnt ~= 0) then
                pinfo.desegment_len = DESEGMENT_ONE_MORE_SEGMENT
                return -1, splits
            else
                --response payload determination
                if (max_brackets == 1 and #splits == 1)     then response_type = 0 end --raw payload
                if (max_brackets == 2 and #splits == 3)     then response_type = 1 end --message
                if (max_brackets == 3 and #splits % 2 == 0) then response_type = 2 end --messages
                if (max_brackets == 2 and #splits == 0)     then response_type = 3 end --no message
                if (max_brackets == 1 and #splits == 2)     then response_type = 4 end --login
            end

            break
        end

        --skip slashes and escaped quotes
        if (s:sub(i, i + 1) == "\\\\" or s:sub(i, i + 1) == "\\\"") then
            i = i + 1   --increment i to skip the second character

        --check for quotes
        elseif (s:sub(i, i) == "\"") then
            if (start_index ~= -1) then
                table.insert(splits, {start_index, i, i - start_index + 1})
                start_index = -1;
            else
                start_index = i;
            end
        end

        i = i + 1   --increment i
    end

    return response_type, splits
end


--main function for dissecting
function Isa_entry.dissector(buffer, pinfo, tree)
    local proto_len = buffer:len() --entire protocol length

    --no data -> no protocol
    if proto_len < 7 then
        return
    end

    --(ISA ENTRY)--
    pinfo.cols.protocol = Isa_entry.name --rename collumn to ISA

    --start building the tree
    local isa_tree = tree:add(Isa_entry, buffer(), "ISA Protocol") --add ISA protocl sub tree
    isa_tree:add(Data_length, buffer(0, -1), proto_len)             --highlight entire message and show length
    
    --find response/command
    local idc, res_com_end = string.find(buffer():string(), " ", 0, true)
    local res_com = buffer(1, res_com_end - 2):string()

    --split incoming messages to individual strings by unescaped quotes
    local response_type, msg_splits = SplitByQuotes(buffer():string(), pinfo)
    if (response_type == -1) then return end

    --payload (and command) data
    local payload_len = proto_len - res_com_end - 1  --calculate payload length
    local payload_buffer = buffer(res_com_end, payload_len)

    --(SERVER)--
    if (res_com == "ok" or res_com == "err") then
        pinfo.cols.info = "Server response - " .. res_com
        isa_tree:add(Msg_sender, "server")                                            --add sender
        isa_tree:add(Response, buffer(1, res_com_end - 2))                            --add response
        local payload_tree = isa_tree:add(Payload_entry, payload_buffer(), "Payload") --add payload entry
        payload_tree:add(Payload_length, payload_buffer(), payload_len)               --get payload length
        payload_tree:add(Payload_raw, payload_buffer())                               --print raw payload
        local msg_buffer                                                              --for code clarity

        --raw payload and login (for now)
        if (response_type == 0 or response_type == 4) then
            payload_tree:add(Payload_msg_cnt, payload_buffer(), 1)

        --single message
        elseif (response_type == 1) then
            payload_tree:add(Payload_msg_cnt, payload_buffer(), 1)
            msg_buffer = buffer(msg_splits[1][1] - 1, msg_splits[3][2] - msg_splits[1][1])   --get message buffer (to highlight the current message)
            local message_tree = payload_tree:add(Message_entry, msg_buffer(), "Message 1")  --add message tree entry
            message_tree:add(Message_sender, buffer(msg_splits[1][1], msg_splits[1][3] - 2)) --add sender
            message_tree:add(Message_sub,    buffer(msg_splits[2][1], msg_splits[2][3] - 2)) --add subject
            message_tree:add(Message_body,   buffer(msg_splits[3][1], msg_splits[3][3] - 2)) --add body

        --multiple messages
        elseif (response_type == 2) then
            payload_tree:add(Payload_msg_cnt, payload_buffer(), #msg_splits / 2)
            --loop the splits untill all messages are added
            for i = 1, #msg_splits / 2 do
                local si = i * 2
                msg_buffer = buffer(msg_splits[si - 1][1] - 1, msg_splits[si][2] - msg_splits[si - 1][1])
                local message_tree = payload_tree:add(Message_entry, msg_buffer(), "Message " .. tostring(i))
                message_tree:add(Message_sender, buffer(msg_splits[si - 1][1],     msg_splits[si - 1][3] - 2))
                message_tree:add(Message_sub,    buffer(msg_splits[si][1],         msg_splits[si][3] - 2))
            end
        --no messages do nothing
        end

    --(CLIENT)--
    else
        pinfo.cols.info = "Client request - " .. res_com
        isa_tree:add(Msg_sender, "client")                                                    --add sender
        isa_tree:add(Command, buffer(1, res_com_end - 2))                                     --add command
        local arguments_tree = isa_tree:add(Arguments_entry, payload_buffer(), "Argument(s)") --add arguments entry
        arguments_tree:add(Argument_cnt, #msg_splits)                                         --add argument count

        --1 argument
        if (res_com == "logout" or res_com == "list") then
            arguments_tree:add(Argument_session_hash, buffer(msg_splits[1][1], msg_splits[1][3] - 2)) --add session hash without '"' and ')'
        end

        --2 arguments
        if (res_com == "register" or res_com == "login") then
            arguments_tree:add(Argument_user,        buffer(msg_splits[1][1], msg_splits[1][3] - 2)) --add username
            arguments_tree:add(Argument_passwd_hash, buffer(msg_splits[2][1], msg_splits[2][3] - 2)) --add password hash
        end

        --fetch is special
        if (res_com == "fetch") then
            --get the msg id we want to fetch (from the back)
            local id_len, id_start = buffer():string():reverse():find(" ", 0, true)
            id_start = buffer():len() - id_start + 2    --+2 -> flip and space
            id_len = buffer():len() - id_start

            arguments_tree:add(Argument_session_hash, buffer(msg_splits[1][1], msg_splits[1][3] - 2))
            arguments_tree:add(Argument_msg_id,       buffer(id_start - 1, id_len)) --add message id
        end

        --send is special
        if (res_com == "send") then
            arguments_tree:add(Argument_session_hash, buffer(msg_splits[1][1], msg_splits[1][3] - 2))
            arguments_tree:add(Argument_recipient,    buffer(msg_splits[2][1], msg_splits[2][3] - 2))   --add recipient
            arguments_tree:add(Argument_msg_sub,      buffer(msg_splits[3][1], msg_splits[3][3] - 2))   --add message sub
            arguments_tree:add(Argument_msg_body,     buffer(msg_splits[4][1], msg_splits[4][3] - 2))   --add message body
        end
    end
end

--specify port for ISA protocol
local tcp_port = DissectorTable.get("tcp.port")
tcp_port:add(32323, Isa_entry)
