all: mvfst.o

mvfst.o: mvfst/_build mvfst.hpp
	clang++ \
		-std=c++17 \
		-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/ \
		-I /usr/local/Cellar/glog/0.4.0/include \
		-I /usr/local/Cellar/gflags/2.2.2/include \
		-I /usr/local/Cellar/boost/1.69.0_2/include \
		-I /usr/local/Cellar/double-conversion/3.1.4/include \
		-I /usr/local/Cellar/libevent/2.1.8/include \
		-I /usr/local/Cellar/openssl/1.0.2r/include \
		-I /usr/local/Cellar/libsodium/1.0.17/include \
		-I./mvfst/_build/deps/include \
		-I./mvfst/quic/common/test \
		-I./mvfst/_build/build/fizz_project/src/fizz_project \
		-I./mvfst/_build/build/googletest/src/googletest/googlemock/include \
		-I./mvfst/_build/build/googletest/src/googletest/googletest/include \
		-I./mvfst \
		-I./mvfst/_build/deps/include \
		-o mvfst.o \
		mvfst.hpp

mvfst/_build:
	cd mvfst && env MVFST_FOLLY_USE_JEMALLOC=n ./build_helper.sh

clean_mvfst:
	cd mvfst && rm -rf _build
