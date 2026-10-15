#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>

#include "libslic3r/PrintConfig.hpp"
#include "slic3r/Utils/Moonraker.hpp"

using Slic3r::Moonraker;

TEST_CASE("The Moonraker upload response yields the stored path", "[Moonraker]")
{
    SECTION("Moonraker's own response has no result envelope") {
        // Recorded from Moonraker v0.11.0 on the virtual printer rig, path in a subdirectory.
        const std::string body = R"({"action":"create_file","item":{"modified":1792056844.82,"size":335653,)"
                                 R"("permissions":"rw","path":"parts/klipslice cube.gcode","root":"gcodes"},)"
                                 R"("print_started":false,"print_queued":false})";
        CHECK(Moonraker::uploaded_path(body) == "parts/klipslice cube.gcode");
    }
    SECTION("an enveloped response is accepted") {
        CHECK(Moonraker::uploaded_path(R"({"result":{"item":{"path":"a/b.gcode","root":"gcodes"}}})") == "a/b.gcode");
    }
    SECTION("a response without a path yields nothing") {
        CHECK(Moonraker::uploaded_path(R"({"action":"create_file"})").empty());
    }
    SECTION("a body that is not JSON throws") {
        CHECK_THROWS(Moonraker::uploaded_path("<html>502 Bad Gateway</html>"));
    }
}

TEST_CASE("The Moonraker host takes defaults for print host options a config lacks", "[Moonraker]")
{
    // A full print config carries print_host and printhost_apikey but none of the physical
    // printer's other printhost_* options.
    Slic3r::DynamicPrintConfig config = Slic3r::DynamicPrintConfig::full_print_config();
    config.set_deserialize_strict({{"print_host", "http://127.0.0.1:7125"}, {"printhost_apikey", "key"}});
    config.erase("printhost_cafile");
    config.erase("printhost_ssl_ignore_revoke");
    REQUIRE_FALSE(config.has("printhost_ssl_ignore_revoke"));

    const Moonraker host(&config);
    CHECK(host.get_host() == "http://127.0.0.1:7125");
    CHECK(host.get_apikey() == "key");
    CHECK(host.get_cafile().empty());
}
