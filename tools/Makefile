EXES_FETCH = Fetch
EXES_INVALIDATE = invalidate
EXES_MODIFY = Modify

FPNN_DIR = ../../fpnn
DEPLOYMENT_DIR = ../../deployment/tableCache
CFLAGS +=
CXXFLAGS +=
CPPFLAGS += -I$(FPNN_DIR)/core -I$(FPNN_DIR)/proto -I$(FPNN_DIR)/base -I$(FPNN_DIR)/proto/msgpack -I$(FPNN_DIR)/proto/rapidjson
LIBS += -L$(FPNN_DIR)/extends -L$(FPNN_DIR)/core -L$(FPNN_DIR)/proto -L$(FPNN_DIR)/base -lfpnn

OBJS_FETCH = Fetch.o
OBJS_INVALIDATE = invalidate.o
OBJS_MODIFY = Modify.o

all: $(EXES_FETCH) $(EXES_INVALIDATE) $(EXES_MODIFY)

deploy:
	-mkdir -p $(DEPLOYMENT_DIR)/tools/
	cp -rf $(EXES_FETCH) $(DEPLOYMENT_DIR)/tools/
	cp -rf $(EXES_INVALIDATE) $(DEPLOYMENT_DIR)/tools/
	cp -rf $(EXES_MODIFY) $(DEPLOYMENT_DIR)/tools/

clean:
	$(RM) *.o $(EXES_FETCH) $(EXES_INVALIDATE) $(EXES_MODIFY)
include $(FPNN_DIR)/def.mk
