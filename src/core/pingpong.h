#pragma once

 typedef enum pingpong_state {

    UNINIT = 0,
    READY = 1,
    RUNNING = 2,
} pingpong_state_t;


struct pingpong_priv {

    pingpong_state_t state;

};