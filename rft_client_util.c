/*
 * Replace the following string of 0s with your student number
 * 
 */
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include "rft_client_logging.h"

/* 
 * is_corrupted - returns true with the given probability 
 * 
 * The result can be passed to the checksum function to "corrupt" a 
 * checksum with the given probability to simulate network errors in 
 * file transfer.
 *
 * DO NOT EDIT the is_corrupted function.
 */
static bool is_corrupted(float prob) {
    if (fpclassify(prob) == FP_ZERO)
        return false;
    
    float r = (float) rand();
    float max = (float) RAND_MAX;
    
    return (r / max) <= prob;
}

/* 
 * The verify_server function checks that the given server structure is 
 * equivalent to the server field of the proto struct. That is, the protocol
 * family, port and address of the two server structs are the same. 
 * It also checks that the given server_size is the same as the size of the
 * socket address size field of the proto struct.
 *
 * These checks can be used to ensure that an ACK is from the same server that 
 * a client sent data to.
 * 
 * If any of the checks fail, the function returns a corresponding error 
 * protocol state: PS_BAD_S_SIZE, PS_BAD_S_PORT, PS_BAD_S_FAM or PS_BAD_D_ADDR.
 *
 * If the servers are the same, the function returns PS_SERVER_OK state.
 *
 * DO NOT EDIT the verify_server function.
 */
static proto_state verify_server(protocol_t* proto, struct sockaddr_in* server, 
    socklen_t server_size) {
    if (server_size != proto->sockaddr_size)
        return PS_BAD_S_SIZE;
    
    if (server->sin_port != proto->server.sin_port)
        return PS_BAD_S_PORT;
        
    if (server->sin_family != proto->server.sin_family)
        return PS_BAD_S_FAM;
        
    if (server->sin_addr.s_addr != proto->server.sin_addr.s_addr)
        return PS_BAD_S_ADDR;
        
    return PS_SERVER_OK;
}

/* 
 * init_protocol - initialises fields of the given protocol struct to the 
 * values shown in the implementation of the function.
 *
 * DO NOT EDIT the init_protocol function.
 */
void init_protocol(protocol_t* proto) {
    memset(proto, 0, sizeof(protocol_t));
    proto->log = stdout;
    proto->state = PS_INIT;
    proto->in_file = NULL;
    proto->sockfd = -1;
    proto->seg_size = sizeof(segment_t);
    proto->sockaddr_size = (socklen_t) sizeof(struct sockaddr_in); 
    proto->timeout_sec = DEFAULT_TIMEOUT;
    proto->curr_retry = 0;
    proto->max_retries = DEFAULT_RETRIES;
    proto->close = close_protocol;
    
    init_segment(proto, DATA_SEG, false);
    init_segment(proto, ACK_SEG, false);
}

/* 
 * TODO: you must implement this function.
 *
 * See documentation in rft_client_util.h and the assignment specification
 */
void init_segment(protocol_t* proto, seg_type type, bool payload_only) {
	if(proto != NULL) {
		if(payload_only) {
			if(type == DATA_SEG) {
				int i = 0;
				while(i < PAYLOAD_SIZE) {
					proto->data.payload[i] = 0;
					i++;
				}
			}
			if(type == ACK_SEG) {
				int i = 0;
				while(i < PAYLOAD_SIZE) {
					proto->ack.payload[i] = 0;
					i++;
				}
			}
			return;
		}
		if(type == DATA_SEG) {
			memset(&proto->data, 0, sizeof(proto->data));
			proto->data.type = type;
		}
		if(type == ACK_SEG) {
			memset(&proto->ack, 0, sizeof(proto->ack));
			proto->ack.type = type;		
		}
		return;
	}
	exit(EXIT_FAILURE);
}

/* 
 * TODO: you must implement this function.
 *
 * See documentation in rft_client_util.h and the assignment specification
 *
 * Hint:
 *  - another function can be used to ensure that the data segment's payload
 *      buffer is zeroed
 *  - you have to detect an error when reading from the proto's input file
 *  - you have to use the library function for reading from a binary stream. 
 *      You cannot use fgets because it stops at a line feed and you must read 
 *      a fixed number of bytes including any line feed character.
 */
proto_state read_data(protocol_t* proto) {
	if(proto == NULL) {
		exit(EXIT_FAILURE);
	}

	size_t exp_file_data = proto->tfr_bytes < PAYLOAD_SIZE ? proto->tfr_bytes : PAYLOAD_SIZE - 1;
	proto->data.file_data = exp_file_data;
    proto->tfr_bytes = proto->tfr_bytes - exp_file_data;
	
	if(exp_file_data < PAYLOAD_SIZE) {
		int i = 0;
		while(i < PAYLOAD_SIZE) {
			proto->data.payload[i] = 0;
			i++;
		}
	}
	
	size_t read_successful = fread(proto->data.payload, sizeof(char), PAYLOAD_SIZE-1, proto->in_file);
	if(read_successful && read_successful > 0) {
		return PS_DATA_READ;
	}
	
	return PS_BAD_READ;
}

/* 
 * TODO: you must implement this function.
 *
 * See documentation in rft_client_util.h and the assignment specification
 * and see how to send data in the preliminary exercise.
 *
 * Hints:
 *  - you have to handle the case when the current retry count exceeds the
 *      maximum number of retries allowed
 *  - you have to introduce a probability of corruption of the payload checksum
 *      if the tfr_mode is with timeout (wt)
 *  - you have to detect an error from sending data - see how the rft_server
 *      detects an error when sending an ACK
 */
proto_state send_data(protocol_t* proto) {
    if(proto == NULL) {
		exit(EXIT_FAILURE);
	}
	
	if(proto->curr_retry < proto->max_retries) {
		bool corruption = false;
		if(strcmp(proto->tfr_mode, TIMEOUT_TFR_MODE) == 0) {
			corruption = is_corrupted(proto->loss_prob);
		}
		
		proto->data.checksum = checksum(proto->data.payload, corruption);
		
		if(sendto(proto->sockfd, &proto->data, proto->seg_size, 0, (struct sockaddr*) &proto->server, sizeof(struct sockaddr_in)) == proto->seg_size) {
			return PS_DATA_SENT;
		}
		return PS_BAD_SEND;
	}
	return PS_EXCEED_RETRY;
}

/* 
 * send_file_normal - sends a file to a server according to the RFT protocol 
 * with positive acknowledgement, without retransmission.
 *
 * DO NOT EDIT the send_file_normal function.
 */
proto_state send_file_normal(protocol_t* proto) { 
    if (!proto->fsize)
        return PS_EMPTY_FILE;

    proto->state = PS_START_SEND;
    log_protocol(__FILE__, __LINE__, proto);
    
    while (proto->tfr_bytes) {
        proto->state = read_data(proto);
        
        if (proto->state != PS_DATA_READ) 
            exit_err(__FILE__, __LINE__, proto);
        
        proto->state = PS_SEND_DATA;
        log_protocol(__FILE__, __LINE__, proto);
        
        proto->state = send_data(proto);
        
        if (proto->state != PS_DATA_SENT)
            exit_err(__FILE__, __LINE__, proto);
        
        proto->total_segments++;
        proto->total_file_data += proto->data.file_data;
        
        proto->state = PS_ACK_WAIT;
        log_protocol(__FILE__, __LINE__, proto);
 
        init_segment(proto, ACK_SEG, false);        
        socklen_t server_size = proto->sockaddr_size;
        struct sockaddr_in server;
        memset(&server, 0, server_size);
        ssize_t nbytes = recvfrom(proto->sockfd, &proto->ack, proto->seg_size,
            0, (struct sockaddr *) &server, &server_size);

        if (nbytes != proto->seg_size) {
            proto->state = PS_BAD_ACK;
            exit_err(__FILE__, __LINE__, proto);
        }
        
        if (proto->data.sq != proto->ack.sq) {
            proto->state = PS_BAD_ACK_SQ;
            exit_err(__FILE__, __LINE__, proto);
        }

        proto->state = verify_server(proto, &server, server_size);
        if (proto->state != PS_SERVER_OK)
            exit_err(__FILE__, __LINE__, proto);

        proto->state = PS_ACK_RECV;
        log_protocol(__FILE__, __LINE__, proto);

        proto->data.sq++;
    }

    return proto->total_file_data == proto->fsize 
            ? PS_TFR_COMPLETE
            : PS_TFR_MISMATCH;
}      

/* 
 * TODO: you must implement this function.
 *
 * See documentation in rft_client_util.h and the assignment specification
 * and adapt send_file_normal
 */
proto_state send_file_with_timeout(protocol_t* proto) {
    if (!proto->fsize)
        return PS_EMPTY_FILE;

	proto->state = set_socket_timeout(proto);
	
	if(proto->state == PS_SOCKTOUT_SET) {
		log_protocol(__FILE__, __LINE__, proto);

		proto->state = PS_START_SEND;
		log_protocol(__FILE__, __LINE__, proto);
		
		while (proto->tfr_bytes) {
			proto->state = read_data(proto);
			if (proto->state != PS_DATA_READ) 
				exit_err(__FILE__, __LINE__, proto);
			
			do {
				if(proto->state == PS_BAD_SEND || proto->state == PS_RESEND_DATA || proto->state == PS_BAD_ACK_SQ) {
					proto->curr_retry++;
				}
				
				proto->state = PS_SEND_DATA;
				log_protocol(__FILE__, __LINE__, proto);
				proto->state = send_data(proto);
			
				if(proto->state == PS_EXCEED_RETRY) {
					exit_err(__FILE__, __LINE__, proto);
				}
			
				if (proto->state != PS_DATA_SENT) {
					log_protocol(__FILE__, __LINE__, proto);
					continue;
				}
				
				proto->state = PS_ACK_WAIT;
				log_protocol(__FILE__, __LINE__, proto);
				init_segment(proto, ACK_SEG, false);        
				socklen_t server_size = proto->sockaddr_size;
				struct sockaddr_in server;
				memset(&server, 0, server_size);
				ssize_t nbytes = recvfrom(proto->sockfd, &proto->ack, proto->seg_size, 0, (struct sockaddr *) &server, &server_size);

				if (nbytes != proto->seg_size) {
					proto->state = PS_NO_ACK;
					log_protocol(__FILE__, __LINE__, proto);
					proto->state = PS_RESEND_DATA;
					continue;
				}
			
				if (proto->data.sq != proto->ack.sq) {
					proto->state = PS_BAD_ACK_SQ;
					exit_err(__FILE__, __LINE__, proto);
					continue;
				}

				proto->state = verify_server(proto, &server, server_size);
				if (proto->state != PS_SERVER_OK) {
					exit_err(__FILE__, __LINE__, proto);
					proto->curr_retry++;
					continue;
				}
				
				proto->total_segments++;
				proto->total_file_data += proto->data.file_data;
				proto->state = PS_ACK_RECV;
				log_protocol(__FILE__, __LINE__, proto);
				proto->data.sq++;
			} while(proto->state != PS_ACK_RECV);
		}
	}

	if(proto->state == PS_BAD_META) {
		exit_err(__FILE__, __LINE__, proto);
	}

    return proto->total_file_data == proto->fsize 
            ? PS_TFR_COMPLETE
            : PS_TFR_MISMATCH;
}

/* 
 * TODO: you must implement this function.
 *
 * See documentation in rft_client_util.h and the assignment specification
 *
 * Hint:
 *  - you must copy proto information to a metadata struct
 *  - you have to detect an error from sending metadata - see how the rft_server
 *      detects an error when sending an ACK
 */
proto_state send_metadata(protocol_t* proto) {     
	if(proto == NULL) {
		exit(EXIT_FAILURE);
	}
	metadata_t md_file;
	memset(&md_file, 0, sizeof(metadata_t));
	strncpy(md_file.name, proto->out_fname, MAX_FILENAME_SIZE);
	md_file.size = proto->fsize;
	int meta_size = sizeof(md_file);
	if(sendto(proto->sockfd, &md_file, meta_size, 0, (struct sockaddr*) &proto->server, proto->sockaddr_size) == meta_size) {
		return PS_META_SENT;
	}
	return PS_BAD_META;
} 
  
/* 
 * TODO: you must implement this function.
 *
 * See documentation in rft_client_util.h and the assignment specification
 */
proto_state set_socket_timeout(protocol_t* proto) {
    if(proto == NULL) {
		exit(EXIT_FAILURE);
	}
	
	struct timeval s_timeout;
	s_timeout.tv_sec = proto->timeout_sec;
    s_timeout.tv_usec = 0;
	socklen_t to_size = sizeof(struct timeval);
	
	if((setsockopt(proto->sockfd, SOL_SOCKET, SO_RCVTIMEO, &s_timeout, to_size)) == 0 && (setsockopt(proto->sockfd, SOL_SOCKET, SO_SNDTIMEO, &s_timeout, to_size)) == 0) {
		return PS_SOCKTOUT_SET;
	}
	if((setsockopt(proto->sockfd, SOL_SOCKET, SO_RCVTIMEO, &s_timeout, to_size)) != 0 || (setsockopt(proto->sockfd, SOL_SOCKET, SO_SNDTIMEO, &s_timeout, to_size)) != 0) {
		return PS_BAD_SOCKTOUT;
	}
	
	return PS_BAD_META;
}

/* 
 * TODO: you must implement this function.
 *
 * See documentation in rft_client_util.h and the assignment specification,
 * and look at rft_server and preliminary exercise code.
 *
 * Hint:
 *  - Make sure you check the return values of system calls such as 
 *      socket and inet_aton
 */
proto_state set_udp_socket(protocol_t* proto) {  
	if(proto == NULL) {
		exit(EXIT_FAILURE);
	}
	
	if((proto->sockfd = socket(AF_INET, SOCK_DGRAM, 0)) != -1) {
		proto->server.sin_family = AF_INET;
		proto->server.sin_port = htons(proto->server_port);
		if (inet_aton(proto->server_addr, &proto->server.sin_addr) == 0) {
			return PS_BAD_SERVER;
		}
		return PS_TFR_READY;
	}
	proto->sockfd = -1;
	return PS_BAD_SOCKET;
} 

/* 
 * TODO: you must implement this function.
 *
 * See documentation in rft_client_util.h and the assignment specification.
 *
 * Hint:
 *  - Do not close the protocol log if it is stdout or stderr
 *  - Do not close NULL or unopened resources
 */
void close_protocol(protocol_t* proto) {
	if(proto != NULL) {
		if(proto->in_file != NULL) {
			fclose(proto->in_file);
		}
		if(proto->log != NULL && proto->log != stdout && proto->log != stderr) {
			fclose(proto->log);
		}
		if(proto->sockfd != -1) {
			proto->sockfd = -1;
		}
		return;
	}
	exit(EXIT_FAILURE);
}
