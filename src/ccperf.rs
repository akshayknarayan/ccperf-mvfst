#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
include!(concat!(env!("OUT_DIR"), "/ccperf.rs"));

pub struct Future(std_future);

impl Future {
    fn wait(self) -> i32 {
        unsafe { get(self.0) }
    }
}

use std::sync::{Arc, Mutex};

pub struct Client(Arc<Mutex<QuicClient>>);

impl Client {
    fn new(addr: std::net::SocketAddrV4) -> Self {
        let ip = format!("{}", addr.ip());
        let port = addr.port();

        Self(Arc::new(Mutex::new(unsafe {
            QuicClient::new(std::ffi::CString::new(ip).unwrap().as_ptr(), port)
        })))
    }

    fn connect(&mut self) -> Future {
        let mut cl = self.0.lock().unwrap();
        Future(unsafe { cl.connect() })
    }

    fn send(&mut self, data: &[u8]) -> Future {
        let raw_data = data.as_ptr() as *const std::ffi::c_void;
        let len = data.len();
        let mut cl = self.0.lock().unwrap();
        Future(unsafe { cl.send(raw_data, len as u32) })
    }

    fn new_stream(&mut self, len: usize) -> Stream {
        let mut cl = self.0.lock().unwrap();
        let res = unsafe { cl.createStream(len as u32) };
        Stream(self.0.clone(), res.first, Future(res.second), len)
    }
}

pub struct Stream(Arc<Mutex<QuicClient>>, quic_StreamId, Future, usize);

impl Stream {
    fn send(&mut self, data: &[u8]) {
        let raw_data = data.as_ptr() as *const std::ffi::c_void;
        let len = data.len() as u32;
        let mut cl = self.0.lock().unwrap();
        unsafe { cl.sendOnStream(self.1, raw_data, len) };
    }

    fn wait(self) -> i32 {
        self.2.wait()
    }
}

pub struct Server(QuicServer);

impl Server {
    fn new(port: u16) -> Self {
        Self(unsafe { QuicServer::new(port) })
    }

    fn start(mut self) {
        unsafe { self.0.start() };
    }
}
