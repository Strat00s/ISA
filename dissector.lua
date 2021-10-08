isa_protocol = Proto("ISA",  "ISA Protocol")    --required

data_length = ProtoField.int16("isa.data_length", "Data length", base.DEC)
msg_type = ProtoField.string("isa.type", "Type")
code = ProtoField.string("isa.code", "Code")
message_length = ProtoField.int16("isa.Message_length", "Message length", base.DEC)
raw_message = ProtoField.string("isa.message", "Message (raw)")


isa_protocol.fields = {data_length, msg_type, code, message_length, raw_message}                        --required

--main function for dissecting
function isa_protocol.dissector(buffer, pinfo, tree)
    local length = buffer:len()
    if length == 0 then return end  --no data -> no protocol

    local isa_tree = tree:add(isa_protocol, buffer(), "ISA Protocol") --add ISA protocl sub tree

    --find code/command by first white space
    local message = buffer(0):string()
    local position = 0
    for i=1, length do
        if (message:sub(i,i) == " ") then
            position = i
            break
        end
    end

    --decide between request and response packet
    local type = "request"
    if (message:sub(2, position-1) == "ok" or message:sub(2, position-2) == "er") then
        type = "response"
    end

    --isa_tree:add("Message length:", length) --add message length field
    isa_tree:add(message_length, buffer(0, -1), length) --highlight entire message a show length
    isa_tree:add(msg_type, type)
    isa_tree:add(code, buffer(1, position-2))

    pinfo.cols.protocol = isa_protocol.name --rename collumn to ISA
end

local tcp_port = DissectorTable.get("tcp.port") --specify port for ISA protocol
tcp_port:add(16853, isa_protocol)
