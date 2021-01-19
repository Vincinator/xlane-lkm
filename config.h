#pragma once




#define ASGARD_DPDK 0



#ifdef ASGARD_KERNEL
    #define HBI_OF_1MS 1000000
    #define HBI_OF_10MS 10000000
    #define HBI_OF_100MS 100000000
    #define HBI_OF_1S 1000000000
    #define CYCLES_PER_1MS 2400000
    #define CYCLES_PER_5MS 12000000
    #define CYCLES_PER_10MS 24000000
    #define CYCLES_PER_100MS 240000000
#endif