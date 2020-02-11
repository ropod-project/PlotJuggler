#include "dataload_mongodb.h"
#include <QTextStream>
#include <QFile>
#include <QMessageBox>
#include <QDebug>
#include <QSettings>
#include <QProgressDialog>
#include "dialog_select_mongodb_collections.h"
#include "dialog_with_itemlist.h"
#include "PlotJuggler/selectlistdialog.h"
#include <iostream>
#include <sstream>
#include <json/json.h>


DataLoadMongoDB::DataLoadMongoDB()
{
    _extensions.push_back( "mongo");
}

const std::vector<const char*> &DataLoadMongoDB::compatibleFileExtensions() const
{
    return _extensions;
}

std::vector<std::string> DataLoadMongoDB::getDBInfo(QFile *file, std::string &db_name)
{
    QTextStream inA(file);
    db_name = inA.readLine().toStdString();
    std::vector<std::string> collection_strings;

    mongocxx::client db_client{mongocxx::uri{}};
    auto database = db_client[db_name];
    auto collections = database.list_collections();
    for (auto collection_info : collections)
    {
        std::string collection_name = collection_info["name"].get_utf8().value.to_string();
        collection_strings.push_back(collection_name);
    }
    return collection_strings;
}

std::vector<std::string> DataLoadMongoDB::getVariableNames(const Json::Value &root, const std::string &root_name)
{
    std::vector<std::string> variables;
    if (root.isObject())
    {
        for (Json::Value::const_iterator itr = root.begin(); itr != root.end(); itr++)
        {
            if (itr->isObject())
            {
                if (itr.key() == "_id")
                {
                    continue;
                }
                for (Json::Value::const_iterator itr2 = itr->begin(); itr2 != itr->end(); itr2++)
                {
                    std::string sub_root_name = root_name + "/" + itr.key().asString() + "/" + itr2.key().asString();
                    std::vector<std::string> sub_variables = getVariableNames(*itr2, sub_root_name);
                    variables.insert(variables.end(), sub_variables.begin(), sub_variables.end());
                }
            }
            else if (itr->isArray())
            {
                Json::ArrayIndex size = itr->size();
                for (int j = 0; j < size; j++)
                {
                    std::stringstream ss;
                    ss << j;
                    std::string sub_root_name = root_name + "/" + itr.key().asString() + "." + ss.str();
                    std::vector<std::string> sub_variables = getVariableNames((*itr)[j], sub_root_name);
                    variables.insert(variables.end(), sub_variables.begin(), sub_variables.end());
                }
            }
            else
            {
                std::string sub_root_name = root_name + "/" + itr.key().asString();
                variables.push_back(sub_root_name);
            }
        }
    }
    else
    {
        variables.push_back(root_name);
    }
    return variables;
}

void DataLoadMongoDB::getData(const Json::Value &root, const std::string &root_name, double timestamp, std::map<std::string, PlotData*> &plot_map)
{
    if (root.isObject())
    {
        for (Json::Value::const_iterator itr = root.begin(); itr != root.end(); itr++)
        {
            if (itr->isObject())
            {
                if (itr.key() == "_id")
                {
                    continue;
                }
                for (Json::Value::const_iterator itr2 = itr->begin(); itr2 != itr->end(); itr2++)
                {
                    std::string sub_root_name = root_name + "/" + itr.key().asString() + "/" + itr2.key().asString();
                    getData(*itr2, sub_root_name, timestamp, plot_map);
                }
            }
            else if (itr->isArray())
            {
                Json::ArrayIndex size = itr->size();
                for (int j = 0; j < size; j++)
                {
                    std::stringstream ss;
                    ss << j;
                    std::string sub_root_name = root_name + "/" + itr.key().asString() + "." + ss.str();
                    getData((*itr)[j], sub_root_name, timestamp, plot_map);
                }
            }
            else if (itr->isNumeric())
            {
                std::string sub_root_name = root_name + "/" + itr.key().asString();
                PlotData::Point point(timestamp, itr->asDouble());
                plot_map[sub_root_name]->pushBack(point);
            }
        }
    }
    else if (root.isNumeric())
    {
        PlotData::Point point(timestamp, root.asDouble());
        plot_map[root_name]->pushBack(point);
    }
}

bool DataLoadMongoDB::readDataFromFile(FileLoadInfo* info, PlotDataMapRef& plot_data)
{
    bool use_provided_configuration = false;

    if( info->plugin_config.hasChildNodes() )
    {
        use_provided_configuration = true;
        xmlLoadState( info->plugin_config.firstChildElement() );
    }

    QFile file( info->filename );
    file.open(QFile::ReadOnly);

    // get list of collections in the DB
    std::vector<std::string> collections;
    std::string db_name;
    collections = getDBInfo(&file, db_name);
    file.close();

    if( info->plugin_config.hasChildNodes() )
    {
        xmlLoadState( info->plugin_config.firstChildElement() );
    }

    if( ! info->selected_datasources.empty() )
    {
        _config.selected_collections = info->selected_datasources;
    }
    else
    {
        DialogSelectMongoDBCollections* dialog = new DialogSelectMongoDBCollections(collections, _config );

        if( dialog->exec() != static_cast<int>(QDialog::Accepted) )
        {
            return false;
        }
        _config = dialog->getResult();
    }

    mongocxx::client db_client{mongocxx::uri{}};
    auto database = db_client[db_name];

    std::map<std::string, PlotData*> plot_map;
    int total_documents = 0;
    // get all variable names from all selected collections
    // and create PlotData objects for each variable
    for (int i = 0; i < _config.selected_collections.size(); i++)
    {
        std::string collection_name = _config.selected_collections[i].toStdString();
        auto collection = database[collection_name];
        auto cursor = collection.find({});
        int num_documents = collection.count_documents({});
        total_documents += num_documents;
        for(auto doc : cursor)
        {
            std::string json_doc = bsoncxx::to_json(doc);
            std::stringstream sstr(json_doc);
            Json::Value json;
            sstr >> json;
            std::vector<std::string> variables = getVariableNames(json, collection_name);
            for (int j = 0; j < variables.size(); j++)
            {
                auto it = plot_data.addNumeric(variables[j]);
                plot_map.insert(std::pair<std::string, PlotData*>(variables[j], &(it->second)));
            }
            break;
        }
    }

    QProgressDialog progress_dialog;
    progress_dialog.setLabelText("Loading... please wait");
    progress_dialog.setWindowModality( Qt::ApplicationModal );

    progress_dialog.setRange(0, total_documents-1);
    progress_dialog.show();

    int doc_count = 0;

    // populate the timeseries data for each variable
    for (int i = 0; i < _config.selected_collections.size(); i++)
    {
        std::string collection_name = _config.selected_collections[i].toStdString();
        auto collection = database[collection_name];
        auto cursor = collection.find({});
        for(auto doc : cursor)
        {
            std::string json_doc = bsoncxx::to_json(doc);
            std::stringstream sstr(json_doc);
            Json::Value json;
            sstr >> json;
            double timestamp = json["timestamp"].asDouble();

            // will traverse the json tree for this document
            // and populate the data
            getData(json, collection_name, timestamp, plot_map);

            doc_count++;
            if (doc_count % 100 == 0)
            {
                progress_dialog.setValue(doc_count);
                QApplication::processEvents();
                if (progress_dialog.wasCanceled())
                {
                    return false;
                }
            }
        }
    }

    return true;
}

DataLoadMongoDB::~DataLoadMongoDB()
{

}

bool DataLoadMongoDB::xmlSaveState(QDomDocument &doc, QDomElement &parent_element) const
{
    QDomElement elem = doc.createElement("default");
    elem.setAttribute("time_axis", _default_time_axis.c_str() );

    parent_element.appendChild( elem );
    return true;
}

bool DataLoadMongoDB::xmlLoadState(const QDomElement &parent_element)
{
    QDomElement elem = parent_element.firstChildElement( "default" );
    if( !elem.isNull()    )
    {
        if( elem.hasAttribute("time_axis") )
        {
            _default_time_axis = elem.attribute("time_axis").toStdString();
            return true;
        }
    }
    return false;
}

void DataLoadMongoDB::saveDefaultSettings()
{
    QSettings settings;

    settings.setValue("DataLoadMongoDB/default_collections", _config.selected_collections);
}

void DataLoadMongoDB::loadDefaultSettings()
{
    QSettings settings;

    _config.selected_collections = settings.value("DataLoadMongoDB/default_collections", false ).toStringList();
}
