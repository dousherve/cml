CC := clang
CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Wno-nullability-extension
CPPFLAGS := -Iinclude -D_POSIX_C_SOURCE=200809L
LDFLAGS :=

PKG_CFLAGS := $(shell pkg-config --cflags libcurl libzip 2>/dev/null)
PKG_LIBS := $(shell pkg-config --libs libcurl libzip 2>/dev/null)

ifeq ($(strip $(PKG_LIBS)),)
  LDLIBS := -lcurl -lzip
else
  CPPFLAGS += $(PKG_CFLAGS)
  LDLIBS := $(PKG_LIBS)
endif

BIN_DIR := bin
LIB_DIR := lib
BUILD_DIR := build

CLI_TARGET := $(BIN_DIR)/cml
LIB_TARGET := $(LIB_DIR)/libcml.a

LIB_SRCS := \
  src/cml.c \
  src/cml_http.c \
  src/cml_proto.c \
  src/cml_api.c \
  src/cml_crypto.c \
  src/cml_fs.c \
  src/cml_naming.c \
  src/cml_export_raw.c \
  src/cml_export_cbz.c \
  src/cml_loader.c \
  src/cml_ids.c \
  src/cml_url.c

CLI_SRCS := \
  src/cml_cli.c

LIB_OBJS := $(LIB_SRCS:src/%.c=$(BUILD_DIR)/%.o)
CLI_OBJS := $(CLI_SRCS:src/%.c=$(BUILD_DIR)/%.o)
DEPS := $(LIB_OBJS:.o=.d) $(CLI_OBJS:.o=.d)

.PHONY: all clean
all: $(LIB_TARGET) $(CLI_TARGET)

$(CLI_TARGET): $(CLI_OBJS) $(LIB_TARGET) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $(CLI_OBJS) $(LIB_TARGET) $(LDLIBS)

$(LIB_TARGET): $(LIB_OBJS) | $(LIB_DIR)
	ar rcs $@ $(LIB_OBJS)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BIN_DIR) $(LIB_DIR) $(BUILD_DIR):
	mkdir -p $@

-include $(DEPS)

clean:
	rm -rf $(BIN_DIR) $(LIB_DIR) $(BUILD_DIR)

