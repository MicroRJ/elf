/*
** See Copyright Notice In elf.h
** lnetlib.c
** Very Simple Sockets Lib
*/


/* this is just to get some basic games going on */

#if !defined(PLATFORM_WEB)

# pragma comment(lib,"Ws2_32")

#include "Winsock2.h"
#include "ws2tcpip.h"
#include   "ws2def.h"

typedef struct LMSG {
	unsigned int length;
} LMSG;



elf_api int netlib_init(elState *R) {
	WSADATA data;
	WSAStartup(MAKEWORD(2,2),&data);
	return 0;
}


elf_api int netlib_close(elState *R) {
	WSACleanup();
	return 0;
}


elf_api int netlib_listen(elState *R) {
	SOCKET handle = (SOCKET) elf_getsys(R,0);
	int error = listen(handle,SOMAXCONN);
	elf_pushint(R,error!=SOCKET_ERROR);
	return 1;
}


elf_api int netlib_accept(elState *R) {
	SOCKET handle = (SOCKET) elf_getsys(R,0);
	SOCKET client = accept(handle,NULL,NULL);
	elf_pushsys(R,(elHandle)client);
	return 1;
}


elf_api int netlib_pollclient(elState *R) {
	SOCKET handle = (SOCKET) elf_getsys(R,0);
	fd_set ready;
	FD_ZERO(&ready);
	FD_SET(handle,&ready);
	TIMEVAL timeout = {0};
	int result = select(0,&ready,NULL,NULL,&timeout);
   if (FD_ISSET(handle,&ready)) {
      SOCKET client = accept(handle,NULL,NULL);
      elf_ensure(client != INVALID_SOCKET);
		elf_pushsys(R,(elHandle)client);
   } else elf_pushnil(R);
	return 1;
}


elf_api int netlib_tcpserver(elState *R) {
	elString *addrnameS = elf_getstr(R,0);
	elString *addrportS = elf_getstr(R,1);
	char *addrname = addrnameS ? addrnameS->c : 0;
	char *addrport = addrportS ? addrportS->c : 0;
	ADDRINFOA idealaddr = {0};
	idealaddr.ai_flags = AI_PASSIVE;
	idealaddr.ai_family = AF_INET;
	idealaddr.ai_socktype = SOCK_STREAM;
	idealaddr.ai_protocol = IPPROTO_TCP;
	ADDRINFOA *addrinfo = NULL;
	getaddrinfo(addrname,addrport,&idealaddr,&addrinfo);

	SOCKET thesocket = socket(addrinfo->ai_family,addrinfo->ai_socktype,addrinfo->ai_protocol);
	int error = bind(thesocket,addrinfo->ai_addr,addrinfo->ai_addrlen);
	if(error != SOCKET_ERROR) {
		elf_pushsys(R,(elHandle)thesocket);
	} else elf_pushnil(R);

	return 1;
}


elf_api int netlib_tcpclient(elState *R) {
	elString *addrnameS = elf_getstr(R,0);
	elString *addrportS = elf_getstr(R,1);
	char *addrname = addrnameS ? addrnameS->c : 0;
	char *addrport = addrportS ? addrportS->c : 0;
	ADDRINFOA idealaddr = {0};
	idealaddr.ai_flags = AI_PASSIVE;
	idealaddr.ai_family = AF_INET;
	idealaddr.ai_socktype = SOCK_STREAM;
	idealaddr.ai_protocol = IPPROTO_TCP;
	ADDRINFOA *addrinfo = NULL;
	getaddrinfo(addrname,addrport,&idealaddr,&addrinfo);

	SOCKET thesocket = socket(addrinfo->ai_family,addrinfo->ai_socktype,addrinfo->ai_protocol);

	int error = connect(thesocket,addrinfo->ai_addr,addrinfo->ai_addrlen);
	if(error != SOCKET_ERROR) {
		elf_pushsys(R,(elHandle)thesocket);
	} else elf_pushnil(R);
	return 1;
}


elf_api int netlib_send(elState *R) {
	/* todo: make this a class? */
	SOCKET socket = (SOCKET) elf_getsys(R,0);
	elString *payload = elf_getstr(R,1);
	LMSG message = { payload->length };
	elInteger sent = 0;
	sent += send(socket,(char*)&message,sizeof(message),0);
	sent += send(socket,payload->c,payload->length,0);
	elf_pushint(R,sent);
	return 1;
}


elf_api int netlib_ioctl(elState *R) {
	SOCKET socket = (SOCKET) elf_getsys(R,0);
	long mode = 1;
	int error = ioctlsocket(socket,FIONBIO,&mode);
	elf_pushint(R,error == 0);
	return 1;
}


elf_api int netlib_recv(elState *R) {
	SOCKET socket = (SOCKET) elf_getsys(R,0);
	LMSG message = {0};
	if (recv(socket,(char*)&message,sizeof(message),0) != -1) {
		if (message.length != 0) {
			elInteger length = message.length;
			elString *obj = elf_newstrlen(R,length);
			elf_pushstr(R,obj);
			char *cursor = obj->c;
			do {
				elInteger result = recv(socket,cursor,length,0);
				if (result == SOCKET_ERROR) {
					int error = WSAGetLastError();
					if (error == WSAEWOULDBLOCK) {
						/* todo: retry until a certain timeout? */
					} else {
						char erbuf[0x100];
						sys_geterrormsg(error,erbuf,sizeof(erbuf));
						elf_logerror("netlib sys error '%i': %s",error,erbuf);
						break;
					}
				} else if (result == 0) {
					/* connection closed gracefully, simply break */
					break;
				} else {
					elf_ensure(result > 0);
					length -= result;
					cursor += result;
				}
			} while (length != 0);
			*cursor = 0;
		} else elf_pushnil(R);
	} else elf_pushnil(R);
	return 1;
}
#else
elf_api int netlib_init(elState *R) { return 0; };
elf_api int netlib_close(elState *R) { return 0; };
elf_api int netlib_listen(elState *R) { return 0; };
elf_api int netlib_accept(elState *R) { return 0; };
elf_api int netlib_pollclient(elState *R) { return 0; };
elf_api int netlib_tcpserver(elState *R) { return 0; };
elf_api int netlib_tcpclient(elState *R) { return 0; };
elf_api int netlib_send(elState *R) { return 0; };
elf_api int netlib_ioctl(elState *R) { return 0; };
elf_api int netlib_recv(elState *R) { return 0; };
#endif


elf_api void netlib_load(elState *R) {
	elModule *md = R->md;
	lang_addglobal(md,elf_pushnewstr(R,"listen"),elf_valbid(netlib_listen));
	lang_addglobal(md,elf_pushnewstr(R,"accept"),elf_valbid(netlib_accept));
	lang_addglobal(md,elf_pushnewstr(R,"pollclient"),elf_valbid(netlib_pollclient));
	lang_addglobal(md,elf_pushnewstr(R,"tcpserver"),elf_valbid(netlib_tcpserver));
	lang_addglobal(md,elf_pushnewstr(R,"tcpclient"),elf_valbid(netlib_tcpclient));
	lang_addglobal(md,elf_pushnewstr(R,"netlib_init"),elf_valbid(netlib_init));
	lang_addglobal(md,elf_pushnewstr(R,"netlib_close"),elf_valbid(netlib_close));
	lang_addglobal(md,elf_pushnewstr(R,"send"),elf_valbid(netlib_send));
	lang_addglobal(md,elf_pushnewstr(R,"recv"),elf_valbid(netlib_recv));
	lang_addglobal(md,elf_pushnewstr(R,"ioctl"),elf_valbid(netlib_ioctl));
}


