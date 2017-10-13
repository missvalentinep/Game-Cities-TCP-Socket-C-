#include <cstdio>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

void GetIpAndPort(char *address, char *ip, char *port);

int InitializeSocket(char *ip, char *port);

void Game(int socketDescriptor);

void SendMessage(int socketDescriptor, string message);

string ReceiveMessage(int socketDescriptor);

void drawPicture();

int main(int argc, char **argv) {

    char *ipAndPort = argv[1], ipAddress[15], portNumber[10], *ip, *port;
    int socketDescriptor;

    if (argc < 2) {
        //Проверяем количество аргументов в адерсной строке
        perror("IP and Port were not entered");
        exit(1);
    }

    GetIpAndPort(ipAndPort, ip = ipAddress, port = portNumber);
    socketDescriptor = InitializeSocket(ip, port);
    Game(socketDescriptor);

    return 0;
}

void GetIpAndPort(char *address, char *ip, char *port) {
    /* Берет значение айпи и порта из адресной строки и записывает их в
     * соответствующие массивы в main()*/
    int i = 0, j = 0;

    while (address[i] != ':' && i < strlen(address)) {
        ip[i] = address[i];
        i++;
    }

    if (i == strlen(address)) {
        //Если двоеточие не найдено, то выдает соответсующую ошибку
        perror("Port number was not entered or wrong format was used.");
        exit(1);
    }

    ip[i] = '\0';
    i++;
    //После двоеточия идет номер порта
    while (i < strlen(address)) {
        port[j] = address[i];
        i++;
        j++;
    }

    port[j + 1] = '\0';

}

int InitializeSocket(char *ip, char *port) {
    /* Создает сокет, заполняем структуру для адреса сервера, устанавливает соединение
     * с сокетом сервера, возвращает дескриптор созданного сокета.*/
    int portNumber;
    int socketDescriptor;
    struct sockaddr_in serverAddress;

    if ((socketDescriptor = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error creating socket");
        exit(1);
    }

    portNumber = atoi(port);
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(portNumber);

    if (inet_aton(ip, &serverAddress.sin_addr) == 0) {
        printf("Invalid IP address\n");
        close(socketDescriptor);
        exit(1);
    }

    if (connect(socketDescriptor, (struct sockaddr *) &serverAddress,
                sizeof(serverAddress)) < 0) {
        perror("Error connecting");
        close(socketDescriptor);
        exit(1);
    }

    return socketDescriptor;
}

void Game(int socketDescriptor) {
    /* Функция игры. Обрабатывает ходы игрока и его выход из игры. */
    string messageReceive;
    string messageSend;
    string temp;

    drawPicture();
    messageReceive = ReceiveMessage(socketDescriptor);
    cout << messageReceive << endl; //Welcome to the game message from server
    cout<< "Waiting for your opponent..."<<endl;
    for (;;) {

        messageReceive = ReceiveMessage(socketDescriptor);
        cout << messageReceive << endl;

        if (messageReceive == "Your opponent left") {
            //Если противник вышел - игрок побеждает и выходит из игры
            cout<<"You win!"<<endl;
            cout<<"\\|/ ____ \\|/"<<endl;
            cout<<" @~/ ,. \\~@"<<endl;
            cout<<"/_( \\__/ )_\\  "<<endl;
            cout<<"   \\__U_/"<<endl;
            close(socketDescriptor);
            exit(0);
        }

        if (messageReceive != "You start the game! Enter any US city:" || messageReceive.length()<30) {
            //Если не первый ход, и не сообщение о том, что слов на данную букву не осталось,
            // то просит ввести город на заданную букву
            cout << "Enter a city that starts with '" << messageReceive[messageReceive.length() - 1] << "':" << endl;
        }


        cout << ">";
        getline(cin, temp);

        while (temp.length() > 50) {
            //Так как размер символьных массивов равен 50, то проверяем размер
            //введенной пользователем строки. Если он больше 50 - выводим
            //предупреждение, т.к. иначе игра сломается
            cout<<"Your guess is too big, max size: 50 symbols"<<endl;
            cout << ">";
            getline(cin, temp);
        }
        messageSend = temp;

        SendMessage(socketDescriptor, messageSend);

        if (messageSend == "exit") {
            //Если игрок ввел exit, то выходим из игры
            cout<<"You lose =("<<endl;
            cout << "Goodbye!\n";
            exit(0);
        }

        messageReceive = ReceiveMessage(socketDescriptor);

        while (messageReceive != "guess_passed") {
            //Пока слово не прошло проверки, просим ввести город
            cout << messageReceive << endl;
            messageSend="";
            cout << ">";
            getline(cin, temp);

            while (temp.length()>50){
                cout<<"Your guess is too big, max size: 50 symbols"<<endl;
                cout << ">";
                getline(cin, temp);
            }

            messageSend=temp;
            SendMessage(socketDescriptor, messageSend);

            if (messageSend == "exit") {
                cout<<"You lose =("<<endl;
                cout << "Goodbye!\n";
                close(socketDescriptor);
                exit(0);
            }

            messageReceive = ReceiveMessage(socketDescriptor);
        }
        cout << "-----------------"<<endl;
        cout << "~ Guess passed! ~"<<endl;
        cout << "-----------------"<<endl;
        cout << "Your opponent's turn..." << endl<<endl;
    }
}

void SendMessage(int socketDescriptor, string message) {
    /* Функция отправки сообщения. Сначала отправляет размер сообщения, затем само сообщение.
     * Получатель сравнивает количество полученных байт и полученный размер сообщения, и если
     * они равны - посылает подтверждение, передача прекращается. Если не равны, то сообщение
     * будет послыаться до тех пор, пока отправитель не получит нужное кол-во байтов. */
    bool allBytesReceived = false;
    int bytesSend, bytesReceived, messageLength;
    //char localMsg[50];
    
    //strcpy(localMsg, message);
    messageLength = message.length() + 1;
    
    
    if ((bytesSend = send(socketDescriptor, &messageLength, sizeof(int), 0)) < 0) {
        perror("Cant send length of message\n");
        exit(1);
    }
    
    while (!allBytesReceived) {
        
        if ((bytesSend = send(socketDescriptor, message.data(), messageLength, 0)) < 0) {
            perror("Cant send message\n");
            exit(1);
        }
        if ((bytesReceived = recv(socketDescriptor, &allBytesReceived, sizeof(bool), 0)) < 0) {
            perror("Can't receive approval\n");
            exit(1);
        }
    }
}


string ReceiveMessage(int socketDescriptor) {
    /* Функция получения сообщений. Получает размерность сообщения, затем само сообщение.
     * Сравнивает количество полученных байт и размерность, и если они равны, то посылает
     * подтверждение. Возвращает указатель на полученное сообщение.*/
    int messageLength;
    int bytesReceived, bytesSend;
    string message;
    char buffer[50];
    bool allBytesReceived = false;
    
    
    if ((bytesReceived = recv(socketDescriptor, &messageLength, sizeof(int), 0)) < 0) {
        perror("Can\'t read message length\n");
        exit(1);
    }
    
    while (!allBytesReceived) {
        
        if ((bytesReceived = recv(socketDescriptor, &buffer, messageLength, 0)) < 0) {
            perror("Can\'t read message\n");
            exit(1);
        }
        message.append(buffer);
        if (bytesReceived == messageLength) allBytesReceived = true;
        
        if ((bytesSend = send(socketDescriptor, &allBytesReceived, sizeof(bool), 0)) < 0) {
            perror("Cant send approval\n");
            exit(1);
        }
    }
    
    return message;
}

void drawPicture(){
    /*Рисует картинку города :) */
    cout<<endl;
    cout<<endl;
    cout<<"                      ~Welcome to the game of Cities!~ "<<endl;
    cout<<endl;
    cout<<endl;
    cout<<"                                 t__n__r__  "<<endl;
    cout<<"                                | ####### |"<<endl;
    cout<<"                       ___      | ####### |             ____i__           /"<<endl;
    cout<<"            _____p_____l_l____  | ####### |            | ooooo |         qp"<<endl;
    cout<<" i__p__    |  ##############  | | ####### |__l___xp____| ooooo |      |~~~~|"<<endl;
    cout<<"| oooo |_I_|  ##############  | | ####### |oo%Xoox%ooxo| ooooo |p__h__|##%#|"<<endl;
    cout<<"| oooo |ooo|  ##############  | | ####### |o%xo%%xoooo%| ooooo |      |#xx%|"<<endl;
    cout<<"| oooo |ooo|  ##############  | | ####### |o%ooxx%ooo%%| ooooo |######|x##%|"<<endl;
    cout<<"| oooo |ooo|  ##############  | | ####### |o%ooxx%ooo%%| ooooo |######|x##%|"<<endl;
    cout<<"| oooo |ooo|  ##############  | | ####### |o%ooxx%ooo%%| ooooo |######|x##%|"<<endl;
    cout<<"| oooo |ooo|  ##############  | | ####### |oo%%x%oo%xoo| ooooo |######|##%x|"<<endl;
    cout<<"| oooo |ooo|  ##############  | | ####### |%x%%oo%/oo%o| ooooo |######|/#%x|"<<endl;
    cout<<"| oooo |ooo|  ##############  | | ####### |oox%%o/x%%ox| ooooo |~~~$~~|x##/|"<<endl;
    cout<<"| oooo |ooo|  ##############  | | ####### |x%oo%x/o%//x| ooooo |_KKKK_|#x/%|"<<endl;
    cout<<"| oooo |ooo|  ##############  | | ####### |oox%xo%%oox%| ooooo |_|~|~ |xx%/|"<<endl;
    cout<<"| oooo |oHo|  ####AAAA######  | | ##XX### |x%x%WWx%%/ox||ooDoo |_| |~||xGGx|"<<endl;
    cout<<" ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"<<endl;
    cout<<endl;
    cout<<endl;
}
