#include "client.hpp"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


sstd::socket::socket(const std::string& hostNameOrAddress_in, const std::string& service_in){
	this->hostNameOrAddress = hostNameOrAddress_in;
	this->service           = service_in;
}
sstd::socket::~socket(){
	::close(this->sock);
}

bool sstd::socket::open(){
	struct addrinfo info;
	info.ai_flags    = 0;
	info.ai_family   = AF_UNSPEC;   // allow IPv4 or IPv6
	info.ai_socktype = SOCK_STREAM; // SOCK_STREAM: TCP, SOCK_DGRAM: UDP
	info.ai_protocol = 0;
	
	struct addrinfo *pInfo;
	int ret = getaddrinfo(this->hostNameOrAddress.c_str(), this->service.c_str(), &info, &pInfo);
	if(ret!=0){ sstd::pdbg("ERROR: getaddrinfo() was failed: %s\n",gai_strerror(ret)); return false; }
	
	struct addrinfo *rp; // for repeat
	
	for(rp=pInfo; rp!=NULL; rp=rp->ai_next){
		this->sock = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

		if(this->sock!=-1){
			if(::connect(this->sock, rp->ai_addr, rp->ai_addrlen)==0){ break; } //接続に成功したので、for文を抜けて次の処理へ移行する。
			sstd::pdbg("ERROR: connect() was failed.\n");
			::close(this->sock);
		}else{
			sstd::pdbg("ERROR: socket() was failed.\n");
		}
	}

	if(rp==NULL){
		sstd::pdbg("ERROR: All connection was failed.\n");
		freeaddrinfo(pInfo);
		return false;
	}
	freeaddrinfo(pInfo);
	
	return true;
}

int sstd::socket::send(const std::string& msg){
	return ::write(this->sock, msg.c_str(), msg.length());
}
int sstd::socket::send_withPrint(const std::string& msg){
	std::vector<std::string> lines = sstd::splitByLine(msg);
	for(uint i=0; i<lines.size(); i++){ printf("Sending Data >> %s\n", lines[i].c_str()); }
	printf("\n");
	return sstd::socket::send(msg);
}

std::string sstd::socket::recv(bool& result){
	result=true;
	
	std::string recvMsg;
	char buf[1024*1024];
	for(int previous_readSize=0, readSize=1;
			previous_readSize != readSize;
			previous_readSize  = readSize	)
	{
		memset(buf, 0, sizeof(buf));
		readSize = ::read(this->sock, buf, sizeof(buf)-1);
		if(readSize<=0){ break; }
		buf[readSize] = '\0';
		
		recvMsg += buf;
	}
	if(recvMsg.size()==0){ result=false; }
	return recvMsg;
}

