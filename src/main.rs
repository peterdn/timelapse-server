use std::env;
use std::fs::File;
use std::io::{BufRead, BufReader};
use std::io::Read;
use std::io::Write;
use std::net::Shutdown;
use std::net::TcpListener;
use std::net::TcpStream;
use std::thread;

fn handle_request(mut stream: TcpStream, dir: String) {
    // GET will be in first line of header.
    let mut get_line = String::new();

    {
        let mut reader = BufReader::new(&stream);
        reader.read_line(&mut get_line)
                    .expect("Failed to read line from stream");

        // Read rest of request. If we don't, the client will
        // behave unexpectedly when we close the socket on
        // shutdown. For example, curl will complain that the
        // transfer closed with X bytes remaining to read.
        let mut line = String::new();
        loop {
            reader.read_line(&mut line)
                    .expect("Failed to read line from stream");
            if line == "\r\n" {
                break;
            }
            line.clear();
        }
    }

    let get_line = get_line.trim();

    if get_line == "GET /timelapse HTTP/1.1" {
        println!("Getting timelapse!");
        main_page(&mut stream);
    } else if get_line == "GET /current.jpg HTTP/1.1" {
        println!("Getting image!");
        current_image(&mut stream, &dir);
    } else {
        not_found(&mut stream);
    }

    stream.shutdown(Shutdown::Write).expect("Could not shut down!");
}

fn main() {
    println!("Timelapse Server");

    let args: Vec<String> = env::args().collect();

    // Directory where timelapse images are stored.
    let mut dir = "";
    let binding = "0.0.0.0:8060";
    let mut i = 0;
    while i < args.len() {
        if args[i] == "-d" || args[i] == "--directory" {
            i += 1;
            dir = args[i].as_str();
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
            Ok(stream) => {
                println!("Received request");
                let dir_string = String::from(dir);
                thread::spawn(|| {
                    handle_request(stream, dir_string);
                });
            }
            Err(_) => { panic!("Connection failed!"); }
        }
    }
}

fn main_page(stream: &mut TcpStream) {
    write!(stream, "HTTP/1.1 200 OK\n\
        Content-Type: text/html\n\
        Content-Length: 65\n\
        \n\
        <html><head></head><body>\
          <img width='800' src='/current.jpg' />\
        </body></html>").expect("Failed to write to stream");
}

fn not_found(stream: &mut TcpStream) {
    stream.write(b"HTTP/1.1 404 Not Found\n\
        \n").expect("Failed to write to stream");
}

fn current_image(stream: &mut TcpStream, dir: &str) {
    let image_filepath = format!("{}/current.jpg", dir);
    let image_file = File::open(image_filepath).expect("Unable to open image file");
    let mut reader = BufReader::new(image_file);
    let mut image: Vec<u8> = Vec::new();
    reader.read_to_end(&mut image).expect("Unable to read image");

    println!("Image {}/current.jpg is size {}", dir, image.len());

    write!(stream, "HTTP/1.1 200 OK\n\
        Content-Type: image/jpeg\n\
        Content-Length: {}\n\
        \n", image.len()).expect("Failed to write to stream");
    stream.write_all(image.as_slice()).expect("Failed to write image data to stream");
}
