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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

using namespace std;

#define DEFAULT_ADDRESS "localhost"
#define DEFAULT_PORT 32323
#define MAX_BUFFER_SIZE 256
#define OK_START 4      //start of payload from ok response
#define ERR_START 5     //      -- || --        err response
#define RESPONSE_END 1  //')'
#define SESSION_HASH_FILE "login-token"

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

//TODO base64
//string base64Encode(string s) {
//
//}
//string base64Decode(string s) {
//
//}

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


//TODO split by delimiter
        //WARNING regex might be required
//split string
void splitByDelimiter(string s, string del) {
    cout << s << endl;
    int start_index = 0;
    int index;
    //cout << "Del: " << count(s.begin(), s.end(), del) << endl;
    //while (true) {
    //    s.
    //}
    
}

//check and convert string to number
int StringToNumber(string s) {
    //check if all chars are numbers
    for (int i = 0; i < s.length(); i++) {
        if (!isdigit(s.at(i))) {
            cout << s.at(i) << " is not a number" << endl;
            return -1;
        }
    }
    return stoi(s);
}

//FIX escape special characters
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
            line_args->arguments.push_back(string(argv[i + 2]));
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
            if (argument.at(0) == '-') return errorExit("client: unknown switch: " + argument, 1);
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
        cout << line_args.arguments.at(i) << " ";
    }
    cout << endl;


    /*----(socket stuff)----*/
    //TODO comments and understand/rewamp
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
    if (connect(socket_fd, (struct sockaddr *) &server, sizeof(server)) < 0) return errorExit("ERROR connecting to host", 1);

    //create requiest payload with command
    request = "(" + line_args.command;
    
    //add hash only on supported commands
    if (line_args.command != "login" && line_args.command != "register") {
        string hash = loadHash();
        if (hash == ".") return errorExit("Not logged in", 1);
        request += " " + hash;
    }

    //add arguments to payload (fetch is special)
    if (line_args.command == "fetch") request += line_args.arguments.at(0);
    else {
        for (int i = 0; i < line_args.arguments.size(); i++) {
            request += " \"" + line_args.arguments.at(i) + "\"";
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

    //TODO format message on list, save hash on login, 
    if (response.substr(1, 2) == "ok") {
        splitByDelimiter(response.substr(OK_START, response.length() - (OK_START + RESPONSE_END)), " \"");
        //cout << "SUCCESS: " << response.substr(OK_START, response.length() - (OK_START + RESPONSE_END)) << endl;
    }
    else if (response.substr(1, 3) == "err") {
        cout << "ERROR: " << response.substr(ERR_START, response.length() - (ERR_START + RESPONSE_END)) << endl;
    }
    else return errorExit("Unknown response", 1);

    return 0;
}