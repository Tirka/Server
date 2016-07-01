﻿/**************************************************************************************************/
/*           ███████╗██╗███╗   ███╗██████╗ ██╗     ███████╗    ██╗   ██╗██████╗ ██████╗           */
/*           ██╔════╝██║████╗ ████║██╔══██╗██║     ██╔════╝    ██║   ██║██╔══██╗██╔══██╗          */
/*           ███████╗██║██╔████╔██║██████╔╝██║     █████╗      ██║   ██║██║  ██║██████╔╝          */
/*           ╚════██║██║██║╚██╔╝██║██╔═══╝ ██║     ██╔══╝      ██║   ██║██║  ██║██╔═══╝           */
/*           ███████║██║██║ ╚═╝ ██║██║     ███████╗███████╗    ╚██████╔╝██████╔╝██║               */
/*           ╚══════╝╚═╝╚═╝     ╚═╝╚═╝     ╚══════╝╚══════╝     ╚═════╝ ╚═════╝ ╚═╝               */
/*                                                                                                */
/*        ███████╗██╗██╗     ███████╗    ███████╗███████╗██████╗ ██╗   ██╗███████╗██████╗         */
/*        ██╔════╝██║██║     ██╔════╝    ██╔════╝██╔════╝██╔══██╗██║   ██║██╔════╝██╔══██╗        */
/*        █████╗  ██║██║     █████╗      ███████╗█████╗  ██████╔╝██║   ██║█████╗  ██████╔╝        */
/*        ██╔══╝  ██║██║     ██╔══╝      ╚════██║██╔══╝  ██╔══██╗╚██╗ ██╔╝██╔══╝  ██╔══██╗        */
/*        ██║     ██║███████╗███████╗    ███████║███████╗██║  ██║ ╚████╔╝ ███████╗██║  ██║        */
/*        ╚═╝     ╚═╝╚══════╝╚══════╝    ╚══════╝╚══════╝╚═╝  ╚═╝  ╚═══╝  ╚══════╝╚═╝  ╚═╝        */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                                                                                */
/*                                  ОПИСАНИЕ ПРОТОКОЛА НАЧАЛО                                     */
/*                                                                                                */
/***************************************************************************************************

Сервер и клиент обмениваются датаграммами с информацией. Нулевой байт датаграммы всегда
содержит *код операции*, после чего следуют данные в зависимости от кода.

Передача файла начинается с того, что клиент передает серверу датаграмму с кодом 0x10,
после чего следует нуль-терминированное имя файла в ASCII кодировке. Если после
нулл-байта идут еще какие-то данные, то они игнорируются
Пример: запросить File Info для файла с именем "FILE"
+-----------------------------------------------------+
|        |        |        |        |        |        |
|  0x10  |   F    |   I    |   L    |   E    |  0x00  |
|        |        |        |        |        |        |
+-----------------------------------------------------+
 0      7 8     15 16    23 24    31 32    39 40    47

В случае успеха сервер возвращает датаграмму с кодом 0x20 после чего идет 16-битный *идентификатор файла*.
Завершает кадр 32-битное число – *количество блоков*, на которые разбит файл. Идентификатор файла клиенту
необходимо запомнить, чтобы в последствии использовать его при запросе блоков файла.
Примечание: 16- и 32-битные числа здесь и далее записаны в little-endian. Т.е. байты идут от младшего
к старшему. Это упрощает чтение и запись данных в буфер (ниже по исходникам видно)
Пример:
 <--Код-> <---Ид. файла---> <--------Количество блоков-------->
+--------------------------------------------------------------+
|        |                 |                                   |
|  0x20  |  0x01     0x00  |  0x71    0x1C      0x00     0x00  |
|        |                 |                                   |
+--------------------------------------------------------------+
 0      7 8              23 24                               55

Идентификатор файла   = 0x 00 01        = 1d
Количество блоков     = 0x 00 00 1С 71  = 7281d

Если файл на сервере не найден, либо в имени файла недопустимые символы, сервер возвращает ошибку
с кодом 0xE0
Пример:
+--------+
|        |
|  0xE0  |
|        |
+--------+
 0      7

Получив идентификатор файла и количество блоков файла, клиент может начинать запрашивать у
сервера блоки двумя способами:

а) клиент запрашивает диапазон блоков [блок1; блокN] допустимые числовые значение от нуля
до *количество блоков* - 1, код операции 0x30.
Пример:
 <--Код-> <---Ид. файла---> <--------Начальный блок-----------> <---------Конечный блок----------->
+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |        |        |        |
|  0x30  |  0x01  |  0x00  |  0x00  |  0x00  |  0x00  |  0x00  |  0x70  |  0x1C  |  0x00  |  0x00  |
|        |        |        |        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
 0      7 8              23 24                               55 56                               87
Идентификатор запрашиваемого файла   = 0x 00 01         = 1d
Начальный блок                       = 0x 00 00 00 00   = 0d
Конечный блок                        = 0x 00 00 1C 70   = 7280d

б) клиент запрашивает массив блоков, т.е. произвольный набор. Количество запрашиваемых
блоков ограничено только размером датаграммы. Код операции 0x40.
Пример:
 <--Код-> <---Ид. файла---> <----------Первый блок------------> <-----------Второй блок----------->
+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |        |        |        |
|  0x40  |  0x01  |  0x00  |  0x02  |  0x00  |  0x00  |  0x00  |  0x04  |  0x00  |  0x00  |  0x00  |
|        |        |        |        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
 0      7 8              23 24                               55 56                               87

Здесь запрашиваются два блока. 2d и 4d


Получив запрос на передачу блока сервер отправляет данные в следующем формате:
 <--Код-> <---Ид. файла---> <--------------------------Данные------------------------------------->
+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |        |        |        |
|  0x50  |  0x01  |  0x00  |  0xA2  |  0x92  |  0xDD  |  0x72  |  0x71  |  0x3F  |  0x33  |  0x76  |
|        |        |        |        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
 0      7 8              23 24                                                                   87

Длина данных может быть любой. Обычно она равна максимальному размеру данных минус три байта.
Для последнего блока длина данных может быть меньшей.

***************************************************************************************************/
/*                                                                                                */
/*                                   ОПИСАНИЕ ПРОТОКОЛА КОНЕЦ                                     */
/*                                                                                                */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                                                                                */
/*                                        НАЧАЛО ДИРЕКТИВ                                         */
/*                                                                                                */
/**************************************************************************************************/
#include "stdafx.h"

// Директива для линковщика статически прилинковать
// либу winsock2 с API для работы с TCP/IP
#pragma comment(lib, "ws2_32.lib")
/**************************************************************************************************/
/*                                                                                                */
/*                                         КОНЕЦ ДИРЕКТИВ                                         */
/*                                                                                                */
/**************************************************************************************************/





/**************************************************************************************************/
/*                                                                                                */
/*                                       ТИПЫ ДАННЫХ НАЧАЛО                                       */
/*                                                                                                */
/**************************************************************************************************/
// Код операции (см. описание протокола)
enum MSG_TYPE : uint8_t
{
	C_FILE_INFO_REQUEST = 0x10,           // CLI->SRV
	C_FILE_INFO_RESPONSE = 0x20,          // SRV->CLI
	C_FILE_BLOCK_RANGE_REQUEST = 0x30,    // CLI->SRV
	C_FILE_BLOCK_ARRAY_REQUEST = 0x40,    // CLI->SRV
	C_FILE_BLOCK_RESPONSE = 0x50,         // SRV->CLI
	C_ERR_FILE_NOT_FOUND = 0xE0,          // SRV->CLI
	C_ERR_INVALID_FILE_HANDLER = 0xE1,    // SRV->CLI
	C_ERR_INVALID_BLOCK_REQUESTED = 0xE2, // SRV->CLI
	C_ERR_INVALID_OPERATION = 0xE3,       // SRV->CLI
};

// Т.к. наш сервер будет иметь возможность раздавать не один захардкоженный файл,
// а тот, о котором его попросит клиент, мы будем в этом классе (структуре) 
// хранить информацию о запрошенном файле. После того как клиент запрашивает файл
// у сервера по его имени, сервер проверяет, есть ли такой файл в его рабочей папке,
// и если есть, то он создает новый объект этого типа, где хранит *идентификатор* этого
// файла, т.е. числовое значение, которое в дальнейшем клиент должен использовать,
// запрашивая блоки этого файла. Также структура хранить количество блоков, на которое
// файл будет разбит при передаче, ссылку на файл и его имя.
class FileInfo
{
public:
	uint16_t handler;
	uint32_t blocksCount;
	std::string name = "";
	std::ifstream file;
};
/**************************************************************************************************/
/*                                                                                                */
/*                                        ТИПЫ ДАННЫХ КОНЕЦ                                       */
/*                                                                                                */
/**************************************************************************************************/





/**************************************************************************************************/
/*                                                                                                */
/*                               НАЧАЛО ОБЪЯВЛЕНИЙ ПРОТОТИПОВ ФУНКЦИЙ                             */
/*                                                                                                */
/**************************************************************************************************/
void SetConsoleToUTF8();
int CheckArgumens(int argc, char* argv[]);
int WinSockInit();
inline void SendBlock(uint32_t block, FileInfo* fileInfo);
/**************************************************************************************************/
/*                                                                                                */
/*                                КОНЕЦ ОБЪЯВЛЕНИЙ ПРОТОТИПОВ ФУНКЦИЙ                             */
/*                                                                                                */
/**************************************************************************************************/





/**************************************************************************************************/
/*                                                                                                */
/*                            ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ И КОНСТАНТЫ НАЧАЛО                            */
/*                                                                                                */
/**************************************************************************************************/
// Максимальный размер UDP-датаграммы
// uint16_t следует понимать как unsigned int длиной 16 бит. _t говорит,
// что это токен "ТИП ПЕРЕМЕННОЙ" (type)
const uint16_t DATAGRAM_MAX_SIZE = 4096;

// Максимальный размер блока (т.е. куска файла, который будет влазить в датаграмму.
// 7 байт отводится пролокотом на служебную информацию (см. описание протокола)
const uint16_t MAX_BLOCK_SIZE = 4096 - 7;

// Под максимальный размер датаграммы выделяем буфер, который будем
// использовать для получения/отправки данных из/в сеть
// Немножко глобальных буферов еще никому не навредило
char* buf = new char[DATAGRAM_MAX_SIZE];

// Серверный сокет, посредством которого будем передавать данные по сети
SOCKET serverSocket;

// Структура, которая будет хранить IP-адрес и порт клиента, работающего с
// нашим сервером, данные в нее будет записывать WinSock, мы будем
// использовать эту структуру в качестве обратного адреса, по которому
// сервер будет слать ответ на запрос клиента
sockaddr_in clientAddr;

// Вычисляем размер этой структуры. Т.к структура clientAddr будет
// передаваться в методы WinSock по ссылке, надо так же передать
// помимо ссылки на структуру ее размер, чтобы WinSock мог понять, где
// заканчивается память структуры, а где начинается чья-то чужая память
// неуправляемый код, хуле...
int clientAddrSize = sizeof(clientAddr);

// Глобальный счетчик идентификаторов файлов (см. описание протокола)
uint16_t fileHandlerCounter = 1;

// Это такая хитрая структура данных. Ассоциативный массив, где в качестве ключа
// используется идентификатор файла, а в качестве значения структура со всей необходимой
// инфой соответствующего физического файла. Выбор именно такой структуры обусловлен
// желанием максимально быстро находить структуру с инфой о файле, соответствующей
// ключу (идентификатору), т.к. критическая часть кода.
std::map<uint16_t, FileInfo*> files;
/**************************************************************************************************/
/*                                                                                                */
/*                             ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ И КОНСТАНТЫ КОНЕЦ                            */
/*                                                                                                */
/**************************************************************************************************/





/**************************************************************************************************/
/*                                                                                                */
/*                                        НАЧАЛО ФУНКЦИИ MAIN                                     */
/*                                                                                                */
/**************************************************************************************************/
int main(int argc, char* argv[])
{
	// Ф-я переводит консоль в режим UTF8. Это надо
	// для возможности писать русскими буквами.
	// В самом окне консоли необходимо поменять
	// шрифт на Consolas или Ludida Console.
	// (ПКМ по окну консоли -> Свойства -> Шрифт)
	// http://ru.stackoverflow.com/questions/459154/Русский-язык-в-консоли
	SetConsoleToUTF8();

	int error;

	// Проверяем правильность ввода параметров коммандной строки.
	error = CheckArgumens(argc, argv);
	if (error != 0)
	{
		wprintf(L"Ошибка параметров коммандной строки\n");
		wprintf(L"Пример запуска: .\\Server 192.168.10.20 8080\n");
		return 10;
	}

	// Перед началом использования сокетов в винде,
	// их надо инициализировать и проверить версию.
	// https://msdn.microsoft.com/en-us/library/windows/desktop/ms742213(v=vs.85).aspx
	error = WinSockInit();
	if (error != 0)
	{
		wprintf(L"Ошибка при инициализации WinSock\n");
		return 20;
	}

	// https://msdn.microsoft.com/en-us/library/windows/desktop/bb530742(v=vs.85).aspx 
	serverSocket = INVALID_SOCKET;
	// Инициализируем сокет. В качестве параметров указываем, что мы хотим
	// UDP протокол и передачу данных датаграммами
	serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (serverSocket == INVALID_SOCKET) {
		wprintf(L"Ошибка при вызове socket(): %d\n", WSAGetLastError());
		WSACleanup();
		return 30;
	}

	// После создания сокета его надо привязать к какому-то адресу и порту
	// на локальной машине, для этого создаем структуру sockaddr_in
	struct sockaddr_in serverAddr;

	// Обнуляем память (т.к. там может лежать какой-нибудь мусор с момента
	// последнего ее использования, возможно другим процессом или этим же
	// процессом, выделившим по этому адресу память и в последствии освободившим ее
	ZeroMemory(&serverAddr, sizeof(serverAddr));

	// Забиваем в структуру семейство адресов (IPv4)
	serverAddr.sin_family = AF_INET;

	// Порт сначала преобразуем функцией atoi из строки, переданной в качестве
	// аргумента командной строки в число. Затем это число записываем в поле нашей
	// структуры serverAddr, используя правильный порядок байтов (ф-я htons)
	serverAddr.sin_port = htons(atoi(argv[2]));

	// Функция inet_pton принимает вторым агрументом адрес IPv4 (первый агрумент) в
	// виде строки и преобразует ее в нужное числовое значение с нужным порядком 
	// байтов и записывает по адресу третьего агрумента
	// https://msdn.microsoft.com/en-us/library/windows/desktop/ms740120(v=vs.85).aspx
	error = inet_pton(AF_INET, argv[1], &serverAddr.sin_addr.s_addr);
	if (error < 0)
	{
		wprintf(L"Ошибка inet_pton()\n");
		return 40;
	}

	// Теперь, имея сокет и структуру с адресом-портом, мы "привязываем" (binding) 
	// адрес-порт к нашему сокету. Зачем-то при этом структуру sockaddr_in надо привести
	// к структуре SOCKADDR. Это можно сделать и сишным способом (SOCKADDR*)&serverAddr
	// и по идее будет работать, но в С++ правильнее использовать одну из специальных операций
	// приведения. В данном случае это reinterpret_cast, которая как бы говорит, что 
	// SOCKADDR и sockaddr_in это два разных названия одной и той же сущности. Еще один пример,
	// где можно использовать reinterpret_cast это приведение между char и uint8_t, т.к. по сути
	// это одно и то же
	error = bind(serverSocket, reinterpret_cast<SOCKADDR*>(&serverAddr), sizeof(serverAddr));
	if (error != 0)
	{
		wprintf(L"Ошибка при вызове bind(): %d\n", WSAGetLastError());
		WSACleanup();
		return 50;
	}

	wprintf(L"Сервер успешно запущен по адресу %hs:%hs\n\n", argv[1], argv[2]);

	// Если в общем, то это бесконечный цикл ждет пока по сети прилезит запрос, анализирует код
	// операции, обслуживает запрос и возвращается к ожиданию новых запросов
	while (true)
	{
		// recvfrom - блокирующий вызов, т.е. программа приостанавливается в этом месте до тех
		// пор пока не будут получены какие-нибудь данные по сети в сокет serverSocket.
		// После того, как данные получены, функция возвращает количество полученных байтов
		// и выполнение программы продолжается. Данные записываются в буфер buf длиной
		// DATAGRAM_MAX_SIZE, в структуру clientAddr записывается адрес отправителя датаграммы,
		// этот адрес используется в последствии для отправки ответа
		// https://msdn.microsoft.com/en-us/library/windows/desktop/ms740120(v=vs.85).aspx
		int bytes = recvfrom(
				serverSocket, 
				buf, 
				DATAGRAM_MAX_SIZE, 
				0, 
				reinterpret_cast<SOCKADDR *>(&clientAddr), 
				&clientAddrSize);
		if (bytes == SOCKET_ERROR)
		{
			wprintf(L"Ошибка recvfrom(): %d\n", WSAGetLastError());
			return 60;
		}

		switch (buf[0])
		{
			// Пришел запрос на получение File Info
		case C_FILE_INFO_REQUEST:
		{
			wprintf(L"Получен запрос на FileInfo\n");
			FileInfo* requestedFile = nullptr;
			// В первую очередь мы проверяем не делал ли уже кто-то запрос для этого же
			// файла. Если да, то возвращаем из нашего map уже готовую структуру с уже
			// присвоенным идентификатором данных и посчитанным количеством блоков
			// it - специальный объект, который называется итератор. С помощью него
			// мы проходим по всем объектам коллекции и ищем подходящую и достаем из
			// нее указатель на FileInfo. Ключевое слово auto просит у компилятора
			// вывести тип переменной it самостоятельно, что возможно, т.к. структура
			// files определена и известна на этапе компиляции. В данном случае 
			// it имеет тип std::map<uint16_t, FileInfo*>::iterator
			// it->first в данном случае будет указывать на ключ uint16_t, а 
			// it->second на FileInfo*.
			// И еще надо понимать чем отличаются операторы "->" от ".". Если вкратце,
			// то it->first это то же самое, что (*it).first, то есть это совокупность
			// операции разыменования указателя (в данном случае указателя на итератор it)
			// и перехода по вложенному полю структуры/класса
			for (auto it = files.begin(); it != files.end(); ++it)
			{
				// _stricmp проводит сравнение двух строк без учета регистра символов
				// и возвращает 0 если строки равны.
				if (0 == _stricmp(it->second->name.c_str(), &buf[1]))
				{
					// Нашли нужную FileInfo, запомнили указатель на нее
					requestedFile = it->second;
					wprintf(L"Файл найден в списке\n");
					break;
				}
			}

			// Если выше ничего подходящего в ассоциативном массиве не нашлось, то 
			// будем создавать новую запись и добавлять ее в массив
			if (requestedFile == nullptr) {
				wprintf(L"Файл НЕ найден в списке\n");
				// TODO: Проверка переданного имени файла на длину, на наличие слэшей и т.д.
				bool fileNameIsGood = true;
				if (fileNameIsGood)
				{
					// Создали в памяти новый объект
					FileInfo* newRequest = new FileInfo();

					// Пробуем открыть файл по имени, пришедшему в сетевом запрос и лежащему
					// в буфере по первому индексу
					newRequest->file.open(&buf[1], std::ios::binary);

					// Проверяем успешно открыт файл 
					if (!newRequest->file.good())
					{
						// Если не успешно, то не забываем
						// убирать за собой какашки...
						delete newRequest;
						newRequest = nullptr;

						// ...отправляем клиенту ошибку...
						buf[0] = C_ERR_FILE_NOT_FOUND;
						sendto(serverSocket, 
								buf, 
								1, 
								0, 
								reinterpret_cast<SOCKADDR*>(&clientAddr), 
								clientAddrSize);

						// ...и возвращаемся слушать сетевой интерфейс
						break;
					}

					// Ну а если успешно, то присваеваем этому файлу очередной идентификатор
					newRequest->handler = fileHandlerCounter++;

					// Сохраняем его имя (если его попросят снова)
					// смотри строку программы с вызовом _stricmp
					newRequest->name = &buf[1];

					// Определяем размер файла в байтах
					uint64_t begin = newRequest->file.tellg();
					newRequest->file.seekg(0, std::ios::end);
					uint64_t end = newRequest->file.tellg();
					uint64_t sizeInBytes = end - begin;

					// Зная размер файла в байтах и размер блока считаем размер файла в блоках
					newRequest->blocksCount = static_cast<uint32_t>(sizeInBytes / MAX_BLOCK_SIZE);

					// Если остался остаток от деления, то будет еще один последний
					// обрезанный блок меньшего размера
					if (sizeInBytes % MAX_BLOCK_SIZE > 0) newRequest->blocksCount++;

					// Вставляем в ассоциативный массив наш объект
					files.insert(std::pair<uint16_t, FileInfo*>(newRequest->handler, newRequest));
					requestedFile = newRequest;
				}
				else
				{
					// Здесь типа отправляется ошибка, если Василий решил в имени файла
					// попросить что-нибудь типа "C:\Windows\win.ini"
					wprintf(L"Некорректное имя файла\n");
					buf[0] = C_ERR_FILE_NOT_FOUND;
					sendto(serverSocket, 
							buf, 
							1, 
							0, 
							reinterpret_cast<SOCKADDR*>(&clientAddr),
							clientAddrSize);
					break;
				}
			}

			wprintf(L"Отправляем FileInfo\n");

			// Обнуляем первые 7 байт буфера
			memset(buf, 0, 7);
			buf[0] = C_FILE_INFO_RESPONSE;

			// Тут арифметика указателей такая:
			// 1. buf[1] - это некоторое значение типа char (переменная)
			// 2. &buf[1] - это АДРЕС некоторого значения типа char (указатель на char)
			// 3. reinterpret_cast<uint16_t*>(&buf[1]) - будем интерпретировать этот адрес
			// на char как адрес на uint_16.
			// 4. *reinterpret_cast<uint16_t*>(&buf[1]) - наконец РАЗЫМЕНУЕМ этот указатель
			// на "как бы" uint_16 и получим его ЗНАЧЕНИЕ, куда сложим наше число
			// ИЧСХ запишется это число в память в little-endian. Вот она где собака порылась.
			*reinterpret_cast<uint16_t*>(&buf[1]) = requestedFile->handler;
			*reinterpret_cast<uint32_t*>(&buf[3]) = requestedFile->blocksCount;
			sendto(serverSocket, 
					buf, 
					7, 
					0, 
					reinterpret_cast<SOCKADDR*>(&clientAddr), 
					clientAddrSize);
			break;
		}
		// Запрос на получение диапазона блоков
		case C_FILE_BLOCK_RANGE_REQUEST:
		{
			// Получаем из запроса клиента идентификатор файла
			uint16_t fileIdentifier = *reinterpret_cast<uint32_t*>(&buf[1]);

			// Ищем по этому идентификатору в нашем ассоциативном массиве 
			// Структуру с данными по этому файлу вот таким образом
			// Соль в том, что поиск по ключу для ассоциативного массива 
			// это быстрая операция, а к скорости у нас требования 
			if (files.find(fileIdentifier) == files.end())
			{
				wprintf(L"Неверный идентификатор файла\n");
				buf[0] = C_ERR_FILE_NOT_FOUND;
				sendto(serverSocket, 
						buf, 
						1, 
						0, 
						reinterpret_cast<SOCKADDR*>(&clientAddr), 
						clientAddrSize);
				break;
			}

			// Достаем из буфера с какого блока клиенту нужны данные...
			uint32_t lowerBound = *reinterpret_cast<uint32_t*>(&buf[3]);

			// ...и по какой
			uint32_t upperBound = *reinterpret_cast<uint32_t*>(&buf[7]);
			wprintf(L"Запрос на получение блоков с %ld по %ld\n", lowerBound, upperBound);
			for (uint32_t i = lowerBound; i <= upperBound; i++)
			{
				SendBlock(i, files[fileIdentifier]);
			}
			break;
		}
		// Запрос на получение произвольного массива блоков
		case C_FILE_BLOCK_ARRAY_REQUEST:
		{
			// TODO: проверить типы. uint32_t -> uint16_t
			uint16_t fileIdentifier = *reinterpret_cast<uint32_t*>(&buf[1]);
			if (files.find(fileIdentifier) == files.end())
			{
				wprintf(L"Неверный идентификатор файла\n");
				buf[0] = C_ERR_FILE_NOT_FOUND;
				sendto(serverSocket, 
						buf, 
						1, 
						0, 
						reinterpret_cast<SOCKADDR*>(&clientAddr),
						clientAddrSize);
				break;
			}

			// Считаем количество блоков в запросе на основе длины полученной датаграммы
			uint16_t blocksCountRequested = (bytes - 3) / 4;

			// Сохраняем из буфера номера блоков в динамически создаваемый массив
			uint32_t* blocksRequested = new uint32_t[blocksCountRequested];
			for (uint16_t i = 0; i<blocksCountRequested; i++)
			{
				blocksRequested[i] = *reinterpret_cast<uint32_t*>(&buf[i * 4 + 3]);
			}
			for (uint16_t i = 0; i<blocksCountRequested; i++)
			{
				wprintf(L"Запрос блока %lu\n", blocksRequested[i]);
				SendBlock(blocksRequested[i], files[fileIdentifier]);
			}

			// Чистим кал
			delete[] blocksRequested;
			break;
		}
		default:
		{
			buf[0] = C_ERR_INVALID_OPERATION;
			sendto(serverSocket, 
					buf, 
					1, 
					0, 
					reinterpret_cast<SOCKADDR*>(&clientAddr), 
					clientAddrSize);
			wprintf(L"Неопознанный код операции. Отправлен код ошибки\n");
			break;
		}
		}
	}

	// В конце работы с сокетами необходимо вызвать эту
	// функцию. В принципе необязательно, т.к. после этого
	// мы сразу завершаем процесс и ресурсы в любом случае
	// будут освобождены операционной системой.
	WSACleanup();
	return 0;
}
/**************************************************************************************************/
/*                                                                                                */
/*                                        КОНЕЦ ФУНКЦИИ MAIN                                      */
/*                                                                                                */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                                                                                */
/*                                     НАЧАЛО ОБЪЯВЛЕНИЙ ФУНКЦИЙ                                  */
/*                                                                                                */
/**************************************************************************************************/
void SetConsoleToUTF8()
{
	_setmode(_fileno(stdout), _O_U16TEXT);
	_setmode(_fileno(stdin), _O_U16TEXT);
	_setmode(_fileno(stderr), _O_U16TEXT);
}

int CheckArgumens(int argc, char* argv[])
{
	// TODO: добавить дополнительные проверки на валидность ip, порта и т.д.
	if (argc != 3)
	{
		return 20;
	}
	return 0;
}

int WinSockInit()
{
	// Существуют версии WinSock: 1.0, 1.1, 2.0, 2.1, 2.2
	// Мы запрашиваем версию 2.2
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSAData wsaData;
	int16_t err;

	// Перед началом работы с сокетами необходимо вызвать
	// инициализирующую функцию и передать ей 
	// нужную нам версиию. При успешном вызове функции
	// должен вернуться код 0
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		return err;
	}

	// Библиотека инициализирована, но ее еще желательно проверить
	// на соответствие версии (должна быть 2.2).
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		WSACleanup();
		return 10;
	}

	return 0;
}

// Ключевое слово inline говорит нам, что на самом деле это не функция.
// Т.е. грубо говоря компилятор вместо компиляции этого в исполняемый код,
// копирования его по некоторому адресу в память, и вызова call на машинном
// языке с передачей параметров в стек с последующим возвратом ret в то место,
// откуда она была вызвана при каждом вызове "функции", просто копипастит 
// тело функции туда, откуда она вызывается. Таким образом это работает чутка
// быстрее, т.к. нет операций call, ret, push и pop и прочего блуждания по памяти
// но увеличивается размер исполняемого файла, т.к. этот код дублируется
// во всех местах, где вызывается эта "функция"
inline void SendBlock(uint32_t block, FileInfo* fileInfo)
{
	// Если запрашиваемый номер блока выходит за границы, то посылаем ошибку
	if (block > fileInfo->blocksCount - 1)
	{
		buf[0] = C_ERR_INVALID_BLOCK_REQUESTED;
		sendto(serverSocket, 
				buf, 
				1, 
				0, 
				reinterpret_cast<SOCKADDR*>(&clientAddr), 
				clientAddrSize);
		return;
	}

	buf[0] = C_FILE_BLOCK_RESPONSE;
	*reinterpret_cast<uint16_t*>(&buf[1]) = fileInfo->handler;
	*reinterpret_cast<uint32_t*>(&buf[3]) = block;

	// Установка курсора на нужное место файла.
	fileInfo->file.seekg(block*MAX_BLOCK_SIZE);
	// Читаем с этого места кусок данных размера MAX_BLOCK_SIZE
	fileInfo->file.read(&buf[7], MAX_BLOCK_SIZE);
	// Этот if сработает если мы не смогли считать кусок запрашиваемого размера
	// Обычно это происходит в том случае, если достигнут конец файла
	// В таком случае потом входит в состояние ошибки и необхдимо вызвать функцию clear()
	// чтобы он снова стал доступным
	if (!fileInfo->file) fileInfo->file.clear();

	// fileInfo->file.gcount() возвращает актуальное количество считанного куска данных. 
	// Таким образом корректируем размер датаграммы под последний кусок файла.
	sendto(serverSocket,
			buf, 
			static_cast<int>(fileInfo->file.gcount()) + 7, 
			0, 
			reinterpret_cast<SOCKADDR*>(&clientAddr), 
			clientAddrSize);
}
/**************************************************************************************************/
/*                                                                                                */
/*                                    КОНЕЦ ОБЪЯВЛЕНИЙ ФУНКЦИЙ                                    */
/*                                                                                                */
/**************************************************************************************************/