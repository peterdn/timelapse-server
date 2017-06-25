use std::env;
use std::io::Write;
use std::net::TcpListener;

fn main() {
    println!("Timelapse Server");

    let args: Vec<String> = env::args().collect();

    // Directory where timelapse images are stored.
    let mut dir = "";
    let mut binding = "0.0.0.0:8060";
    let mut i = 0;
    while i < args.len() {
        if args[i] == "-d" || args[i] == "--directory" {
            i += 1;
            dir = &args[i];
        }
        if i >= args.len() {
            panic!("Not enough argument parameters given!");
        }
        i += 1;
    }

    if dir == "" {
        println!("Using current directory...");
    } else {
        println!("Using directory {}", dir);
    }

    println!("Starting server...");
    
    let listener = TcpListener::bind(binding).unwrap();

    for stream in listener.incoming() {
        match stream {
            Ok(mut stream) => {
                println!("Got connection!");
                stream.write(b"HTTP/1.x 200 OK\n\
                        Content-Type: text/html; charset=UTF-8\n\
                        \n\
                        <html><head></head><body><h1>Hello</h1></body></html>");
                stream.flush();
            }
            Err(e) => { panic!("Connection failed!"); }
        }
    }
}
