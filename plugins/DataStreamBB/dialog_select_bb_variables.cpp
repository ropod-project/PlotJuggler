#include <QFile>
#include <QTextStream>
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QFileDialog>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QDebug>
#include <QMessageBox>
#include <QAbstractItemView>

#include "dialog_select_bb_variables.h"
#include <ros_type_introspection/ros_introspection.hpp>
#include "ui_dialog_select_bb_variables.h"


/**
 * Adapted from plotJuggler's DialogSelectROSTopics class:
 */
DialogSelectBBVariables::DialogSelectBBVariables(const std::vector<std::pair<QString, QString>>& variable_list,
                                             const Configuration &config,
                                             QWidget *parent) :
    QDialog(parent),
    ui(new Ui::dialogSelectBBVariables),
    _default_selected_variables(config.selected_variables),
    _select_all(QKeySequence(Qt::CTRL + Qt::Key_A), this),
    _deselect_all(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_A), this)
{

    auto flags = this->windowFlags();
    this->setWindowFlags( flags | Qt::WindowStaysOnTopHint);

    ui->setupUi(this);

    ui->spinBoxArraySize->setValue( config.max_array_size );

    if( config.discard_large_arrays )
    {
        ui->radioMaxDiscard->setChecked(true);
    }
    else{
        ui->radioMaxClamp->setChecked(true);
    }

    QStringList labels;
    labels.push_back("Variable name");
    labels.push_back("Datatype");

    ui->listBBVariables->setHorizontalHeaderLabels(labels);
    ui->listBBVariables->verticalHeader()->setVisible(false);

    updateVariableList(variable_list);

    if( ui->listBBVariables->rowCount() == 1)
    {
        ui->listBBVariables->selectRow(0);
    }
    ui->listBBVariables->setFocus();

    _select_all.setContext(Qt::WindowShortcut);
    _deselect_all.setContext(Qt::WindowShortcut);

    connect( &_select_all, &QShortcut::activated,
            ui->listBBVariables, [this]()
            {
              for (int row=0; row< ui->listBBVariables->rowCount(); row++)
              {
                if( !ui->listBBVariables->isRowHidden(row) &&
                    !ui->listBBVariables->item(row,0)->isSelected())
                {
                  ui->listBBVariables->selectRow(row);
                }
              }
            });

    connect( &_deselect_all, &QShortcut::activated,
             ui->listBBVariables, &QAbstractItemView::clearSelection );

    on_spinBoxArraySize_valueChanged( ui->spinBoxArraySize->value() );

    QSettings settings;
    restoreGeometry(settings.value("DialogSelectBBVariables.geometry").toByteArray());

}

void DialogSelectBBVariables::updateVariableList(std::vector<std::pair<QString, QString> > variable_list )
{
    std::set<QString> newly_added;

    // add if not present
    for (const auto& it: variable_list)
    {
        const QString& topic_name = it.first ;
        const QString& type_name  = it.second ;

        bool found = false;
        for(int r=0; r < ui->listBBVariables->rowCount(); r++ )
        {
            const QTableWidgetItem *item = ui->listBBVariables->item(r,0);
            if( item->text() == topic_name){
                found = true;
                break;
            }
        }

        if( !found )
        {
            int new_row = ui->listBBVariables->rowCount();
            ui->listBBVariables->setRowCount( new_row+1 );

            // order IS important, don't change it
            // ui->listBBVariables->setItem(new_row, 1, new QTableWidgetItem( type_name ));
            ui->listBBVariables->setItem(new_row, 0, new QTableWidgetItem( topic_name ));
            newly_added.insert(topic_name);
        }
    }

    ui->listBBVariables->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    // ui->listBBVariables->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->listBBVariables->sortByColumn(0, Qt::AscendingOrder);
    //----------------------------------------------

    QModelIndexList selection = ui->listBBVariables->selectionModel()->selectedRows();

    for(int row=0; row < ui->listBBVariables->rowCount(); row++ )
    {
        const QTableWidgetItem *item = ui->listBBVariables->item(row,0);
        QString topic_name = item->text();

        if(newly_added.count(topic_name) &&  _default_selected_variables.contains( topic_name ) )
        {
            bool selected = false;
            for (const auto& selected_item: selection)
            {
                if( selected_item.row() == row)
                {
                    selected = true;
                    break;
                }
            }
            if( !selected ){
                ui->listBBVariables->selectRow(row);
            }
        }
    }
}


DialogSelectBBVariables::~DialogSelectBBVariables()
{
    QSettings settings;
    settings.setValue("DialogSelectBBVariables.geometry", saveGeometry());
    delete ui;
}

DialogSelectBBVariables::Configuration DialogSelectBBVariables::getResult() const
{
    Configuration config;
    config.selected_variables      = _variable_list;
    config.max_array_size       = ui->spinBoxArraySize->value();
    config.discard_large_arrays = ui->radioMaxDiscard->isChecked();
    return config;
}


void DialogSelectBBVariables::on_buttonBox_accepted()
{
    QModelIndexList selected_indexes = ui->listBBVariables->selectionModel()->selectedIndexes();
    QString selected_variables;

    foreach(QModelIndex index, selected_indexes)
    {
        if(index.column() == 0){
            _variable_list.push_back( index.data(Qt::DisplayRole).toString() );
            selected_variables.append( _variable_list.back() ).append(" ");
        }
    }
}

void DialogSelectBBVariables::on_listBBVariables_itemSelectionChanged()
{
    QModelIndexList indexes = ui->listBBVariables->selectionModel()->selectedIndexes();

    ui->buttonBox->setEnabled( indexes.size() > 0) ;
}


void DialogSelectBBVariables::on_maximumSizeHelp_pressed()
{
    QMessageBox msgBox;
    msgBox.setWindowTitle("Help");
    msgBox.setText("Maximum Size of Arrays:\n\n"
                   "If the size of an Arrays is larger than this maximum value, the entire array is skipped.\n\n"
                   "This parameter is used to prevent the user from loading HUGE arrays, "
                   "such as images, pointclouds, maps, etc.\n"
                   "The term 'array' refers to the array in a message field,\n\n"
                   " See http://wiki.ros.org/msg.\n\n"
                   "This is NOT about the duration of a time series!\n\n"
                   "MOTIVATION: pretend that a user tries to load a RGB image, which probably contains "
                   "a few millions pixels.\n"
                   "Plotjuggler would naively create a single time series for each pixel of the image! "
                   "That makes no sense, of course, and it would probably freeze your system.\n"
                   );
    msgBox.exec();
}

void DialogSelectBBVariables::on_lineEditFilter_textChanged(const QString& search_string)
{
    int visible_count = 0;
    bool updated = false;

    QStringList spaced_items = search_string.split(' ');

    for (int row=0; row < ui->listBBVariables->rowCount(); row++)
    {
        auto item = ui->listBBVariables->item(row,0);
        QString name = item->text();
        int pos = 0;
        bool toHide = false;

        for (const auto& item: spaced_items)
        {
            if( !name.contains(item) )
            {
                toHide = true;
                break;
            }
        }
        ui->listBBVariables->setRowHidden(row, toHide );
    }
}

void DialogSelectBBVariables::on_spinBoxArraySize_valueChanged(int value)
{

}



