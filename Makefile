EXES_SERVER = TableCache

FPNN_DIR = ../fpnn
DEPLOYMENT_DIR = ../deployment/tableCache

CFLAGS +=
CXXFLAGS +=
CPPFLAGS += -I$(FPNN_DIR)/extends -I$(FPNN_DIR)/core -I$(FPNN_DIR)/proto -I$(FPNN_DIR)/base -I$(FPNN_DIR)/proto/msgpack -I$(FPNN_DIR)/proto/rapidjson
LIBS += -L$(FPNN_DIR)/extends -L$(FPNN_DIR)/core -L$(FPNN_DIR)/proto -L$(FPNN_DIR)/base -lextends -lfpnn

OBJS_SERVER = TableCache.o TableCacheProcessor.o ClusterNotifier.o

all: $(EXES_SERVER)

deploy:
	-mkdir -p $(DEPLOYMENT_DIR)/bin/
	-mkdir -p $(DEPLOYMENT_DIR)/conf/
	cp -rf $(EXES_SERVER) $(DEPLOYMENT_DIR)/bin/
	cp -rf tableCache.conf $(DEPLOYMENT_DIR)/conf/
	make deploy -C tools

clean:
	$(RM) *.o $(EXES_SERVER)
include $(FPNN_DIR)/def.mk