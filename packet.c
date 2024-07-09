#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/if_xdp.h>
#include <net/ethernet.h>
#include <linux/if_link.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/filter.h>
#include <linux/bpf.h>


#include <pthread.h>
#include <unistd.h>
#include <string.h>


#include <net/if.h>

#include <netinet/in.h>
#include <netinet/ip.h>

#include <netpacket/packet.h>


struct timespec res;
struct timespec before;
struct timespec after;

/* ALLOCATE BUFFERS */
#define XDP_PACKET_BUFFER_SIZE (1 << 20) // 1 MB
char packet_buffer[XDP_PACKET_BUFFER_SIZE];
char * RxBuff = &packet_buffer[0];
char * TxBuff = &packet_buffer[1 << 19];


#define SYNC_ETHTYPE 42111
#define DATA_ETHTYPE 42112

#define RX_BUFFER_SIZE 1000000

#define print_eth_addr(addr) printf("%x:%x:%x:%x:%x:%x", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5])

char device_address[6] = { 0x00, 0x0a, 0x35, 0x01, 0x02, 0x03 };
char host_address[6] = { 0x00, 0x0a, 0x35, 0x01, 0x02, 0x04 };

struct sock_filter data_filter[] = {
	{0x28, 0, 0, 0x0000000c},
	{0x15, 0, 1, 0x0000a480},
	{0x06, 0, 0, 0x000005ee},
	{0x06, 0, 0, 0x00000000} 
};

struct ethernet_frame {
    char dest_addr[6];
    char src_addr[6];
    char eth_type[2];
    char payload[1500];
};

struct data_frame {
    char dest_addr[6];
    char src_addr[6];
    char eth_type[2];
    short order;
    char payload[1024];
};

struct sync_frame {
    char dest_addr[6];
    char src_addr[6];
    char eth_type[2];
    short state;
    int received_frames[32];
};

void print_frame(struct ethernet_frame* frame);

unsigned short endianSwap(unsigned short num) {
    unsigned short res = 0;
    res += num >> 8;
    res += num << 8;
    return res;
}

char data[8][1024];


void fill_data_frame(struct data_frame * frame, int number, char * content) {
	for (int i = 0; i < 6; i++) {
		frame->src_addr[i] = host_address[i];
		frame->dest_addr[i] = device_address[i];
	}
	short * type = (short *) &(frame->eth_type);
    *type = endianSwap(DATA_ETHTYPE);
	frame->order = number;
	for (int i = 1; i < 1024; i++) {
		frame->payload[i] = content[i];
	}
}

void fill_sync_frame(struct sync_frame * frame, int number) {
	for (int i = 0; i < 6; i++) {
		frame->src_addr[i] = host_address[i];
		frame->dest_addr[i] = device_address[i];
	}
	unsigned short * type = (unsigned short *) &(frame->eth_type);
    *type = endianSwap(SYNC_ETHTYPE);
	frame->state = number;
	for (int i = 1; i < 32; i++) {
		frame->received_frames[i] = i;
	}
    frame->received_frames[31] = 0;
}
/* GLOBAL VARS */
int sock_fd;
struct sockaddr_ll sll;
int status;
socklen_t sll_len;
int frames_to_receive = 0;
void * receive_thread(void * data);
struct data_frame * received[8];

int main() {

    char * TxPtr = TxBuff;
    sock_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock_fd == -1) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    } else {
        printf("sock_fd = %d\n", sock_fd);
    }
    

    
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = if_nametoindex("enp8s0");
    sll_len = sizeof(sll);


    /* Bind the socket to the address */
    status = bind(sock_fd, (struct sockaddr*) &sll, sll_len);
    
    /*if (status == -1) {
        perror("bind() failed");
        exit(EXIT_FAILURE);
    }*/

    struct sock_fprog bpf = {
        .len = sizeof(data_filter)/sizeof(data_filter[0]),
        .filter = data_filter
    };

    status = setsockopt(sock_fd, SOL_SOCKET, SO_ATTACH_FILTER, &bpf, sizeof(bpf));
    if (status == -1) {
        perror("setsockopt() failed");
        exit(EXIT_FAILURE);
    }
    char temp = 0;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 1024; j++) {
            data[i][j] = temp % 256;
            temp++;
        }
    }
    pthread_t alt_thread;
    pthread_create(&alt_thread, NULL, receive_thread, (void *) NULL);
    /* cast buffer to frames */
    struct data_frame * ptr_frames = (struct data_frame *) TxBuff;
    frames_to_receive = 8;

    clock_gettime(CLOCK_MONOTONIC, &before);
    for (int i = 0; i < 8; i++) {
        fill_data_frame(&ptr_frames[i], i, &data[i][0]);
        sendto(sock_fd, &ptr_frames[i], sizeof(struct data_frame), 0, (struct sockaddr*) &sll, sll_len);
    }
    clock_gettime(CLOCK_MONOTONIC, &after);
    printf("1 time = %ld - %ld = %ldns\n", after.tv_sec * 1000000000 + after.tv_nsec, before.tv_sec * 1000000000 + before.tv_nsec,after.tv_sec * 1000000000 + after.tv_nsec - before.tv_sec * 1000000000 - before.tv_nsec );
    TxPtr = (char *) &ptr_frames[8];

    clock_gettime(CLOCK_MONOTONIC, &after);
    printf("2 time = %ld - %ld = %ldns\n", after.tv_sec * 1000000000 + after.tv_nsec, before.tv_sec * 1000000000 + before.tv_nsec,after.tv_sec * 1000000000 + after.tv_nsec - before.tv_sec * 1000000000 - before.tv_nsec );
    while(frames_to_receive);
    clock_gettime(CLOCK_MONOTONIC, &after);
    printf("3 time = %ld - %ld = %ldns\n", after.tv_sec * 1000000000 + after.tv_nsec, before.tv_sec * 1000000000 + before.tv_nsec,after.tv_sec * 1000000000 + after.tv_nsec - before.tv_sec * 1000000000 - before.tv_nsec );
    ptr_frames = (struct data_frame *) RxBuff;
    for (int i = 0; i < 8; i++) {
        printf("frame %d order = %d\n",i,received[i]->order);
    }
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 1024; j++) {
            data[i][j] = received[i]->payload[j];
        }
    }
    struct sync_frame * sync_ptr = (struct sync_frame *) TxPtr;
    fill_sync_frame(sync_ptr, 0);
    sendto(sock_fd, sync_ptr, sizeof(struct sync_frame), 0, (struct sockaddr*) &sll, sll_len);

    clock_gettime(CLOCK_MONOTONIC, &after);
    printf("4 time = %ld - %ld = %ldns\n", after.tv_sec * 1000000000 + after.tv_nsec, before.tv_sec * 1000000000 + before.tv_nsec,after.tv_sec * 1000000000 + after.tv_nsec - before.tv_sec * 1000000000 - before.tv_nsec );
    printf("DONE!\n");
    
    return 0;
}

void print_frame(struct ethernet_frame* frame) {
    printf("Frame from ");
    print_eth_addr(frame->src_addr);
    printf(" to ");
    print_eth_addr(frame->dest_addr);
    printf("\n with type = %u\nContent: %s\n", endianSwap(*((unsigned short*) &frame->eth_type)), &frame->payload[0]);
}

void * receive_thread(void * data) {
    struct ethernet_frame * frame_ptr = (struct ethernet_frame * ) RxBuff;
    struct  data_frame * data_ptr;
    while (1) {
        int len = recvfrom(sock_fd, &frame_ptr[frames_to_receive - 1], sizeof(struct ethernet_frame), 0, (struct sockaddr*) &sll, &sll_len);
        if (len == -1) {
            perror("RECV ERROR");
        } else {
            data_ptr = (struct  data_frame *) &frame_ptr[frames_to_receive - 1];
            printf("receive order = %d\n", data_ptr->order);
            received[data_ptr->order] = data_ptr;
            frames_to_receive--;
        }
    }
}
    /*int xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST | XDP_FLAGS_SKB_MODE;
    if (setsockopt(sock_fd, SOL_XDP, XDP_RX_RING, &xdp_flags, sizeof(xdp_flags)) == -1) {
        perror("setsockopt() 1 failed");
        exit(EXIT_FAILURE);
    }*/


    /* ARCHIVED TEST CODE
    int total_length = 0;
    struct ethernet_frame * frame;
    frame = (struct ethernet_frame * ) RxBuff;

    struct ethernet_frame * send_frame = (struct ethernet_frame * ) TxBuff;
    
    int len = recvfrom(sock_fd, RxBuff, RX_BUFFER_SIZE, 0, (struct sockaddr*) &sll, &sll_len);
    //int len = recv(sock_fd, RxBuff, 1200, 0);
    int len = 0;
    if (len > -1) {
        //total_length += len;
        print_frame((struct ethernet_frame * ) RxBuff);
        fill_sync_frame((struct sync_frame * ) TxBuff, 55);
        print_frame((struct ethernet_frame * ) TxBuff);
        if(-1 == sendto(sock_fd, TxBuff, sizeof(struct sync_frame), 0, (struct sockaddr*) &sll, sll_len)) {
            perror("SEND ERROR");
        }
    } else {
        perror("RECEIVE ERROR");
    }*/