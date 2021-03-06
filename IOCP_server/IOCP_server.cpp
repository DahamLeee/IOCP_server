// IOCP_server.cpp: 콘솔 응용 프로그램의 진입점을 정의합니다.
//

#include "stdafx.h"
#include <WinSock2.h>
#include <mysql.h>
#include <string.h>

#pragma comment(lib, "libmysql.lib")
#pragma comment(lib, "Ws2_32.lib")

#define MAX_BUFFER 1024
#define SERVER_PORT 6666

#define MYSQLUSER "dlek1011"
#define MYSQLPASSWORD "ekgka8168"
#define MYSQLIP "localhost"

using namespace std;

struct SOCKETINFO {
	WSAOVERLAPPED overlapped;
	WSABUF dataBuffer;
	SOCKET socket;
	char messageBuffer[MAX_BUFFER];
	int receiveBytes;
	int sendBytes;
};

char *buffer = new char; // header의 AAAA인지, BBBB인지 구분해주는 buffer공간
MYSQL * cons = mysql_init(NULL);

DWORD WINAPI makeThread(LPVOID hIOCP);
void loadmysql(const char* mysqlip, MYSQL *cons);

int main()
{
	loadmysql(MYSQLIP, cons);
	//Winsock Start - windock.dll 로드
	WSADATA WSAData;
	if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0) {
		printf("Error - Can not load 'winsock.dll' file\n");
		return 1;
	}

	// 1. 소켓 생성
	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (listenSocket == INVALID_SOCKET) {
		printf("Error - Invalid socket\n");
		return 1;
	}

	// 서버정보 객체 설정
	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	// 2. 소켓 설정
	if (bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR) {
		printf("Error - Fail bind\n");
		// 6. 소켓 종료
		closesocket(listenSocket);
		// Winsock End
		WSACleanup();
		return 1;
	}

	// 3. 수신 대기열 생성
	if (listen(listenSocket, 5) == SOCKET_ERROR) {
		printf("Error - Fail listen\n");
		// 6. 소켓 종료
		closesocket(listenSocket);
		// Winsock End
		WSACleanup();
		return 1;
	}

	// 완료 결과를 처리하는 객체(CP : Completion Port) 생성
	HANDLE hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	// 워커스래드 생성
	// - CPU * 2개
	
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	int threadCount = systemInfo.dwNumberOfProcessors * 2;
	unsigned long threadId;
	// - thread Handler 선언
	HANDLE *hThread = (HANDLE*)malloc(threadCount * sizeof(HANDLE));
	// - thread 생성
	for (int i = 0; i < threadCount; i++) {
		hThread[i] = CreateThread(NULL, 0, makeThread, &hIOCP, 0, &threadId);
	}

	SOCKADDR_IN clientAddr;
	int addrLen = sizeof(SOCKADDR_IN);
	memset(&clientAddr, 0, addrLen);
	SOCKET clientSocket;
	SOCKETINFO *socketInfo;
	DWORD receiveBytes;
	DWORD flags;

	while (1) {
		clientSocket = accept(listenSocket, (struct sockaddr*)&clientAddr, &addrLen);
		if (clientSocket == INVALID_SOCKET) {
			printf("Error - Accept Failure\n");
			return 1;
		}
		socketInfo = (struct SOCKETINFO *)malloc(sizeof(struct SOCKETINFO));
		memset((void*)socketInfo, 0x00, sizeof(struct SOCKETINFO));
		socketInfo->socket = clientSocket;
		socketInfo->receiveBytes = 0;
		socketInfo->sendBytes = 0;
		socketInfo->dataBuffer.len = MAX_BUFFER;
		socketInfo->dataBuffer.buf = socketInfo->messageBuffer;
		flags = 0;

		hIOCP = CreateIoCompletionPort((HANDLE)clientSocket, hIOCP, (DWORD)socketInfo, 0);



		//중첩 소캣을 지정하고 완료시 실행될 함수를 넘겨준다.
		if (WSARecv(socketInfo->socket, &socketInfo->dataBuffer, 1, &receiveBytes, &flags, &(socketInfo->overlapped), NULL)) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				printf("Error - IO pending Failure\n");
				return 1;
			}
		}
	}
	// 6-2 리슨 소켓종료
	closesocket(listenSocket);

	// Winsock End
	WSACleanup();

    return 0;
}

DWORD WINAPI makeThread(LPVOID hIOCP) {
	HANDLE threadHandler = *((HANDLE*)hIOCP);
	DWORD receiveBytes;
	DWORD sendBytes;
	DWORD completionKey;
	DWORD flags;
	struct SOCKETINFO *eventSocket;

	char *query = new char;

	while (1) {
		// 입출력 완료 대기
		// mysql -- mysql load

		if (GetQueuedCompletionStatus(threadHandler, &receiveBytes, (PULONG_PTR)&completionKey, (LPOVERLAPPED*)&eventSocket, INFINITE) == 0) {
			printf("Error - GetQueuedCompletionStatus Failure\n");
			closesocket(eventSocket->socket);
			free(eventSocket);
			return 1;
		}

		eventSocket->dataBuffer.len = receiveBytes;

		if (receiveBytes == 0) {
			closesocket(eventSocket->socket);
			free(eventSocket);
			continue;
		}
		else {
			MYSQL_RES *sql_result;
			MYSQL_ROW sql_row;
			
			//헤더가 AAAA 이면 login ==> login을 성공 시키면 main의 status를 변경하여서 그에 맞춰 대응 하도록
			//헤더가 BBBB 이면 register 
			strcpy(buffer, eventSocket->dataBuffer.buf);
			//login ==> status를 리턴해준다...??
			if (strstr(buffer, "AAAA") > 0) { 
				bool status = false;  // id입력이 정확한지
				char *id = new char; // id
				char *pwd = new char;
				char *SHApwd = new char;
				char *token = new char;
				int i = 0;

				token = strtok(buffer, "|");

				while (token = strtok(NULL, "|")) {
					if (i == 0) {
						strcpy(id, token);
					}
					else if (i == 1) {
						strcpy(pwd, token);
						break;
					}
					i++;
				}
				cout << "ID : " << id << endl;
				cout << "PWD : " << pwd << endl;
				// I get a ID, PWD here   can make a function
				cout << eventSocket->dataBuffer.buf << endl;

				memset(query, 0, sizeof(query));
				sprintf(query, "select SHA('%s');", pwd);
				mysql_query(cons, query);
				sql_result = mysql_store_result(cons);
				sql_row = mysql_fetch_row(sql_result);
				SHApwd = sql_row[0];

				mysql_query(cons, "select * from emp");
				sql_result = mysql_store_result(cons);

				ZeroMemory(eventSocket->dataBuffer.buf, MAX_BUFFER);
				while (sql_row = mysql_fetch_row(sql_result)) {
					if (strcmp(sql_row[0], id) == 0) {
						if (strcmp(sql_row[1], SHApwd) == 0) {
							// login success라고 client에게 보내면 되고
							strcpy(eventSocket->dataBuffer.buf, "login Success");
							if (WSASend(eventSocket->socket, &(eventSocket->dataBuffer), 1, &sendBytes, 0, NULL, NULL) == SOCKET_ERROR) {
								if (WSAGetLastError() != WSA_IO_PENDING) {
									printf("Error - Fail WSASend(error_code : %d)\n", WSAGetLastError());
								}
							}
							printf("TRACE - Send message : %s (%d bytes)\n", eventSocket->dataBuffer.buf, eventSocket->dataBuffer.len);
							status = true;
						}
						else {
							// 잘못된 비밀번호를 입력하였다고 보내면되고
							strcpy(eventSocket->dataBuffer.buf, "wrong PWD");
							if (WSASend(eventSocket->socket, &(eventSocket->dataBuffer), 1, &sendBytes, 0, NULL, NULL) == SOCKET_ERROR) {
								if (WSAGetLastError() != WSA_IO_PENDING) {
									printf("Error - Fail WSASend(error_code : %d)\n", WSAGetLastError());
								}
							}
							printf("TRACE - Send message : %s (%d bytes)\n", eventSocket->dataBuffer.buf, eventSocket->dataBuffer.len);
							status = true;
						}
					}
				}
				// 밖으로 빠져 나왔다는 의미가 일치하는 아이디가 없다는 거임 즉 id를 다시 입력하세요
				if (status == false) {
					strcpy(eventSocket->dataBuffer.buf, "No id");

					if (WSASend(eventSocket->socket, &(eventSocket->dataBuffer), 1, &sendBytes, 0, NULL, NULL) == SOCKET_ERROR) {
						if (WSAGetLastError() != WSA_IO_PENDING) {
							printf("Error - Fail WSASend(error_code : %d)\n", WSAGetLastError());
						}
					}
					printf("TRACE - Send message : %s (%d bytes)\n", eventSocket->dataBuffer.buf, eventSocket->dataBuffer.len);
					
				}
				memset(eventSocket->messageBuffer, 0x00, MAX_BUFFER);
				eventSocket->receiveBytes = 0;
				eventSocket->sendBytes = 0;
				eventSocket->dataBuffer.len = MAX_BUFFER;
				eventSocket->dataBuffer.buf = eventSocket->messageBuffer;
				flags = 0;

				if (WSARecv(eventSocket->socket, &(eventSocket->dataBuffer), 1, &receiveBytes, &flags, &eventSocket->overlapped, NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != WSA_IO_PENDING) {
						printf("Error - Fail WSARecv(error_code : %d)\n", WSAGetLastError());
					}
				}
			}
			// Register
			else if (strstr(buffer, "BBBB") > 0) { 
				eventSocket->dataBuffer.buf = strstr(buffer, " ");
				cout << eventSocket->dataBuffer.buf << endl;
				
				printf("TRACE - Receive Message : %s (%d bytes) \n", eventSocket->dataBuffer.buf, eventSocket->dataBuffer.len);
				strcpy(query, "insert into emp (id, pwd, name, age, gender, macAddress) values");
				strcat(query, eventSocket->dataBuffer.buf);
				
				cout << query << endl;				
				mysql_query(cons, query);
				Sleep(1000);

				if (WSASend(eventSocket->socket, &(eventSocket->dataBuffer), 1, &sendBytes, 0, NULL, NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != WSA_IO_PENDING) {
						printf("Error - Fail WSASend(error_code : %d)\n", WSAGetLastError());
					}
				}

				printf("TRACE - Send message : %s (%d bytes)\n", eventSocket->dataBuffer.buf, eventSocket->dataBuffer.len);

				memset(eventSocket->messageBuffer, 0x00, MAX_BUFFER);
				eventSocket->receiveBytes = 0;
				eventSocket->sendBytes = 0;
				eventSocket->dataBuffer.len = MAX_BUFFER;
				eventSocket->dataBuffer.buf = eventSocket->messageBuffer;
				flags = 0;

				if (WSARecv(eventSocket->socket, &(eventSocket->dataBuffer), 1, &receiveBytes, &flags, &eventSocket->overlapped, NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != WSA_IO_PENDING) {
						printf("Error - Fail WSARecv(error_code : %d)\n", WSAGetLastError());
					}
				}
				
			}
			
		}
		
	}
}

void loadmysql(const char* mysqlip, MYSQL *cons) {
	if (cons == NULL) {
		fprintf(stderr, "%s\n", mysql_error(cons));
		Sleep(1000);
		exit(1);
	}

	if (!(mysql_real_connect(cons, mysqlip, MYSQLUSER, MYSQLPASSWORD, "test1", 0, 0, 0) == NULL)) {
		mysql_set_character_set(cons, "euckr");
	}
	else {
		fprintf(stderr, "연결 오류 : %s \n", mysql_error(cons));
		getchar();
	}

	return;
}