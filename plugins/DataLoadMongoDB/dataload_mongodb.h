#ifndef DATALOAD_MONGODB_H
#define DATALOAD_MONGODB_H

#include <QObject>
#include <QtPlugin>
#include "PlotJuggler/database_loader_base.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include "dialog_select_mongodb_collections.h"
#include <json/json.h>



class  DataLoadMongoDB: public DatabaseLoader
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.icarustechnology.PlotJuggler.DatabaseLoader" "../databaseloader.json")
    Q_INTERFACES(DatabaseLoader)

public:
    DataLoadMongoDB();

    virtual bool readDataFromDatabase(DBLoadInfo* dbload_info, PlotDataMapRef& destination) override;
    virtual std::vector<std::string> getDatabaseNames() override;

    virtual ~DataLoadMongoDB();

    virtual const char* name() const override { return "DataLoad MongoDB"; }

    virtual bool xmlSaveState(QDomDocument &doc, QDomElement &parent_element) const override;

    virtual bool xmlLoadState(const QDomElement &parent_element ) override;

protected:
    std::vector<std::string> getDBInfo(const std::string &name);
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
