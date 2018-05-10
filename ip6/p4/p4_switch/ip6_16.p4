#include <core.p4>
#include <v1model.p4>

struct routing_metadata_t {
    bit<32>  nhop_ipv4;
    bit<128> nhop_ipv6;
}

header arp_t {
    bit<16> hType;
    bit<16> pType;
    bit<8>  hSize;
    bit<8>  pSize;
    bit<16> opcode;
    bit<48> sMac;
    bit<32> sIP;
    bit<48> dMac;
    bit<32> dIP;
}

header icmp6_t {
    bit<8>  ipType;
    bit<8>  code;
    bit<16> checksum;
}


header bb_t {
    bit<4>  version;
    bit<8>  trafficClass;
    bit<20> flowLabel;
    bit<16> payloadLen;
    bit<8>  nextHdr;
    bit<8>  hopLimit;
    bit<32> s_wildcard;
    bit<16> s_subnet;
    bit<16> s_cmd;
    bit<64> s_bb_pointer;
    bit<32> d_wildcard;
    bit<16> d_subnet;
    bit<16> d_cmd;
    bit<64> d_bb_pointer;
}

header bb_alloc_t {
    bit<16> srcPort;
    bit<16> dstPort;
    bit<16> length_;
    bit<16> checksum;
}

header ethernet_t {
    bit<48> dstAddr;
    bit<48> srcAddr;
    bit<16> etherType;
}

header ipv4_t {
    bit<4>  version;
    bit<4>  ihl;
    bit<8>  diffserv;
    bit<16> totalLen;
    bit<16> identification;
    bit<3>  flags;
    bit<13> fragOffset;
    bit<8>  ttl;
    bit<8>  protocol;
    bit<16> hdrChecksum;
    bit<32> srcAddr;
    bit<32> dstAddr;
}

header ipv6_t {
    bit<4>   version;
    bit<8>   trafficClass;
    bit<20>  flowLabel;
    bit<16>  payloadLen;
    bit<8>   nextHdr;
    bit<8>   hopLimit;
    bit<128> srcAddr;
    bit<128> dstAddr;
}

header tcp_t {
    bit<16> srcPort;
    bit<16> dstPort;
    bit<32> seqNo;
    bit<32> ackNo;
    bit<4>  dataOffset;
    bit<4>  res;
    bit<8>  flags;
    bit<16> window;
    bit<16> checksum;
    bit<16> urgentPtr;
}

header thrift_t {
    bit<16>  version;
    bit<16>  ttype;
    bit<32>  len;
    bit<128> name;
    bit<32>  seq;
}

header thrift_ip_t {
    bit<56> crap;
    bit<32> wildcard;
    bit<16> subnet;
    bit<16> cmd;
    bit<64> bb_pointer;
}

header udp_t {
    bit<16> srcPort;
    bit<16> dstPort;
    bit<16> length_;
    bit<16> checksum;
}

struct metadata {
}

struct headers {
    @name(".arp") 
    arp_t       arp;
    @name(".icmp6") 
    icmp6_t       icmp6;
    @name(".bb") 
    bb_t        bb;
    @name(".bb_alloc") 
    bb_alloc_t  bb_alloc;
    @name(".ethernet") 
    ethernet_t  ethernet;
    @name(".ipv4") 
    ipv4_t      ipv4;
    @name(".ipv6") 
    ipv6_t      ipv6;
    @name(".tcp") 
    tcp_t       tcp;
    @name(".thrift") 
    thrift_t    thrift;
    @name(".thrift_ip") 
    thrift_ip_t thrift_ip;
    @name(".udp") 
    udp_t       udp;
}

parser MyParser(packet_in packet, out headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {
    @name(".parse_arp") state parse_arp {
        packet.extract(hdr.arp);
        transition accept;
    }
    @name(".parse_ndp") state parse_ndp {
        packet.extract(hdr.icmp6);
        transition accept;
    }
    @name(".parse_bb") state parse_bb {
        packet.extract(hdr.bb);
        transition select(hdr.bb.d_cmd) {
            0x100: parse_bb_alloc;
            0x400: parse_bb_alloc;
            0x600: parse_bb_alloc;
            default: accept;
        }
    }
    @name(".parse_bb_alloc") state parse_bb_alloc {
        packet.extract(hdr.bb_alloc);
        transition accept;
    }
    @name(".parse_ethernet") state parse_ethernet {
        packet.extract(hdr.ethernet);
        transition select(hdr.ethernet.etherType) {
            0x806: parse_arp;
            0x800: parse_ipv4;
            0x86dd: parse_ipv6;
            0xffff: parse_bb;
            default: accept;
        }
    }
    @name(".parse_ipv4") state parse_ipv4 {
        packet.extract(hdr.ipv4);
        transition select(hdr.ipv4.protocol) {
            8w6: parse_tcp;
            8w17: parse_udp;
            default: accept;
        }
    }
    @name(".parse_ipv6") state parse_ipv6 {
        packet.extract(hdr.ipv6);
        transition select(hdr.ipv6.nextHdr) {
            8w6: parse_tcp;
            8w17: parse_udp;
            0x3a: parse_ndp;
            default: accept;
        }
    }
    @name(".parse_tcp") state parse_tcp {
        packet.extract(hdr.tcp);
        transition select(hdr.tcp.dstPort) {
            default: accept;
        }
    }
    @name(".parse_thrift") state parse_thrift {
        packet.extract(hdr.thrift);
        transition select(hdr.thrift.version) {
            16w0x8001: parse_thrift_ip;
            default: accept;
        }
    }
    @name(".parse_thrift_ip") state parse_thrift_ip {
        packet.extract(hdr.thrift_ip);
        transition accept;
    }
    @name(".parse_udp") state parse_udp {
        packet.extract(hdr.udp);
        transition parse_thrift;
    }
    @name(".start") state start {
        transition parse_ethernet;
    }
}

control MyEgress(inout headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {
    apply {}
}

@name("bb_digest") struct bb_digest {
    bit<16> s_subnet;
    bit<16> d_cmd;
    bit<64> d_bb_pointer;
    bit<16> etherType;
}

@name("thrift_digest") struct thrift_digest {
    bit<16> subnet;
    bit<16> cmd;
    bit<64> bb_pointer;
    bit<16> ttype;
}

control MyIngress(inout headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {
    @name(".update_access_bb") action update_access_bb() {
        digest<bb_digest>((bit<32>)1, { hdr.bb.s_subnet, hdr.bb.d_cmd, hdr.bb.d_bb_pointer, hdr.ethernet.etherType });
    }
    @name("._drop") action _drop() {
        mark_to_drop();
    }
    @name(".broadcast") action broadcast() {
        standard_metadata.mcast_grp = 16w1;
    }
    @name("._nop") action _nop() {
    }
    @name(".set_bb") action set_bb(bit<9> port) {
        standard_metadata.egress_spec = port;
    }
    @name(".set_dmac") action set_dmac(bit<48> dmac) {
        hdr.ethernet.dstAddr = dmac;
    }
    @name(".fwd_ip4") action fwd_ip4(bit<9> port) {
        standard_metadata.egress_spec = port;
    }
    @name(".fwd_ip6") action fwd_ip6(bit<9> port) {
        standard_metadata.egress_spec = port;
    }
    @name(".update_access_thrift") action update_access_thrift() {
        digest<thrift_digest>((bit<32>)1, { hdr.thrift_ip.subnet, hdr.thrift_ip.cmd, hdr.thrift_ip.bb_pointer, hdr.thrift.ttype });
    }
    @name(".thrift_forward") action thrift_forward(bit<9> port) {
        standard_metadata.egress_spec = port;
        hdr.ipv6.hopLimit = hdr.ipv6.hopLimit - 8w1;
    }
    @name(".arp") table arp {
        actions = {
            broadcast;
            _drop;
        }
        default_action = broadcast();
    }
    @name(".ndp") table ndp {
        actions = {
            broadcast;
            _drop;
        }
        default_action = broadcast();
    }
    @name(".alloc_access") table alloc_access {
        actions = {
            update_access_bb;
            _drop;
        }
        default_action = update_access_bb();
    }
    @name(".rpc_access") table rpc_access {
        actions = {
            update_access_thrift;
            _drop;
        }
        default_action = update_access_thrift();
    }
    @name(".ipv4") table ipv4 {
        actions = {
            fwd_ip4;
            _drop;
        }
        key = {
            hdr.ipv4.dstAddr: lpm;
        }
        size = 1024;
    }
    @name(".ipv6") table ipv6 {
        actions = {
            fwd_ip6;
            _drop;
        }
        key = {
            hdr.ipv6.dstAddr: lpm;
        }
        size = 1024;
    }
    @name(".bb_acl") table bb_acl {
        actions = {
            _nop;
            _drop;
        }
        key = {
            hdr.bb.d_subnet    : exact;
            hdr.bb.d_bb_pointer: lpm;
        }
        default_action = _drop();
        size = 1024;
    }
    @name(".bb_fwd") table bb_fwd {
        actions = {
            set_bb;
            _drop;
        }
        key = {
            hdr.bb.d_subnet: exact;
        }
        size = 1024;
    }
    @name(".thrift") table thrift {
        actions = {
            thrift_forward;
            _drop;
        }
        key = {
            hdr.thrift.name: exact;
        }
        size = 1024;
    }
    apply {
        if (hdr.arp.isValid()) {
            arp.apply();
        }
        else if (hdr.thrift.isValid() && hdr.thrift.version == 0x8001) {
            rpc_access.apply();
            if (hdr.thrift.ttype == 16w0x1) {
                thrift.apply();
            }
            else {
                ipv6.apply();
            }
        }
        else if (hdr.ipv4.isValid()) {
                ipv4.apply();
        }
        else if (hdr.ipv6.isValid()) {
            if (hdr.icmp6.isValid()) {
                ndp.apply();
            } else {
                ipv6.apply();
            }
        }
        else if (hdr.bb.isValid()) {
            bb_fwd.apply();
            if (hdr.bb_alloc.isValid() && hdr.bb_alloc.srcPort == 0x1388) {
                alloc_access.apply();
            }
            else {
                bb_acl.apply();
            }
        }
    }
}

control MyDeparser(packet_out packet, in headers hdr) {
    apply {
        packet.emit(hdr.ethernet);
        packet.emit(hdr.arp);
        packet.emit(hdr.ipv6);
        packet.emit(hdr.icmp6);
        packet.emit(hdr.ipv4);
        packet.emit(hdr.bb);
        packet.emit(hdr.bb_alloc);
        packet.emit(hdr.udp);
        packet.emit(hdr.thrift);
        packet.emit(hdr.thrift_ip);
        packet.emit(hdr.tcp);
    }
}

control verifyChecksum(inout headers hdr, inout metadata meta) {
    apply {
    }
}

control computeChecksum(inout headers hdr, inout metadata meta) {
    apply {
    }
}

V1Switch(MyParser(), verifyChecksum(), MyIngress(), MyEgress(), computeChecksum(), MyDeparser()) main;

