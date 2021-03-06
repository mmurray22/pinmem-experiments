/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */
#include "mem.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <assert.h>
#include <limits.h>
#include <ctype.h>
#include <getopt.h>
#include <sys/mman.h>
#include "fcntl.h"
#include <sys/types.h>
#include <unistd.h>
#include <numaif.h>
#include <math.h> 
/*#include <fstream>
#include <iostream>
#include <vector>*/

#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_ip.h>
#include <rte_lcore.h>
#include <rte_memcpy.h>
#include <rte_errno.h>
#include <rte_udp.h>
#include <rte_ethdev.h>
#include <rte_arp.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <mlx5_custom.h>

#define NNUMA 2
/******************************************/
/******************************************/
typedef unsigned long physaddr_t;
typedef unsigned long virtaddr_t;
/******************************************/
/******************************************/
#define MAX_ITERATIONS 100000000
typedef struct Latency_Dist_t
{
    uint64_t min, max;
    uint64_t latency_sum;
    uint64_t total_count;
    float moving_avg;
    uint64_t latencies[MAX_ITERATIONS];
} Latency_Dist_t;

struct tx_pktmbuf_priv
{
    int32_t lkey;
    int32_t field2; // needs to be atleast 8 bytes large
};

static void add_latency(Latency_Dist_t *dist, uint64_t latency) {
    dist->latencies[dist->total_count] = latency;
    dist->total_count++;
    dist->latency_sum += latency;
    if (latency < dist->min) {
        dist->min = latency;
    }

    if (latency > dist->max) {
        dist->max = latency;
    }

    // calculate moving avg
    dist->moving_avg = dist->moving_avg * ((float)(dist->total_count - 1)/(float)dist->total_count) + ((float)(latency) / (float)(dist->total_count));
}
int cmpfunc(const void * a, const void *b) {
    const uint64_t *a_ptr = (const uint64_t *)a;
    const uint64_t *b_ptr = (const uint64_t *)b;
    return (int)(*a_ptr - *b_ptr);
}

static void dump_latencies(Latency_Dist_t *dist) {
    // sort the latencies
    size_t amt_to_remove = (size_t)((double)dist->total_count * 0.05);

    uint64_t *arr = malloc(dist->total_count * sizeof(uint64_t));
    uint64_t *arr2 = malloc((dist->total_count - amt_to_remove) * sizeof(uint64_t));
    if (arr == NULL) {
        printf("Not able to allocate array to sort latencies\n");
        exit(1);
    }
    if (arr2 == NULL) {
        printf("Not able to allocate array to sort latencies\n");
        exit(1);
    }
    uint64_t min2 = LONG_MAX;
    uint64_t max2 = 0;
    uint64_t total2 = 0;
    for (size_t i = 0; i < dist->total_count; i++) {
        arr[i] = dist->latencies[i];
        if (i >= amt_to_remove) {
            if (dist->latencies[i] > max2) {
                max2 = dist->latencies[i];
            }
            if (dist->latencies[i] < min2) {
                min2 = dist->latencies[i];
            }
            arr2[i - amt_to_remove] = dist->latencies[i];
            total2 += dist->latencies[i];
        }
        
    }

    qsort(arr, dist->total_count, sizeof(uint64_t), cmpfunc);
    qsort(arr2, dist->total_count - amt_to_remove, sizeof(uint64_t), cmpfunc);
    uint64_t avg_latency = (dist->latency_sum) / (dist->total_count);
    printf("Avg latency\n");
    uint64_t median = arr[(size_t)((double)dist->total_count * 0.50)];
    printf("Median\n");
    uint64_t p99 = arr[(size_t)((double)dist->total_count * 0.99)];
    printf("P99\n");
    uint64_t p999 = arr[(size_t)((double)dist->total_count * 0.999)];
    printf("P999\n");
    uint64_t average2 = total2 / (dist->total_count - amt_to_remove);
    uint64_t median2 = arr2[(size_t)((double)(dist->total_count - amt_to_remove) * 0.50)];
    uint64_t p992 = arr2[(size_t)((double)(dist->total_count - amt_to_remove) * 0.99)];
    uint64_t p9992 = arr2[(size_t)((double)(dist->total_count - amt_to_remove) * 0.999)];
    
    printf("Stats:\n\t- Min latency: %u ns\n\t- Max latency: %u ns\n\t- Avg latency: %" PRIu64 " ns", (unsigned)dist->min, (unsigned)dist->max, avg_latency);
    printf("\n\t- Median latency: %u ns\n\t- p99 latency: %u ns\n\t- p999 latency: %u ns\n", (unsigned)median, (unsigned)p99, (unsigned)p999);
    printf("------------------------\n");
    printf("Stats with %u removed:\n\t- Min latency: %u ns\n\t- Max latency: %u ns\n", (unsigned)amt_to_remove, (unsigned)min2, (unsigned)max2);
    printf("\n\t- Median latency: %u ns\n\t- p99 latency: %u ns\n\t- p999 latency: %u ns\n\t- Avg latency: %" PRIu64 "ns\n", (unsigned)median2, (unsigned)p992, (unsigned)p9992, (unsigned)average2);
    /*FILE *fp = fopen ("tmp.log", "w");
    for (int i = 0; i < dist->total_count; i++) {
        fprintf(fp, "%u\n", dist->latencies[i]);
    }
    fclose(fp);*/
    free((void *)arr);
    //free((void *)arr2);

}
typedef void (*netperf_onfail_t)(int error_arg,
      const char *expr_arg, const char *funcn_arg, const char *filen_arg,
      int lineno_arg);

static void default_onfail(int error_arg,
      const char *expr_arg, const char *fnn_arg, const char *filen_arg,
      int lineno_arg);

static netperf_onfail_t current_onfail = &default_onfail;

static void netperf_panic(const char *why_arg, const char *filen_arg, int lineno_arg) {
    if (NULL == why_arg) {
        why_arg = "*unspecified*";
    }

    if (NULL == filen_arg) {
        filen_arg = "*unspecified*";
    }

    /* there's really no point in checking the return code of fprintf().
     * if it fails, i don't have a backup plan for informing the
     * operator. */
    fprintf(stderr, "*** panic in line %d of `%s`: %s\n", lineno_arg, filen_arg, why_arg);
    abort();
}
#define NETPERF_PANIC(Why) netperf_panic((Why), __FILE__, __LINE__)

static void netperf_fail(int error_arg, const char *expr_arg,
      const char *fnn_arg, const char *filen_arg, int lineno_arg) {
   current_onfail(error_arg, expr_arg, fnn_arg, filen_arg,
         lineno_arg);
}

static void default_onfail(int error_arg, const char *expr_arg,
   const char *fnn_arg, const char *filen_arg, int lineno_arg) {
    int n = -1;

    if (0 == error_arg) {
        NETPERF_PANIC("attempt to fail with a success code.");
    }

    /* to my knowledge, Windows doesn't support providing the function name,
     * so i need to tolerate a NULL value for fnn_arg. */
    const char *err_msg = NULL;
    if (error_arg > 0) {
        err_msg = strerror(error_arg);
    } else {
        err_msg = "error message is undefined";
    }

    if (NULL == fnn_arg) {
        n = fprintf(stderr, "FAIL (%d => %s) at %s, line %d: %s\n", error_arg, err_msg,
                filen_arg, lineno_arg, expr_arg);
        if (n < 1) {
            NETPERF_PANIC("fprintf() failed.");
        }
    } else {
        n = fprintf(stderr, "FAIL (%d => %s) in %s, at %s, line %d: %s\n", error_arg, err_msg,
                fnn_arg, filen_arg, lineno_arg, expr_arg);
        if (n < 1) {
            NETPERF_PANIC("fprintf() failed.");
        }
   }
}

#define NETPERF_UNLIKELY(Cond) __builtin_expect((Cond), 0)
#define NETPERF_LIKELY(Cond) __builtin_expect((Cond), 1)
#define MAKE_IP_ADDR(a, b, c, d)			\
	(((uint32_t) a << 24) | ((uint32_t) b << 16) |	\
	 ((uint32_t) c << 8) | (uint32_t) d)

#define NETPERF_TRUE2(Error, Condition, ErrorCache) \
    do { \
        const int ErrorCache = (Error); \
        if (NETPERF_UNLIKELY(!(Condition))) { \
            netperf_fail(ErrorCache, #Condition, NULL, __FILE__, __LINE__);  \
            return ErrorCache; \
        } \
   } while (0)

#define NETPERF_TRUE(Error, Condition) \
    NETPERF_TRUE2(Error, Condition, NETPERF_TRUE_errorCache)

#define NETPERF_ZERO(Error, Value) NETPERF_TRUE((Error), 0 == (Value))
#define NETPERF_NONZERO(Error, Value) NETPERF_TRUE((Error), 0 != (Value))
#define NETPERF_NULL(Error, Value) NETPERF_TRUE((Error), NULL == (Value))
#define NETPERF_NOTNULL(Error, Value) NETPERF_TRUE((Error), NULL != (Value
/******************************************/
/******************************************/
/* DPDK CONSTANTS */
#define RX_RING_SIZE 2048
#define TX_RING_SIZE 2048

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define UDP_MAX_PAYLOAD 1472
#define BURST_SIZE 32
#define IP_DEFTTL  64   /* from RFC 1340. */
#define IP_VERSION 0x40
#define IP_HDRLEN  0x05 /* default IP header length == five 32-bits words. */
#define IP_VHL_DEF (IP_VERSION | IP_HDRLEN)
#define MBUF_BUF_SIZE RTE_ETHER_MAX_JUMBO_FRAME_LEN + RTE_PKTMBUF_HEADROOM
#define RX_PACKET_LEN 9216
/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
#define RX_PTHRESH          8 /**< Default values of RX prefetch threshold reg. */
#define RX_HTHRESH          8 /**< Default values of RX host threshold reg. */
#define RX_WTHRESH          0 /**< Default values of RX write-back threshold reg. */

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define TX_PTHRESH          0 /**< Default values of TX prefetch threshold reg. */
#define TX_HTHRESH          0  /**< Default values of TX host threshold reg. */
#define TX_WTHRESH          0  /**< Default values of TX write-back threshold reg. */

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT    128
#define RTE_TEST_TX_DESC_DEFAULT    128

#define FULL_MAX 0xFFFFFFFF
#define EMPTY_MAX 0x0
#define PAGE_SIZE 4096
/******************************************/
/******************************************/
/*Static Variables*/
enum {
    MODE_UDP_CLIENT = 0,
    MODE_UDP_SERVER
};

enum {
    MEM_DPDK = 0,
    MEM_EXT,
    MEM_EXT_MANUAL,
    MEM_EXT_MANUAL_DPDK
};

const struct rte_ether_addr ether_broadcast = {
    .addr_bytes = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
};
struct rte_ether_addr server_mac = {
    .addr_bytes = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
};
static uint16_t dpdk_nbports;
static uint8_t mode;
static uint8_t memory_mode;
static size_t num_mbufs = 1;
static uint32_t my_ip;
static uint32_t server_ip;
static size_t message_size = 1000;
static uint32_t seconds = 1;
static uint32_t rate = 500000; // in packets / second
static uint32_t intersend_time;
static unsigned int client_port = 12345;
static unsigned int server_port = 72345;
struct rte_mempool *mbuf_pool;
struct rte_mempool *extbuf_mempool;
struct rte_mempool *header_mempool;
//struct rte_mempool *tx_mbuf_pool;
//struct rte_mempool *tx_mbuf_pool;
static uint64_t ext_mem_iova;
static uint16_t our_dpdk_port_id;
static struct rte_ether_addr my_eth;
static Latency_Dist_t latency_dist = { .min = LONG_MAX, .max = 0, .total_count = 0, .latency_sum = 0 };
static uint64_t clock_offset = 0;
static size_t header_payload_size = 0; // for num_mbufs = 2, how much data comes after header in first mbuf (rest of size in second)

static struct rte_mbuf_ext_shared_info *shinfo = NULL;
float page_alloc_times[1023];

static int zero_copy_mode = 0;
static void *payload_to_copy = NULL;

// static unsigned int num_queues = 1;
/******************************************/
/******************************************/

static int str_to_mac(const char *s, struct rte_ether_addr *mac_out) {
    assert(RTE_ETHER_ADDR_LEN == 6);
    unsigned int values[RTE_ETHER_ADDR_LEN];
    int ret = sscanf(s, "%2x:%2x:%2x:%2x:%2x:%2x%*c", &values[0], &values[1], &values[2], &values[3],&values[4], &values[5]);
    if (6 != ret) {
        printf("Scan of mac addr %s was not 6, but length %d\n", s, ret);
        return EINVAL;
    }

    for (size_t i = 0; i < RTE_ETHER_ADDR_LEN; ++i) {
        mac_out->addr_bytes[i] = (uint8_t)(values[i]);
    }
    return 0;
}

static int str_to_ip(const char *str, uint32_t *addr)
{
	uint8_t a, b, c, d;
	if(sscanf(str, "%hhu.%hhu.%hhu.%hhu", &a, &b, &c, &d) != 4) {
		return -EINVAL;
	}

	*addr = MAKE_IP_ADDR(a, b, c, d);
	return 0;
}

static int str_to_long(const char *str, long *val)
{
	char *endptr;

	*val = strtol(str, &endptr, 10);
	if (endptr == str || (*endptr != '\0' && *endptr != '\n') ||
	    ((*val == LONG_MIN || *val == LONG_MAX) && errno == ERANGE))
		return -EINVAL;
	return 0;
}

static void print_usage(void) {
    printf("To run client: netperf <EAL_INIT> -- --mode=CLIENT --ip=<CLIENT_IP> --server_ip=<SERVER_IP> --server_mac=<SERVER_MAC> --port=<PORT> --time=<TIME_SECONDS> --message_size<MESSAGE_SIZE_BYTES> --rate<RATE_PKTS_PER_S>.\n");
    printf("To run server: netperf <EAL_INIT> -- --mode=SERVER --ip=<SERVER_IP> --memory=<EXTERNAL,DPDK,MANUAL,MANUAL_DPDK> --num_mbufs=<INT> --header_payload=<INT>\n");
}

static inline struct tx_pktmbuf_priv *tx_pktmbuf_get_priv(struct rte_mbuf *buf)
{
	return (struct tx_pktmbuf_priv *)(((char *)buf)
			+ sizeof(struct rte_mbuf));
}

static void custom_init_priv(struct rte_mempool *mp __attribute__((unused)), void *opaque_arg __attribute__((unused)), void *m, unsigned i __attribute__((unused))) {
    struct rte_mbuf *buf = m;
	struct tx_pktmbuf_priv *data = tx_pktmbuf_get_priv(buf);
	memset(data, 0, sizeof(*data));
    data->lkey = -1;
    struct rte_mbuf *pkt = (struct rte_mbuf *)(m);
}

static void custom_pkt_init(struct rte_mempool *mp __attribute__((unused)), void *opaque_arg __attribute__((unused)), void *m, unsigned i __attribute__((unused))) {
    struct rte_mbuf *pkt = (struct rte_mbuf *)(m);
    uint8_t *p = rte_pktmbuf_mtod(pkt, uint8_t *);
    p += 42;
    char *s = (char *)(p);
    memset(s, 'a', 8000);
}

static int init_extbuf_mempool(void) {
    struct rte_pktmbuf_pool_private mbp_priv;
    int ret;
    unsigned elt_size;
    elt_size = sizeof(struct rte_mbuf) + sizeof(struct rte_pktmbuf_pool_private);
    mbp_priv.mbuf_data_room_size = 0;
    mbp_priv.mbuf_priv_size = sizeof(struct tx_pktmbuf_priv);

    extbuf_mempool = rte_mempool_create_empty("extbuf_mempool", NUM_MBUFS * dpdk_nbports,
                                                elt_size,
                                                MBUF_CACHE_SIZE,
                                                sizeof(struct rte_pktmbuf_pool_private),
                                                rte_socket_id(), 0);
    if (extbuf_mempool == NULL) {
        printf("Couldn't initialize an extbuf mempool\n");
        return 1;
    }

    rte_pktmbuf_pool_init(extbuf_mempool, &mbp_priv);
    if (rte_mempool_populate_default(extbuf_mempool) != (NUM_MBUFS * dpdk_nbports)) {
        printf("Mempool populate didnt init correct number\n");
        return 1;
    }

    if (rte_mempool_obj_iter(
            extbuf_mempool,
            rte_pktmbuf_init,
            NULL) != (NUM_MBUFS * dpdk_nbports)) {
            printf("Mempool obj iter didn't init correct number.\n");
            return 1;
        };

    if (rte_mempool_obj_iter(
                extbuf_mempool,
                custom_init_priv,
                NULL) != (NUM_MBUFS * dpdk_nbports)) {
        return 1;
    }

    printf("Finished initializing extbuf_mempool\n");

    return 0;
}

static int parse_args(int argc, char *argv[]) {
    long tmp;
    int has_server_ip = 0;
    int has_port = 0;
    int has_message_size = 0;
    int has_server_mac = 0;
    int has_seconds = 0;
    int has_rate = 0;
    int opt = 0;
    int has_memory = 0;

    static struct option long_options[] = {
        {"mode",      required_argument,       0,  'm' },
        {"ip",      required_argument,       0,  'i' },
        {"server_ip", optional_argument,       0,  's' },
        {"port", optional_argument, 0,  'p' },
        {"server_mac",   optional_argument, 0,  'c' },
        {"message_size",   optional_argument, 0,  'z' },
        {"time",   optional_argument, 0,  't' },
        {"rate",   optional_argument, 0,  'r' },
        {"memory", optional_argument, 0, 'b' },
        {"num_mbufs", optional_argument, 0, 'n'},
        {"header_payload", optional_argument, 0, 'h'},
        {"zero_copy", no_argument, 0, 'k'},
        {0,           0,                 0,  0   }
    };
    int long_index = 0;
    while ((opt = getopt_long(argc, argv,"m:i:s:p:c:z:t:r:b:n:h:k:",
                   long_options, &long_index )) != -1) {
        switch (opt) {
            case 'm':
                if (!strcmp(optarg, "CLIENT")) {
                    mode = MODE_UDP_CLIENT;
                } else if (!strcmp(optarg, "SERVER")) {
                    mode = MODE_UDP_SERVER;
                } else {
                    printf("mode should be SERVER or CLIENT\n");
                    return -EINVAL;
                }
                break;
            case 'i':
                str_to_ip(optarg, &my_ip);
                break;
            case 's':
                has_server_ip = 1;
                str_to_ip(optarg, &server_ip);
                break;
            case 'p':
                has_port = 1;
                if (sscanf(optarg, "%u", &client_port) != 1) {
                    return -EINVAL;
                }
                server_port = client_port;
                break;
            case 'c':
                has_server_mac = 1;
                if (str_to_mac(optarg, &server_mac) != 0) {
                    printf("Failed to convert %s to mac address\n", optarg);
                    return -EINVAL;
                }
                break;
            case 'z':
                has_message_size = 1;
                str_to_long(optarg, &tmp);
                message_size = tmp;
                break;
            case 't':
                has_seconds = 1;
                str_to_long(optarg, &tmp);
                seconds = tmp;
                break;
            case 'r':
                has_rate = 1;
                str_to_long(optarg, &tmp);
                rate = tmp;
                intersend_time = 1e9 / rate;
                break;
            case 'b':
                has_memory = 1;
                if (!strcmp(optarg, "EXTERNAL")) {
                    memory_mode = MEM_EXT;
                } else if (!strcmp(optarg, "MANUAL")) {
                    memory_mode = MEM_EXT_MANUAL;
                } else if (!strcmp(optarg, "MANUAL_DPDK")) {
                    memory_mode = MEM_EXT_MANUAL_DPDK;
                } else {
                    memory_mode = MEM_DPDK;
                }
                break;
            case 'n':
                str_to_long(optarg, &tmp);
                num_mbufs = (size_t)(tmp);
                break;
            case 'h':
                str_to_long(optarg, &tmp);
                header_payload_size = (size_t)(tmp);
                break;
            case 'k':
                zero_copy_mode = 1;
                break;
            default: 
                 print_usage();
                 exit(EXIT_FAILURE);
        }
    }
    if (mode == MODE_UDP_CLIENT) {
        if (!has_server_ip) {
            printf("Server ip, -s, --server_ip=, required.\n");
            exit(EXIT_FAILURE);
        }
        if (!has_server_mac) {
            printf("Server mac, -c, --server_mac=,required.\n");
            exit(EXIT_FAILURE);
        }

        // check we have enough space to store all the times.
        if ((1e9 * seconds)/intersend_time > MAX_ITERATIONS) {
            printf("Provided rate: %u in %u seconds implies more than %u packets sent. Please change the MAX_ITERATIONS constant and recompile (how many latencies are stored).\n", (unsigned)(rate), (unsigned)seconds, (unsigned)MAX_ITERATIONS);
           exit(EXIT_FAILURE); 
        }

        if (!has_port || !has_seconds || !has_rate || !has_message_size) {
            printf("If options for --time, --rate, or --message_size aren't provided, defaults will be used.\n");
        }
        printf("Running with:\n\t- port: %u\n\t- time: %u seconds\n\t- message_size: %u bytes\n\t- rate: %u pkts/sec (%u ns inter-packet send time)\n", (unsigned)client_port, (unsigned)seconds, (unsigned)message_size, (unsigned)rate, (unsigned)intersend_time);
    } else {
        // server mode
        if (!has_memory) {
            memory_mode = MEM_DPDK;
        }
        printf("Using: \n\t- nb_mbufs: %u\n\t- external_memory_mode: %d, external_memory_mode manual: %d\n", (unsigned)num_mbufs, (memory_mode == MEM_EXT), (memory_mode == MEM_EXT_MANUAL));
    
        if (memory_mode == MEM_EXT || memory_mode == MEM_EXT_MANUAL || memory_mode == MEM_EXT_MANUAL_DPDK) {
            int ret = init_extbuf_mempool();
            printf("Initialized extbuf mempool\n");
            if (ret != 0) {
                printf("Error in int extbuf mempool: %d\n", ret);
            }
        }

        if (memory_mode == MEM_DPDK) {
            if (zero_copy_mode != 1) {
                printf("Trying to initialize payload to copy\n");
                payload_to_copy = malloc(8000);
                memset(payload_to_copy, 'E', 8000);
                if (payload_to_copy == NULL) {
                    printf("Could not initialize payload to copy\n.");
                    return 1;
                }
            }
        }
        if ((memory_mode == MEM_DPDK || memory_mode == MEM_EXT_MANUAL_DPDK) && num_mbufs == 2) {
            // don't need to initialize payload into this mempool
            header_mempool = rte_pktmbuf_pool_create(
                                "header_pool",
                                NUM_MBUFS * dpdk_nbports,
                                MBUF_CACHE_SIZE,
                                sizeof(struct tx_pktmbuf_priv),
                                MBUF_BUF_SIZE,
                                rte_socket_id());
            if (header_mempool == NULL) {
                printf("Failed to initialize header mempool\n");
            }
            rte_mempool_obj_iter(header_mempool, &custom_pkt_init, NULL);
            if (rte_mempool_obj_iter(
                    header_mempool,
                    &custom_init_priv,
                    NULL) != (NUM_MBUFS * dpdk_nbports)) {
            return 1;
            }


        }

        if (header_payload_size != 0) {
            if (num_mbufs != 2) {
                printf("Warning: ignoring header_payload_size arg if num_mbufs!=2.\n");
            }
        }

    }

    const char *s = getenv("MLX5_SHUT_UP_BF");
    printf("Running with shutupbf flag set as: %s.\n", s);
    return 0;

}

static void free_external_buffer_callback(void *addr, void *opaque) {
    if (0 == 1) {
        printf("Addr: %p\n", addr);
        printf("opaque is null: %d\n", opaque == NULL);
    }
}

/* Taken from shenango:
 * https://github.com/shenango/shenango/blob/16ea43895a37cb4c04d5065f1073cc452f0bc00c/base/mem.c
 * */
static int mem_lookup_page_phys_addrs(void *addr, size_t len,
			       size_t pgsize, physaddr_t *paddrs)
{
	uintptr_t pos;
	uint64_t tmp;
	int fd, i = 0, ret = 0;

	/*
	 * 4 KB pages could be swapped out by the kernel, so it is not
	 * safe to get a machine address. If we later decide to support
	 * 4KB pages, then we need to mlock() the page first.
	 */
	/*if (pgsize == PGSIZE_4KB)
		return -EINVAL;*/
	if (pgsize == PGSIZE_4KB) {
	  mlock(addr, len);
	}

	fd = open("/proc/self/pagemap", O_RDONLY);
	if (fd < 0)
		return -EIO;

	for (pos = (uintptr_t)addr; pos < (uintptr_t)addr + len;
	     pos += pgsize) {
		if (lseek(fd, pos / PGSIZE_4KB * sizeof(uint64_t), SEEK_SET) ==
		    (off_t)-1) {
			ret = -EIO;
			goto out;
		}
		if (read(fd, &tmp, sizeof(uint64_t)) <= 0) {
			ret = -EIO;
			goto out;
		}
		if (!(tmp & PAGEMAP_FLAG_PRESENT)) {
			ret = -ENODEV;
			goto out;
		}

		paddrs[i++] = (tmp & PAGEMAP_PGN_MASK) * PGSIZE_4KB;
	}

out:
	close(fd);
	return ret;
}

static int ext_mem_manual(void **ext_mem_addr, 
			       physaddr_t *maddrs, 
			       int32_t *lkey_out,
			       int8_t division,
			       size_t num_pages) {
    // TODO: seems like specifying MAP_HUGE_2MB is not possible
    // printf("Register and deregister external memory!\n");
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_HUGETLB;
    size_t pgsize;
    if (division == 0) {
	pgsize = PGSIZE_4KB;
    } else {
	pgsize = PGSIZE_2MB;
    }
    //size_t num_pages = 5;
    void * addr = mmap(NULL, pgsize * num_pages, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (addr == MAP_FAILED) {
        printf("Failed to mmap memory\n");
    }

    memset((char *)addr, 'D', pgsize * num_pages);
    *ext_mem_addr = addr;
    int ret = mem_lookup_page_phys_addrs(addr, pgsize * num_pages, pgsize, maddrs);
    if (ret != 0) {
        printf("Lookup to phys addr failed\n");
        goto out;
    }
    // manually register the memory with the MLX5 device
    void *ret_addr = mlx5_manual_reg_mr(0, addr, pgsize * num_pages, (uint32_t *)lkey_out);
    if (!ret_addr) {
        printf("Failed to register mem with mlx5 device.\n");
        goto out;
    } else if (division < 0) {
      printf("Deregistering memory!\n");
      mlx5_manual_dereg_mr(ret_addr);
    }

out:
    munmap(addr, pgsize * num_pages);
    return ret;
}

static int init_ext_mem_manual(void **ext_mem_addr, 
			       physaddr_t *maddrs, 
			       int32_t *lkey_out) {
    // TODO: seems like specifying MAP_HUGE_2MB is not possible
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_HUGETLB;
    size_t pgsize = PGSIZE_2MB;
    size_t num_pages = 5;
    void * addr = mmap(NULL, pgsize * num_pages, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (addr == MAP_FAILED) {
        printf("Failed to mmap memory\n");
    }

    memset((char *)addr, 'D', pgsize * num_pages);
    *ext_mem_addr = addr;
    int ret = mem_lookup_page_phys_addrs(addr, pgsize * num_pages, pgsize, maddrs);
    if (ret != 0) {
        printf("Lookup to phys addr failed\n");
        goto out;
    }

    // manually register the memory with the MLX5 device
    void *ret_addr = mlx5_manual_reg_mr(0, addr, pgsize * num_pages, (uint32_t *)lkey_out);
	if (!ret_addr) {
        printf("Failed to register mem with mlx5 device.\n");
        goto out;
    }

out:
    munmap(addr, pgsize * num_pages);
    return ret;
}

static int init_ext_mem(void **ext_mem_addr) {
    size_t page_size = PAGE_SIZE;
    size_t num_pages = 100;
    /*void * addr = mmap(NULL, page_size * num_pages, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED , -1, 0);
    if (addr == MAP_FAILED) {
        printf("Failed to mmap memory\n");
        return 1;
    }

    // next, register this external memory
    int ret = rte_extmem_register(addr, (uint16_t)(page_size * num_pages), NULL, 0, page_size);
    if (ret != 0) {
        printf("Register err: %s\n", rte_strerror(rte_errno));
        return ret;
    }

    uint16_t pid = 0;
    RTE_ETH_FOREACH_DEV(pid) {
        struct rte_eth_dev *dev = &rte_eth_devices[pid];
        ret = rte_dev_dma_map(dev->device, addr, 0, (uint16_t)(page_size * num_pages));
        if (ret != 0) {
            printf("DMA map errno: %s\n", rte_strerror(rte_errno));
            return ret;
        }
    }
    memset((char *)addr, 'D', page_size * num_pages);
    uint16_t buf_len = (uint16_t)(page_size * num_pages);
    shinfo = rte_pktmbuf_ext_shinfo_init_helper(addr, &buf_len, free_external_buffer_callback, NULL);
    printf("Finished registering external memory\n");
    *ext_mem_addr = addr;*/
    void *addr = rte_malloc("Serialization_Memory", page_size * num_pages, 0);
    if (addr == NULL) {
        printf("Rte malloc failed\n");
        exit(1);
    }
    *ext_mem_addr = addr;
    ext_mem_iova = rte_malloc_virt2iova(*ext_mem_addr);
    uint16_t pid = 0;
    RTE_ETH_FOREACH_DEV(pid) {
        struct rte_eth_dev *dev = &rte_eth_devices[pid];
        int ret = rte_dev_dma_map(dev->device, *ext_mem_addr, ext_mem_iova, (size_t)(page_size * num_pages));
        if (ret != 0) {
            printf("DMA map errno: %s\n", rte_strerror(rte_errno));
            return ret;
        }
    }
    memset((char *)addr, 'D', page_size * num_pages);
    uint16_t buf_len = (uint16_t)(page_size * num_pages);
    shinfo = rte_pktmbuf_ext_shinfo_init_helper(addr, &buf_len, free_external_buffer_callback, NULL);
    if (ext_mem_iova == RTE_BAD_IOVA) {
        printf("Failed to get iova\n");
        return EINVAL;
    }
    printf("Ext mem adrr: %p\n", *ext_mem_addr);
    return 0;
}

static int print_link_status(FILE *f, uint16_t port_id, const struct rte_eth_link *link) {

    struct rte_eth_link link2 = {};
    if (NULL == link) {
        rte_eth_link_get_nowait(port_id, &link2);
        link = &link2;
    }
    if (ETH_LINK_UP == link->link_status) {
        const char * const duplex = ETH_LINK_FULL_DUPLEX == link->link_duplex ?  "full" : "half";
        fprintf(f, "Port %d Link Up - speed %u " "Mbps - %s-duplex\n", port_id, link->link_speed, duplex);
    } else {
        printf("Port %d Link Down\n", port_id);
    }

    return 0;
}

static int wait_for_link_status_up(uint16_t port_id) {
    NETPERF_TRUE(ERANGE, rte_eth_dev_is_valid_port(port_id));

    const size_t sleep_duration_ms = 100;
    const size_t retry_count = 90;

    struct rte_eth_link link = {};
    for (size_t i = 0; i < retry_count; ++i) {
        rte_eth_link_get_nowait(port_id, &link);
        if (ETH_LINK_UP == link.link_status) {
            print_link_status(stderr, port_id, &link);
            return 0;
        }

        rte_delay_ms(sleep_duration_ms);
    }
    print_link_status(stderr, port_id, &link);

    return ECONNREFUSED;

}


static int init_dpdk_port(uint16_t port_id, struct rte_mempool *mbuf_pool) {
    printf("Initializing port %u\n", (unsigned)(port_id));
    NETPERF_TRUE(ERANGE, rte_eth_dev_is_valid_port(port_id)); 
    const uint16_t rx_rings = 1;
    const uint16_t tx_rings = 1;
    const uint16_t nb_rxd = RX_RING_SIZE;
    const uint16_t nb_txd = TX_RING_SIZE;
    uint16_t mtu;
    
    struct rte_eth_dev_info dev_info = {};
    rte_eth_dev_info_get(port_id, &dev_info);
    rte_eth_dev_set_mtu(port_id, RX_PACKET_LEN);
    rte_eth_dev_get_mtu(port_id, &mtu);
    fprintf(stderr, "Dev info MTU:%u\n", mtu);
    struct rte_eth_conf port_conf = {};
    port_conf.rxmode.max_rx_pkt_len = RX_PACKET_LEN;
            
    port_conf.rxmode.offloads = DEV_RX_OFFLOAD_JUMBO_FRAME | DEV_RX_OFFLOAD_TIMESTAMP;
    port_conf.txmode.offloads = DEV_TX_OFFLOAD_MULTI_SEGS | DEV_TX_OFFLOAD_IPV4_CKSUM | DEV_TX_OFFLOAD_UDP_CKSUM;
    //    port_conf.rxmode.mq_mode = ETH_MQ_RX_RSS;
    //    port_conf.rx_adv_conf.rss_conf.rss_hf = ETH_RSS_IP | dev_info.flow_type_rss_offloads;
    port_conf.txmode.mq_mode = ETH_MQ_TX_NONE;

    struct rte_eth_rxconf rx_conf = {};
    rx_conf.rx_thresh.pthresh = RX_PTHRESH;
    rx_conf.rx_thresh.hthresh = RX_HTHRESH;
    rx_conf.rx_thresh.wthresh = RX_WTHRESH;
    rx_conf.rx_free_thresh = 32;

    struct rte_eth_txconf tx_conf = {};
    tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    tx_conf.tx_thresh.wthresh = TX_WTHRESH;

    // configure the ethernet device.
    rte_eth_dev_configure(port_id, rx_rings, tx_rings, &port_conf);

    // todo: what does this do?
    /*
    retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
    if (retval != 0) {
        return retval;
    }
    */

    // todo: this call fails and i don't understand why.
    int socket_id = rte_eth_dev_socket_id(port_id);

    // allocate and set up 1 RX queue per Ethernet port.
    for (uint16_t i = 0; i < rx_rings; ++i) {
        rte_eth_rx_queue_setup(port_id, i, nb_rxd, socket_id, &rx_conf, mbuf_pool);
    }

    // allocate and set up 1 TX queue per Ethernet port.
    for (uint16_t i = 0; i < tx_rings; ++i) {
        rte_eth_tx_queue_setup(port_id, i, nb_txd, socket_id, &tx_conf);
    }

    // start the ethernet port.
    int dev_start_ret = rte_eth_dev_start(port_id);
    if (dev_start_ret != 0) {
        printf("Failed to start ethernet for prot %u\n", (unsigned)port_id);
    }

    //NETPERF_OK(rte_eth_promiscuous_enable(port_id));

    // disable the rx/tx flow control
    // todo: why?
    struct rte_eth_fc_conf fc_conf = {};
    rte_eth_dev_flow_ctrl_get(port_id, &fc_conf);
    fc_conf.mode = RTE_FC_NONE;
    rte_eth_dev_flow_ctrl_set(port_id, &fc_conf);
    wait_for_link_status_up(port_id);

   return 0;
}

static int dpdk_init(int argc, char **argv) {
    
    // initialize Environment Abstraction Layer
    // our arguments: "-c", "0xff", "-n", "4", "-w", "0000:37:00.0","--proc-type=auto"
    int args_parsed = rte_eal_init(argc, argv);
    if (args_parsed < 0) {
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    }

    // initialize ports
    const uint16_t nbports = rte_eth_dev_count_avail();
    dpdk_nbports = nbports;
    if (nbports <= 0) {
       rte_exit(EXIT_FAILURE, "No ports available\n"); 
    }
    fprintf(stderr, "DPDK reports that %d ports (interfaces) are available.\n", nbports);

    // create a pool of memory for ring buffers
    mbuf_pool = rte_pktmbuf_pool_create(
                                "mbuf_pool",
                                NUM_MBUFS * nbports,
                                MBUF_CACHE_SIZE,
                                sizeof(struct tx_pktmbuf_priv),
                                MBUF_BUF_SIZE,
                                rte_socket_id());
    if (mbuf_pool == NULL) {
        rte_exit(EXIT_FAILURE, "Was not able to initialize mbuf_pool.\n");
    }

    rte_mempool_obj_iter(mbuf_pool, &custom_pkt_init, NULL);
    if (rte_mempool_obj_iter(
                mbuf_pool,
                &custom_init_priv,
                NULL) != (NUM_MBUFS * dpdk_nbports)) {
        return 1;
    }
    // initialize all ports
    uint16_t i = 0;
    uint16_t port_id = 0;
    RTE_ETH_FOREACH_DEV(i) {
        port_id = i;
        if (init_dpdk_port(i, mbuf_pool) != 0) {
            rte_exit(EXIT_FAILURE, "Failed to initialize port %u\n", (unsigned) port_id);
        }
    }
    our_dpdk_port_id = port_id;
    rte_eth_macaddr_get(our_dpdk_port_id, &my_eth);
    printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			(unsigned)our_dpdk_port_id,
			my_eth.addr_bytes[0], my_eth.addr_bytes[1],
			my_eth.addr_bytes[2], my_eth.addr_bytes[3],
			my_eth.addr_bytes[4], my_eth.addr_bytes[5]);


    if (rte_lcore_count() > 1) {
        printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");
    }
    
    return args_parsed;
}

static uint64_t raw_time(void) {
    struct timespec tstart={0,0};
    clock_gettime(CLOCK_MONOTONIC, &tstart);
    uint64_t t = (uint64_t)(tstart.tv_sec*1.0e9 + tstart.tv_nsec);
    return t;

}

static uint64_t time_now(uint64_t offset) {
    return raw_time() - offset;
}

static int parse_packet(struct sockaddr_in *src,
                        struct sockaddr_in *dst,
                        void **payload,
                        size_t *payload_len,
                        struct rte_mbuf *pkt)
{
    // packet layout order is (from outside -> in):
    // ether_hdr
    // ipv4_hdr
    // udp_hdr
    // client timestamp
    uint8_t *p = rte_pktmbuf_mtod(pkt, uint8_t *);
    size_t header = 0;

    // check the ethernet header
    struct rte_ether_hdr * const eth_hdr = (struct rte_ether_hdr *)(p);
    p += sizeof(*eth_hdr);
    header += sizeof(*eth_hdr);
    uint16_t eth_type = ntohs(eth_hdr->ether_type);
    struct rte_ether_addr mac_addr = {};

    rte_eth_macaddr_get(our_dpdk_port_id, &mac_addr);
    if (!rte_is_same_ether_addr(&mac_addr, &eth_hdr->d_addr) && !rte_is_same_ether_addr(&ether_broadcast, &eth_hdr->d_addr)) {
        printf("Bad MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
            eth_hdr->d_addr.addr_bytes[0], eth_hdr->d_addr.addr_bytes[1],
			eth_hdr->d_addr.addr_bytes[2], eth_hdr->d_addr.addr_bytes[3],
			eth_hdr->d_addr.addr_bytes[4], eth_hdr->d_addr.addr_bytes[5]);
        return 1;
    }
    if (RTE_ETHER_TYPE_IPV4 != eth_type) {
        printf("Bad ether type\n");
        return 1;
    }

    // check the IP header
    struct rte_ipv4_hdr *const ip_hdr = (struct rte_ipv4_hdr *)(p);
    p += sizeof(*ip_hdr);
    header += sizeof(*ip_hdr);

    // In network byte order.
    in_addr_t ipv4_src_addr = ip_hdr->src_addr;
    in_addr_t ipv4_dst_addr = ip_hdr->dst_addr;

    if (IPPROTO_UDP != ip_hdr->next_proto_id) {
        printf("Bad next proto_id\n");
        return 1;
    }
    
    src->sin_addr.s_addr = ipv4_src_addr;
    dst->sin_addr.s_addr = ipv4_dst_addr;

    // check udp header
    struct rte_udp_hdr * const udp_hdr = (struct rte_udp_hdr *)(p);
    p += sizeof(*udp_hdr);
    header += sizeof(*udp_hdr);

    // In network byte order.
    in_port_t udp_src_port = udp_hdr->src_port;
    in_port_t udp_dst_port = udp_hdr->dst_port;

    src->sin_port = udp_src_port;
    dst->sin_port = udp_dst_port;
    src->sin_family = AF_INET;
    dst->sin_family = AF_INET;
    
    *payload_len = pkt->pkt_len - header;
    *payload = (void *)p;
    return 0;

}

static uint16_t rte_eth_tx_burst_(uint16_t port_id, uint16_t queue_id, struct rte_mbuf **tx_pkts, uint16_t nb_pkts) {
    return rte_eth_tx_burst(port_id, queue_id, tx_pkts, nb_pkts);
}

void rte_pktmbuf_free_(struct rte_mbuf *packet) {
    rte_pktmbuf_free(packet);
}

struct rte_mbuf* rte_pktmbuf_alloc_(struct rte_mempool *mp) {
    return rte_pktmbuf_alloc(mp);
}

uint16_t rte_eth_rx_burst_(uint16_t port_id, uint16_t queue_id, struct rte_mbuf **rx_pkts, const uint16_t nb_pkts) {
    uint16_t ret = rte_eth_rx_burst(port_id, queue_id, rx_pkts, nb_pkts);
    return ret;
}

uint64_t rte_get_timer_cycles_() {
    return rte_get_timer_cycles();
}

uint64_t rte_get_timer_hz_() {
    return rte_get_timer_hz();
}

void rte_pktmbuf_attach_extbuf_(struct rte_mbuf *m, void *buf_addr, rte_iova_t buf_iova, uint16_t buf_len, struct rte_mbuf_ext_shared_info *shinfo) {
    rte_pktmbuf_attach_extbuf(m, buf_addr, buf_iova, buf_len, shinfo);
}

uint32_t
checksum(unsigned char *buf, uint32_t nbytes, uint32_t sum)
{
	unsigned int	 i;

	/* Checksum all the pairs of bytes first. */
	for (i = 0; i < (nbytes & ~1U); i += 2) {
		sum += (uint16_t)ntohs(*((uint16_t *)(buf + i)));
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}

	/*
	 * If there's a single byte left over, checksum it, too.
	 * Network byte order is big-endian, so the remaining byte is
	 * the high byte.
	 */
	if (i < nbytes) {
		sum += buf[i] << 8;
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}

	return sum;
}

uint32_t
wrapsum(uint32_t sum)
{
	sum = ~sum & 0xFFFF;
	return htons(sum);
}

static int do_client(void) {
    clock_offset = raw_time();
    uint64_t start_time, end_time;
    struct rte_mbuf *pkts[BURST_SIZE];
    struct rte_mbuf *pkt;
    // char *buf_ptr;
    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ipv4_hdr;
    struct rte_udp_hdr *udp_hdr;
    uint16_t nb_rx;
    uint64_t reqs = 0;
    uint64_t cycle_wait = intersend_time * rte_get_timer_hz() / (1e9);
    
    // TODO: add in scaffolding for timing/printing out quick statistics
    start_time = rte_get_timer_cycles();
    int outstanding = 0;
    while (rte_get_timer_cycles_() < start_time + seconds * rte_get_timer_hz_()) {
        // send a packet
        pkt = rte_pktmbuf_alloc_(mbuf_pool);
        if (pkt == NULL) {
            printf("Error allocating tx mbuf\n");
            return -EINVAL;
        }
        size_t header_size = 0;

        uint8_t *ptr = rte_pktmbuf_mtod(pkt, uint8_t *);
        /* add in an ethernet header */
        eth_hdr = (struct rte_ether_hdr *)ptr;
        rte_ether_addr_copy(&my_eth, &eth_hdr->s_addr);
        rte_ether_addr_copy(&server_mac, &eth_hdr->d_addr);
        eth_hdr->ether_type = htons(RTE_ETHER_TYPE_IPV4);
        ptr += sizeof(*eth_hdr);
        header_size += sizeof(*eth_hdr);

        /* add in ipv4 header*/
        ipv4_hdr = (struct rte_ipv4_hdr *)ptr;
        ipv4_hdr->version_ihl = 0x45;
        ipv4_hdr->type_of_service = 0x0;
        ipv4_hdr->total_length = rte_cpu_to_be_16(sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr) + message_size);
        ipv4_hdr->packet_id = rte_cpu_to_be_16(1);
        ipv4_hdr->fragment_offset = 0;
        ipv4_hdr->time_to_live = 64;
        ipv4_hdr->next_proto_id = IPPROTO_UDP;
        ipv4_hdr->src_addr = rte_cpu_to_be_32(my_ip);
        ipv4_hdr->dst_addr = rte_cpu_to_be_32(server_ip);

        uint32_t ipv4_checksum = wrapsum(checksum((unsigned char *)ipv4_hdr, sizeof(struct rte_ipv4_hdr), 0));
        //printf("Checksum is %u\n", (unsigned)ipv4_checksum);
        ipv4_hdr->hdr_checksum = rte_cpu_to_be_32(ipv4_checksum);
        header_size += sizeof(*ipv4_hdr);
        ptr += sizeof(*ipv4_hdr);

        /* add in UDP hdr*/
        udp_hdr = (struct rte_udp_hdr *)ptr;
        udp_hdr->src_port = rte_cpu_to_be_16(client_port);
        udp_hdr->dst_port = rte_cpu_to_be_16(server_port);
        udp_hdr->dgram_len = rte_cpu_to_be_16(sizeof(struct rte_udp_hdr) + message_size);

	//sum = wrapsum(checksum((unsigned char *)udp, sizeof(*udp),
	 //   checksum(data, len, checksum((unsigned char *)&ip->ip_src,
	   // 2 * sizeof(ip->ip_src),
	    //IPPROTO_UDP + (uint32_t)ntohs(udp->uh_ulen)))));
        uint16_t udp_cksum =  rte_ipv4_udptcp_cksum(ipv4_hdr, (void *)udp_hdr);


        /*uint32_t udp_cksum = wrapsum(checksum((unsigned_char *)udp_hdr,
                                    sizeof(*udp_hdr), 
                                    checksum((ptr + sizeof(*udp_hdr)), 
                                              message_size, 
                                              checksum((unsigned_char *)&ipv4_hdr->src_addr, 
                                              2 * sizeof(ip->src_addr), 
                                              IPPROTO_UDP + (uint32_t)ntohs(sizeof(*udp_hdr))))));*/
        //printf("Udp checksum is %u\n", (unsigned)udp_cksum);
        udp_hdr->dgram_cksum = rte_cpu_to_be_16(udp_cksum);
        ptr += sizeof(*udp_hdr);
        header_size += sizeof(*udp_hdr);
        
        /* set the payload */
        memset(ptr, 'a', message_size);
        /* record timestamp in the payload itself*/
        uint64_t send_time = time_now(clock_offset);
        uint64_t *timestamp_ptr = (uint64_t *)(ptr);
        *timestamp_ptr = send_time;

        pkt->l2_len = RTE_ETHER_HDR_LEN;
        pkt->l3_len = sizeof(struct rte_ipv4_hdr);
        pkt->ol_flags = PKT_TX_IP_CKSUM | PKT_TX_IPV4;
        pkt->data_len = header_size + message_size;
        pkt->pkt_len = header_size + message_size;
        pkt->nb_segs = 1;
        int pkts_sent = 0;

        unsigned char *pkt_buffer = rte_pktmbuf_mtod(pkt, unsigned char *);
        int ct = 0;
        for (int i = 0; i < sizeof(struct rte_ether_hdr); i++) {
            //printf("%02hhx", pkt_buffer[i]);
            ct += 1;
            if (ct % 2 == 0) {
                //printf(" ");
            }
        }
        //printf("\n");
        ct = 0;
        for (int i = sizeof(struct rte_ether_hdr); i < sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr); i++) {
            //printf("%02hhx", pkt_buffer[i]);
            ct += 1;
            if (ct % 2 == 0) {
                //printf(" ");
            }
        }
        //printf("\n");
        ct = 0;
        for (int i = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr); i < sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr); i++) {
            //printf("%02hhx", pkt_buffer[i]);
            ct += 1;
            if (ct % 2 == 0) {
                //printf(" ");
            }
        }
        //printf("\n");
        //exit(1);

        while (pkts_sent < 1) {
            pkts_sent = rte_eth_tx_burst(our_dpdk_port_id, 0, &pkt, 1);
        }
        outstanding ++;
        uint64_t last_sent = rte_get_timer_cycles();
        printf("Sent packet at %u, %d is outstanding, intersend is %u\n", (unsigned)last_sent, outstanding, (unsigned)intersend_time);

        /* now poll on receiving packets */
        nb_rx = 0;
        reqs += 1;
        while ((outstanding > 0)) {
            nb_rx = rte_eth_rx_burst_(our_dpdk_port_id, 0, pkts, BURST_SIZE);
            if (nb_rx == 0) {
                if (rte_get_timer_cycles() > (last_sent + cycle_wait)) {
                    break;
                }
                continue;
            }

            printf("Received burst of %u\n", (unsigned)nb_rx);
            for (int i = 0; i < nb_rx; i++) {
                struct sockaddr_in src, dst;
                void *payload = NULL;
                size_t payload_length = 0;
                int valid = parse_packet(&src, &dst, &payload, &payload_length, pkts[i]);
                if (valid == 0) {
                    /* parse the timestamp and record it */
                    uint64_t now = (uint64_t)time_now(clock_offset);
                    //printf("Got a packet at time now: %u\n", (unsigned)(now));
                    uint64_t then = (*(uint64_t *)payload);
                    add_latency(&latency_dist, now - then);
                    rte_pktmbuf_free_(pkts[i]);
                    outstanding--;
                } else {
                    rte_pktmbuf_free_(pkts[i]);
                }
            }
        }
        while (((last_sent + cycle_wait) >= rte_get_timer_cycles())) {
            continue;
        }
        // printf("Reached end of loop\n");
    }
    end_time = rte_get_timer_cycles();
    printf("Ran for %f seconds, sent %"PRIu64" packets.\n",
			(float) (end_time - start_time) / rte_get_timer_hz(), reqs);
    dump_latencies(&latency_dist);
    return 0;
}

static int do_server(size_t division, size_t page_num) {
    // initialize external memory
    /*void *ext_mem_addr = NULL;
    void *paddrs_mem = malloc(sizeof(physaddr_t) * 100);
    int32_t lkey = -1;
    if (paddrs_mem == NULL) {
        printf("Error malloc'ing paddr for storing physical addresses.\n");
        return ENOMEM;
    }
    physaddr_t *paddrs = (physaddr_t *)paddrs_mem;
    void *ext_mem_phys_addr = NULL;
    if (memory_mode == MEM_EXT) {
        int ret = init_ext_mem(&ext_mem_addr);
        if (ret != 0) {
            printf("Error in extmem init: %d\n", ret);
            return ret;
        }
    } else if (memory_mode == MEM_EXT_MANUAL) {
        int ret = init_ext_mem_manual(&ext_mem_addr, paddrs, &lkey);
        if (ret != 0) {
            printf("Error in extmem manual init: %d\n", ret);
            return ret;
        } else if (lkey == -1) {
            printf("Lkey still -1\n");
            return ret;
        }
    }*/
    void *ext_mem_addr = NULL;
    void *paddrs_mem = malloc(sizeof(physaddr_t) * 100);
    int32_t lkey = -1;
    if (paddrs_mem == NULL) {
       printf("Error malloc'ing paddr for storing physical addresses.\n");
       return ENOMEM;
    }
    physaddr_t *paddrs = (physaddr_t *)paddrs_mem;
    void *ext_mem_phys_addr = NULL;
    int ret = ext_mem_manual(&ext_mem_addr, paddrs, &lkey, division, page_num);
    if (ret != 0) {
           printf("Error in extmem manual init: %d\n", ret);
           return ret;
    } else if (lkey == -1) {
           printf("Lkey still -1\n");
           return -1;
    }
    
    printf("Starting server program\n");
    return 0;
}

int
run_max_pages_benchmark(size_t page_num) 
{
   uint64_t start_time, end_time;

   // initialize external memory
   void *ext_mem_addr = NULL;
   void *paddrs_mem = malloc(sizeof(physaddr_t) * 100);
   int32_t lkey = -1;
   if (paddrs_mem == NULL) {
       printf("Error malloc'ing paddr for storing physical addresses.\n");
       return ENOMEM;
   }
   physaddr_t *paddrs = (physaddr_t *)paddrs_mem;
   void *ext_mem_phys_addr = NULL;
   printf("Nope 2\n");
   start_time = rte_get_timer_cycles();
   int ret = ext_mem_manual(&ext_mem_addr, paddrs, &lkey, -1, page_num);
   end_time = rte_get_timer_cycles();
   printf("Register %zu Pages: Ran for %f seconds.\n",
		page_num, (float) (end_time - start_time) / rte_get_timer_hz()); 
   page_alloc_times[page_num] = (float) (end_time - start_time) / rte_get_timer_hz();
   free(paddrs_mem);
   printf("Nope 3\n");
   if (ret != 0) {
	   printf("Error in extmem manual init: %d\n", ret);
           return ret;
   } else if (lkey == -1) {
	   printf("Lkey still -1\n");
	   return -1;
   }
   return 0;
}


void* max_mappings_benchmark(void **ext_mem_addr, 
			       physaddr_t *maddrs, 
			       int32_t *lkey_out,
			       size_t num_pages) {
    // TODO: seems like specifying MAP_HUGE_2MB is not possible
    // printf("Register and deregister external memory!\n");
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_HUGETLB;
    size_t pgsize = PGSIZE_2MB;
    size_t success = 0;
    //size_t num_pages = 5;
    void * addr = mmap(NULL, pgsize * num_pages, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (addr == MAP_FAILED) {
        printf("Failed to mmap memory\n");
    }

    memset((char *)addr, 'D', pgsize * num_pages);
    *ext_mem_addr = addr;
    int ret = mem_lookup_page_phys_addrs(addr, pgsize * num_pages, pgsize, maddrs);
    if (ret != 0) {
        printf("Lookup to phys addr failed\n");
        goto out;
    }
    // manually register the memory with the MLX5 device
    void *ret_addr = mlx5_manual_reg_mr(0, addr, pgsize * num_pages, (uint32_t *)lkey_out);
    if (!ret_addr) {
        printf("Failed to register mem with mlx5 device.\n");
        goto out;
    }
    success = 1;

out:
    munmap(addr, pgsize * num_pages);
    if (success) {
      return ret_addr;
    }
    return NULL;
}

unsigned int
deregister_memory(void** register_arr, size_t len)
{
  for (size_t i = 0; i < len; i++) {
    mlx5_manual_dereg_mr(register_arr[i]);
  }
}

int
main(int argc, char **argv)
{
    int ret;
    int args_parsed = dpdk_init(argc, argv);
    argc -= args_parsed;
    argv += args_parsed;

    // initialize our arguments
    ret = parse_args(argc, argv);
    if (ret != 0) {
        return ret;
    }

    /*Benchmark #1: How expensive is pinning large amounts of pages?*/
    size_t max_num_pages = 1023; //Why is 1024 not good??
    /*FILE *fp = fopen ("bench_one.csv", "w");
    for (size_t i = 1; i <= max_num_pages; i++) {
      int ret = run_max_pages_benchmark(i);
      if (ret != 0) {
        break;
      }
    }
    for (size_t i = 0; i < max_num_pages; i++) {
      fprintf(fp, "%lu,%f\n", i, page_alloc_times[i]);
    }
    fclose(fp);*/
    FILE *fp_two = fopen ("bench_two.csv", "w");
    void *ext_mem_addr = NULL;
   void *paddrs_mem = malloc(sizeof(physaddr_t) * 100);
   int32_t lkey = -1;
   if (paddrs_mem == NULL) {
       printf("Error malloc'ing paddr for storing physical addresses.\n");
       return ENOMEM;
   }
   physaddr_t *paddrs = (physaddr_t *)paddrs_mem;
   void *ext_mem_phys_addr = NULL;
   void* register_ptr[max_num_pages*max_num_pages];
   size_t num_mapping;
   size_t idx = 0;
   for (num_mapping = 1; num_mapping < max_num_pages - 5; num_mapping+=1) {
     uint64_t reg_is_null = 0;
     uint64_t start_time = rte_get_timer_cycles();
     size_t j = 0;
     for (j = 1; j < 5; j++) {
       void* reg = max_mappings_benchmark(&ext_mem_addr, paddrs, &lkey, 1);
       uint64_t end_time = rte_get_timer_cycles();
       fprintf(fp_two, "%lu,%lu,%f\n", num_mapping, j, (float) (end_time - start_time) / rte_get_timer_hz());
       if (reg == NULL) {
	 reg_is_null = 1;
         break;
       }
       register_ptr[idx] = reg;
       idx+=1;
     }
     if (reg_is_null) {
       continue;
     }
   }
   deregister_memory(&register_ptr, idx);
    /*Benchmark #2: What is the maximum number of mappings we can create?*/
   /* FILE *fp_two = fopen ("bench_two.csv", "w");
    size_t num_mappings = 1;
      printf("Number of pages: %lu\n", 50);
      void *ext_mem_addr = NULL;
        ret = max_mappings_benchmark(50, num_mappings, ext_mem_addr);
         if (ret != 0) {
	  break;
         }
	if (num_mappings > 100) {
	  continue;
        printf("Number of mappings: %lu with number of pages %u\n", num_mappings, 50);
	num_mappings += 1;*/
    printf("Reached end of program execution\n");
    return 0;
}
