#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

#include "fmt/core.h"
#include "fmt/ostream.h"

#include "../src/caps.h"
#include "../src/dbv1.h"

Cap caps{}; 

bool debugflag = true;
bool traceflag = true;

TEST_CASE( "Database Test", "[db]" ) {



  // create a stub/mockup  of everything
  //   - config
  //   - DB

    auto tmpbase = fs::temp_directory_path();
    auto id = getpid();
    auto basedirname = tmpbase / fs::path(fmt::format("wstest{}",id));

    fs::create_directories(basedirname / "ws.d");

    auto ws1dbname = basedirname / fs::path("ws1-db");
    auto ws2dbname = basedirname / fs::path("ws2-db");

    fs::create_directories(ws1dbname);
    fs::create_directories(ws2dbname);

    // create a normal config file, V1 way
    std::ofstream wsconf(basedirname / "ws.conf"); 
    fmt::println(wsconf, 
R"yaml(
admins: [root]
clustername: old_ws_conf
adminmail: [root]
dbgid: 2
dbuid: 2
duration: 10
maxextensions: 1
smtphost: mailhost
default: ws2
workspaces:
    ws1:
        database: {}
        deleted: .removed
        spaces: [/tmp]
    ws2:
        database: {}
        deleted: .removed
        spaces: [/tmp/ws2-old]
)yaml",  ws1dbname.string() , ws2dbname.string()  );
    wsconf.close();

    // create a DB entry
    std::ofstream wsentry1(ws1dbname / "user1-TEST1"); 
    fmt::println(wsentry1, 
R"yaml(
workspace: /a/path
expiration: 1734701876
extensions: 3
reminder: 0
mailaddress: ""
comment: ""
)yaml");
    wsconf.close();    

    // create a DB entry
    std::ofstream wsentry2(ws1dbname / "user2-TEST1"); 
    fmt::println(wsentry2, 
R"yaml(
workspace: /a/path11
expiration: 1734701876
extensions: 3
reminder: 0
mailaddress: ""
comment: ""
)yaml");
    wsconf.close();  

        // create a DB entry
    std::ofstream wsentry3(ws1dbname / "user2-TEST2"); 
    fmt::println(wsentry3, 
R"yaml(
workspace: /a/path12
expiration: 1734701876
extensions: 3
reminder: 0
mailaddress: ""
comment: ""
)yaml");
    wsconf.close();  

        // create a DB entry
    std::ofstream wsentry4(ws2dbname / "user1-TEST1"); 
    fmt::println(wsentry4, 
R"yaml(
workspace: /a/path21
expiration: 1734701876
extensions: 3
reminder: 0
mailaddress: ""
comment: ""
)yaml");
    wsconf.close();  

        // create a DB entry
    std::ofstream wsentry(ws2dbname / "user2-TEST2"); 
    fmt::println(wsconf, 
R"yaml(
workspace: /a/path22
expiration: 1734701876
extensions: 3
reminder: 0
mailaddress: ""
comment: ""
)yaml");
    wsconf.close();  

    auto config = Config(std::vector<fs::path>{basedirname / "ws.conf"});

    std::unique_ptr<Database> db1(config.openDB("ws1"));
    std::unique_ptr<Database> db2(config.openDB("ws2"));

    SECTION("list entries") {
      REQUIRE(db1->matchPattern("TEST*", "user1", vector<string>{}, false, false) ==  vector<string>{"user1-TEST1"});
      REQUIRE(db1->matchPattern("*EST*", "user2", vector<string>{}, false, false) ==  vector<string>{"user2-TEST2", "user2-TEST1"});
      REQUIRE(db1->matchPattern("*Pest*", "user1", vector<string>{}, false, false) ==  vector<string>{});
      REQUIRE(db2->matchPattern("T*T2", "user2", vector<string>{}, false, false) ==  vector<string>{"user2-TEST2"});
      // TODO: test for groups and deleted workspaces
    }

    SECTION("read entry") {
      REQUIRE( db1->readEntry("user1-TEST1", false) != nullptr);

    }
 

}