#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/uio.h>

#define PORT 6969
#define BUFFER_SIZE 4096

char *html_body_get =
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head><title>Nice Server</title></head>\n"
    "<body style=\"font-family: Comic Sans MS, cursive; background: #1a1a2e; color: #eee; padding: 40px;\">\n"
    "<h1>Welcome to the Nice Server on port 6969</h1>\n"
    "<p>Why did the programmer quit his job?</p>\n"
    "<p><b>Because he didn't get arrays!</b></p>\n"
    "<hr>\n"
    "<h2>Tell me a secret:</h2>\n"
    "<form method=\"POST\" action=\"/\">\n"
    "  <input type=\"text\" name=\"secret\" placeholder=\"Your deepest secret...\" style=\"padding: 10px; width: 300px;\">\n"
    "  <button type=\"submit\" style=\"padding: 10px 20px;\">Confess</button>\n"
    "</form>\n"
    "</body>\n"
    "</html>\n";

void send_get_response(int client_fd){
    char headers[256];
    int body_len = strlen(html_body_get);
    int headers_len = snprintf(headers, sizeof headers,
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        body_len);
    struct iovec iov[2] = {
        { headers, headers_len },
        { html_body_get, body_len }
    };
    writev(client_fd, iov, 2);
}

void html_escape(char *dst, int dst_size, const char *src){
    int j = 0;
    for(int i = 0; src[i] && j < dst_size - 1; i++){
        char c = src[i];
        if(c == '<'){
            if(j + 4 >= dst_size) break;
            dst[j++] = '&'; dst[j++] = 'l'; dst[j++] = 't'; dst[j++] = ';';
        }
        else if(c == '>'){
            if(j + 4 >= dst_size) break;
            dst[j++] = '&'; dst[j++] = 'g'; dst[j++] = 't'; dst[j++] = ';';
        }
        else if(c == '&'){
            if(j + 5 >= dst_size) break;
            dst[j++] = '&'; dst[j++] = 'a'; dst[j++] = 'm'; dst[j++] = 'p'; dst[j++] = ';';
        }
        else if(c == '"'){
            if(j + 6 >= dst_size) break;
            dst[j++] = '&'; dst[j++] = 'q'; dst[j++] = 'u'; dst[j++] = 'o'; dst[j++] = 't'; dst[j++] = ';';
        }
        else dst[j++] = c;
    }
    dst[j] = '\0';
}

int handle_post(int client_fd, char *body){
    char headers[256];
    char html_body[BUFFER_SIZE];
    char *secret = strstr(body, "secret=");
    char decoded_secret[256] = "nothing";
    if(secret){
        secret += 7; // skip "secret="
        char *end = strchr(secret, '&');
        if(end) *end = '\0';

        // Simple URL decode (just + to space)
        int j = 0;
        for(int i = 0; secret[i] && j < 255; i++){
            if(secret[i] == '+') decoded_secret[j++] = ' ';
            else if(secret[i] == '%' && secret[i+1] && secret[i+2]){
                int val;
                sscanf(secret + i + 1, "%2x", &val);
                decoded_secret[j++] = val;
                i += 2;
            }
            else decoded_secret[j++] = secret[i];
        }
        decoded_secret[j] = '\0';
        if(strcmp(decoded_secret, "shutdown") == 0)
            return 1;
    }
    char safe_secret[1024];
    html_escape(safe_secret, sizeof safe_secret, decoded_secret);
    int body_len = snprintf(html_body, sizeof html_body,
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>Secret Received!</title></head>\n"
        "<body style=\"font-family: Comic Sans MS, cursive; background: #1a1a2e; color: #eee; padding: 40px; text-align: center;\">\n"
        "<h1>I have received your secret!</h1>\n"
        "<p style=\"font-size: 24px;\">You said: <i>\"%s\"</i></p>\n"
        "<p>Don't worry, I definitely won't tell anyone. Trust me bro.</p>\n"
        "<p style=\"font-size: 100px;\">&#128064;</p>\n"
        "<a href=\"/\" style=\"color: #7ec8e3;\">Tell me another secret</a>\n"
        "</body>\n"
        "</html>\n",
        safe_secret);
    int headers_len = snprintf(headers, sizeof headers,
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        body_len);
    struct iovec iov[2] = {
        { headers, headers_len },
        { html_body, body_len }
    };
    writev(client_fd, iov, 2);
    return 0;
}

int main(){
    int server_fd, client_fd;
    struct sockaddr_in addr;
    char buffer[BUFFER_SIZE];
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        perror("socket");
        return 1;
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(PORT);
    if(bind(server_fd, (struct sockaddr *)&addr, sizeof addr) < 0){
        perror("bind");
        return 1;
    }
    if(listen(server_fd, 5) < 0){
        perror("listen");
        return 1;
    }
    printf("Server running on http://localhost:%d\n", PORT);
    printf("Press Ctrl+C to stop\n\n");
    int stop = 0;
    while(!stop){
        client_fd = accept(server_fd, NULL, NULL);
        if(client_fd < 0){
            perror("accept");
            continue;
        }
        int total = 0;
        int n;
        while((n = read(client_fd, buffer + total, sizeof buffer - 1 - total)) > 0){
            total += n;
            buffer[total] = '\0';
            // Check if we have complete headers
            char *body_start = strstr(buffer, "\r\n\r\n");
            if(!body_start)
                continue;
            // For GET, we're done once we have headers
            if(strncmp(buffer, "POST", 4) != 0)
                break;
            // For POST, check Content-Length
            char *cl = strstr(buffer, "Content-Length:");
            if(!cl) cl = strstr(buffer, "content-length:");
            if(!cl)
                break; // no content-length, use what we have
            int content_len = atoi(cl + 15);
            int body_received = total - (body_start + 4 - buffer);
            if(body_received >= content_len)
                break;
        }
        if(total > 0){
            printf("--- Request ---\n%s\n", buffer);
            if(strncmp(buffer, "POST", 4) == 0){
                char *body = strstr(buffer, "\r\n\r\n");
                if(body){
                    body += 4;
                    stop = handle_post(client_fd, body);
                }
            } 
            else {
                send_get_response(client_fd);
            }
        }
        close(client_fd);
    }
    close(server_fd);
    return 0;
}
