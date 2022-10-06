#define FMT_PAD        4                       /* padding size       */
#define FMT_UP_STR     "↑"                     /* upload indicator   */
#define FMT_DW_STR     "↓"                     /* download indicator */

#define INTERFACES_MAX 16u                     /* unsigned int       */
#define DELAY          1000000                 /* microsecond        */
#define NET_DIR        "/sys/class/net/"
#define RX_BYTES       "/statistics/rx_bytes"
#define TX_BYTES       "/statistics/tx_bytes"

/* --- */
extern int make_iso_compilers_happy;