// /*
// ** See Copyright Notice In elf.h
// ** socketslib.c
// ** Very Simple Sockets Lib
// */


// /* ------------------------------------
// 	Very Simple Sockets Library
// 	Just to get some basic games
// 	going on.
// --------------------------------*/

// #if !defined(PLATFORM_WEB)

// typedef struct LMSG {
// 	unsigned int length;
// } LMSG;


// elAPI int netlib_init(elState *R) {
// 	WSADATA data;
// 	WSAStartup(MAKEWORD(2,2),&data);
// 	return 0;
// }


// elAPI int netlib_close(elState *R) {
// 	WSACleanup();
// 	return 0;
// }


// elAPI int netlib_listen(elState *R) {
// 	SOCKET handle = (SOCKET) elf_get_handle(R,0);
// 	int error = listen(handle,SOMAXCONN);
// 	elPUSHINT(R,error!=SOCKET_ERROR);
// 	return 1;
// }


// elAPI int netlib_accept(elState *R) {
// 	SOCKET handle = (SOCKET) elf_get_handle(R,0);
// 	SOCKET client = accept(handle,NULL,NULL);
// 	elPUSHSYS(R,(elHandle)client);
// 	return 1;
// }


// elAPI int netlib_pollclient(elState *R) {
// 	SOCKET handle = (SOCKET) elf_get_handle(R,0);
// 	fd_set ready;
// 	FD_ZERO(&ready);
// 	FD_SET(handle,&ready);
// 	TIMEVAL timeout = {0};
// 	int result = select(0,&ready,NULL,NULL,&timeout);
//    if (FD_ISSET(handle,&ready)) {
//       SOCKET client = accept(handle,NULL,NULL);
//       elASSERT(client != INVALID_SOCKET);
// 		elPUSHSYS(R,(elHandle)client);
//    } else elf_pnil(R);
// 	return 1;
// }


// elAPI int netlib_tcpserver(elState *R) {
// 	elString *addrnameS = elf_get_string(R,0);
// 	elString *addrportS = elf_get_string(R,1);
// 	char *addrname = addrnameS ? addrnameS->c : 0;
// 	char *addrport = addrportS ? addrportS->c : 0;
// 	ADDRINFOA idealaddr = {0};
// 	idealaddr.ai_flags = AI_PASSIVE;
// 	idealaddr.ai_family = AF_INET;
// 	idealaddr.ai_socktype = SOCK_STREAM;
// 	idealaddr.ai_protocol = IPPROTO_TCP;
// 	ADDRINFOA *addrinfo = NULL;
// 	getaddrinfo(addrname,addrport,&idealaddr,&addrinfo);

// 	SOCKET thesocket = socket(addrinfo->ai_family,addrinfo->ai_socktype,addrinfo->ai_protocol);
// 	int error = bind(thesocket,addrinfo->ai_addr,addrinfo->ai_addrlen);
// 	if(error != SOCKET_ERROR) {
// 		elPUSHSYS(R,(elHandle)thesocket);
// 	} else elf_pnil(R);

// 	return 1;
// }


// elAPI int netlib_tcpclient(elState *R) {
// 	elString *addrnameS = elf_get_string(R,0);
// 	elString *addrportS = elf_get_string(R,1);
// 	char *addrname = addrnameS ? addrnameS->c : 0;
// 	char *addrport = addrportS ? addrportS->c : 0;
// 	ADDRINFOA idealaddr = {0};
// 	idealaddr.ai_flags = AI_PASSIVE;
// 	idealaddr.ai_family = AF_INET;
// 	idealaddr.ai_socktype = SOCK_STREAM;
// 	idealaddr.ai_protocol = IPPROTO_TCP;
// 	ADDRINFOA *addrinfo = NULL;
// 	getaddrinfo(addrname,addrport,&idealaddr,&addrinfo);

// 	SOCKET thesocket = socket(addrinfo->ai_family,addrinfo->ai_socktype,addrinfo->ai_protocol);

// 	int error = connect(thesocket,addrinfo->ai_addr,addrinfo->ai_addrlen);
// 	if(error != SOCKET_ERROR) {
// 		elPUSHSYS(R,(elHandle)thesocket);
// 	} else elf_pnil(R);
// 	return 1;
// }


// elAPI int netlib_send(elState *R) {
// 	/* todo: make this a class? */
// 	SOCKET socket = (SOCKET) elf_get_handle(R,0);
// 	elString *payload = elf_get_string(R,1);
// 	LMSG message = { payload->length };
// 	elInteger sent = 0;
// 	sent += send(socket,(char*)&message,sizeof(message),0);
// 	sent += send(socket,payload->c,payload->length,0);
// 	elPUSHINT(R,sent);
// 	return 1;
// }


// elAPI int netlib_ioctl(elState *R) {
// 	SOCKET socket = (SOCKET) elf_get_handle(R,0);
// 	long mode = 1;
// 	int error = ioctlsocket(socket,FIONBIO,&mode);
// 	elPUSHINT(R,error == 0);
// 	return 1;
// }


// elAPI int netlib_recv(elState *R) {
// 	SOCKET socket = (SOCKET) elf_get_handle(R,0);
// 	LMSG message = {0};
// 	if (recv(socket,(char*)&message,sizeof(message),0) != -1) {
// 		if (message.length != 0) {
// 			elInteger length = message.length;
// 			elString *obj = elf_new_lstring(R,length);
// 			elf_pstr(R,obj);
// 			char *cursor = obj->c;
// 			do {
// 				elInteger result = recv(socket,cursor,length,0);
// 				if (result == SOCKET_ERROR) {
// 					int error = WSAGetLastError();
// 					if (error == WSAEWOULDBLOCK) {
// 						/* todo: retry until a certain timeout? */
// 					} else {
// 						char erbuf[0x100];
// 						sys_geterrormsg(error,erbuf,sizeof(erbuf));
// 						elf_logerror("netlib sys error '%i': %s",error,erbuf);
// 						break;
// 					}
// 				} else if (result == 0) {
// 					/* connection closed gracefully, simply break */
// 					break;
// 				} else {
// 					elASSERT(result > 0);
// 					length -= result;
// 					cursor += result;
// 				}
// 			} while (length != 0);
// 			*cursor = 0;
// 		} else elf_pnil(R);
// 	} else elf_pnil(R);
// 	return 1;
// }
// #else
// elAPI int netlib_init(elState *R) { return 0; };
// elAPI int netlib_close(elState *R) { return 0; };
// elAPI int netlib_listen(elState *R) { return 0; };
// elAPI int netlib_accept(elState *R) { return 0; };
// elAPI int netlib_pollclient(elState *R) { return 0; };
// elAPI int netlib_tcpserver(elState *R) { return 0; };
// elAPI int netlib_tcpclient(elState *R) { return 0; };
// elAPI int netlib_send(elState *R) { return 0; };
// elAPI int netlib_ioctl(elState *R) { return 0; };
// elAPI int netlib_recv(elState *R) { return 0; };
// #endif


// elAPI void elf_netlib_loadfunctions(elState *R) {
// 	elModule *md = R->md;

// 	elf_register_binding(R,"elf.sockets.init",netlib_init);
// 	elf_register_binding(R,"elf.sockets.close",netlib_close);
// 	elf_register_binding(R,"elf.sockets.listen",netlib_listen);
// 	elf_register_binding(R,"elf.sockets.accept",netlib_accept);
// 	elf_register_binding(R,"elf.sockets.pollclient",netlib_pollclient);
// 	elf_register_binding(R,"elf.sockets.tcpserver",netlib_tcpserver);
// 	elf_register_binding(R,"elf.sockets.tcpclient",netlib_tcpclient);
// 	elf_register_binding(R,"elf.sockets.send",netlib_send);
// 	elf_register_binding(R,"elf.sockets.recv",netlib_recv);
// 	elf_register_binding(R,"elf.sockets.ioctl",netlib_ioctl);
// }

