#pragma once
#include <sstd/sstd.hpp>

#include <netinet/in.h> // for sockaddr_in
#include <sys/epoll.h>  // for epoll

//-----------------------------------------------------------------------------------------------------------------------------------------------

struct fdState{
private:
public:
	fdState();
	~fdState();
	
	std::string writeMsg;
};

//-----------------------------------------------------------------------------------------------------------------------------------------------

namespace sstd{ class tcpSrv_nonblocking; }

class sstd::tcpSrv_nonblocking{
private:
	// settings
	int numOfListenLimits;
	uint maxEvents;

	// for init
	struct sockaddr_in sIn;
	
	// for all
	int  sock;              // listening socket
	int  cFd;               // current feed
	int  eNum;              // event number
	uint sNum;              // state number
	struct fdState* pState; // status set
	
	// for epoll
	int epFd;                    // epoll feed
	int nFd;                     // number of feeds
	int fd_offset;               // offset of feed number
	struct epoll_event* pEvents; // events set
	struct epoll_event cEv;      // current epoll event

	// for event loop
	bool isProc;
	
	bool new_fd();
	bool del_fd(int delFd);
	bool workerRecv(std::vector<uchar>& retOut, const uint& limitSize);
	bool printInfo_fd(const struct sockaddr_storage& ss, socklen_t& ssLen);
	
	bool isEvent();
	bool ctlState_setR (const int& setFd); // control state: set read
	bool ctlState_setW (const int& setFd); // control state: set write
	bool ctlState_setRW(const int& setFd); // control state: set read and write
	bool ctlState_rmR  (const int&  rmFd); // control state: remove read
	bool ctlState_rmW  (const int&  rmFd); // control state: remove write
	bool ctlState_rmRW (const int&  rmFd); // control state: remove read and write
	bool ctlState_addR (const int& addFd); // control state: add read
	bool ctlState_addW (const int& addFd); // control state: add write
	bool ctlState_addRW(const int& addFd); // control state: add read and write
	
public:
	tcpSrv_nonblocking(const uint maxEvents_in);
	~tcpSrv_nonblocking();
	
	bool open(const uint portNum);
	
	bool wait();
	bool isRedy4recv();
	bool isRedy4send();
	
	bool recv(std::vector<uchar>& retOut, const uint& limitSize);
	bool setSend(std::string& msg);
	bool setSend(const char*& pMsg);
	bool send();
};

//-----------------------------------------------------------------------------------------------------------------------------------------------

