use std::io::{BufRead, BufReader};
use std::io::Write;
use std::net::Shutdown;
use std::net::TcpListener;
use std::net::TcpStream;
use std::process::Command;
use std::thread;

extern crate redis;
use redis::Commands;

fn handle_request(mut stream: TcpStream) {
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
        current_image(&mut stream);
    } else {
        not_found(&mut stream);
    }

    stream.shutdown(Shutdown::Write).expect("Could not shut down!");
}

fn main() {
    println!("Timelapse Server");

    let binding = "0.0.0.0:8060";

    println!("Starting server...");
    
    let listener = TcpListener::bind(binding).unwrap();

    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                println!("Received request");
                thread::spawn(|| {
                    handle_request(stream);
                });
            }
            Err(_) => { panic!("Connection failed!"); }
        }
    }
}

fn main_page(stream: &mut TcpStream) {
    let page_body = "<html><head></head><body>\
          <img src='/current.jpg' />\
        </body></html>";
    write!(stream, "HTTP/1.1 200 OK\n\
        Content-Type: text/html\n\
        Content-Length: {}\n\
        \n\
        {}", page_body.len(), page_body).expect("Failed to write to stream");
}

fn not_found(stream: &mut TcpStream) {
    stream.write(b"HTTP/1.1 404 Not Found\n\
        \n").expect("Failed to write to stream");
}

fn current_image(stream: &mut TcpStream) {
    let redis_client = redis::Client::open("redis://127.0.0.1/").expect("Failed to connect to redis server!");
    let conn = redis_client.get_connection().expect("Failed to get connection!");
    let image_filepath : String = conn.get("current.jpg").expect("Failed to get current image!");
    let image_filepath = image_filepath.as_str();

    // Attempt to load image from Redis. If it's not there, load and cache.
    let mut image_buffer: Vec<u8> = conn.get::<&str, Vec<u8>>(image_filepath).expect("Failed to read from Redis!");
    if image_buffer.len() > 0 {
        println!("Found cached image in Redis...");
    } else {
        println!("Loading image from file...");

        let output = Command::new("./omx_resize_image").args(&[image_filepath]).output()
                .expect("Failed to execute omx_resize_image");
        image_buffer = output.stdout;

        // TODO: pointlessly have to clone the image here?
        conn.set::<&str, Vec<u8>, ()>(image_filepath, image_buffer.clone()).expect("Failed to cache image in Redis!");
    }

    println!("Image {} is size {}", image_filepath, image_buffer.len());

    write!(stream, "HTTP/1.1 200 OK\n\
        Content-Type: image/jpeg\n\
        Content-Length: {}\n\
        \n", image_buffer.len()).expect("Failed to write to stream");
    stream.write_all(image_buffer.as_slice()).expect("Failed to write image data to stream");
}
