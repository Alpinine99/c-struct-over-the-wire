#pragma once

typedef struct {
    uint8_t type;
    uint32_t length;
} header;

typedef struct {
    header p_header;
    char content[];
} packet;

