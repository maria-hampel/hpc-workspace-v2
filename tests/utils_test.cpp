#include <filesystem>
#define CATCH_CONFIG_MAIN // This tells Catch to provide a main() - only do this in one cpp file
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "../src/utils.h"
#include "../src/ws.h"

namespace fs = std::filesystem;

bool debugflag = false;
bool traceflag = false;

TEST_CASE("utils", "[utils]")
{
    SECTION("parceACL")
    {
        auto m = utils::parseACL({"-a", "+abc:list,create", "-z", "c", "-y"});
        REQUIRE(m["a"].first == "-");
        REQUIRE(m["abc"].first == "+");
        REQUIRE(m["c"].first == "+");
        REQUIRE(m["y"].first == "-");
        REQUIRE(m["a"].second.size() == 0);
        REQUIRE(m["abc"].second.size() == 2);
        REQUIRE(m["c"].second.size() == 0);
        REQUIRE(m["abc"].second == std::vector<int>{ws::LIST, ws::CREATE});
    }

    SECTION("rmtree")
    {
        fs::create_directories("/tmp/_wsTT/a/a/a/a");
        fs::create_directories("/tmp/_wsTT/a/a/a/b");
        REQUIRE( fs::exists("/tmp/_wsTT") );
        utils::rmtree("/tmp/_wsTT");
        REQUIRE(fs::exists("/tmp/_wsTT") == false);
    }

    SECTION("prettySize")
    {
        REQUIRE(utils::prettyBytes(100) == "100 B");
        REQUIRE(utils::prettyBytes(1000) == "1 KB");
        REQUIRE(utils::prettyBytes(1500000) == "1.5 MB");
    }

    SECTION("trimright")
    {
        REQUIRE(utils::trimright("- \n") == std::string("-"));
        REQUIRE(utils::trimright(std::string("- \n")) == std::string("-"));
        REQUIRE(utils::trimright(" ") == "");
    }
}
