#ifdef __GNUC__
#define ntohl __builtin_bswap32
#define ntohs __builtin_bswap16 
#define htonl __builtin_bswap32
#define htons __builtin_bswap16 

#else
#define ntohl _byteswap_ulong
#define ntohs _byteswap_ushort 
#define htonl _byteswap_ulong
#define htons _byteswap_ushort
#endif

#define EX_OK           0       /* successful termination */

#define EX__BASE        64      /* base value for error messages */

#define EX_USAGE        64      /* command line usage error */
#define EX_DATAERR      65      /* data format error */
#define EX_NOINPUT      66      /* cannot open input */
#define EX_NOUSER       67      /* addressee unknown */
#define EX_NOHOST       68      /* host name unknown */
#define EX_UNAVAILABLE  69      /* service unavailable */
#define EX_SOFTWARE     70      /* internal software error */
#define EX_OSERR        71      /* system error (e.g., can't fork) */
#define EX_OSFILE       72      /* critical OS file missing */
#define EX_CANTCREAT    73      /* can't create (user) output file */
#define EX_IOERR        74      /* input/output error */
#define EX_TEMPFAIL     75      /* temp failure; user is invited to retry */
#define EX_PROTOCOL     76      /* remote error in protocol */
#define EX_NOPERM       77      /* permission denied */
#define EX_CONFIG       78      /* configuration error */

#define EX__MAX 78      /* maximum listed value */

#define warn(...) do { \
	char *cp = strerror(errno); \
	fputs("dot_clean: ", stderr); \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr,": %s", cp); \
} while(0)

#define warnx(...) do { \
	fputs("dot_clean: ", stderr); \
	fprintf(stderr, __VA_ARGS__); \
} while(0)

#define warnc(ec, ...) do { \
	char *cp = strerror(ec); \
	fputs("dot_clean: ", stderr); \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr,": %s", cp); \
} while(0)
