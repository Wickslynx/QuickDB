/* 

Copyright (c) 2024 Wicks (Original Author of AuroraDB)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/








#include <iostream>      // For cout, cin, and more.
#include <string>        // String library.
#include <unordered_map> // The unordered_map.
#include <fstream>       // For file usage.
#include <mutex>         // Imports standard mutex library.
#include <shared_mutex>  // Imports an add-on to the standard mutex library.
#include <thread>        // Imports multi-threading.
#include <netinet/in.h>  // For networking.
#include <sys/socket.h>  // For networking.
#include <unistd.h>      // For networking.
#include <stdexcept>     // For all error, Will be replaced and removed soon..
#include <sstream>       // For getting strings.
#include <vector>        //For vectors.
#include <functional>    //For usefull stuff.
#include <algorithm>     // For additional string and algorithm functions
#include <cstring>       // For memset
#include <ctime>         //For time stuff.

// Define, the best thing in C++.
#define ERROR_MSG(string) (std::cerr << "ERROR!: " << string << std::endl)
#define ServerMessage(input, socket) (const char* message = input; send(socket, message, strlen(message), 0))

// All using statements (Makes my life easier.)
using std::cin;
using std::cout;
using std::cerr;
using std::shared_lock;
using std::string;
using std::unique_lock; 

class AuroraDB {
private:
    std::unordered_map<string, string> db;     // Starts unordered map.
    std::unordered_map<string, string> buffer; // Starts a buffer to load data into.
    std::unordered_map<string, bool> tags;     //Creates unordered map to set tags.
    std::unordered_map<string, std::vector<std::pair<string, string>>> tagged_users; //Starts a map to load the users with the same tag into.
    std::shared_mutex db_mutex;                // Starts a mutex that's called "db_mutex".
    string current_tag = "default";            // Added to track current tag

    //----------------------------------------------------------------------------------------------------------------------------------------------
    //Internal function definitions.
    std::string hash(const std::string &input) {
        std::hash<std::string> hasher; //Declare the hasher.
        size_t hashed_value = hasher(input); //Hash the input.
        return std::to_string(hashed_value);  //return the hashed value.
    }

    //----------------------------------------------------------------------------------------------------------------------------------------------

    void load(const string &file) {
        std::ifstream inputFile(file); // Opens file in "read" mode.
        if (!inputFile) {
            // If file doesn't exist, just return silently
            return;
        }

        string line;
        current_tag = "default"; // Reset to default tag

        // First, look for the tag
        while (std::getline(inputFile, line)) {
            if (line.substr(0, 12) == "<AuroraDB::") {
                // Extract tag name between <AuroraDB::-  and -> 
                size_t start = line.find("-") + 2;
                size_t end = line.find(">", start);
                if (start != string::npos && end != string::npos) {
                    current_tag = line.substr(start, end - start); //Set the current_tag to the tag.
                }
            } else if (!line.empty()) {
                // Parse username and password
                std::istringstream iss(line);
                string username, password;
                if (iss >> username >> password) {
                    // Store with tag
                    db[current_tag + ":" + username] = password;
                }
            }
        }
    }

    void save(const string &filename) {
        std::ofstream outfile(filename, std::ios::out | std::ios::trunc); 
    
        if (!outfile.is_open()) {
            // Add more detailed error reporting
            cerr << "Error: Cannot open file for saving. Filename: " << filename << "\n";
            cerr << "Errno: " << errno << " - " << strerror(errno) << "\n";
            throw std::runtime_error("Error opening file for saving.");
        }    

        // Debug: print out database contents before saving
        cerr << "Saving database. Total entries: " << db.size() << "\n";
        for (const auto &entry : db) {
            cerr << "Key: " << entry.first << ", Value: " << entry.second << "\n";
        }

        // Group users by their database tag
        std::unordered_map<string, std::vector<std::pair<string, string>>> tagged_users;
    
        for (const auto &pair : db) {
            size_t tag_separator = pair.first.find(':');  // Split the key into tag and username
        
            if (tag_separator != string::npos) {
                string tag = pair.first.substr(0, tag_separator);
                string username = pair.first.substr(tag_separator + 1);
                tagged_users[tag].emplace_back(username, pair.second);
            }
        }

        // Write each database tag group
        for (const auto &tag_group : tagged_users) {
            // Write tag at the beginning of the user list
            outfile << "<AuroraDB::" << tag_group.first << "> -\n";
        
            // Write users in this tag group
            for (const auto &user : tag_group.second) {
                outfile << user.first << " " << user.second << "\n";
            }
        
            outfile << "</AuroraDB>\n\n"; 
        }

        outfile.flush();  // Explicitly flush
    
        if (outfile.fail()) {
            cerr << "Error: Failed to write to file\n";
        }

        outfile.close(); // Close the file.

        // Verify file was written
        std::ifstream verifyFile(filename);
        if (verifyFile.peek() == std::ifstream::traits_type::eof()) {
            cerr << "Warning: File appears to be empty after saving\n";
        }
    }

    string GetCurrentTime() {
        
        std::time_t currentTime = std::time(nullptr); //Get current time.
    
        std::tm* localTime = std::localtime(&currentTime); // Convert to local.
    

        char buffer[80]; //Declare an buffer.
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localTime); // Convert to string format
    
        return string(buffer);
    }




    void WriteToLog(const string& message) {
        std::ofstream outfile("storage/log.txt", std::ios::out);

        if (!outfile.is_open()) {
            throw std::runtime_error("Error opening file for saving.");
        }

        outfile << GetCurrentTime() << " [AuroraDB] " << message << "\n";
        
    }


public:
    AuroraDB() {
        try {
            tags["default"] = true;
            load("storage/storage.txt"); // Runs loading method.
        } catch (const std::runtime_error &e) {
            cerr << "Error loading database: " << e.what() << "\n";
        }
    }

    ~AuroraDB() {
        try {
            save("storage/storage.txt");
        } catch (const std::runtime_error &e) {
            cerr << "Error saving database: " << e.what() << "\n";
            
        }
    }
    

     //----------------------------------------------------------------------------------------------------------------------------------------------

     void connect(const int &port) {
        int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        sockaddr_in serverAddress;
        memset(&serverAddress, 0, sizeof(serverAddress));
        serverAddress.sin_family = AF_INET;         //Use TCP
        serverAddress.sin_port = htons(port);       // Set port to 8080.
        serverAddress.sin_addr.s_addr = INADDR_ANY; // Change to accept only one IP.

        if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
            close(serverSocket);
            throw std::runtime_error("Failed to bind socket");
        }

        if (listen(serverSocket, 10000) < 0) {
            close(serverSocket);
            throw std::runtime_error("Failed to listen on socket");
        }

        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket < 0) {
            close(serverSocket);
            throw std::runtime_error("Failed to accept client connection");
        }

        bool quit = false;
        while (!quit) {
            char buffer[1024] = {0};
            ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytesReceived <= 0) {
                break; // Connection closed or error
            }

            buffer[bytesReceived] = '\0'; // Null-terminate the received data

            std::string command, name, password;
            std::istringstream stream(buffer);
            
            if (!(stream >> command >> name >> password)) {
                ERROR_MSG("Invalid command format");
                continue;
            }

            try {
                if (command == "get") {
                    thread("get", name, password);
                } else if (command == "set") {
                    thread("set", name, password);
                } else if (command == "compare") {
                    thread("compare", name, password);
                } else if (command == "quit") {
                    quit = true;
                } else {
                    ERROR_MSG("Unknown command: " + command);
                }
            } catch (const std::exception& e) {
                ERROR_MSG(e.what());
            }
        }

        close(clientSocket);
        close(serverSocket);
    }

    //----------------------------------------------------------------------------------------------------------------------------------------------
    void cmdArgs(int argc, char *argv[]) {
        if (argc > 1 && argc < 5) {  // If argument is under 4 and over 1.
            std::string cmd, name, password; // Declare cmd, name, password.
            try {
                cmd = argv[1];                        // Assign cmd to argv[1].
                name = (argc > 2) ? argv[2] : "";     // Assign name to argv[2].
                password = (argc > 3) ? argv[3] : ""; // Assign password to argv[3].

                if (cmd == "get" && argc > 2) {
                    cout << (get(name) == 0 ? "Get successful" : "Get failed") << "\n";
                } else if (cmd == "set" && argc > 3) {
                    set(name, password);
                } else if (cmd == "rm" && argc > 2) {
                    rm(name);
                } else if (cmd == "compare" && argc > 3) {
                    compare(name, password);
                } else {
                    throw std::runtime_error("Error: Must use (set (Name, Password), get (Name), rm (Name), compare (Name, Password))");
                }
            } catch (std::runtime_error &e) {
                cerr << "Error: Argument not recognised: " << e.what() << "\n";
            }
        } else if (argc > 4) {
            cerr << "Error: Arguments exceeding limit, maximum of three arguments.\n";
        } else {
            cout << "No command line arguments given. Total arguments: " << argc << std::endl;
        }
    }
    //-----------------------------------------------------------------------------------------------------------------------------------------------
    void set(const string tag, const string &username, const string &password) {
        if (tags.find(tag) == tags.end()) {
            ERROR_MSG("Tag doesn't exist, please create it using addTag()");
        }
        
        // Prevent empty or whitespace-only usernames
        if (username.empty() || std::all_of(username.begin(), username.end(), ::isspace)) {
            cerr << "Error: Username cannot be empty\n";
            return;
        }

        unique_lock<std::shared_mutex> lock(db_mutex);   

        WriteToLog(string("SET " + username + " " + password + " 0 "));
        
        db[tag + ":" + username] = hash(password);   
        
        cout << "Database: User added: " << username << "\n"; 
        
    }


    void addTag(const string& tag) {
        
        if (tag.empty() || std::all_of(tag.begin(), tag.end(), ::isspace)) {
            cerr << "Error: Tag cannot be empty\n"; //If empty of whitespace.
            return;
        }

    
        if (tags.find(tag) != tags.end()) { // Check if tag already exists.
            cerr << "Error: Tag '" << tag << "' already exists\n"; 
            return;
        }
        
        tags[tag] = true;  // Add the tag to "tags" map.
    
        cout << "Tag added: " << tag << "\n";
    }



    void set(const string &username, const string &password) { 
        set("default", username, password); //Set the tag to default tag.
    }

    int get(const string &username) {
        if (username.empty()) {
            cerr << "Error: Username cannot be empty\n";
            return -1;
        }

        shared_lock<std::shared_mutex> lock(db_mutex);
        auto it = db.find(username);
        if (it != db.end()) {
            WriteToLog(string("GET " + username + " 0 "));
            cout << "User: " << username << ":" << it->second << "\n";
            return 0;
        } else {
            cout << "Error: User not found\n";
            return -2;
        }
    }

     //-----------------------------------------------------------------------------------------------------------------------------------------------

    void setLock(string& password) {
        string input;
        while (true) {
            cout << "Verification password: ";
            cin >> input;

            if (input == password) {
                cout << "Password confirmed, Welcome back!" << "\n";
                break;
            } else {
                cout << "Password denied, try again.";
            }
        }
    }

    void thread(const string &function, const string &name, const string &password) {
        std::vector<std::thread> threads;
        try {
            if (function == "get") {
                threads.push_back(std::thread([this, &name]() {
                    this->get(name);
                }));
            } else if (function == "set") {
                threads.push_back(std::thread([this, &name, &password]() {
                    this->set(name, password);
                }));
            } else if (function == "rm") {
                threads.push_back(std::thread([this, &name]() {
                this->rm(name);
            }));
            } else if (function == "compare") {
                threads.push_back(std::thread([this, &name, &password]() {
                    this->compare(name, password);
                }));
            } else {
                throw std::runtime_error("Unknown thread function");
            }

            for (auto &t : threads) {
                t.join();
            }
        } catch (const std::exception& e) {
            cerr << "Thread error: " << e.what() << "\n";
        }
    }

    void InterfaceMode() {
        string action, username, password;
        cout << "Welcome to AuroraDB! \n Please enter which of following actions you want to do \n (1) Set user. (2) Remove user. (3) Get user. (4). Compare user. : ";
        cin >> action;

        cout << "Please enter the username: ";
        cin >> username;
        
        cout << "\nPlease enter the password: ";
        cin >> password;
        
        switch (std::stoi(action)) {
            case 1:
                set(username, password);
                break;
            case 2:
                rm(username);
                break;
            case 3:
                get(username);
                break;
            case 4:
                compare(username, password);
                break;
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------------------
    bool compare(const string &username, const string &password) {
        if (username.empty() || password.empty()) {
            cerr << "Error: Username and password cannot be empty\n";
            return false;
        }

        shared_lock<std::shared_mutex> lock(db_mutex); // Uses shared_lock, multiple threads can read at the same time.
        auto it = db.find(username);                   // Declare variable.
        if (it != db.end() && it->second == hash(password)) {  // If user is found.
            cout << "Password matched for user: " << username << "\n"; // Print success message.
            return true; // Return true.
        } else {
            cout << "No match found for user: " << username << "\n"; // Print no match.
            return false; // Return false.
        }
    }

    void rm(const string &username) {
        if (username.empty()) {
            cerr << "Error: Username cannot be empty\n";
            return;
        }

        unique_lock<std::shared_mutex> lock(db_mutex); // Locks the db.
        WriteToLog(string("RM " + username + " " + " 0 "));
        size_t removed = db.erase(username);            // Erases user from database.
        if (removed > 0) {
            cout << "User removed: " << username << "\n";  // Outputs message.
        } else {
            cout << "User not found: " << username << "\n";
        }
    }
};

int main(int argc, char* argv[]) {
    //Write in your code here.
    AuroraDB db;
    db.cmdArgs(argc, argv);
    db.InterfaceMode();
    db.connect(8080);
    
    // Database supports multiple different solutions, both command line arguments, networking, interactive command line menu and you can write commands under here: 
    // Threading is being worked on!
    // Have fun, greetings Wicks.
    return 0;
}
