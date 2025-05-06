#define CPPHTTPLIB_OPENSSL_SUPPORT // If you need HTTPS
#include <httplib.h>
#include <iostream>

int main(void) {
    httplib::Server svr;

    svr.Get("/hello", [](const httplib::Request &, httplib::Response &res) {
        res.set_content("Hello from C++ Backend!", "text/plain");
    });


    svr.Post("/chat", [](const httplib::Request &req, httplib::Response &res) {
        std::string body = req.body;
        std::cout << "Received message: " << body << std::endl;

        std::string reply = "Backend received: " + body; 

        res.set_content(reply, "text/plain");
    });

    std::cout << "C++ Backend server starting on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080); 

    return 0;
}