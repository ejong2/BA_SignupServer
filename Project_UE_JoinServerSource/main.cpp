#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <stdlib.h>

#include <WinSock2.h>
#include <process.h>
#include <vector>
#include <string>

#include "jdbc/mysql_connection.h"
#include "jdbc/cppconn/driver.h"
#include "jdbc/cppconn/exception.h"
#include "jdbc/cppconn/prepared_statement.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment (lib, "mysqlcppconn.lib")

#define PORT 19935
#define IP_ADDRESS "127.0.0.1"
#define PACKET_SIZE 200

using namespace std;

const string server = "tcp://127.0.0.1:3306";
const string username = "root";
const string password = "1234";

vector<SOCKET> vSocketList;

CRITICAL_SECTION ServerCS;

sql::Driver* driver = nullptr;
sql::Connection* con = nullptr;
sql::Statement* stmt = nullptr;
sql::PreparedStatement* pstmt = nullptr;
sql::ResultSet* rs = nullptr;

unsigned WINAPI WorkThread(void* Args)
{
    SOCKET CS = *(SOCKET*)Args;

    while (true)
    {
        char IdBuffer[PACKET_SIZE] = { 0, };
        char PwdBuffer[PACKET_SIZE] = { 0, };
        char PlayerNameBuffer[PACKET_SIZE] = { 0, };
        char ExitBuffer[PACKET_SIZE] = "EXIT";

        int RecvBytes = recv(CS, IdBuffer, sizeof(IdBuffer), 0);
        if (RecvBytes <= 0)
        {
            cout << "클라이언트 연결 종료 : " << CS << '\n';

            closesocket(CS);
            EnterCriticalSection(&ServerCS);
            vSocketList.erase(find(vSocketList.begin(), vSocketList.end(), CS));
            LeaveCriticalSection(&ServerCS);
            break;
        }
        IdBuffer[PACKET_SIZE - 1] = '\0';
        string strID = IdBuffer;

        RecvBytes = recv(CS, PwdBuffer, sizeof(PwdBuffer), 0);
        if (RecvBytes <= 0)
        {
            cout << "클라이언트 연결 종료 : " << CS << '\n';

            closesocket(CS);
            EnterCriticalSection(&ServerCS);
            vSocketList.erase(find(vSocketList.begin(), vSocketList.end(), CS));
            LeaveCriticalSection(&ServerCS);
            break;
        }
        PwdBuffer[PACKET_SIZE - 1] = '\0';
        string strPWD = PwdBuffer;

        RecvBytes = recv(CS, PlayerNameBuffer, sizeof(PlayerNameBuffer), 0);
        if (RecvBytes <= 0)
        {
            cout << "클라이언트 연결 종료 : " << CS << '\n';

            closesocket(CS);
            EnterCriticalSection(&ServerCS);
            vSocketList.erase(find(vSocketList.begin(), vSocketList.end(), CS));
            LeaveCriticalSection(&ServerCS);
            break;
        }

        PlayerNameBuffer[PACKET_SIZE - 1] = '\0';
        string strPlayerName = PlayerNameBuffer;

        //      pstmt = con->prepareStatement("SELECT * FROM UserTable WHERE `PlayerName` = ?");
        //      pstmt->setString(4, strPlayerName);
        //      rs = pstmt->executeQuery();
        //      bool PlayerNameExists = rs->rowsCount() > 0 ? true : false;

        //      if (PlayerNameExists)
        //      {
        //          cout << "이미 가입된 플레이어 이름입니다" << '\n';

        //          int SendBytes = 0;
        //          int TotalSentBytes = 0;
        //          do
        //          {
        //              SendBytes = send(CS, &ExitBuffer[TotalSentBytes], sizeof(ExitBuffer) - TotalSentBytes, 0);
        //              TotalSentBytes += SendBytes;
        //          } while (TotalSentBytes < sizeof(ExitBuffer));
              //}

        pstmt = con->prepareStatement("SELECT * FROM UserTable WHERE `ID` = ?");
        pstmt->setString(1, strID);
        rs = pstmt->executeQuery();
        bool IdExists = rs->rowsCount() > 0 ? true : false;

        if (IdExists)
        {
            cout << "이미 가입된 ID 입니다." << '\n';

            int SendBytes = 0;
            int TotalSentBytes = 0;
            do
            {
                SendBytes = send(CS, &ExitBuffer[TotalSentBytes], sizeof(ExitBuffer) - TotalSentBytes, 0);
                TotalSentBytes += SendBytes;
            } while (TotalSentBytes < sizeof(ExitBuffer));
        }
        else
        {
            pstmt = con->prepareStatement("INSERT INTO UserTable(`ID`,`PWD`, `isLogin`, `PlayerName`) VALUES(?, ?, ?, ?)");
            pstmt->setString(1, strID);
            pstmt->setString(2, strPWD);
            pstmt->setBoolean(3, false);
            pstmt->setString(4, strPlayerName);

            pstmt->execute();
            cout << "가입이 완료되었습니다." << endl;
        }

        EnterCriticalSection(&ServerCS);
        for (int i = 0; i < vSocketList.size(); i++)
        {
            int SendBytes = 0;
            int TotalSentBytes = 0;
            do
            {
                SendBytes = send(CS, &PwdBuffer[TotalSentBytes], sizeof(PwdBuffer) - TotalSentBytes, 0);
                TotalSentBytes += SendBytes;
            } while (TotalSentBytes < sizeof(PwdBuffer));

            if (SendBytes <= 0)
            {
                closesocket(CS);
                EnterCriticalSection(&ServerCS);
                vSocketList.erase(find(vSocketList.begin(), vSocketList.end(), CS));
                LeaveCriticalSection(&ServerCS);
                break;
            }
        }
        LeaveCriticalSection(&ServerCS);
    }
    return 0;
}

int main()
{
    driver = get_driver_instance();
    con = driver->connect(server, username, password);
    con->setSchema("UE4SERVER");

    cout << "[회원가입 서버 활성화]" << '\n';

    InitializeCriticalSection(&ServerCS);

    WSADATA WSAData;
    WSAStartup(MAKEWORD(2, 2), &WSAData);
    SOCKET SS = socket(AF_INET, SOCK_STREAM, 0);

    SOCKADDR_IN SA = { 0, };
    SA.sin_family = AF_INET;
    SA.sin_addr.S_un.S_addr = inet_addr(IP_ADDRESS);
    SA.sin_port = htons(PORT);

    if (::bind(SS, (SOCKADDR*)&SA, sizeof(SA)) != 0) { exit(-3); };
    if (listen(SS, SOMAXCONN) == SOCKET_ERROR) { exit(-4); };

    cout << "클라이언트 연결을 기다리는 중입니다......." << '\n';

    while (true)
    {
        SOCKADDR_IN CA = { 0, };
        int sizeCA = sizeof(CA);
        SOCKET CS = accept(SS, (SOCKADDR*)&CA, &sizeCA);

        cout << "클라이언트 접속 : " << CS << '\n';

        EnterCriticalSection(&ServerCS);
        vSocketList.push_back(CS);
        LeaveCriticalSection(&ServerCS);

        HANDLE hThread = (HANDLE)_beginthreadex(0, 0, WorkThread, (void*)&CS, 0, 0);
    }
    closesocket(SS);

    WSACleanup();
}

