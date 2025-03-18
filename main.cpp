
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

#include <Poco/Thread.h>
#include <Poco/Util/Option.h>
#include <Poco/Util/OptionSet.h>
#include <Poco/Util/Subsystem.h>

#include <zmq.hpp>

#include "monster_generated.h"
#include <flatbuffers/flatbuffers.h>

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

using Poco::Thread;
using Poco::Util::Subsystem;

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
            Poco::AutoPtr<Notification> pNf(_queue.waitDequeueNotification(3000));
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

class MessageInterceptor
{
  public:
    void intercept(const zmq::message_t &topic, const zmq::message_t &message)
    {
        std::cout << "Intercepted: [" << topic.to_string() << "] " << message.to_string() << std::endl;
    }
};

class ZmqTask : public Task
{
  public:
    ZmqTask() : Task{"ZmqTask"}, m_context(1), m_xsub(m_context, ZMQ_XSUB), m_xpub(m_context, ZMQ_XPUB), m_interceptor()
    {
        m_xsub.bind("tcp://*:5555");
        m_xsub.set(zmq::sockopt::rcvtimeo, 1000);

        m_xpub.bind("tcp://*:5556");
    }

    void runTask() override
    {
        Application &app = Application::instance();
        app.logger().information("zmq task uptime: " + DateTimeFormatter::format(app.uptime()));

        while (!isCancelled())
        {
            zmq::message_t topic_msg, message_msg;
            m_xsub.recv(topic_msg, zmq::recv_flags::none);
            m_xsub.recv(message_msg, zmq::recv_flags::none);

            m_interceptor.intercept(topic_msg, message_msg);

            m_xpub.send(topic_msg, zmq::send_flags::sndmore);
            m_xpub.send(message_msg, zmq::send_flags::none);
        }
    }

  private:
    zmq::context_t m_context;
    zmq::socket_t m_xsub;
    zmq::socket_t m_xpub;
    MessageInterceptor m_interceptor;
};

class MySubsystem : public Subsystem
{
  public:
    MySubsystem(NotificationQueue *queue) : _parameterValue(""), m_zmqTask{}, m_thread{}
    {
    }

    const char *name() const override
    {
        return "MySubsystem";
    }

    void initialize(Application &app) override
    {
        std::cout << "MySubsystem initialized with parameter: " << _parameterValue << std::endl;
        m_thread.start(m_zmqTask);
    }

    void uninitialize() override
    {
        m_zmqTask.cancel();
        m_thread.join();
        std::cout << "MySubsystem uninitialized" << std::endl;
    }

    void defineOptions(OptionSet &options) override
    {
        Subsystem::defineOptions(options);
        options.addOption(Option("myparam", "p", "Set a parameter for MySubsystem")
                              .required(false)
                              .repeatable(false)
                              .argument("value")
                              .callback(OptionCallback<MySubsystem>(this, &MySubsystem::handleParameter)));
    }

    void handleParameter(const std::string &name, const std::string &value)
    {
        _parameterValue = value;
        std::cout << "Parameter '" << name << "' set to: " << value << std::endl;
    }

  private:
    std::string _parameterValue;
    ZmqTask m_zmqTask;
    Thread m_thread;
};

class SampleServer : public ServerApplication
{
  public:
    SampleServer() : mHelpRequested{false}, m_tm{}, m_queue{}
    {
        addSubsystem(new MySubsystem(&m_queue));
    }

    ~SampleServer()
    {
    }

  protected:
    void initialize(Application &self) override
    {
        logger().information("starting up");
        loadConfiguration(); // load default configuration files, if present
        ServerApplication::initialize(self);
    }

    void uninitialize() override
    {
        ServerApplication::uninitialize();
        logger().information("shutting down");
    }

    void defineOptions(OptionSet &options) override
    {
        ServerApplication::defineOptions(options);
        options.addOption(Option("help", "h", "display help information on command line arguments")
                              .required(false)
                              .repeatable(false)
                              .callback(OptionCallback<SampleServer>(this, &SampleServer::handleHelp)));

        // options.addOption(Option("myparam", "p", "Set a parameter for MySubsystem")
        //                       .required(false)
        //                       .repeatable(false)
        //                       .argument("value")
        //                       .callback(OptionCallback<SampleServer>(this, &SampleServer::handleParameter)));
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

    void handleParameter(const std::string &name, const std::string &value)
    {
        std::cout << "Parameter '" << name << "' set to: " << value << std::endl;
    }

    int main(const ArgVec &args) override
    {
        // MyGame::MonsterT monster;
        // monster.mana = 150;
        // monster.hp = 100;
        // monster.name = "Orc";

        // flatbuffers::FlatBufferBuilder builder;
        // builder.Finish(MyGame::Monster::Pack(builder, &monster));

        // std::cout << "============>:" << builder.GetSize() << ", name:" << monster.name << std::endl;

        flatbuffers::FlatBufferBuilder builder;

        static const int32_t weapon_ids[] = {1, 2, 3, 4};

        auto inventory = builder.CreateVector(weapon_ids, 4); // 创建向量

        auto name = builder.CreateString("Orc");

        MyGame::MonsterBuilder monster_builder(builder);

        monster_builder.add_name(name);

        auto monster = monster_builder.Finish();

        builder.Finish(monster);

        auto monster_data = flatbuffers::GetRoot<MyGame::Monster>(builder.GetBufferPointer());

        std::cout << "Monster Name: " << monster_data->name()->c_str() << std::endl;

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
            m_tm.start(new ProducerTask(m_queue));
            m_tm.start(new ConsumerTask(m_queue));

            // 等待终止请求 Ctrl+C
            waitForTerminationRequest();

            // 取消所有任务
            m_tm.cancelAll();
            m_tm.joinAll();
        }

        return Application::EXIT_OK;
    }

  private:
    bool mHelpRequested;
    TaskManager m_tm;
    NotificationQueue m_queue;
};

POCO_SERVER_MAIN(SampleServer)
