#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include <catch2/catch_test_macros.hpp>
#include <string>
#include "../src/config.h"


bool debugflag = false;
bool traceflag = false;




TEST_CASE( "config file", "[config]" ) {

    SECTION("canFind") {
        REQUIRE(canFind(std::vector<string>{"a","b"},string("a")) == true);
        REQUIRE(canFind(std::vector<string>{"a","b"},string("b")) == true);
        REQUIRE(canFind(std::vector<string>{"a"},string("a")) == true);
        REQUIRE(canFind(std::vector<string>{"a","b"},string("c")) == false);
        REQUIRE(canFind(std::vector<string>{"a"},string("c")) == false);
        REQUIRE(canFind(std::vector<string>{},string("c")) == false);
    }

    SECTION("validFilesystems" ) {
        auto config =  Config(std::string(R"(
default: third
admins: [e]
dbuid: 0
dbgid: 0
workspaces:
    first:
        user_acl: [+a,-b,d]
        groupdefault: [gb]
        database: /tmp
        spaces: [/tmp]
        deleted: .del
    fourth:
        database: /tmp
        spaces: [/tmp]
        deleted: .del
filesystems: 
    second:
        userdefault: [a]
        database: /tmp
        spaces: [/tmp]
        deleted: .del
    third:
        userdefault: [z,y]
        database: /tmp
        spaces: [/tmp]
        deleted: .del
)"));

        // see if workspace and filesystems are used
        REQUIRE(config.validFilesystems("c", std::vector<string>{}) == std::vector<string>{"third","fourth","second"} );
        // see if userdefault and acl works
        REQUIRE(config.validFilesystems("a", std::vector<string>{}) == std::vector<string>{"second","third","first","fourth"});
        // see if ACL works, default does not override ACL
        REQUIRE(config.validFilesystems("b", std::vector<string>{"gb"}) == std::vector<string>{"third", "fourth","second"});
        // global, groupdefault, others
        REQUIRE(config.validFilesystems("d", std::vector<string>{"gb"}) == std::vector<string>{"first","third","fourth","second"});
        // userdefault first, group second, multiple groups
        REQUIRE(config.validFilesystems("a", std::vector<string>{"gc","gb"}) == std::vector<string>{"second","first","third","fourth"});
        // user first, others, no denied, not only for first user, multiple groups
        REQUIRE(config.validFilesystems("y", std::vector<string>{"ga","gc"}) == std::vector<string>{"third","fourth","second"});
        // admin user check, sees all filesystems
        REQUIRE(config.validFilesystems("e", std::vector<string>{}) == std::vector<string>{"third","first","fourth", "second"});
    }

    SECTION( "hasAccess") {
        auto config =  Config(std::string(R"(
admins: [d]
dbuid: 0
dbgid: 0
workspaces:
    testacl:
        user_acl: [+a,-b]
        group_acl: [+bg]
        database: /tmp
        spaces: [/tmp]
        deleted: .del
filesystems:
    testnoacl:
        database: /tmp
        spaces: [/tmp]
        deleted: .del

)"));

        // workspace no acl
        REQUIRE(config.hasAccess("a", std::vector<string>{"cg","dg"},"testnoacl") == true);
        // workspace with acl, through user
        REQUIRE(config.hasAccess("a", std::vector<string>{"cg","dg"},"testacl") == true);
        // workspace with acl, unknown user
        REQUIRE(config.hasAccess("c", std::vector<string>{},"testacl") == false);
        // workspace with acl, through group
        REQUIRE(config.hasAccess("c", std::vector<string>{"bg"},"testacl") == true);
        // workspace with acl, through group but forbidden as user
        REQUIRE(config.hasAccess("b", std::vector<string>{"bg"},"testacl") == false);
        // admin user
        REQUIRE(config.hasAccess("d", std::vector<string>{""},"testacl") == true);

    }

    SECTION("isAdmin") {
        auto config =  Config(std::string(R"(
admins: [d]
dbuid: 0
dbgid: 0
workspaces:
    testacl:
        user_acl: [+a,-b]
        group_acl: [+bg]
        database: /tmp
        spaces: [/tmp]
        deleted: .del
filesystems:
    testnoacl:
        database: /tmp
        spaces: [/tmp]
        deleted: .del

)"));

        REQUIRE(config.hasAccess("d", std::vector<string>{""},"testacl") == true);
    }
 }

