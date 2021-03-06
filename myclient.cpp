//
// Created by ken on 6/17/18.
//
#include <errno.h>
#include <iostream>
#include <cstring>
#include <string>
#include <netdb.h>
#include "MyProtocol.h"
#include <stdio.h>
#include <vector>
#include <queue>
#include <sys/types.h>
#include <netinet/tcp.h>
using namespace std;
int keepalive = 1; // 打开keepalive

int keepidle = 10; // 空闲10s开始发送检测包（系统默认2小时）Linux下单位是秒

int keepinterval = 1; // 发送检测包间隔 （系统默认75s）

int keepcount = 5; // 发送次数如果5次都没有回应，就认定peer端断开了。（系统默认9次）

int debug = 0;
unsigned char recv_buffer[READ_BUFFER_SIZE];

int client_fd;
client_info list[MAX_CLIENT_NUM];
void GetTime(int state, int source_fd, int dest_fd);

void GetName(int, int, int);

void GetList(int state, int fd, int i);

void SendMessage(int state, int source_fd, int dest_fd, int tr_fd);

void Connect(int &client_fd, struct sockaddr_in &server_address)
{
    int check = connect(client_fd, (struct sockaddr *)&server_address, sizeof(server_address));
    if (check < 0)
        perror("Connect failed\n");
}
void *threadFunction(void *args);
deque<Packet> packet_queue;

int main(int argc, char const *argv[])
{
    int port_num;
    int state = 0;
    struct sockaddr_in server_address;
    struct hostent *server;

    if (argc < 3)
    {
        cerr << "Syntax : ./client <host name> <port>" << endl;
        return 0;
    }

    port_num = atoi(argv[2]);

    if ((port_num > 65535) || (port_num < 2000))
    {
        cerr << "Please enter port number between 2000 - 65535" << endl;
        return 0;
    }

    server = gethostbyname(argv[1]);

    if (server == NULL)
    {
        cerr << "Host does not exist" << endl;
        return 0;
    }
    memset((char *)&server_address, 0, sizeof(server_address)); //initial
    server_address.sin_family = AF_INET;
    memmove((char *)&server_address.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
    //bcopy((char *)server->h_addr, (char *) &server_address.sin_addr.s_addr, server->h_length);

    server_address.sin_port = htons(port_num);
    cout << "************************************\n";
    cout << "*   Welcome to use this client!    *\n";
    cout << "************************************\n";
    cout << "Commander 1: connect the server\n";
    cout << "Commander 2: disconnect\n";
    cout << "Commander 3: get time\n";
    cout << "Commander 4: get server name\n";
    cout << "Commander 5: get client list\n";
    cout << "Commander 6: send message\n";
    cout << "Commander 7: exit\n";
    cout << "Commander 8: send 500 times \n";
    cout << "************************************\n";
    cout << "Please input your commander:\n";

    pthread_t childthread;

    int commander = 0;
    int flag = 1;
    while (flag)
    {
        cin >> commander;

        switch (commander)
        {
        case 1:
        { //connect
            //create client socket
            client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
            setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPALIVE, &keepidle, sizeof(keepidle));
            setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepinterval, sizeof(keepinterval));
            setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcount, sizeof(keepcount));
            
            if (client_fd < 0)
            {
                cerr << "Cannot open socket" << endl;
                return 0;
            }

            Connect(client_fd, server_address);
            state = 1;
            pthread_create(&childthread, nullptr, threadFunction, (void *)&client_fd);
            cout << "Connect success.\n";
            break;
        }
        case 2:
        { //disconnect
            if (state == 0)
                cout << "No connection\n";
            else
            {
                close(client_fd);
                pthread_cancel(childthread);
                cout << "Close socket success\n";
            }
            break;
        }
        case 3:
        { //get time
            GetTime(state, client_fd, -1);

            while (packet_queue.empty())
                ;

            if (packet_queue.empty())
            {
                cout << "no data" << endl;
            }
            time_t valread = -1;

            auto iter = packet_queue.begin();
            for (; iter != packet_queue.end();)
            {
                if (iter->header.op == TIME)
                {
                    valread = *(time_t *)(iter->body.data);
                    //!!ATTENTION!! erase method will influence iterator; so if use erase,must break; or you will get segmentation fault
                    // write like this way to avoid such fault;
                    packet_queue.erase(iter);
                    break;
                }
                else
                    iter++;
            }

            if (valread == -1)
                cout << "No time packet received" << endl;

            else
                cout << ctime(&valread) << endl;

            if (debug)
                cout << "fuck hey" << endl;

            break;
        }
        case 4:
        { //get name
            GetName(state, client_fd, -1);

            while (packet_queue.empty())
                ;

            if (packet_queue.empty())
            {
                cout << "No data" << endl;
            }
            string name = "";
            auto iter = packet_queue.begin();
            for (; iter != packet_queue.end();)
            {
                if (iter->header.op == NAME)
                {
                    name = (char *)(iter->body.data);
                    packet_queue.erase(iter);
                    break;
                }
                else
                    iter++;
            }

            if (name == "")
                cout << "No name packet received or name is empty" << endl;
            else
                cout << "name is: " << name << endl;

            if (debug)
                cout << "fuck hey" << endl;
            break;
        }

        case 5:
        { //get client list

            //
            //
            GetList(state, client_fd, -1);

            if (debug)
                cout << "damn1:" << *(int *)(((Packet *)recv_buffer)->body.data) << endl;

            while (packet_queue.empty())
                ;

            if (packet_queue.empty())
            {
                cout << "no data" << endl;
            }
            int n = -1;
            cout << "************\n";
            cout << "Active client list:\n";
            cout << "************\n";
            auto iter = packet_queue.begin();
            for (; iter != packet_queue.end();)
            {
                if (iter->header.op == ACTIVE_LIST)
                {
                    n = *(int *)(iter->body.data);
                    for (int i = 0; i < n; ++i)
                    {
                        if (iter->body.list[i].isThisMyfd == 1)
                            cout << "num: " << iter->body.list[i].num << " (me)" << endl;
                        else
                            cout << "num: " << iter->body.list[i].num << endl;
                        cout << "ip: " << iter->body.list[i].ip << endl;
                        cout << "port: " << iter->body.list[i].port << endl;
                        cout << "************\n";
                    }
                    packet_queue.erase(iter);
                    break;
                }
                else
                    iter++;
            }

            if (n == -1)
                cout << "No name packet received or name is empty" << endl;
            else
                cout << "Total number of client is: " << n << endl;
            if (debug)
                cout << "fuck hey" << endl;

            break;
        }
        case 6:
        { //send message

            cout << "Please input your fd(your id listed by commander 5):\n";
            int tr_fd = -1;
            cin >> tr_fd;
            cout << "Please input destination fd:\n";
            int dest_fd = -1;
            cin >> dest_fd;
            SendMessage(state, client_fd, dest_fd, tr_fd);
            if (debug)
                cout << "aaa\n";
            while (packet_queue.empty())
                ;
            if (debug)
                cout << "bbb\n";
            if (packet_queue.empty())
            {
                cout << "no data" << endl;
            }
            string isOk = "";
            auto iter = packet_queue.begin();
            for (; iter != packet_queue.end();)
            {

                if (iter->header.op == MESSAGE && iter->header.type == 0)
                {
                    isOk = "ok";
                    if (iter->header.source == dest_fd && isOk == "ok")
                        cout << "Get message done!\n";

                    packet_queue.erase(iter);
                    break;
                }
                else if (iter->header.type == 2)
                {
                    isOk = "error";
                    cout << "No such client linked\n"
                         << endl;
                    packet_queue.erase(iter);
                    break;
                }
                else
                    iter++;
            }

            if (isOk == "")
                cout << "No send packet received " << endl;

            else if (isOk == "error")
                cout << "Use commander 5 to find the linked client " << endl;
            else
                cout << "Get a reply. Send success." << endl;

            if (debug)
                cout << "damn3:" << isOk << endl;

            if (debug)
                cout << "fuck hey" << endl;

            break;
        }
        case 7:
        { //exit
            close(client_fd);
            pthread_cancel(childthread);
            cout << "Exit success.\n";
            flag = 0;
            break;
        }
        case 8:
        {
            for (int i = 0; i < 500; i++)
            {
                GetTime(state, client_fd, -1);
            }
            cout << "Send 100 time request\n";
            int count = 0;
            for (int i = 0; i < 500; i++)
            {
                //while (packet_queue.empty());

                if (packet_queue.empty())
                {
                    cout << "no data" << endl;
                }
                time_t valread = -1;

                auto iter = packet_queue.begin();
                for (; iter != packet_queue.end();)
                {
                    if (iter->header.op == TIME)
                    {
                        valread = *(time_t *)(iter->body.data);
                        count++;
                        //!!ATTENTION!! erase method will influence iterator; so if use erase,must break; or you will get segmentation fault
                        // write like this way to avoid such fault;
                        packet_queue.erase(iter);
                        break;
                    }
                    else
                        iter++;
                }

                if (valread == -1)
                    cout << "No time packet received" << endl;

                else
                    cout << ctime(&valread) << endl;

                if (debug)
                    cout << "fuck hey" << endl;
            }

            cout << "Got " << count << " time reply." << endl;

            break;
        }

        default:
        {
            cout << "invalid commander!\n";
            break;
        }
        }
        if (flag == 0)
            break;
    }
    //pthread_cancel(childthread);
    return 0;
}

void *threadFunction(void *args)
{

    while (1)
    {

        int flag = 1;

        if (debug)
            cout << "rece begin\n";
        memset(recv_buffer, 0, READ_BUFFER_SIZE);

        int head_total = sizeof(Packet);
        int received = 0;
        int bytes = 0;
        int sockfd = *(int *)args;
        //cout<<"fd:"<<sockfd<<endl;
        if (debug)
            cout << "rece 1\n";
        Packet temp;
        do
        {
            if (debug)
                cout << "rece 1111\n";
            bytes = read(sockfd, &temp, head_total - received);
            if (debug)
                cout << "read type" << temp.header.op << endl;
            if (debug)
                cout << "bytes:" << bytes << endl;
            if (bytes < 0)
            {
                perror("ERROR reading recv1_buffer from socket");
                flag = 0;
                break;
            }
            if (bytes == 0) //the server is closed
            {
                cout << "the server is closed!!!!!!!!!!!!!\n";
                flag = 0;
                break;
            }
            received += bytes;
        } while (received < head_total);

        Packet *phead = (Packet *)recv_buffer;
        int data_length = phead->header.length - sizeof(PacketHeader);
        if (debug)
            cout << "time le" << data_length << endl;

        if (debug)
            printf("datais:%s\n", phead->body.data);
        if (data_length > 0 && data_length > sizeof(Packet) - sizeof(PacketHeader))
        { //need extra read
            received = 0;
            if (debug)
                cout << "extra read\n";
            do
            {
                bytes = read(sockfd, recv_buffer + head_total + received, data_length - received);

                if (bytes < 0)
                    perror("ERROR reading recv123_buffer from socket");
                if (bytes == 0) //the server is closed
                {
                    break;
                }
                received += bytes;

            } while (received < data_length);
        }

        if (debug)
            printf("datais:%s\n", phead->body.data);
        if (debug)
            cout << "rece done\n";

        packet_queue.push_front(temp);
        if (debug)
            cout << "type:" << packet_queue.front().header.op << endl;
        // here handle when get a message request; it's better to be handled in the main thread; but I don't have a good idea as it doesn't read any keyboard input; Maybe create a new thread or just put it here
        if (packet_queue.front().header.op == MESSAGE && packet_queue.front().header.type == 1)
        {
            if (debug)
                cout << "motherfucker\n";
            Packet to_send(packet_queue.front().header.destination, packet_queue.front().header.source, sizeof(Packet), 0, MESSAGE, nullptr);
            strncpy((char *)to_send.body.data, "ok", strlen("ok") + 1); //copy data
            send(client_fd, &to_send, sizeof(Packet), 0);
            cout << "destination:" << to_send.header.destination << "src " << to_send.header.source << " reply succuess\n";
            packet_queue.pop_front();
        }
        if (!flag)
            break;
    }
}

void GetTime(int state, int source_fd, int dest_fd)
{
    if (state == 0)
    {
        perror("Connect first!\n");
        return;
    }

    Packet to_send(source_fd, dest_fd, sizeof(Packet), 1, TIME, nullptr);
    send(source_fd, &to_send, sizeof(Packet), 0);

    if (debug)
        cout << "send success\n";
}

void SendMessage(int state, int source_fd, int dest_fd, int true_s_fd)
{
    if (state == 0)
    {
        perror("Connect first!\n");
        return;
    }

    Packet to_send(true_s_fd, dest_fd, sizeof(Packet), 1, MESSAGE, nullptr);
    if (debug)
        cout << "babababab" << true_s_fd << "  des:" << dest_fd << endl;
    send(source_fd, &to_send, sizeof(Packet), 0);

    if (debug)
        cout << "send success\n";
}

void GetList(int state, int source_fd, int dest_fd)
{
    if (state == 0)
    {
        perror("Connect first!\n");
        return;
    }

    Packet to_send(source_fd, dest_fd, sizeof(Packet), 1, ACTIVE_LIST, nullptr);
    send(source_fd, &to_send, sizeof(Packet), 0);

    if (debug)
        cout << "send success\n";
}

void GetName(int state, int source_fd, int dest_fd)
{
    if (state == 0)
    {
        perror("Connect first!\n");
        return;
    }

    Packet to_send(source_fd, dest_fd, sizeof(Packet), 1, NAME, nullptr);
    send(source_fd, &to_send, sizeof(Packet), 0);

    if (debug)
        cout << "send success\n";
}