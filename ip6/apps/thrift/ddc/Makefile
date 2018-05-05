GLIB_DIR = gen-c_glib
SHARED_GLIB = $(GLIB_DIR)/shared_types.c

MEM_SERVER = remote_mem_test
MEM_SERVER_GLIB =  $(GLIB_DIR)/$(MEM_SERVER).c $(GLIB_DIR)/$(MEM_SERVER)_types.c

ARR_SERVER = simple_arr_comp
ARR_SERVER_GLIB = $(GLIB_DIR)/$(ARR_SERVER).c $(GLIB_DIR)/$(ARR_SERVER)_types.c

CLIENT = client
CLIENT_DEPS = thrift_client.c $(MEM_SERVER_GLIB) $(ARR_SERVER_GLIB)

all: $(SHARED_GLIB) $(MEM_SERVER) $(ARR_SERVER) $(CLIENT)

$(SHARED_GLIB):
	@$(GEN_TEMPLATE) shared.thrift

gen-c_glib/remote_mem_test.c:
	@$(GEN_TEMPLATE) $(MEM_SERVER).thrift

gen-c_glib/simple_arr_comp.c:
	@$(GEN_TEMPLATE) $(ARR_SERVER).thrift

simple_arr_comp: $(ARR_SERVER)_server.c $(ARR_SERVER_GLIB)
	@echo "Building $(ARR_SERVER)"
	@$(CC) -o $@ $(ARR_SERVER)_server.c $(ARR_SERVER_GLIB) $(SHARED_GLIB) $(CFLAGS) $(LIBS)

remote_mem_test: $(MEM_SERVER)_server.c $(MEM_SERVER_GLIB)
	@echo "Building $(MEM_SERVER)"
	@$(CC) -o $@ $(MEM_SERVER)_server.c $(MEM_SERVER_GLIB) $(SHARED_GLIB) $(CFLAGS) $(LIBS)

client: $(CLIENT_DEPS)
	@echo "Building client"
	@$(CC) -o $@ $(CLIENT_DEPS) $(SHARED_GLIB) $(CFLAGS) $(LIBS)

clean:
	@$(RM) -r $(GLIB_DIR)
	@$(RM) $(ARR_SERVER) $(MEM_SERVER) $(CLIENT)