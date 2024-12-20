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

bool debugflag = false;
bool traceflag = false;

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
    wsentry1.close();    

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
    wsentry2.close();  

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
    wsentry3.close();  

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
    wsentry4.close();  

    // create a DB entry
    std::ofstream wsentry5(ws2dbname / "user2-TEST2"); 
    fmt::println(wsentry5, 
R"yaml(
workspace: /a/path22
expiration: 1734701876
extensions: 3
reminder: 0
mailaddress: ""
comment: ""
)yaml");
    wsentry5.close();  

    // create a BROKEN DB entry
    std::ofstream wsentry6(ws2dbname / "user3-BROKEN"); 
    fmt::print(wsentry6, 
R"yaml(works)yaml");
    wsentry6.close();  

    // create a BROKEN DB entry
    std::ofstream wsentry7(ws2dbname / "user3-BROKEN2"); 
    fmt::print(wsentry7, 
R"yaml(yaml: dabadu
)yaml");
    wsentry7.close();  

    auto config = Config(std::vector<fs::path>{basedirname / "ws.conf"});

    std::unique_ptr<Database> db1(config.openDB("ws1"));
    std::unique_ptr<Database> db2(config.openDB("ws2"));

    SECTION("list entries") {
        REQUIRE(db1->matchPattern("TEST*", "user1", vector<string>{}, false, false) ==  vector<string>{"user1-TEST1"});
        REQUIRE(db1->matchPattern("*EST*", "user2", vector<string>{}, false, false) ==  vector<string>{"user2-TEST2", "user2-TEST1"});
        REQUIRE(db1->matchPattern("*", "user2", vector<string>{}, false, false) ==  vector<string>{"user2-TEST2", "user2-TEST1"});
        REQUIRE(db1->matchPattern("*Pest*", "user1", vector<string>{}, false, false) ==  vector<string>{});
        REQUIRE(db2->matchPattern("T*T2", "user2", vector<string>{}, false, false) ==  vector<string>{"user2-TEST2"});

        // TODO: test for groups and deleted workspaces
    }

    SECTION("read entry") {
      
        std::unique_ptr<DBEntry> entry(db1->readEntry("user1-TEST1", false));
        REQUIRE( entry != nullptr);
        REQUIRE( entry->getExpiration() == 1734701876);
        REQUIRE( entry->getWSPath() == "/a/path");

        // does not exist
        REQUIRE_THROWS( [&]() {
            std::unique_ptr<DBEntry> entry2(db1->readEntry("user-TEST", false));
        }() );

        // is no yaml
        REQUIRE_THROWS( [&]() {
            std::unique_ptr<DBEntry> entry3(db2->readEntry("user3-BROKEN", false));
        }() );

        // is yaml, but has no known entries, should give default
        std::unique_ptr<DBEntry> entry4(db2->readEntry("user3-BROKEN2", false));
        REQUIRE( entry4->getExtension() == 0);

    }
 
}
