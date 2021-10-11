#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string>
#include <iostream>
#include <vector>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

using namespace std;

#define DEFAULT_ADDRESS "localhost"
#define DEFAULT_PORT 32323

//TODO argument parsing
//TODO response parsing
/*
[isa@isa isa]$ ./client_d --help
usage: client [ <option> ... ] <command> [<args>] ...

<option> is one of

  -a <addr>, --address <addr>
     Server hostname or address to connect to
  -p <port>, --port <port>
     Server port to connect to
  --help, -h
     Show this help
  --
     Do not treat any remaining argument as a switch (at this level)

 Multiple single-letter switches can be combined after
 one `-`. For example, `-h-` is the same as `-h --`.
 Supported commands:
   register <username> <password>
   login <username> <password>
   list
   send <recipient> <subject> <body>
   fetch <id>
   logout
*/

struct LineArgs{
    string address = DEFAULT_ADDRESS;
    int port = DEFAULT_PORT;
    string command = "NONE";
    vector<string> arguments;
};


//string base64Encode(string s) {
//
//}
//string base64Decode(string s) {
//
//}

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

//TODO fix argument response to look the same as reference client
int main(int argc, char *argv[]) {
    /*----(argument parsing)----*/
    LineArgs line_args;
    bool help = false;
    bool addr_set = false;
    bool port_set = false;

    //loop through all arguments (skip first one)
    for (int i = 1; i < argc; i++) {
        cout << argv[i] << endl;
        string argument = string(argv[i]);  //save current argument

        //help parsing
        if (argument == "--help" || argument == "-h") help = true;

        //address parsing
        else if (argument == "--address" || argument == "-a") {
            //check if address parameter was already used
            if (addr_set) return errorExit("client: only one instance of one option from (-a --address) is allowed", 1);
            addr_set = true;    //address was set

            //do we have enough arguments
            if (argc < i + 2) return errorExit("client: the \"" + argument + "\" option needs 1 argument, but 0 provided", 1);
            line_args.address = string(argv[i + 1]);  //save address
            i++;                            //skip one argument as we are using it as address
        }

        //port parsing (almost the same as address)
        else if (argument == "--port" || argument == "-p") {
            if (port_set) return errorExit("client: only one instance of one option from (-p --port) is allowed", 1);
            port_set = true;

            if (argc < i + 2) return errorExit("client: the \"" + argument + "\" option needs 1 argument, but 0 provided", 1);
            line_args.port = StringToNumber(string(argv[i + 1]));

            //return if port is not a number
            if (line_args.port < 0) return errorExit("Port number is not a string", 1);
            i++;
        }

        //command and argument "parsing"
        //register and login
        else if (argument == "register" || argument == "login") {
            if (argc - (i + 1) != 2) return errorExit(argument + " <username> <password>", 1);
            line_args.command = argument;
            line_args.arguments.push_back(string(argv[i + 1]));
            line_args.arguments.push_back(string(argv[i + 2]));
            break;
        }

        //list
        else if (argument == "list" || argument == "logout") {
            if (argc - (i + 1) != 0) return errorExit(argument, 1);
            line_args.command = argument;
            break;
        }

        //send
        else if (argument == "send") {
            if (argc - (i + 1) != 3) return errorExit(argument + " <recipient> <subject> <body>", 1);
            line_args.command = argument;
            line_args.arguments.push_back(string(argv[i + 1]));
            line_args.arguments.push_back(string(argv[i + 2]));
            line_args.arguments.push_back(string(argv[i + 3]));
            break;
        }

        //fetch
        else if (argument == "fetch") {
            if (argc - (i + 1) != 1) return errorExit(argument + " <id>", 1);
            line_args.command = argument;
            line_args.arguments.push_back(string(argv[i + 1]));
            break;
        }

        //unknown command/switche
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

    if(line_args.command == "NONE") return errorExit("client: expects <command> [<args>] ... on the command line, given 0 arguments", 1);

    cout << "port: " << line_args.port << endl;
    cout << "address: " << line_args.address << endl;
    cout << "command: " << line_args.command << " ";
    for (int i = 0; i < line_args.arguments.size(); i++) {
        cout << line_args.arguments.at(i) << " ";
    }
    cout << endl;
/*
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
    printf("Please enter the message: ");
    bzero(buffer,256);
    fgets(buffer,255,stdin);
    n = write(sockfd, buffer, strlen(buffer));
    if (n < 0) 
         error("ERROR writing to socket");
    bzero(buffer,256);
    n = read(sockfd, buffer, 255);
    if (n < 0) 
         error("ERROR reading from socket");
    printf("%s\n", buffer);
    close(sockfd);
    return 0;
    */
   return 0;
}