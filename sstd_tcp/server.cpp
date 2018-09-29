#include "server.hpp"
#include <sys/socket.h>
#include <fcntl.h>
#include <netdb.h>
#include <sstd/sstd.hpp>

//-----------------------------------------------------------------------------------------------------------------------------------------------

fdState::fdState(){
	this->writeMsg = "";
}
fdState::~fdState(){}

//-----------------------------------------------------------------------------------------------------------------------------------------------

sstd::tcpSrv_nonblocking::tcpSrv_nonblocking(const uint maxEvents_in){
	this->eNum = 0;
	this->nFd  = 0;
	this->numOfListenLimits = 16;
	this->isProc = false;

	this->maxEvents = maxEvents_in;
	this->pState  = new struct fdState    [maxEvents];
	this->pEvents = new struct epoll_event[maxEvents];
}
sstd::tcpSrv_nonblocking::~tcpSrv_nonblocking(){
	delete[] this->pState;
	delete[] this->pEvents;
}

//---

bool sstd::tcpSrv_nonblocking::printInfo_fd(const struct sockaddr_storage& ss, socklen_t& ssLen){
	
	// get host (host name or its IP address) and port number
	char host[NI_MAXHOST];
	char port[NI_MAXSERV];
	int ret = getnameinfo((struct sockaddr*)&ss, ssLen, host, NI_MAXHOST, port, NI_MAXSERV, 0);
	if(ret!=0){
		if(ret==EAI_SYSTEM){ sstd::pdbg("ERROR: getnameinfo() failed: %s\n", strerror(errno)  );
		}       else       { sstd::pdbg("ERROR: getnameinfo() failed: %s\n", gai_strerror(ret)); }
		return false;
	}
	sstd::pdbg("info of feed: host: %s, port: %s\n", host, port);
		
	return true;
}

//---

bool sstd::tcpSrv_nonblocking::isEvent(){ return ((cEv.events & EPOLLIN) || (cEv.events & EPOLLOUT)); }
bool sstd::tcpSrv_nonblocking::ctlState_setR(const int& setFd){
	cEv={0};
	cEv.events = EPOLLIN;
	cEv.data.fd = setFd;
	if(epoll_ctl(epFd, EPOLL_CTL_ADD, setFd, &cEv)==-1){ sstd::pdbg("ERROR: epoll_ctl(): %s.\n", strerror(errno)); return false; }
	return true;
}
bool sstd::tcpSrv_nonblocking::ctlState_setW(const int& setFd){ return true; } // not implimented yet
bool sstd::tcpSrv_nonblocking::ctlState_setRW(const int& setFd){ return true; } // not implimented yet
bool sstd::tcpSrv_nonblocking::ctlState_rmR(const int& rmFd){
	cEv={0};
	cEv.data.fd = rmFd;
	if(pEvents[eNum].events & EPOLLOUT){ cEv.events |= EPOLLOUT; }
	if(epoll_ctl(epFd, EPOLL_CTL_MOD, rmFd, &cEv)==-1){ sstd::pdbg("ERROR: epoll_ctl(): %s.\n", strerror(errno)); return false; }
	return true;
}
bool sstd::tcpSrv_nonblocking::ctlState_rmW(const int& rmFd){
	cEv={0};
	cEv.data.fd = cFd;
	if(pEvents[eNum].events & EPOLLIN){ cEv.events |= EPOLLIN; }
	if(epoll_ctl(epFd, EPOLL_CTL_MOD, rmFd, &cEv)==-1){ sstd::pdbg("ERROR: epoll_ctl(): %s.\n", strerror(errno)); return false; }
	return true;
}
bool sstd::tcpSrv_nonblocking::ctlState_rmRW(const int& rmFd){ return true; } // not implimented yet
bool sstd::tcpSrv_nonblocking::ctlState_addR(const int& addFd){ return true; } // not implimented yet
bool sstd::tcpSrv_nonblocking::ctlState_addW(const int& addFd){
	cEv={0};
	cEv.data.fd = addFd;
	if(pEvents[eNum].events & EPOLLIN){ cEv.events |= EPOLLIN; }
	cEv.events |= EPOLLOUT;
	if(epoll_ctl(epFd, EPOLL_CTL_MOD, addFd, &cEv)==-1){ sstd::pdbg("ERROR: epoll_ctl(): %s.\n", strerror(errno)); return false; }
	return true;
}
bool sstd::tcpSrv_nonblocking::ctlState_addRW(const int& addFd){ return true; } // not implimented yet

//---

void set_nonblocking(int fd){ fcntl(fd, F_SETFL, O_NONBLOCK); }
bool sstd::tcpSrv_nonblocking::open(const uint portNum){
	sIn.sin_family      = AF_INET;
	sIn.sin_addr.s_addr = 0;
	sIn.sin_port        = htons(portNum);
	
	sock = socket(AF_INET, SOCK_STREAM, 0); // SOCK_STREAM : TCP/IP
	if(sock<0){ sstd::pdbg("ERROR: socket(): %s.\n", strerror(errno)); return false; }
	set_nonblocking(sock);
	
	// SO_REUSEADDR : in order to solve "ERROR: bind(): Address already in use.".
	// SO_REUSEPORT : 同一ポートに複数リスナー接続可能にする．
	int on = 1;
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int))!=0){ sstd::pdbg("ERROR: setsockopt(): %s.\n", strerror(errno)); ::close(sock); return false; }
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(int))!=0){ sstd::pdbg("ERROR: setsockopt(): %s.\n", strerror(errno)); ::close(sock); return false; }
	
	if(  bind(sock, (struct sockaddr*)&sIn, sizeof(sIn))<0){ sstd::pdbg("ERROR: bind(): %s. (You might need to run your program by \"$ sudo ./exe\".)\n", strerror(errno)); ::close(sock); return false; }
	if(listen(sock, numOfListenLimits)                  <0){ sstd::pdbg("ERROR: listen(): %s.\n",                                                         strerror(errno)); ::close(sock); return false; }
	
	// init epoll()
	if((epFd=epoll_create(maxEvents))<0){ sstd::pdbg("ERROR: epoll_create(): %s.\n", strerror(errno)); ::close(sock); return false; }
	if(!ctlState_setR(sock)){ sstd::pdbg("ERROR: ctlState_setR(): was failed.\n"); return false; }
	fd_offset = epFd+1; // newFd number will begin "epFd+1".
	
	return true;
}
bool sstd::tcpSrv_nonblocking::new_fd(){
	struct sockaddr_storage ss;
	socklen_t ssLen = sizeof(struct sockaddr_storage);
//	int newFd = accept(sock, (struct sockaddr*)&ss, &ssLen);
	int newFd = accept4(sock, (struct sockaddr*)&ss, &ssLen, SOCK_NONBLOCK); // accept4() can set SOCK_NONBLOCK flag
	if(newFd<0){ sstd::pdbg("ERROR: accept(): %s.\n", strerror(errno)); return false; }
//	sfet_nonblocking(newFd);
	
	// init state buffer
	sNum = newFd - fd_offset; if(sNum>=maxEvents){ sstd::pdbg("ERROR: accept(): Over the maxEvents.\n"); ::close(newFd); return false; }
	pState[sNum].writeMsg.clear();
	
	if(!printInfo_fd(ss, ssLen)){ sstd::pdbg("ERROR: printInfo_fd() faild.\n"); }
	
	// set epoll
	if(!ctlState_setR(newFd)){ sstd::pdbg("ERROR: ctlState_setR(): was failed.\n"); return false; } // If you want to receive the data, the program needs to wait until the "epoll_wait()" function will return "EPOLLIN" singal.
	
	return true;
}
bool sstd::tcpSrv_nonblocking::del_fd(int delFd){
	if(epoll_ctl(epFd, EPOLL_CTL_DEL, delFd, NULL)==-1){ sstd::pdbg("ERROR: epoll_ctl(): %s.\n", strerror(errno)); ::close(delFd); return false; }
	::close(delFd);
	return true;
}

bool sstd::tcpSrv_nonblocking::wait(){
 continue_SSTD_TCP_WAIT:
	
	if(isProc && !isEvent()){ sstd::pdbg("close socket: %d\n\n\n", cFd); del_fd(cFd); } // if there is no events, close socket
	isProc=false;
	
	// blocking here until reads() or write() functions enable to process data without waite time, or EPOLLERR.
	if(eNum==0){ if((nFd=epoll_wait(epFd, pEvents, maxEvents, -1))==-1){ sstd::pdbg("ERROR: epoll_wait(): %s.\n", strerror(errno)); ::close(sock); return false; }
	}   else   { eNum++; } // count up one state
	
	for(; eNum<nFd; eNum++){
		if(pEvents[eNum].data.fd==sock){
			// accept new socket
			if(!sstd::tcpSrv_nonblocking::new_fd()){ sstd::pdbg("ERROR: sstd::tcpSrv_nonblocking::new_fd() was failed.\n"); return false; }
		}else{
			// in order to process recv() or send()
			isProc=true;
			return true;
		}
	}
	eNum=0;
	goto continue_SSTD_TCP_WAIT; // same as for(;;){}, but i dont like deep indents.
}
bool sstd::tcpSrv_nonblocking::isRedy4recv(){ return (pEvents[eNum].events & EPOLLIN); }
bool sstd::tcpSrv_nonblocking::isRedy4send(){ return (pEvents[eNum].events & EPOLLOUT); }

bool sstd::tcpSrv_nonblocking::workerRecv(std::vector<uchar>& retOut, const uint& limitSize){
	
	retOut.resize(limitSize);
	ssize_t len = ::recv(cFd, &retOut[0], retOut.size(), 0);
	if(len<=0){
		retOut.resize( 0 );
		if(errno==EAGAIN || errno==EWOULDBLOCK){ ctlState_rmR(cFd); return false; }
		sstd::pdbg("ERROR: error number: %d, message: %s\n", errno, strerror(errno));
		del_fd(cFd);
		return false;
	}else{
		retOut.resize(len);
	}
	
	std::vector<uchar> buf(limitSize);
	for(;;){
		len = ::recv(cFd, &buf[0], buf.size(), 0);
		if(len<=0){ break; // len == '0' or '-1'
		}  else   { retOut.insert(retOut.end(), &buf[0], &buf[0]+len); }
		
		if(retOut.size()>=limitSize){ sstd::pdbg("ERROR: ret over limitSize\n"); return true; }
	}
	
	return true;
}
bool sstd::tcpSrv_nonblocking::recv(std::vector<uchar>& retOut, const uint& limitSize){
	sstd::pdbg("--- in recv(), fd: %d ---\n", eNum); // for debug
	
	// update to current states
	cFd  = pEvents[eNum].data.fd;
	sNum = cFd - fd_offset;
	
	sstd::pdbg("--- in recv(), eNum: %d, cFd: %d ---\n", eNum, cFd); // for debug
	
	if(workerRecv(retOut, limitSize)==false || retOut.size()==0){ return false; }
	
	// update epoll event (remove EPOLLIN)
	ctlState_rmR(cFd);
	
	return true;
}
bool sstd::tcpSrv_nonblocking::setSend(std::string& msg){
	
	// update epoll event (add EPOLLOUT)
	ctlState_addW(cFd);
	
	pState[sNum].writeMsg.swap(msg);
	msg.clear();
	
	return true;
}
bool sstd::tcpSrv_nonblocking::setSend(const char*& pMsg){
	std::string msg = pMsg;
	return sstd::tcpSrv_nonblocking::setSend(msg);
}
bool sstd::tcpSrv_nonblocking::send(){
	sstd::pdbg("--- in send(), fd: %d ---\n", eNum); // for debug
	
	// update to current states
	cFd  = pEvents[eNum].data.fd;
	sNum = cFd - fd_offset;
	
	if(pState[sNum].writeMsg.size()<=0){ del_fd(cFd); return false; }
	
	ssize_t result = ::send(cFd, pState[sNum].writeMsg.c_str(), pState[sNum].writeMsg.size(), MSG_NOSIGNAL); // MSG_NOSIGNAL: SIGPIPE を送信しない (送信されると，プログラムが終了する)
	if(result==-1){ sstd::pdbg("ERROR: ::send(): %s.\n", strerror(errno)); del_fd(cFd); return false; }
	
	// update epoll event (add EPOLLOUT)
	ctlState_rmW(cFd);
	
	return true;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------

