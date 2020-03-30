//
// Created by root on 3/22/20.
//

#ifndef INTELLIGENT_CURTAIN_PROTOCOL_PARSER_H
#define INTELLIGENT_CURTAIN_PROTOCOL_PARSER_H

#include <stdint.h>

#define PROTOCOL_BUFFER_LEN     23

typedef struct {
    uint8_t rx_data[PROTOCOL_BUFFER_LEN], tx_data[PROTOCOL_BUFFER_LEN];
    uint16_t rx_len, tx_len;
} protocol_data_block_t;

int protocol_parser(protocol_data_block_t *data);

#endif //INTELLIGENT_CURTAIN_PROTOCOL_PARSER_H
