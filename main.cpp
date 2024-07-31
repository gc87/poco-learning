
#include <iostream>

#include "Poco/DateTime.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/DateTimeParser.h"
#include "Poco/LocalDateTime.h"
#include "Poco/Task.h"
#include "Poco/TaskManager.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/ServerApplication.h"

using Poco::DateTime;
using Poco::DateTimeFormat;
using Poco::DateTimeFormatter;
using Poco::DateTimeParser;
using Poco::LocalDateTime;

using Poco::DateTimeFormatter;
using Poco::Task;
using Poco::TaskManager;
using Poco::Util::Application;
using Poco::Util::HelpFormatter;
using Poco::Util::Option;
using Poco::Util::OptionCallback;
using Poco::Util::OptionSet;
using Poco::Util::ServerApplication;

class SampleTask : public Task
{
  public:
    SampleTask() : Task("SampleTask")
    {
    }

    void runTask() override
    {
        Application &app = Application::instance();
        while (!sleep(5000))
        {
            // app.logger().information("busy doing nothing... " + DateTimeFormatter::format(app.uptime()));

            app.logger().information("application uptime: " + DateTimeFormatter::format(app.uptime()));
        }
    }
};

class SampleServer : public ServerApplication
{
  public:
    SampleServer() : m_help_requested(false)
    {
    }

    ~SampleServer()
    {
    }

  protected:
    void initialize(Application &self) override
    {
        loadConfiguration(); // load default configuration files, if present
        ServerApplication::initialize(self);
        logger().information("starting up");
    }

    void uninitialize() override
    {
        logger().information("shutting down");
        ServerApplication::uninitialize();
    }

    void defineOptions(OptionSet &options) override
    {
        ServerApplication::defineOptions(options);
        options.addOption(Option("help", "h", "display help information on command line arguments")
                              .required(false)
                              .repeatable(false)
                              .callback(OptionCallback<SampleServer>(this, &SampleServer::HandleHelp)));
    }

    void HandleHelp(const std::string &name, const std::string &value)
    {
        m_help_requested = true;
        DisplayHelp();
        stopOptionsProcessing();
    }

    void DisplayHelp()
    {
        HelpFormatter helpFormatter(options());
        helpFormatter.setCommand(commandName());
        helpFormatter.setUsage("OPTIONS");
        helpFormatter.setHeader(
            "A sample server application that demonstrates some of the features of the Util::ServerApplication class.");
        helpFormatter.format(std::cout);
    }

    int main(const ArgVec &args) override
    {
        if (!m_help_requested)
        {
            LocalDateTime now;
            std::string str = DateTimeFormatter::format(now, DateTimeFormat::ISO8601_FORMAT);
            std::cout << str << std::endl;

            DateTime dt;
            int tzd;
            DateTimeParser::parse(DateTimeFormat::ISO8601_FORMAT, str, dt, tzd);
            dt.makeUTC(tzd);
            LocalDateTime ldt(tzd, dt);

            Application &app = Application::instance();
            app.logger().information("<application path>: " + app.config().getString("application.path"));
            app.logger().information("<application name>: " + app.config().getString("application.name"));
            app.logger().information("<application base name>: " + app.config().getString("application.baseName"));
            app.logger().information("<application dir>: " + app.config().getString("application.dir"));
            app.logger().information("<application config dir>: " + app.config().getString("application.configDir"));
            app.logger().information("<application cache dir>: " + app.config().getString("application.cacheDir"));
            app.logger().information("<application data dir>: " + app.config().getString("application.dataDir"));
            app.logger().information("<application temp dir>: " + app.config().getString("application.tempDir"));

            TaskManager tm;
            tm.start(new SampleTask);
            waitForTerminationRequest();
            tm.cancelAll();
            tm.joinAll();
        }
        return Application::EXIT_OK;
    }

  private:
    bool m_help_requested;
};

POCO_SERVER_MAIN(SampleServer)
