#include <bits/sockaddr.h>
#include <stdint.h>

#define AF_BP 60518

typedef enum bp_scheme {
  BP_SCHEME_IPN = 1,
  BP_SCHEME_DTN = 2,
} bp_scheme_t;

struct sockaddr_bp {
  __SOCKADDR_COMMON(sbp_);
  bp_scheme_t sbp_scheme;
  union {
    struct {
      uint32_t node;
      uint32_t service;
    } sbp_ipn;
    char sbp_dtn[255]; // RFC 9171 doesn't specify a limit, we'll use the same
                       // as domain names
  } sbp_addr;
};
