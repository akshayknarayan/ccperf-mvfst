all: mvfst.o

ccperf: mvfst/_build mvfst.hpp ccperf.cpp
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
		-o ccperf \
		-L /usr/local/Cellar/glog/0.4.0/lib \
		-L /usr/local/Cellar/gflags/2.2.2/lib \
		-L /usr/local/Cellar/boost/1.69.0_2/lib \
		-L /usr/local/Cellar/double-conversion/3.1.4/lib \
		-L /usr/local/Cellar/libevent/2.1.8/lib \
		-L /usr/local/Cellar/openssl/1.0.2r/lib \
		-L /usr/local/Cellar/libsodium/1.0.17/lib \
		-L ./mvfst/_build/deps/lib \
		-L ./mvfst/_build/build/fizz_project/src/fizz_project-build/lib \
		-L ./mvfst/_build/build/googletest/src/googletest-build/googlemock/gtest \
		-L ./mvfst/_build/build/googletest/src/googletest-build/googlemock \
		-L ./mvfst/_build/build/quic \
		-L ./mvfst/_build/build/quic/api \
		-L ./mvfst/_build/build/quic/client \
		-L ./mvfst/_build/build/quic/codec \
		-L ./mvfst/_build/build/quic/congestion_control \
		-L ./mvfst/_build/build/quic/common \
		-L ./mvfst/_build/build/quic/common/test \
		-L ./mvfst/_build/build/quic/flowcontrol \
		-L ./mvfst/_build/build/quic/handshake \
		-L ./mvfst/_build/build/quic/happyeyeballs \
		-L ./mvfst/_build/build/quic/loss \
		-L ./mvfst/_build/build/quic/server \
		-L ./mvfst/_build/build/quic/state \
		-lglog -lgflags -ldouble-conversion -levent -lcrypto -lssl -lsodium -lfizz -lfolly \
		-lgmock -lgtest -lgtest_main \
		-lmvfst_constants -lmvfst_exception \
		-lmvfst_codec -lmvfst_codec_decode -lmvfst_codec_packet_number_cipher -lmvfst_codec_pktbuilder -lmvfst_codec_pktrebuilder -lmvfst_codec_types \
		-lmvfst_state_ack_handler -lmvfst_state_functions -lmvfst_state_machine -lmvfst_state_pacing_functions -lmvfst_state_qpr_functions -lmvfst_state_simple_frame_functions -lmvfst_state_stream -lmvfst_state_stream_functions \
		-lmvfst_handshake -lmvfst_happyeyeballs -lmvfst_looper -lmvfst_flowcontrol -lmvfst_loss -lmvfst_cc_algo -lmvfst_test_utils \
		-lmvfst_transport -lmvfst_client -lmvfst_server \
		ccperf.cpp

mvfst/_build:
	cd mvfst && env MVFST_FOLLY_USE_JEMALLOC=n ./build_helper.sh

clean_mvfst:
	cd mvfst && rm -rf _build
