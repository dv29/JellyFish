
#include "RFTPServer.h"
using namespace std;

/**
RFTPServer constructor used to create a socket and initialize
 all the values used by the server
*/
RFTPServer::RFTPServer(){
	sockS = socket(AF_INET, SOCK_DGRAM, 0);
   	sockR  = socket(AF_INET, SOCK_DGRAM, 0);

	if (sockS < 0 || sockR < 0)
		cout<<"Error Opening Socket"<<endl;
   	length = sizeof(serverS);
   	memset(&serverS, 0, length);
	
	lengthAck = sizeof(serverR);
    memset(&serverR, 0, lengthAck);
	
	serverS.sin_family=AF_INET;
   	serverS.sin_addr.s_addr=INADDR_ANY;
   	serverS.sin_port=htons(PORT_NUMBER_DATA);

	serverR.sin_family=AF_INET;
    serverR.sin_addr.s_addr=INADDR_ANY;
    serverR.sin_port=htons(PORT_NUMBER_ACK);

	read_timeout.tv_sec = 0;
	read_timeout.tv_usec = 100000;

	reset_timeout.tv_sec = 0;
	reset_timeout.tv_usec = 0;

	isConnected = false;
}

/**
Bind() is used to bind the socket with the socket struct i.e. Port number and IP address
*/
void RFTPServer :: Bind(){
   if ( (bind(sockS,(struct sockaddr *)&serverS,length)<0) || (bind(sockR,(struct sockaddr *)&serverR,lengthAck)<0))
       cout<<"Binding Error"<<endl;
   fromlen = sizeof(struct sockaddr_in);
   //fromlen_ack = sizeof(struct sockaddr_in);   
   cout<<"Bind complete\n";
}

/**
ListenAccept() is used to listen to client and accept connection request
*/
void RFTPServer::ListenAccept(){
	cout<<"Connection Request Received\n";
    send_packet(sockS, CONNECTION_ACK, 1);
	cout<<"Connection Acknowledgement Sent\n";
	this->isConnected = true;
}

/**
fileReq() is used for sending the requested file to the client and
handling error cases like retransmission or packet loss during the
file transfer.
@param vfilename is the file we transfer
@param size_of_data is the size of data theat we transfer
*/
bool RFTPServer::fileReq(void *vfilename, int size_of_data)
{
	cout<<"In file req.\nSize of data is: "<<size_of_data;
	if (!this->isConnected)
		return false;
	
	char absfilename[6+1+size_of_data];
	char filename[size_of_data];
	memcpy(filename, vfilename, size_of_data);		
	cout<<"Filename requested: "<<filename<<endl;
	strcpy(absfilename, server_fs);
	strcat(absfilename, "/");
	strcat(absfilename, filename);
	cout<<absfilename<<endl;

	if( access(absfilename, F_OK ) == -1 ) {
		return false;
	}	

	int fdRead = open(absfilename, 'r');
	cout<<"fdRead is: "<<fdRead<<endl;
	
	//Create a data packet
    send_packet(sockS, FILE_REQUEST_ACK, 3);
	cout<<"File req ack sent\n";
	
	void *data = malloc(DATA_SIZE);
	memset(data, 0, DATA_SIZE);

	void *tempptr = malloc(PACKET_SIZE);
	recvfrom(sockR, tempptr, PACKET_SIZE, 0, (struct sockaddr *)&clientR, &fromlen);
	Packet p = Packet(tempptr);
	if (p.kind != START_DATA_TRANSFER)
		return false;	
	
	int bytesRead = 0;
	int datasn = 4;

	setsockopt(sockS, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
	int total_transmissions = 0;
	while(1) {
		if((bytesRead = read(fdRead, data, DATA_SIZE)) <= 0) break;
		void *ptr = malloc(PACKET_SIZE);	
		Packet *temp;
		bool cond = true;
		do {
			send_packet(DATA, datasn, bytesRead, data);
			total_transmissions++;
			memset(ptr, 0, PACKET_SIZE);
	
			int n = recvfrom(sockR, ptr,PACKET_SIZE,0,(struct sockaddr *)&clientS, &fromlen);
			if (n < 0) continue;
				cout<<"Received Packet.\n";

			temp = new Packet(ptr);
			cond = temp->kind != DATA_ACK;
			delete(temp);
		} while(cond);
		datasn++;
		delete(ptr);
	}

	cout<<"Number of re-transmissions: "<<(total_transmissions-(datasn-4))<<endl;
	cout<<"Sending close connection signal.\n";

    send_packet(sockS, CLOSE_CONNECTION, 0);
	delete(data);
	setsockopt(sockS, SOL_SOCKET, SO_RCVTIMEO, &reset_timeout, sizeof reset_timeout);
	return true;
}

/**
receivePacket() is used to receive packets like CONNECTION_REQUEST,
 FILE_REQUEST and call the appropriate functions to handle them
*/
void RFTPServer::receivePacket(){
	void *buf = malloc(PACKET_SIZE);  //To read a packet clientS socket.
	int n; //Number of bytes read.
    while(1) {
		if (n = recvfrom(sockR, buf, PACKET_SIZE,0,(struct sockaddr *)&clientS,&fromlen) < 0)
		cout<<"Nothing read clientS socket"<<endl;
        else
        {
         	Packet packet = Packet(buf);
			packet.printPacket();
			switch(packet.kind){
			case CONNECTION_REQUEST:
				ListenAccept();
				break;
			case FILE_REQUEST:
				if (fileReq(packet.data, packet.sizeOfData))
					cout<<"File Request function done.\n";
				else
                    send_packet(sockS, FILE_REQ_ERROR, -1);
				break;	
			default:
				cout<<"Could not recognize the packet kind and thus ignoring the packet."<<endl; 
			}
        }
	}
}

/**
send_packet() is used to send packet to the server without data.
Type of packet sent here is mostly an acknowledgement packet
@PacketKind is the type of packet being send
@seq_no is the sequence number of the packet
*/
void RFTPServer::send_packet(int socket, PacketKind pk, int seq_no) {
	void *data = malloc(DATA_SIZE);
	memset(data, 0, DATA_SIZE);
	Packet packet = Packet(pk, seq_no, 0, data);
	void *ptr = packet.serialize();
	if(pk == CLOSE_CONNECTION)
		sendto(socket, ptr, PACKET_SIZE,0,(struct sockaddr *)&clientR,fromlen);
	sendto(socket, ptr, PACKET_SIZE,0,(struct sockaddr *)&clientS,fromlen);
	delete(data);	
	delete(ptr);	
}

/**
send_packet() is used to send packet to the client with data.
Type of packet sent here is mostly a data packet
@param PacketKind is the type of packet being send
@param seq_no is the sequence number of the packet
@param data is the data being sent by the server
*/
void RFTPServer::send_packet(PacketKind pk, int seq_no, int size, void *data) {
	Packet packet = Packet(pk, seq_no, size, data);
	void *ptr = packet.serialize();
	sendto(sockS, ptr, PACKET_SIZE,0,(struct sockaddr *)&clientR,fromlen);
	delete(ptr);	
}