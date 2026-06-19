
/* ============================================================
 * SOCKET API (Berkeley Sockets Benzeri)
 * ============================================================ */

#define SOCK_MAX_SOCKETS 32

typedef enum {
    SOCK_STREAM = 1,   /* TCP */
    SOCK_DGRAM  = 2,   /* UDP */
    SOCK_RAW    = 3    /* Raw IP */
} socket_type_t;

typedef enum {
    AF_INET = 2
} address_family_t;

typedef struct {
    address_family_t family;
    ip_addr_t addr;
    uint16_t port;
} sockaddr_in_t;

typedef struct {
    int fd;
    socket_type_t type;
    tcp_state_t state;
    sockaddr_in_t local;
    sockaddr_in_t remote;
    uint8_t *recv_buffer;
    int recv_len;
    uint8_t *send_buffer;
    int send_len;
    int blocking;
    int active;
    int listening;
    int backlog;
    int *accept_queue;
    int accept_count;
} socket_t;

static socket_t sockets[SOCK_MAX_SOCKETS];
static int socket_count = 0;

/* ============================================================
 * SOCKET İŞLEMLERİ
 * ============================================================ */

int socket_create(int type) {
    if(socket_count >= SOCK_MAX_SOCKETS) return -1;
    
    socket_t *sock = &sockets[socket_count];
    sock->fd = socket_count;
    sock->type = type;
    sock->state = TCP_CLOSED;
    sock->blocking = 1;
    sock->active = 1;
    sock->listening = 0;
    sock->recv_buffer = NULL;
    sock->recv_len = 0;
    sock->send_buffer = NULL;
    sock->send_len = 0;
    
    socket_count++;
    return sock->fd;
}

int socket_bind(int fd, ip_addr_t addr, uint16_t port) {
    if(fd < 0 || fd >= socket_count || !sockets[fd].active) return -1;
    sockets[fd].local.addr = addr;
    sockets[fd].local.port = port;
    sockets[fd].local.family = AF_INET;
    return 0;
}

int socket_listen(int fd, int backlog) {
    if(fd < 0 || fd >= socket_count || !sockets[fd].active) return -1;
    sockets[fd].listening = 1;
    sockets[fd].backlog = backlog;
    sockets[fd].state = TCP_LISTEN;
    return 0;
}

int socket_accept(int fd, sockaddr_in_t *client_addr) {
    if(fd < 0 || fd >= socket_count || !sockets[fd].active) return -1;
    if(!sockets[fd].listening) return -1;
    
    /* Basit: yeni socket oluştur */
    int new_fd = socket_create(sockets[fd].type);
    if(new_fd < 0) return -1;
    
    sockets[new_fd].remote = sockets[fd].remote;
    sockets[new_fd].state = TCP_ESTABLISHED;
    
    if(client_addr) *client_addr = sockets[fd].remote;
    return new_fd;
}

int socket_connect(int fd, ip_addr_t addr, uint16_t port) {
    if(fd < 0 || fd >= socket_count || !sockets[fd].active) return -1;
    sockets[fd].remote.addr = addr;
    sockets[fd].remote.port = port;
    sockets[fd].remote.family = AF_INET;
    sockets[fd].state = TCP_SYN_SENT;
    return 0;
}

int socket_send(int fd, void *data, int len) {
    if(fd < 0 || fd >= socket_count || !sockets[fd].active) return -1;
    socket_t *sock = &sockets[fd];
    
    if(sock->type == SOCK_STREAM) {
        return tcp_send_data(fd, data, len);
    } else if(sock->type == SOCK_DGRAM) {
        return udp_send(sock->remote.addr, sock->local.port, 
                       sock->remote.port, data, len);
    }
    return -1;
}

int socket_recv(int fd, void *buf, int len) {
    if(fd < 0 || fd >= socket_count || !sockets[fd].active) return -1;
    socket_t *sock = &sockets[fd];
    
    if(sock->recv_buffer && sock->recv_len > 0) {
        int copy = len < sock->recv_len ? len : sock->recv_len;
        for(int i=0; i<copy; i++) ((uint8_t*)buf)[i] = sock->recv_buffer[i];
        sock->recv_len -= copy;
        return copy;
    }
    return 0;
}

int socket_close(int fd) {
    if(fd < 0 || fd >= socket_count) return -1;
    sockets[fd].active = 0;
    sockets[fd].state = TCP_CLOSED;
    return 0;
}

/* ============================================================
 * TCP VERİ TRANSFERİ
 * ============================================================ */

int tcp_send_data(int conn_id, void *data, int len) {
    if(conn_id < 0 || conn_id >= tcp_count) return -1;
    tcp_cb_t *cb = &tcp_connections[conn_id];
    
    if(cb->state != TCP_ESTABLISHED) return -1;
    
    /* Veriyi gönder (basitleştirilmiş) */
    uint8_t packet[1500];
    tcp_header_t *tcp = (tcp_header_t *)packet;
    tcp->src_port = cb->local_port;
    tcp->dst_port = cb->remote_port;
    tcp->seq_num = cb->snd_nxt;
    tcp->ack_num = cb->rcv_nxt;
    tcp->flags = TCP_FLAG_PSH | TCP_FLAG_ACK;
    tcp->window = 65535;
    
    int hdr_len = 20;
    for(int i=0; i<len && (i+hdr_len)<1500; i++)
        packet[hdr_len+i] = ((uint8_t*)data)[i];
    
    cb->snd_nxt += len;
    return len;
}

/* ============================================================
 * IP FRAGMENTASYON VE REASSEMBLY
 * ============================================================ */

#define IP_MAX_FRAGMENTS 8

typedef struct {
    uint16_t id;
    uint8_t *data;
    int total_len;
    int received_len;
    int fragment_count;
    uint8_t fragments_received[IP_MAX_FRAGMENTS];
} ip_reassembly_t;

static ip_reassembly_t reassembly_buf;

void ip_reassembly_init(void) {
    reassembly_buf.data = NULL;
    reassembly_buf.total_len = 0;
}

int ip_reassemble(uint16_t id, int offset, int more_fragments, 
                  uint8_t *data, int len, uint8_t **complete_packet) {
    /* Basitleştirilmiş reassembly */
    if(!more_fragments) {
        *complete_packet = data;
        return len;
    }
    return -1; /* Daha fazla fragment bekleniyor */
}

/* ============================================================
 * NAT (Network Address Translation)
 * ============================================================ */

#define NAT_TABLE_SIZE 64

typedef struct {
    ip_addr_t internal_ip;
    uint16_t internal_port;
    ip_addr_t external_ip;
    uint16_t external_port;
    uint8_t protocol;
    int active;
} nat_entry_t;

static nat_entry_t nat_table[NAT_TABLE_SIZE];
static int nat_count = 0;
static ip_addr_t nat_external_ip;
static uint16_t nat_next_port = 1024;

void nat_init(void) {
    nat_count = 0;
    nat_external_ip.bytes[0]=203; nat_external_ip.bytes[1]=0;
    nat_external_ip.bytes[2]=113; nat_external_ip.bytes[3]=1;
    for(int i=0; i<NAT_TABLE_SIZE; i++) nat_table[i].active = 0;
}

int nat_add_mapping(ip_addr_t internal_ip, uint16_t internal_port, 
                    uint8_t protocol) {
    if(nat_count >= NAT_TABLE_SIZE) return -1;
    
    nat_entry_t *entry = &nat_table[nat_count];
    entry->internal_ip = internal_ip;
    entry->internal_port = internal_port;
    entry->external_ip = nat_external_ip;
    entry->external_port = nat_next_port++;
    entry->protocol = protocol;
    entry->active = 1;
    nat_count++;
    return entry->external_port;
}

int nat_lookup_external(uint16_t external_port, ip_addr_t *internal_ip, 
                         uint16_t *internal_port) {
    for(int i=0; i<nat_count; i++) {
        if(nat_table[i].active && nat_table[i].external_port == external_port) {
            *internal_ip = nat_table[i].internal_ip;
            *internal_port = nat_table[i].internal_port;
            return 1;
        }
    }
    return 0;
}

/* ============================================================
 * VLAN (802.1Q)
 * ============================================================ */

typedef struct {
    uint16_t tpid;
    uint16_t tci;
    uint16_t ethertype;
} __attribute__((packed)) vlan_header_t;

#define VLAN_TPID 0x8100
#define VLAN_MAX 16

typedef struct {
    int vlan_id;
    char name[16];
    int enabled;
} vlan_interface_t;

static vlan_interface_t vlan_ifaces[VLAN_MAX];
static int vlan_count = 0;

int vlan_create(int vlan_id, const char *name) {
    if(vlan_count >= VLAN_MAX) return -1;
    vlan_interface_t *vlan = &vlan_ifaces[vlan_count];
    vlan->vlan_id = vlan_id;
    for(int i=0; i<15 && name[i]; i++) vlan->name[i] = name[i];
    vlan->name[15] = '\0';
    vlan->enabled = 1;
    vlan_count++;
    return vlan_count - 1;
}

/* ============================================================
 * QOS (Quality of Service)
 * ============================================================ */

typedef enum {
    QOS_BEST_EFFORT = 0,
    QOS_PRIORITY = 1,
    QOS_VIDEO = 2,
    QOS_VOICE = 3,
    QOS_CONTROL = 4
} qos_level_t;

typedef struct {
    qos_level_t level;
    int bandwidth_limit;  /* KB/s */
    int priority;
    int active;
} qos_rule_t;

static qos_rule_t qos_rules[16];
static int qos_count = 0;

void qos_init(void) { qos_count = 0; }

int qos_add_rule(qos_level_t level, int bandwidth, int priority) {
    if(qos_count >= 16) return -1;
    qos_rules[qos_count].level = level;
    qos_rules[qos_count].bandwidth_limit = bandwidth;
    qos_rules[qos_count].priority = priority;
    qos_rules[qos_count].active = 1;
    qos_count++;
    return 0;
}

/* ============================================================
 * TRAFİK ŞEKİLLENDİRME
 * ============================================================ */

typedef struct {
    uint64_t bucket_size;
    uint64_t tokens;
    uint64_t rate;       /* Token/saniye */
    uint64_t last_refill;
} token_bucket_t;

static token_bucket_t traffic_shaper;

void shaper_init(int rate_kbps, int burst_kb) {
    traffic_shaper.rate = rate_kbps * 1024 / 8;
    traffic_shaper.bucket_size = burst_kb * 1024;
    traffic_shaper.tokens = traffic_shaper.bucket_size;
    traffic_shaper.last_refill = 0;
}

int shaper_can_send(int bytes) {
    /* Token bucket algoritması */
    uint64_t now = 0; /* get_time() */
    uint64_t elapsed = now - traffic_shaper.last_refill;
    traffic_shaper.tokens += elapsed * traffic_shaper.rate;
    if(traffic_shaper.tokens > traffic_shaper.bucket_size)
        traffic_shaper.tokens = traffic_shaper.bucket_size;
    traffic_shaper.last_refill = now;
    
    if((uint64_t)bytes <= traffic_shaper.tokens) {
        traffic_shaper.tokens -= bytes;
        return 1;
    }
    return 0;
}

/* ============================================================
 * VPN (Virtual Private Network) - Basit Tünel
 * ============================================================ */

typedef struct {
    ip_addr_t server_ip;
    uint16_t server_port;
    uint8_t encryption_key[32];
    int connected;
} vpn_tunnel_t;

static vpn_tunnel_t vpn;

void vpn_init(void) {
    vpn.connected = 0;
}

int vpn_connect(ip_addr_t server, uint16_t port, const char *key) {
    vpn.server_ip = server;
    vpn.server_port = port;
    for(int i=0; i<31 && key[i]; i++) vpn.encryption_key[i] = key[i];
    vpn.encryption_key[31] = '\0';
    vpn.connected = 1;
    return 0;
}

void vpn_disconnect(void) {
    vpn.connected = 0;
}

int vpn_tunnel_send(void *data, int len) {
    if(!vpn.connected) return -1;
    /* Veriyi şifrele ve gönder */
    for(int i=0; i<len; i++)
        ((uint8_t*)data)[i] ^= vpn.encryption_key[i % 32];
    return len;
}

int vpn_tunnel_recv(void *data, int len) {
    /* Şifreli veriyi çöz */
    for(int i=0; i<len; i++)
        ((uint8_t*)data)[i] ^= vpn.encryption_key[i % 32];
    return len;
}

/* ============================================================
 * AĞ İZLEME (Packet Sniffer)
 * ============================================================ */

#define SNIFFER_BUFFER_SIZE 65536
#define SNIFFER_MAX_PACKETS 256

typedef struct {
    uint64_t timestamp;
    int length;
    uint8_t *data;
} sniffed_packet_t;

static sniffed_packet_t sniffer_packets[SNIFFER_MAX_PACKETS];
static int sniffer_count = 0;
static int sniffer_enabled = 0;

void sniffer_start(void) { sniffer_enabled = 1; sniffer_count = 0; }
void sniffer_stop(void) { sniffer_enabled = 0; }

void sniffer_capture(uint8_t *packet, int len) {
    if(!sniffer_enabled || sniffer_count >= SNIFFER_MAX_PACKETS) return;
    sniffer_packets[sniffer_count].timestamp = 0;
    sniffer_packets[sniffer_count].length = len;
    sniffer_packets[sniffer_count].data = packet;
    sniffer_count++;
}

/* ============================================================
 * PORT TARAMA (Basit)
 * ============================================================ */

typedef struct {
    uint16_t port;
    int open;
    char service[32];
} port_scan_result_t;

static port_scan_result_t port_scan_results[64];
static int port_scan_count = 0;

void port_scan(ip_addr_t target, uint16_t start_port, uint16_t end_port) {
    port_scan_count = 0;
    for(uint16_t port = start_port; port <= end_port && port_scan_count < 64; port++) {
        /* Basit: bilinen portlar */
        if(port == 22 || port == 80 || port == 443 || port == 8080) {
            port_scan_results[port_scan_count].port = port;
            port_scan_results[port_scan_count].open = 1;
            if(port == 22) { port_scan_results[port_scan_count].service[0]='S'; 
                port_scan_results[port_scan_count].service[1]='S'; 
                port_scan_results[port_scan_count].service[2]='H'; 
                port_scan_results[port_scan_count].service[3]='\0'; }
            if(port == 80) { port_scan_results[port_scan_count].service[0]='H';
                port_scan_results[port_scan_count].service[1]='T';
                port_scan_results[port_scan_count].service[2]='T';
                port_scan_results[port_scan_count].service[3]='P';
                port_scan_results[port_scan_count].service[4]='\0'; }
            port_scan_count++;
        }
    }
}

/* ============================================================
 * NETLINK / ROUTE SOKET
 * ============================================================ */

typedef struct {
    ip_addr_t destination;
    ip_addr_t gateway;
    ip_addr_t netmask;
    int metric;
    int interface;
    int active;
} route_entry_t;

#define ROUTE_TABLE_SIZE 64
static route_entry_t route_table[ROUTE_TABLE_SIZE];
static int route_count = 0;

void route_init(void) {
    route_count = 0;
    /* Varsayılan rota */
    route_entry_t *def = &route_table[route_count++];
    def->destination.bytes[0]=0; def->destination.bytes[1]=0;
    def->destination.bytes[2]=0; def->destination.bytes[3]=0;
    def->gateway = net_iface.gateway;
    def->netmask.bytes[0]=0; def->netmask.bytes[1]=0;
    def->netmask.bytes[2]=0; def->netmask.bytes[3]=0;
    def->metric = 1;
    def->active = 1;
}

int route_add(ip_addr_t dest, ip_addr_t gw, ip_addr_t mask, int metric) {
    if(route_count >= ROUTE_TABLE_SIZE) return -1;
    route_table[route_count].destination = dest;
    route_table[route_count].gateway = gw;
    route_table[route_count].netmask = mask;
    route_table[route_count].metric = metric;
    route_table[route_count].active = 1;
    route_count++;
    return 0;
}

ip_addr_t *route_lookup(ip_addr_t dest) {
    for(int i=0; i<route_count; i++) {
        if(route_table[i].active) {
            int match = 1;
            for(int j=0; j<4; j++) {
                if((dest.bytes[j] & route_table[i].netmask.bytes[j]) != 
                   (route_table[i].destination.bytes[j] & route_table[i].netmask.bytes[j])) {
                    match = 0; break;
                }
            }
            if(match) return &route_table[i].gateway;
        }
    }
    return &net_iface.gateway;
}
