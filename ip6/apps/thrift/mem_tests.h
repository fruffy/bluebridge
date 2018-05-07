#ifndef THRIFT_MEM_TESTS
#define THRIFT_MEM_TESTS
void microbenchmark_perf(RemoteMemTestIf *client, int iterations);
void test_server_functionality(RemoteMemTestIf *client);

#endif