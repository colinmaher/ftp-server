// $Id: cix.cpp,v 1.6 2018-07-26 14:18:32-07 - - $

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

#include <libgen.h>
#include <sys/types.h>
#include <unistd.h>

#include "logstream.h"
#include "protocol.h"
#include "sockets.h"

logstream log(cout);
struct cix_exit : public exception {
};

unordered_map<string, cix_command> command_map{
    { "get", cix_command::GET },
    { "put", cix_command::PUT },
    { "rm", cix_command::RM },
    { "exit", cix_command::EXIT },
    { "help", cix_command::HELP },
    { "ls", cix_command::LS },
};

static const string help = R"||(
exit         - Exit the program.  Equivalent to EOF.
get filename - Copy remote file to local host.
help         - Print help summary.
ls           - List names of files on remote server.
put filename - Copy local file to remote host.
rm filename  - Remove file from remote server.
)||";

void cix_help()
{
    cout << help;
}

void cix_ls(client_socket& server)
{
    cix_header header;
    header.command = cix_command::LS;
    log << "sending header " << header << endl;
    send_packet(server, &header, sizeof header);
    recv_packet(server, &header, sizeof header);
    log << "received header " << header << endl;
    if (header.command != cix_command::LSOUT) {
        log << "sent LS, server did not return LSOUT" << endl;
        log << "server returned " << header << endl;
        log << "ls: '" << header.filename << "' " 
            << strerror(header.nbytes) << endl;
    } else {
        char buffer[header.nbytes + 1];
        recv_packet(server, buffer, header.nbytes);
        log << "received " << header.nbytes << " bytes" << endl;
        buffer[header.nbytes] = '\0';
        cout << buffer;
    }
}

void cix_get(client_socket& server, string filename)
{
    cix_header header;
    header.command = cix_command::GET;
    for (string::iterator it = filename.begin();
    it != filename.end(); ++it) {
        header.filename[it - filename.begin()] = *it;
    }
    cout << "filename: " << header.filename << endl;
    log << "sending header " << header << endl;
    send_packet(server, &header, sizeof header);
    recv_packet(server, &header, sizeof header);
    log << "received header " << header << endl;
    if (header.command != cix_command::FILE) {
        log << "sent GET, server did not return FILE" << endl;
        log << "server returned " << header << endl;
        log << "get: '" << header.filename << "' " 
            << strerror(header.nbytes) << endl;
    } else {
        char buffer[header.nbytes + 1];
        recv_packet(server, buffer, header.nbytes);
        log << "received " << header.nbytes << " bytes" << endl;
        buffer[header.nbytes] = '\0';
        cout << buffer;
        ofstream os(header.filename);
        os.write(buffer, header.nbytes);
        os.close();
    }
}

void cix_put(client_socket& server, string filename)
{
    cix_header header;
    header.command = cix_command::PUT;
    for (string::iterator it = filename.begin();
    it != filename.end(); ++it) {
        header.filename[it - filename.begin()] = *it;
    }
    ifstream is(header.filename);
    stringstream buffer;
    buffer << is.rdbuf();
    string str_buff = buffer.str();
    header.nbytes = str_buff.size();
    log << "sending header " << header << endl;
    send_packet(server, &header, sizeof header);
    send_packet(server, str_buff.c_str(), str_buff.size());
    recv_packet(server, &header, sizeof header);
    log << "received header " << header << endl;
    if (header.command != cix_command::ACK) {
        log << "sent PUT, server did not return ACK" << endl;
        log << "server returned " << header << endl;
        log << "put: '" << header.filename << "' " 
            << strerror(header.nbytes) << endl;
    } else { 
        log << "received " << header.nbytes << " bytes" << endl;
    }
}

void cix_rm(client_socket& server, string filename)
{
    cix_header header;
    header.command = cix_command::RM;
    for (string::iterator it = filename.begin();
    it != filename.end(); ++it) {
        header.filename[it - filename.begin()] = *it;
    }
    log << "sending header " << header << endl;
    send_packet(server, &header, sizeof header);
    recv_packet(server, &header, sizeof header);
    log << "received header " << header << endl;
    if (header.command != cix_command::ACK) {
        log << "sent RM, server did not return ACK" << endl;
        log << "server returned " << header << endl;
        log << "rm: '" << header.filename << "' " 
            << strerror(header.nbytes) << endl;
    } else {

        log << "received " << header.nbytes << " bytes" << endl;
    }
}

void usage()
{
    cerr << "Usage: " << log.execname() << " [host] [port]" << endl;
    throw cix_exit();
}

int main(int argc, char** argv)
{
    log.execname(basename(argv[0]));
    log << "starting" << endl;
    vector<string> args(&argv[1], &argv[argc]);
    if (args.size() > 2)
        usage();
    string host = get_cix_server_host(args, 0);
    in_port_t port = get_cix_server_port(args, 1);
    log << to_string(hostinfo()) << endl;
    try {
        log << "connecting to " << host << " port " << port << endl;
        client_socket server(host, port);
        log << "connected to " << to_string(server) << endl;
        for (;;) {
            string line;
            getline(cin, line);
            if (cin.eof())
                throw cix_exit();
            log << "command " << line << endl;
            stringstream ss(line);
            vector<string> tokens;
            string token;
            while (getline(ss, token, ' ')) {
                tokens.push_back(token);
            }
            const auto& itor = command_map.find(tokens[0]);
            cix_command cmd = itor == command_map.end()
                ? cix_command::ERROR
                : itor->second;
            switch (cmd) {
            case cix_command::GET:
                cix_get(server, tokens[1]);
                break;
            case cix_command::PUT:
                cix_put(server, tokens[1]);
                break;
            case cix_command::RM:
                cix_rm(server, tokens[1]);
                break;
            case cix_command::EXIT:
                throw cix_exit();
                break;
            case cix_command::HELP:
                cix_help();
                break;
            case cix_command::LS:
                cix_ls(server);
                break;
            default:
                log << line << ": invalid command" << endl;
                break;
            }
        }
    } catch (socket_error& error) {
        log << error.what() << endl;
    } catch (cix_exit& error) {
        log << "caught cix_exit" << endl;
    }
    log << "finishing" << endl;
    return 0;
}
