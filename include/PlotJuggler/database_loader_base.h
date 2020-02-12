#ifndef DATABASE_LOADER_BASE_H
#define DATABASE_LOADER_BASE_H


#include <QFile>

#include <functional>
#include "PlotJuggler/plotdata.h"
#include "PlotJuggler/pj_plugin.h"
#include "PlotJuggler/messageparser_base.h"


struct DBLoadInfo
{
    QString dbname;
    QStringList selected_datasources;
};


class DatabaseLoader: public PlotJugglerPlugin
{

public:
    DatabaseLoader(){}

    virtual bool readDataFromDatabase(DBLoadInfo* dbload_info, PlotDataMapRef& destination) = 0;
    virtual std::vector<std::string> getDatabaseNames() = 0;

    virtual ~DatabaseLoader() {}

protected:


};

QT_BEGIN_NAMESPACE

#define DatabaseRead_iid "com.icarustechnology.PlotJuggler.DatabaseLoader"

Q_DECLARE_INTERFACE(DatabaseLoader, DatabaseRead_iid)

QT_END_NAMESPACE


#endif

