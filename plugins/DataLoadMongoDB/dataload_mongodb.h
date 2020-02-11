#ifndef DATALOAD_MONGODB_H
#define DATALOAD_MONGODB_H

#include <QObject>
#include <QtPlugin>
#include "PlotJuggler/dataloader_base.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include "dialog_select_mongodb_collections.h"
#include <json/json.h>



class  DataLoadMongoDB: public DataLoader
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.icarustechnology.PlotJuggler.DataLoader" "../dataloader.json")
    Q_INTERFACES(DataLoader)

public:
    DataLoadMongoDB();
    virtual const std::vector<const char*>& compatibleFileExtensions() const override;

    virtual bool readDataFromFile(FileLoadInfo* fileload_info, PlotDataMapRef& destination) override;

    virtual ~DataLoadMongoDB();

    virtual const char* name() const override { return "DataLoad MongoDB"; }

    virtual bool xmlSaveState(QDomDocument &doc, QDomElement &parent_element) const override;

    virtual bool xmlLoadState(const QDomElement &parent_element ) override;

protected:
    std::vector<std::string> getDBInfo(QFile *file, std::string &name);
    std::vector<std::string> getVariableNames(const Json::Value &root, const std::string &root_name);
    void getData(const Json::Value &root, const std::string &root_name, double timestamp, std::map<std::string, PlotData*> &plot_map);
    mongocxx::instance instance_;

private:
    std::vector<const char*> _extensions;

    std::string _default_time_axis;

    void saveDefaultSettings();

    void loadDefaultSettings();

    DialogSelectMongoDBCollections::Configuration _config;


};

#endif // DATALOAD_MONGODB_H
