#ifndef DATASTREAM_BB_TOPIC_H
#define DATASTREAM_BB_TOPIC_H

#include <QtPlugin>
#include <QAction>
#include <QTimer>
#include <thread>
#include <topic_tools/shape_shifter.h>
#include "PlotJuggler/datastreamer_base.h"
#include "dialog_select_bb_variables.h"


#include <json/json.h>
#include "/opt/ropod/ropod_common/ropodcpp/zyre_communicator/include/ZyreBaseCommunicator.h"

class DataStreamBB: public DataStreamer, public ZyreBaseCommunicator
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.icarustechnology.PlotJuggler.DataStreamer" "../datastreamer.json")
    Q_INTERFACES(DataStreamer)

public:

    DataStreamBB();

    virtual bool start(QStringList* selected_datasources) override;

    virtual void shutdown() override;

    virtual bool isRunning() const override;

    virtual ~DataStreamBB() override;

    virtual const char* name() const override { return "Black Box Streamer";  }

    virtual bool xmlSaveState(QDomDocument &doc, QDomElement &parent_element) const override;

    virtual bool xmlLoadState(const QDomElement &parent_element ) override;

    // virtual void addActionsToParentMenu( QMenu* menu ) override;

    virtual std::vector<QString> appendData(PlotDataMapRef& destination) override
    {
        _destination_data = &destination;
        return DataStreamer::appendData(destination);
    }

    bool queryVariableListFromBB();
    bool queryLatestVariableValuesFromBB();
    void queryVariableValuesFromBB();

    void recvMsgCallback(ZyreMsgContent *msgContent);

    bool initializeDataMap();
    void instantiateVariableThreads();

    void streamingLoop(std::string variableName);
    void queryingLoop();

    void multiMeasurementSingleCycle(std::string variableName);

private:
    Json::CharReaderBuilder jsonBuilder;
    std::string myUUID;

    QStringList BBVariableList;
    std::map<std::string, std::deque<std::pair<double, double>>> BBVariableDataMulti;
    std::map<std::string, std::pair<double, double>> LatestBBVariableData;

    std::chrono::high_resolution_clock::time_point _initialTime;

    bool _running;
    bool _waitingForBBResponse;
    double _latestTimestamp;

    PlotDataMapRef* _destination_data;

    std::thread _thread;
    std::thread _queryThread;
    std::vector<std::thread> variableThreads;

    DialogSelectBBVariables::Configuration _config;

    void saveDefaultSettings();

    void loadDefaultSettings();
};

#endif // DATASTREAM_BB_TOPIC_H
