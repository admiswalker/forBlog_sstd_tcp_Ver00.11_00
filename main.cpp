#include "./sstd_tcp/server.hpp"
#include "./sstd_tcp/client.hpp"

#include <unordered_map>
#include <unistd.h>   // for fork
#include <sys/wait.h> // for fork

//-----------------------------------------------------------------------------------------------------------------------------------------------
// client sample

//int httpc(){
//	sstd::sleep_ms(500);
//	
//	sstd::socket sock("localhost", "80");
//	if(!sock.open()){ sstd::pdbg("ERROR: sstd::socket::open() was failed.\n"); return -1; }
//	
//	std::string msg;
//	msg += "GET /index.html HTTP/1.1\r\n";
//	msg += "Host: localhost\r\n";
//	msg += "Connection: keep-alive\r\n";
//	msg += "Upgrade-Insecure-Requests: 1\r\n";
//	msg += "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/66.0.3359.139 Safari/537.36\r\n";
//	msg += "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8\r\n";
//	msg += "Accept-Encoding: gzip, deflate, br\r\n";
//	msg += "Accept-Language: ja,en-US;q=0.9,en;q=0.8\r\n";
//	msg += "\r\n";
//	//sock.send(msg);
//	sock.send_withPrint(msg);
//	
//	bool TF;
//	std::string ret = sock.recv(TF);
//	sstd::printn(ret);
//	
//	return 0;
//}

//-----------------------------------------------------------------------------------------------------------------------------------------------
// server sample

std::unordered_map<std::string, bool> getURL_table(bool& result, const char* URL_rootPath){ // const char* rootPath = "./contents/root";
	result = true;
	
	std::vector<std::string> ret;
	if(!sstd::getAllFile(ret, URL_rootPath)){ sstd::pdbg("ERROR: getAllFile() failed.\n"); result=false; return std::unordered_map<std::string, bool>(); }
	
	std::unordered_map<std::string, bool> URL_table(ret.size()); // In order to avoid directory traversal
	for(uint i=0; i<ret.size(); i++){ URL_table[ret[i]]=true; }  // add on a hash table
	return URL_table;
}
std::string getURL(std::unordered_map<std::string, bool>& URL_table, const char* str_in){
	std::vector<std::string> vecLine = sstd::splitByLine(str_in);
	sstd::printn_all(vecLine); // fro debug

	std::string ret;
	for(uint i=0; i<vecLine.size(); i++){
		std::vector<std::string> vecRow = sstd::split(vecLine[i], ' ');
		if(vecRow.size()<2){ continue; }
		
		if      (sstd::strcmp(vecRow[0], "GET"                       )){
			auto itr = URL_table.find("./contents/root"+vecRow[1]);
			if(itr!=URL_table.end()){ ret=itr->first; } // iterator により提供される key は，iterator の開放と同時に開放される一時変数．従って，ポインタのみのコピーしても開放されてしまう．
		}else if(sstd::strcmp(vecRow[0], "Host:"                     )){
		}else if(sstd::strcmp(vecRow[0], "Connection:"               )){ // keep-alive
		}else if(sstd::strcmp(vecRow[0], "Cache-Control:"            )){ // max-age=0
		}else if(sstd::strcmp(vecRow[0], "Upgrade-Insecure-Requests:")){ // Upgrade-Insecure-Requests: 1
		}else if(sstd::strcmp(vecRow[0], "User-Agent:"               )){ // User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/66.0.3359.139 Safari/537.36
		}else if(sstd::strcmp(vecRow[0], "Accept:"                   )){ // text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8
		}else if(sstd::strcmp(vecRow[0], "Accept-Encoding:"          )){ // Accept-Encoding: gzip, deflate, br
		}else if(sstd::strcmp(vecRow[0], "Accept-Language:"          )){ // ja,en-US;q=0.9,en;q=0.8
		}
	}
	
	return ret;
}
std::string genMsg(std::string& requestURL){
	std::string msg;
	
	sstd::printn_all(requestURL); // ./contents/root/index.html
	if(requestURL.size()!=0){
		// status 200
		std::string rawFile = sstd::readAll(requestURL); // There is no problem with BOM
		msg += "HTTP/1.1 200 OK\r\n";
		msg += sstd::ssprintf("Content-Length: %d\r\n", rawFile.size());
		msg += "Content-Type: text/html\r\n";
		msg += "\r\n";
		msg += rawFile;
	}else{
		// 404
		std::string rawFile = sstd::readAll("./contents/statusCodes/404.html"); // There is no problem with BOM
		msg += "HTTP/1.1 404 Not Found\r\n";
		msg += "Connection: close\r\n";
		msg += "Content-Type: text/html\r\n";
		msg += sstd::ssprintf("Content-Length: %d\r\n", rawFile.size());
		msg += "\r\n";
		msg += rawFile;
		
	//	msg += "HTTP/1.1 404 Not Found\r\n";
	//	msg += "Connection: close\r\n";
	//	msg += "Content-Length: 0\r\n";
	//	msg += "\r\n";
	}
	
	return msg;
}
void httpd(const uint portNum, const char* URL_rootPath){

	// - init URL table -----------------------------------------------------------------
	bool result;
	std::unordered_map<std::string, bool> URL_table = getURL_table(result, URL_rootPath);
	if(!result){ sstd::pdbg("ERROR: getURL_table() is failed\n"); return; }
	// ---------------------------------------------------------- end of init URL table -
	
	uint limitSize = 1024; // limit size of recv buf.
	uint maxEvents = 1024; // limit size of events
	sstd::tcpSrv_nonblocking srv(maxEvents); if(!srv.open(portNum)){ sstd::pdbg("ERROR: sstd::tcpSrv_nonblocking::open() was faild.\n"); return; }
	
	for(;;){
		srv.wait(); // blocking here
		
		if(srv.isRedy4recv()){
			std::vector<uint8> rhs;
			if(!srv.recv(rhs, limitSize)){ sstd::pdbg("failed\n"); continue; } rhs.push_back('\n');
			std::string requestURL = getURL(URL_table, (const char*)&rhs[0]);
			std::string msg = genMsg(requestURL);
			srv.setSend(msg);
		}else if(srv.isRedy4send()){
			//bool result = srv.send();
			srv.send();
		}
	}
}

//-----------------------------------------------------------------------------------------------------------------------------------------------

// usage
//   1. $ sudo ./exe
//   2. accessing to http://localhost/index.html

// how to check ip address and access from the other machine.
//   1. $ ip a
//   2. an address begin 192.168.XXX.XXX is your local address.
//   3. A machine like a smart phone on the same local net work is able to access httpd by "http://192.168.XXX.XXX/index.html".

int main(){
	setvbuf(stdout, NULL, _IONBF, 0); // output immediately without buffering (not waiting line feed code)
	
	// - settings --------------------------------------------------------
	const uint portNum          =  80;               // Port number of http is 80. Web browser requires to specify port number without 80 using ':' option like "http://192.168.XXX.XXX/index.html:8080".
	const char* URL_rootPath    = "./contents/root"; // Root directory of httpd
	// ------------------------------------------------- end of settings -
	
	httpd(portNum, URL_rootPath); // http Daemon
	
	return 0;
}

