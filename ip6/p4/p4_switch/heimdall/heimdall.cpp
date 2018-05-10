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

/*
 * Antonin Bas (antonin@barefootnetworks.com)
 *
 */

#include <bm/bm_apps/learn.h>
#include <bm/Standard.h>
#include <bm/SimplePre.h>

#include <iostream>
#include <string>
#include <cstdint>
#include <cstring>
 #include <vector>
#include <cassert>
#include <unordered_map>
namespace {

bm_apps::LearnListener *listener;

struct sample_t {
  char bb_subnet[2];
  char bb_cmd[2];
  char bb_ptr[8];
  uint16_t type;
} __attribute__((packed));

}  // namespace
std::unordered_map<std::string, long> entry_map;

namespace runtime = bm_runtime::standard;


namespace {

/*
 * Print reverse byte buffer including specified length
 */
int print_n_bytes(void *receiveBuffer, int num) {
  int i;

  // for (i = num-1; i>=0; i--) {
  //  printf("%02x", (unsigned char) receiveBuffer[i]);
  // }
  for (i =0; i<=num-1; i++) {
    printf("%02x", ((unsigned char *)receiveBuffer)[i]);
  }
  printf("\n");
  return i;
}


char key[10];
void learn_cb(const bm_apps::LearnListener::MsgInfo &msg_info,
              const char *data, void *cookie) {
  (void) cookie;
  //std::cout << "CB with " << msg_info.num_samples << " samples\n";

  boost::shared_ptr<runtime::StandardClient> client = listener->get_client();
  assert(client);
  for (unsigned int i = 0; i < msg_info.num_samples; i++) {
    const sample_t *sample = ((const sample_t *) data) + i;
    auto add_entry = [client](
        const std::string &t_name, const runtime::BmMatchParams &match_params,
        const std::string &a_name, const runtime::BmActionData &action_data) {
      runtime::BmAddEntryOptions options;
      try {
        long handle = client->bm_mt_add_entry(0, t_name, match_params, a_name, action_data,
                                options);
        return handle;
      } catch (runtime::InvalidTableOperation &ito) {
        auto what = runtime::_TableOperationErrorCode_VALUES_TO_NAMES.find(
            ito.code)->second;
        std::cout << "Insert: Invalid table (" << t_name << ") operation ("
                  << ito.code << "): " << what << std::endl;
      }
      return -1l;
    };
    auto del_entry = [client](
        const std::string &t_name, const runtime::BmEntryHandle entry_handle) {
      runtime::BmAddEntryOptions options;
      try {
        client->bm_mt_delete_entry(0, t_name, entry_handle);
      } catch (runtime::InvalidTableOperation &ito) {
        auto what = runtime::_TableOperationErrorCode_VALUES_TO_NAMES.find(
            ito.code)->second;
        std::cout << "Delete: Invalid table (" << t_name << ") operation ("
                  << ito.code << "): " << what << std::endl;
      }
    };

    /* Src Address */
    /* runtime::BmMatchParam match_src;
    match_src.type = runtime::BmMatchParamType::type::EXACT;
    runtime::BmMatchParamExact match_src_type;
    match_src_type.key =
      std::string(sample->bb_src, sizeof(sample->bb_src));
    match_src.__set_exact(match_src_type);*/
    /* Subnet */
    runtime::BmMatchParam match_subnet;
    match_subnet.type = runtime::BmMatchParamType::type::EXACT;
    runtime::BmMatchParamExact match_subnet_type;
    match_subnet_type.key =
      std::string(sample->bb_subnet, sizeof(sample->bb_subnet));
    match_subnet.__set_exact(match_subnet_type);
    /* Cmd */
    runtime::BmMatchParam match_cmd;
    match_cmd.type = runtime::BmMatchParamType::type::EXACT;
    //match_cmd.type = runtime::BmMatchParamType::type::LPM;
    runtime::BmMatchParamExact match_cmd_type;
    //runtime::BmMatchParamLPM match_cmd_type;
    match_cmd_type.key =
      std::string(sample->bb_cmd, sizeof(sample->bb_cmd));
    match_cmd.__set_exact(match_cmd_type);
    //match_cmd.__set_lpm(match_cmd_type);
    /* Pointer */
    runtime::BmMatchParam match_ptr;
    //match_ptr.type = runtime::BmMatchParamType::type::EXACT;
    match_ptr.type = runtime::BmMatchParamType::type::LPM;
    //runtime::BmMatchParamExact match_ptr_type;
    runtime::BmMatchParamLPM match_ptr_type;
    match_ptr_type.key =
      std::string(sample->bb_ptr, sizeof(sample->bb_ptr));
    match_ptr_type.prefix_length = 64;
    match_ptr.__set_lpm(match_ptr_type);
    //match_ptr.__set_exact(match_ptr_type);
    runtime::BmMatchParams match_params({match_subnet, match_ptr});
    //std::string key(sample->bb_subnet);
    std::memcpy(key,sample->bb_subnet,2 );
    std::memcpy(&key[2],sample->bb_ptr,8);
    //key.append(sample->bb_cmd);
    //key.append(sample->bb_ptr);
    uint16_t cmd =*(uint16_t *)&sample->bb_cmd;

    // print_n_bytes((void*)&sample->type, 2);
    // print_n_bytes((void*)&sample->bb_cmd, 2);
    // print_n_bytes((void*)key, 10);
    // for(auto elem : entry_map)
    //    std::cout << elem.second << " ";
    // printf("\n");
    //printf("CMD %d\n", cmd );
    if (ntohs(sample->type) == 2 || cmd == 4) {
      runtime::BmEntryHandle bm_handle = entry_map[key];
      if (bm_handle != 0) {
        del_entry("bb_acl", bm_handle);//,printf("DELETING ENTRY %d \n", bm_handle);
        entry_map.erase(key);
      }
      // else {
      //   printf("DELETE NOOP\n");
      //}
    } else {
        long bm_handle = add_entry("bb_acl", match_params, "_nop", std::vector<std::string>());
        // printf("ADDING ENTRY %lu\n", bm_handle);
        if (bm_handle != -1)
          entry_map[key] = bm_handle;
        // else
        //   printf("ADD NOOP\n");
    }
    //printf("\n");
    // std::vector<std::string> action_data =
    //   {std::string(reinterpret_cast<const char *>(&sample->ingress_port), 2)};
  }

  client->bm_learning_ack_buffer(0, msg_info.list_id, msg_info.buffer_id);
}

}  // namespace

int main() {
  listener = new bm_apps::LearnListener("ipc:///tmp/bmv2-0-notifications.ipc");
  listener->register_cb(learn_cb, nullptr);
  listener->start();

  while (true) std::this_thread::sleep_for(std::chrono::milliseconds(100));

  return 0;
}
