#include <bmcweb_config.h>
#include <systemd/sd-daemon.h>

#include <app.hpp>
#include <boost/asio/io_context.hpp>
#include <cors_preflight.hpp>
#include <dbus_monitor.hpp>
#include <dbus_singleton.hpp>
#include <event_dbus_monitor.hpp>
#include <google/google_service_root.hpp>
#include <hostname_monitor.hpp>
#include <ibm/management_console_rest.hpp>
#include <image_upload.hpp>
#include <kvm_websocket.hpp>
#include <login_routes.hpp>
#include <nbd_proxy.hpp>
#include <obmc_console.hpp>
#include <obmc_hypervisor.hpp>
#include <obmc_shell.hpp>
#include <openbmc_dbus_rest.hpp>
#include <redfish.hpp>
#ifdef BMCWEB_ENABLE_REDFISH_AGGREGATION
#include <redfish_aggregator.hpp>
#endif
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <security_headers.hpp>
#include <ssl_key_handler.hpp>
#include <user_monitor.hpp>
#include <vm_websocket.hpp>
#include <webassets.hpp>

#include <exception>
#include <memory>
#include <string>

constexpr int defaultPort = 18080;

inline void setupSocket(crow::App& app)
{
    int listenFd = sd_listen_fds(0);
    if (1 == listenFd)
    {
        BMCWEB_LOG_INFO << "attempting systemd socket activation";
        if (sd_is_socket_inet(SD_LISTEN_FDS_START, AF_UNSPEC, SOCK_STREAM, 1,
                              0) != 0)
        {
            BMCWEB_LOG_INFO << "Starting webserver on socket handle "
                            << SD_LISTEN_FDS_START;
            app.socket(SD_LISTEN_FDS_START);
        }
        else
        {
            BMCWEB_LOG_INFO
                << "bad incoming socket, starting webserver on port "
                << defaultPort;
            app.port(defaultPort);
        }
    }
    else
    {
        BMCWEB_LOG_INFO << "Starting webserver on port " << defaultPort;
        app.port(defaultPort);
    }
}

static int run()
{
    // If user has enabled logging, set level at debug so we get everything
#ifdef BMCWEB_ENABLE_LOGGING
    crow::Logger::setLogLevel(crow::LogLevel::Debug);
#else
    // otherwise just enable the error logging
    crow::Logger::setLogLevel(crow::LogLevel::Error);
#endif

    auto io = std::make_shared<boost::asio::io_context>();
    App app(io);

    sdbusplus::asio::connection systemBus(*io);
    crow::connections::systemBus = &systemBus;

    // Static assets need to be initialized before Authorization, because auth
    // needs to build the whitelist from the static routes

#ifdef BMCWEB_ENABLE_STATIC_HOSTING
    crow::webassets::requestRoutes(app);
#endif

#ifdef BMCWEB_ENABLE_KVM
    crow::obmc_kvm::requestRoutes(app);
#endif

#ifdef BMCWEB_ENABLE_REDFISH
    redfish::RedfishService redfish(app);

    // Create EventServiceManager instance and initialize Config
    redfish::EventServiceManager::getInstance();

#ifdef BMCWEB_ENABLE_REDFISH_AGGREGATION
    // Create RedfishAggregator instance and initialize Config
    redfish::RedfishAggregator::getInstance();
#endif
#endif

#ifdef BMCWEB_ENABLE_DBUS_REST
    crow::image_upload::requestRoutes(app);
    crow::openbmc_mapper::requestRoutes(app);
#endif

#ifdef BMCWEB_ENABLE_EVENT_SUBSCRIPTION_WEBSOCKET
    crow::dbus_monitor::requestRoutes(app);
#endif

#ifdef BMCWEB_ENABLE_HOST_SERIAL_WEBSOCKET
    crow::obmc_console::requestRoutes(app);
#endif

#ifdef BMCWEB_ENABLE_BMC_SHELL_WEBSOCKET
    crow::obmc_shell::requestRoutes(app);
#endif

#ifdef BMCWEB_ENABLE_HYPERVISOR_SERIAL_WEBSOCKET
    crow::obmc_hypervisor::requestRoutes(app);
#endif

#ifdef BMCWEB_ENABLE_VM_WEBSOCKET
    crow::obmc_vm::requestRoutes(app);
#endif

#ifdef BMCWEB_ENABLE_IBM_MANAGEMENT_CONSOLE
    crow::ibm_mc::requestRoutes(app);
    crow::ibm_mc_lock::Lock::getInstance();
    // Start BMC and Host state change dbus monitor
    crow::dbus_monitor::registerStateChangeSignal();
    // Start Dump created signal monitor for BMC and System Dump
    crow::dbus_monitor::registerDumpUpdateSignal();
    // Start BIOS Attr change dbus monitor
    crow::dbus_monitor::registerBIOSAttrUpdateSignal();
    // Start event log entry created monitor
    crow::dbus_monitor::registerEventLogCreatedSignal();
    // Start PostCode change signal
    crow::dbus_monitor::registerPostCodeChangeSignal();
    // Start hypervisor app dbus monitor for hypervisor
    // network configurations
    crow::dbus_monitor::registerVMIConfigChangeSignal();
    // Start Platform and Partition SAI state change monitor
    crow::dbus_monitor::registerSAIStateChangeSignal();

#endif

#ifdef BMCWEB_ENABLE_GOOGLE_API
    crow::google_api::requestRoutes(app);
#endif

    if (bmcwebInsecureDisableXssPrevention != 0)
    {
        cors_preflight::requestRoutes(app);
    }

    crow::login_routes::requestRoutes(app);

    setupSocket(app);

#ifdef BMCWEB_ENABLE_VM_NBDPROXY
    crow::nbd_proxy::requestRoutes(app);
#endif

#ifndef BMCWEB_ENABLE_REDFISH_DBUS_LOG_ENTRIES
    int rc = redfish::EventServiceManager::startEventLogMonitor(*io);
    if (rc != 0)
    {
        BMCWEB_LOG_ERROR << "Redfish event handler setup failed...";
        return rc;
    }
#endif

#ifdef BMCWEB_ENABLE_SSL
    BMCWEB_LOG_INFO << "Start Hostname Monitor Service...";
    crow::hostname_monitor::registerHostnameSignal();
#endif

    bmcweb::registerUserRemovedSignal();

    app.run();
    io->run();

    crow::connections::systemBus = nullptr;

    return 0;
}

int main(int /*argc*/, char** /*argv*/)
{
    try
    {
        return run();
    }
    catch (const std::exception& e)
    {
        BMCWEB_LOG_CRITICAL << "Threw exception to main: " << e.what();
        return -1;
    }
    catch (...)
    {
        BMCWEB_LOG_CRITICAL << "Threw exception to main";
        return -1;
    }
}
