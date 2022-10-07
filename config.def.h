#define FMT_BASE       1000                    /* bytes divider      */
#define FMT_PREC       "1"                     /* precision          */
#define FMT_UP_STR     "↑"                     /* upload indicator   */
#define FMT_DW_STR     "↓"                     /* download indicator */
#define FMT_PAD        6                       /* padding size       */

#define IO_BUF_SIZE    4096
#define INTERFACES_MAX 16u                     /* unsigned int       */
#define DELAY          1000000                 /* microsecond        */
#define NET_DIR        "/sys/class/net/"
#define RX_BYTES       "/statistics/rx_bytes"
#define TX_BYTES       "/statistics/tx_bytes"

/* --- */
extern int make_iso_compilers_happy;
