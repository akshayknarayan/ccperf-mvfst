all: ccperf

ccperf: mvfst/_build mvfst.hpp ccperf.cpp
	$(eval MVFST_STATIC_LIB_LOCS := $(shell find mvfst/_build -name "*.a" -type f | xargs -I{} dirname {} | sort | uniq | awk '{print "-L " $$0 " "}'))
	$(eval MVFST_LIBNAMES := $(shell find mvfst/_build -name "*mvfst*.a" -type f | xargs -I{} basename {} | sort | uniq | sed 's/lib\(.*\)\.a/-l\1/g'))
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
		-g -O0 \
		-o ccperf \
		-L /usr/local/lib \
		-L /usr/local/opt/openssl/lib \
		$(MVFST_STATIC_LIB_LOCS) \
		$(MVFST_LIBNAMES) \
		-lfizz -lfizz_test_support  -lfollybenchmark -lfolly_test_util -lgmock -lgtest  \
		-lfolly \
		-lgflags -ldouble-conversion -levent -lpthread -lcrypto -lssl -lsodium -lglog \
		ccperf.cpp

mvfst/_build:
	cd mvfst && env MVFST_FOLLY_USE_JEMALLOC=n ./build_helper.sh

clean_mvfst:
	cd mvfst && rm -rf _build
