#pragma once
#include <sstd/sstd.hpp>

namespace sstd{ class socket; }

class sstd::socket{
private:
	// Socketの通信部分
	int sock;
	std::string hostNameOrAddress; //ex:[www.google.co.jp]
	std::string service;           //ex:[http]	//ポート番号を指定する代わり。(httpのポート番号80番をここに指定しても上手く動作する事を確認した。)
	
public:
	socket(const std::string& hostNameOrAddress_in, const std::string& service_in);
	~socket();
	
	bool open();
	
	int send          (const std::string& msg);
	int send_withPrint(const std::string& msg);
	// RETURN VALUES (https://www.openssl.org/docs/man1.0.2/ssl/SSL_write.html)
	// The following return values can occur:
	//   >  0: The write operation was successful, the return value is the number of bytes actually written to the TLS/SSL connection.
	//   <= 0: The write operation was not successful, because either the connection was closed, an error occurred or action must be taken by the calling process. Call SSL_get_error() with the return value ret to find out the reason.
	//         SSLv2 (deprecated) does not support a shutdown alert protocol, so it can only be detected, whether the underlying connection was closed. It cannot be checked, why the closure happened.
	//         Old documentation indicated a difference between 0 and -1, and that -1 was retryable. You should instead call SSL_get_error() to find out if it's retryable.

	std::string recv(bool& result);
};

// http client
//int httpc();





