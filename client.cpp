#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

using namespace std;

/*----(macros)----*/
#define DEFAULT_ADDRESS "localhost"
#define DEFAULT_PORT "32323"
#define MAX_BUFFER_SIZE 256
#define SESSION_HASH_FILE "login-token"

/*----(structs)----*/
/**
 * @brief main struct for arguments and commands
 * 
 */
struct LineArgs{
    string address = DEFAULT_ADDRESS;
    string port = DEFAULT_PORT;
    string command = "NONE";
    vector<string> arguments;
};


/*----(Exit handeling)----*/
/** @brief signal callback for ctrl+c
 * 
 * @param signal 
 */
void signalCallback(int signal) {
    cerr << "Process interrupted by the user" << endl;
    exit(1);
}

/** @brief Helper function for easier error message printing
 * 
 * @param msg message to be printed
 * @param err_code error which will be returned
 * @return err_code
 */
int errorExit(string msg, int err_code) {
    cerr << msg << endl;
    return err_code; //exit(err_code);
}


/*----(base 64)----*/
/** @brief Base64 encoding (taken from: https://stackoverflow.com/a/37109258)
 * 
 * @param s string to encode
 * @return string encoded in base64
 */
string base64Encode(string s) {
    unsigned char base64_table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    int s_len       = s.length();      //save string length
    int s_left      = s_len % 3;       //how manny extra characters are left
    int s_loop_end  = s_len - s_left;  //end of looped encoding
    int s_index;

    int val = 0;

    //calculate remaining space that has to be filled with padding
    if (s_len % 3)
        s_len += (3 - (s_len % 3));
    string encoded(4 * (s_len) / 3, '='); //fill encoded string with padding
    int e_index = 0;                      //current index inside encoded string

    //fill encoded string as far as we can
    for (s_index = 0; s_index < s_loop_end; s_index += 3) {
        val = int(s[s_index]) << 16 | int(s[s_index + 1]) << 8 | s[s_index + 2]; //take 3 letters, shift them and save all of them to single variable
        encoded[e_index++] = base64_table[val >> 18           ];                 //get first sextet
        encoded[e_index++] = base64_table[val >> 12 & 0b111111];                 //get second
        encoded[e_index++] = base64_table[val >> 6  & 0b111111];                 //third
        encoded[e_index++] = base64_table[val       & 0b111111];                 //fourth    (3 * 8 = 4 * 6)
    }

    //take care of the remaining characters
    if (s_left == 2) {
        val = int(s[s_loop_end]) << 8 | int(s[s_loop_end + 1]);
        encoded[e_index++] = base64_table[val >> 10 & 0b111111]; //18 - 8 = 10
        encoded[e_index++] = base64_table[val >> 4  & 0b111111];
        encoded[e_index++] = base64_table[val << 2  & 0b111111];
    }
    if (s_left == 1) {
        val = int(s[s_loop_end]);
        encoded[e_index++] = base64_table[val >> 2           ]; //18 - 8 - 8 = 2
        encoded[e_index++] = base64_table[val << 4 & 0b111111];
    }

    return encoded;
}


/*----(file operations)----*/
/** @brief Save login/session hash to file
 * 
 * @param hash string which should be saved
 * @return returns 0 on success and 1 on failure
 */
int saveHash(string hash) {
    ofstream hash_file(SESSION_HASH_FILE, ofstream::trunc); //open file in overwrite mode
    if (hash_file.fail())
        return 1;
    hash_file << hash;
    hash_file.close();
    return 0;
}

//load hash from file
/** @brief Load session hash from file
 * 
 * @return string containing first line from file
 */
string loadHash() {
    string session_hash;
    ifstream hash_file(SESSION_HASH_FILE);
    if (hash_file.fail())
        return ".";                   //if something went wrong, return character that can't be created via base64 encoding
    getline(hash_file, session_hash); //get hash from file
    return session_hash;
}


/*----(String operations)----*/
/** @brief Split string by unescaped quotes
 * 
 * @param s string which should be split
 * @return vector<string> containing individual splits
 */
vector<string> splitByQuotes(string s) {
    vector<string> splits;
    int start_index = -1;

    //loop string and search for string in quotes
    for (int i = 0; i < s.length(); i++) {
        //skip slashes
        if (s.substr(i, 2) == "\\\\" || s.substr(i, 2) == "\\\"")
            i++;
        //if quote was found, set it either as start or end of the string
        else if (s.substr(i, 1) == "\"") {
            if (start_index != -1) {
                splits.push_back(s.substr(start_index, i - start_index + 1));
                start_index = -1;
            }
            else
                start_index = i;
        }
    }

    return splits;
}

/** @brief Replace any text with any other text in string
 * 
 * @param s string where we want to replace
 * @param x substring which should be replaced
 * @param y substring which we will replace it with
 * @return string with X replaced by Y
 */
string replaceInString(string s, string x, string y) {
    int pos = 0;
    while ((pos = s.find(x, pos)) != string::npos) {
        s.replace(pos, x.length(), y);
        pos = pos + y.length();  //skip the newly escaped character
    }
    return s;
}


/*----(networking)----*/
/** @brief Setup socket, host info and connect to server
 * 
 * @param *socket_fd pointer to socket file descriptor 
 * @param address server address (ipv4, ipv6, domain name)
 * @param port service port
 * @return returns 0 on success, 1 on failure
 */
int setupAndConnect(int *socket_fd, string address, string port) {
    struct addrinfo *res, hint = {0};
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    int rc = -1;

    //get host info
    if (getaddrinfo(address.c_str(), port.c_str(), &hint, &res) != 0) {
        return errorExit("Falied to get host '" + address + "'", 1);
    }

    //go through all possible hosts and get the info
    for(; res != NULL; res = res->ai_next) {

        //if we get ipv4 or ipv6, create socket and try to connect
        if (res->ai_family == AF_INET6 || res->ai_family == AF_INET) {
            *socket_fd = socket(res->ai_family, SOCK_STREAM, 0);
            if (*socket_fd < 0) 
                return errorExit("Failed to create socket", 1);
            if ((rc = connect(*socket_fd, (struct sockaddr *) res->ai_addr, res->ai_addrlen)) < 0)
                continue;
            break;
        }
    }

    freeaddrinfo(res); //free host info

    if (rc < 0) {
        close(*socket_fd);
        return errorExit("Failed to connect to host '" + address + "'", 1);
    }
    
    return 0;
}

/** @brief Send data to server
 * 
 * @param socket_fd socket file descriptor
 * @param command command to send
 * @param arguments arguments vector
 * @return returns 0 on success, 1 on failure
 */
int mySend(int socket_fd, string command, vector<string> arguments) {
    string request; //request payload

    request = "(" + command; //add command
    
    //add hash only on supported commands
    if (command != "login" && command != "register") {
        string hash = loadHash();
        if (hash == ".") {
            close(socket_fd);
            return errorExit("Not logged in", 1);
        }
        request += " " + hash;
    }

    //add arguments to payload (fetch is special)
    if (command == "fetch") 
        request += " " + arguments[0];
    else {
        for (int i = 0; i < arguments.size(); i++)
            request += " \"" + arguments[i] + "\"";
    }
    request += ")";

    //write request to socket
    if (write(socket_fd, request.c_str(), request.length()) < 0) {
        close(socket_fd);
        return errorExit("Failed writing to socket", 1);
    }

    return 0;
}

/** @brief Receive data from server
 * 
 * @param socket_fd socket file descriptor
 * @param *response pointer to string where server response will be stored
 * @return returns 0 on success, 1 on failure
 */
int myReceive(int socket_fd, string *response) {
    int rc;
    char buffer[MAX_BUFFER_SIZE] = {0};
    //read from socket
    do {
        rc = read(socket_fd, buffer, MAX_BUFFER_SIZE - 1);  //read
        *response += string(buffer);                        //save
        memset(buffer, 0, MAX_BUFFER_SIZE);                 //clear
    } while(rc > 0);

    if (rc < 0) {
        close(socket_fd);
        return errorExit("Failed reading from socket", 1);
    }
    
    return 0;
}


/*----(response parsing)----*/
/** @brief Parse response and print it
 * 
 * @param response string containing server resposne
 * @param command client's command which was used to invoke this response
 * @return returns 0 on success, 1 on failure
 */
int printResponse(string response, string command) {
    vector<string> splits = splitByQuotes(response);  //split response by unescaped quotes

    //un-escape everything
    for (int i = 0; i < splits.size(); i++){
        splits[i] = splits[i].substr(1, splits[i].length() - 2);    //remove quotes, as they are no longer neede and would only cause trouble
        for (int j = 0; j < splits[i].length(); j++) {
            //un-escape '\'
            if (splits[i].substr(j, 2) == "\\\\")
                splits[i].replace(j, 2, "\\");
            //un-escape '\n'
            else if (splits[i].substr(j, 2) == "\\n")
                splits[i].replace(j, 2, "\n");
            //un-escape '"'
            else if (splits[i].substr(j, 2) == "\\\"")
                splits[i].replace(j, 2, "\"");
        }
    }

    if (response.substr(1, 2) == "ok") {
        //command specific parsing
        if (command == "logout") {
            if (remove("login-token"))    //remove token file
                return errorExit("ERROR failed to remove login token", 1);
            cout << "SUCCESS: " << splits[0] << endl;
        }

        else if (command == "login") {
            if (saveHash("\"" + splits[1] + "\""))
                return errorExit("ERROR failed to save login token", 1);
            cout << "SUCCESS: " << splits[0] << endl;
        }

        else if (command == "fetch") {
            cout << "SUCCESS: " << endl << endl;
            cout << "From: " << splits[0] << endl;
            cout << "Subject: " << splits[1] << endl;
            cout << endl;
            cout << splits[2];
        }

        else if (command == "list") {
            cout << "SUCCESS: " << endl;
            for (int i = 0; i < splits.size(); i += 2) {
                cout << i / 2 + 1 << ":" << endl;
                cout << "  From: " << splits[i] << endl;
                cout << "  Subject: " << splits[i+1] << endl;
            }
        }

        //payloads without splits
        else
            cout << "SUCCESS: " << splits[0] << endl;
    }
    else if (response.substr(1, 3) == "err")
        cout << "ERROR: " << splits[0] << endl;
    else
        return errorExit("Unknown response", 1);

    return 0;
}


//get arguments, commands and so on
/*----(argument parsing)----*/
/** @brief Inut arguments parsing
 * 
 * @param argc main argc
 * @param argv main argv
 * @param line_args struct to save arguments and commands
 * @return returns 0 on success, 1 on failure
 */
int parseArguments(int argc, char *argv[], LineArgs *line_args) {
    bool help = false;
    bool addr_set = false;
    bool port_set = false;

    //loop through all arguments (skip first one)
    for (int i = 1; i < argc; i++) {
        string argument = string(argv[i]);  //save current argument

        //help parsing
        if (argument == "--help" || argument == "-h") help = true;

        //address parsing
        else if (argument == "--address" || argument == "-a") {
            //check if address parameter was already used
            if (addr_set)
                return errorExit("client: only one instance of one option from (-a --address) is allowed", 1);
            addr_set = true;    //address was set

            //do we have enough arguments
            if (argc < i + 2)
                return errorExit("client: the \"" + argument + "\" option needs 1 argument, but 0 provided", 1);
            line_args->address = string(argv[i + 1]);  //save address
            i++;                            //skip one argument as we are using it as address
        }

        //port parsing (almost the same as address)
        else if (argument == "--port" || argument == "-p") {
            if (port_set)
                return errorExit("client: only one instance of one option from (-p --port) is allowed", 1);
            port_set = true;

            if (argc < i + 2)
                return errorExit("client: the \"" + argument + "\" option needs 1 argument, but 0 provided", 1);
            line_args->port = string(argv[i + 1]);  //save port

            //check that port is a number, otherwise exit
            if (line_args->port.find_first_not_of("0123456789") != string::npos)
                return errorExit("Port number is not a string", 1);
            i++;
        }

        //command and argument "parsing"
        else if (argument == "register" || argument == "login") {
            if (argc - (i + 1) != 2)
                return errorExit(argument + " <username> <password>", 1);
            line_args->command = argument;
            line_args->arguments.push_back(string(argv[i + 1]));
            line_args->arguments.push_back(base64Encode(string(argv[i + 2])));
            break;
        }

        else if (argument == "list" || argument == "logout") {
            if (argc - (i + 1) != 0)
                return errorExit(argument, 1);
            line_args->command = argument;
            break;
        }

        else if (argument == "send") {
            if (argc - (i + 1) != 3)
                return errorExit(argument + " <recipient> <subject> <body>", 1);
            line_args->command = argument;
            line_args->arguments.push_back(string(argv[i + 1]));
            line_args->arguments.push_back(string(argv[i + 2]));
            line_args->arguments.push_back(string(argv[i + 3]));
            break;
        }

        else if (argument == "fetch") {
            if (argc - (i + 1) != 1)
                return errorExit(argument + " <id>", 1);
            line_args->command = argument;
            line_args->arguments.push_back(string(argv[i + 1]));
            break;
        }

        //unknown command/switch
        else {
            if (argument[0] == '-')
                return errorExit("client: unknown switch: " + argument, 1);
            return errorExit("unknown command", 1);
        }
    }

    //print help
    if (help) {
        cout << "usage: client [ <option> ... ] <command> [<args>] ...\n" << endl;
        cout << "<option> is one of\n" << endl;
        cout << "  -a <addr>, --address <addr>" << endl;
        cout << "     Server hostname or address to connect to" << endl;
        cout << "  -p <port>, --port <port>" << endl;
        cout << "     Server port to connect to" << endl;
        cout << "  --help, -h" << endl;
        cout << "     Show this help" << endl << endl;
        cout << " Supported commands:" << endl;
        cout << "   register <username> <password>" << endl;
        cout << "   login <username> <password>" << endl;
        cout << "   list" << endl;
        cout << "   send <recipient> <subject> <body>" << endl;
        cout << "   fetch <id>" << endl;
        cout << "   logout" << endl;
        return 0;
    }

    if(line_args->command == "NONE")
        return errorExit("client: expects <command> [<args>] ... on the command line, given 0 arguments", 1);
    
    //escape required characters
    for (int i = 0; i < line_args->arguments.size(); i++) {
        line_args->arguments[i] = replaceInString(line_args->arguments[i], "\\", "\\\\"); //escape '\'
        line_args->arguments[i] = replaceInString(line_args->arguments[i], "\"", "\\\""); //escape '"'
        line_args->arguments[i] = replaceInString(line_args->arguments[i], "\n", "\\n"); //escape '\n'; in testing, the new line was always somehow escaped automatically
    }
    return -1;
}


/*----(main)----*/
int main(int argc, char *argv[]) {
    signal (SIGINT, signalCallback);
    LineArgs line_args;
    int rc, socket_fd;
    string response;

    //get arguments
    if ((rc = parseArguments(argc, argv, &line_args)) >= 0)
        return rc;

    //connect
    if ((rc = setupAndConnect(&socket_fd, line_args.address, line_args.port)) > 0)
        return rc;
    
    //send request
    if ((rc = mySend(socket_fd, line_args.command, line_args.arguments)) > 0)
        return rc;

    //get response
    if ((rc = myReceive(socket_fd, &response)) > 0)
        return rc;
    
    close(socket_fd);
    
    //print response
    if ((rc = printResponse(response, line_args.command)) > 0)
        return rc;
}