
#include <iostream>

#include "Poco/DateTime.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/DateTimeParser.h"
#include "Poco/Environment.h"
#include "Poco/LocalDateTime.h"
#include "Poco/Notification.h"
#include "Poco/NotificationQueue.h"
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
using Poco::Notification;
using Poco::NotificationQueue;
using Poco::Task;
using Poco::TaskManager;
using Poco::Util::Application;
using Poco::Util::HelpFormatter;
using Poco::Util::Option;
using Poco::Util::OptionCallback;
using Poco::Util::OptionSet;
using Poco::Util::ServerApplication;

using Poco::Environment;

class SampleNotification : public Notification
{
  public:
    SampleNotification(const std::string &message) : _message(message)
    {
    }
    const std::string &message() const
    {
        return _message;
    }

  private:
    std::string _message;
};

class ProducerTask : public Task
{
  public:
    ProducerTask(NotificationQueue &queue) : Task("ProducerTask"), _queue(queue)
    {
    }

    void runTask() override
    {
        for (int i = 0; i < 10; ++i)
        {
            std::string message = "Message " + std::to_string(i);
            _queue.enqueueNotification(new SampleNotification(message));
            sleep(1000);
        }
    }

  private:
    NotificationQueue &_queue;
};

class ConsumerTask : public Task
{
  public:
    ConsumerTask(NotificationQueue &queue) : Task("ConsumerTask"), _queue(queue)
    {
    }

    void runTask() override
    {
        while (!isCancelled())
        {
            Poco::AutoPtr<Notification> pNf(_queue.waitDequeueNotification());
            if (pNf)
            {
                SampleNotification *pSampleNf = dynamic_cast<SampleNotification *>(pNf.get());
                if (pSampleNf)
                {
                    Application::instance().logger().information("Received: " + pSampleNf->message());
                }
            }
        }
    }

  private:
    NotificationQueue &_queue;
};

class SampleTask : public Task
{
  public:
    SampleTask() : Task{"SampleTask"}
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
    SampleServer() : mHelpRequested{false}
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
                              .callback(OptionCallback<SampleServer>(this, &SampleServer::handleHelp)));
    }

    void handleHelp(const std::string &name, const std::string &value)
    {
        mHelpRequested = true;
        displayHelp();
        stopOptionsProcessing();
    }

    void displayHelp()
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
        if (!mHelpRequested)
        {
            // 简单打印时间
            LocalDateTime now;
            std::string str = DateTimeFormatter::format(now, DateTimeFormat::ISO8601_FORMAT);
            std::cout << str << std::endl;

            DateTime dt;
            int tzd;
            DateTimeParser::parse(DateTimeFormat::ISO8601_FORMAT, str, dt, tzd);
            dt.makeUTC(tzd);
            LocalDateTime ldt(tzd, dt);

            // 单例模式
            Application &app = Application::instance();
            // 应用程序可执行文件的绝对路径
            app.logger().information("<application path>: " + app.config().getString("application.path"));
            // 应用程序可执行文件的文件名
            app.logger().information("<application name>: " + app.config().getString("application.name"));
            // 应用程序可执行文件的文件名（不包括扩展名
            app.logger().information("<application base name>: " + app.config().getString("application.baseName"));
            // 应用程序可执行文件所在目录的路径
            app.logger().information("<application dir>: " + app.config().getString("application.dir"));
            // 用户配置文件所在目录的路径，如果没有配置文件则为Roaming目录
            app.logger().information("<application config dir>: " + app.config().getString("application.configDir"));
            // 用户指定的应用程序非必要数据文件存放目录的路径
            app.logger().information("<application cache dir>: " + app.config().getString("application.cacheDir"));
            // 应用程序的用户特定数据文件存放目录的路径
            app.logger().information("<application data dir>: " + app.config().getString("application.dataDir"));
            // 存放用户临时文件和应用程序其他文件对象的目录路径
            app.logger().information("<application temp dir>: " + app.config().getString("application.tempDir"));

            // Environment
            app.logger().information("<os name>: " + Environment::osName());
            app.logger().information("<os display name>: " + Environment::osDisplayName());
            app.logger().information("<os version>: " + Environment::osVersion());
            app.logger().information("<os architecture>: " + Environment::osArchitecture());
            app.logger().information("<node name>: " + Environment::nodeName());
            app.logger().information("<node id>: " + Environment::nodeId());

#if POCO_OS == POCO_OS_WINDOWS_NT
            if (Environment::has("USERPROFILE"))
            {
                app.logger().information("<home>: " + Environment::get("USERPROFILE"));
            }

            if (Environment::has("USERNAME"))
            {
                app.logger().information("<user>: " + Environment::get("USERNAME"));
            }
#elif POCO_OS == POCO_OS_LINUX
            if (Environment::has("HOME"))
            {
                app.logger().information("<home>: " + Environment::get("HOME"));
            }

            if (Environment::has("USER"))
            {
                app.logger().information("<user>: " + Environment::get("USER"));
            }
#endif

            // 任务管理器中添加一个任务
            TaskManager tm;
            // tm.start(new SampleTask);

            NotificationQueue queue;
            tm.start(new ProducerTask(queue));
            tm.start(new ConsumerTask(queue));

            // 等待终止请求 Ctrl+C
            waitForTerminationRequest();

            // 取消所有任务
            tm.cancelAll();
            tm.joinAll();
        }

        return Application::EXIT_OK;
    }

  private:
    bool mHelpRequested;
};

POCO_SERVER_MAIN(SampleServer)
