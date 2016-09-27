PREFIX ?= arm-none-eabi

OPENCM3_DIR ?= $(realpath libopencm3)
RULES = elf

all: build

bin: build
hex: build
srec: build
list: build
images: build

build: lib src

lib:
	$(Q)if [ ! "`ls -A $(OPENCM3_DIR)`" ] ; then \
		printf "######## ERROR ########\n"; \
		printf "\tlibopencm3 is not initialized.\n"; \
		printf "\tPlease run:\n"; \
		printf "\t$$ git submodule init\n"; \
		printf "\t$$ git submodule update\n"; \
		printf "\tbefore running make.\n"; \
		printf "######## ERROR ########\n"; \
		exit 1; \
		fi
	$(Q)$(MAKE) -C $(OPENCM3_DIR)

src: lib
	@printf "  BUILD  $@\n":
	$(Q)$(MAKE) --directory=./$@ OPENCM3_DIR=$(OPENCM3_DIR) $(RULES)

.PHONY: build lib src install bin hex srec list images
