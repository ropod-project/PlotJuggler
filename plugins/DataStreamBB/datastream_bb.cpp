#include "datastream_bb.h"
#include <QMessageBox>
#include <QDebug>
#include <thread>
#include <mutex>
#include <chrono>
#include <QProgressDialog>
#include <QtGlobal>
#include <QApplication>
#include <QProcess>
#include <QCheckBox>
#include <QSettings>
#include <QFileDialog>

// #include "../ROS/dialog_select_ros_topics.h"
// #include "../ROS/rule_editing.h"
// #include "../ROS/qnodedialog.h"
// #include "../ROS/shape_shifter_factory.hpp"

#include "dialog_select_bb_variables.h"

#include <math.h>

/**
 * Copied from the ropod_com_mediator component:
 * https://git.ropod.org/ropod/communication/ropod_com_mediator/blob/master/src/com_mediator.cpp
 */
std::string getEnv(const std::string &var)
{
    char const* value = std::getenv(var.c_str());
    if (value == NULL)
    {
        std::cerr << "Warning: environment variable " << var << " not set!" << std::endl;
        return std::string();
    }
    else
    {
        return std::string(value);
    }
}

DataStreamBB::DataStreamBB():
    DataStreamer(), 
    ZyreBaseCommunicator(getEnv("ROPOD_ID"), false, "", true, false),
    _destination_data(nullptr),
    _prev_clock_time(0)
{
    _running = false;
    _periodic_timer = new QTimer();
    // connect( _periodic_timer, &QTimer::timeout,
    //          this, &DataStreamBB::timerCallback);                        // <-- Removing this seems to have fixed an undefined symbol issue

    loadDefaultSettings();

    myUUID = this->generateUUID();
    this->startZyreNode();

    std::map<std::string, std::string> headers;
    headers["name"] = getEnv("ROPOD_ID") + std::string("_bb_streaming_plugin");
    this->setHeaders(headers);

    this->joinGroup("ROPOD");
}

bool DataStreamBB::start(QStringList* selected_datasources)
{
    {
        std::lock_guard<std::mutex> lock( mutex() );
        dataMap().numeric.clear();
        dataMap().user_defined.clear();
    }

    this->queryVariableListFromBB();
    std::this_thread::sleep_for ( std::chrono::milliseconds(100) );
	if (BBVariableList.size() == 0)
	{
		QMessageBox::warning(nullptr, "No Black Box Data Received",
            "Could not connect with the black-box. Please try again.");
        return false;
	}

    std::vector<std::pair<QString,QString>> all_variables;

    for(auto variable_name : BBVariableList)
    {
        all_variables.push_back(std::make_pair(variable_name, QString("dummy type") ) );
    }

    QTimer timer;
    timer.setSingleShot(false);
    timer.setInterval( 1000);
    timer.start();

    DialogSelectBBVariables dialog( all_variables, _config );

    connect( &timer, &QTimer::timeout, [&]()
    {
        all_variables.clear();
        for(auto variable_name : BBVariableList)
        {
            all_variables.push_back(std::make_pair(variable_name, QString("dummy type")));
        }
        dialog.updateVariableList(all_variables);
    });

    int res = dialog.exec();

    _config = dialog.getResult();

    timer.stop();

    if( res != QDialog::Accepted || _config.selected_variables.empty() )
    {
        return false;
    }

    initializeDataMap();

    _running = true;

    _periodic_timer->setInterval(500);
    _periodic_timer->start();

    _thread = std::thread([this](){ this->streamingLoop();} );

    return true;
}

bool DataStreamBB::isRunning() const { return _running; }

void DataStreamBB::shutdown()
{
    _periodic_timer->stop();
    _running = false;

    if( _thread.joinable()) 
    {
        _thread.join();
    }
}

DataStreamBB::~DataStreamBB()
{
    shutdown();
}

bool DataStreamBB::xmlSaveState(QDomDocument &doc, QDomElement &plugin_elem) const
{
    return true;
}

bool DataStreamBB::xmlLoadState(const QDomElement &parent_element)
{
    return true;
}

void DataStreamBB::saveDefaultSettings()
{
    QSettings settings;

    settings.setValue("DataStreamBB/default_topics", _config.selected_variables);
}


void DataStreamBB::loadDefaultSettings()
{
    QSettings settings;

    _config.selected_variables = settings.value("DataStreamBB/default_topics", false ).toStringList();
}

void DataStreamBB::queryVariableListFromBB()
{
    Json::Value query_msg;

    query_msg["header"]["type"] = "VARIABLE-QUERY";
    query_msg["payload"]["blackBoxId"] = "black_box_001";
    query_msg["payload"]["senderId"] = this->myUUID;

    std::stringstream feedbackMsg("");
    feedbackMsg << query_msg;
    this->shout(feedbackMsg.str(), "ROPOD");
}

void DataStreamBB::queryLatestVariableValuesFromBB()
{
    Json::Value query_msg;
    Json::Value variable_list;

    query_msg["header"]["type"] = "LATEST-DATA-QUERY";
    query_msg["payload"]["blackBoxId"] = "black_box_001";
    query_msg["payload"]["senderId"] = this->myUUID;
    query_msg["header"]["msgId"] = this->generateUUID();

    for (auto variable : _config.selected_variables)
        variable_list.append(variable.toStdString());

    query_msg["payload"]["variables"] = variable_list;

    std::stringstream feedbackMsg("");
    feedbackMsg << query_msg;
    this->shout(feedbackMsg.str(), "ROPOD");
}

/**
 * Adapted from the ropod_com_mediator component:
 * https://git.ropod.org/ropod/communication/ropod_com_mediator/blob/master/src/com_mediator.cpp
 */
void DataStreamBB::recvMsgCallback(ZyreMsgContent *msgContent)
{
	std::stringstream msg;
	msg << msgContent->message;
	
	Json::Value root;
	std::string errors;
	bool ok = Json::parseFromStream(json_builder, msg, &root, &errors);

	if (root["payload"]["receiverId"] == this->myUUID)
	{
        if (root["header"]["type"] == "VARIABLE-QUERY")
        {
            for (auto variableGroup : root["payload"]["variableList"])
            {
                for (auto variable : variableGroup)
                {
                    BBVariableList.push_back(QString::fromStdString(variable.asString()));
                }
            }
        }
        else if (root["header"]["type"] == "LATEST-DATA-QUERY")
        {
            std::string timestamp_value_str;
            std::string timestamp_str;
            std::string value_str;

            for (auto variableName : root["payload"]["dataList"].getMemberNames())
            {
                timestamp_value_str = root["payload"]["dataList"][variableName].asString();
                timestamp_value_str = timestamp_value_str.substr(1, timestamp_value_str.length() - 2);

                size_t pos = timestamp_value_str.find(",");
                timestamp_str = timestamp_value_str.substr(0, pos);
                value_str = timestamp_value_str.substr(pos+2, timestamp_value_str.length());

                float timestamp = std::stof(timestamp_str);
                float value = std::stof(value_str);

                BBVariableValues[variableName] = value;
            }
        }
  	}
}

void DataStreamBB::streamingLoop()
{
    _running = true;
    while( _running )
    {
        singleCycle();
        std::this_thread::sleep_for ( std::chrono::milliseconds(100) );
    }
}

void DataStreamBB::singleCycle()
{
    std::lock_guard<std::mutex> lock( mutex() );
    queryLatestVariableValuesFromBB();

    using namespace std::chrono;
    static std::chrono::high_resolution_clock::time_point initial_time = high_resolution_clock::now();
    const double offset = duration_cast< duration<double>>( initial_time.time_since_epoch() ).count() ;

    auto now =  high_resolution_clock::now();
    for (auto& it: dataMap().numeric )
    {
        if( it.first == "empty") continue;

        auto& plot = it.second;
        const double t = duration_cast< duration<double>>( now - initial_time ).count() ;
        double variable_value = BBVariableValues.find(it.first)->second;

        plot.pushBack( PlotData::Point( t + offset, variable_value ) );
    }

    // for (auto i = BBVariableValues.begin(); i != BBVariableValues.end(); ++i)
    // {
    //     std::cout << "Variable: " << i->first << ", Value: " << BBVariableValues.find(i->first)->second << std::endl;
    // }
}

void DataStreamBB::initializeDataMap()
{
    std::string variable_name;
    for (auto i = _config.selected_variables.begin(); i != _config.selected_variables.end(); ++i)
    {
        QString qstring = *i;
        variable_name = qstring.toStdString();

		PlotData::Point data_point;

		auto plot_pair = dataMap().addNumeric( variable_name );
		
		PlotData& plot_raw = plot_pair->second;
		plot_raw.pushBack( data_point );
    }

    singleCycle();
}