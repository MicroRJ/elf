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


// elAPI int netlib_init(elf_State *R) {
// 	WSADATA data;
// 	WSAStartup(MAKEWORD(2,2),&data);
// 	return 0;
// }


// elAPI int netlib_close(elf_State *R) {
// 	WSACleanup();
// 	return 0;
// }


// elAPI int netlib_listen(elf_State *R) {
// 	SOCKET handle = (SOCKET) elf_get_sys(R,0);
// 	int error = listen(handle,SOMAXCONN);
// 	elf_add_int(R,error!=SOCKET_ERROR);
// 	return 1;
// }


// elAPI int netlib_accept(elf_State *R) {
// 	SOCKET handle = (SOCKET) elf_get_sys(R,0);
// 	SOCKET client = accept(handle,NULL,NULL);
// 	elf_add_sys(R,(elf_Handle)client);
// 	return 1;
// }


// elAPI int netlib_pollclient(elf_State *R) {
// 	SOCKET handle = (SOCKET) elf_get_sys(R,0);
// 	fd_set ready;
// 	FD_ZERO(&ready);
// 	FD_SET(handle,&ready);
// 	TIMEVAL timeout = {0};
// 	int result = select(0,&ready,NULL,NULL,&timeout);
//    if (FD_ISSET(handle,&ready)) {
//       SOCKET client = accept(handle,NULL,NULL);
//       ASSERT(client != INVALID_SOCKET);
// 		elf_add_sys(R,(elf_Handle)client);
//    } else elf_push_nil(R);
// 	return 1;
// }


// elAPI int netlib_tcpserver(elf_State *R) {
// 	elf_String *addrnameS = elf_get_string(R,0);
// 	elf_String *addrportS = elf_get_string(R,1);
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
// 		elf_add_sys(R,(elf_Handle)thesocket);
// 	} else elf_push_nil(R);

// 	return 1;
// }


// elAPI int netlib_tcpclient(elf_State *R) {
// 	elf_String *addrnameS = elf_get_string(R,0);
// 	elf_String *addrportS = elf_get_string(R,1);
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
// 		elf_add_sys(R,(elf_Handle)thesocket);
// 	} else elf_push_nil(R);
// 	return 1;
// }


// elAPI int netlib_send(elf_State *R) {
// 	/* todo: make this a class? */
// 	SOCKET socket = (SOCKET) elf_get_sys(R,0);
// 	elf_String *payload = elf_get_string(R,1);
// 	LMSG message = { payload->length };
// 	elf_Int sent = 0;
// 	sent += send(socket,(char*)&message,sizeof(message),0);
// 	sent += send(socket,payload->c,payload->length,0);
// 	elf_add_int(R,sent);
// 	return 1;
// }


// elAPI int netlib_ioctl(elf_State *R) {
// 	SOCKET socket = (SOCKET) elf_get_sys(R,0);
// 	long mode = 1;
// 	int error = ioctlsocket(socket,FIONBIO,&mode);
// 	elf_add_int(R,error == 0);
// 	return 1;
// }


// elAPI int netlib_recv(elf_State *R) {
// 	SOCKET socket = (SOCKET) elf_get_sys(R,0);
// 	LMSG message = {0};
// 	if (recv(socket,(char*)&message,sizeof(message),0) != -1) {
// 		if (message.length != 0) {
// 			elf_Int length = message.length;
// 			elf_String *obj = elf_alloc_string2(R,length);
// 			elf_add_str(R,obj);
// 			char *cursor = obj->c;
// 			do {
// 				elf_Int result = recv(socket,cursor,length,0);
// 				if (result == SOCKET_ERROR) {
// 					int error = WSAGetLastError();
// 					if (error == WSAEWOULDBLOCK) {
// 						/* todo: retry until a certain timeout? */
// 					} else {
// 						char erbuf[0x100];
// 						sys_get_error_msg(error,erbuf,sizeof(erbuf));
// 						elf_error_log("netlib sys error '%i': %s",error,erbuf);
// 						break;
// 					}
// 				} else if (result == 0) {
// 					/* connection closed gracefully, simply break */
// 					break;
// 				} else {
// 					ASSERT(result > 0);
// 					length -= result;
// 					cursor += result;
// 				}
// 			} while (length != 0);
// 			*cursor = 0;
// 		} else elf_push_nil(R);
// 	} else elf_push_nil(R);
// 	return 1;
// }
// #else
// elAPI int netlib_init(elf_State *R) { return 0; };
// elAPI int netlib_close(elf_State *R) { return 0; };
// elAPI int netlib_listen(elf_State *R) { return 0; };
// elAPI int netlib_accept(elf_State *R) { return 0; };
// elAPI int netlib_pollclient(elf_State *R) { return 0; };
// elAPI int netlib_tcpserver(elf_State *R) { return 0; };
// elAPI int netlib_tcpclient(elf_State *R) { return 0; };
// elAPI int netlib_send(elf_State *R) { return 0; };
// elAPI int netlib_ioctl(elf_State *R) { return 0; };
// elAPI int netlib_recv(elf_State *R) { return 0; };
// #endif


// elAPI void elf_netlib_loadfunctions(elf_State *R) {
// 	elf_Module *md = R->md;

// 	elf_gsetx_cfn(R,"elf.sockets.init",netlib_init);
// 	elf_gsetx_cfn(R,"elf.sockets.close",netlib_close);
// 	elf_gsetx_cfn(R,"elf.sockets.listen",netlib_listen);
// 	elf_gsetx_cfn(R,"elf.sockets.accept",netlib_accept);
// 	elf_gsetx_cfn(R,"elf.sockets.pollclient",netlib_pollclient);
// 	elf_gsetx_cfn(R,"elf.sockets.tcpserver",netlib_tcpserver);
// 	elf_gsetx_cfn(R,"elf.sockets.tcpclient",netlib_tcpclient);
// 	elf_gsetx_cfn(R,"elf.sockets.send",netlib_send);
// 	elf_gsetx_cfn(R,"elf.sockets.recv",netlib_recv);
// 	elf_gsetx_cfn(R,"elf.sockets.ioctl",netlib_ioctl);
// }

