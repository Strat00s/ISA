#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
//#include <strings.h>
#include <iostream>
#include <vector>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

using namespace std;

#define DEFAULT_ADDRESS "localhost"
#define DEFAULT_PORT 32323

struct LineArgs{
    string address = DEFAULT_ADDRESS;
    int port = DEFAULT_PORT;
    string command = "NONE";
    vector<string> arguments;
};

//TODO base64
//string base64Encode(string s) {
//
//}
//string base64Decode(string s) {
//
//}

//TODO save and load session hash

//TODO move argument parsing to own function

//TODO test this abomination
int errorExit(string msg, int err_code) {
    cout << msg << endl;
    return err_code; //exit(err_code);
}

//TODO + - .
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


void error(const char *msg)
{
    perror(msg);
    exit(0);
}

//TODO fix argument response to look the same as reference client
int main(int argc, char *argv[]) {
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


    //TODO comments and understand/rewamp
    struct sockaddr_in server;
    struct hostent *host_entry;
    char buffer[256];
    
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) return errorExit("ERROR opening socket", 1);
    
    host_entry = gethostbyname(line_args.address.c_str());
    if (host_entry == NULL) return errorExit("ERROR, no such host", 1);
    
    server.sin_family = AF_INET;
    server.sin_port = htons(line_args.port);
    memcpy(&server.sin_addr, host_entry->h_addr_list[0], host_entry->h_length);
    
    if (connect(socket_fd, (struct sockaddr *) &server, sizeof(server)) < 0) return errorExit("ERROR connecting", 1);

    string request = "(" + line_args.command;//   "(register \"test\" \"dGVzdA==\")";
    for (int i = 0; i < line_args.arguments.size(); i++) {
        request = request + " \"" + line_args.arguments.at(i) + "\"";
    }
    request = request + ")";
    
    int n = write(socket_fd, request.c_str(), request.length());
    if (n < 0) 
         error("ERROR writing to socket");
    
    bzero(buffer,256);
    
    n = read(socket_fd, buffer, 255);
    if (n < 0) 
         error("ERROR reading from socket");
    
    printf("%s\n", buffer);
    
    close(socket_fd);
    
    return 0;
}