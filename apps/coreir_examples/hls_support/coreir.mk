SHELL := /bin/bash

APP_PATH := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
APPNAME := $(notdir $(patsubst %/,%,$(dir $(APP_PATH))))

# empty "make all" rule included here so "make" refers to "make all"
all:


# Use halide compiler to create design.
pipeline: pipeline.cpp
	$(CXX) $(CXXFLAGS) -Wall -g $^ $(LIB_HALIDE) -o $@ $(LDFLAGS) -ltinfo

pipeline_hls.cpp pipeline_native.o design_top.json design_top.txt: pipeline
	HL_DEBUG_CODEGEN=0 ./pipeline


# Use design and run it to create output image.
run: run.cpp pipeline_hls.cpp pipeline_native.o hls_target.cpp
	$(CXX) $(CXXFLAGS) -O1 -DNDEBUG $(HLS_CXXFLAGS) -g -Wall -Werror $^ -lpthread -ldl $(LIB_HALIDE) -o $@ $(PNGFLAGS) $(LDFLAGS)

out.png: run input.png design_top.json
	./run input.png

# Use graphviz to create graph of processing nodes.
graph.png: design_top.txt
	dot -Tpng design_top.txt > graph.png


# Test if app works, using cached result if json design matches golden.
test:
	@$(MAKE) -s design_top.json
	@if [ -f "passed.md5" ]; then \
		md5sum -c --status passed.md5; \
		EXIT_CODE=$$?; \
		if [[ $$EXIT_CODE = "0" ]]; then \
			printf "%-15s \033[0;32m%s\033[0m\n" $(APPNAME) "PASSED"; \
		else \
			$(MAKE) -s testrun; \
		fi \
	elif [ -f "failed.md5" ]; then \
		md5sum -c --status failed.md5; \
		EXIT_CODE=$$?; \
		if [[ $$EXIT_CODE = "0" ]]; then \
			printf "%-15s \033[0;31m%s\033[0m\n" $(APPNAME) "FAILED" && exit 1; \
		else \
			$(MAKE) -s testrun; \
		fi \
	else \
		$(MAKE) -s testrun; \
	fi

# Run design on cpu and coreir interpreter, and print if it passes/fails.
testrun:
		@-$(MAKE) out.png; \
		EXIT_CODE=$$?; \
		if [[ $$EXIT_CODE = "0" ]]; then \
			printf "%-15s \033[1;33m%s\033[0m\n" $(APPNAME) "PASSED, but needs golden updated"; \
		else \
			printf "%-15s \033[0;31m%s\033[0m\n" $(APPNAME) "FAILED, and needs golden updated" && exit 1; \
		fi

# Update golden file, run design, and store result in md5 filename.
update_golden passed.md5 failed.md5: design_top.json
#	@$(MAKE) -s design_top.json
	@cp design_top.json design_top_golden.json
	@-$(MAKE) -s out.png; \
	EXIT_CODE=$$?; \
	if [[ $$EXIT_CODE = "0" ]]; then \
		rm -f failed.md5; \
		md5sum design_top.json > passed.md5; \
		echo "$(APPNAME): Updated design_top_golden.json and created passed.md5"; \
	else \
		rm -f failed.md5; \
		md5sum design_top.json > failed.md5; \
		echo "$(APPNAME): Updated design_top_golden.json and created failed.md5"; \
	fi

# Clean all of the common generated files.
clean_commonfiles:
	rm -f pipeline run
	rm -f *.html
	rm -f pipeline_native.h pipeline_native.o
	rm -f pipeline_hls.h pipeline_hls.cpp hls_target.h hls_target.cpp
	rm -f pipeline_coreir.cpp pipeline_coreir.h coreir_target.h coreir_target.cpp
	rm -f input_unique.pgm input_unique.png
	rm -f design_prepass.json design_top.json design_flattened.json
	rm -f design_top.txt graph.png out.png
