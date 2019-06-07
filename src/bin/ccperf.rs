extern crate ccperf_mvfst;
extern crate chrono;
extern crate chrono_english;
extern crate failure;

use failure::bail;
use std::str::FromStr;

use structopt::StructOpt;

#[derive(Debug)]
enum Mode {
    Client,
    Server,
}

impl std::str::FromStr for Mode {
    type Err = failure::Error;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "c" | "client" => Ok(Mode::Client),
            "s" | "server" => Ok(Mode::Server),
            _ => bail!("Mode should be (client|server)"),
        }
    }
}

#[derive(StructOpt, Debug)]
#[structopt(name = "basic")]
struct Opt {
    #[structopt(short = "m", long = "mode")]
    mode: Mode,

    #[structopt(short = "p", long = "port")]
    port: u16,

    #[structopt(short = "a", long = "addr")]
    addr: String,
}

fn main() {
    let x =
        chrono_english::parse_date_string("1m", chrono::Local::now(), chrono_english::Dialect::Us);
    println!("{:?}", x);

    return;

    let opt = Opt::from_args();

    match opt.mode {
        Mode::Client => {
            let cl = ccperf_mvfst::ccperf::Client::new(std::net::SocketAddrV4::new(
                std::net::Ipv4Addr::from_str(&opt.addr).unwrap(),
                opt.port,
            ));
        }
        Mode::Server => {
            let srv = ccperf_mvfst::ccperf::Server::new(opt.port);
            srv.start();
        }
    }
}
