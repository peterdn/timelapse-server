use std::env;
use std::fs::File;
use std::io::BufReader;
use std::io::Read;
use std::io::Write;
use std::net::Shutdown;
use std::net::TcpListener;
use std::net::TcpStream;
use std::thread;

fn read_string(stream: &mut TcpStream) -> String {
    let buffer: &mut [u8; 1] = &mut [0];
    let mut ret = String::new();
    while buffer[0] != b'\n' {
        stream.read(buffer).expect("Failed to read from stream");
        if buffer[0] != b'\r' && buffer[0] != b'\n' {
            ret.push(buffer[0] as char);
        }
    }
    return ret;
}

fn handle_request(mut stream: TcpStream, dir: String) {
    let mut valid = true;
    loop {
        let line = read_string(&mut stream);
        if line.len() == 0 {
            valid = false;
            break;
        }
        if line == "GET /timelapse HTTP/1.1" {
            println!("Getting timelapse!");
            main_page(&mut stream);
            break;
        } else if line == "GET /current.jpg HTTP/1.1" {
            println!("Getting image!");
            current_image(&mut stream, &dir);
            break;
        }
    }

    if !valid {
        not_found(&mut stream);
    }

    stream.flush().expect("Failed to flush stream!");
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
          <img src='/current.jpg' />\
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
