--TODO comments and rewamp

--isa protocol tree entry
Isa_entry = Proto("ISA",  "ISA Protocol")
Data_length = ProtoField.int16("isa.data_length", "Data length", base.DEC)
Msg_sender  = ProtoField.string("isa.sender", "Sender")
Command     = ProtoField.string("isa.command", "Command")
Response    = ProtoField.string("isa.response", "Response")
Isa_entry.fields = {Data_length, Msg_sender, Command, Response}


--payload sub tree entry
Payload_entry = Proto("ISA_payload", "Payload")
Payload_length       = ProtoField.int16("isa.payload.length", "Payload length", base.DEC)
Payload_raw          = ProtoField.string("isa.payload.raw", "Payload (raw)")
Payload_entry.fields = {Payload_length, Payload_raw}


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
Argument_cnt          = ProtoField.int16("isa.arguments.count", "Argument count", base.DEC)
Argument_session_hash = ProtoField.string("isa.argument.session_hash", "Session hash")
Argument_recipient    = ProtoField.string("isa.argument.recipient"   , "Recipient")
Argument_msg_sub      = ProtoField.string("isa.argument.msg_topic"   , "Message topic") --fix change this to real name
Argument_msg_body     = ProtoField.string("isa.argument.msg_body"    , "Message body")
Argument_msg_id       = ProtoField.string("isa.argument.mgs_id"      , "Message id")
Argument_user         = ProtoField.string("isa.argument.username"        , "Username")
Argument_passwd_hash  = ProtoField.string("isa.argument.passwd_hash" , "Password hash")
Payload_entry.fields = {Argument_cnt, Argument_session_hash, Argument_recipient, Argument_msg_sub, Argument_msg_body, Argument_msg_id, Argument_user, Argument_passwd_hash}


--split payload by spaces
--return table of tables: list{{starting_index, split},...}
function SplitPayload(payload)
    local splits = {}
    local last_index = 0
    --loop and insert splits
    while true do
        i, j = payload:find(" ", last_index + 1)    --find first space after last one
        if (i == nil) then break end    --break on end
        table.insert(splits, {last_index + 2, payload:sub(last_index + 1, i - 1)})
        last_index = i
    end
    --fix for fetch
    if (last_index + 2 > #payload) then last_index = last_index - 2 end
    table.insert(splits, {last_index + 2, payload:sub(last_index + 1, #payload)})   --inser last element manually, as there is are no more spaces
    return splits;
end

--split recieved messages
function SplitReceived(payload)
    local list = {};
    while true do
        
    end
    return list;
end

--main function for dissecting
function Isa_entry.dissector(buffer, pinfo, tree)
    local proto_len = buffer:len()       --entire protocol length
    if proto_len == 0 then return end    --no data -> no protocol

    --(ISA ENTRY)--
    --find code/command by first white space
    local data = buffer(0):string()
    local position = 0
    for i=1, proto_len do
        if (data:sub(i,i) == " ") then
            position = i
            break
        end
    end

    --decide between client and server packet
    local sender = "client"
    if (data:sub(2, position-1) == "ok" or data:sub(2, position  -2) == "er") then
        sender = "server"
    end

    --start building the tree
    local isa_tree = tree:add(Isa_entry, buffer(), "ISA Protocol") --add ISA protocl sub tree
    isa_tree:add(Data_length, buffer(0, -1), proto_len) --highlight entire message a show length
    isa_tree:add(Msg_sender, sender)

    --payload (and command) data
    local payload_len = proto_len - position - 1                                        --calculate payload length
    local payload_buffer = buffer(position, payload_len)
    --local code = buffer(1, position - 2):string()                                       --response and command variable
    
    --client has arguments
    if (sender == "client") then
        isa_tree:add(Command, buffer(1, position - 2))  --add command entry
        local arguments_tree = isa_tree:add(Arguments_entry, payload_buffer(), "Argument(s)")   --add arguments entry
        
        local code = buffer(1, position - 2):string()
        local arguments = SplitPayload(payload_buffer():string())

        --session hash
        if (code == "logout" or code == "list") then
            arguments_tree:add(Argument_cnt, payload_buffer(), #arguments)   --add argument count
            arguments_tree:add(Argument_session_hash, payload_buffer(arguments[1][1] - 1, #arguments[1][2] - 2))   --add session hash without '"' and ')'
        end
        --2 args
        if (code == "register" or code == "login") then
            arguments_tree:add(Argument_cnt, payload_buffer(), #arguments)
            arguments_tree:add(Argument_user,        payload_buffer(arguments[1][1] - 1, #arguments[1][2] - 2)) --add username
            arguments_tree:add(Argument_passwd_hash, payload_buffer(arguments[2][1] - 1, #arguments[2][2] - 2)) --add password hash
        end
        if (code == "fetch") then   --fetch is special
            arguments_tree:add(Argument_cnt, payload_buffer(), #arguments)
            arguments_tree:add(Argument_session_hash, payload_buffer(arguments[1][1] - 1, #arguments[1][2] - 2))
            arguments_tree:add(Argument_msg_id, payload_buffer(arguments[2][1]))    --add message id
        end
        --4 args
        if (code == "send") then
            arguments_tree:add(Argument_cnt, payload_buffer(), #arguments)
            arguments_tree:add(Argument_session_hash, payload_buffer(arguments[1][1] - 1, #arguments[1][2] - 2))
            arguments_tree:add(Argument_recipient,    payload_buffer(arguments[2][1] - 1, #arguments[2][2] - 2))   --add recipient
            arguments_tree:add(Argument_msg_sub,      payload_buffer(arguments[3][1] - 1, #arguments[3][2] - 2))   --add message sub
            arguments_tree:add(Argument_msg_body,     payload_buffer(arguments[4][1] - 1, #arguments[4][2] - 2))   --add message body
        end
    end

    --server has paylod
    if (sender == "server") then
        isa_tree:add(Response, buffer(1, position - 2))

        --(PAYLOAD ENTRY)--
        --local payload_raw_data = buffer(position):string():sub(1, proto_len - position - 1) --get entire payload string
        --local payload_len = proto_len - position - 1                                        --calculate payload length

        local payload_tree = isa_tree:add(Payload_entry, payload_buffer(), "Payload")
        payload_tree:add(Payload_length, payload_buffer(), payload_len)
        payload_tree:add(Payload_raw, payload_buffer(), payload_buffer():string())
    end
    --isa_tree:add(code, buffer(1, position - 2))

    pinfo.cols.protocol = Isa_entry.name --rename collumn to ISA
end

local tcp_port = DissectorTable.get("tcp.port") --specify port for ISA protocol
tcp_port:add(16853, Isa_entry)
