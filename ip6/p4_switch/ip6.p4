/* Copyright 2013-present Barefoot Networks, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

header_type ethernet_t {
    fields {
        dstAddr : 48;
        srcAddr : 48;
        etherType : 16;
    }
}

header_type ipv4_t {
    fields {
        version : 4;
        ihl : 4;
        diffserv : 8;
        totalLen : 16;
        identification : 16;
        flags : 3;
        fragOffset : 13;
        ttl : 8;
        protocol : 8;
        hdrChecksum : 16;
        srcAddr : 32;
        dstAddr: 32;
    }
}

header_type ipv6_t {
    fields {
        version : 4;
        trafficClass : 8;
        flowLabel : 20;
        payloadLen : 16;
        nextHdr : 8;
        hopLimit : 8;
        srcAddr : 128;
        dstAddr : 128;
    }
}

header_type tcp_t {
    fields {
        srcPort : 16;
        dstPort : 16;
        seqNo : 32;
        ackNo : 32;
        dataOffset : 4;
        res : 4;
        flags : 8;
        window : 16;
        checksum : 16;
        urgentPtr : 16;
    }
}

header_type udp_t {
    fields {
        srcPort : 16;
        dstPort : 16;
        length_ : 16;
        checksum : 16;
    }
}

header_type arp_t {
    fields {
        hType : 16;
        pType : 16;
        hSize : 8;
        pSize : 8;
        opcode : 16;
        sMac : 48;
        sIP : 32;
        dMac : 48;
        dIP : 32;
    }
}

header_type thrift_t {
    fields {
        version: 16;
        ttype: 16;
        len : 32;
        name : 128;
        seq : 32;
    }
}

header_type intrinsic_metadata_t {
    fields {
        ingress_global_timestamp : 48;  // ingress timestamp, in microseconds
        mcast_grp : 4;  // to be set in the ingress pipeline for multicast
        egress_rid : 4;  // replication id, available on egress if packet was
                            // multicast
        mcast_hash : 16;  // unused
        lf_field_list : 32;  // set by generate_digest primitive, not to be used
                                // directly by P4 programmer
        resubmit_flag : 16;  // used internally
        recirculate_flag : 16;  // used internally
    }
}

parser start {
    return parse_ethernet;
}

#define ETHERTYPE_IPV4         0x0800
#define ETHERTYPE_IPV6         0x86dd
#define ETHERTYPE_ARP          0x0806
#define IP_PROTOCOLS_ICMP      1
#define IP_PROTOCOLS_IGMP      2
#define IP_PROTOCOLS_IPV4      4
#define IP_PROTOCOLS_TCP       6
#define IP_PROTOCOLS_UDP       17
#define IP_PROTOCOLS_IPV6      41


header ethernet_t ethernet;
header ipv4_t ipv4;
header ipv6_t ipv6;
header arp_t arp;
header thrift_t thrift;
header udp_t udp;
header tcp_t tcp;
metadata intrinsic_metadata_t intrinsic_metadata;


parser parse_arp {
    extract(arp);
    return ingress;
}

parser parse_ethernet {
    extract(ethernet);
    return select(latest.etherType) {
        ETHERTYPE_IPV4 : parse_ipv4;
        ETHERTYPE_IPV6 : parse_ipv6;
        ETHERTYPE_ARP : parse_arp;
        default: ingress;
    }
}

parser parse_ipv4 {
    extract(ipv4);
    return ingress;
}

parser parse_ipv6 {
    extract(ipv6);
    return select(latest.nextHdr) {
        IP_PROTOCOLS_TCP : parse_tcp;
        IP_PROTOCOLS_UDP : parse_udp;
        default: ingress;
    }
}

parser parse_tcp {
    extract(tcp);
    return select(latest.dstPort) {
        default: ingress;
    }
}

parser parse_udp {
    extract(udp);
    //set_metadata(l3_metadata.lkp_outer_l4_sport, latest.srcPort);
    //set_metadata(l3_metadata.lkp_outer_l4_dport, latest.dstPort);
    //return select(latest.dstPort) {
    //    9080 : parse_thrift;
    return parse_thrift;
    //}
}

parser parse_thrift {
    extract(thrift);
    return ingress;
}


action _drop() {
    drop();
}

header_type routing_metadata_t {
    fields {
        nhop_ipv4 : 32;
        nhop_ipv6 : 128;
    }
}

metadata routing_metadata_t routing_metadata;

action set_nhop4(nhop_ipv4, port) {
    modify_field(routing_metadata.nhop_ipv4, nhop_ipv4);
    modify_field(standard_metadata.egress_spec, port);
    modify_field(ipv4.ttl, ipv4.ttl - 1);
}

action set_nhop6(nhop_ipv6, port) {
    modify_field(routing_metadata.nhop_ipv6, nhop_ipv6);
    modify_field(standard_metadata.egress_spec, port);
    modify_field(ipv6.hopLimit, ipv6.hopLimit - 1);
}

action thrift_forward(port) {
    modify_field(standard_metadata.egress_spec, port);
    modify_field(ipv6.hopLimit, ipv6.hopLimit - 1);
}

action set_dmac(dmac) {
    modify_field(ethernet.dstAddr, dmac);
}

action rewrite_mac(smac) {
    modify_field(ethernet.srcAddr, smac);
}

action broadcast() {
    modify_field(intrinsic_metadata.mcast_grp, 1);
}

table arp {
    reads {
        ethernet.dstAddr : exact;
    }
    actions {broadcast;_drop;}
    size : 512;
}

table forward {
    reads {
        routing_metadata.nhop_ipv4 : exact;
        routing_metadata.nhop_ipv6 : exact;
    }
    actions {
        set_dmac;
        _drop;
    }
    size: 512;
}

table send_frame {
    reads {
        standard_metadata.egress_port: exact;
    }
    actions {
        rewrite_mac;
        _drop;
    }
    size: 256;
}

table ipv4_lpm {
    reads {
        ipv4.dstAddr : lpm;
    }
    actions {
        set_nhop4;
        _drop;
    }
    size: 1024;
}

table ipv6_lpm {
    reads {
        ipv6.dstAddr : lpm;
    }
    actions {
        set_nhop6;
        _drop;
    }
    size: 1024;
}

table thrift {
    reads {
        thrift.name : exact;
    }
    actions {
        thrift_forward;
        _drop;
    }
    size: 1024;
}


control ingress {
    if(valid(ipv4) and ipv4.ttl > 0) {
        apply(ipv4_lpm);
        apply(forward);
    }
    else if(valid(ipv6) and ipv6.hopLimit > 0) {
        if (ipv6.nextHdr == 0x3A) {
            apply(arp);
        } else {
            if (thrift.ttype == 1 and thrift.version == 0x8001) {
                apply(thrift);
            }
            else {
                apply(ipv6_lpm);
            }
        }
    } else if (valid(arp)) {
        apply(arp);
    }
}

control egress {
    apply(send_frame);
}


