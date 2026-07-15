// See Copyright Notice In elf.h

// Todo, proper platform abstraction!
#define WIN32_LEAN_AND_MEAN
#include <Winsock2.h>
#include <ws2tcpip.h>
#include  <windows.h>

typedef struct
{
	u32   size;
	char  data[];
}
AtomPacket;


ELF_FUNCTION(l_sockets_init)
{
	WSADATA data;
	WSAStartup(MAKEWORD(2,2), &data);
	return 0;
}

ELF_FUNCTION(l_sockets_new_udp_server)
{
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	struct sockaddr_in addr = {};
	addr.sin_family         = AF_INET;
	addr.sin_port           = htons(27015);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

	bind(sock, (struct sockaddr *) &addr, sizeof(addr));

	u_long mode = 1;
	ioctlsocket(sock, FIONBIO, &mode);

	elf_pushsys(S, sock);
	return 1;
}

ELF_FUNCTION(l_sockets_new_udp_client)
{
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	struct sockaddr_in addr = {};
	addr.sin_family         = AF_INET;
	addr.sin_port           = htons(27015);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

	connect(sock, (struct sockaddr *) &addr, sizeof(addr));

	elf_pushsys(S, sock);
	return 1;
}

ELF_FUNCTION(l_sockets_receive)
{
	SOCKET sock = elf_loadsys(S, 1);

	char buffer[512];

	struct sockaddr_in from;
	i32 from_len = sizeof(from);

	i32 bytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *) &from, &from_len);

	if (bytes > 0)
	{
		AtomPacket *packet = (AtomPacket *) buffer;
		elf_push_atom_text_size(S, packet->data, packet->size);
	}
	else
	{
		elf_push_nil(S);
	}

	return 1;
}

ELF_FUNCTION(l_sockets_send)
{
	SOCKET sock = elf_loadsys(S, 1);

	Scratch scratch = get_scratch();
	elf_StrSlice text = elf_load_atom_copy(S, 2, scratch.arena);

	AtomPacket *packet;
	u32 packet_size = sizeof(*packet) + (u32)text.size;
	packet = malloc(packet_size);
	packet->size = (u32)text.size;
	memcpy(packet->data, text.data, text.size);

	send(sock, (char *) packet, packet_size, 0);

	free(packet);
	end_scratch(scratch);

	return 0;
}

static const elf_Binding l_sockets[] =
{
	{"init", l_sockets_init},
	{"new_udp_server", l_sockets_new_udp_server},
	{"new_udp_client", l_sockets_new_udp_client},
	{"receive", l_sockets_receive},
	{"send", l_sockets_send},
};

static elf_Table *elf_lib_sockets(elf_State *state)
{
	return new_binding_table(state, l_sockets, ARRAY_COUNT(l_sockets));
}














#if 0
int netlib_close(elf_State *R) {
	WSACleanup();
	return 0;
}


int netlib_listen(elf_State *R) {
	SOCKET handle = (SOCKET) f_checkhand(R,0);
	int error = listen(handle,SOMAXCONN);
	elf_pushint(R,error!=SOCKET_ERROR);
	return 1;
}


int netlib_accept(elf_State *R) {
	SOCKET handle = (SOCKET) f_checkhand(R,0);
	SOCKET client = accept(handle,NULL,NULL);
	elf_pushsys(R,(elf_Handle)client);
	return 1;
}


int netlib_pollclient(elf_State *R) {
	SOCKET handle = (SOCKET) f_checkhand(R,0);
	fd_set ready;
	FD_ZERO(&ready);
	FD_SET(handle,&ready);
	TIMEVAL timeout = {0};
	int result = select(0,&ready,NULL,NULL,&timeout);
   if (FD_ISSET(handle,&ready)) {
      SOCKET client = accept(handle,NULL,NULL);
      ASSERT(client != INVALID_SOCKET);
		elf_pushsys(R,(elf_Handle)client);
   } else elf_push_nil(R);
	return 1;
}


int netlib_tcpserver(elf_State *R) {
	elf_Value addrname_value = load_value(R, 0);
	elf_Value addrport_value = load_value(R, 1);
	check_value_type(R, addrname_value, ELF_VALUE_TYPE_ATOM);
	check_value_type(R, addrport_value, ELF_VALUE_TYPE_ATOM);
	elf_Atom *addrnameS = value_as_atom(addrname_value);
	elf_Atom *addrportS = value_as_atom(addrport_value);
	char *addrname = addrnameS ? addrnameS->data : 0;
	char *addrport = addrportS ? addrportS->data : 0;
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
		elf_pushsys(R,(elf_Handle)thesocket);
	} else elf_push_nil(R);

	return 1;
}


int netlib_tcpclient(elf_State *R) {
	elf_Value addrname_value = load_value(R, 0);
	elf_Value addrport_value = load_value(R, 1);
	check_value_type(R, addrname_value, ELF_VALUE_TYPE_ATOM);
	check_value_type(R, addrport_value, ELF_VALUE_TYPE_ATOM);
	elf_Atom *addrnameS = value_as_atom(addrname_value);
	elf_Atom *addrportS = value_as_atom(addrport_value);
	char *addrname = addrnameS ? addrnameS->data : 0;
	char *addrport = addrportS ? addrportS->data : 0;
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
		elf_pushsys(R,(elf_Handle)thesocket);
	} else elf_push_nil(R);
	return 1;
}


int netlib_send(elf_State *R) {
	/* todo: make this a class? */
	SOCKET socket = (SOCKET) f_checkhand(R,0);
	elf_Value payload_value = load_value(R, 1);
	check_value_type(R, payload_value, ELF_VALUE_TYPE_ATOM);
	elf_Atom *payload = value_as_atom(payload_value);
	LMSG message = { payload->size };
	elf_Integer sent = 0;
	sent += send(socket,(char*)&message,sizeof(message),0);
	sent += send(socket,payload->data,payload->size,0);
	elf_pushint(R,sent);
	return 1;
}


int netlib_ioctl(elf_State *R) {
	SOCKET socket = (SOCKET) f_checkhand(R,0);
	long mode = 1;
	int error = ioctlsocket(socket,FIONBIO,&mode);
	elf_pushint(R,error == 0);
	return 1;
}


int netlib_recv(elf_State *R) {
	SOCKET socket = (SOCKET) f_checkhand(R,0);
	LMSG message = {0};
	if (recv(socket,(char*)&message,sizeof(message),0) != -1) {
		if (message.length != 0) {
			elf_Integer length = message.length;
			char *buffer = malloc(length + 1);
			char *cursor = buffer;
			do {
				elf_Integer result = recv(socket,cursor,length,0);
				if (result == SOCKET_ERROR) {
					int error = WSAGetLastError();
					if (error == WSAEWOULDBLOCK) {
						/* todo: retry until a certain timeout? */
					} else {
						char erbuf[0x100];
						sys_get_error_msg(error,erbuf,sizeof(erbuf));
						elf_lerror("netlib sys error '%i': %s",error,erbuf);
						break;
					}
				} else if (result == 0) {
					/* connection closed gracefully, simply break */
					break;
				} else {
					ASSERT(result > 0);
					length -= result;
					cursor += result;
				}
			} while (length != 0);
			*cursor = 0;
			push_value(R, value_from_atom(elf_atom_from_data_size(R, buffer, cursor - buffer)));
			free(buffer);
		} else elf_push_nil(R);
	} else elf_push_nil(R);
	return 1;
}
#endif


