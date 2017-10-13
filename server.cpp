#include <cstdio>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sys/shm.h>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <algorithm>
#include <sys/wait.h>

using namespace std;

const int TOTAL_NUM_OF_CITIES = 1681;

struct city {
    string name;
    bool used;
    int firstLetterIndex;
};

struct sharedMemoryStruct {
    int connectedPlayers;
    int turn;
    char lastWord[50];
    vector<city> listOfCities;
    int firstLettersArray[25];


};

int GetSharedMemoryId();

void InitializeFirstLettersArray(sharedMemoryStruct *sharedMemory);

void CreateListOfCities(sharedMemoryStruct *sharedMemory);

int InitializeSocket();

int AcceptConnection(int socketDescriptor);

void Game(int userId, sharedMemoryStruct *sharedMemory, int socketDescriptor);

void FirstTurn(sharedMemoryStruct *sharedMemory, int socketDescriptor);

int CheckGuess(string message, sharedMemoryStruct *sharedMemory);

void SendMessage(int socketDescriptor, string message);

string ReceiveMessage(int socketDescriptor);

void MakeTurn(sharedMemoryStruct *sharedMemory, int socketDescriptor);


int main() {


    int memoryId;
    sharedMemoryStruct *sharedMemory;
    int socketDescriptor, newSockedDescriptor;
    vector<city> listOfCities;
    pid_t pid;

    memoryId = GetSharedMemoryId();
    sharedMemory = (sharedMemoryStruct *) shmat(memoryId, 0, 0);
    InitializeFirstLettersArray(sharedMemory);
    CreateListOfCities(sharedMemory);
    socketDescriptor = InitializeSocket();
    
    srand (time(NULL));
    sharedMemory->turn = rand()%2+1; //Выбираем первого игрока случайным образом
    cout<<"Player #"<<sharedMemory->turn<<" makes first turn"<<endl;
    sharedMemory->connectedPlayers = 0;

    cout << "Waiting for all the players to connect..." << endl;
        if ((pid = fork()) < 0) {
            perror("Error with fork");
            exit(1);

        } else if (pid == 0) {

            newSockedDescriptor = AcceptConnection(socketDescriptor);
            cout << "Player " << 1 << " has connected\n";
            sharedMemory->connectedPlayers++;
            Game(1, sharedMemory, newSockedDescriptor);

            cout << "USER #" << 1 << " DONE\n";
            close(newSockedDescriptor);
            cout << "About to exit\n";
            exit(0);
        } else {
            newSockedDescriptor = AcceptConnection(socketDescriptor);
            cout << "Player " << 2 << " has connected\n";
            sharedMemory->connectedPlayers++;
            Game(2, sharedMemory, newSockedDescriptor);

            cout << "USER #" << 2 << " DONE\n";
            close(newSockedDescriptor);
            cout << "About to exit\n";
        }

    waitpid(pid, 0,0);
    close(socketDescriptor);
    cout<<"Closed socket descriptor\n";


    if (shmctl(memoryId, IPC_RMID, 0) < 0) {
        perror("Error cleaning shared memory.");
        exit(1);
    }
    cout << "Cleared memory\nGoodbye!\n";
    exit(0);

}


int GetSharedMemoryId() {
    /*Создает новый сегмент разделяемой памяти и возвращает его идентификатор*/
    int memoryId = shmget(IPC_PRIVATE, 5000, 0666 | IPC_CREAT | IPC_EXCL);
    if (memoryId < 0) {
        perror("Error with shmget");
        exit(1);
    }
    return memoryId;
}


void InitializeFirstLettersArray(sharedMemoryStruct *sharedMemory) {
    /*Заполняет массив первых букв нулями. Назначение массива первых букв - следить,
     * остались ли слова на заданную букву. Для этого, при создании списка городов,
     * мы берем первую букву названия, получаем ее ASCII-значение с помощью (int),
     * вычитаем из него 65 (чтобы массив начинался с 0, а не с 65), и увеличиваем
     * значенеи соответсвующей ячейки на 1*/

    for (int i = 0; i < 26; i++) {
        sharedMemory->firstLettersArray[i] = 0;
    }
}

void CreateListOfCities(sharedMemoryStruct *sharedMemory) {
    /*Создает вектор со списком городов*/

    vector<city> listOfCities;
    int i;
    string line;
    char firstLetter;
    city newCity;

    ifstream file("updatedListOfCities.txt");

    if (!file) {
        perror("Can't open file.");
        exit(1);
    }

    for (i = 0; i < TOTAL_NUM_OF_CITIES; i++) {
        getline(file, line); //Cчитываем название города
        transform(line.begin(), line.end(), line.begin(), ::tolower); //Делаем все буквы lowercase
        newCity.name=line;
        //strcpy(newCity.name, line.c_str());
        newCity.used = false;
        firstLetter = line[0];
        newCity.firstLetterIndex = int(firstLetter) - 65;
        sharedMemory->firstLettersArray[newCity.firstLetterIndex]++; //Увеличиваем количество слов на данную букву на 1
        sharedMemory->listOfCities.push_back(newCity);
    }

    file.close();
}

int InitializeSocket() {
    /* Создает сокет, заполняет структуру адреса сервера, настраивает адрес сокета,
     * переводит его в слушащее состояние, возвращает дескриптор сокета.*/
    int socketDescriptor;
    struct sockaddr_in serverAddress, sin;

    if ((socketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error creating socket");
        exit(1);
    }

    bzero(&serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(0);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(socketDescriptor, (struct sockaddr *) &serverAddress, sizeof(sockaddr_in)) < 0) {
        perror("Error binding");
        exit(1);
    }

    socklen_t len = sizeof(sin);

    if (getsockname(socketDescriptor, (struct sockaddr *) &sin, &len) < 0){
        perror("Error with getsockname");
        exit(1);
    }

    else printf("Port number is: %d\n", ntohs(sin.sin_port));

    if (listen(socketDescriptor, 2) < 0) {
        perror("Error listening");
        close(socketDescriptor);
        exit(1);
    }

    return socketDescriptor;
}


int AcceptConnection(int socketDescriptor) {
    /* Устанавливает соединение на слушающем сокете, возвращает дескриптор сокета клиента.*/
    int newSocketDescriptor;
    struct sockaddr_in clientAddress;
    socklen_t addressLength;

    if ((newSocketDescriptor = accept(socketDescriptor, (struct sockaddr *) &clientAddress, &addressLength)) < 0) {
        perror(NULL);
        close(newSocketDescriptor);
        exit(1);
    }

    return newSocketDescriptor;
}

void Game(int userId, sharedMemoryStruct *sharedMemory, int socketDescriptor) {
    /* Функция игры. Определяет, чей сейчас ход, обрабатывает первый ход игрока, обычный
     * ход и выход.*/

    string message;
    int count;

    message = "Welcome to the game!";
    SendMessage(socketDescriptor, message);


    while (sharedMemory->connectedPlayers != 2) {
        //Ждем, пока количество подключенных игроков не будет равно 2
        sleep(1);
    }

    while (sharedMemory->lastWord != "exit") {
        count=0;
        while (sharedMemory->turn != userId) {
            //игрок ждет своего хода
            sleep(1);
            count++;

            if (count==180){
                //В случае неполадок (например, противник нечаянно закрыл программу,
                //или сдался, не написав "exit", включаем таймер. После 3ех минут
                //ожидания, ждавший игрок становиться победителем.
                strcpy(sharedMemory->lastWord, "exit");
                break;
            }
        }

        cout << "Player #" << userId << " turn" << endl;

        if (strcmp(sharedMemory->lastWord, "") == 0) {

            //Если раннее не было угадано слов, то запускается функция первого хода
            FirstTurn(sharedMemory, socketDescriptor);
        }
        else if (strcmp(sharedMemory->lastWord, "exit") == 0) {

            //Если последнее угаданное слово - exit, означает что противник вышел.
            //Игроку отсылается соответствующее сообщение
            SendMessage(socketDescriptor, "Your opponent left");
            break;
        }
        else {
            
            for (int i = 0; i < TOTAL_NUM_OF_CITIES; i++) {
                //Отмечаем, что последнее одгаданное слово было использовано
                if (strcmp(sharedMemory->lastWord, sharedMemory->listOfCities[i].name.c_str()) == 0) {

                    sharedMemory->listOfCities[i].used = true;
                }
            }
            //Функция обработки хода
            MakeTurn(sharedMemory, socketDescriptor);
            //Если игрок написал exit, то выходим из игры
            if (strcmp(sharedMemory->lastWord, "exit") == 0) {
                sharedMemory->turn = 3 - userId;
                break;
            }

        }
    
        sharedMemory->turn = 3 - userId;
    }

}

void FirstTurn(sharedMemoryStruct *sharedMemory, int socketDescriptor) {
    /* Функция первого хода. Отличается от обычного хода тем, что игрок может ввести 
     * название любого города. */
    string message;
    string guess;
    int result;

    message = "You start the game! Enter any US city:";
    SendMessage(socketDescriptor, message);
    guess = ReceiveMessage(socketDescriptor);
    cout << "Received guess: " << guess << endl;

    while ((result = CheckGuess(guess, sharedMemory)) != 1) {
        //Пока попыта игрока не пройдет проверку - просим его ввести город занаво
        
        SendMessage(socketDescriptor, "City doesn't exist");
        guess = ReceiveMessage(socketDescriptor);
    }

    //strcpy(message, "guess_passed");
    SendMessage(socketDescriptor, "guess_passed");
    cout << "City found\n";
    strcpy(sharedMemory->lastWord, guess.c_str()); //Записываем название города в последнее отгаданное слова


}

void MakeTurn(sharedMemoryStruct *sharedMemory, int socketDescriptor) {
    /* Функция совершения хода */
    string message;
    string guess;
    string temp;
    int result;
    int letterIndex=1;

    message = "Your opponent guessed: ";
    message.append(sharedMemory->lastWord);
    SendMessage(socketDescriptor, message); //Отправляем последнее отгаданное слово игроку
    cout << "Sent message\n";
    guess = ReceiveMessage(socketDescriptor);
    cout << "Received guess: " << guess << endl;


    if (guess=="exit") {
        //Если игрок ввел exit, то выходим из функции
        strcpy(sharedMemory->lastWord, "exit");
    }
    else {
        char lastGuessedLetter = sharedMemory->lastWord[(strlen(sharedMemory->lastWord)) - letterIndex];

        while ((result = CheckGuess(guess, sharedMemory)) != 1 || guess[0] != lastGuessedLetter) {

            if (guess[0] != lastGuessedLetter) {
                //Игрок ввел слово не на ту букву
                message = "Start with ";
                message += lastGuessedLetter;
                SendMessage(socketDescriptor, message);
            }
            else if (sharedMemory->firstLettersArray[(int)lastGuessedLetter-65]==0) {
                //Если слов на данную букву не осталось, то просим пользователя ввести
                //город на предыдущую букву в слове
                message = "No more words that start with ";
                message += lastGuessedLetter;
                message += ". Start with ";
                lastGuessedLetter = sharedMemory->lastWord[(strlen(sharedMemory->lastWord)) - letterIndex];
                message += lastGuessedLetter;
                SendMessage(socketDescriptor, message);
            }
            else if (result == 0) {
                //Игрок ввел несуществующий город
                SendMessage(socketDescriptor, "City doesn't exist. Try again.");
            }
            else if (result == 2) {
                //Город, который ввел игрок, уже использовали
                SendMessage(socketDescriptor,"City was already used. Try again.");
            }

            guess="";
            guess = ReceiveMessage(socketDescriptor);
            cout << "Received guess: " << guess << endl;

            if (guess == "exit") {
                //Если игрок ввел exit, то выходим из функции
                strcpy(sharedMemory->lastWord, "exit");
                break;
            }
        }

        if (guess != "exit") {
            //Сообщение о том, что слово прошло проверку и принято сервреом
            SendMessage(socketDescriptor, "guess_passed");
            cout << "City found\n";
        }
    }
}

int CheckGuess(string message, sharedMemoryStruct *sharedMemory) {
    /* Функция проверки введенного игроком города. Возвращает число: 0 - если город не был найден,
     * 1 - если город был найден и не был раннее использован, 2 - если город был раннее использован */
    
    int cityFound = 0;
    int i;


    for (i = 0; i < TOTAL_NUM_OF_CITIES; i++) {

        if (sharedMemory->listOfCities[i].name == message) {
            cityFound = 1;
            if (sharedMemory->listOfCities[i].used) {
                cityFound = 2;
            }
            else {
                //Город был найден и не был использован
                strcpy(sharedMemory->lastWord, message.c_str()); //Записываем его в последнее угаданное слово
                sharedMemory->firstLettersArray[sharedMemory->listOfCities[i].firstLetterIndex]--;
                //Уменьшаем количество слов на данную букву
                sharedMemory->listOfCities[i].used = true; //Указываем, что город был использован
            }
            break;
        }
    }

    return cityFound;
}

void SendMessage(int socketDescriptor, string message) {
    /* Функция отправки сообщения. Сначала отправляет размер сообщения, затем само сообщение.
     * Получатель сравнивает количество полученных байт и полученный размер сообщения, и если
     * они равны - посылает подтверждение, передача прекращается. Если не равны, то сообщение
     * будет послыаться до тех пор, пока отправитель не получит нужное кол-во байтов. */
    bool allBytesReceived = false;
    int bytesSend, bytesReceived, messageLength;

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
