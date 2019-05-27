all: mvfst.o

MVFST_STATIC_LIBS := $(shell find mvfst/_build -name "*.a")
MVFST_STATIC_LIB_LOCS := $(shell find mvfst/_build -name "*.a" | xargs -I{} dirname {} | uniq | awk '{print "-L " $$0 " "}')
MVFST_LIBNAMES := $(shell find mvfst/_build -name "*.a" -type f | xargs basename | sort | uniq | sed 's/lib\(.*\)\.a/-l\1/g')

ccperf: mvfst/_build mvfst.hpp ccperf.cpp $(MVFST_STATIC_LIBS)
	clang++ \
		-std=c++17 \
		-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/ \
		-I /usr/local/include \
		-I /usr/local/opt/openssl/include \
		-I./mvfst/_build/deps/include \
		-I./mvfst/quic/common/test \
		-I./mvfst/_build/build/fizz_project/src/fizz_project \
		-I./mvfst/_build/build/googletest/src/googletest/googlemock/include \
		-I./mvfst/_build/build/googletest/src/googletest/googletest/include \
		-I./mvfst \
		-I./mvfst/_build/deps/include \
		-o ccperf \
		-L /usr/local/lib \
		-L /usr/local/opt/openssl/lib \
		$(MVFST_STATIC_LIB_LOCS) \
		-lglog -lgflags -ldouble-conversion -levent -lcrypto -lssl -lsodium \
		$(MVFST_LIBNAMES) \
		ccperf.cpp

mvfst/_build:
	cd mvfst && env MVFST_FOLLY_USE_JEMALLOC=n ./build_helper.sh

clean_mvfst:
	cd mvfst && rm -rf _build
