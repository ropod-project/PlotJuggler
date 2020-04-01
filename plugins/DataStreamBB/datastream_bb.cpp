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
    _destination_data(nullptr)
{
    _running = false;
    loadDefaultSettings();

    myUUID = this->generateUUID();
    this->startZyreNode();

    std::map<std::string, std::string> headers;
    headers["name"] = getEnv("ROPOD_ID") + std::string("_bb_streaming_plugin");
    this->setHeaders(headers);

    this->joinGroup("ROPOD");

    std::cout.precision(20);
}

bool DataStreamBB::start(QStringList* selected_datasources)
{
    {
        std::lock_guard<std::mutex> lock( mutex() );
        dataMap().numeric.clear();
        dataMap().user_defined.clear();
    }

    if (!queryVariableListFromBB()) return false;

    if (BBVariableList.size() == 0)
    {
        QMessageBox::warning(nullptr, "No Black Box Data Received",
        "Could not receive a variable list from the black box. Try again.");
        return false;
    }

    std::vector<std::pair<QString,QString>> all_variables;

    for(auto variableName : BBVariableList)
    {
        all_variables.push_back(std::make_pair(variableName, QString("dummy type") ) );
    }

    BBVariableList.clear();

    QTimer timer;
    timer.setSingleShot(false);
    timer.setInterval( 1000);
    timer.start();

    DialogSelectBBVariables dialog( all_variables, _config );

    connect( &timer, &QTimer::timeout, [&]()
    {
        dialog.updateVariableList(all_variables);
    });

    int res = dialog.exec();

    _config = dialog.getResult();

    timer.stop();

    if( res != QDialog::Accepted || _config.selected_variables.empty() )
    {
        return false;
    }

    if (!initializeDataMap()) return false;
    instantiateVariableThreads();

    _queryThread = std::thread([this](){ this->queryingLoop();} );

    _running = true;

    _initialTime = std::chrono::high_resolution_clock::now();

    return true;
}

bool DataStreamBB::isRunning() const { return _running; }

void DataStreamBB::shutdown()
{
    _running = false;

    if( _queryThread.joinable()) _queryThread.join();

    for (auto& thread: variableThreads) if(thread.joinable()) thread.join();
}

DataStreamBB::~DataStreamBB()
{
    shutdown();
    emit connectionClosed();
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

bool DataStreamBB::queryVariableListFromBB()
{
    Json::Value query_msg;

    query_msg["header"]["type"] = "VARIABLE-QUERY";
    query_msg["payload"]["blackBoxId"] = "black_box_001";
    query_msg["payload"]["senderId"] = this->myUUID;

    std::stringstream feedbackMsg("");
    feedbackMsg << query_msg;
    this->shout(feedbackMsg.str(), "ROPOD");

    auto start_time = std::chrono::steady_clock::now();
    int queryTimeOut = 2;
    _waitingForBBResponse = true;

    while (_waitingForBBResponse)
    {
        if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count() > queryTimeOut)
        {
            QMessageBox::warning(nullptr, "No Black Box Response",
            "Could not connect with the black-box. Please try again.");
            return false;
        }
    }

    return true;
}

bool DataStreamBB::queryLatestVariableValuesFromBB()
{
    Json::Value query_msg;
    Json::Value variable_list;

    query_msg["header"]["type"] = "LATEST-DATA-QUERY";
    query_msg["header"]["msgId"] = this->generateUUID();
    query_msg["payload"]["blackBoxId"] = "black_box_001";
    query_msg["payload"]["senderId"] = this->myUUID;

    for (auto variable : _config.selected_variables)
        variable_list.append(variable.toStdString());

    query_msg["payload"]["variables"] = variable_list;

    std::stringstream feedbackMsg("");
    feedbackMsg << query_msg;
    this->shout(feedbackMsg.str(), "ROPOD");

    auto start_time = std::chrono::steady_clock::now();
    int queryTimeOut = 2;
    _waitingForBBResponse = true;

    while (_waitingForBBResponse)
    {
        if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count() > queryTimeOut)
        {
            return false;
        }
    }

    return true;
}

void DataStreamBB::queryVariableValuesFromBB()
{
    Json::Value query_msg;
    Json::Value variable_list;

    query_msg["header"]["type"] = "DATA-QUERY";
    query_msg["header"]["msgId"] = this->generateUUID();
    query_msg["payload"]["blackBoxId"] = "black_box_001";
    query_msg["payload"]["senderId"] = this->myUUID;
    query_msg["payload"]["startTime"] = this->_latestTimestamp;
    query_msg["payload"]["endTime"] = "-1";

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
	bool ok = Json::parseFromStream(jsonBuilder, msg, &root, &errors);

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

                double timestamp = std::stod(timestamp_str);
                double value = std::stod(value_str);

                // Store variable name with timestamp and value:
                LatestBBVariableData[variableName].first = timestamp;
                LatestBBVariableData[variableName].second = value;
                
                _latestTimestamp = timestamp;
            }
        }
        else if (root["header"]["type"] == "DATA-QUERY")
        {
            std::string timestamp_value_str;
            std::string timestamp_str;
            std::string value_str;

            for (auto variableName : root["payload"]["dataList"].getMemberNames())
            {
                for (auto variableData : root["payload"]["dataList"][variableName])
                {
                    timestamp_value_str = variableData.asString();
                    timestamp_value_str = timestamp_value_str.substr(1, timestamp_value_str.length() - 2);

                    size_t pos = timestamp_value_str.find(",");
                    timestamp_str = timestamp_value_str.substr(0, pos);
                    value_str = timestamp_value_str.substr(pos+2, timestamp_value_str.length());

                    double timestamp = std::stod(timestamp_str);
                    double value = std::stod(value_str);
                    
                    BBVariableDataMulti[variableName].push_back(std::pair<double, double>(timestamp, value));

                    _latestTimestamp = timestamp;
                }
            }
        }
  	}
    _waitingForBBResponse = false;
}

void DataStreamBB::streamingLoop(std::string variableName)
{
    _running = true;
    while( _running )
    {
        multiMeasurementSingleCycle(variableName);
    }
}

void DataStreamBB::queryingLoop()
{
    double querying_frequency = 1;
    
    while( _running )
    {
        queryVariableValuesFromBB();
        std::this_thread::sleep_for (std::chrono::milliseconds((int((1 / querying_frequency) * 1000))));
    }
}

void DataStreamBB::multiMeasurementSingleCycle(std::string variableName)
{
    using namespace std::chrono;

    if (BBVariableDataMulti[variableName].size() == 0) return;

    double latest_data_timestamp = BBVariableDataMulti[variableName].back().first;
    double latest_previous_data_timestamp = LatestBBVariableData[variableName].first;

    // Note: Sending consecutive data queries where startTime is the latestTimeStamp
    // results in the first entry often being a duplicate of the last of the previous
    // query. However, the next condition filters out such duplicates.
    if (latest_data_timestamp - latest_previous_data_timestamp == 0) return;

    double variable_value;
    double variable_timestamp;

    while (BBVariableDataMulti[variableName].size() != 0)
    {
        auto& plot = dataMap().numeric.find(variableName)->second;
        const double t = duration_cast< duration<double>>( high_resolution_clock::now() - _initialTime ).count();

        auto variable_data = BBVariableDataMulti[variableName].front();
        variable_timestamp = variable_data.first;
        variable_value = variable_data.second;
        // std::cout << "Timestamp: " << variable_timestamp << std::endl;
        // std::cout << "Value: " << variable_value << std::endl << std::endl;

        plot.pushBack( PlotData::Point( t, variable_value ) );

        LatestBBVariableData[variableName] = variable_data;
        BBVariableDataMulti[variableName].pop_front();
        
        if (BBVariableDataMulti[variableName].size() != 0)
        {
            double next_measurement_timestamp = BBVariableDataMulti[variableName].front().first;
            double time_to_next_measurement = next_measurement_timestamp - variable_timestamp;
            
            std::this_thread::sleep_for(milliseconds((int(time_to_next_measurement * 1000))));
        }
    }
}

void DataStreamBB::instantiateVariableThreads()
{
    std::string variableName;
    for (auto variableNameQString : _config.selected_variables)
    {
        variableName = variableNameQString.toStdString();

        // std::cout << "Initialized thread for variable: " << variableName << std::endl;
        std::thread variable_thread = std::thread([this, variableName](){ this->streamingLoop(variableName);} );
        variableThreads.push_back(std::move(variable_thread));
    }
}

bool DataStreamBB::initializeDataMap()
{
    std::lock_guard<std::mutex> lock( mutex() );

    std::string variableName;
    for (auto variableNameQString : _config.selected_variables)
    {
        variableName = variableNameQString.toStdString();

		auto plot_pair = dataMap().addNumeric( variableName );
		PlotData::Point data_point;

		plot_pair->second.pushBack( data_point );
    }

    bool latestDataQuerySuccess = queryLatestVariableValuesFromBB();

    if (!latestDataQuerySuccess)
    {
        QMessageBox::warning(nullptr, "Failed to Initialize Data Map",
            "Did not receive initial data values from black-box. Aborting...");
    }

    return latestDataQuerySuccess;
}