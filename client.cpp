#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <regex>
#include <bitset>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define DEFAULT_ADDRESS "localhost"
#define DEFAULT_PORT 32323
#define MAX_BUFFER_SIZE 256
#define OK_START 4      //start of payload from ok response
#define ERR_START 5     //      -- || --        err response
#define RESPONSE_END 1  //')'
#define SESSION_HASH_FILE "login-token"

using namespace std;

struct LineArgs{
    string address = DEFAULT_ADDRESS;
    int port = DEFAULT_PORT;
    string command = "NONE";
    vector<string> arguments;
};


void signalCallback(int signal) {
           cerr << "Process interrupted by the user" << endl;
           exit(1);
}

int errorExit(string msg, int err_code) {
    cout << msg << endl;
    return err_code; //exit(err_code);
}


//TODO comments
string base64Encode(string s) {
    cout << "String to decode: " << s << endl;
    unsigned char base64_table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int len = s.length();
    string encoded(4 * (len + 2) / 3, '='); //fill string with padding
    int j = 0;
    int p = len % 3;
    int last = len - p;

    for (size_t i = 0; i < last; i += 3) {
        int n = int(s[i]) << 16 | int(s[i + 1]) << 8 | s[i + 2];
        encoded[j++] = base64_table[n >> 18];
        encoded[j++] = base64_table[n >> 12 & 0x3F];
        encoded[j++] = base64_table[n >> 6  & 0x3F];
        encoded[j++] = base64_table[n       & 0x3F];
    }

    //padding is required
    if (p) {
        int n = --p ? int(s[last]) << 8 | s[last + 1] : s[last];
        encoded[j++] = base64_table[p ? n >> 10 & 0x3F  : n >> 2];
        encoded[j++] = base64_table[p ? n >> 4  & 0x03F : n << 4 & 0x3F];
        encoded[j++] = p ? base64_table[n << 2  & 0x3F] : '=';
    }

    cout << encoded << endl;
    return encoded;
}

/*----(file operations)----*/
//TODO save and load session hash
//save hash to file
int saveHash(string hash) {
    ofstream hash_file(SESSION_HASH_FILE, ofstream::trunc); //open file in overwrite mode
    if (hash_file.fail()) return 1;
    hash_file << hash;
    hash_file.close();
    return 0;
}

//load hash from file
string loadHash() {
    string session_hash;
    ifstream hash_file(SESSION_HASH_FILE);
    if (hash_file.fail()) return ".";   //if something went wrong, return character that can't be created via base64 encoding
    getline(hash_file, session_hash);   //get hash from file
    return session_hash;
}


/*----(String operations)----*/
//split string by regex delimiter
vector<string> splitByDelimiter(string s, string rgx_exp) {
    vector<string> splits;
    int old_index = 0;
    regex rgx(rgx_exp);
    //TODO comments
    for (auto it = sregex_iterator(s.begin(), s.end(), rgx); it != sregex_iterator(); it++) {
        splits.push_back(s.substr(old_index + 1, it->position()  - old_index));
        old_index = it->position() + 1; //move to next one
    }
    splits.push_back(s.substr(old_index + 1, s.length() - old_index - 2));
    return splits;
}

//check and convert string to number
int StringToNumber(string s) {
    //check if all chars are numbers
    for (int i = 0; i < s.length(); i++) {
        if (!isdigit(s[i])) return -1;
    }
    return stoi(s);
}

string ltrim(string s) {
    size_t start = s.find_first_not_of(" \n\r\t\f\v");
    if (start == string::npos) return s;
    return s.substr(start);
}
 
string rtrim(string s) {
    size_t end = s.find_last_not_of(" \n\r\t\f\v");
    if (end == string::npos) return s;
    return s.substr(0, end + 1);
}
 
string trim(string s) {
    s = ltrim(s);
    s = rtrim(s);
    return s;
}

//replace x with y in s
string replaceInString(string s, string x, string y) {
    int pos = 0;
    while ((pos = s.find(x, pos)) != string::npos) {
        s.replace(pos, x.length(), y);
        pos = pos + y.length();  //skip the newly escaped character
    }
    return s;
}


//get arguments, commands and so on
int parseArguments(int argc, char *argv[], LineArgs *line_args) {
    bool help = false;
    bool addr_set = false;
    bool port_set = false;

    //loop through all arguments (skip first one)
    for (int i = 1; i < argc; i++) {
        cout << argv[i] << endl;
        string argument = string(argv[i]);  //save current argument

        //help parsing
        if (argument == "--help" || argument == "-h") help = true;

        //TODO address and port have some duplicate code
        //address parsing
        else if (argument == "--address" || argument == "-a") {
            //check if address parameter was already used
            if (addr_set) return errorExit("client: only one instance of one option from (-a --address) is allowed", 1);
            addr_set = true;    //address was set

            //do we have enough arguments
            if (argc < i + 2) return errorExit("client: the \"" + argument + "\" option needs 1 argument, but 0 provided", 1);
            line_args->address = string(argv[i + 1]);  //save address
            i++;                            //skip one argument as we are using it as address
        }

        //port parsing (almost the same as address)
        else if (argument == "--port" || argument == "-p") {
            if (port_set) return errorExit("client: only one instance of one option from (-p --port) is allowed", 1);
            port_set = true;

            if (argc < i + 2) return errorExit("client: the \"" + argument + "\" option needs 1 argument, but 0 provided", 1);
            line_args->port = StringToNumber(string(argv[i + 1]));

            //return if port is not a number
            if (line_args->port < 0) return errorExit("Port number is not a string", 1);
            i++;
        }

        //command and argument "parsing"
        //register and login
        //TODO maybe some duplicate code
        else if (argument == "register" || argument == "login") {
            if (argc - (i + 1) != 2) return errorExit(argument + " <username> <password>", 1);
            line_args->command = argument;
            line_args->arguments.push_back(string(argv[i + 1]));
            line_args->arguments.push_back(base64Encode(string(argv[i + 2])));
            break;
        }

        //list
        else if (argument == "list" || argument == "logout") {
            if (argc - (i + 1) != 0) return errorExit(argument, 1);
            line_args->command = argument;
            break;
        }

        //send
        else if (argument == "send") {
            if (argc - (i + 1) != 3) return errorExit(argument + " <recipient> <subject> <body>", 1);
            line_args->command = argument;
            line_args->arguments.push_back(string(argv[i + 1]));
            line_args->arguments.push_back(string(argv[i + 2]));
            line_args->arguments.push_back(string(argv[i + 3]));
            break;
        }

        //fetch
        else if (argument == "fetch") {
            if (argc - (i + 1) != 1) return errorExit(argument + " <id>", 1);
            line_args->command = argument;
            line_args->arguments.push_back(string(argv[i + 1]));
            break;
        }

        //unknown command/switch
        else {
            if (argument[0] == '-') return errorExit("client: unknown switch: " + argument, 1);
            return errorExit("unknown command", 1);
        }
    }

    if (help) {
        cout << "usage: client [ <option> ... ] <command> [<args>] ...\n" << endl;
        cout << "<option> is one of\n" << endl;
        cout << "  -a <addr>, --address <addr>" << endl;
        cout << "     Server hostname or address to connect to" << endl;
        cout << "  -p <port>, --port <port>" << endl;
        cout << "     Server port to connect to" << endl;
        cout << "  --help, -h" << endl;
        cout << "     Show this help" << endl;
        cout << "  --" << endl;
        cout << "     Do not treat any remaining argument as a switch (at this level)\n" << endl;
        cout << " Multiple single-letter switches can be combined after" << endl;
        cout << " one `-`. For example, `-h-` is the same as `-h --`." << endl;
        cout << " Supported commands:" << endl;
        cout << "   register <username> <password>" << endl;
        cout << "   login <username> <password>" << endl;
        cout << "   list" << endl;
        cout << "   send <recipient> <subject> <body>" << endl;
        cout << "   fetch <id>" << endl;
        cout << "   logout" << endl;
        return 0;
    }

    if(line_args->command == "NONE") return errorExit("client: expects <command> [<args>] ... on the command line, given 0 arguments", 1);
    
    //"fix" all arguments to make them protocol correct
    for (int i = 0; i < line_args->arguments.size(); i++) {
        line_args->arguments[i] = trim(line_args->arguments[i]);  //trim spaces for later easier response parsing
        line_args->arguments[i] = replaceInString(line_args->arguments[i], "\\", "\\\\"); //escape '\'
        line_args->arguments[i] = replaceInString(line_args->arguments[i], "\"", "\\\""); //escape '"'
    }
    return -1;
}


int main(int argc, char *argv[]) {
    signal (SIGINT, signalCallback);

    /*----(argument parsing)----*/
    LineArgs line_args;
    int early_exit = parseArguments(argc, argv, &line_args);
    if (early_exit >= 0) return early_exit;

    cout << "port: " << line_args.port << endl;
    cout << "address: " << line_args.address << endl;
    cout << "command: " << line_args.command << " ";
    for (int i = 0; i < line_args.arguments.size(); i++) {
        cout << line_args.arguments[i] << " ";
    }
    cout << endl;


    //TODO move to won function
    /*----(socket stuff)----*/
    int socket_fd, wr;
    struct sockaddr_in server;
    struct hostent *host_entry;
    char buffer[MAX_BUFFER_SIZE] = {0};
    string request, response = "";
    
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);    //create socket file descriptor
    if (socket_fd < 0) return errorExit("ERROR opening socket", 1);
    
    host_entry = gethostbyname(line_args.address.c_str());  //get host (ip/hostname)
    if (host_entry == NULL) return errorExit("ERROR, no such host", 1);
    
    server.sin_family = AF_INET;
    server.sin_port = htons(line_args.port);                                    //set port
    memcpy(&server.sin_addr, host_entry->h_addr_list[0], host_entry->h_length); //set ip
    
    //try and connect
    if (connect(socket_fd, (struct sockaddr *) &server, sizeof(server)) < 0) return errorExit("Filed to connect to host", 1);

    //create requiest payload with command
    request = "(" + line_args.command;
    
    //add hash only on supported commands
    if (line_args.command != "login" && line_args.command != "register") {
        string hash = loadHash();
        if (hash == ".") return errorExit("Not logged in", 1);
        request += " " + hash;
    }

    //add arguments to payload (fetch is special)
    if (line_args.command == "fetch") request += line_args.arguments[0];
    else {
        for (int i = 0; i < line_args.arguments.size(); i++) {
            request += " \"" + line_args.arguments[i] + "\"";
        }
    }
    request += ")";

    cout << "request: " << request << endl;
    
    //write request to socket
    wr = write(socket_fd, request.c_str(), request.length());
    if (wr < 0) return errorExit("ERROR writing to socket", 2);
       
    //read from socket
    do {
        wr = read(socket_fd, buffer, MAX_BUFFER_SIZE - 1);  //read
        response += string(buffer);                         //save
        memset(buffer, 0, MAX_BUFFER_SIZE);                 //clear
    } while(wr > 0);
    if (wr < 0) return errorExit("ERROR reading from socket", 1);
    close(socket_fd);   //close socket

    cout << response << endl;
    cout << response.substr(OK_START, response.length() - (OK_START + RESPONSE_END)) << endl;

    //TODO escape sequences
    if (response.substr(1, 2) == "ok") {
        cout << "SUCCESS: ";
        vector<string> splits;

        //command specific parsing
        if (line_args.command == "login") {
            splits = splitByDelimiter(response.substr(OK_START, response.length() - (OK_START + RESPONSE_END)), "\" \"");
            if (saveHash(splits[1].substr(0, splits[1].length()) + "\"")) return errorExit("ERROR failed to save session hash", 1);
            cout << splits[0].substr(0, splits[0].length() - 1) << endl;
        }

        else if (line_args.command == "fetch") {
            splits = splitByDelimiter(response.substr(OK_START, response.length() - (OK_START + RESPONSE_END)), "\" \"");
            cout << endl << endl;
            cout << "From: " << splits[0].substr(1, splits[0].length() - 2) << endl;
            cout << "Subject: " << splits[1].substr(1, splits[1].length() - 2) << endl;
            cout << splits[2].substr(1, splits[2].length() - 2);
        }

        else if (line_args.command == "list") {
            splits = splitByDelimiter(response.substr(OK_START, response.length() - (OK_START + RESPONSE_END)), "\\) \\(\\d");
            cout << endl;
            for (int i = 0; i < splits.size(); i++) {
                cout << splits[i].substr(1, 1) << ":" << endl;
                vector<string> single_msg = splitByDelimiter(splits[i].substr(2), "\" \"");
                cout << "  From: " << single_msg[0].substr(1, single_msg[0].length() - 2) << endl;
                cout << "  Subject: " << single_msg[1].substr(1, single_msg[1].length() - 2) << endl;
            }
        }

        else {
            cout << response.substr(OK_START + 1, response.length() - (OK_START + RESPONSE_END + 2)) << endl;
        }
    }
    else if (response.substr(1, 3) == "err") {
        cout << "ERROR: " << response.substr(ERR_START + 1, response.length() - (ERR_START + RESPONSE_END + 2)) << endl;
    }
    else return errorExit("Unknown response", 1);

    return 0;
}