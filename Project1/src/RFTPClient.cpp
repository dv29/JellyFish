#include "RFTPClient.h"

using namespace std;

RFTPClient::RFTPClient()
{
	server.sin_family = AF_INET;
	sock= socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) 
		cout<<"socket"<<endl;
}

RFTPClient::~RFTPClient()
{
	close(sock);
}

int RFTPClient::connectAndSend()
{
	hp = gethostbyname(HOSTNAME);
	if (hp==0) return -1;	
	
	memcpy((char *)&server.sin_addr, (char *)hp->h_addr, hp->h_length);
	server.sin_port = htons(PORT_NUMBER);
   	length=sizeof(struct sockaddr_in);

	void *buf = malloc(DATA_SIZE);
	memset(buf, 0, DATA_SIZE);
	

	void *received_packet = malloc(PACKET_SIZE);

	Packet p = Packet(CONNECTION_REQUEST, 0, 0, buf);	
	void * serialized_packet = p.serialize();
	int n=sendto(sock, serialized_packet, PACKET_SIZE, 0,(const struct sockaddr *)&server,length);
	delete(serialized_packet);
	if (n < 0)
		return 0;

	n = recvfrom(sock, received_packet,  PACKET_SIZE, 0, (struct sockaddr *)&from, &length);
	Packet packet = new Packet(buf);
	packet.printPacket();
	if (n < 0)
		return 0;
	return 1;

	delete(received_packet);
	delete(buf);
}

void RFTPClient::receivePacket(){

 	while(1){
 	
 		
 	}
 
 }

bool RFTPClient::requestFile(char *filename)
{
	//Copying filename on the data field of packet.
	void *vfilename = malloc(DATA_SIZE);
	int len = strlen(filename);
	memset(vfilename, 0, DATA_SIZE);
	memcpy(vfilename, filename, len);
	Packet pack = Packet(FILE_REQUEST, 0, len, vfilename);
	void * serialized_packet = pack.serialize();	//Freed

	void *received_packet;
	int x;
	int n=sendto(sock, serialized_packet, PACKET_SIZE, 0,(const struct sockaddr *)&server,length);
	delete(serialized_packet);
	//cout<<"Checkpoint 1\n";
	if (n < 0)
        return false;
	//cout<<"Checkpoint 2\n";

	received_packet = malloc(PACKET_SIZE);
    n = recvfrom(sock, received_packet,  PACKET_SIZE, 0, (struct sockaddr *)&from, &length);
	//cout<<"Checkpoint 3\n";
	if (n < 0)
		return false;
	//cout<<"Checkpoint 4\n";
    Packet packet = Packet(received_packet);
    packet.printPacket();
	
	memset(received_packet, 0, PACKET_SIZE);
	
	cout<<"Creating file.\n";
	int fdWrite = open("./testWrite.txt", O_CREAT | O_TRUNC| O_WRONLY, 0644);
	while (1) {
		 n = recvfrom(sock, received_packet,  PACKET_SIZE, 0, (struct sockaddr *)&from, &length);
		if (n<= 0) break;
		Packet pack = Packet(received_packet);
		x = write(fdWrite, pack.data, pack.sizeOfData);
		void *ptr3 = malloc(DATA_SIZE);
		memset(ptr3, 0, DATA_SIZE);
		Packet pack2 = Packet(DATA_ACK, 0, 0, ptr3);
		void *serialized_packet2 = pack2.serialize();
		sendto(sock, serialized_packet2, PACKET_SIZE, 0,(const struct sockaddr *)&server,length);
		delete(ptr3);
		delete(serialized_packet2);
	}
	delete(received_packet);
	delete(vfilename);
	return true;
}
